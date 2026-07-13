/*
 * plugin/src/panel/modules/mod_update.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "update" module — Unraid OS + plugin update status on a card. Data from
 * read_updates() (/tmp/unraidcheck/result.json + /tmp/plugins/pluginPending).
 * Composes the card style shim. Part of panel_dash: #included in a fixed order.
 */
#ifndef PANEL_MOD_UPDATE_H
#define PANEL_MOD_UPDATE_H

static int mod_update(int y, stats_t *st, int variant){
    char b[64];
    int tot = st->os_update + st->plugin_updates;
    if (variant == 1){                                   /* compact — one-line status */
        if (tot == 0) return value_card(y, 60, "UPDATES", "up to date", UN_OK);
        snprintf(b, sizeof b, "%d available", tot);
        return value_card(y, 60, "UPDATES", b, UN_WARN);
    }
    int h = 96;
    card(y, gy(h), "UPDATES");
    if (tot > 0){ snprintf(b, sizeof b, "%d", tot); card_tag(y, b, UN_WARN); }
    if (st->os_update){ snprintf(b, sizeof b, "Unraid %s", st->os_new_ver[0] ? st->os_new_ver : "update");
                        text(C_X0, y + gy(38), 2.0f, UN_WARN, b); }
    else                text(C_X0, y + gy(38), 2.0f, UN_OK, "Unraid up to date");
    if (st->plugin_updates > 0){ snprintf(b, sizeof b, "%d plugin update%s", st->plugin_updates, st->plugin_updates > 1 ? "s" : "");
                                 text(C_X0, y + gy(68), 1.7f, UN_WARN, b); }
    else                         text(C_X0, y + gy(68), 1.7f, UN_DIM, "Plugins up to date");
    return gy(h) + gy(C_GAP);
}

#endif /* PANEL_MOD_UPDATE_H */
