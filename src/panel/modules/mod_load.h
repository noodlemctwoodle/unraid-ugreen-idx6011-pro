/*
 * plugin/src/panel/modules/mod_load.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "load" module — system load average (1 / 5 / 15 min) from /proc/loadavg. Big
 * 1-minute value with a 5m/15m sub-line on its own row; 0.00 if unreadable.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_LOAD_H
#define PANEL_MOD_LOAD_H

static int mod_load(int y, stats_t *st, int variant){
    (void)variant;
    int h = 108;
    card(y, gy(h), "LOAD AVG");
    char big[16]; snprintf(big, sizeof big, "%.2f", st->load1);       /* 1-min value, left */
    text(C_X0, y + gy(44), 3.6f, UN_TEXT, big);
    char sub[48];                                                     /* own row below — no overlap */
    snprintf(sub, sizeof sub, "5m %.2f \xC2\xB7 15m %.2f", st->load5, st->load15);
    trunc_fit(sub, C_SUB, C_W);
    text(C_X0, y + gy(84), C_SUB, UN_DIM, sub);
    return gy(h) + gy(C_GAP);
}

#endif /* PANEL_MOD_LOAD_H */
