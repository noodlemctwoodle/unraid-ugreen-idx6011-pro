/*
 * plugin/src/panel/modules/mod_shares.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "shares" module — Shares (all in one card). Reads read_shares() (stats_extra.h) and degrades to a
 * dim idle line when its source is absent. Part of panel_dash: #included in order.
 */
#ifndef PANEL_MOD_SHARES_H
#define PANEL_MOD_SHARES_H

/* "shares" — all user shares in ONE list card (like mod_disks all-in-one), a row
 * per share: health dot + name (left) + used size (right), via item_head so every
 * field truncates to its zone. Caps the visible rows and appends "+N more" when
 * over. variant: 0=card (roomier rows), 1=compact (tighter, more rows). Robust:
 * no shares.ini / no shares -> n_shares=0 -> dim idle line. */
static int mod_shares(int y, stats_t *st, int variant){
    if (st->n_shares == 0)
        return value_card(y, 84, "SHARES", "No shares", UN_DIM);

    int compact  = (variant == 1);
    int rowh     = compact ? 26 : 32;
    int head     = 36, pad = 12, maxrows = compact ? 6 : 5;
    float name_sc = compact ? 1.8f : 2.1f, right_sc = compact ? 1.5f : 1.8f;

    int shown = st->n_shares, over = 0;
    if (shown > maxrows){ shown = maxrows; over = 1; }
    int ch = head + (shown + (over ? 1 : 0)) * rowh + pad;

    card(y, gy(ch), "SHARES");
    int ry = y + gy(head);
    for (int i = 0; i < shown; i++){
        char used[24];
        fmt_bytes(used, sizeof used, (unsigned long long)(st->shares[i].used_gb * 1e9));
        item_head(ry, col_health(st->shares[i].health), st->shares[i].name,
                  name_sc, used, right_sc, UN_DIM);
        ry += gy(rowh);
    }
    if (over){
        char more[24];
        snprintf(more, sizeof more, "+%d more", st->n_shares - maxrows);
        text(C_X0 + gy(18), ry + gy(12), right_sc, UN_DIM, more);
    }
    return gy(ch) + gy(C_GAP);
}

#endif /* PANEL_MOD_SHARES_H */
