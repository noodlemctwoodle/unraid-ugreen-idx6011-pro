/*
 * plugin/src/panel/modules/mod_host.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "host" module — hostname / IP / array status. Variants: card (name + ip +
 * array/resync), hero (large name + ip), compact (name + ip). Composes the card
 * style shim. Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_HOST_H
#define PANEL_MOD_HOST_H

static int mod_host(int y, stats_t *st, int variant){
    (void)variant;
    const char *s = g_item_key;
    char b[128];
    const char *host = st->host[0] ? st->host : "unraid";
    const char *ip   = st->ip[0]   ? st->ip   : "no ip";

    if (!strcmp(s, "compact")){                          /* name + ip only */
        card(y, gy(76), NULL);
        text_bold((W - text_w(2.4f, host)) / 2 - 1, y + gy(12), 2.4f, UN_TEXT, host);
        text_c(y + gy(44), 1.7f, UN_DIM, ip);
        return gy(76) + gy(C_GAP);
    }
    if (!strcmp(s, "hero")){                             /* large name + ip */
        int h = 120;
        card(y, gy(h), NULL);
        text_bold((W - text_w(3.4f, host)) / 2 - 1, y + gy(24), 3.4f, UN_TEXT, host);
        text_c(y + gy(76), 2.2f, UN_DIM, ip);
        return gy(h) + gy(C_GAP);
    }

    /* default card — name + ip + array/resync line */
    card(y, gy(112), NULL);
    text_bold((W - text_w(2.9f, host)) / 2 - 1, y + gy(14), 2.9f, UN_TEXT, host);
    text_c(y + gy(52), 2.3f, UN_DIM, ip);
    if (st->arr[0]){
        if (st->resync > 0){
            double pct = 100.0 * (double)st->resync_pos / (double)st->resync;
            snprintf(b, sizeof b, "%s %.1f%%", st->resync_act[0] ? st->resync_act : "sync", pct);
            text_c(y + gy(82), 1.8f, UN_WARN, b);
        } else {
            snprintf(b, sizeof b, "array: %s", st->arr);
            text_c(y + gy(82), 1.8f, strcmp(st->arr, "STARTED") ? UN_WARN : UN_OK, b);
        }
    }
    return gy(124);
}

#endif /* PANEL_MOD_HOST_H */
