/*
 * plugin/src/panel/modules/mod_gpu.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "gpu" module — iGPU busy %. Variants: bar, graph (sparkline), big, gauge.
 * Composes the card style shim (cardstyle.h).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_GPU_H
#define PANEL_MOD_GPU_H

static int mod_gpu(int y, stats_t *st, int variant){
    char v[32];
    if (!st->gpu_avail) return value_card(y, 76, "GPU", "not available", UN_DIM);
    const char *s = g_item_key;                          /* style name */
    (void)variant;
    snprintf(v, sizeof v, "%.0f%%", st->gpu_busy);
    if (!strcmp(s, "graph") || !strcmp(s, "area")){
        int h = graph_card(y, "GPU", v, h_gpu, UN_ORANGE, s[0] == 'a', 100);
        if (st->gpu_freq >= 0){ char t[16]; snprintf(t, sizeof t, "%d MHz", st->gpu_freq); card_tag(y, t, UN_DIM); }
        return h;
    }
    if (!strcmp(s, "blocks")) return blocks_card(y, "GPU", st->gpu_busy, v);
    if (!strcmp(s, "trend"))  return trend_card(y, "GPU", v, h_gpu, UN_ORANGE);
    int style = !strcmp(s, "big") ? 2 : !strcmp(s, "gauge") ? 3 : 0;
    int h = metric_card(y, "GPU", st->gpu_busy, v, style);
    if (style == 0 && st->gpu_freq >= 0){
        char t[16]; snprintf(t, sizeof t, "%d MHz", st->gpu_freq);
        card_tag(y, t, UN_DIM);
    }
    return h;
}

#endif /* PANEL_MOD_GPU_H */
