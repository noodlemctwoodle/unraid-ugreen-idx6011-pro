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
 * Part of panel_dash: #included by panel_dash.c after ui.h/pageframe.h (for
 * card/bar/ring/spark/text) and before the mod_*.h files.
 */
#ifndef PANEL_CARDSTYLE_H
#define PANEL_CARDSTYLE_H

/* ---- content geometry (every card lines up to these) ---- */
#define C_X0    22                 /* content left inset                     */
#define C_R     (W - 22)           /* content right edge (right-aligned text) */
#define C_W     (W - 44)           /* full content width (bars)              */
#define C_GAP   12                 /* vertical gap after every card          */

/* ---- shared text scales (so a value is the same size on every page) ---- */
#define C_VAL   4.4f               /* primary value, e.g. "42%"              */
#define C_SUB   2.0f               /* secondary value (temp / power)         */
#define C_TAG   1.8f               /* small top-right tag (threads / freq)   */
#define C_BODY  2.4f               /* body line (used/total, rates)          */

/* ---- shared metric card heights ---- */
#define CH_METRIC 142              /* metric card (value + bar/ring)         */
#define CH_SPARK  152              /* metric card with a sparkline           */

/* ---- ring geometry (metric_card ring variant) ---- */
#define RING_CX 60                 /* ring centre x                          */
#define RING_RO 40                 /* ring outer radius                      */
#define RING_DX (RING_CX + RING_RO + 8)  /* detail-text left edge (right of the ring) */
#define RING_DSC 1.6f                     /* detail scale (fits the right-of-ring zone) */

/* ---- semantic colours ----------------------------------------------------
 * Map a module's STATE to a palette colour in ONE place, so every card colours
 * the same way. A module must NEVER hardcode a colour decision (e.g. ternaries
 * on a threshold) — call one of these, or add a new one here. The palette
 * itself (UN_TEXT/DIM/OK/WARN/BAD/ORANGE/…) lives in draw.h and is themeable at
 * runtime from Settings > Screen (the theme colours). */
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
    text(C_R - text_w(C_TAG, v), y + 12, C_TAG, col, v);
}
/* right-aligned SHORT secondary value at a standard slot (0 = upper, 1 = lower),
 * e.g. temp / power. Truncates to the right zone (leaves room for the value on
 * the left). For a long used/total line use metric_detail() instead. */
static void card_sub(int y, int slot, const char *s, uint32_t col){
    char v[32]; snprintf(v, sizeof v, "%s", s);
    trunc_fit(v, C_SUB, C_R - (C_X0 + 70));
    text(C_R - text_w(C_SUB, v), y + (slot ? 80 : 50), C_SUB, col, v);
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

/* STANDARD METRIC CARD. style: 0=bar, 1=ring, 2=big (large centred value), 3=gauge.
 * Returns the advance (card height + gap). Add temp/power via card_sub() (bar/ring
 * only) and a used/total line via metric_detail(). */
static int metric_card(int y, const char *title, double pct, const char *value, int style){
    card(y, CH_METRIC, title);
    if (style == 1){                                     /* ring */
        int cy = y + 82;
        ring_gauge(RING_CX, cy, RING_RO, 27, pct, col_load(pct));
        text(RING_CX - text_w(2.2f, value) / 2, cy - 9, 2.2f, UN_TEXT, value);
    } else if (style == 2){                              /* big — large centred value */
        text_c(y + 54, 6.4f, UN_TEXT, value);
    } else if (style == 3){                              /* gauge — 270° arc */
        int cy = y + 92;
        arc_gauge(W / 2, cy, 46, 32, pct, col_load(pct));
        text(W / 2 - text_w(2.6f, value) / 2, cy - 12, 2.6f, UN_TEXT, value);
    } else {                                             /* bar (default) */
        text(C_X0, y + 44, C_VAL, UN_TEXT, value);
        bar(C_X0, y + 108, C_W, 18, pct, col_load(pct));
    }
    return CH_METRIC + C_GAP;
}

/* the used/total (or similar) detail line of a metric card, placed so it can
 * NEVER collide with the value/ring, whatever the string length:
 *   ring mode -> the zone to the RIGHT of the ring (left-aligned, small scale)
 *   bar  mode -> its OWN ROW below the big value (full width, left-aligned)
 * Always truncated to that zone. Use this for long metric details; use card_sub
 * for short right-aligned values (temp/power). */
static void metric_detail(int y, const char *s, uint32_t col, int ring){
    char v[64]; snprintf(v, sizeof v, "%s", s);
    if (ring){ trunc_fit(v, RING_DSC, C_R - RING_DX); text(RING_DX, y + 74, RING_DSC, col, v); }
    else     { trunc_fit(v, C_SUB,    C_W);           text(C_X0,    y + 82, C_SUB,    col, v); }
}

/* METRIC CARD WITH SPARKLINE: title + big value + a 62px history graph. */
static int spark_card(int y, const char *title, const char *value,
                      const float *hist, int hcnt, int hpos, uint32_t col){
    card(y, CH_SPARK, title);
    text(C_X0, y + 44, 3.6f, UN_TEXT, value);
    spark(C_X0, y + 84, C_W, 62, hist, hcnt, hpos, col);
    return CH_SPARK + C_GAP;
}

/* SMALL VALUE CARD: title + one value line (truncated to fit), height `h`. */
static int value_card(int y, int h, const char *title, const char *value, uint32_t col){
    card(y, h, title);
    char v[128]; snprintf(v, sizeof v, "%s", value);
    trunc_fit(v, C_BODY, C_W);
    text(C_X0, y + (h >= 96 ? 44 : 30), C_BODY, col, v);
    return h + C_GAP;
}

/* BAR CARD: title + right value + a bar, content height `h`. */
static int bar_card(int y, int h, const char *title, const char *value,
                    double pct, uint32_t barcol){
    card(y, h, title);
    text(C_R - text_w(C_SUB, value), y + 10, C_SUB, UN_DIM, value);
    bar(C_X0, y + h - 34, C_W, 16, pct, barcol);
    return h + C_GAP;
}

/* LIST-ITEM HEADER: status dot + name (left) + right-aligned status/temp.
 * Used by per-disk / per-interface cards which then add their own detail rows. */
static void item_head(int y, uint32_t dot, const char *name, float name_sc,
                      const char *right, float right_sc, uint32_t right_col){
    rect(C_X0, y + 14, 11, 11, dot, 255);
    char nm[64]; snprintf(nm, sizeof nm, "%s", name);
    trunc_fit(nm, name_sc, 150);
    text(C_X0 + 18, y + 12, name_sc, UN_TEXT, nm);
    if (right) text(C_R - text_w(right_sc, right), y + 13, right_sc, right_col, right);
}

#endif /* PANEL_CARDSTYLE_H */
