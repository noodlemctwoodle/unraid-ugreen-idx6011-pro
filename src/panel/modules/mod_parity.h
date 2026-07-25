/*
 * plugin/src/panel/modules/mod_parity.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "parity" module — Unraid array-operation status: a live parity check / sync /
 * rebuild / clear (progress %, rate, ETA), a disabled-disk warning, a sync-error
 * count, mover activity, or a healthy idle line. Reads the mdResync* / md_bad /
 * sync_errs / mover fields (read_misc, stats_array.h). Part of panel_dash.
 */
#ifndef PANEL_MOD_PARITY_H
#define PANEL_MOD_PARITY_H

static int mod_parity(int y, stats_t *st, int variant){
    (void)variant;
    unsigned long long total = st->resync_size ? st->resync_size : st->resync;
    int active = total > 0 && (st->resync > 0 ||
                 (st->resync_pos > 0 && st->resync_pos < total));
    if (active){
        double pct = 100.0 * (double)st->resync_pos / (double)total;
        char title[24];
        snprintf(title, sizeof title, "%s", st->resync_act[0] ? st->resync_act : "ARRAY");
        for (char *c = title; *c; c++) *c = (char)toupper((unsigned char)*c);
        char det[64];
        if (st->resync_mbs > 0.01){
            unsigned long long rem = st->resync_pos < total ? total - st->resync_pos : 0;
            long secs = (long)((double)rem / 1024.0 / st->resync_mbs);   /* rem KiB, mbs MiB/s */
            snprintf(det, sizeof det, "%.0f MB/s \xC2\xB7 ETA %ld:%02ld:%02ld",
                     st->resync_mbs, secs / 3600, secs % 3600 / 60, secs % 60);
        } else {
            snprintf(det, sizeof det, "%s", st->resync_act[0] ? st->resync_act : "in progress");
        }
        return progress_card(y, title, pct, det);
    }
    if (st->md_bad > 0)
        return value_card(y, 84, "PARITY", "Disk disabled \xE2\x80\x94 check array", col_health(2));
    if (st->sync_errs > 0){
        char v[40];
        snprintf(v, sizeof v, "%lld sync error%s", st->sync_errs, st->sync_errs == 1 ? "" : "s");
        return value_card(y, 84, "PARITY", v, col_health(1));
    }
    if (st->mover)
        return value_card(y, 84, "PARITY", "Mover running", UN_DIM);
    return value_card(y, 84, "PARITY", "Array healthy", col_health(0));
}

#endif /* PANEL_MOD_PARITY_H */
