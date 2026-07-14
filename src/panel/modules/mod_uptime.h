/*
 * plugin/src/panel/modules/mod_uptime.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "uptime" module — system uptime. Variants: card (up Xd Yh Zm), big (the
 * duration large), hero (big days count). Composes the card style shim.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_UPTIME_H
#define PANEL_MOD_UPTIME_H

static int mod_uptime(int y, stats_t *st, int variant){
    (void)variant;
    const char *s = g_item_key;
    char b[64];
    fmt_up(b, sizeof b, st->up_s);                       /* "up Xd Yh Zm" */
    if (!strcmp(s, "big"))                               /* the duration large */
        return big_value_card(y, "UPTIME", strncmp(b, "up ", 3) == 0 ? b + 3 : b, UN_TEXT);
    if (!strcmp(s, "hero")){                             /* big days count */
        int h = 120; long d = st->up_s / 86400;
        card(y, gy(h), "UPTIME");
        snprintf(b, sizeof b, "%ld", d);
        text_c(y + gy(34), 5.6f, UN_TEXT, b);
        text_c(y + gy(96), 1.6f, UN_DIM, d == 1 ? "day up" : "days up");
        return gy(h) + gy(C_GAP);
    }
    return value_card(y, 60, "UPTIME", b, UN_TEXT);
}

#endif /* PANEL_MOD_UPTIME_H */
