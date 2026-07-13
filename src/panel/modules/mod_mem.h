/*
 * plugin/src/panel/modules/mod_mem.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "mem" module — memory usage (metric card). Variants: 0=bar, 1=ring.
 * Composes the card style shim (cardstyle.h). Big % + used/total as a sub, so it
 * reads identically to the CPU/Storage cards.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_MEM_H
#define PANEL_MOD_MEM_H

static int mod_mem(int y, stats_t *st, int variant){
    char v[32];
    double mp = st->mem_tot_mb ? 100.0 * st->mem_used_mb / st->mem_tot_mb : 0;
    snprintf(v, sizeof v, "%.0f%%", mp);
    int h = metric_card(y, "MEMORY", mp, v, variant == 1);
    snprintf(v, sizeof v, "%.1f / %.1f GB", st->mem_used_mb / 1024.0, st->mem_tot_mb / 1024.0);
    if (variant == 1) ring_detail(y, 74, v, UN_DIM);     /* right of the ring */
    else              card_sub(y, 0, v, UN_DIM);          /* right-aligned (bar) */
    return h;
}

#endif /* PANEL_MOD_MEM_H */
