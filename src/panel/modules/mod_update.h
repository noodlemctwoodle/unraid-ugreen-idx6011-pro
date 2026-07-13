/*
 * plugin/src/panel/modules/mod_update.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "update" module — Unraid OS + plugin update status. Data from read_updates()
 * (/tmp/unraidcheck/result.json + /tmp/plugins/pluginPending). Variants: card
 * (OS + plugin breakdown), badge (big count in a pill), hero (large status),
 * compact (one line). Composes the card style shim. Part of panel_dash.
 */
#ifndef PANEL_MOD_UPDATE_H
#define PANEL_MOD_UPDATE_H

static int mod_update(int y, stats_t *st, int variant){
    (void)variant;
    const char *s = g_item_key;
    char b[64];
    int tot = st->os_update + st->plugin_updates;

    if (!strcmp(s, "compact")){                          /* one-line status */
        if (tot == 0) return value_card(y, 60, "UPDATES", "up to date", UN_OK);
        snprintf(b, sizeof b, "%d available", tot);
        return value_card(y, 60, "UPDATES", b, UN_WARN);
    }

    if (!strcmp(s, "badge")){                             /* big count in a pill */
        int h = 132;
        card(y, gy(h), "UPDATES");
        int pw = gy(96), ph = gy(56), px0 = (W - pw) / 2, py = y + gy(40);
        rect(px0, py, pw, ph, tot ? UN_WARN : UN_OK, 255);
        if (tot){ snprintf(b, sizeof b, "%d", tot);
                  text(W / 2 - text_w(3.4f, b) / 2, py + gy(8), 3.4f, UN_BLACK, b); }
        else      text(W / 2 - text_w(2.6f, "OK") / 2, py + gy(12), 2.6f, UN_BLACK, "OK");
        text_c(py + ph + gy(14), 1.6f, UN_DIM, tot ? (tot > 1 ? "updates available" : "update available") : "up to date");
        return gy(h) + gy(C_GAP);
    }

    if (!strcmp(s, "hero")){                              /* large status word/number */
        int h = 120;
        card(y, gy(h), "UPDATES");
        if (tot){ snprintf(b, sizeof b, "%d", tot); text_c(y + gy(42), 5.4f, UN_WARN, b);
                  text_c(y + gy(96), 1.6f, UN_DIM, tot > 1 ? "updates available" : "update available"); }
        else      text_c(y + gy(56), 3.6f, UN_OK, "Up to date");
        return gy(h) + gy(C_GAP);
    }

    /* default "card" — OS line + plugin line */
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
