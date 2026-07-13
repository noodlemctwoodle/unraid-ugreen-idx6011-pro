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

/* small top-right tag (e.g. "16T", "1500 MHz") */
static void card_tag(int y, const char *s, uint32_t col){
    text(C_R - text_w(C_TAG, s), y + 12, C_TAG, col, s);
}
/* right-aligned secondary value at a standard slot (0 = upper, 1 = lower) */
static void card_sub(int y, int slot, const char *s, uint32_t col){
    text(C_R - text_w(C_SUB, s), y + (slot ? 80 : 50), C_SUB, col, s);
}

/* STANDARD METRIC CARD: title + big value with a bar (ring=0) or ring (ring=1).
 * Returns the advance (card height + gap). Add temp/power etc. via card_sub(). */
static int metric_card(int y, const char *title, double pct, const char *value, int ring){
    card(y, CH_METRIC, title);
    if (ring){
        int cx = 60, cy = y + 82, ro = 40, ri = 27;
        ring_gauge(cx, cy, ro, ri, pct, level_col(pct));
        text(cx - text_w(2.2f, value) / 2, cy - 9, 2.2f, UN_TEXT, value);
    } else {
        text(C_X0, y + 44, C_VAL, UN_TEXT, value);
        bar(C_X0, y + 108, C_W, 18, pct, level_col(pct));
    }
    return CH_METRIC + C_GAP;
}

/* METRIC CARD WITH SPARKLINE: title + big value + a 62px history graph. */
static int spark_card(int y, const char *title, const char *value,
                      const float *hist, int hcnt, int hpos, uint32_t col){
    card(y, CH_SPARK, title);
    text(C_X0, y + 44, 3.6f, UN_TEXT, value);
    spark(C_X0, y + 84, C_W, 62, hist, hcnt, hpos, col);
    return CH_SPARK + C_GAP;
}

/* SMALL VALUE CARD: title + one value line, content height `h`. */
static int value_card(int y, int h, const char *title, const char *value, uint32_t col){
    card(y, h, title);
    text(C_X0, y + (h >= 96 ? 44 : 30), C_BODY, col, value);
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
