/*
 * plugin/src/panel/touch.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * AXS15231B userspace I2C touch (gestures, drag-scroll)
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_TOUCH_H
#define PANEL_TOUCH_H

/* ============================================================
 * TOUCH — AXS15231B native I2C protocol (userspace polling).
 * Unraid's kernel lacks GPIOLIB_IRQCHIP so the vendor/HID paths
 * can't bind; instead we poll the chip directly at 0x3b with the
 * AXS command-prefixed read (magic b5 ab a5 5a). Auto-discovers
 * the DesignWare adapter; daemon runs fine without touch.
 * Protocol reverse-engineered on this unit; the b5 ab a5 5a prefix
 * and frame layout match published AXS15231B drivers/datasheets.
 * ============================================================ */
typedef enum {
    TOUCH_NONE = 0, TOUCH_TAP, TOUCH_LONG_PRESS,
    TOUCH_SWIPE_LEFT, TOUCH_SWIPE_RIGHT, TOUCH_SWIPE_UP, TOUCH_SWIPE_DOWN,
    TOUCH_DRAG          /* continuous vertical scroll; ev.y = delta px this frame */
} touch_gesture_t;
typedef struct { touch_gesture_t type; int x, y; } touch_event_t;

static int cal_swap_xy = 0, cal_inv_x = 0, cal_inv_y = 0;
static int scr_w = 258, scr_h = 960;

#define TAP_MS    300
#define LONG_MS   600
#define MOVE_PX    18
#define SWIPE_PX   60
#define SWIPE_MS  500

static int tfd = -1;
static struct { int minimum, maximum; } tax, tay;
static int tdown, tmoved, long_fired, have_origin;
static int rawx, rawy, raw_seen, pend_down, pend_valid;
static int t_ox, t_oy, t_cx, t_cy, t_lasty, t_axis, t_drag_dy;
static long t_down_ms, t_last_i2c_ms;

#define AXS_ADDR 0x3b
#define AXS_FRAME_LEN 15

static int axs_frame(int fd, unsigned char *out){
    unsigned char cmd[11] = {0xb5,0xab,0xa5,0x5a,0,0,
                             0, AXS_FRAME_LEN, 0,0,0};
    struct i2c_msg m[2] = {
        { .addr = AXS_ADDR, .flags = 0,        .len = 11,            .buf = cmd },
        { .addr = AXS_ADDR, .flags = I2C_M_RD, .len = AXS_FRAME_LEN, .buf = out },
    };
    struct i2c_rdwr_ioctl_data d = { m, 2 };
    return ioctl(fd, I2C_RDWR, &d);
}

static int touch_init(const char *dev_hint){
    char path[64];
    unsigned char b[AXS_FRAME_LEN];
    if (dev_hint && dev_hint[0] == '/'){
        int fd = open(dev_hint, O_RDWR | O_CLOEXEC);
        if (fd >= 0 && axs_frame(fd, b) >= 0){
            fprintf(stderr, "touch: AXS15231B on %s (forced)\n", dev_hint);
            tfd = fd;
            goto found;
        }
        if (fd >= 0) close(fd);
        fprintf(stderr, "touch: %s not answering\n", dev_hint);
        return -1;
    }
    for (int i = 0; i < 64; i++){
        char namep[128], name[80] = "";
        snprintf(namep, sizeof namep, "/sys/bus/i2c/devices/i2c-%d/name", i);
        FILE *f = fopen(namep, "r");
        if (!f) continue;
        if (fgets(name, sizeof name, f)) name[strcspn(name, "\n")] = 0;
        fclose(f);
        if (!strstr(name, "DesignWare")) continue;
        snprintf(path, sizeof path, "/dev/i2c-%d", i);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        if (axs_frame(fd, b) >= 0){
            fprintf(stderr, "touch: AXS15231B on %s\n", path);
            tfd = fd;
            goto found;
        }
        close(fd);
    }
    fprintf(stderr, "touch: no AXS touch chip found (running without touch)\n");
    return -1;
found:
    tax.minimum = 0; tax.maximum = scr_w - 1;   /* chip reports panel coords */
    tay.minimum = 0; tay.maximum = scr_h - 1;
    return tfd;
}

static void map_xy(int rx, int ry, int *sx, int *sy){
    int drx = tax.maximum - tax.minimum; if (drx <= 0) drx = 1;
    int dry = tay.maximum - tay.minimum; if (dry <= 0) dry = 1;
    float fx = (float)(rx - tax.minimum) / (float)drx;
    float fy = (float)(ry - tay.minimum) / (float)dry;
    if (cal_swap_xy){ float t = fx; fx = fy; fy = t; }
    if (cal_inv_x) fx = 1.0f - fx;
    if (cal_inv_y) fy = 1.0f - fy;
    *sx = (int)(fx * (scr_w - 1) + 0.5f);
    *sy = (int)(fy * (scr_h - 1) + 0.5f);
}

static touch_gesture_t touch_commit(void){
    touch_gesture_t g = TOUCH_NONE;
    if (pend_valid && pend_down && !tdown){                /* finger down */
        tdown = 1; tmoved = 0; long_fired = 0; have_origin = 0;
        t_axis = 0; t_drag_dy = 0; t_down_ms = now_ms();
    }
    if (tdown && raw_seen){                                /* motion */
        map_xy(rawx, rawy, &t_cx, &t_cy);
        if (!have_origin){ t_ox = t_cx; t_oy = t_cy; t_lasty = t_cy; have_origin = 1; }
        else {
            if (t_axis == 0 &&
                (abs(t_cx - t_ox) > MOVE_PX || abs(t_cy - t_oy) > MOVE_PX)){
                t_axis = abs(t_cy - t_oy) >= abs(t_cx - t_ox) ? 1 : 2;  /* 1=vert 2=horiz */
                tmoved = 1;
            }
            if (t_axis == 1){                             /* vertical -> live scroll */
                int ddy = t_cy - t_lasty;
                if (ddy){ t_drag_dy = ddy; g = TOUCH_DRAG; }
            }
            t_lasty = t_cy;
        }
    }
    if (pend_valid && !pend_down && tdown){                /* finger up */
        tdown = 0;
        long dt = now_ms() - t_down_ms;
        int dx = t_cx - t_ox;
        if (!long_fired && have_origin){
            if (t_axis == 2 && dt <= SWIPE_MS && abs(dx) >= SWIPE_PX)
                g = dx < 0 ? TOUCH_SWIPE_LEFT : TOUCH_SWIPE_RIGHT;
            else if (t_axis == 0 && !tmoved && dt < TAP_MS)
                g = TOUCH_TAP;
        }
    }
    pend_valid = 0; raw_seen = 0;
    return g;
}

static int touch_poll(touch_event_t *ev){
    ev->type = TOUCH_NONE;
    if (tfd < 0) return 0;
    long nowms = now_ms();
    if (nowms - t_last_i2c_ms >= 15){                    /* ~66Hz I2C poll cap */
        t_last_i2c_ms = nowms;
        unsigned char b[AXS_FRAME_LEN];
        if (axs_frame(tfd, b) >= 0){
            unsigned char crc = 0;
            for (int i = 0; i < AXS_FRAME_LEN - 1; i++) crc += b[i];
            int idle_fill = 1;              /* uniform-fill frame = release/idle keepalive.
                                             * The fill byte is firmware-dependent (0x10 on
                                             * some revs, 0x18 on this panel) — a real touch
                                             * frame is never uniform, so detect by uniformity
                                             * rather than a hard-coded value. */
            for (int i = 1; i < AXS_FRAME_LEN; i++) if (b[i] != b[0]){ idle_fill = 0; break; }
            int pts = idle_fill ? 0 : (b[1] & 0x0f);
            if (idle_fill || crc == b[AXS_FRAME_LEN - 1]){
                if (pts > 0){
                    rawx = ((b[2] & 0x0f) << 8) | b[3];
                    rawy = ((b[4] & 0x0f) << 8) | b[5];
                    raw_seen = 1;
                    pend_down = 1; pend_valid = 1;
                } else if (tdown){
                    pend_down = 0; pend_valid = 1;
                }
                if (getenv("TOUCH_DEBUG") && (pts > 0 || tdown))
                    fprintf(stderr, "tp: pts=%d raw=(%d,%d) tdown=%d idle=%d\n",
                            pts, rawx, rawy, tdown, idle_fill);
                touch_gesture_t g = touch_commit();
                if (g != TOUCH_NONE){
                    if (getenv("TOUCH_DEBUG") && g != TOUCH_DRAG)
                        fprintf(stderr, "GESTURE type=%d at (%d,%d)\n", g, t_ox, t_oy);
                    ev->type = g;
                    if (g == TOUCH_DRAG){ ev->x = 0; ev->y = t_drag_dy; }
                    else { ev->x = t_ox; ev->y = t_oy; }
                }
            } else if (getenv("TOUCH_DEBUG")){
                static int badcnt;
                if (++badcnt % 50 == 1)
                    fprintf(stderr, "tp: crc-fail frame (%d so far)\n", badcnt);
            }
        }
    }
    if (ev->type != TOUCH_NONE) return 1;
    if (tdown && have_origin && !tmoved && !long_fired && now_ms() - t_down_ms >= LONG_MS){
        long_fired = 1;
        ev->type = TOUCH_LONG_PRESS; ev->x = t_cx; ev->y = t_cy;
        return 1;
    }
    return 0;
}

static void touch_close(void){ if (tfd >= 0) close(tfd); tfd = -1; }


#endif /* PANEL_TOUCH_H */
