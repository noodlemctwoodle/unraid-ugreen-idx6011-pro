/*
 * plugin/src/panel/modules/mod_temps.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "temps" module — Temperatures. Reads read_temps() (stats_extra.h).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_TEMPS_H
#define PANEL_MOD_TEMPS_H

/* "temps" — every labelled thermal zone as a coloured name + NNC row. Variants:
 * 0=card, 1=compact (tighter rows). CPU/Board -> col_temp, NVMe -> col_disktemp. */
static int mod_temps(int y, stats_t *st, int variant){
    if (st->n_tz == 0)                                          /* no sensors -> idle card */
        return value_card(y, 84, "TEMPERATURES", "No sensors", UN_DIM);
    int compact = (variant == 1);
    int rowh = compact ? 30 : 40;
    int ch = 40 + st->n_tz * rowh;
    card(y, gy(ch), "TEMPERATURES");
    int ry = y + gy(38);
    for (int i = 0; i < st->n_tz; i++){
        int t = st->tz[i].temp;
        uint32_t col = !strncmp(st->tz[i].name, "NVMe", 4) ? col_disktemp(t) : col_temp(t);
        char r[16]; snprintf(r, sizeof r, "%dC", t);
        item_head(ry, col, st->tz[i].name,
                  compact ? 1.8f : 2.0f, r, compact ? 1.6f : 1.8f, col);
        ry += gy(rowh);
    }
    return gy(ch) + gy(C_GAP);
}

#endif /* PANEL_MOD_TEMPS_H */
