/*
 * plugin/src/panel/modules/mod_ifaces.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Network interface cards. "ifaces" draws one card per interface (variants:
 * full, compact); "iface" (indexed, e.g. iface:1) draws a single interface so
 * they can be split and reordered. Share iface_card() + the shim + col_dot/state.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_IFACES_H
#define PANEL_MOD_IFACES_H

/* one interface card. compact = name + DN/UP only; full = + IPs + totals. */
static int iface_card(int y, iface_t *ic, int prim, int compact){
    char b[64], r1[32], r2[32];
    if (compact){                                        /* name + link + DN/UP (no IPs/totals) */
        int ch = 108;
        card(y, ch, NULL);
        item_head(y, col_dot(ic->up), ic->name, 2.3f,
                  ic->up ? "LINK" : "DOWN", 1.5f, col_state(ic->up));
        fmt_rate(r1, sizeof r1, ic->rx_kbs); snprintf(b, sizeof b, "DN  %s", r1);
        text(C_X0 + 18, y + 44, 1.9f, UN_TEXT, b);
        fmt_rate(r2, sizeof r2, ic->tx_kbs); snprintf(b, sizeof b, "UP  %s", r2);
        text(C_X0 + 18, y + 72, 1.9f, UN_TEXT, b);
        return ch + C_GAP;
    }
    int has6 = ic->ip6[0] != 0;
    int ch = 172 + (has6 ? 22 : 0);
    card(y, ch, NULL);
    item_head(y, col_dot(ic->up), ic->name, 2.4f, ic->up ? "LINK" : "DOWN", 1.6f, col_state(ic->up));
    if (prim)
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
    return ch + C_GAP;
}

static int mod_ifaces(int y, stats_t *st, int variant){      /* all interfaces */
    if (st->n_ifaces == 0) return value_card(y, 90, "NETWORK", "no interfaces", UN_DIM);
    int y0 = y, compact = (variant == 1);
    for (int i = 0; i < st->n_ifaces; i++)
        y += iface_card(y, &st->ifc[i], !strcmp(st->ifc[i].name, st->prim_if), compact);
    return y - y0;
}

static int mod_iface(int y, stats_t *st, int variant){       /* one interface by index */
    if (variant < 0 || variant >= st->n_ifaces)
        return value_card(y, 76, "INTERFACE", "not present", UN_DIM);
    return iface_card(y, &st->ifc[variant], !strcmp(st->ifc[variant].name, st->prim_if), 0);
}

#endif /* PANEL_MOD_IFACES_H */
