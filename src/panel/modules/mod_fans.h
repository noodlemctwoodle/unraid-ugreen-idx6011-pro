/*
 * plugin/src/panel/modules/mod_fans.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "fans" module — chassis fan speeds (RPM) from the ITE EC tachometers
 * (read_fan_rpm, fan.h). Read-only display; fan *control* lives in fan.h and is
 * driven by the Fan mode. Four display variants share one animated fan glyph
 * (draw_fan, cardstyle.h) whose blades spin at a speed mapped from live RPM:
 *   dials   — a grid of spinning fans, one per fan, with role + rpm (default)
 *   list    — a row per fan: mini spinning icon + role + rpm
 *   hero    — one large spinning fan + the aggregate rpm
 *   compact — a single text line (no animation), for tight layouts
 * Any spinning variant re-arms g_anim so the main loop redraws at ~15 fps.
 * Part of panel_dash: #included in a fixed order.
 */
#ifndef PANEL_MOD_FANS_H
#define PANEL_MOD_FANS_H

static const char *fanmode_tag[] = { "AUTO", "SILENT", "QUIET", "TURBO", "MAX" };

/* fan role labels: the tach order is cpufan1, cpufan2, sysfan1, sysfan2 (fan.h) */
static const char *fan_name(int n, int i){
    static const char *four[] = { "CPU 1", "CPU 2", "CASE 1", "CASE 2" };
    static const char *two[]  = { "SYS 1", "SYS 2" };
    static char g[8];
    if (n == 4 && i >= 0 && i < 4) return four[i];
    if (n == 2 && i >= 0 && i < 2) return two[i];
    snprintf(g, sizeof g, "FAN %d", i + 1);
    return g;
}

/* per-glyph spin angle, advanced by real elapsed time so the spin is smooth and
 * frame-rate independent. Slots 0..3 = the four fans, slot 4 = the hero glyph. */
static double g_fan_ang[5] = { 0, 0, 0, 0, 0 };
static long   g_fan_ms = 0;
static int    g_fan_dt = 0;
static void fan_frame(void){                         /* call once per card render */
    long t = now_ms(), dt = t - g_fan_ms;
    if (dt < 0 || dt > 400) dt = 16;                 /* first frame / was off-screen */
    g_fan_ms = t; g_fan_dt = (int)dt;
}
static double fan_ang(int slot, int rpm){
    if (slot < 0 || slot > 4) return 0;
    double om = rpm > 0 ? 40.0 + rpm * 0.055 : 0.0;  /* deg/s: 585->72 .. 4000 capped */
    if (om > 300) om = 300;                           /* stays watchable, never a blur */
    g_fan_ang[slot] += om * g_fan_dt / 1000.0;
    if (g_fan_ang[slot] >= 3600.0) g_fan_ang[slot] -= 3600.0;
    return g_fan_ang[slot];
}

static int mod_fans(int y, stats_t *st, int variant){
    char b[32];
    int n = st->n_fan_rpm;
    if (n <= 0) return value_card(y, 76, "FANS", "n/a", UN_DIM);
    if (n > 4) n = 4;

    int any = 0, on = 0, sum = 0, mx = 0;
    for (int i = 0; i < n; i++){
        int rp = st->fan_rpm[i];
        if (rp > 0){ any = 1; on++; sum += rp; if (rp > mx) mx = rp; }
    }
    int mode = (unsigned)cfg_fan_mode % 5;
    uint32_t tagcol = mode ? UN_ORANGE_M : UN_DIM;
    fan_frame();

    /* ---- compact: one text line, no animation ---- */
    if (variant == 3){
        if (mx > 0) snprintf(b, sizeof b, "%d/%d  %d rpm", on, n, mx);
        else        snprintf(b, sizeof b, "%d/%d  stopped", on, n);
        return value_card(y, 60, "FANS", b, any ? UN_TEXT : UN_DIM);
    }

    /* ---- hero: one large spinning fan + aggregate rpm ---- */
    if (variant == 2){
        int h0 = 232, R = gy(58);
        card(y, gy(h0), "FANS");
        card_tag(y, fanmode_tag[mode], tagcol);
        int cx = W / 2, cy = y + gy(38) + R;
        draw_fan(cx, cy, R, fan_ang(4, mx), any);
        int avg = on ? sum / on : 0;
        snprintf(b, sizeof b, "%d", avg);
        text(cx - text_w(3.8f, b) / 2, cy + R + gy(6), 3.8f, any ? UN_TEXT : UN_DIM, b);
        text_c(cy + R + gy(44), 1.5f, UN_DIM, "rpm avg");
        snprintf(b, sizeof b, "%d/%d spinning", on, n);
        text_c(y + gy(h0) - gy(20), 1.5f, UN_DIM, b);
        if (any && card_visible(y, gy(h0))) g_anim = 1;
        return gy(h0) + gy(C_GAP);
    }

    /* ---- list: a row per fan (mini spinning icon + role + rpm) ---- */
    if (variant == 1){
        int top = 42, rowh = 38, h0 = top + n * rowh + 6;
        card(y, gy(h0), "FANS");
        card_tag(y, fanmode_tag[mode], tagcol);
        for (int i = 0; i < n; i++){
            int rp = st->fan_rpm[i];
            int ry = y + gy(top + i * rowh);
            draw_fan(C_X0 + gy(14), ry + gy(15), gy(14), fan_ang(i, rp), rp > 0);
            text(C_X0 + gy(38), ry + gy(6), 1.9f, UN_TEXT, fan_name(n, i));
            if (rp > 0) snprintf(b, sizeof b, "%d rpm", rp);
            else        snprintf(b, sizeof b, "stopped");
            text(C_R - text_w(1.9f, b), ry + gy(6), 1.9f, rp > 0 ? UN_TEXT : UN_DIM, b);
        }
        if (any && card_visible(y, gy(h0))) g_anim = 1;
        return gy(h0) + gy(C_GAP);
    }

    /* ---- dials (default): a 2-column grid of spinning fans ---- */
    int cols = n > 1 ? 2 : 1, rows = (n + cols - 1) / cols;
    int cellw = C_W / cols, top = 44, cellh = 98, h0 = top + rows * cellh + 4;
    int R = gy(23);
    card(y, gy(h0), "FANS");
    card_tag(y, fanmode_tag[mode], tagcol);
    for (int i = 0; i < n; i++){
        int rp = st->fan_rpm[i];
        int c = i % cols, rw = i / cols;
        int cxs = C_X0 + c * cellw + cellw / 2;
        int cys = y + gy(top) + rw * gy(cellh);
        draw_fan(cxs, cys + gy(26), R, fan_ang(i, rp), rp > 0);
        const char *nm = fan_name(n, i);
        text(cxs - text_w(1.5f, nm) / 2, cys + gy(56), 1.5f, UN_DIM, nm);
        if (rp > 0) snprintf(b, sizeof b, "%d rpm", rp);
        else        snprintf(b, sizeof b, "stopped");
        trunc_fit(b, 1.6f, cellw - gy(6));
        text(cxs - text_w(1.6f, b) / 2, cys + gy(74), 1.6f, rp > 0 ? UN_TEXT : UN_DIM, b);
    }
    if (any && card_visible(y, gy(h0))) g_anim = 1;
    return gy(h0) + gy(C_GAP);
}

#endif /* PANEL_MOD_FANS_H */
