/*
 * plugin/src/panel/pages/network.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * NETWORK page — primary + per-interface cards
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_NETWORK_H
#define PANEL_PAGE_NETWORK_H

static void page_network(stats_t *st){
    char b[64], r1[32], r2[32];
    int y = body_top;

    if (st->n_ifaces == 0){
        card(y, 90, "NETWORK");
        text(22, y + 44, 2.0f, UN_DIM, "no interfaces");
        page_end = y + 100;
        return;
    }

    /* one card per interface: header, IPs, DN/UP on their own lines (fit any
     * speed), then since-boot totals. Vertical space is fine — the page scrolls. */
    for (int i = 0; i < st->n_ifaces; i++){
        iface_t *ic = &st->ifc[i];
        int prim = !strcmp(ic->name, st->prim_if);
        int has6 = ic->ip6[0] != 0;
        int ch = 172 + (has6 ? 22 : 0);
        card(y, ch, NULL);
        int cy = y + 14;

        /* header: status dot + name (+ PRIMARY) + link state */
        rect(22, cy + 6, 12, 12, ic->up ? UN_OK : 0x555555, 255);
        text(42, cy, 2.4f, UN_TEXT, ic->name);
        if (prim)
            text(42 + text_w(2.4f, ic->name) + 12, cy + 6, 1.4f, UN_ORANGE, "PRIMARY");
        text(W - 22 - text_w(1.6f, ic->up ? "LINK" : "DOWN"), cy + 6, 1.6f,
             ic->up ? UN_OK : UN_DIM, ic->up ? "LINK" : "DOWN");
        cy += 34;

        /* IPv4 */
        text(42, cy, 2.0f, UN_DIM, ic->ip[0] ? ic->ip : "-");
        cy += 28;

        /* IPv6 (optional) */
        if (has6){
            snprintf(b, sizeof b, "%s", ic->ip6);
            trunc_fit(b, 1.5f, W - 64);
            text(42, cy, 1.5f, UN_DIM, b);
            cy += 22;
        }

        /* live rates — separate lines, left-aligned */
        fmt_rate(r1, sizeof r1, ic->rx_kbs);
        snprintf(b, sizeof b, "DN  %s", r1);
        text(42, cy, 2.0f, UN_TEXT, b);
        cy += 28;
        fmt_rate(r2, sizeof r2, ic->tx_kbs);
        snprintf(b, sizeof b, "UP  %s", r2);
        text(42, cy, 2.0f, UN_TEXT, b);
        cy += 30;

        /* totals since boot */
        fmt_bytes(r1, sizeof r1, ic->rx_tot);
        fmt_bytes(r2, sizeof r2, ic->tx_tot);
        snprintf(b, sizeof b, "RX %s   TX %s", r1, r2);
        trunc_fit(b, 1.6f, W - 62);
        text(42, cy, 1.6f, UN_DIM, b);

        y += ch + 12;
    }
    page_end = y;
}


#endif /* PANEL_PAGE_NETWORK_H */
