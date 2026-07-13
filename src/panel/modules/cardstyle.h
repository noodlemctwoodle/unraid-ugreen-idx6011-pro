/*
 * plugin/src/panel/modules/cardstyle.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * THE CARD STYLE SHIM — one place that defines how every dashboard card looks,
 * so all modules render with an identical visual language (padding, title, value
 * scale, bar/ring geometry, secondary-value slots). A module should NEVER lay a
 * card out by hand: it composes these helpers. Change the look here and it
 * applies globally. See docs/dashboard-modules.md for the contributor guide.
 *
 * Vertical geometry (y-offsets, card heights, gaps, ring/gauge radii) is wrapped
 * in gy() (draw.h), which scales by the Theme text/heading size — so bigger text
 * makes cards TALLER instead of overlapping. gy() is the identity at 100%.
 *
 * Part of panel_dash: #included by panel_dash.c after ui.h/pageframe.h (for
 * card/bar/ring/spark/text) and before the mod_*.h files.
 */
#ifndef PANEL_CARDSTYLE_H
#define PANEL_CARDSTYLE_H

/* ---- content geometry (every card lines up to these) ---- */
#define C_X0    22                 /* content left inset                     */
#define C_R     (W - 22)           /* content right edge (right-aligned text) */
#define C_W     (W - 44)           /* full content width (bars)              */
#define C_GAP   12                 /* vertical gap after every card (scaled)  */

/* ---- shared text scales (so a value is the same size on every page) ---- */
#define C_VAL   4.4f               /* primary value, e.g. "42%"              */
#define C_SUB   2.0f               /* secondary value (temp / power)         */
#define C_TAG   1.8f               /* small top-right tag (threads / freq)   */
#define C_BODY  2.4f               /* body line (used/total, rates)          */

/* ---- shared metric card heights (base; scaled by gy at use) ---- */
#define CH_METRIC 142              /* metric card (value + bar/ring)         */
#define CH_SPARK  152              /* metric card with a sparkline           */

/* ---- ring geometry (metric_card ring variant) ---- */
#define RING_CX 60                 /* ring centre x (base)                   */
#define RING_RO 40                 /* ring outer radius (base)               */
#define RING_DSC 1.6f              /* detail scale (fits the right-of-ring zone) */
/* ring centre/radius grow with the layout scale but stay anchored on-panel */
static int ring_ro(void){ return gy(RING_RO); }
static int ring_cx(void){ return RING_CX + (gy(RING_RO) - RING_RO); }

/* ---- semantic colours ----------------------------------------------------
 * Map a module's STATE to a palette colour in ONE place, so every card colours
 * the same way. A module must NEVER hardcode a colour decision (e.g. ternaries
 * on a threshold) — call one of these, or add a new one here. The palette
 * itself (UN_TEXT/DIM/OK/WARN/BAD/ORANGE/…) lives in draw.h and is themeable at
 * runtime from Layout > Theme. */
static uint32_t col_load(double pct){ return level_col(pct); }          /* usage / load %     */
static uint32_t col_temp(int c){                                        /* CPU / board temp   */
    return c <= 0 ? UN_DIM : c > 85 ? UN_BAD : c > 70 ? UN_WARN : UN_OK; }
static uint32_t col_disktemp(int c){                                    /* disk temp          */
    return c < 0 ? UN_DIM : c > 55 ? UN_BAD : c > 45 ? UN_WARN : UN_DIM; }
static uint32_t col_state(int up){ return up ? UN_OK : UN_DIM; }        /* link/service text  */
static uint32_t col_dot(int up){ return up ? UN_OK : 0x555555; }        /* presence dot       */
static uint32_t col_health(int h){                                      /* 0=ok 1=warn 2=bad  */
    return h == 2 ? UN_BAD : h == 1 ? UN_WARN : UN_OK; }

/* small top-right tag (e.g. "16T", "1500 MHz") — truncates to the zone right of
 * the title, so it can never run back over the title. */
static void card_tag(int y, const char *s, uint32_t col){
    char v[32]; snprintf(v, sizeof v, "%s", s);
    trunc_fit(v, C_TAG, C_R - (C_X0 + 96));
    text(C_R - text_w(C_TAG, v), y + gy(12), C_TAG, col, v);
}
/* right-aligned SHORT secondary value at a standard slot (0 = upper, 1 = lower),
 * e.g. temp / power. Truncates to the right zone (leaves room for the value on
 * the left). For a long used/total line use metric_detail() instead. */
static void card_sub(int y, int slot, const char *s, uint32_t col){
    char v[32]; snprintf(v, sizeof v, "%s", s);
    trunc_fit(v, C_SUB, C_R - (C_X0 + 70));
    text(C_R - text_w(C_SUB, v), y + gy(slot ? 80 : 50), C_SUB, col, v);
}

/* 270-degree arc gauge (speedometer): filled clockwise from the lower-left, with a
 * 90-degree gap at the bottom. Same annulus approach as ring_gauge. */
static void arc_gauge(int cx, int cy, int ro, int ri, double pct, uint32_t col){
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    float lim = (float)(pct / 100.0), gaphalf = 0.125f, sweep = 1.0f - 2.0f * gaphalf;
    int ro2 = ro * ro, ri2 = ri * ri;
    for (int dy = -ro; dy <= ro; dy++)
        for (int dx = -ro; dx <= ro; dx++){
            int d2 = dx * dx + dy * dy;
            if (d2 > ro2 || d2 < ri2) continue;
            float a = atan2f((float)dx, (float)dy);      /* 0 at bottom, clockwise + */
            if (a < 0) a += 2.0f * (float)M_PI;
            float t = a / (2.0f * (float)M_PI);          /* 0..1 from bottom, cw */
            if (t < gaphalf || t > 1.0f - gaphalf) continue;   /* the bottom gap */
            float frac = (t - gaphalf) / sweep;
            px(cx + dx, cy + dy, frac <= lim ? col : 0x2a2a2a);
        }
}

/* is a card at screen-y `y` of gy-scaled height `h` within the visible body clip
 * band? Animated cards gate their redraw request on this so they don't burn frames
 * computing fully-clipped, invisible pixels while scrolled out of view. */
static int card_visible(int y, int h){
    return y < g_clip_bot && y + h > g_clip_top;
}

/* SPINNING FAN GLYPH. N swept blades + a hub, rotated to `ang` degrees, anti-
 * aliased, with an orange->red radial gradient (Unraid signature). spinning=0
 * draws it static and dimmed (a stopped/failed fan). Pure pixel-annulus like
 * arc_gauge/ring_gauge; honours the body clip via px_blend. */
static void draw_fan(int cx, int cy, int r, double ang, int spinning){
    if (r < 7) r = 7;
    const int    N   = 7;                         /* blades */
    const double PI2 = 6.2831853071795864;
    double arad = ang * (PI2 / 360.0);
    double hub  = r * 0.27;                        /* hub radius */
    double curl = 1.15;                            /* radians a blade sweeps across the disc */
    double fill = 0.60;                            /* fraction of each slot that is blade */
    uint32_t root = spinning ? UN_ORANGE : 0x606060;   /* blade colour at the hub */
    uint32_t tip  = spinning ? UN_RED    : 0x484848;   /* blade colour at the rim */
    for (int dy = -r - 1; dy <= r + 1; dy++){
        int y = cy + dy;
        for (int dx = -r - 1; dx <= r + 1; dx++){
            double rr = sqrt((double)dx * dx + (double)dy * dy);
            if (rr > r + 0.7) continue;
            int x = cx + dx;
            if (rr <= hub + 1.0){                          /* hub disc (AA rim) + bright centre */
                double c = hub + 0.5 - rr; if (c > 1) c = 1; if (c < 0) continue;
                px_blend(x, y, 0x2b2b2b, (uint8_t)(c * 255));
                if (rr <= hub * 0.45) px_blend(x, y, root, 210);
                continue;
            }
            double a    = atan2((double)dy, (double)dx);
            double as   = a - arad - curl * (rr / r);      /* rotate + curl */
            double slot = as * N / PI2;
            double frac = slot - floor(slot);              /* 0..1 within a blade slot */
            if (frac >= fill) continue;
            double cov = 1.0, e = 0.09;                     /* soft blade edges */
            if      (frac < e)        cov = frac / e;
            else if (frac > fill - e) cov = (fill - frac) / e;
            double ro = r - rr;   if (ro < 1.2) cov *= ro < 0 ? 0 : ro / 1.2;   /* soft rim */
            double ri = rr - hub; if (ri < 2.0) cov *= ri < 0 ? 0 : ri / 2.0;   /* soft root */
            if (cov <= 0) continue; if (cov > 1) cov = 1;
            float t = (float)((rr - hub) / (r - hub));      /* 0 root .. 1 tip */
            px_blend(x, y, lerp_rgb(root, tip, t), (uint8_t)(cov * 255));
        }
    }
}

/* STANDARD METRIC CARD. style: 0=bar, 1=ring, 2=big (large centred value), 3=gauge.
 * Returns the advance (card height + gap). Add temp/power via card_sub() (bar/ring
 * only) and a used/total line via metric_detail(). */
static int metric_card(int y, const char *title, double pct, const char *value, int style){
    card(y, gy(CH_METRIC), title);
    if (style == 1){                                     /* ring */
        int cx = ring_cx(), cy = y + gy(82);
        ring_gauge(cx, cy, ring_ro(), gy(27), pct, col_load(pct));
        text(cx - text_w(2.2f, value) / 2, cy - gy(9), 2.2f, UN_TEXT, value);
    } else if (style == 2){                              /* big — large centred value */
        text_c(y + gy(54), 6.4f, UN_TEXT, value);
    } else if (style == 3){                              /* gauge — 270° arc */
        int cy = y + gy(92);
        arc_gauge(W / 2, cy, gy(46), gy(32), pct, col_load(pct));
        text(W / 2 - text_w(2.6f, value) / 2, cy - gy(12), 2.6f, UN_TEXT, value);
    } else {                                             /* bar (default) */
        text(C_X0, y + gy(44), C_VAL, UN_TEXT, value);
        bar(C_X0, y + gy(108), C_W, gy(18), pct, col_load(pct));
    }
    return gy(CH_METRIC) + gy(C_GAP);
}

/* the used/total (or similar) detail line of a metric card, placed so it can
 * NEVER collide with the value/ring, whatever the string length:
 *   ring mode -> the zone to the RIGHT of the ring (left-aligned, small scale)
 *   bar  mode -> its OWN ROW below the big value (full width, left-aligned)
 * Always truncated to that zone. Use this for long metric details; use card_sub
 * for short right-aligned values (temp/power). */
static void metric_detail(int y, const char *s, uint32_t col, int ring){
    char v[64]; snprintf(v, sizeof v, "%s", s);
    if (ring){ int dx = ring_cx() + ring_ro() + gy(8);
               trunc_fit(v, RING_DSC, C_R - dx); text(dx, y + gy(74), RING_DSC, col, v); }
    else     { trunc_fit(v, C_SUB, C_W);         text(C_X0, y + gy(82), C_SUB, col, v); }
}

/* METRIC CARD WITH SPARKLINE: title + big value + a history graph. */
static int spark_card(int y, const char *title, const char *value,
                      const float *hist, int hcnt, int hpos, uint32_t col){
    card(y, gy(CH_SPARK), title);
    text(C_X0, y + gy(44), 3.6f, UN_TEXT, value);
    spark(C_X0, y + gy(84), C_W, gy(62), hist, hcnt, hpos, col);
    return gy(CH_SPARK) + gy(C_GAP);
}

/* GRAPH CARD: title + value + history sparkline (line or filled area). maxv<=0
 * auto-scales the y-axis (for unbounded series such as network rate). */
static int graph_card(int y, const char *title, const char *value,
                      const float *hist, uint32_t col, int fill, float maxv){
    card(y, gy(CH_SPARK), title);
    text(C_X0, y + gy(44), 3.6f, UN_TEXT, value);
    spark_ex(C_X0, y + gy(84), C_W, gy(62), hist, h_cnt, h_pos, col, fill, maxv);
    return gy(CH_SPARK) + gy(C_GAP);
}

/* segmented / blocks bar — the percentage as 10 discrete lit blocks */
static void seg_bar(int x, int y, int w, int h, double pct, uint32_t col){
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    int nseg = 10, gap = 3, sw = (w - (nseg - 1) * gap) / nseg;
    int lit = (int)(pct / 100.0 * nseg + 0.5);
    if (lit == 0 && pct > 0) lit = 1;                            /* any load lights >= 1 block */
    for (int i = 0; i < nseg; i++)
        rect(x + i * (sw + gap), y, sw, h, i < lit ? col : 0x2a2a2a, 255);
}
/* BLOCKS METRIC CARD: title + big value + a segmented bar. */
static int blocks_card(int y, const char *title, double pct, const char *value){
    card(y, gy(CH_METRIC), title);
    text(C_X0, y + gy(44), C_VAL, UN_TEXT, value);
    seg_bar(C_X0, y + gy(108), C_W, gy(18), pct, col_load(pct));
    return gy(CH_METRIC) + gy(C_GAP);
}

/* SPLIT USED/FREE BAR CARD: a two-tone bar with both values labelled (below the
 * title so they never collide with it). */
static int split_card(int y, const char *title, double usedpct,
                      const char *used, const char *freev){
    int h = 122;
    if (usedpct < 0) usedpct = 0; if (usedpct > 100) usedpct = 100;
    card(y, gy(h), title);
    text(C_X0, y + gy(44), 2.0f, UN_TEXT, used);                 /* used (left)  */
    text(C_R - text_w(2.0f, freev), y + gy(44), 2.0f, UN_DIM, freev);  /* free (right) */
    int uw = (int)(C_W * usedpct / 100.0), by = y + gy(74);
    rect(C_X0, by, uw, gy(22), col_load(usedpct), 255);          /* used bar */
    rect(C_X0 + uw, by, C_W - uw, gy(22), 0x3a3a3a, 255);        /* free bar */
    char p[16]; snprintf(p, sizeof p, "%.0f%%", usedpct);
    text(C_R - text_w(1.6f, p), y + gy(100), 1.6f, UN_DIM, p);
    return gy(h) + gy(C_GAP);
}

/* TREND CARD: big current value (left) + a mini area spark + delta (right). */
static int trend_card(int y, const char *title, const char *value,
                      const float *hist, uint32_t col){
    int h = 120;
    card(y, gy(h), title);
    text(C_X0, y + gy(44), C_VAL, UN_TEXT, value);
    int sw = 100;
    spark_ex(C_R - sw, y + gy(42), sw, gy(42), hist, h_cnt, h_pos, col, 1, 100);
    if (h_cnt >= 2){
        float a = hist[(h_pos - h_cnt + 2 * SPARK_N) % SPARK_N];
        float b = hist[(h_pos - 1 + SPARK_N) % SPARK_N];
        char d[16]; snprintf(d, sizeof d, "%+.0f", b - a);
        text(C_R - text_w(1.6f, d), y + gy(92), 1.6f, b >= a ? UN_OK : UN_DIM, d);
    }
    return gy(h) + gy(C_GAP);
}

/* SMALL VALUE CARD: title + one value line (truncated to fit), base height `h`. */
static int value_card(int y, int h, const char *title, const char *value, uint32_t col){
    card(y, gy(h), title);
    char v[128]; snprintf(v, sizeof v, "%s", value);
    trunc_fit(v, C_BODY, C_W);
    text(C_X0, y + gy(h >= 96 ? 44 : 30), C_BODY, col, v);
    return gy(h) + gy(C_GAP);
}

/* BIG VALUE CARD: title + one large centred value (no bar/ring). For the "big"
 * variant of informational modules (power / uptime / …). */
static int big_value_card(int y, const char *title, const char *value, uint32_t col){
    int h = 120;
    card(y, gy(h), title);
    char v[64]; snprintf(v, sizeof v, "%s", value); trunc_fit(v, 4.2f, C_W);
    text_c(y + gy(58), 4.2f, col, v);
    return gy(h) + gy(C_GAP);
}

/* BAR CARD: title + right value + a bar, base content height `h`. */
static int bar_card(int y, int h, const char *title, const char *value,
                    double pct, uint32_t barcol){
    card(y, gy(h), title);
    text(C_R - text_w(C_SUB, value), y + gy(10), C_SUB, UN_DIM, value);
    bar(C_X0, y + gy(h - 34), C_W, gy(16), pct, barcol);
    return gy(h) + gy(C_GAP);
}

/* LIST-ITEM HEADER: status dot + name (left) + right-aligned status/temp.
 * Used by per-disk / per-interface cards which then add their own detail rows. */
static void item_head(int y, uint32_t dot, const char *name, float name_sc,
                      const char *right, float right_sc, uint32_t right_col){
    rect(C_X0, y + gy(14), gy(11), gy(11), dot, 255);
    char nm[64]; snprintf(nm, sizeof nm, "%s", name);
    while (nm[0] && htext_w(name_sc, nm) > 150) nm[strlen(nm) - 1] = 0;  /* fit (heading scale) */
    htext(C_X0 + gy(18), y + gy(12), name_sc, UN_TEXT, nm);             /* item name = heading */
    if (right) text(C_R - text_w(right_sc, right), y + gy(13), right_sc, right_col, right);
}

#endif /* PANEL_CARDSTYLE_H */
