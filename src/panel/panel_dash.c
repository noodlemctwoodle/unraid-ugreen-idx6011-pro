/*
 * plugin/src/panel/panel_dash.c
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * panel_dash — UGREEN iDX6011 Pro front-panel dashboard for Unraid.
 * Renders live system stats on the 258x960 portrait eDP panel via DRM/KMS
 * dumb buffers. Seven touch-navigable pages (HOME / OVERVIEW / HARDWARE /
 * NETWORK / DISKS / DOCKER / SETTINGS) with Unraid branding, notification
 * banner, auto-rotation, interactive settings with flash persistence,
 * screen-off timeout and reboot/shutdown controls.
 *
 * This translation unit is a thin aggregator: it pulls in the system/vendor
 * headers, then #includes the panel modules (draw -> stats -> ui -> pages ->
 * render -> touch) in dependency order, and finally defines main(). The
 * modules are single-TU fragments (each relies on the includes and the
 * modules before it) — they are NOT compiled separately.
 *
 * Deps: libdrm (runtime), stb_easy_font.h + stb_image.h (build, vendored here)
 *
 * Build: gcc -O2 -w -o panel_dash panel_dash.c -I/usr/include/libdrm -ldrm -lm
 *        (run from this directory; see build.sh)
 *
 * CLI (CLI overrides settings.cfg):
 *   --bg <img>          background image (png/jpg), scaled to panel
 *   --interval <sec>    stats refresh interval (default 1)
 *   --backlight <pct>   backlight brightness (default 75)
 *   --once              render one frame and exit
 *   --shot <dir>        render every page to <dir>/<n>-<name>.png and exit
 *                       (offscreen; no DRM/panel needed — for README screenshots)
 *   --touch <hint>      substring match for touch device name
 *   --no-touch          skip touch device discovery
 *   --cal <flags>       touch calibration: any of 's' (swap xy),
 *                       'x' (invert x), 'y' (invert y), e.g. --cal sxy
 *   --rotate <sec>      auto page rotation (0 = off)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/io.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "vendor/stb_image.h"
#include "vendor/stb_easy_font.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb_image_write.h"

/* ---------- panel modules (order matters: declare-before-use) ---------- */
#include "unraid_logo.h"
#include "draw.h"
#include "iniutil.h"
#include "prefs.h"
#include "stats.h"
#include "stats_system.h"
#include "stats_devices.h"
#include "stats_array.h"
#include "ui.h"
#include "backlight.h"
#include "actions.h"
#include "pageframe.h"
#include "modules/module.h"
#include "modules/cardstyle.h"
#include "modules/mod_host.h"
#include "modules/mod_array.h"
#include "modules/mod_update.h"
#include "modules/mod_cpu.h"
#include "modules/mod_mem.h"
#include "modules/mod_net.h"
#include "modules/mod_storage.h"
#include "modules/mod_uptime.h"
#include "modules/mod_gpu.h"
#include "modules/mod_cpu_temp.h"
#include "modules/mod_fans.h"
#include "modules/mod_npu.h"
#include "modules/mod_power.h"
#include "modules/mod_ifaces.h"
#include "modules/mod_disks.h"
#include "modules/mod_containers.h"
#include "modules/mod_vms.h"
#include "modules/registry.h"
#include "pages/settings.h"      /* the one built-in interactive page; content pages are
                                    config-driven (g_cpage) and drawn via render_modules */
#include "render.h"
#include "touch.h"

/* ---------- main ---------- */
int main(int argc, char **argv){
    g_argv = argv;
    const char *bgpath = NULL; int once = 0;
    const char *touch_hint = NULL; int no_touch = 0;
    const char *shot_dir = NULL;
    int prev_page = -1; const char *prev_layout = NULL, *prev_out = NULL;
    int want_modules = 0;
    const char *font_path = NULL;

    settings_load();                            /* flash cfg first, CLI overrides */
    for (int i = 1; i < argc; i++){
        if (!strcmp(argv[i], "--bg") && i + 1 < argc) bgpath = argv[++i];
        else if (!strcmp(argv[i], "--interval") && i + 1 < argc) cfg_interval = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--backlight") && i + 1 < argc) cfg_brightness = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--once")) once = 1;
        else if (!strcmp(argv[i], "--touch") && i + 1 < argc) touch_hint = argv[++i];
        else if (!strcmp(argv[i], "--no-touch")) no_touch = 1;
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot_dir = argv[++i];
        else if (!strcmp(argv[i], "--preview") && i + 3 < argc){   /* <page> <layout> <out.png> */
            prev_page = atoi(argv[++i]); prev_layout = argv[++i]; prev_out = argv[++i];
        }
        else if (!strcmp(argv[i], "--modules")) want_modules = 1;   /* print registry JSON */
        else if (!strcmp(argv[i], "--layouts")) want_modules = 2;   /* print current layouts JSON */
        else if (!strcmp(argv[i], "--items"))   want_modules = 3;   /* print live item names JSON */
        else if (!strcmp(argv[i], "--rotate") && i + 1 < argc) cfg_rotate = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--font") && i + 1 < argc) font_path = argv[++i];
        else if (!strcmp(argv[i], "--cal") && i + 1 < argc){
            const char *c = argv[++i];
            cal_swap_xy = strchr(c, 's') != NULL;
            cal_inv_x   = strchr(c, 'x') != NULL;
            cal_inv_y   = strchr(c, 'y') != NULL;
        }
    }
    /* font: --font / PANEL_FONT override wins (used by the web live-preview);
     * otherwise the configured name. A bare name (no '/') resolves to
     * panel/fonts/<name>.ttf; a path is used as-is. */
    char fontbuf[288];
    const char *fp = font_path ? font_path : getenv("PANEL_FONT");
    if (!fp && cfg_font[0]) fp = cfg_font;
    if (fp && !strchr(fp, '/')){
        snprintf(fontbuf, sizeof fontbuf, "%s/fonts/%s.ttf", CFG_DIR2, fp);
        fp = fontbuf;
    }
    settings_env_overrides();                   /* draft theme/size for the web preview */
    pages_finalize();                           /* build the content-page list (cfg or migrated) */
    font_init(fp);                              /* real TTF; falls back to easy_font */
    if (want_modules == 1) return write_modules_json();   /* module catalog for the web editor */
    if (want_modules == 2) return write_layouts_json();   /* current layouts + toggles */
    if (want_modules == 3) return write_items_json();     /* live interface/disk/container names */
    if (prev_out) return write_preview(prev_page, prev_layout, prev_out);  /* one page, draft layout */
    if (shot_dir) return write_shots(shot_dir, bgpath);   /* offscreen PNG render, then exit */
    if (cfg_interval < 1) cfg_interval = 1;
    if (cfg_brightness < 5) cfg_brightness = 5;
    if (!page_on(cur_page)) cur_page = next_page(cur_page, +1);   /* don't start on a disabled page */
    if (cfg_brightness > 100) cfg_brightness = 100;
    int interval = cfg_interval;
    rotate_ms = (long)cfg_rotate * 1000L;

    int fd = -1; drmModeRes *res = NULL; drmModeConnector *conn = NULL;
    for (int c = 0; c < 4 && !conn; c++){
        char path[32]; snprintf(path, sizeof(path), "/dev/dri/card%d", c);
        fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        res = drmModeGetResources(fd);
        if (!res){ close(fd); fd = -1; continue; }
        for (int i = 0; i < res->count_connectors; i++){
            drmModeConnector *cn = drmModeGetConnector(fd, res->connectors[i]);
            if (cn && cn->connector_type == DRM_MODE_CONNECTOR_eDP &&
                cn->connection == DRM_MODE_CONNECTED && cn->count_modes > 0){
                conn = cn; break;
            }
            if (cn) drmModeFreeConnector(cn);
        }
        if (!conn){ drmModeFreeResources(res); res = NULL; close(fd); fd = -1; }
    }
    if (!conn){ fprintf(stderr, "no connected eDP connector found\n"); return 1; }

    drmModeModeInfo mode = conn->modes[0];
    for (int i = 0; i < conn->count_modes; i++)
        if (conn->modes[i].type & DRM_MODE_TYPE_PREFERRED){ mode = conn->modes[i]; break; }
    W = mode.hdisplay; H = mode.vdisplay;
    fprintf(stderr, "eDP-1: %dx%d@%d\n", W, H, mode.vrefresh);

    /* find a crtc */
    uint32_t crtc_id = 0;
    if (conn->encoder_id){
        drmModeEncoder *e = drmModeGetEncoder(fd, conn->encoder_id);
        if (e){ crtc_id = e->crtc_id; drmModeFreeEncoder(e); }
    }
    if (!crtc_id){
        for (int i = 0; i < conn->count_encoders && !crtc_id; i++){
            drmModeEncoder *e = drmModeGetEncoder(fd, conn->encoders[i]);
            if (!e) continue;
            for (int j = 0; j < res->count_crtcs; j++)
                if (e->possible_crtcs & (1u << j)){ crtc_id = res->crtcs[j]; break; }
            drmModeFreeEncoder(e);
        }
    }
    if (!crtc_id){ fprintf(stderr, "no crtc for eDP\n"); return 1; }

    struct drm_mode_create_dumb cd = { .width = W, .height = H, .bpp = 32 };
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd)){ perror("create_dumb"); return 1; }
    uint32_t fb_id;
    if (drmModeAddFB(fd, W, H, 24, 32, cd.pitch, cd.handle, &fb_id)){ perror("addfb"); return 1; }
    struct drm_mode_map_dumb md = { .handle = cd.handle };
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &md)){ perror("map_dumb"); return 1; }
    fbmem = mmap(0, cd.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, md.offset);
    if (fbmem == MAP_FAILED){ perror("mmap"); return 1; }
    fbpitch = cd.pitch / 4;

    drmSetMaster(fd); /* best effort; SetCrtc needs master or no-master-present */
    make_bg(bgpath);
    apply_brightness();

    signal(SIGINT, on_sig); signal(SIGTERM, on_sig);
    signal(SIGCHLD, SIG_IGN);                  /* auto-reap spawned actions */

    stats_t st; memset(&st, 0, sizeof(st));
    read_cpu(&st); read_net(&st); read_gpu(&st); read_npu(&st); read_power(&st);   /* prime deltas */
    usleep(300 * 1000);

    if (!no_touch) touch_init(touch_hint);
    scr_w = W; scr_h = H;

    int first = 1, dim = 0, screen_off = 0;
    long next_stats = 0;
    long last_touch = now_ms(), last_rotate = now_ms();
    while (!stop_flag){
        long nowms = now_ms();
        int dirty = 0;
        if (nowms >= next_stats){
            read_cpu(&st); read_mem(&st); read_net(&st);
            read_disk(&st); read_temp(&st); read_misc(&st);
            read_disks(&st); read_gpu(&st); read_npu(&st);
            read_fans(&st); read_fan_rpm(&st); read_zones(&st); read_docker(&st); read_power(&st);
            read_about(&st);
            read_notif(&st);
            read_updates(&st);
            hist_push_all(&st);
            next_stats = nowms + (long)interval * 1000L;
            dirty = 1;
        }
        touch_event_t ev;
        if (touch_poll(&ev)){
            last_touch = nowms; last_rotate = nowms;
            if (screen_off){
                /* the waking touch is CONSUMED, never routed to widgets */
                screen_off = 0; dim = 0;
                apply_brightness();
                dirty = 1;
            } else switch (ev.type){
            case TOUCH_SWIPE_LEFT:
                cur_page = next_page(cur_page, +1); scrolly[cur_page] = 0;
                confirm_which = 0; dirty = 1; break;
            case TOUCH_SWIPE_RIGHT:
                cur_page = next_page(cur_page, -1); scrolly[cur_page] = 0;
                confirm_which = 0; dirty = 1; break;
            case TOUCH_DRAG:
                scrolly[cur_page] -= ev.y; dirty = 1; break;   /* 1:1 finger scroll */
            case TOUCH_TAP:
                if (is_settings_page(cur_page) && widget_tap(ev.x, ev.y)){
                    dirty = 1;
                } else if (ev.y >= FOOTER_Y){
                    cur_page = next_page(cur_page, +1); confirm_which = 0; dirty = 1;
                } else {
                    dim = 0; apply_brightness();                   /* wake */
                }
                break;
            case TOUCH_LONG_PRESS:
                if (is_settings_page(cur_page) && widget_long(ev.x, ev.y)){
                    dirty = 1;
                } else {
                    dim = !dim;
                    set_backlight(dim ? 10 : eff_brightness());    /* dim toggle */
                }
                break;
            default: break;
            }
        }
        if (confirm_which && nowms >= confirm_until){          /* confirm expired */
            confirm_which = 0;
            if (is_settings_page(cur_page)) dirty = 1;
        }
        if (!screen_off && cfg_screen_off > 0 &&
            nowms - last_touch >= (long)cfg_screen_off * 60000L){
            screen_off = 1;
            ec_write(0xA5, 198);                               /* EC backlight off */
            set_bl_power(0);                                   /* panel off */
        }
        if (rotate_ms > 0 && !screen_off &&
            nowms - last_touch >= rotate_ms && nowms - last_rotate >= rotate_ms){
            cur_page = next_page(cur_page, +1);
            scrolly[cur_page] = 0;
            last_rotate = nowms; dirty = 1;
        }
        if (dirty){
            render(&st);
            /* i915 FBC caches a compressed copy of the scanout buffer; CPU
             * writes to a dumb buffer don't invalidate it. DirtyFB does. */
            drmModeDirtyFB(fd, fb_id, NULL, 0);
            if (first || (interval >= 5)){
                /* (re)assert our fb on the crtc — also recovers from fbcon stomps */
                if (drmModeSetCrtc(fd, crtc_id, fb_id, 0, 0, &conn->connector_id, 1, &mode)){
                    if (first){ perror("setcrtc"); return 1; }
                }
                first = 0;
            } else {
                drmModeCrtc *cur = drmModeGetCrtc(fd, crtc_id);
                if (cur){
                    if (cur->buffer_id != fb_id)
                        drmModeSetCrtc(fd, crtc_id, fb_id, 0, 0, &conn->connector_id, 1, &mode);
                    drmModeFreeCrtc(cur);
                }
            }
            if (once) break;
        }
        usleep(20 * 1000);
    }
    touch_close();
    fprintf(stderr, "exiting\n");
    return 0;
}
