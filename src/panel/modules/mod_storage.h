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
    char v[32];
    snprintf(v, sizeof v, "%.0f%%", st->disk_used_pct);
    int h = metric_card(y, "STORAGE", st->disk_used_pct, v, variant == 1);
    snprintf(v, sizeof v, "%.0f / %.0f GB", st->disk_used_gb, st->disk_tot_gb);
    metric_detail(y, v, UN_DIM, variant == 1);
    return h;
}

#endif /* PANEL_MOD_STORAGE_H */
