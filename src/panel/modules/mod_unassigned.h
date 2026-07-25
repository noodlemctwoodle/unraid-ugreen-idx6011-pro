/*
 * plugin/src/panel/modules/mod_unassigned.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "unassigned" module — Unassigned devices. Reads read_unassigned() (stats_extra.h) and degrades to a
 * dim idle line when its source is absent. Part of panel_dash: #included in order.
 */
#ifndef PANEL_MOD_UNASSIGNED_H
#define PANEL_MOD_UNASSIGNED_H

static int ud_row(int y, const char *name, double used_gb, double tot_gb, double pct, int style){
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    uint32_t pc = col_load(pct);
    char right[16]; snprintf(right, sizeof right, "%.0f%%", pct);

    if (style == 1){                                     /* compact — one row: dot + name + fill% */
        int ch = 44;
        card(y, gy(ch), NULL);
        item_head(y, pc, name, 1.9f, right, 1.6f, pc);
        return gy(ch) + gy(C_GAP);
    }

    /* card (default) — header row + used/total line + usage bar */
    int ch = 100;
    card(y, gy(ch), NULL);
    item_head(y, pc, name, 2.1f, right, 1.8f, pc);
    char u[32], t[32], b[72];
    fmt_bytes(u, sizeof u, (unsigned long long)(used_gb * 1e9));
    fmt_bytes(t, sizeof t, (unsigned long long)(tot_gb  * 1e9));
    snprintf(b, sizeof b, "%s / %s", u, t);
    trunc_fit(b, 1.6f, C_W);
    text(C_X0, y + gy(44), 1.6f, UN_DIM, b);
    bar(C_X0, y + gy(70), C_W, gy(14), pct, pc);
    return gy(ch) + gy(C_GAP);
}

static int mod_unassigned(int y, stats_t *st, int variant){
    if (st->n_ud == 0)
        return value_card(y, 84, "UNASSIGNED", "No unassigned devices", UN_DIM);
    int y0 = y;
    for (int i = 0; i < st->n_ud; i++)
        y += ud_row(y, st->ud[i].name, st->ud[i].used_gb, st->ud[i].tot_gb, st->ud[i].pct, variant);
    return y - y0;
}

#endif /* PANEL_MOD_UNASSIGNED_H */
