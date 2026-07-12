/*
 * plugin/src/panel/pageframe.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * page table, header / banner / footer, settings hit-testing
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGEFRAME_H
#define PANEL_PAGEFRAME_H

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
    draw_unraid_icon(12, 25);                             /* official 60x34 mark */
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


#endif /* PANEL_PAGEFRAME_H */
