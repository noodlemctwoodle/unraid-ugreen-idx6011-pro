/*
 * plugin/src/panel/modules/mod_cpu.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "cpu" module — CPU load with temp + package power. Variants: 0=bar, 1=ring.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_CPU_H
#define PANEL_MOD_CPU_H

static int mod_cpu(int y, stats_t *st, int variant){
    char b[64];
    card(y, 142, "CPU");
    /* temp (top-right) + package power (below it) — shown in both variants */
    if (st->temp_c > 0){
        snprintf(b, sizeof b, "%dC", st->temp_c);
        text(W - 22 - text_w(2.8f, b), y + 50, 2.8f, st->temp_c > 85 ? UN_BAD : UN_DIM, b);
    }
    /* psys RAPL is miscalibrated on this board (reads ~0); package matches webUI */
    double pw = st->pwr_pkg_w;
    if (st->pwr_sys_w > st->pwr_pkg_w) pw = st->pwr_sys_w;
    if (pw >= 0){
        snprintf(b, sizeof b, "%.1f W", pw);
        text(W - 22 - text_w(2.0f, b), y + 80, 2.0f, UN_DIM, b);
    }
    snprintf(b, sizeof b, "%.0f%%", st->cpu);
    if (variant == 1){                                   /* ring */
        int cx = 60, cy = y + 82, ro = 40, ri = 27;
        ring_gauge(cx, cy, ro, ri, st->cpu, level_col(st->cpu));
        text(cx - text_w(2.2f, b) / 2, cy - 9, 2.2f, UN_TEXT, b);
    } else {                                             /* bar (default) */
        text(22, y + 44, 4.8f, UN_TEXT, b);
        bar(22, y + 108, W - 44, 18, st->cpu, level_col(st->cpu));
    }
    return 154;
}

#endif /* PANEL_MOD_CPU_H */
