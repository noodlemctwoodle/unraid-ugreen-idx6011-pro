/*
 * plugin/src/panel/modules/mod_load.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "load" module — Load average. Reads read_load() (stats_extra.h) and degrades to a
 * dim idle line when its source is absent. Part of panel_dash: #included in order.
 */
#ifndef PANEL_MOD_LOAD_H
#define PANEL_MOD_LOAD_H

static int mod_load(int y, stats_t *st, int variant){
    (void)variant;
    char big[16], sub[48];
    snprintf(big, sizeof big, "%.2f", st->load1);
    snprintf(sub, sizeof sub, "5m %.2f · 15m %.2f", st->load5, st->load15);
    int h = big_value_card(y, "LOAD AVG", big, UN_TEXT);   /* title + big centred 1-min value */
    metric_detail(y, sub, UN_DIM, 0);                      /* own-row, full-width, truncated */
    return h;
}

#endif /* PANEL_MOD_LOAD_H */
