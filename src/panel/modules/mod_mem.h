/*
 * plugin/src/panel/modules/mod_mem.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "mem" module — memory usage. Variants: 0=bar, 1=ring.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_MEM_H
#define PANEL_MOD_MEM_H

static int mod_mem(int y, stats_t *st, int variant){
    char b[64];
    card(y, 128, "MEMORY");
    double mp = st->mem_tot_mb ? 100.0 * st->mem_used_mb / st->mem_tot_mb : 0;
    if (variant == 1){                                   /* ring */
        int cx = 58, cy = y + 74, ro = 36, ri = 24;
        ring_gauge(cx, cy, ro, ri, mp, level_col(mp));
        snprintf(b, sizeof b, "%.0f%%", mp);
        text(cx - text_w(2.0f, b) / 2, cy - 8, 2.0f, UN_TEXT, b);
        snprintf(b, sizeof b, "%.1f / %.1f GB", st->mem_used_mb / 1024.0, st->mem_tot_mb / 1024.0);
        text(110, y + 66, 1.9f, UN_TEXT, b);
    } else {                                             /* bar (default) */
        snprintf(b, sizeof b, "%.0f%%", mp);
        text(W - 22 - text_w(1.9f, b), y + 10, 1.9f, UN_DIM, b);
        snprintf(b, sizeof b, "%.1f / %.1f GB", st->mem_used_mb / 1024.0, st->mem_tot_mb / 1024.0);
        text(22, y + 44, 2.4f, UN_TEXT, b);
        bar(22, y + 94, W - 44, 18, mp, level_col(mp));
    }
    return 140;
}

#endif /* PANEL_MOD_MEM_H */
