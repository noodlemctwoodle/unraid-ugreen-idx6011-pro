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
    (void)variant;
    const char *s = g_item_key;                          /* style name */
    double mp = st->mem_tot_mb ? 100.0 * st->mem_used_mb / st->mem_tot_mb : 0;
    char v[32]; snprintf(v, sizeof v, "%.0f%%", mp);
    if (!strcmp(s, "graph") || !strcmp(s, "area")) return graph_card(y, "MEMORY", v, h_mem, UN_ORANGE_M, s[0] == 'a', 100);
    if (!strcmp(s, "blocks")) return blocks_card(y, "MEMORY", mp, v);
    if (!strcmp(s, "trend"))  return trend_card(y, "MEMORY", v, h_mem, UN_ORANGE_M);
    if (!strcmp(s, "split")){
        char u[24], f[24];
        snprintf(u, sizeof u, "%.1f GB", st->mem_used_mb / 1024.0);
        snprintf(f, sizeof f, "%.1f free", (st->mem_tot_mb - st->mem_used_mb) / 1024.0);
        return split_card(y, "MEMORY", mp, u, f);
    }
    int style = !strcmp(s, "ring") ? 1 : !strcmp(s, "big") ? 2 : !strcmp(s, "gauge") ? 3 : 0;
    int h = metric_card(y, "MEMORY", mp, v, style);
    if (style == 0 || style == 1){
        snprintf(v, sizeof v, "%.1f / %.1f GB", st->mem_used_mb / 1024.0, st->mem_tot_mb / 1024.0);
        metric_detail(y, v, UN_DIM, style == 1);
    }
    return h;
}

#endif /* PANEL_MOD_MEM_H */
