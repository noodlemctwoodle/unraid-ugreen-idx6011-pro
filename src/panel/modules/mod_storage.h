/*
 * plugin/src/panel/modules/mod_storage.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "storage" module — array used % (metric card). Variants: 0=bar, 1=ring.
 * Composes the card style shim (cardstyle.h).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_STORAGE_H
#define PANEL_MOD_STORAGE_H

static int mod_storage(int y, stats_t *st, int variant){
    (void)variant;
    const char *s = g_item_key;                          /* style name */
    double p = st->disk_used_pct;
    char v[32]; snprintf(v, sizeof v, "%.0f%%", p);
    if (!strcmp(s, "graph") || !strcmp(s, "area")) return graph_card(y, "STORAGE", v, h_sto, UN_ORANGE_M, s[0] == 'a', 100);
    if (!strcmp(s, "blocks")) return blocks_card(y, "STORAGE", p, v);
    if (!strcmp(s, "trend"))  return trend_card(y, "STORAGE", v, h_sto, UN_ORANGE_M);
    if (!strcmp(s, "split")){
        char u[24], f[24];
        snprintf(u, sizeof u, "%.0f GB", st->disk_used_gb);
        snprintf(f, sizeof f, "%.0f free", st->disk_tot_gb - st->disk_used_gb);
        return split_card(y, "STORAGE", p, u, f);
    }
    int style = !strcmp(s, "ring") ? 1 : !strcmp(s, "big") ? 2 : !strcmp(s, "gauge") ? 3 : 0;
    int h = metric_card(y, "STORAGE", p, v, style);
    if (style == 0 || style == 1){
        snprintf(v, sizeof v, "%.0f / %.0f GB", st->disk_used_gb, st->disk_tot_gb);
        metric_detail(y, v, UN_DIM, style == 1);
    }
    return h;
}

#endif /* PANEL_MOD_STORAGE_H */
