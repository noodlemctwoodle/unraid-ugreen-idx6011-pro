/*
 * plugin/src/panel/modules/mod_shares.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * Share cards. "shares" draws one card per user share; "share" (indexed, e.g.
 * share:Media or share:Media:compact) draws a single share. Both share
 * share_card() + the shim. Each shows the share's placement (its cache pool or
 * the array), the pool's free space and health. Styles: card, compact.
 * Reads read_shares() (stats_extra.h). Part of panel_dash: #included in order.
 */
#ifndef PANEL_MOD_SHARES_H
#define PANEL_MOD_SHARES_H

/* one share. style: 0=card (name + placement + free), 1=compact (name + placement). */
static int share_card(int y, stats_t *st, int i, int style){
    uint32_t dot = col_health(st->shares[i].health);
    const char *nm = st->shares[i].name, *wh = st->shares[i].where;
    if (style == 1){                                     /* compact — dot + name + placement */
        int ch = 46;
        card(y, gy(ch), NULL);
        item_head(y, dot, nm, 1.9f, wh, 1.5f, UN_DIM);
        return gy(ch) + gy(C_GAP);
    }
    int ch = 82;                                         /* card — + pool free */
    card(y, gy(ch), NULL);
    item_head(y, dot, nm, 2.1f, wh, 1.6f, UN_DIM);
    char fr[24], b[40];
    fmt_bytes(fr, sizeof fr, (unsigned long long)(st->shares[i].free_gb * 1e9));
    snprintf(b, sizeof b, "%s free", fr);
    text(C_X0, y + gy(46), 1.7f, UN_DIM, b);
    return gy(ch) + gy(C_GAP);
}

static int mod_shares(int y, stats_t *st, int variant){      /* all shares (page scrolls) */
    if (st->n_shares == 0)
        return value_card(y, 84, "SHARES", "No shares", UN_DIM);
    int y0 = y;
    for (int i = 0; i < st->n_shares; i++) y += share_card(y, st, i, variant);
    return y - y0;
}

static int mod_share(int y, stats_t *st, int variant){       /* one share, "name" or "name:style" */
    (void)variant;
    char key[64]; snprintf(key, sizeof key, "%s", g_item_key);
    char *style = strchr(key, ':'); if (style) *style++ = 0;
    int sidx = (style && !strcmp(style, "compact")) ? 1 : 0;
    int i = -1;
    if (key[0]){
        for (int k = 0; k < st->n_shares; k++) if (!strcmp(st->shares[k].name, key)){ i = k; break; }
        if (i < 0 && key[0] >= '0' && key[0] <= '9'){ int n = atoi(key); if (n >= 0 && n < st->n_shares) i = n; }
    } else if (st->n_shares > 0) i = 0;
    if (i < 0) return value_card(y, 76, "SHARE", "not present", UN_DIM);
    return share_card(y, st, i, sidx);
}

#endif /* PANEL_MOD_SHARES_H */
