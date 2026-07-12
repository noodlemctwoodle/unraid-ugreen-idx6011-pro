/*
 * plugin/src/panel_dash3.c
 *
 * Created by Toby G on 12/07/2026.
 *
 * panel_dash3.c — UGREEN iDX6011 Pro front-panel dashboard for Unraid (v3)
 * Renders live system stats on the 258x960 portrait eDP panel via DRM/KMS
 * dumb buffers. Seven touch-navigable pages (HOME / OVERVIEW / HARDWARE /
 * NETWORK / DISKS / DOCKER / SETTINGS) with Unraid branding, notification
 * banner, auto-rotation, interactive settings with flash persistence,
 * screen-off timeout and reboot/shutdown controls.
 * Deps: libdrm (runtime), stb_easy_font.h + stb_image.h (build, vendored)
 *
 * Build: gcc -O2 -w -o panel_dash3 panel_dash3.c -I/usr/include/libdrm -ldrm -lm
 *
 * CLI (v1/v2 flags unchanged; CLI overrides settings.cfg):
 *   --bg <img>          background image (png/jpg), scaled to panel
 *   --interval <sec>    stats refresh interval (default 1)
 *   --backlight <pct>   backlight brightness (default 75)
 *   --once              render one frame and exit
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
#include "stb_image.h"
#include "stb_easy_font.h"

/* ---------- framebuffer (unchanged from v1) ---------- */
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

/* ---------- text via stb_easy_font (unchanged from v1) ---------- */
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
static void text_bold(int x, int y, float s, uint32_t c, const char *str){
    text(x, y, s, c, str);
    text(x + 1, y, s, c, str);
}
static void trunc_fit(char *s, float scale, int maxw){
    size_t n = strlen(s);
    while (n > 0 && text_w(scale, s) > maxw) s[--n] = 0;
}

/* ---------- Unraid brand palette ---------- */
#define UN_RED       0xe22828   /* signature gradient start (red)      */
#define UN_ORANGE    0xff8c2f   /* signature gradient end (orange)     */
#define UN_ORANGE_M  0xf15a2c   /* midpoint — single-colour accent     */
#define UN_BLACK     0x1b1b1b   /* webGUI black-theme background       */
#define UN_GREY_90   0x141414
#define UN_GREY_80   0x262626   /* card fill                           */
#define UN_GREY_70   0x333333   /* dividers / card top edge            */
#define UN_TEXT      0xf2f2f2
#define UN_DIM       0x999999
#define UN_OK        0x3fb950   /* array STARTED                       */
#define UN_WARN      0xf0a020
#define UN_BAD       0xe22828

static uint32_t lerp_rgb(uint32_t a, uint32_t b, float t){
    if (t < 0) t = 0; if (t > 1) t = 1;
    int r  = (int)((a >> 16 & 255) + t * ((float)(b >> 16 & 255) - (float)(a >> 16 & 255)));
    int g  = (int)((a >>  8 & 255) + t * ((float)(b >>  8 & 255) - (float)(a >>  8 & 255)));
    int bl = (int)((a       & 255) + t * ((float)(b       & 255) - (float)(a       & 255)));
    return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)bl;
}

/* horizontal red->orange gradient strip — the signature Unraid rule */
static void hgrad(int x, int y, int w, int h, uint32_t c0, uint32_t c1){
    for (int i = 0; i < w; i++)
        rect(x + i, y, 1, h, lerp_rgb(c0, c1, w > 1 ? (float)i / (w - 1) : 0), 255);
}

/*
 * draw_unraid_logo — simplified geometric Unraid "glitch bars" mark.
 * Bounding box: 120*scale wide, 96*scale tall; (x,y) = top-left.
 */
static void draw_unraid_logo(int x, int y, float s){
    static const int up[7] = { 10, 30, 18, 48, 38, 14, 22 };
    static const int dn[7] = { 22, 14, 38, 48, 18, 30, 10 };
    int bw   = (int)(12 * s + 0.5f); if (bw < 2) bw = 2;
    int step = (int)(18 * s + 0.5f); if (step <= bw) step = bw + 1;
    int cy   = y + (int)(48 * s + 0.5f);
    int top  = y, bot = y + (int)(96 * s + 0.5f);
    if (bot <= top) bot = top + 1;
    for (int i = 0; i < 7; i++){
        int bx = x + i * step;
        int y0 = cy - (int)(up[i] * s + 0.5f);
        int y1 = cy + (int)(dn[i] * s + 0.5f);
        for (int yy = y0; yy < y1; yy++){
            uint32_t c = lerp_rgb(UN_ORANGE, UN_RED,
                                  (float)(yy - top) / (float)(bot - top));
            int in = (bw > 3 && (yy == y0 || yy == y1 - 1)) ? 1 : 0;
            rect(bx + in, yy, bw - 2 * in, 1, c, 255);
        }
    }
}

/* ---------- background (v1 image path; brand-dark fallback) ---------- */
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
        for (int y = 0; y < H; y++){            /* brand black -> near black */
            float t = (float)y / H;
            uint8_t g = (uint8_t)(27 - 14 * t);  /* 0x1b -> 0x0d */
            for (int x = 0; x < W; x++) bg[y * W + x] = (uint32_t)g << 16 | (uint32_t)g << 8 | g;
        }
    }
}

/* ---------- generic ini helpers (emhttp .ini / .notify files) ---------- */
static int ini_kv(char *line, char **k, char **v){
    char *eq = strchr(line, '='); if (!eq) return 0;
    *eq = 0; *k = line; *v = eq + 1;
    (*v)[strcspn(*v, "\r\n")] = 0;
    size_t n = strlen(*v);
    if (n >= 2 && (*v)[0] == '"' && (*v)[n-1] == '"'){
        (*v)[n-1] = 0; (*v)++;
        char *s = *v, *d = *v;                  /* unescape \" and \\ */
        while (*s){ if (*s == '\\' && (s[1] == '"' || s[1] == '\\')) s++; *d++ = *s++; }
        *d = 0;
    }
    return 1;
}
static int read_ull_file(const char *p, unsigned long long *out){
    FILE *f = fopen(p, "r"); if (!f) return 0;
    int ok = fscanf(f, "%llu", out) == 1; fclose(f); return ok;
}

/* ---------- stats ---------- */
#define MAX_IFACES 6
#define MAX_DISKS  12
#define MAX_CTRS   20

typedef struct {
    char name[32], ip[44], ip6[48];
    int up;
    unsigned long long rx_tot, tx_tot;
    double rx_kbs, tx_kbs;
} iface_t;

typedef struct {
    char name[32], type[16];
    int present, np, spun, health;   /* health: 0 ok, 1 warn, 2 bad, 3 grey */
    int temp;                        /* C, -1 = unavailable */
    long long errors;
    unsigned long long fs_size, fs_used, size_kib;   /* KiB */
} disk_t;

typedef struct {
    char name[40];
    char state[16];                  /* running / exited / paused / ...      */
    char status[48];                 /* "Up 3 hours" free text               */
} ctr_t;

typedef struct {
    char host[64], ip[48], arr[32];
    double cpu; long mem_used_mb, mem_tot_mb;
    double rx_kbs, tx_kbs;
    double disk_used_pct; double disk_tot_gb, disk_used_gb;
    int temp_c; long up_s;
    char prim_if[32];
    int n_ifaces; iface_t ifc[MAX_IFACES];
    int n_disks;  disk_t disks[MAX_DISKS];
    int gpu_avail; double gpu_busy; int gpu_freq;
    double pwr_sys_w, pwr_pkg_w;          /* RAPL watts; <0 = n/a */
    char cpu_model[96]; int cpu_threads;
    long mem_free_mb, mem_cache_mb;
    double boot_pct, log_pct, docker_pct;  /* <0 = n/a */
    int npu_avail; double npu_busy; int npu_freq, npu_max_freq;
    unsigned long long npu_mem;
    int fans_total, fans_on;
    int docker;                      /* running count, -1 = n/a */
    int docker_total;                /* total containers, -1 = n/a */
    int n_ctrs; ctr_t ctrs[MAX_CTRS];
    int vm_enabled, vm_count;
    int pkg_temp, nvme_temp, board_temp;
    char version[32], kernel[64];
    unsigned long long resync, resync_pos; char resync_act[32];
    double resync_mbs;
    int md_bad;                      /* disabled + invalid + missing */
    long long sync_errs;
    int mover;
    int notif_count, notif_imp;      /* imp: 0 normal, 1 warning, 2 alert */
    char notif_subj[128];
} stats_t;

static long now_ms(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* --- CPU (v1) --- */
static unsigned long long pc_busy, pc_tot;
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

/* --- memory (v1) --- */
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

/* --- network: default-route iface + per-iface rows --- */
static void primary_iface(char out[32]){
    strcpy(out, "br0");                        /* fallback */
    FILE *f = fopen("/proc/net/route", "r"); if (!f) return;
    char line[256];
    if (!fgets(line, sizeof line, f)){ fclose(f); return; }   /* header */
    while (fgets(line, sizeof line, f)){
        char ifn[32]; unsigned long dest;
        if (sscanf(line, "%31s %lx", ifn, &dest) == 2 && dest == 0){
            snprintf(out, 32, "%s", ifn); break;
        }
    }
    fclose(f);
}
static int iface_skip(const char *n){
    return !strcmp(n, "lo") || !strncmp(n, "tunl", 4) ||
           !strncmp(n, "sit", 3) || !strncmp(n, "veth", 4);
}
static struct { char name[32]; unsigned long long rx, tx; } pnet[MAX_IFACES];
static int pnet_n; static struct timespec pnet_t;

static void read_net(stats_t *st){
    primary_iface(st->prim_if);
    st->n_ifaces = 0;
    FILE *f = fopen("/proc/net/dev", "r"); if (!f) return;
    char line[512];
    if (!fgets(line, sizeof line, f) || !fgets(line, sizeof line, f)){ fclose(f); return; }
    while (fgets(line, sizeof line, f) && st->n_ifaces < MAX_IFACES){
        char *c = strchr(line, ':'); if (!c) continue;
        *c = ' ';
        char ifn[32]; unsigned long long rx, d1,d2,d3,d4,d5,d6,d7, tx;
        if (sscanf(line, "%31s %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   ifn, &rx,&d1,&d2,&d3,&d4,&d5,&d6,&d7,&tx) < 10) continue;
        if (iface_skip(ifn)) continue;
        iface_t *ic = &st->ifc[st->n_ifaces++];
        memset(ic, 0, sizeof *ic);
        snprintf(ic->name, sizeof ic->name, "%s", ifn);
        ic->rx_tot = rx; ic->tx_tot = tx;
        char p[128]; snprintf(p, sizeof p, "/sys/class/net/%s/operstate", ifn);
        FILE *g = fopen(p, "r");
        if (g){ char stbuf[16] = ""; if (fgets(stbuf, sizeof stbuf, g)) ic->up = !strncmp(stbuf, "up", 2); fclose(g); }
    }
    fclose(f);

    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    double dt = pnet_t.tv_sec
        ? (now.tv_sec - pnet_t.tv_sec) + (now.tv_nsec - pnet_t.tv_nsec) / 1e9 : 0;
    for (int i = 0; i < st->n_ifaces; i++){
        for (int j = 0; j < pnet_n; j++){
            if (strcmp(st->ifc[i].name, pnet[j].name)) continue;
            if (dt > 0.05 && st->ifc[i].rx_tot >= pnet[j].rx && st->ifc[i].tx_tot >= pnet[j].tx){
                st->ifc[i].rx_kbs = (st->ifc[i].rx_tot - pnet[j].rx) / dt / 1024.0;
                st->ifc[i].tx_kbs = (st->ifc[i].tx_tot - pnet[j].tx) / dt / 1024.0;
            }
            break;
        }
    }
    pnet_n = st->n_ifaces;
    for (int i = 0; i < pnet_n; i++){
        snprintf(pnet[i].name, sizeof pnet[i].name, "%s", st->ifc[i].name);
        pnet[i].rx = st->ifc[i].rx_tot; pnet[i].tx = st->ifc[i].tx_tot;
    }
    pnet_t = now;

    st->rx_kbs = st->tx_kbs = 0;               /* overview = primary iface only */
    int found = 0;
    for (int i = 0; i < st->n_ifaces; i++)
        if (!strcmp(st->ifc[i].name, st->prim_if)){
            st->rx_kbs = st->ifc[i].rx_kbs; st->tx_kbs = st->ifc[i].tx_kbs;
            found = 1; break;
        }
    if (!found && st->n_ifaces > 0){
        st->rx_kbs = st->ifc[0].rx_kbs; st->tx_kbs = st->ifc[0].tx_kbs;
    }
}

/* --- storage summary via statvfs (v1) --- */
static void read_disk(stats_t *st){
    struct statvfs v; const char *p = "/mnt/cache";
    if (statvfs(p, &v) != 0){ p = "/mnt/user"; if (statvfs(p, &v) != 0) return; }
    double tot = (double)v.f_blocks * v.f_frsize, freeb = (double)v.f_bfree * v.f_frsize;
    st->disk_tot_gb  = tot / 1e9; st->disk_used_gb = (tot - freeb) / 1e9;
    st->disk_used_pct = tot > 0 ? 100.0 * (tot - freeb) / tot : 0;
}

/* --- CPU temp via hwmon (v1) --- */
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

/* --- extra temps: x86 pkg (thermal zones), nvme + board (hwmon) --- */
static void read_zones(stats_t *st){
    st->pkg_temp = st->nvme_temp = st->board_temp = -1;
    DIR *d = opendir("/sys/class/thermal");
    if (d){
        struct dirent *e;
        while ((e = readdir(d))){
            if (strncmp(e->d_name, "thermal_zone", 12)) continue;
            char p[512]; snprintf(p, sizeof p, "/sys/class/thermal/%s/type", e->d_name);
            FILE *f = fopen(p, "r"); if (!f) continue;
            char ty[64] = ""; if (fgets(ty, sizeof ty, f)) ty[strcspn(ty, "\n")] = 0;
            fclose(f);
            if (strcmp(ty, "x86_pkg_temp")) continue;
            snprintf(p, sizeof p, "/sys/class/thermal/%s/temp", e->d_name);
            f = fopen(p, "r");
            if (f){ int t; if (fscanf(f, "%d", &t) == 1) st->pkg_temp = t / 1000; fclose(f); }
            break;
        }
        closedir(d);
    }
    d = opendir("/sys/class/hwmon");
    if (d){
        struct dirent *e;
        while ((e = readdir(d))){
            if (e->d_name[0] == '.') continue;
            char p[512]; snprintf(p, sizeof p, "/sys/class/hwmon/%s/name", e->d_name);
            FILE *f = fopen(p, "r"); if (!f) continue;
            char name[64] = ""; if (fgets(name, sizeof name, f)) name[strcspn(name, "\n")] = 0;
            fclose(f);
            int want_nvme = !strcmp(name, "nvme"), want_board = !strcmp(name, "acpitz");
            if (!want_nvme && !want_board) continue;
            snprintf(p, sizeof p, "/sys/class/hwmon/%s/temp1_input", e->d_name);
            f = fopen(p, "r"); if (!f) continue;
            int t;
            if (fscanf(f, "%d", &t) == 1){
                if (want_nvme  && st->nvme_temp  < 0) st->nvme_temp  = t / 1000;
                if (want_board && st->board_temp < 0) st->board_temp = t / 1000;
            }
            fclose(f);
        }
        closedir(d);
    }
    if (st->pkg_temp > 0) st->temp_c = st->pkg_temp;   /* prefer pkg zone */
}

/* --- fans: ACPI cooling devices (on/off only — no RPM on this box) --- */
static void read_fans(stats_t *st){
    st->fans_total = st->fans_on = 0;
    DIR *d = opendir("/sys/class/thermal"); if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))){
        if (strncmp(e->d_name, "cooling_device", 14)) continue;
        char p[512]; snprintf(p, sizeof p, "/sys/class/thermal/%s/type", e->d_name);
        FILE *f = fopen(p, "r"); if (!f) continue;
        char ty[64] = ""; if (fgets(ty, sizeof ty, f)) ty[strcspn(ty, "\n")] = 0;
        fclose(f);
        if (strcmp(ty, "Fan")) continue;
        st->fans_total++;
        snprintf(p, sizeof p, "/sys/class/thermal/%s/cur_state", e->d_name);
        f = fopen(p, "r");
        if (f){ int cs; if (fscanf(f, "%d", &cs) == 1 && cs > 0) st->fans_on++; fclose(f); }
    }
    closedir(d);
}

/* --- GPU busy via RC6 residency delta (gt0 ONLY — gt1 reads constant 0) --- */
static unsigned long long prev_rc6; static struct timespec prev_rc6_t;
/* --- power draw via Intel RAPL energy counters (psys = whole platform) --- */
static unsigned long long p_pkg_uj, p_sys_uj; static struct timespec p_pwr_t;
static void read_power(stats_t *st){
    st->pwr_sys_w = st->pwr_pkg_w = -1;
    unsigned long long pkg = 0, sys = 0;
    char path[128], name[32];
    for (int i = 0; i < 4; i++){
        snprintf(path, sizeof path, "/sys/class/powercap/intel-rapl:%d/name", i);
        FILE *f = fopen(path, "r"); if (!f) continue;
        name[0] = 0; if (fgets(name, sizeof name, f)) name[strcspn(name, "\n")] = 0;
        fclose(f);
        snprintf(path, sizeof path, "/sys/class/powercap/intel-rapl:%d/energy_uj", i);
        f = fopen(path, "r"); if (!f) continue;
        unsigned long long uj = 0;
        if (fscanf(f, "%llu", &uj) == 1){
            if (!strncmp(name, "package", 7)) pkg = uj;
            else if (!strcmp(name, "psys"))   sys = uj;
        }
        fclose(f);
    }
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    if (p_pwr_t.tv_sec){
        double dt = (now.tv_sec - p_pwr_t.tv_sec) + (now.tv_nsec - p_pwr_t.tv_nsec) / 1e9;
        if (dt > 0.2){
            if (pkg > p_pkg_uj) st->pwr_pkg_w = (pkg - p_pkg_uj) / dt / 1e6;
            if (sys > p_sys_uj) st->pwr_sys_w = (sys - p_sys_uj) / dt / 1e6;
        }
    }
    p_pkg_uj = pkg; p_sys_uj = sys; p_pwr_t = now;
}

/* --- cpu identity, memory breakdown, fs usage --- */
static double fs_pct(const char *path){
    struct statvfs v;
    if (statvfs(path, &v) != 0 || v.f_blocks == 0) return -1;
    return 100.0 * (double)(v.f_blocks - v.f_bfree) / (double)v.f_blocks;
}
static void read_about(stats_t *st){
    if (!st->cpu_model[0]){
        FILE *f = fopen("/proc/cpuinfo", "r");
        if (f){
            char line[256];
            while (fgets(line, sizeof line, f)){
                if (!strncmp(line, "model name", 10)){
                    char *c = strchr(line, ':');
                    if (c){
                        c += 2; line[strcspn(line, "\n")] = 0;
                        char *r = strstr(c, "Intel(R) Core(TM) ");
                        if (r) c = r + 18;
                        snprintf(st->cpu_model, sizeof st->cpu_model, "%s", c);
                    }
                    break;
                }
            }
            fclose(f);
        }
        st->cpu_threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    }
    FILE *f = fopen("/proc/meminfo", "r");
    if (f){
        char k[64]; long v; char u[16];
        long freem = 0, cached = 0, buffers = 0;
        while (fscanf(f, "%63s %ld %15s", k, &v, u) >= 2){
            if      (!strcmp(k, "MemFree:")) freem = v;
            else if (!strcmp(k, "Buffers:")) buffers = v;
            else if (!strcmp(k, "Cached:")){ cached = v; break; }
        }
        fclose(f);
        st->mem_free_mb  = freem / 1024;
        st->mem_cache_mb = (cached + buffers) / 1024;
    }
    st->boot_pct   = fs_pct("/boot");
    st->log_pct    = fs_pct("/var/log");
    st->docker_pct = fs_pct("/var/lib/docker");
}

static void read_gpu(stats_t *st){
    unsigned long long rc6;
    if (!read_ull_file("/sys/class/drm/card0/gt/gt0/rc6_residency_ms", &rc6)){
        st->gpu_avail = 0; return;
    }
    st->gpu_avail = 1;
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    if (prev_rc6_t.tv_sec){
        double wall_ms = (now.tv_sec - prev_rc6_t.tv_sec) * 1e3
                       + (now.tv_nsec - prev_rc6_t.tv_nsec) / 1e6;
        if (wall_ms > 50 && rc6 >= prev_rc6){
            double busy = 100.0 * (1.0 - (double)(rc6 - prev_rc6) / wall_ms);
            st->gpu_busy = busy < 0 ? 0 : busy > 100 ? 100 : busy;
        }
    }
    prev_rc6 = rc6; prev_rc6_t = now;
    unsigned long long fr;
    st->gpu_freq = read_ull_file("/sys/class/drm/card0/gt_act_freq_mhz", &fr) ? (int)fr : -1;
}

/* --- NPU busy via cumulative busy-time delta --- */
static unsigned long long prev_npu; static struct timespec prev_npu_t;
static void read_npu(stats_t *st){
    unsigned long long busy_us;
    if (!read_ull_file("/sys/class/accel/accel0/device/npu_busy_time_us", &busy_us)){
        st->npu_avail = 0; return;
    }
    st->npu_avail = 1;
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    if (prev_npu_t.tv_sec){
        double wall_us = (now.tv_sec - prev_npu_t.tv_sec) * 1e6
                       + (now.tv_nsec - prev_npu_t.tv_nsec) / 1e3;
        if (wall_us > 50000 && busy_us >= prev_npu){
            double b = 100.0 * (double)(busy_us - prev_npu) / wall_us;
            st->npu_busy = b > 100 ? 100 : b;
        }
    }
    prev_npu = busy_us; prev_npu_t = now;
    unsigned long long v;
    st->npu_freq     = read_ull_file("/sys/class/accel/accel0/device/npu_current_frequency_mhz", &v) ? (int)v : -1;
    st->npu_max_freq = read_ull_file("/sys/class/accel/accel0/device/npu_max_frequency_mhz", &v) ? (int)v : -1;
    st->npu_mem      = read_ull_file("/sys/class/accel/accel0/device/npu_memory_utilization", &v) ? v : 0;
}

/* --- docker via unix socket (generic GET helper, v3) --- */
static char dkbuf[262144];
static int docker_http_get(const char *path){
    int fd = socket(AF_UNIX, SOCK_STREAM, 0); if (fd < 0) return -1;
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    struct sockaddr_un a; memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    strcpy(a.sun_path, "/var/run/docker.sock");
    if (connect(fd, (struct sockaddr*)&a, sizeof a)){ close(fd); return -1; }
    char req[256];
    snprintf(req, sizeof req, "GET %s HTTP/1.0\r\nHost: docker\r\n\r\n", path);
    if (write(fd, req, strlen(req)) < 0){ close(fd); return -1; }
    int n = 0, r;
    while (n < (int)sizeof dkbuf - 1 && (r = read(fd, dkbuf + n, sizeof dkbuf - 1 - n)) > 0) n += r;
    close(fd); dkbuf[n] = 0;
    if (!strstr(dkbuf, "200 OK")) return -1;
    return n;
}

/* copy a JSON string value that follows `p` up to the closing quote */
static void json_copy_str(const char *p, char *out, size_t outsz){
    size_t i = 0;
    while (*p && *p != '"' && i < outsz - 1){
        if (*p == '\\' && p[1]){ p++; }        /* keep escaped char verbatim */
        out[i++] = *p++;
    }
    out[i] = 0;
}

/*
 * Minimal hand-rolled scan of GET /containers/json?all=1.
 * Per object we extract Names[0] (leading '/' stripped), State, Status.
 * Fields are matched by key between this object's "Names" and the next,
 * so nested Health.Status (which appears later) can't shadow them.
 */
static void docker_parse(stats_t *st){
    st->n_ctrs = 0;
    char *body = strstr(dkbuf, "\r\n\r\n");
    if (!body) return;
    body += 4;
    char *p = body;
    while (st->n_ctrs < MAX_CTRS && (p = strstr(p, "\"Names\":[\""))){
        p += 10;
        if (*p == '/') p++;
        ctr_t *c = &st->ctrs[st->n_ctrs];
        memset(c, 0, sizeof *c);
        json_copy_str(p, c->name, sizeof c->name);
        char *next = strstr(p, "\"Names\":[\"");
        char *s = strstr(p, "\"State\":\"");
        if (s && (!next || s < next)) json_copy_str(s + 9, c->state, sizeof c->state);
        char *t = strstr(p, "\"Status\":\"");
        if (t && (!next || t < next)) json_copy_str(t + 10, c->status, sizeof c->status);
        st->n_ctrs++;
        if (!next) break;
        p = next;
    }
}

/* --- VMs: SERVICE gate in domain.cfg; running count = qemu pidfiles --- */
static void read_vms(stats_t *st){
    st->vm_enabled = 0; st->vm_count = 0;
    FILE *f = fopen("/boot/config/domain.cfg", "r");
    if (f){
        char line[256];
        while (fgets(line, sizeof line, f)){
            char *k, *v;
            if (!ini_kv(line, &k, &v)) continue;
            if (!strcmp(k, "SERVICE")){ st->vm_enabled = !strcmp(v, "enable"); break; }
        }
        fclose(f);
    }
    if (!st->vm_enabled) return;
    DIR *d = opendir("/var/run/libvirt/qemu"); if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))){
        size_t l = strlen(e->d_name);
        if (l > 4 && !strcmp(e->d_name + l - 4, ".pid")) st->vm_count++;
    }
    closedir(d);
}

/* cached 10s: full container list + running/total + VM state */
static void read_docker(stats_t *st){
    static long last = -100000;
    static int c_run = -1, c_tot = -1, c_n;
    static ctr_t c_list[MAX_CTRS];
    static int c_vm_en, c_vm_cnt;
    long nowms = now_ms();
    if (nowms - last >= 10000){
        last = nowms;
        if (docker_http_get("/containers/json?all=1") >= 0){
            docker_parse(st);
            c_n = st->n_ctrs; c_tot = st->n_ctrs; c_run = 0;
            for (int i = 0; i < st->n_ctrs; i++)
                if (!strcmp(st->ctrs[i].state, "running")) c_run++;
            memcpy(c_list, st->ctrs, sizeof c_list);
        } else {
            c_run = c_tot = -1; c_n = 0;
        }
        read_vms(st);
        c_vm_en = st->vm_enabled; c_vm_cnt = st->vm_count;
    }
    st->docker = c_run; st->docker_total = c_tot;
    st->n_ctrs = c_n;
    memcpy(st->ctrs, c_list, sizeof c_list);
    st->vm_enabled = c_vm_en; st->vm_count = c_vm_cnt;
}

/* --- per-disk rows from /var/local/emhttp/disks.ini --- */
static void disks_commit(stats_t *st, disk_t *cur, int have){
    if (have && cur->present && !cur->np && cur->health != 3 && st->n_disks < MAX_DISKS)
        st->disks[st->n_disks++] = *cur;
}
static void read_disks(stats_t *st){
    st->n_disks = 0;
    FILE *f = fopen("/var/local/emhttp/disks.ini", "r"); if (!f) return;
    char line[512]; disk_t cur; int have = 0;
    memset(&cur, 0, sizeof cur);
    while (fgets(line, sizeof line, f)){
        if (line[0] == '['){
            disks_commit(st, &cur, have);
            memset(&cur, 0, sizeof cur); cur.temp = -1; have = 1;
            if (sscanf(line, "[\"%31[^\"]\"]", cur.name) != 1) have = 0;
            continue;
        }
        if (!have) continue;
        char *k, *v;
        if (!ini_kv(line, &k, &v)) continue;
        if      (!strcmp(k, "device"))    cur.present = v[0] != 0;
        else if (!strcmp(k, "status"))    cur.np = !strcmp(v, "DISK_NP");
        else if (!strcmp(k, "type"))      snprintf(cur.type, sizeof cur.type, "%s", v);
        else if (!strcmp(k, "temp"))      cur.temp = isdigit((unsigned char)v[0]) ? atoi(v) : -1;
        else if (!strcmp(k, "spundown"))  cur.spun = v[0] == '1';
        else if (!strcmp(k, "numErrors")) cur.errors = atoll(v);
        else if (!strcmp(k, "size"))      cur.size_kib = strtoull(v, NULL, 10);
        else if (!strcmp(k, "fsSize"))    cur.fs_size = strtoull(v, NULL, 10);
        else if (!strcmp(k, "fsUsed"))    cur.fs_used = strtoull(v, NULL, 10);
        else if (!strcmp(k, "color")){
            if      (!strncmp(v, "green", 5)) cur.health = 0;
            else if (strstr(v, "red"))        cur.health = 2;
            else if (!strncmp(v, "grey", 4))  cur.health = 3;
            else                              cur.health = 1;
        }
    }
    disks_commit(st, &cur, have);
    fclose(f);
}

/* --- notifications: /tmp/notifications/unread --- */
static void read_notif(stats_t *st){
    st->notif_count = 0; st->notif_imp = 0; st->notif_subj[0] = 0;
    DIR *d = opendir("/tmp/notifications/unread"); if (!d) return;
    struct dirent *e; time_t best = 0; char bestp[512] = "";
    while ((e = readdir(d))){
        size_t l = strlen(e->d_name);
        if (l < 8 || strcmp(e->d_name + l - 7, ".notify")) continue;
        char p[512]; snprintf(p, sizeof p, "/tmp/notifications/unread/%s", e->d_name);
        struct stat sb;
        if (stat(p, &sb) || !S_ISREG(sb.st_mode)) continue;
        if ((sb.st_mode & 0777) == 0400) continue;      /* hidden in WebGUI */
        st->notif_count++;
        if (sb.st_mtime >= best){ best = sb.st_mtime; snprintf(bestp, sizeof bestp, "%s", p); }
    }
    closedir(d);
    if (!st->notif_count || !bestp[0]) return;
    FILE *f = fopen(bestp, "r"); if (!f) return;
    char line[512], event[64] = "";
    while (fgets(line, sizeof line, f)){
        char *k, *v;
        if (!ini_kv(line, &k, &v)) continue;
        if      (!strcmp(k, "subject")) snprintf(st->notif_subj, sizeof st->notif_subj, "%s", v);
        else if (!strcmp(k, "event"))   snprintf(event, sizeof event, "%s", v);
        else if (!strcmp(k, "importance"))
            st->notif_imp = !strcmp(v, "alert") ? 2 : !strcmp(v, "warning") ? 1 : 0;
    }
    fclose(f);
    if (!st->notif_subj[0]) snprintf(st->notif_subj, sizeof st->notif_subj, "%s", event);
}

/* --- host / ip (v4 + global v6) / uptime / var.ini --- */
static void read_misc(stats_t *st){
    gethostname(st->host, sizeof(st->host) - 1);
    st->ip[0] = 0;
    struct ifaddrs *ifa, *p;
    if (getifaddrs(&ifa) == 0){
        char fallback[48] = "";
        for (p = ifa; p; p = p->ifa_next){
            if (!p->ifa_addr || !strcmp(p->ifa_name, "lo")) continue;
            if (p->ifa_addr->sa_family == AF_INET){
                char ipb[48];
                inet_ntop(AF_INET, &((struct sockaddr_in*)p->ifa_addr)->sin_addr, ipb, sizeof ipb);
                if (!fallback[0]) snprintf(fallback, sizeof fallback, "%s", ipb);
                if (!strcmp(p->ifa_name, st->prim_if) && !st->ip[0])
                    snprintf(st->ip, sizeof st->ip, "%s", ipb);
                for (int i = 0; i < st->n_ifaces; i++)
                    if (!strcmp(st->ifc[i].name, p->ifa_name) && !st->ifc[i].ip[0])
                        snprintf(st->ifc[i].ip, sizeof st->ifc[i].ip, "%s", ipb);
            } else if (p->ifa_addr->sa_family == AF_INET6){
                struct sockaddr_in6 *s6 = (struct sockaddr_in6*)p->ifa_addr;
                if (IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr) ||
                    IN6_IS_ADDR_LOOPBACK(&s6->sin6_addr)) continue;
                char ipb[48];
                inet_ntop(AF_INET6, &s6->sin6_addr, ipb, sizeof ipb);
                for (int i = 0; i < st->n_ifaces; i++)
                    if (!strcmp(st->ifc[i].name, p->ifa_name) && !st->ifc[i].ip6[0])
                        snprintf(st->ifc[i].ip6, sizeof st->ifc[i].ip6, "%s", ipb);
            }
        }
        if (!st->ip[0]) snprintf(st->ip, sizeof st->ip, "%s", fallback);
        freeifaddrs(ifa);
    }
    FILE *f = fopen("/proc/uptime", "r");
    if (f){ double u; if (fscanf(f, "%lf", &u) == 1) st->up_s = (long)u; fclose(f); }

    st->arr[0] = 0; st->resync = st->resync_pos = 0; st->resync_act[0] = 0;
    st->resync_mbs = 0; st->md_bad = 0; st->sync_errs = 0; st->mover = 0;
    f = fopen("/var/local/emhttp/var.ini", "r");
    if (f){
        char line[512];
        unsigned long long dt = 0, db = 0;
        while (fgets(line, sizeof line, f)){
            char *k, *v;
            if (!ini_kv(line, &k, &v)) continue;
            if      (!strcmp(k, "mdState"))          snprintf(st->arr, sizeof st->arr, "%s", v);
            else if (!strcmp(k, "version"))          snprintf(st->version, sizeof st->version, "%s", v);
            else if (!strcmp(k, "mdResync"))         st->resync = strtoull(v, NULL, 10);
            else if (!strcmp(k, "mdResyncPos"))      st->resync_pos = strtoull(v, NULL, 10);
            else if (!strcmp(k, "mdResyncAction"))   snprintf(st->resync_act, sizeof st->resync_act, "%s", v);
            else if (!strcmp(k, "mdResyncDt"))       dt = strtoull(v, NULL, 10);
            else if (!strcmp(k, "mdResyncDb"))       db = strtoull(v, NULL, 10);
            else if (!strcmp(k, "mdNumDisabled"))    st->md_bad += atoi(v);
            else if (!strcmp(k, "mdNumInvalid"))     st->md_bad += atoi(v);
            else if (!strcmp(k, "mdNumMissing"))     st->md_bad += atoi(v);
            else if (!strcmp(k, "sbSyncErrs"))       st->sync_errs = atoll(v);
            else if (!strcmp(k, "shareMoverActive")) st->mover = !strcmp(v, "yes");
        }
        if (dt > 0) st->resync_mbs = (double)db / (double)dt / 1024.0;
        fclose(f);
    }
    static int kdone; static char krnl[64];
    if (!kdone){
        struct utsname u;
        if (!uname(&u)) snprintf(krnl, sizeof krnl, "%s", u.release);
        kdone = 1;
    }
    snprintf(st->kernel, sizeof st->kernel, "%s", krnl);
}

/* ---------- sparkline history (60 samples, pushed each stats tick) ---------- */
#define SPARK_N 60
static float h_cpu[SPARK_N], h_gpu[SPARK_N];
static int h_cnt, h_pos;
static void hist_push(double cpu, double gpu){
    h_cpu[h_pos] = (float)cpu; h_gpu[h_pos] = (float)gpu;
    h_pos = (h_pos + 1) % SPARK_N;
    if (h_cnt < SPARK_N) h_cnt++;
}

/* ---------- UI primitives ---------- */
static void bar(int x, int y, int w, int h, double pct, uint32_t col){
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    rect(x, y, w, h, 0x2a2a2a, 255);
    rect(x, y, (int)(w * pct / 100.0), h, col, 255);
}
static uint32_t level_col(double pct){
    return pct > 90 ? UN_BAD : pct > 75 ? UN_WARN : UN_ORANGE_M;
}
static void fmt_rate(char *out, size_t n, double kbs){
    if (kbs >= 1024) snprintf(out, n, "%.1f MB/s", kbs / 1024);
    else             snprintf(out, n, "%.0f KB/s", kbs);
}
static void fmt_rate_s(char *out, size_t n, double kbs){   /* compact, for tiles */
    if (kbs >= 1024) snprintf(out, n, "%.1fM", kbs / 1024);
    else             snprintf(out, n, "%.0fK", kbs);
}
static void fmt_bytes(char *out, size_t n, unsigned long long b){
    double v = (double)b;
    if      (v >= 1e12) snprintf(out, n, "%.2f TB", v / 1e12);
    else if (v >= 1e9)  snprintf(out, n, "%.1f GB", v / 1e9);
    else if (v >= 1e6)  snprintf(out, n, "%.0f MB", v / 1e6);
    else                snprintf(out, n, "%.0f KB", v / 1e3);
}
static void fmt_up(char *b, size_t n, long up){
    long d = up / 86400, hh = up % 86400 / 3600, mm = up % 3600 / 60;
    if (d) snprintf(b, n, "up %ldd %ldh %ldm", d, hh, mm);
    else   snprintf(b, n, "up %ldh %ldm", hh, mm);
}
static void card(int y, int h, const char *title){
    rect(10, y, W - 20, h, UN_GREY_80, 230);
    rect(10, y, W - 20, 1, UN_GREY_70, 255);
    rect(10, y, 3, h, UN_ORANGE_M, 255);          /* webGUI-style orange spine */
    if (title) text(22, y + 10, 1.9f, UN_DIM, title);
}

/* ring gauge — filled annulus, clockwise from 12 o'clock; no libs */
static void ring_gauge(int cx, int cy, int ro, int ri, double pct, uint32_t col){
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    float lim = (float)(pct / 100.0 * 2.0 * M_PI);
    int ro2 = ro * ro, ri2 = ri * ri;
    for (int dy = -ro; dy <= ro; dy++)
        for (int dx = -ro; dx <= ro; dx++){
            int d2 = dx * dx + dy * dy;
            if (d2 > ro2 || d2 < ri2) continue;
            float ang = atan2f((float)dx, (float)-dy);   /* 0 at top, cw + */
            if (ang < 0) ang += 2.0f * (float)M_PI;
            px(cx + dx, cy + dy, ang <= lim ? col : 0x2a2a2a);
        }
}

/* sparkline — connected 2px columns over a 0..100 ring buffer */
static void spark(int x, int y, int w, int h, const float *vals, int cnt, int pos,
                  uint32_t col){
    rect(x, y, w, h, 0x202020, 255);
    hline(x, y + h - 1, w, 0x383838);
    if (cnt < 2) return;
    int prev_yv = 0;
    for (int i = 0; i < cnt; i++){
        float v = vals[(pos - cnt + i + 2 * SPARK_N) % SPARK_N];
        if (v < 0) v = 0; if (v > 100) v = 100;
        int yv = y + h - 1 - (int)(v / 100.0f * (h - 2));
        int xc = x + (int)((float)i * (w - 2) / (SPARK_N - 1));
        if (i > 0){
            int y0 = yv < prev_yv ? yv : prev_yv;
            int y1 = yv > prev_yv ? yv : prev_yv;
            rect(xc, y0, 2, y1 - y0 + 2, col, 255);
        }
        prev_yv = yv;
    }
}

/* small home-page tile */
static void tile(int x, int y, int w, int h, const char *label){
    rect(x, y, w, h, UN_GREY_80, 230);
    rect(x, y, w, 1, UN_GREY_70, 255);
    text(x + 10, y + 8, 1.4f, UN_DIM, label);
}

/* ---------- settings (flash-persistent, webGUI-editable) ---------- */
#define CFG_DIR1 "/boot/config/plugins/ugreen-idx6011-pro"
#define CFG_DIR2 "/boot/config/plugins/ugreen-idx6011-pro/panel"
#define CFG_PATH "/boot/config/plugins/ugreen-idx6011-pro/panel/settings.cfg"

static int cfg_brightness = 75;     /* 5..100 */
static int cfg_interval   = 1;      /* stats seconds */
static int cfg_rotate     = 0;      /* auto-rotate seconds: 0/10/20/60 */
static int cfg_screen_off = 0;      /* minutes: 0(never)/1/5/15 */
static int cfg_night      = 0;      /* 1 = clamp brightness to 15% */
static int cfg_leds       = 1;      /* chassis LEDs on/off */

static void settings_load(void){
    FILE *f = fopen(CFG_PATH, "r"); if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)){
        char *k, *v;
        if (!ini_kv(line, &k, &v)) continue;
        if      (!strcmp(k, "BRIGHTNESS"))     cfg_brightness = atoi(v);
        else if (!strcmp(k, "INTERVAL"))       cfg_interval   = atoi(v);
        else if (!strcmp(k, "ROTATE"))         cfg_rotate     = atoi(v);
        else if (!strcmp(k, "SCREEN_OFF_MIN")) cfg_screen_off = atoi(v);
        else if (!strcmp(k, "NIGHT"))          cfg_night      = atoi(v) != 0;
        else if (!strcmp(k, "LEDS"))           cfg_leds       = atoi(v) != 0;
    }
    fclose(f);
    if (cfg_brightness < 5)   cfg_brightness = 5;
    if (cfg_brightness > 100) cfg_brightness = 100;
    if (cfg_interval < 1)     cfg_interval = 1;
}

/* atomic write (tmp + rename); called only when a value actually changed */
static void settings_save(void){
    mkdir(CFG_DIR1, 0755); mkdir(CFG_DIR2, 0755);
    char tmp[sizeof CFG_PATH + 8];
    snprintf(tmp, sizeof tmp, "%s.tmp", CFG_PATH);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    fprintf(f, "BRIGHTNESS=%d\nINTERVAL=%d\nROTATE=%d\n"
               "SCREEN_OFF_MIN=%d\nNIGHT=%d\nLEDS=%d\n",
            cfg_brightness, cfg_interval, cfg_rotate,
            cfg_screen_off, cfg_night, cfg_leds);
    fclose(f);
    rename(tmp, CFG_PATH);
}

/* ---------- backlight (v1 core + bl_power for screen-off) ---------- */
/*
 * Brightness on this panel is EC-controlled (ITE IT55xx, ports 0x62/0x66,
 * EC-RAM 0xA5: 1 = full ... 198 = off). The i915 intel_backlight PWM only
 * gates on/off, so levels are written to the EC; intel_backlight is pinned
 * to max + unblanked so the EC is the sole dimmer.
 */
static int ec_ok = -1;
static void ec_wait_ibf(void){ for (int i = 0; i < 5000 && (inb(0x66) & 2); i++) usleep(100); }
static void ec_write(unsigned char addr, unsigned char val){
    if (ec_ok < 0) ec_ok = (ioperm(0x62, 1, 1) == 0 && ioperm(0x66, 1, 1) == 0);
    if (!ec_ok) return;
    ec_wait_ibf(); outb(0x81, 0x66);
    ec_wait_ibf(); outb(addr, 0x62);
    ec_wait_ibf(); outb(val, 0x62);
    ec_wait_ibf();
}
static void set_backlight(int pct){
    const char *base = "/sys/class/backlight/intel_backlight";
    char p[256]; long max = 0;
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    /* EC: the real dimmer. 100% -> 1, 1% -> ~196, 0% -> 198 (off) */
    unsigned char v = pct == 0 ? 198 : (unsigned char)(1 + (100 - pct) * 197 / 100);
    ec_write(0xA5, v);
    /* i915 PWM pinned to max + unblanked */
    snprintf(p, sizeof(p), "%s/max_brightness", base);
    FILE *f = fopen(p, "r"); if (!f) return;
    if (fscanf(f, "%ld", &max) != 1) max = 0;
    fclose(f);
    if (max <= 0) return;
    snprintf(p, sizeof(p), "%s/brightness", base);
    f = fopen(p, "w"); if (!f) return;
    fprintf(f, "%ld", max);
    fclose(f);
    snprintf(p, sizeof(p), "%s/bl_power", base);
    f = fopen(p, "w"); if (f){ fputs("0", f); fclose(f); }
}
/* FB_BLANK: 0 = unblank (on), 4 = powerdown (off) */
static void set_bl_power(int on){
    FILE *f = fopen("/sys/class/backlight/intel_backlight/bl_power", "w");
    if (f){ fputs(on ? "0" : "4", f); fclose(f); }
}
static int eff_brightness(void){
    int b = cfg_brightness;
    if (cfg_night && b > 15) b = 15;
    return b;
}
static void apply_brightness(void){ set_backlight(eff_brightness()); }

/* ---------- non-blocking action spawns ---------- */
static char **g_argv;               /* saved for RESTART DASH re-exec */

static void spawn_shell(const char *cmd){
    pid_t pid = fork();
    if (pid == 0){
        setsid();
        int nul = open("/dev/null", O_RDWR);
        if (nul >= 0){ dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); if (nul > 2) close(nul); }
        execl("/bin/bash", "bash", "-c", cmd, (char*)NULL);
        _exit(127);
    }
}
static void do_restart_dash(void){
    execv("/proc/self/exe", g_argv);   /* fds are all O_CLOEXEC */
    execv(g_argv[0], g_argv);          /* fallback */
}
static void do_reboot(void){ spawn_shell("/sbin/reboot"); }
static void do_shutdown(void){
    if (access("/usr/local/sbin/powerdown", X_OK) == 0)
        spawn_shell("/usr/local/sbin/powerdown");
    else
        spawn_shell("/sbin/poweroff");
}
static void apply_leds(void){
    spawn_shell(cfg_leds
        ? "bash /boot/config/plugins/ugreen-idx6011-pro/start.sh"
        : "bash /boot/config/plugins/ugreen-idx6011-pro/stop.sh");
}

/* ---------- page system ---------- */
#define NPAGES    7
#define HEADER_H  84
#define BODY_Y0   124
#define FOOTER_Y  918
#define SCROLL_STEP 300
static long rotate_ms = 0;              /* auto-rotate off by default */
static int scrolly[NPAGES];
static int page_end;                    /* set by each page: content bottom y */
static int content_h[NPAGES];

static int cur_page;
static int body_top = BODY_Y0;

#define PAGE_SETTINGS 6

typedef void (*page_fn)(stats_t *st);
static void page_home(stats_t *st);
static void page_overview(stats_t *st);
static void page_hardware(stats_t *st);
static void page_network(stats_t *st);
static void page_disks(stats_t *st);
static void page_docker(stats_t *st);
static void page_settings(stats_t *st);
static const struct { const char *title; page_fn body; } pages[NPAGES] = {
    { "HOME",     page_home     },
    { "OVERVIEW", page_overview },
    { "HARDWARE", page_hardware },
    { "NETWORK",  page_network  },
    { "DISKS",    page_disks    },
    { "DOCKER",   page_docker   },
    { "SETTINGS", page_settings },
};

static void draw_header(stats_t *st){
    rect(0, 0, W, BODY_Y0 - 6, UN_BLACK, 255);   /* opaque incl. title zone */
    draw_unraid_logo(12, 15, 0.5f);                       /* 60x48 mark */
    if (st->notif_count > 0){
        char nb[16];
        snprintf(nb, sizeof nb, "%d", st->notif_count > 99 ? 99 : st->notif_count);
        int tw = text_w(1.5f, nb);
        uint32_t bc = st->notif_imp == 2 ? UN_BAD : st->notif_imp == 1 ? UN_WARN : UN_ORANGE;
        rect(76, 12, tw + 12, 17, bc, 255);
        text(82, 16, 1.5f, 0x1b1b1b, nb);
    }
    char b[64]; time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm);
    strftime(b, sizeof b, "%H:%M", &tm);
    text(W - 16 - text_w(3.2f, b), 18, 3.2f, UN_TEXT, b);
    strftime(b, sizeof b, "%a %d %b", &tm);
    text(W - 16 - text_w(1.5f, b), 52, 1.5f, UN_DIM, b);
    hgrad(0, HEADER_H, W, 3, UN_RED, UN_ORANGE);          /* signature rule */
    text_c(HEADER_H + 18, 2.2f, UN_DIM, pages[cur_page].title);
    snprintf(b, sizeof b, "%d/%d", cur_page + 1, NPAGES);
    text(W - 16 - text_w(1.4f, b), HEADER_H + 22, 1.4f, 0x555555, b);
}

static void draw_banner(stats_t *st){
    uint32_t c = st->notif_imp == 2 ? UN_BAD : st->notif_imp == 1 ? UN_WARN : UN_ORANGE_M;
    rect(10, BODY_Y0, W - 20, 36, UN_GREY_80, 245);
    rect(10, BODY_Y0, 3, 36, c, 255);
    char b[32]; snprintf(b, sizeof b, "%d UNREAD", st->notif_count);
    text(22, BODY_Y0 + 5, 1.3f, c, b);
    char subj[128];
    snprintf(subj, sizeof subj, "%s", st->notif_subj[0] ? st->notif_subj : "notification");
    trunc_fit(subj, 1.5f, W - 44);
    text(22, BODY_Y0 + 19, 1.5f, UN_TEXT, subj);
}

static void draw_footer(void){
    rect(0, FOOTER_Y - 4, W, H - (FOOTER_Y - 4), UN_BLACK, 255);
    hline(16, FOOTER_Y, W - 32, UN_GREY_70);
    int pitch = 24, x0 = (W - (NPAGES - 1) * pitch) / 2;
    for (int i = 0; i < NPAGES; i++){
        int cx = x0 + i * pitch;
        if (i == cur_page){
            rect(cx - 5, 932, 10, 10, UN_ORANGE, 255);
            rect(cx - 4, 931, 8, 12, UN_ORANGE, 255);
        } else {
            rect(cx - 3, 934, 6, 6, 0x555555, 255);
        }
    }
}

/* ---------- settings widget hit-testing (static, no allocation) ---------- */
enum {
    WID_NONE = 0, WID_BRIGHT, WID_SCROFF, WID_ROTATE, WID_LEDS,
    WID_NIGHT, WID_RESTART, WID_REBOOT, WID_SHUTDOWN
};
typedef struct { int id, x, y, w, h; } hitbox_t;
static hitbox_t hbs[16];
static int hb_n;
static void hb_add(int id, int x, int y, int w, int h){
    if (hb_n < (int)(sizeof hbs / sizeof hbs[0])){
        hbs[hb_n].id = id; hbs[hb_n].x = x; hbs[hb_n].y = y;
        hbs[hb_n].w = w;  hbs[hb_n].h = h; hb_n++;
    }
}
static int hb_find(int x, int y){
    for (int i = 0; i < hb_n; i++)
        if (x >= hbs[i].x && x < hbs[i].x + hbs[i].w &&
            y >= hbs[i].y && y < hbs[i].y + hbs[i].h)
            return hbs[i].id;
    return WID_NONE;
}

/* confirm state for REBOOT / SHUTDOWN (5s window after LONG_PRESS) */
static int  confirm_which;              /* WID_REBOOT / WID_SHUTDOWN / 0 */
static long confirm_until;

/* slider track geometry (fixed; W is runtime-constant after modeset) */
#define SLIDER_X 22
#define SLIDER_W (W - 44)

/* ---------- pages ---------- */
/* one full-width stat tile: label left, big value right, bar underneath */
static void home_tile(int y, int h, const char *label, const char *val,
                      uint32_t vcol, double barpct, uint32_t barcol){
    int x = 10, w = W - 20;
    tile(x, y, w, h, label);
    text(x + w - 14 - text_w(3.0f, val), y + 24, 3.0f, vcol, val);
    if (barpct >= 0)
        bar(x + 14, y + h - 22, w - 28, 12, barpct, barcol);
}

static void page_home(stats_t *st){
    char b[64], r1[24], r2[24];
    int y = body_top;

    /* CPU ring gauge is the hero (header already shows clock + date) */
    int cx = W / 2, cy = y + 82, ro = 74, ri = 56;
    ring_gauge(cx, cy, ro, ri, st->cpu, level_col(st->cpu));
    snprintf(b, sizeof b, "%.0f%%", st->cpu);
    text_bold(cx - text_w(3.4f, b) / 2 - 1, cy - 22, 3.4f, UN_TEXT, b);
    text(cx - text_w(1.6f, "CPU") / 2, cy + 16, 1.6f, UN_DIM, "CPU");
    y += 178;

    int th = 80;

    double mp = st->mem_tot_mb ? 100.0 * st->mem_used_mb / st->mem_tot_mb : 0;
    snprintf(b, sizeof b, "%.0f%%", mp);
    home_tile(y, th, "RAM", b, UN_TEXT, mp, level_col(mp)); y += th + 10;

    if (st->temp_c > 0) snprintf(b, sizeof b, "%dC", st->temp_c);
    else                snprintf(b, sizeof b, "--");
    home_tile(y, th, "CPU TEMP", b, st->temp_c > 85 ? UN_BAD : UN_TEXT,
              st->temp_c > 0 ? st->temp_c : 0, st->temp_c > 85 ? UN_BAD : UN_ORANGE_M);
    y += th + 10;

    fmt_rate_s(r1, sizeof r1, st->rx_kbs);
    fmt_rate_s(r2, sizeof r2, st->tx_kbs);
    tile(10, y, W - 20, th, "NETWORK");
    snprintf(b, sizeof b, "D %s", r1); text(24, y + 30, 2.2f, UN_TEXT, b);
    snprintf(b, sizeof b, "U %s", r2); text(24, y + 56, 2.2f, UN_TEXT, b);
    y += th + 10;

    snprintf(b, sizeof b, "%.0f%%", st->disk_used_pct);
    home_tile(y, th, "STORAGE", b, UN_TEXT, st->disk_used_pct, level_col(st->disk_used_pct));
    y += th + 10;

    /* power draw (RAPL package; psys is miscalibrated on this board) */
    double pw = st->pwr_pkg_w;
    if (st->pwr_sys_w > st->pwr_pkg_w) pw = st->pwr_sys_w;
    if (pw >= 0) snprintf(b, sizeof b, "%.1f W", pw);
    else         snprintf(b, sizeof b, "n/a");
    home_tile(y, th, "POWER", b, UN_ORANGE, -1, 0);
    y += th + 10;

    fmt_up(b, sizeof b, st->up_s);
    home_tile(y, th, "UPTIME", strncmp(b, "up ", 3) ? b : b + 3, UN_TEXT, -1, 0);
    y += th + 14;
    page_end = y;
}

static void page_overview(stats_t *st){
    char b[128];
    int y = body_top;

    card(y, 112, NULL);
    text_bold((W - text_w(2.9f, st->host[0] ? st->host : "unraid")) / 2 - 1,
              y + 14, 2.9f, UN_TEXT, st->host[0] ? st->host : "unraid");
    text_c(y + 52, 2.3f, UN_DIM, st->ip[0] ? st->ip : "no ip");
    if (st->arr[0]){
        if (st->resync > 0){
            double pct = 100.0 * (double)st->resync_pos / (double)st->resync;
            snprintf(b, sizeof b, "%s %.1f%%",
                     st->resync_act[0] ? st->resync_act : "sync", pct);
            text_c(y + 82, 1.8f, UN_WARN, b);
        } else {
            snprintf(b, sizeof b, "array: %s", st->arr);
            text_c(y + 82, 1.8f, strcmp(st->arr, "STARTED") ? UN_WARN : UN_OK, b);
        }
    }
    y += 124;

    card(y, 142, "CPU");
    snprintf(b, sizeof b, "%.0f%%", st->cpu);
    text(22, y + 34, 4.8f, UN_TEXT, b);
    if (st->temp_c > 0){
        snprintf(b, sizeof b, "%dC", st->temp_c);
        text(W - 22 - text_w(2.8f, b), y + 44, 2.8f, st->temp_c > 85 ? UN_BAD : UN_DIM, b);
    }
    /* psys RAPL is miscalibrated on this board (reads ~0); package matches the webUI */
    double pw = st->pwr_pkg_w;
    if (st->pwr_sys_w > st->pwr_pkg_w) pw = st->pwr_sys_w;
    if (pw >= 0){
        snprintf(b, sizeof b, "%.1f W", pw);
        text(W - 22 - text_w(2.0f, b), y + 80, 2.0f, UN_DIM, b);
    }
    bar(22, y + 108, W - 44, 18, st->cpu, level_col(st->cpu));
    y += 154;

    card(y, 128, "MEMORY");
    double mp = st->mem_tot_mb ? 100.0 * st->mem_used_mb / st->mem_tot_mb : 0;
    snprintf(b, sizeof b, "%.1f / %.1f GB", st->mem_used_mb / 1024.0, st->mem_tot_mb / 1024.0);
    text(22, y + 40, 2.7f, UN_TEXT, b);
    snprintf(b, sizeof b, "%.0f%%", mp);
    text(W - 22 - text_w(2.0f, b), y + 44, 2.0f, UN_DIM, b);
    bar(22, y + 94, W - 44, 18, mp, level_col(mp));
    y += 140;

    card(y, 128, "NETWORK");
    text(W - 22 - text_w(1.7f, st->prim_if), y + 10, 1.7f, UN_DIM, st->prim_if);
    fmt_rate(b + 4, sizeof(b) - 4, st->rx_kbs); memcpy(b, "DN  ", 4);
    text(22, y + 40, 2.5f, UN_TEXT, b);
    fmt_rate(b + 4, sizeof(b) - 4, st->tx_kbs); memcpy(b, "UP  ", 4);
    text(22, y + 82, 2.5f, UN_TEXT, b);
    y += 140;

    card(y, 128, "STORAGE");
    snprintf(b, sizeof b, "%.0f / %.0f GB", st->disk_used_gb, st->disk_tot_gb);
    text(22, y + 40, 2.7f, UN_TEXT, b);
    snprintf(b, sizeof b, "%.0f%%", st->disk_used_pct);
    text(W - 22 - text_w(2.0f, b), y + 44, 2.0f, UN_DIM, b);
    bar(22, y + 94, W - 44, 18, st->disk_used_pct, level_col(st->disk_used_pct));
    y += 140;

    fmt_up(b, sizeof b, st->up_s);
    text_c(y + 8, 1.9f, UN_DIM, b);
    page_end = y + 34;
}

static void page_hardware(stats_t *st){
    char b[128];
    int y = body_top;

    card(y, 152, "CPU");
    snprintf(b, sizeof b, "%.0f%%", st->cpu);
    text(22, y + 30, 3.6f, UN_TEXT, b);
    if (st->cpu_threads > 0){
        snprintf(b, sizeof b, "%dT", st->cpu_threads);
        text(W - 22 - text_w(1.8f, b), y + 12, 1.8f, UN_DIM, b);
    }
    spark(22, y + 78, W - 44, 62, h_cpu, h_cnt, h_pos, UN_ORANGE_M);
    y += 164;

    card(y, 84, "CPU TEMP");
    if (st->temp_c > 0){
        snprintf(b, sizeof b, "%dC", st->temp_c);
        text(W - 22 - text_w(2.4f, b), y + 10, 2.4f,
             st->temp_c > 85 ? UN_BAD : UN_TEXT, b);
        bar(22, y + 46, W - 44, 18, st->temp_c,
            st->temp_c > 85 ? UN_BAD : st->temp_c > 70 ? UN_WARN : UN_ORANGE_M);
    } else {
        text(22, y + 44, 2.0f, UN_DIM, "n/a");
    }
    y += 96;

    card(y, 108, "MEMORY");
    double mp = st->mem_tot_mb ? 100.0 * st->mem_used_mb / st->mem_tot_mb : 0;
    snprintf(b, sizeof b, "%.1f / %.1f GB", st->mem_used_mb / 1024.0, st->mem_tot_mb / 1024.0);
    text(22, y + 34, 2.4f, UN_TEXT, b);
    snprintf(b, sizeof b, "%.0f%%", mp);
    text(W - 22 - text_w(1.9f, b), y + 37, 1.9f, UN_DIM, b);
    bar(22, y + 74, W - 44, 16, mp, level_col(mp));
    y += 120;

    card(y, 152, "GPU");
    if (st->gpu_avail){
        snprintf(b, sizeof b, "%.0f%%", st->gpu_busy);
        text(22, y + 30, 3.6f, UN_TEXT, b);
        if (st->gpu_freq >= 0){
            snprintf(b, sizeof b, "%d MHz", st->gpu_freq);
            text(W - 22 - text_w(1.9f, b), y + 12, 1.9f, UN_DIM, b);
        }
        spark(22, y + 78, W - 44, 62, h_gpu, h_cnt, h_pos, UN_ORANGE);
    } else {
        text(22, y + 64, 2.0f, UN_DIM, "not available");
    }
    y += 164;

    card(y, 66, "NPU");
    if (st->npu_avail){
        if (st->npu_freq > 0) snprintf(b, sizeof b, "busy %.0f%%  %d/%d MHz",
                                       st->npu_busy, st->npu_freq, st->npu_max_freq);
        else                  snprintf(b, sizeof b, "present  (idle)");
        text(22, y + 36, 1.9f, UN_TEXT, b);
    } else {
        text(22, y + 36, 1.9f, UN_DIM, "not present");
    }
    y += 78;

    card(y, 66, "POWER");
    if (st->pwr_pkg_w >= 0){
        if (st->pwr_sys_w > st->pwr_pkg_w)
            snprintf(b, sizeof b, "%.1f W  (cpu %.1f W)", st->pwr_sys_w, st->pwr_pkg_w);
        else
            snprintf(b, sizeof b, "%.1f W", st->pwr_pkg_w);
        text(22, y + 34, 2.2f, UN_TEXT, b);
    } else {
        text(22, y + 36, 1.9f, UN_DIM, "n/a");
    }
    page_end = y + 78;
}

static void page_network(stats_t *st){
    char b[128], r1[32], r2[32];
    int y = body_top;

    iface_t *prim = NULL;
    for (int i = 0; i < st->n_ifaces; i++)
        if (!strcmp(st->ifc[i].name, st->prim_if)){ prim = &st->ifc[i]; break; }

    card(y, 158, "PRIMARY");
    text(22, y + 32, 2.7f, UN_TEXT, st->prim_if);
    text(22, y + 64, 2.0f, UN_DIM, st->ip[0] ? st->ip : "no ip");
    fmt_rate(r1, sizeof r1, st->rx_kbs); fmt_rate(r2, sizeof r2, st->tx_kbs);
    snprintf(b, sizeof b, "DN  %s", r1);
    text(22, y + 94, 2.4f, UN_TEXT, b);
    snprintf(b, sizeof b, "UP  %s", r2);
    text(22, y + 124, 2.4f, UN_TEXT, b);
    y += 170;

    card(y, 112, "SINCE BOOT");
    if (prim){
        fmt_bytes(r1, sizeof r1, prim->rx_tot);
        snprintf(b, sizeof b, "RX  %s", r1);
        text(22, y + 36, 2.3f, UN_TEXT, b);
        fmt_bytes(r2, sizeof r2, prim->tx_tot);
        snprintf(b, sizeof b, "TX  %s", r2);
        text(22, y + 70, 2.3f, UN_TEXT, b);
    } else {
        text(22, y + 44, 2.0f, UN_DIM, "no data");
    }
    y += 124;

    for (int i = 0; i < st->n_ifaces; i++){
        iface_t *ic = &st->ifc[i];
        char r1b[32], r2b[32];
        int has6 = ic->ip6[0] != 0;
        int ch = has6 ? 122 : 104;
        card(y, ch, NULL);
        rect(22, y + 16, 10, 10, ic->up ? UN_OK : 0x555555, 255);
        text(40, y + 12, 2.2f, UN_TEXT, ic->name);
        fmt_rate(r1b, sizeof r1b, ic->rx_kbs); fmt_rate(r2b, sizeof r2b, ic->tx_kbs);
        snprintf(b, sizeof b, "%s / %s", r1b, r2b);
        text(W - 22 - text_w(1.6f, b), y + 16, 1.6f, UN_DIM, b);
        text(40, y + 44, 1.8f, UN_DIM, ic->ip[0] ? ic->ip : "-");
        int ry = y + 72;
        if (has6){
            snprintf(b, sizeof b, "v6 %s", ic->ip6);
            trunc_fit(b, 1.5f, W - 62);
            text(40, ry, 1.5f, UN_DIM, b);
            ry += 20;
        }
        fmt_bytes(r1b, sizeof r1b, ic->rx_tot); fmt_bytes(r2b, sizeof r2b, ic->tx_tot);
        snprintf(b, sizeof b, "RX %s   TX %s", r1b, r2b);
        text(40, ry, 1.8f, UN_DIM, b);
        y += ch + 12;
    }
    page_end = y;
}

static void page_disks(stats_t *st){
    char b[128];
    int y = body_top;

    if (st->n_disks == 0){
        card(y, 100, "DISKS");
        text(22, y + 42, 2.0f, UN_DIM, "no disk info");
        text(22, y + 66, 1.8f, UN_DIM, "(array stopped?)");
        page_end = y + 110;
        return;
    }
    int show = st->n_disks;
    for (int i = 0; i < show; i++){
        disk_t *d = &st->disks[i];
        card(y, 116, NULL);
        uint32_t dot = d->health == 2 ? UN_BAD : d->health == 1 ? UN_WARN
                     : d->spun ? 0x2a6b38 : UN_OK;
        rect(22, y + 16, 10, 10, dot, 255);
        snprintf(b, sizeof b, "%s", d->name);
        trunc_fit(b, 2.3f, 150);
        text(40, y + 12, 2.3f, UN_TEXT, b);
        if (d->spun)            snprintf(b, sizeof b, "SLEEP");
        else if (d->temp >= 0)  snprintf(b, sizeof b, "%dC", d->temp);
        else                    snprintf(b, sizeof b, "--");
        uint32_t tc = d->spun ? UN_DIM : d->temp > 55 ? UN_BAD : d->temp > 45 ? UN_WARN : UN_DIM;
        text(W - 22 - text_w(2.2f, b), y + 13, 2.2f, tc, b);
        if (d->errors > 0){
            snprintf(b, sizeof b, "%s  %lld errs", d->type, d->errors);
            text(22, y + 42, 1.7f, UN_BAD, b);
        } else {
            text(22, y + 42, 1.7f, UN_DIM, d->type[0] ? d->type : "-");
        }
        if (d->fs_size > 0){
            double pct = 100.0 * (double)d->fs_used / (double)d->fs_size;
            bar(22, y + 68, W - 92, 14, pct, level_col(pct));
            snprintf(b, sizeof b, "%.0f%%", pct);
            text(W - 22 - text_w(1.8f, b), y + 67, 1.8f, UN_DIM, b);
            fmt_bytes(b, sizeof b, d->fs_used * 1024ULL);
            char tot[32]; fmt_bytes(tot, sizeof tot, d->fs_size * 1024ULL);
            char l[64]; snprintf(l, sizeof l, "%s / %s", b, tot);
            text(22, y + 92, 1.6f, UN_DIM, l);
        } else if (d->size_kib > 0){
            fmt_bytes(b, sizeof b, d->size_kib * 1024ULL);
            char l[64]; snprintf(l, sizeof l, "%s  (no fs)", b);
            text(22, y + 74, 1.7f, UN_DIM, l);
        }
        y += 126;
    }
    page_end = y;
}

static void page_docker(stats_t *st){
    char b[128];
    int y = body_top;

    card(y, 76, "DOCKER");
    if (st->docker >= 0){
        snprintf(b, sizeof b, "%d / %d running", st->docker, st->docker_total);
        trunc_fit(b, 2.3f, W - 44);
        text(22, y + 38, 2.3f, UN_TEXT, b);
    } else {
        text(22, y + 38, 2.0f, UN_DIM, "docker n/a");
    }
    y += 88;

    for (int i = 0; i < st->n_ctrs; i++){
        ctr_t *c = &st->ctrs[i];
        card(y, 64, NULL);
        uint32_t dot = !strcmp(c->state, "running") ? UN_OK
                     : (!strcmp(c->state, "restarting") || !strcmp(c->state, "paused"))
                       ? UN_WARN : 0x777777;
        rect(22, y + 14, 10, 10, dot, 255);
        snprintf(b, sizeof b, "%s", c->name);
        trunc_fit(b, 2.1f, W - 66);
        text(40, y + 10, 2.1f, UN_TEXT, b);
        snprintf(b, sizeof b, "%s", c->status[0] ? c->status : c->state);
        trunc_fit(b, 1.6f, W - 66);
        text(40, y + 38, 1.6f, UN_DIM, b);
        y += 74;
    }
    if (st->docker >= 0 && st->n_ctrs == 0){
        card(y, 56, NULL);
        text(22, y + 22, 1.8f, UN_DIM, "no containers");
        y += 66;
    }

    card(y, 56, NULL);
    if (!st->vm_enabled)
        snprintf(b, sizeof b, "VMs: disabled");
    else
        snprintf(b, sizeof b, "VMs: %d running", st->vm_count);
    text(22, y + 22, 2.0f, st->vm_enabled ? UN_TEXT : UN_DIM, b);
    page_end = y + 66;
}

/* one tap-to-cycle settings row: label left, value right, whole card tappable */
static int settings_row(int y, int wid, const char *label, const char *val){
    card(y, 52, NULL);
    text(22, y + 18, 2.0f, UN_TEXT, label);
    text(W - 22 - text_w(2.0f, val), y + 18, 2.0f, UN_ORANGE, val);
    hb_add(wid, 10, y, W - 20, 52);
    return y + 62;
}

static int settings_button(int y, int wid, const char *label, uint32_t fill,
                           uint32_t txt_col){
    rect(10, y, W - 20, 56, fill, 255);
    rect(10, y, W - 20, 1, UN_GREY_70, 255);
    text_bold((W - text_w(2.2f, label)) / 2 - 1, y + 19, 2.2f, txt_col, label);
    hb_add(wid, 10, y, W - 20, 56);
    return y + 68;
}

static void page_settings(stats_t *st){
    (void)st;
    char b[64];
    int y = body_top;
    hb_n = 0;
    long nowms = now_ms();

    /* brightness slider */
    card(y, 84, "BRIGHTNESS");
    snprintf(b, sizeof b, "%d%%", cfg_brightness);
    text(W - 22 - text_w(1.9f, b), y + 10, 1.9f, UN_ORANGE, b);
    rect(SLIDER_X, y + 42, SLIDER_W, 24, 0x2a2a2a, 255);
    int fw = SLIDER_W * cfg_brightness / 100;
    hgrad(SLIDER_X, y + 42, fw, 24, UN_RED, UN_ORANGE);
    rect(SLIDER_X + fw - 2, y + 38, 4, 32, UN_TEXT, 255);
    hb_add(WID_BRIGHT, 10, y + 30, W - 20, 48);
    y += 96;

    /* tap-to-cycle rows */
    if (cfg_screen_off == 0) snprintf(b, sizeof b, "never");
    else                     snprintf(b, sizeof b, "%d min", cfg_screen_off);
    y = settings_row(y, WID_SCROFF, "Screen off", b);

    if (cfg_rotate == 0) snprintf(b, sizeof b, "off");
    else                 snprintf(b, sizeof b, "%ds", cfg_rotate);
    y = settings_row(y, WID_ROTATE, "Auto-rotate", b);

    y = settings_row(y, WID_LEDS,  "LEDs",       cfg_leds  ? "on" : "off");
    y = settings_row(y, WID_NIGHT, "Night mode", cfg_night ? "on" : "off");
    y += 8;

    y = settings_button(y, WID_RESTART, "RESTART DASH", UN_GREY_80, UN_TEXT);

    if (confirm_which == WID_REBOOT && nowms < confirm_until){
        snprintf(b, sizeof b, "TAP TO CONFIRM (%lds)", (confirm_until - nowms) / 1000 + 1);
        y = settings_button(y, WID_REBOOT, b, UN_BAD, UN_TEXT);
    } else {
        y = settings_button(y, WID_REBOOT, "REBOOT (hold)", UN_GREY_80, UN_ORANGE);
    }
    if (confirm_which == WID_SHUTDOWN && nowms < confirm_until){
        snprintf(b, sizeof b, "TAP TO CONFIRM (%lds)", (confirm_until - nowms) / 1000 + 1);
        y = settings_button(y, WID_SHUTDOWN, b, UN_BAD, UN_TEXT);
    } else {
        y = settings_button(y, WID_SHUTDOWN, "SHUTDOWN (hold)", UN_GREY_80, UN_ORANGE);
    }
    y += 4;
    text_c(y, 1.4f, 0x666666, "hold, then tap to confirm");
    page_end = y + 22;
}

/* ---------- settings widget actions ---------- */
static int widget_tap(int x, int y){
    int id = hb_find(x, y);
    switch (id){
    case WID_BRIGHT: {
        int pct = (x - SLIDER_X) * 100 / (SLIDER_W > 0 ? SLIDER_W : 1);
        if (pct < 5) pct = 5; if (pct > 100) pct = 100;
        if (pct != cfg_brightness){
            cfg_brightness = pct;
            apply_brightness();
            settings_save();
        }
        return 1;
    }
    case WID_SCROFF:
        cfg_screen_off = cfg_screen_off == 0 ? 1 : cfg_screen_off == 1 ? 5
                       : cfg_screen_off == 5 ? 15 : 0;
        settings_save();
        return 1;
    case WID_ROTATE:
        cfg_rotate = cfg_rotate == 0 ? 10 : cfg_rotate == 10 ? 20
                   : cfg_rotate == 20 ? 60 : 0;
        rotate_ms = (long)cfg_rotate * 1000L;
        settings_save();
        return 1;
    case WID_LEDS:
        cfg_leds = !cfg_leds;
        apply_leds();
        settings_save();
        return 1;
    case WID_NIGHT:
        cfg_night = !cfg_night;
        apply_brightness();
        settings_save();
        return 1;
    case WID_RESTART:
        do_restart_dash();
        return 1;
    case WID_REBOOT:
        if (confirm_which == WID_REBOOT && now_ms() < confirm_until){
            confirm_which = 0;
            do_reboot();
        }
        return 1;
    case WID_SHUTDOWN:
        if (confirm_which == WID_SHUTDOWN && now_ms() < confirm_until){
            confirm_which = 0;
            do_shutdown();
        }
        return 1;
    default:
        return 0;
    }
}

static int widget_long(int x, int y){
    int id = hb_find(x, y);
    if (id == WID_REBOOT || id == WID_SHUTDOWN){
        confirm_which = id;
        confirm_until = now_ms() + 5000;
        return 1;
    }
    return 0;
}

/* ---------- render ---------- */
static void render(stats_t *st){
    for (int y = 0; y < H; y++)
        memcpy(fbmem + y * fbpitch, bg + y * W, (size_t)W * 4);
    int base = BODY_Y0 + (st->notif_count > 0 ? 44 : 0);
    int visible = FOOTER_Y - 12 - base;
    int maxs = content_h[cur_page] - visible;
    if (maxs < 0) maxs = 0;
    if (scrolly[cur_page] > maxs) scrolly[cur_page] = maxs;
    if (scrolly[cur_page] < 0)    scrolly[cur_page] = 0;
    body_top = base - scrolly[cur_page];
    page_end = body_top;
    pages[cur_page].body(st);
    content_h[cur_page] = page_end - body_top;
    draw_header(st);
    if (st->notif_count > 0) draw_banner(st);
    draw_footer();
}

/* ============================================================
 * TOUCH — AXS15231B native I2C protocol (userspace polling).
 * Unraid's kernel lacks GPIOLIB_IRQCHIP so the vendor/HID paths
 * can't bind; instead we poll the chip directly at 0x3b with the
 * AXS command-prefixed read (magic b5 ab a5 5a). Auto-discovers
 * the DesignWare adapter; daemon runs fine without touch.
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
            int idle_fill = 1;                           /* 0x10-fill = release/idle */
            for (int i = 0; i < AXS_FRAME_LEN; i++) if (b[i] != 0x10){ idle_fill = 0; break; }
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

/* ---------- main ---------- */
int main(int argc, char **argv){
    g_argv = argv;
    const char *bgpath = NULL; int once = 0;
    const char *touch_hint = NULL; int no_touch = 0;

    settings_load();                            /* flash cfg first, CLI overrides */
    for (int i = 1; i < argc; i++){
        if (!strcmp(argv[i], "--bg") && i + 1 < argc) bgpath = argv[++i];
        else if (!strcmp(argv[i], "--interval") && i + 1 < argc) cfg_interval = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--backlight") && i + 1 < argc) cfg_brightness = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--once")) once = 1;
        else if (!strcmp(argv[i], "--touch") && i + 1 < argc) touch_hint = argv[++i];
        else if (!strcmp(argv[i], "--no-touch")) no_touch = 1;
        else if (!strcmp(argv[i], "--rotate") && i + 1 < argc) cfg_rotate = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--cal") && i + 1 < argc){
            const char *c = argv[++i];
            cal_swap_xy = strchr(c, 's') != NULL;
            cal_inv_x   = strchr(c, 'x') != NULL;
            cal_inv_y   = strchr(c, 'y') != NULL;
        }
    }
    if (cfg_interval < 1) cfg_interval = 1;
    if (cfg_brightness < 5) cfg_brightness = 5;
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
            read_fans(&st); read_zones(&st); read_docker(&st); read_power(&st);
            read_about(&st);
            read_notif(&st);
            hist_push(st.cpu, st.gpu_avail ? st.gpu_busy : 0);
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
                cur_page = (cur_page + 1) % NPAGES; scrolly[cur_page] = 0;
                confirm_which = 0; dirty = 1; break;
            case TOUCH_SWIPE_RIGHT:
                cur_page = (cur_page + NPAGES - 1) % NPAGES; scrolly[cur_page] = 0;
                confirm_which = 0; dirty = 1; break;
            case TOUCH_DRAG:
                scrolly[cur_page] -= ev.y; dirty = 1; break;   /* 1:1 finger scroll */
            case TOUCH_TAP:
                if (cur_page == PAGE_SETTINGS && widget_tap(ev.x, ev.y)){
                    dirty = 1;
                } else if (ev.y >= FOOTER_Y){
                    cur_page = (cur_page + 1) % NPAGES; confirm_which = 0; dirty = 1;
                } else {
                    dim = 0; apply_brightness();                   /* wake */
                }
                break;
            case TOUCH_LONG_PRESS:
                if (cur_page == PAGE_SETTINGS && widget_long(ev.x, ev.y)){
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
            if (cur_page == PAGE_SETTINGS) dirty = 1;
        }
        if (!screen_off && cfg_screen_off > 0 &&
            nowms - last_touch >= (long)cfg_screen_off * 60000L){
            screen_off = 1;
            ec_write(0xA5, 198);                               /* EC backlight off */
            set_bl_power(0);                                   /* panel off */
        }
        if (rotate_ms > 0 && !screen_off &&
            nowms - last_touch >= rotate_ms && nowms - last_rotate >= rotate_ms){
            cur_page = (cur_page + 1) % NPAGES;
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
