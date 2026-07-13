/*
 * plugin/src/panel/modules/mod_power.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "power" module — package / system power draw (value card).
 * Composes the card style shim (cardstyle.h).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_POWER_H
#define PANEL_MOD_POWER_H

static int mod_power(int y, stats_t *st, int variant){
    (void)variant;
    if (st->pwr_pkg_w < 0) return value_card(y, 76, "POWER", "n/a", UN_DIM);
    char v[48];
    if (st->pwr_sys_w > st->pwr_pkg_w)
        snprintf(v, sizeof v, "%.1f W  (cpu %.1f W)", st->pwr_sys_w, st->pwr_pkg_w);
    else
        snprintf(v, sizeof v, "%.1f W", st->pwr_pkg_w);
    return value_card(y, 76, "POWER", v, UN_TEXT);
}

#endif /* PANEL_MOD_POWER_H */
