/*
 * plugin/src/panel/modules/mod_uptime.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "uptime" module — system uptime in its own card.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_UPTIME_H
#define PANEL_MOD_UPTIME_H

static int mod_uptime(int y, stats_t *st, int variant){
    (void)variant;
    char b[64];
    card(y, 60, "UPTIME");
    fmt_up(b, sizeof b, st->up_s);                       /* "up Xd Yh Zm" */
    text(22, y + 30, 2.6f, UN_TEXT, b);
    return 72;
}

#endif /* PANEL_MOD_UPTIME_H */
