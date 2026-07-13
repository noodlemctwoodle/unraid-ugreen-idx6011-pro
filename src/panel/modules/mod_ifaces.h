/*
 * plugin/src/panel/modules/mod_ifaces.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "ifaces" module — one card per network interface (name, IPs, live rates,
 * totals). Draws all interfaces and returns their combined height. Composes the
 * card style shim + semantic colours (col_dot / col_state).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_IFACES_H
#define PANEL_MOD_IFACES_H

static int mod_ifaces(int y, stats_t *st, int variant){
    (void)variant;
    char b[64], r1[32], r2[32];
    if (st->n_ifaces == 0)
        return value_card(y, 90, "NETWORK", "no interfaces", UN_DIM);
    int y0 = y;
    for (int i = 0; i < st->n_ifaces; i++){
        iface_t *ic = &st->ifc[i];
        int has6 = ic->ip6[0] != 0;
        int ch = 172 + (has6 ? 22 : 0);
        card(y, ch, NULL);
        item_head(y, col_dot(ic->up), ic->name, 2.4f,
                  ic->up ? "LINK" : "DOWN", 1.6f, col_state(ic->up));
        if (!strcmp(ic->name, st->prim_if))
            text(C_X0 + 18 + text_w(2.4f, ic->name) + 12, y + 18, 1.4f, UN_ORANGE, "PRIMARY");
        int cy = y + 48;
        text(C_X0 + 18, cy, 2.0f, UN_DIM, ic->ip[0] ? ic->ip : "-"); cy += 28;
        if (has6){ snprintf(b, sizeof b, "%s", ic->ip6); trunc_fit(b, 1.5f, W - 64);
                   text(C_X0 + 18, cy, 1.5f, UN_DIM, b); cy += 22; }
        fmt_rate(r1, sizeof r1, ic->rx_kbs); snprintf(b, sizeof b, "DN  %s", r1);
        text(C_X0 + 18, cy, 2.0f, UN_TEXT, b); cy += 28;
        fmt_rate(r2, sizeof r2, ic->tx_kbs); snprintf(b, sizeof b, "UP  %s", r2);
        text(C_X0 + 18, cy, 2.0f, UN_TEXT, b); cy += 30;
        fmt_bytes(r1, sizeof r1, ic->rx_tot); fmt_bytes(r2, sizeof r2, ic->tx_tot);
        snprintf(b, sizeof b, "RX %s   TX %s", r1, r2); trunc_fit(b, 1.6f, W - 62);
        text(C_X0 + 18, cy, 1.6f, UN_DIM, b);
        y += ch + C_GAP;
    }
    return y - y0;
}

#endif /* PANEL_MOD_IFACES_H */
