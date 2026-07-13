/*
 * plugin/src/panel/modules/mod_npu.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "npu" module — NPU presence / activity (value card).
 * Composes the card style shim (cardstyle.h).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_NPU_H
#define PANEL_MOD_NPU_H

static int mod_npu(int y, stats_t *st, int variant){
    (void)variant;
    if (!st->npu_avail) return value_card(y, 76, "NPU", "not present", UN_DIM);
    char v[48];
    if (st->npu_freq > 0) snprintf(v, sizeof v, "busy %.0f%%  %d MHz", st->npu_busy, st->npu_freq);
    else                  snprintf(v, sizeof v, "present (idle)");
    return value_card(y, 76, "NPU", v, UN_TEXT);
}

#endif /* PANEL_MOD_NPU_H */
