/*
 * plugin/src/panel/modules/mod_power.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "power" module — package / system power draw (RAPL watts). Watts have no fixed
 * 0-100 scale, so the variants are card / big / graph / area (the history graphs
 * auto-scale) rather than bar/ring/gauge. Composes the card style shim.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_POWER_H
#define PANEL_MOD_POWER_H

static int mod_power(int y, stats_t *st, int variant){
    (void)variant;
    if (st->pwr_pkg_w < 0) return value_card(y, 76, "POWER", "n/a", UN_DIM);
    const char *s = g_item_key;
    double w = st->pwr_sys_w > st->pwr_pkg_w ? st->pwr_sys_w : st->pwr_pkg_w;
    char v[48]; snprintf(v, sizeof v, "%.1f W", w);
    if (!strcmp(s, "graph") || !strcmp(s, "area"))
        return graph_card(y, "POWER", v, h_pwr, UN_ORANGE_M, s[0] == 'a', 0);   /* auto-scale W */
    if (!strcmp(s, "big"))
        return big_value_card(y, "POWER", v, UN_TEXT);
    /* default card — pkg + sys breakdown */
    if (st->pwr_sys_w > st->pwr_pkg_w) snprintf(v, sizeof v, "%.1f W  (cpu %.1f W)", st->pwr_sys_w, st->pwr_pkg_w);
    else                               snprintf(v, sizeof v, "%.1f W", st->pwr_pkg_w);
    return value_card(y, 76, "POWER", v, UN_TEXT);
}

#endif /* PANEL_MOD_POWER_H */
