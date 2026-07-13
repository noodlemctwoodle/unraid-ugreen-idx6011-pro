/*
 * plugin/src/panel/modules/mod_uptime.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "uptime" module — system uptime in its own (value) card.
 * Composes the card style shim (cardstyle.h).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_UPTIME_H
#define PANEL_MOD_UPTIME_H

static int mod_uptime(int y, stats_t *st, int variant){
    char b[64];
    fmt_up(b, sizeof b, st->up_s);                       /* "up Xd Yh Zm" */
    if (variant == 1)                                    /* big — drop the "up " prefix */
        return big_value_card(y, "UPTIME", strncmp(b, "up ", 3) == 0 ? b + 3 : b, UN_TEXT);
    return value_card(y, 60, "UPTIME", b, UN_TEXT);
}

#endif /* PANEL_MOD_UPTIME_H */
