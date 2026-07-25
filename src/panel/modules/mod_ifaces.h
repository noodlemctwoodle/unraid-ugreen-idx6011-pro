/*
 * plugin/src/panel/modules/mod_ifaces.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Network interface cards. "ifaces" draws one card per interface (styles: full,
 * compact, mini, big); "iface" (indexed, e.g. iface:1) draws a single interface
 * so they can be split and reordered. Share iface_card() + the shim + col_dot/
 * state. Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_IFACES_H
#define PANEL_MOD_IFACES_H

/* one interface card. style: 0=full (IPs + totals), 1=compact (DN/UP, 2 lines),
 * 2=mini (one line: name + rates), 3=big (large DN/UP rates). */
static int iface_card(int y, iface_t *ic, int prim, int style){
    char b[64], r1[32], r2[32], lnk[16];
    /* right-hand status: link SPEED when up + known, else LINK / DOWN */
    if (!ic->up)                     snprintf(lnk, sizeof lnk, "DOWN");
    else if (ic->link_mbps >= 1000)  snprintf(lnk, sizeof lnk, "%gG", ic->link_mbps / 1000.0);
    else if (ic->link_mbps > 0)      snprintf(lnk, sizeof lnk, "%dM", ic->link_mbps);
    else                             snprintf(lnk, sizeof lnk, "LINK");

    if (style == 2){                                     /* mini — name as title + DN/UP full-width below */
        int ch = 74;
        card(y, gy(ch), NULL);
        rect(C_X0, y + gy(11), gy(11), gy(11), col_dot(ic->up), 255);
        htext(C_X0 + gy(18), y + gy(6), 1.9f, UN_TEXT, ic->name);
        if (!ic->up) text(C_R - text_w(1.4f, "DOWN"), y + gy(9), 1.4f, UN_DIM, "DOWN");
        fmt_rate(r1, sizeof r1, ic->rx_kbs); fmt_rate(r2, sizeof r2, ic->tx_kbs);
        snprintf(b, sizeof b, "DN %s   UP %s", r1, r2);
        trunc_fit(b, 1.7f, W - 44);
        text(C_X0, y + gy(42), 1.7f, UN_DIM, b);
        return gy(ch) + gy(C_GAP);
    }
    if (style == 3){                                     /* big — large DN/UP rates */
        int ch = 150;
        card(y, gy(ch), NULL);
        item_head(y, col_dot(ic->up), ic->name, 2.4f, lnk, 1.6f, col_state(ic->up));
        fmt_rate(r1, sizeof r1, ic->rx_kbs); snprintf(b, sizeof b, "DN  %s", r1);
        text(C_X0 + gy(4), y + gy(50), 2.9f, UN_TEXT, b);
        fmt_rate(r2, sizeof r2, ic->tx_kbs); snprintf(b, sizeof b, "UP  %s", r2);
        text(C_X0 + gy(4), y + gy(98), 2.9f, UN_TEXT, b);
        return gy(ch) + gy(C_GAP);
    }
    if (style == 1){                                     /* compact — name + link + DN/UP */
        int ch = 108;
        card(y, gy(ch), NULL);
        item_head(y, col_dot(ic->up), ic->name, 2.3f,
                  lnk, 1.5f, col_state(ic->up));
        fmt_rate(r1, sizeof r1, ic->rx_kbs); snprintf(b, sizeof b, "DN  %s", r1);
        text(C_X0 + gy(18), y + gy(44), 1.9f, UN_TEXT, b);
        fmt_rate(r2, sizeof r2, ic->tx_kbs); snprintf(b, sizeof b, "UP  %s", r2);
        text(C_X0 + gy(18), y + gy(72), 1.9f, UN_TEXT, b);
        return gy(ch) + gy(C_GAP);
    }
    int has6 = ic->ip6[0] != 0;                          /* full — IPs + totals */
    int ch = 172 + (has6 ? 22 : 0);
    card(y, gy(ch), NULL);
    item_head(y, col_dot(ic->up), ic->name, 2.4f, lnk, 1.6f, col_state(ic->up));
    if (prim)
        text(C_X0 + gy(18) + htext_w(2.4f, ic->name) + gy(12), y + gy(18), 1.4f, UN_ORANGE, "PRIMARY");
    int cy = y + gy(48);
    text(C_X0 + gy(18), cy, 2.0f, UN_DIM, ic->ip[0] ? ic->ip : "-"); cy += gy(28);
    if (has6){ snprintf(b, sizeof b, "%s", ic->ip6); trunc_fit(b, 1.5f, W - 64);
               text(C_X0 + gy(18), cy, 1.5f, UN_DIM, b); cy += gy(22); }
    fmt_rate(r1, sizeof r1, ic->rx_kbs); snprintf(b, sizeof b, "DN  %s", r1);
    text(C_X0 + gy(18), cy, 2.0f, UN_TEXT, b); cy += gy(28);
    fmt_rate(r2, sizeof r2, ic->tx_kbs); snprintf(b, sizeof b, "UP  %s", r2);
    text(C_X0 + gy(18), cy, 2.0f, UN_TEXT, b); cy += gy(30);
    fmt_bytes(r1, sizeof r1, ic->rx_tot); fmt_bytes(r2, sizeof r2, ic->tx_tot);
    snprintf(b, sizeof b, "RX %s   TX %s", r1, r2); trunc_fit(b, 1.6f, W - 62);
    text(C_X0 + gy(18), cy, 1.6f, UN_DIM, b);
    return gy(ch) + gy(C_GAP);
}

static int mod_ifaces(int y, stats_t *st, int variant){      /* all interfaces */
    if (st->n_ifaces == 0) return value_card(y, 90, "NETWORK", "no interfaces", UN_DIM);
    int y0 = y;                                              /* variant = style */
    for (int i = 0; i < st->n_ifaces; i++)
        y += iface_card(y, &st->ifc[i], !strcmp(st->ifc[i].name, st->prim_if), variant);
    return y - y0;
}

static int mod_iface(int y, stats_t *st, int variant){       /* one interface, "name" or "name:style" */
    (void)variant;
    char key[64]; snprintf(key, sizeof key, "%s", g_item_key);
    char *style = strchr(key, ':'); if (style) *style++ = 0;  /* split name : style */
    int sidx = 0;                                             /* full / compact / mini / big */
    if (style){ if (!strcmp(style, "compact")) sidx = 1; else if (!strcmp(style, "mini")) sidx = 2; else if (!strcmp(style, "big")) sidx = 3; }
    int i = -1;
    if (key[0]){
        for (int k = 0; k < st->n_ifaces; k++) if (!strcmp(st->ifc[k].name, key)){ i = k; break; }
        if (i < 0 && key[0] >= '0' && key[0] <= '9'){ int n = atoi(key); if (n >= 0 && n < st->n_ifaces) i = n; }
    } else if (st->n_ifaces > 0) i = 0;
    if (i < 0) return value_card(y, 76, "INTERFACE", "not present", UN_DIM);
    return iface_card(y, &st->ifc[i], !strcmp(st->ifc[i].name, st->prim_if), sidx);
}

#endif /* PANEL_MOD_IFACES_H */
