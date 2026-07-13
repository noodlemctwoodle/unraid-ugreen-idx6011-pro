/*
 * plugin/src/panel/modules/mod_cpu_temp.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "cputemp" module — CPU package temperature (bar card).
 * Composes the card style shim (cardstyle.h).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_CPU_TEMP_H
#define PANEL_MOD_CPU_TEMP_H

static int mod_cputemp(int y, stats_t *st, int variant){
    (void)variant;
    if (st->temp_c <= 0) return value_card(y, 84, "CPU TEMP", "n/a", UN_DIM);
    char v[16]; snprintf(v, sizeof v, "%dC", st->temp_c);
    return bar_card(y, 84, "CPU TEMP", v, st->temp_c, col_temp(st->temp_c));
}

#endif /* PANEL_MOD_CPU_TEMP_H */
