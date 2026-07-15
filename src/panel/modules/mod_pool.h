/*
 * plugin/src/panel/modules/mod_pool.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "pool" module — Pools (cache) all in one card. Reads read_pool() (stats_extra.h) and degrades to a
 * dim idle line when its source is absent. Part of panel_dash: #included in order.
 */
#ifndef PANEL_MOD_POOL_H
#define PANEL_MOD_POOL_H

static int mod_pools(int y, stats_t *st, int variant){
    if (st->n_pools == 0)
        return value_card(y, 84, "POOLS", "No pools", UN_DIM);
    int compact = (variant == 1), y0 = y;
    for (int i = 0; i < st->n_pools; i++){
        double pct   = st->pools[i].pct;
        uint32_t col = col_load(pct);
        char right[16]; snprintf(right, sizeof right, "%.0f%%", pct);
        if (compact){                                        /* header row + bar */
            int ch = 82;
            card(y, gy(ch), NULL);
            item_head(y, col, st->pools[i].name, 2.1f, right, 1.8f, col);
            bar(C_X0, y + gy(46), W - 92, gy(12), pct, col);
            y += gy(ch) + gy(C_GAP);
        } else {                                             /* header + used/tot + bar */
            int ch = 104;
            char b[48];
            card(y, gy(ch), NULL);
            item_head(y, col, st->pools[i].name, 2.3f, right, 2.0f, col);
            snprintf(b, sizeof b, "%.1f / %.1f GB",
                     st->pools[i].used_gb, st->pools[i].tot_gb);
            text(C_X0, y + gy(44), 1.7f, UN_DIM, b);
            bar(C_X0, y + gy(72), C_W, gy(14), pct, col);
            y += gy(ch) + gy(C_GAP);
        }
    }
    return y - y0;
}

#endif /* PANEL_MOD_POOL_H */
