/*
 * plugin/src/panel/modules/mod_ups.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "ups" module — apcupsd UPS status: battery charge (bar, low = bad), on-line /
 * on-battery state, runtime remaining and load. Reads read_ups() (stats_extra.h);
 * when apcupsd isn't running it shows a dim "No UPS" line. Part of panel_dash.
 */
#ifndef PANEL_MOD_UPS_H
#define PANEL_MOD_UPS_H

static int mod_ups(int y, stats_t *st, int variant){
    (void)variant;
    if (!st->ups_present)
        return value_card(y, 84, "UPS", "No UPS", UN_DIM);

    double ch = st->ups_charge; if (ch < 0) ch = 0; if (ch > 100) ch = 100;
    int hgt = 118;
    card(y, gy(hgt), "UPS");

    /* charge % (left) + on-line / on-battery status (right) */
    char v[16]; snprintf(v, sizeof v, "%.0f%%", ch);
    text(C_X0, y + gy(44), C_VAL, UN_TEXT, v);
    if (st->ups_status[0]){
        int online = strstr(st->ups_status, "ONLINE") != NULL;
        char s[16]; snprintf(s, sizeof s, "%s", st->ups_status);
        text(C_R - text_w(C_SUB, s), y + gy(50), C_SUB, col_state(online), s);
    }

    /* charge bar — battery, so low is bad (col_charge), not the load palette */
    bar(C_X0, y + gy(76), C_W, gy(20), ch, col_charge(ch));

    /* runtime + load on their own row, truncated to the card width */
    char d[48];
    snprintf(d, sizeof d, "%.0fm left \xC2\xB7 %.0f%% load", st->ups_runtime, st->ups_load);
    trunc_fit(d, C_SUB, C_W);
    text(C_X0, y + gy(102), C_SUB, UN_DIM, d);

    return gy(hgt) + gy(C_GAP);
}

#endif /* PANEL_MOD_UPS_H */
