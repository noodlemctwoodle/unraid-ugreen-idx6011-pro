/*
 * plugin/src/panel/modules/mod_cores.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "cores" module — CPU cores. Reads read_cores() (stats_extra.h).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_CORES_H
#define PANEL_MOD_CORES_H

/* per-core CPU load — a compact grid (or a tight row) of one small bar per core,
 * each coloured by its own load via col_load(), with an average-load + average-
 * frequency header. Style comes from the variant name (g_item_key), like the
 * other metric modules: "grid" (default) / "bars". Always present. */
static int mod_cores(int y, stats_t *st, int variant){
    (void)variant;
    int n = st->n_cores;
    if (n > MAX_CORES) n = MAX_CORES;
    if (n <= 0) return value_card(y, 84, "CPU CORES", "no per-core data", UN_DIM);

    double avg = 0;
    for (int i = 0; i < n; i++) avg += st->core_pct[i];
    avg /= n;

    char hv[48];                                         /* short fixed-format header value */
    if (st->cpu_mhz > 0) snprintf(hv, sizeof hv, "%.0f%% avg \xC2\xB7 %d MHz", avg, st->cpu_mhz);
    else                 snprintf(hv, sizeof hv, "%.0f%% avg", avg);
    trunc_fit(hv, C_SUB, C_W);

    if (!strcmp(g_item_key, "bars")){                    /* one tight row of thin vertical bars */
        int h = 96;
        card(y, gy(h), "CPU CORES");
        text(C_X0, y + gy(36), C_SUB, UN_DIM, hv);
        int bw = C_W / n; if (bw < 2) bw = 2;
        int gap = bw > 4 ? 1 : 0;
        int bh = gy(30), by = y + gy(58), bx = C_X0;
        for (int i = 0; i < n; i++){
            double p = st->core_pct[i];
            int fh = (int)(bh * p / 100.0 + 0.5);
            rect(bx, by, bw - gap, bh, UN_GREY_70, 255);                 /* track */
            if (fh > 0) rect(bx, by + bh - fh, bw - gap, fh, col_load(p), 255);
            bx += bw;
        }
        return gy(h) + gy(C_GAP);
    }

    /* grid (default) — 2 columns of small horizontal load bars */
    int rows = (n + 1) / 2;
    int h = 58 + rows * 13 + 6;
    card(y, gy(h), "CPU CORES");
    text(C_X0, y + gy(36), C_SUB, UN_DIM, hv);
    int colw = (C_W - gy(8)) / 2, bh = gy(8);
    for (int i = 0; i < n; i++){
        int col = i % 2, row = i / 2;
        int bx = C_X0 + col * (colw + gy(8));
        double p = st->core_pct[i];
        bar(bx, y + gy(58 + row * 13), colw, bh, p, col_load(p));
    }
    return gy(h) + gy(C_GAP);
}

#endif /* PANEL_MOD_CORES_H */
