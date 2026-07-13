/*
 * plugin/src/panel/modules/mod_array.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "array" module — Unraid array status. Variants: card (state + parity/rebuild
 * progress + protection health), badge (state in a colour-coded pill), hero
 * (large state + protection line), compact (state only). Data from var.ini via
 * read_misc() (stats_array.h). Composes the card style shim.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_ARRAY_H
#define PANEL_MOD_ARRAY_H

/* short protection line + its colour, shared by the badge/hero variants */
static const char *array_protection(stats_t *st, char *buf, size_t n, uint32_t *col){
    if (st->md_bad > 0){ snprintf(buf, n, "%d disk%s degraded", st->md_bad, st->md_bad > 1 ? "s" : "");
                         *col = UN_BAD; return buf; }
    if (st->sync_errs > 0){ snprintf(buf, n, "%lld sync error%s", st->sync_errs, st->sync_errs > 1 ? "s" : "");
                            *col = UN_WARN; return buf; }
    if (!strcmp(st->arr, "STARTED")){ *col = UN_OK; return "Protected"; }
    *col = UN_WARN; return "Array stopped";
}

static int mod_array(int y, stats_t *st, int variant){
    (void)variant;
    const char *s = g_item_key;
    char b[128];
    if (!st->arr[0]) return value_card(y, 76, "ARRAY", "unknown", UN_DIM);
    int started = !strcmp(st->arr, "STARTED");
    uint32_t sc = started ? UN_OK : UN_WARN;

    if (!strcmp(s, "compact"))                           /* just the state */
        return value_card(y, 60, "ARRAY", st->arr, sc);

    if (!strcmp(s, "badge")){                            /* state in a colour-coded pill */
        int h = 120;
        card(y, gy(h), "ARRAY");
        if (st->mover) card_tag(y, "MOVER", UN_ORANGE_M);
        int pw = gy(160), ph = gy(52), px0 = (W - pw) / 2, py = y + gy(40);
        rect(px0, py, pw, ph, sc, 255);
        text(W / 2 - text_w(2.6f, st->arr) / 2, py + gy(12), 2.6f, UN_BLACK, st->arr);
        return gy(h) + gy(C_GAP);
    }

    if (!strcmp(s, "hero")){                             /* large state + protection */
        int h = 120;
        card(y, gy(h), "ARRAY");
        if (st->mover) card_tag(y, "MOVER", UN_ORANGE_M);
        text_c(y + gy(34), 4.2f, sc, st->arr);
        uint32_t pc; const char *prot = array_protection(st, b, sizeof b, &pc);
        text_c(y + gy(90), 1.7f, pc, prot);
        return gy(h) + gy(C_GAP);
    }

    /* default card — state + parity/rebuild progress + health */
    int h = 118;
    card(y, gy(h), "ARRAY");
    if (st->mover) card_tag(y, "MOVER", UN_ORANGE_M);
    text(C_X0, y + gy(40), 3.0f, sc, st->arr);
    if (st->resync > 0){
        double pct = st->resync ? 100.0 * (double)st->resync_pos / (double)st->resync : 0;
        bar(C_X0, y + gy(82), C_W, gy(12), pct, UN_ORANGE_M);
        snprintf(b, sizeof b, "%s  %.1f%%  %.0f MB/s",
                 st->resync_act[0] ? st->resync_act : "sync", pct, st->resync_mbs);
        trunc_fit(b, 1.5f, C_W);
        text(C_X0, y + gy(98), 1.5f, UN_DIM, b);
    } else if (st->md_bad > 0){
        snprintf(b, sizeof b, "%d disk%s degraded", st->md_bad, st->md_bad > 1 ? "s" : "");
        text(C_X0, y + gy(84), 1.8f, UN_BAD, b);
    } else if (st->sync_errs > 0){
        snprintf(b, sizeof b, "%lld sync error%s", st->sync_errs, st->sync_errs > 1 ? "s" : "");
        text(C_X0, y + gy(84), 1.8f, UN_WARN, b);
    } else if (started){
        text(C_X0, y + gy(84), 1.8f, UN_OK, "Protected");
    }
    return gy(h) + gy(C_GAP);
}

#endif /* PANEL_MOD_ARRAY_H */
