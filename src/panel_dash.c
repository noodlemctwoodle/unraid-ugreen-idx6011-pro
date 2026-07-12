/*
 * panel_dash.c — UGREEN iDX6011 Pro front-panel dashboard for Unraid
 * Renders live system stats on the 258x960 eDP panel via DRM/KMS dumb buffers.
 * Deps: libdrm (runtime), stb_easy_font.h + stb_image.h (build, vendored)
 *
 * Build: gcc -O2 -o panel_dash panel_dash.c -I/usr/include/libdrm -ldrm -lm
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
#include "stb_easy_font.h"

/* ---------- framebuffer ---------- */
static int W = 258, H = 960;
static uint32_t *fbmem;            /* mapped dumb buffer */
static int fbpitch;                /* in pixels */
static uint32_t *bg;               /* prerendered background */
static volatile sig_atomic_t stop_flag;

static void on_sig(int s){ (void)s; stop_flag = 1; }

static inline void px(int x, int y, uint32_t c){
    if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H)
        fbmem[y * fbpitch + x] = c;
}
static inline uint32_t blend(uint32_t dst, uint32_t src, uint8_t a){
    uint32_t rb = ((src & 0xff00ff) * a + (dst & 0xff00ff) * (255 - a)) >> 8;
    uint32_t g  = ((src & 0x00ff00) * a + (dst & 0x00ff00) * (255 - a)) >> 8;
    return (rb & 0xff00ff) | (g & 0x00ff00);
}
static void rect(int x, int y, int w, int h, uint32_t c, uint8_t a){
    if (x < 0){ w += x; x = 0; } if (y < 0){ h += y; y = 0; }
    if (x + w > W) w = W - x; if (y + h > H) h = H - y;
    for (int j = 0; j < h; j++){
        uint32_t *row = fbmem + (y + j) * fbpitch + x;
        if (a == 255) for (int i = 0; i < w; i++) row[i] = c;
        else          for (int i = 0; i < w; i++) row[i] = blend(row[i], c, a);
    }
}
static void hline(int x, int y, int w, uint32_t c){ rect(x, y, w, 1, c, 255); }

/* ---------- text via stb_easy_font ---------- */
static char qbuf[64000];
static void text(int x, int y, float scale, uint32_t c, const char *s){
    int n = stb_easy_font_print(0, 0, (char*)s, NULL, qbuf, sizeof(qbuf));
    float *v = (float*)qbuf;                    /* 16B/vertex: x,y,z,color */
    for (int q = 0; q < n; q++){
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (int k = 0; k < 4; k++){
            float vx = v[(q * 4 + k) * 4 + 0], vy = v[(q * 4 + k) * 4 + 1];
            if (vx < x0) x0 = vx; if (vx > x1) x1 = vx;
            if (vy < y0) y0 = vy; if (vy > y1) y1 = vy;
        }
        int rx = x + (int)floorf(x0 * scale), ry = y + (int)floorf(y0 * scale);
        int rw = (int)ceilf((x1 - x0) * scale), rh = (int)ceilf((y1 - y0) * scale);
        if (rw < 1) rw = 1; if (rh < 1) rh = 1;
        rect(rx, ry, rw, rh, c, 255);
    }
}
static int text_w(float scale, const char *s){
    return (int)(stb_easy_font_width((char*)s) * scale);
}
static void text_c(int y, float scale, uint32_t c, const char *s){ /* centered */
    text((W - text_w(scale, s)) / 2, y, scale, c, s);
}

/* ---------- background ---------- */
static void make_bg(const char *path){
    bg = malloc((size_t)W * H * 4);
    int iw, ih, comp; unsigned char *img = NULL;
    if (path) img = stbi_load(path, &iw, &ih, &comp, 3);
    if (img){
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++){
            int sx = x * iw / W, sy = y * ih / H;
            unsigned char *p = img + (sy * iw + sx) * 3;
            bg[y * W + x] = (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
        }
        stbi_image_free(img);
        fprintf(stderr, "background: %s (%dx%d)\n", path, iw, ih);
    } else {
        for (int y = 0; y < H; y++){            /* teal -> near black gradient */
            float t = (float)y / H;
            uint8_t r = (uint8_t)(8  + 10 * (1 - t));
            uint8_t g = (uint8_t)(42 + 60 * (1 - t));
            uint8_t b = (uint8_t)(48 + 70 * (1 - t));
            for (int x = 0; x < W; x++) bg[y * W + x] = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
        }
    }
}

/* ---------- stats ---------- */
typedef struct {
    char host[64], ip[48], arr[32];
    double cpu; long mem_used_mb, mem_tot_mb;
    double rx_kbs, tx_kbs;
    double disk_used_pct; double disk_tot_gb, disk_used_gb;
    int temp_c; long up_s;
} stats_t;

static unsigned long long pc_busy, pc_tot;      /* cpu deltas */
static unsigned long long pn_rx, pn_tx; static struct timespec pn_t;

static void read_cpu(stats_t *st){
    FILE *f = fopen("/proc/stat", "r"); if (!f) return;
    unsigned long long u,n,s,i,io,irq,sirq,steal;
    if (fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &u,&n,&s,&i,&io,&irq,&sirq,&steal) == 8){
        unsigned long long busy = u+n+s+irq+sirq+steal, tot = busy+i+io;
        if (pc_tot && tot > pc_tot)
            st->cpu = 100.0 * (double)(busy - pc_busy) / (double)(tot - pc_tot);
        pc_busy = busy; pc_tot = tot;
    }
    fclose(f);
}
static void read_mem(stats_t *st){
    FILE *f = fopen("/proc/meminfo", "r"); if (!f) return;
    char k[64]; long v; char u[16]; long tot = 0, avail = 0;
    while (fscanf(f, "%63s %ld %15s", k, &v, u) >= 2){
        if (!strcmp(k, "MemTotal:")) tot = v;
        else if (!strcmp(k, "MemAvailable:")){ avail = v; break; }
    }
    fclose(f);
    st->mem_tot_mb = tot / 1024; st->mem_used_mb = (tot - avail) / 1024;
}
static void read_net(stats_t *st){
    FILE *f = fopen("/proc/net/dev", "r"); if (!f) return;
    char line[512]; unsigned long long rx = 0, tx = 0;
    while (fgets(line, sizeof(line), f)){
        char ifn[64]; unsigned long long r, d1,d2,d3,d4,d5,d6,d7, t;
        char *c = strchr(line, ':'); if (!c) continue;
        *c = ' ';
        if (sscanf(line, "%63s %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   ifn, &r,&d1,&d2,&d3,&d4,&d5,&d6,&d7,&t) >= 10){
            if (strcmp(ifn, "lo")) { rx += r; tx += t; }
        }
    }
    fclose(f);
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    if (pn_t.tv_sec){
        double dt = (now.tv_sec - pn_t.tv_sec) + (now.tv_nsec - pn_t.tv_nsec) / 1e9;
        if (dt > 0.05 && rx >= pn_rx && tx >= pn_tx){
            st->rx_kbs = (rx - pn_rx) / dt / 1024.0;
            st->tx_kbs = (tx - pn_tx) / dt / 1024.0;
        }
    }
    pn_rx = rx; pn_tx = tx; pn_t = now;
}
static void read_disk(stats_t *st){
    struct statvfs v; const char *p = "/mnt/cache";
    if (statvfs(p, &v) != 0){ p = "/mnt/user"; if (statvfs(p, &v) != 0) return; }
    double tot = (double)v.f_blocks * v.f_frsize, freeb = (double)v.f_bfree * v.f_frsize;
    st->disk_tot_gb  = tot / 1e9; st->disk_used_gb = (tot - freeb) / 1e9;
    st->disk_used_pct = tot > 0 ? 100.0 * (tot - freeb) / tot : 0;
}
static void read_temp(stats_t *st){
    DIR *d = opendir("/sys/class/hwmon"); if (!d) return;
    struct dirent *e; int best = 0;
    while ((e = readdir(d))){
        if (e->d_name[0] == '.') continue;
        char p[512]; snprintf(p, sizeof(p), "/sys/class/hwmon/%s/name", e->d_name);
        FILE *f = fopen(p, "r"); if (!f) continue;
        char name[64] = ""; if (fgets(name, sizeof(name), f)) name[strcspn(name, "\n")] = 0;
        fclose(f);
        if (strcmp(name, "coretemp") && strcmp(name, "k10temp") && strcmp(name, "acpitz")) continue;
        snprintf(p, sizeof(p), "/sys/class/hwmon/%s/temp1_input", e->d_name);
        f = fopen(p, "r"); if (!f) continue;
        int t = 0; if (fscanf(f, "%d", &t) == 1 && t / 1000 > best) best = t / 1000;
        fclose(f);
    }
    closedir(d);
    st->temp_c = best;
}
static void read_misc(stats_t *st){
    gethostname(st->host, sizeof(st->host) - 1);
    st->ip[0] = 0;
    struct ifaddrs *ifa, *p;
    if (getifaddrs(&ifa) == 0){
        for (p = ifa; p; p = p->ifa_next){
            if (p->ifa_addr && p->ifa_addr->sa_family == AF_INET && strcmp(p->ifa_name, "lo")){
                inet_ntop(AF_INET, &((struct sockaddr_in*)p->ifa_addr)->sin_addr,
                          st->ip, sizeof(st->ip));
                break;
            }
        }
        freeifaddrs(ifa);
    }
    FILE *f = fopen("/proc/uptime", "r");
    if (f){ double u; if (fscanf(f, "%lf", &u) == 1) st->up_s = (long)u; fclose(f); }
    strcpy(st->arr, "");
    f = fopen("/var/local/emhttp/var.ini", "r");
    if (f){
        char line[256];
        while (fgets(line, sizeof(line), f))
            if (!strncmp(line, "mdState=", 8)){
                char *q1 = strchr(line, '"'), *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
                if (q1 && q2){ *q2 = 0; snprintf(st->arr, sizeof(st->arr), "%s", q1 + 1); }
                break;
            }
        fclose(f);
    }
}

/* ---------- UI ---------- */
#define C_TEXT   0xf2f7f7
#define C_DIM    0x9fb8bb
#define C_ACCENT 0x2dd4bf
#define C_WARN   0xf59e0b
#define C_BAD    0xef4444
#define C_CARD   0x0a1416

static void bar(int x, int y, int w, int h, double pct, uint32_t col){
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    rect(x, y, w, h, 0x1c2a2d, 255);
    rect(x, y, (int)(w * pct / 100.0), h, col, 255);
}
static uint32_t level_col(double pct){
    return pct > 90 ? C_BAD : pct > 75 ? C_WARN : C_ACCENT;
}
static void fmt_rate(char *out, size_t n, double kbs){
    if (kbs >= 1024) snprintf(out, n, "%.1f MB/s", kbs / 1024);
    else             snprintf(out, n, "%.0f KB/s", kbs);
}
static void card(int y, int h, const char *title){
    rect(10, y, W - 20, h, C_CARD, 200);
    rect(10, y, W - 20, 1, 0x28464a, 255);
    rect(10, y + h - 1, W - 20, 1, 0x061012, 255);
    if (title) text(20, y + 8, 1.6f, C_DIM, title);
}

static void render(stats_t *st){
    for (int y = 0; y < H; y++)
        memcpy(fbmem + y * fbpitch, bg + y * W, (size_t)W * 4);

    char b[128]; time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm);

    /* clock + date */
    strftime(b, sizeof(b), "%H:%M", &tm);
    text_c(26, 5.4f, C_TEXT, b);
    strftime(b, sizeof(b), "%a %d %b", &tm);
    text_c(86, 1.9f, C_DIM, b);

    /* identity */
    card(120, 78, NULL);
    text_c(132, 2.4f, C_ACCENT, st->host[0] ? st->host : "unraid");
    text_c(162, 1.9f, C_TEXT, st->ip[0] ? st->ip : "no ip");
    if (st->arr[0]){
        snprintf(b, sizeof(b), "array: %s", st->arr);
        text_c(182, 1.5f, strcmp(st->arr, "STARTED") ? C_WARN : C_DIM, b);
    }

    /* CPU */
    card(212, 96, "CPU");
    snprintf(b, sizeof(b), "%.0f%%", st->cpu);
    text(20, 236, 3.6f, C_TEXT, b);
    if (st->temp_c){
        snprintf(b, sizeof(b), "%dC", st->temp_c);
        text(W - 20 - text_w(2.2f, b), 244, 2.2f, st->temp_c > 85 ? C_BAD : C_DIM, b);
    }
    bar(20, 284, W - 40, 12, st->cpu, level_col(st->cpu));

    /* memory */
    card(322, 96, "MEMORY");
    double mp = st->mem_tot_mb ? 100.0 * st->mem_used_mb / st->mem_tot_mb : 0;
    snprintf(b, sizeof(b), "%.1f / %.1f GB", st->mem_used_mb / 1024.0, st->mem_tot_mb / 1024.0);
    text(20, 348, 2.2f, C_TEXT, b);
    bar(20, 394, W - 40, 12, mp, level_col(mp));

    /* network */
    card(432, 110, "NETWORK");
    fmt_rate(b + 4, sizeof(b) - 4, st->rx_kbs); memcpy(b, "DN  ", 4);
    text(20, 458, 2.0f, C_TEXT, b);
    fmt_rate(b + 4, sizeof(b) - 4, st->tx_kbs); memcpy(b, "UP  ", 4);
    text(20, 486, 2.0f, C_TEXT, b);
    rect(20, 514, W - 40, 1, 0x28464a, 255);

    /* storage */
    card(556, 96, "STORAGE");
    snprintf(b, sizeof(b), "%.0f / %.0f GB", st->disk_used_gb, st->disk_tot_gb);
    text(20, 582, 2.2f, C_TEXT, b);
    bar(20, 628, W - 40, 12, st->disk_used_pct, level_col(st->disk_used_pct));

    /* footer */
    long d = st->up_s / 86400, hh = st->up_s % 86400 / 3600, mm = st->up_s % 3600 / 60;
    if (d) snprintf(b, sizeof(b), "up %ldd %ldh %ldm", d, hh, mm);
    else   snprintf(b, sizeof(b), "up %ldh %ldm", hh, mm);
    text_c(906, 1.6f, C_DIM, b);
    text_c(930, 1.4f, 0x3a5a5e, "UNRAID  iDX6011 PRO");
}

/* ---------- backlight ---------- */
static void set_backlight(int pct){
    const char *base = "/sys/class/backlight/intel_backlight";
    char p[256]; long max = 0;
    snprintf(p, sizeof(p), "%s/max_brightness", base);
    FILE *f = fopen(p, "r"); if (!f) return;
    if (fscanf(f, "%ld", &max) != 1) max = 0;
    fclose(f);
    if (max <= 0) return;
    snprintf(p, sizeof(p), "%s/brightness", base);
    f = fopen(p, "w"); if (!f) return;
    fprintf(f, "%ld", max * pct / 100);
    fclose(f);
    snprintf(p, sizeof(p), "%s/bl_power", base);
    f = fopen(p, "w"); if (f){ fputs("0", f); fclose(f); }
}

/* ---------- main ---------- */
int main(int argc, char **argv){
    const char *bgpath = NULL; int interval = 1, once = 0, bl = 70;
    for (int i = 1; i < argc; i++){
        if (!strcmp(argv[i], "--bg") && i + 1 < argc) bgpath = argv[++i];
        else if (!strcmp(argv[i], "--interval") && i + 1 < argc) interval = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--backlight") && i + 1 < argc) bl = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--once")) once = 1;
    }

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
    set_backlight(bl);

    signal(SIGINT, on_sig); signal(SIGTERM, on_sig);

    stats_t st; memset(&st, 0, sizeof(st));
    read_cpu(&st); read_net(&st);            /* prime deltas */
    usleep(300 * 1000);

    int first = 1;
    while (!stop_flag){
        read_cpu(&st); read_mem(&st); read_net(&st);
        read_disk(&st); read_temp(&st); read_misc(&st);
        render(&st);
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
        sleep(interval);
    }
    fprintf(stderr, "exiting\n");
    return 0;
}
