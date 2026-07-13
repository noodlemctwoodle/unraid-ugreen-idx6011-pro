/*
 * plugin/src/panel/modules/mod_cpu.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "cpu" module — CPU load (metric card) with temp + package power. Variants:
 * 0=bar, 1=ring. Composes the card style shim (cardstyle.h) — no hand layout.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_CPU_H
#define PANEL_MOD_CPU_H

static int mod_cpu(int y, stats_t *st, int variant){
    char v[32];
    snprintf(v, sizeof v, "%.0f%%", st->cpu);
    int h = metric_card(y, "CPU", st->cpu, v, variant == 1);
    if (st->temp_c > 0){
        snprintf(v, sizeof v, "%dC", st->temp_c);
        card_sub(y, 0, v, st->temp_c > 85 ? UN_BAD : UN_DIM);
    }
    /* psys RAPL is miscalibrated on this board (reads ~0); package matches webUI */
    double pw = st->pwr_pkg_w;
    if (st->pwr_sys_w > st->pwr_pkg_w) pw = st->pwr_sys_w;
    if (pw >= 0){
        snprintf(v, sizeof v, "%.1f W", pw);
        card_sub(y, 1, v, UN_DIM);
    }
    return h;
}

#endif /* PANEL_MOD_CPU_H */
