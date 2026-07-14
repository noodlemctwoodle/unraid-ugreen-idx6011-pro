/*
 * plugin/src/panel/modules/mod_npu.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "npu" module — NPU activity (a percentage). Full metric variants like CPU:
 * bar, ring, graph, area, blocks, trend, big, gauge. Composes the card style
 * shim (cardstyle.h). Part of panel_dash: #included in a fixed order.
 */
#ifndef PANEL_MOD_NPU_H
#define PANEL_MOD_NPU_H

static int mod_npu(int y, stats_t *st, int variant){
    (void)variant;
    if (!st->npu_avail) return value_card(y, 76, "NPU", "not present", UN_DIM);
    const char *s = g_item_key;
    char v[32]; snprintf(v, sizeof v, "%.0f%%", st->npu_busy);
    if (!strcmp(s, "graph") || !strcmp(s, "area")){
        int h = graph_card(y, "NPU", v, h_npu, UN_ORANGE_M, s[0] == 'a', 100);
        if (st->npu_freq > 0){ char t[16]; snprintf(t, sizeof t, "%d MHz", st->npu_freq); card_tag(y, t, UN_DIM); }
        return h;
    }
    if (!strcmp(s, "blocks")) return blocks_card(y, "NPU", st->npu_busy, v);
    if (!strcmp(s, "trend"))  return trend_card(y, "NPU", v, h_npu, UN_ORANGE_M);
    int style = !strcmp(s, "ring") ? 1 : !strcmp(s, "big") ? 2 : !strcmp(s, "gauge") ? 3 : 0;
    int h = metric_card(y, "NPU", st->npu_busy, v, style);
    if ((style == 0 || style == 1) && st->npu_freq > 0){
        snprintf(v, sizeof v, "%d MHz", st->npu_freq); card_sub(y, 0, v, UN_DIM); }
    return h;
}

#endif /* PANEL_MOD_NPU_H */
