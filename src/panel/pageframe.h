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

/* ---------- page system (dynamic: user-defined content pages + built-in SETTINGS) ----------
 * MAX_CPAGES / cpage_t / g_cpage / g_ncpage are defined in prefs.h (loaded first).
 * SETTINGS is the always-present last page. */
#define MAX_PAGES  (MAX_CPAGES + 1)     /* + the always-present SETTINGS page      */
#define HEADER_H  84
#define BODY_Y0   124
#define FOOTER_Y  918
#define SCROLL_STEP 300
static long rotate_ms = 0;              /* auto-rotate off by default */
static int scrolly[MAX_PAGES];
static int page_end;                    /* set by each page: content bottom y */
static int content_h[MAX_PAGES];

static int cur_page;
static int body_top = BODY_Y0;

/* the page title + position sit in a card below the clock; its height (and so the
 * y at which page content begins) grows with the heading/text size. */
#define TITLECARD_H   28
#define TITLECARD_TOP 12                         /* gap below the gradient rule (so they don't merge) */
static int hdr_bottom = BODY_Y0;                 /* y where content begins (below title card) */
static int header_h(void){ return HEADER_H + gy(TITLECARD_TOP) + gy(TITLECARD_H) + gy(8); }

static void page_settings(stats_t *st);          /* the one built-in interactive page */

/* SETTINGS is the last navigable page, at index g_ncpage. */
static int n_total(void){ return g_ncpage + 1; }
static int is_settings_page(int i){ return i == g_ncpage; }
static const char *page_title(int i){
    if (is_settings_page(i)) return "SETTINGS";
    return (i >= 0 && i < g_ncpage) ? g_cpage[i].name : "";
}

/* ---------- page enable/skip (SETTINGS is always on) ---------- */
static int page_on(int i){
    if (is_settings_page(i)) return 1;
    return (i >= 0 && i < g_ncpage) ? g_cpage[i].on : 0;
}
static int next_page(int cur, int dir){          /* next ENABLED page in a direction */
    int n = n_total();
    for (int k = 0; k < n; k++){
        cur = (cur + dir + n) % n;
        if (page_on(cur)) return cur;
    }
    return cur;                                  /* all off can't happen (SETTINGS on) */
}
static int n_pages_on(void){ int n = 0, t = n_total(); for (int i = 0; i < t; i++) if (page_on(i)) n++; return n; }
static int page_pos(int cur){ int p = 0, t = n_total(); for (int i = 0; i <= cur && i < t; i++) if (page_on(i)) p++; return p; }

static void draw_header(stats_t *st){
    rect(0, 0, W, hdr_bottom - 6, UN_BLACK, 255);   /* opaque incl. title-card zone */
    if (!draw_custom_logo(12, 25))                        /* custom logo.png if uploaded... */
        draw_unraid_icon(12, 25);                         /* ...else the official 60x34 mark */
    if (st->notif_count > 0){
        char nb[16];
        snprintf(nb, sizeof nb, "%d", st->notif_count > 99 ? 99 : st->notif_count);
        int tw = text_w_raw(1.5f, nb);
        uint32_t bc = st->notif_imp == 2 ? UN_BAD : st->notif_imp == 1 ? UN_WARN : UN_ORANGE;
        rect(76, 12, tw + 12, 17, bc, 255);
        text_raw(82, 16, 1.5f, 0x1b1b1b, nb);
    }
    /* header chrome (clock/date/position) stays a fixed size — not user-scaled */
    char b[64]; time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm);
    strftime(b, sizeof b, "%H:%M", &tm);
    text_raw(W - 16 - text_w_raw(3.2f, b), 18, 3.2f, UN_TEXT, b);
    strftime(b, sizeof b, "%a %d %b", &tm);
    text_raw(W - 16 - text_w_raw(1.5f, b), 52, 1.5f, UN_DIM, b);
    hgrad(0, HEADER_H, W, 3, UN_RED, UN_ORANGE);          /* signature rule */
    /* page heading + position, in a card that grows with the heading size */
    int tcY = HEADER_H + gy(TITLECARD_TOP), tcH = gy(TITLECARD_H);
    card(tcY, tcH, NULL);
    htext_c(tcY + gy(6), 2.2f, UN_TEXT, page_title(cur_page));
    snprintf(b, sizeof b, "%d/%d", page_pos(cur_page), n_pages_on());
    text_raw(W - 22 - text_w_raw(1.4f, b), tcY + gy(9), 1.4f, UN_DIM, b);
}

static void draw_banner(stats_t *st){
    uint32_t c = st->notif_imp == 2 ? UN_BAD : st->notif_imp == 1 ? UN_WARN : UN_ORANGE_M;
    rect(10, hdr_bottom, W - 20, gy(36), UN_GREY_80, 245);
    rect(10, hdr_bottom, 3, gy(36), c, 255);
    char b[32]; snprintf(b, sizeof b, "%d UNREAD", st->notif_count);
    text(22, hdr_bottom + gy(5), 1.3f, c, b);
    char subj[128];
    snprintf(subj, sizeof subj, "%s", st->notif_subj[0] ? st->notif_subj : "notification");
    trunc_fit(subj, 1.5f, W - 44);
    text(22, hdr_bottom + gy(19), 1.5f, UN_TEXT, subj);
}

static void draw_footer(void){
    rect(0, FOOTER_Y - 4, W, H - (FOOTER_Y - 4), UN_BLACK, 255);
    hline(16, FOOTER_Y, W - 32, UN_GREY_70);
    int total = n_pages_on();
    int pitch = 24, x0 = (W - (total - 1) * pitch) / 2, j = 0;
    for (int i = 0; i < n_total(); i++){
        if (!page_on(i)) continue;
        int cx = x0 + j * pitch; j++;
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
    WID_NIGHT, WID_FAN, WID_RESTART, WID_REBOOT, WID_SHUTDOWN
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
