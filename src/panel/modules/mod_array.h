/*
 * plugin/src/panel/modules/mod_array.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "array" module — Unraid array status on its own card: state (STARTED/STOPPED),
 * a parity check / rebuild progress bar when one is running, protection health
 * (degraded disks / sync errors / Protected) and a MOVER tag. All read from
 * /var/local/emhttp/var.ini by read_misc() (stats_array.h).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_ARRAY_H
#define PANEL_MOD_ARRAY_H

static int mod_array(int y, stats_t *st, int variant){
    char b[128];
    if (!st->arr[0]) return value_card(y, 76, "ARRAY", "unknown", UN_DIM);
    int started = !strcmp(st->arr, "STARTED");
    uint32_t sc = started ? UN_OK : UN_WARN;

    if (variant == 1)                                    /* compact — just the state */
        return value_card(y, 60, "ARRAY", st->arr, sc);

    int h = 118;
    card(y, gy(h), "ARRAY");
    if (st->mover) card_tag(y, "MOVER", UN_ORANGE_M);
    text(C_X0, y + gy(40), 3.0f, sc, st->arr);           /* STARTED / STOPPED */

    if (st->resync > 0){                                 /* parity check / rebuild in progress */
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
