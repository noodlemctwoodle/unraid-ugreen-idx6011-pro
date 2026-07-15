/*
 * plugin/src/panel/modules/mod_flash.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "flash" module — Boot flash. Reads read_flash() (stats_extra.h).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_FLASH_H
#define PANEL_MOD_FLASH_H

static int mod_flash(int y, stats_t *st, int variant){
    (void)variant;
    if (st->flash_pct < 0 || st->flash_tot_gb <= 0)
        return value_card(y, 84, "BOOT FLASH", "n/a", UN_DIM);
    double p = st->flash_pct;
    char v[16]; snprintf(v, sizeof v, "%.0f%%", p);
    int h = metric_card(y, "BOOT FLASH", p, v, 0);   /* value + col_load bar */
    char d[32]; snprintf(d, sizeof d, "%.1f / %.0f GB", st->flash_used_gb, st->flash_tot_gb);
    metric_detail(y, d, UN_DIM, 0);                  /* used/total on its own row */
    return h;
}

#endif /* PANEL_MOD_FLASH_H */
