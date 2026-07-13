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
    snprintf(v, sizeof v, "%.0f%%", st->gpu_busy);
    if (variant == 1){                                   /* graph */
        int h = spark_card(y, "GPU", v, h_gpu, h_cnt, h_pos, UN_ORANGE);
        if (st->gpu_freq >= 0){
            char t[16]; snprintf(t, sizeof t, "%d MHz", st->gpu_freq);
            card_tag(y, t, UN_DIM);
        }
        return h;
    }
    int style = variant==2 ? 2 : variant==3 ? 3 : 0;     /* big/gauge/bar */
    int h = metric_card(y, "GPU", st->gpu_busy, v, style);
    if (style == 0 && st->gpu_freq >= 0){
        char t[16]; snprintf(t, sizeof t, "%d MHz", st->gpu_freq);
        card_tag(y, t, UN_DIM);
    }
    return h;
}

#endif /* PANEL_MOD_GPU_H */
