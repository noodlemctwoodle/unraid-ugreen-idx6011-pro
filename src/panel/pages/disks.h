/*
 * plugin/src/panel/pages/disks.h
 *
 * Created by Toby G on 12/07/2026.
 *
 * DISKS page — per-disk health, temp, fs usage
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_DISKS_H
#define PANEL_PAGE_DISKS_H

static void page_disks(stats_t *st){
    char b[128];
    int y = body_top;

    if (st->n_disks == 0){
        card(y, 100, "DISKS");
        text(22, y + 42, 2.0f, UN_DIM, "no disk info");
        text(22, y + 66, 1.8f, UN_DIM, "(array stopped?)");
        page_end = y + 110;
        return;
    }
    int show = st->n_disks;
    for (int i = 0; i < show; i++){
        disk_t *d = &st->disks[i];
        card(y, 116, NULL);
        uint32_t dot = d->health == 2 ? UN_BAD : d->health == 1 ? UN_WARN
                     : d->spun ? 0x2a6b38 : UN_OK;
        rect(22, y + 16, 10, 10, dot, 255);
        snprintf(b, sizeof b, "%s", d->name);
        trunc_fit(b, 2.3f, 150);
        text(40, y + 12, 2.3f, UN_TEXT, b);
        if (d->spun)            snprintf(b, sizeof b, "SLEEP");
        else if (d->temp >= 0)  snprintf(b, sizeof b, "%dC", d->temp);
        else                    snprintf(b, sizeof b, "--");
        uint32_t tc = d->spun ? UN_DIM : d->temp > 55 ? UN_BAD : d->temp > 45 ? UN_WARN : UN_DIM;
        text(W - 22 - text_w(2.2f, b), y + 13, 2.2f, tc, b);
        if (d->errors > 0){
            snprintf(b, sizeof b, "%s  %lld errs", d->type, d->errors);
            text(22, y + 42, 1.7f, UN_BAD, b);
        } else {
            text(22, y + 42, 1.7f, UN_DIM, d->type[0] ? d->type : "-");
        }
        if (d->fs_size > 0){
            double pct = 100.0 * (double)d->fs_used / (double)d->fs_size;
            bar(22, y + 68, W - 92, 14, pct, level_col(pct));
            snprintf(b, sizeof b, "%.0f%%", pct);
            text(W - 22 - text_w(1.8f, b), y + 67, 1.8f, UN_DIM, b);
            fmt_bytes(b, sizeof b, d->fs_used * 1024ULL);
            char tot[32]; fmt_bytes(tot, sizeof tot, d->fs_size * 1024ULL);
            char l[64]; snprintf(l, sizeof l, "%s / %s", b, tot);
            text(22, y + 92, 1.6f, UN_DIM, l);
        } else if (d->size_kib > 0){
            fmt_bytes(b, sizeof b, d->size_kib * 1024ULL);
            char l[64]; snprintf(l, sizeof l, "%s  (no fs)", b);
            text(22, y + 74, 1.7f, UN_DIM, l);
        }
        y += 126;
    }
    page_end = y;
}


#endif /* PANEL_PAGE_DISKS_H */
