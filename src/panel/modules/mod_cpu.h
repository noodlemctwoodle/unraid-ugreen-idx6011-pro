/*
 * plugin/src/panel/modules/mod_cpu.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "cpu" module — CPU load. Variants: bar, ring, graph (sparkline), big, gauge.
 * Composes the card style shim (cardstyle.h) — no hand layout.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_CPU_H
#define PANEL_MOD_CPU_H

static int mod_cpu(int y, stats_t *st, int variant){
    char v[32];
    snprintf(v, sizeof v, "%.0f%%", st->cpu);
    if (variant == 2){                                   /* graph */
        int h = spark_card(y, "CPU", v, h_cpu, h_cnt, h_pos, UN_ORANGE_M);
        if (st->cpu_threads > 0){
            char t[16]; snprintf(t, sizeof t, "%dT", st->cpu_threads);
            card_tag(y, t, UN_DIM);
        }
        return h;
    }
    int style = variant==1 ? 1 : variant==3 ? 2 : variant==4 ? 3 : 0;  /* ring/big/gauge/bar */
    int h = metric_card(y, "CPU", st->cpu, v, style);
    if (style == 0 || style == 1){                       /* temp/power fit bar + ring only */
        if (st->temp_c > 0){
            snprintf(v, sizeof v, "%dC", st->temp_c);
            card_sub(y, 0, v, col_temp(st->temp_c));
        }
        double pw = st->pwr_pkg_w;
        if (st->pwr_sys_w > st->pwr_pkg_w) pw = st->pwr_sys_w;
        if (pw >= 0){ snprintf(v, sizeof v, "%.1f W", pw); card_sub(y, 1, v, UN_DIM); }
    }
    return h;
}

#endif /* PANEL_MOD_CPU_H */
