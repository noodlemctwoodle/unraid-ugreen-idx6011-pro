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

static int mod_net(int y, stats_t *st, int variant){     /* primary-interface rates */
    (void)variant;
    const char *s = g_item_key;                          /* style name */
    char b[128], r[32];
    if (!strcmp(s, "graph") || !strcmp(s, "area")){      /* throughput history (auto-scaled) */
        char v[48]; fmt_rate(v, sizeof v, st->rx_kbs + st->tx_kbs);
        return graph_card(y, "NETWORK", v, h_net, UN_ORANGE_M, s[0] == 'a', 0);
    }
    if (!strcmp(s, "compact")){                          /* compact — DN left, UP right, one line */
        card(y, gy(72), "NETWORK");
        text(W - 22 - text_w(1.5f, st->prim_if), y + gy(8), 1.5f, UN_DIM, st->prim_if);
        fmt_rate(r, sizeof r, st->rx_kbs); snprintf(b, sizeof b, "DN %s", r);
        text(22, y + gy(40), 1.9f, UN_TEXT, b);
        fmt_rate(r, sizeof r, st->tx_kbs); snprintf(b, sizeof b, "UP %s", r);
        text(W - 22 - text_w(1.9f, b), y + gy(40), 1.9f, UN_TEXT, b);
        return gy(72) + gy(C_GAP);
    }
    int big = !strcmp(s, "big");                         /* big vs default rows */
    int h = big ? 172 : 128; float sc = big ? 3.2f : 2.5f;
    int y1 = big ? 54 : 44, y2 = big ? 110 : 86;
    card(y, gy(h), "NETWORK");
    text(W - 22 - text_w(1.7f, st->prim_if), y + gy(10), 1.7f, UN_DIM, st->prim_if);
    fmt_rate(b + 4, sizeof(b) - 4, st->rx_kbs); memcpy(b, "DN  ", 4);
    text(22, y + gy(y1), sc, UN_TEXT, b);
    fmt_rate(b + 4, sizeof(b) - 4, st->tx_kbs); memcpy(b, "UP  ", 4);
    text(22, y + gy(y2), sc, UN_TEXT, b);
    return gy(h) + gy(C_GAP);
}

#endif /* PANEL_MOD_NET_H */
