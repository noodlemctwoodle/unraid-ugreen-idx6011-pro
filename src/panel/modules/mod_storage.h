/*
 * plugin/src/panel/modules/mod_storage.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "storage" module — array used %. Variants: 0=bar, 1=ring.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_STORAGE_H
#define PANEL_MOD_STORAGE_H

static int mod_storage(int y, stats_t *st, int variant){
    char b[64];
    card(y, 128, "STORAGE");
    if (variant == 1){                                   /* ring */
        int cx = 58, cy = y + 74, ro = 36, ri = 24;
        ring_gauge(cx, cy, ro, ri, st->disk_used_pct, level_col(st->disk_used_pct));
        snprintf(b, sizeof b, "%.0f%%", st->disk_used_pct);
        text(cx - text_w(2.0f, b) / 2, cy - 8, 2.0f, UN_TEXT, b);
        snprintf(b, sizeof b, "%.0f / %.0f GB", st->disk_used_gb, st->disk_tot_gb);
        text(110, y + 66, 1.9f, UN_TEXT, b);
    } else {                                             /* bar (default) */
        snprintf(b, sizeof b, "%.0f%%", st->disk_used_pct);
        text(W - 22 - text_w(1.9f, b), y + 10, 1.9f, UN_DIM, b);
        snprintf(b, sizeof b, "%.0f / %.0f GB", st->disk_used_gb, st->disk_tot_gb);
        text(22, y + 44, 2.4f, UN_TEXT, b);
        bar(22, y + 94, W - 44, 18, st->disk_used_pct, level_col(st->disk_used_pct));
    }
    return 140;
}

#endif /* PANEL_MOD_STORAGE_H */
