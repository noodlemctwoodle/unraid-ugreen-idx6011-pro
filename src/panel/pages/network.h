/*
 * plugin/src/panel/pages/network.h
 *
 * Created by Toby G on 12/07/2026.
 *
 * NETWORK page — primary + per-interface cards
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_NETWORK_H
#define PANEL_PAGE_NETWORK_H

static void page_network(stats_t *st){
    char b[128], r1[32], r2[32];
    int y = body_top;

    iface_t *prim = NULL;
    for (int i = 0; i < st->n_ifaces; i++)
        if (!strcmp(st->ifc[i].name, st->prim_if)){ prim = &st->ifc[i]; break; }

    card(y, 158, "PRIMARY");
    text(22, y + 32, 2.7f, UN_TEXT, st->prim_if);
    text(22, y + 64, 2.0f, UN_DIM, st->ip[0] ? st->ip : "no ip");
    fmt_rate(r1, sizeof r1, st->rx_kbs); fmt_rate(r2, sizeof r2, st->tx_kbs);
    snprintf(b, sizeof b, "DN  %s", r1);
    text(22, y + 94, 2.4f, UN_TEXT, b);
    snprintf(b, sizeof b, "UP  %s", r2);
    text(22, y + 124, 2.4f, UN_TEXT, b);
    y += 170;

    card(y, 112, "SINCE BOOT");
    if (prim){
        fmt_bytes(r1, sizeof r1, prim->rx_tot);
        snprintf(b, sizeof b, "RX  %s", r1);
        text(22, y + 36, 2.3f, UN_TEXT, b);
        fmt_bytes(r2, sizeof r2, prim->tx_tot);
        snprintf(b, sizeof b, "TX  %s", r2);
        text(22, y + 70, 2.3f, UN_TEXT, b);
    } else {
        text(22, y + 44, 2.0f, UN_DIM, "no data");
    }
    y += 124;

    for (int i = 0; i < st->n_ifaces; i++){
        iface_t *ic = &st->ifc[i];
        char r1b[32], r2b[32];
        int has6 = ic->ip6[0] != 0;
        int ch = has6 ? 122 : 104;
        card(y, ch, NULL);
        rect(22, y + 16, 10, 10, ic->up ? UN_OK : 0x555555, 255);
        text(40, y + 12, 2.2f, UN_TEXT, ic->name);
        fmt_rate(r1b, sizeof r1b, ic->rx_kbs); fmt_rate(r2b, sizeof r2b, ic->tx_kbs);
        snprintf(b, sizeof b, "%s / %s", r1b, r2b);
        text(W - 22 - text_w(1.6f, b), y + 16, 1.6f, UN_DIM, b);
        text(40, y + 44, 1.8f, UN_DIM, ic->ip[0] ? ic->ip : "-");
        int ry = y + 72;
        if (has6){
            snprintf(b, sizeof b, "v6 %s", ic->ip6);
            trunc_fit(b, 1.5f, W - 62);
            text(40, ry, 1.5f, UN_DIM, b);
            ry += 20;
        }
        fmt_bytes(r1b, sizeof r1b, ic->rx_tot); fmt_bytes(r2b, sizeof r2b, ic->tx_tot);
        snprintf(b, sizeof b, "RX %s   TX %s", r1b, r2b);
        text(40, ry, 1.8f, UN_DIM, b);
        y += ch + 12;
    }
    page_end = y;
}


#endif /* PANEL_PAGE_NETWORK_H */
