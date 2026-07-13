/*
 * plugin/src/panel/modules/mod_net.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "net" module — primary-interface down/up rates.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_NET_H
#define PANEL_MOD_NET_H

static int mod_net(int y, stats_t *st, int variant){
    (void)variant;
    char b[128];
    card(y, 128, "NETWORK");
    text(W - 22 - text_w(1.7f, st->prim_if), y + 10, 1.7f, UN_DIM, st->prim_if);
    fmt_rate(b + 4, sizeof(b) - 4, st->rx_kbs); memcpy(b, "DN  ", 4);
    text(22, y + 44, 2.5f, UN_TEXT, b);
    fmt_rate(b + 4, sizeof(b) - 4, st->tx_kbs); memcpy(b, "UP  ", 4);
    text(22, y + 86, 2.5f, UN_TEXT, b);
    return 140;
}

#endif /* PANEL_MOD_NET_H */
