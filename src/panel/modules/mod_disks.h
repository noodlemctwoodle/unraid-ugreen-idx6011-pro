/*
 * plugin/src/panel/modules/mod_disks.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "disks" module — one card per array/pool disk (health dot, name, temp/SLEEP,
 * type/errors, usage bar). Draws all disks and returns their combined height.
 * Composes the card style shim + semantic colours (col_health / col_disktemp /
 * col_load). Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_DISKS_H
#define PANEL_MOD_DISKS_H

static int mod_disks(int y, stats_t *st, int variant){
    (void)variant;
    char b[128];
    if (st->n_disks == 0){
        card(y, 100, "DISKS");
        text(C_X0, y + 42, 2.0f, UN_DIM, "no disk info");
        text(C_X0, y + 66, 1.8f, UN_DIM, "(array stopped?)");
        return 100 + C_GAP;
    }
    int y0 = y;
    for (int i = 0; i < st->n_disks; i++){
        disk_t *d = &st->disks[i];
        card(y, 116, NULL);
        uint32_t dot = d->health ? col_health(d->health) : (d->spun ? 0x2a6b38 : UN_OK);
        char tmp[16];
        if (d->spun)           snprintf(tmp, sizeof tmp, "SLEEP");
        else if (d->temp >= 0) snprintf(tmp, sizeof tmp, "%dC", d->temp);
        else                   snprintf(tmp, sizeof tmp, "--");
        item_head(y, dot, d->name, 2.3f, tmp, 2.2f, d->spun ? UN_DIM : col_disktemp(d->temp));
        if (d->errors > 0){
            snprintf(b, sizeof b, "%s  %lld errs", d->type, d->errors);
            text(C_X0, y + 42, 1.7f, UN_BAD, b);
        } else {
            text(C_X0, y + 42, 1.7f, UN_DIM, d->type[0] ? d->type : "-");
        }
        if (d->fs_size > 0){
            double pct = 100.0 * (double)d->fs_used / (double)d->fs_size;
            bar(C_X0, y + 68, W - 92, 14, pct, col_load(pct));
            snprintf(b, sizeof b, "%.0f%%", pct);
            text(C_R - text_w(1.8f, b), y + 67, 1.8f, UN_DIM, b);
            char u[32], t[32];
            fmt_bytes(u, sizeof u, d->fs_used * 1024ULL);
            fmt_bytes(t, sizeof t, d->fs_size * 1024ULL);
            snprintf(b, sizeof b, "%s / %s", u, t);
            text(C_X0, y + 92, 1.6f, UN_DIM, b);
        } else if (d->size_kib > 0){
            char sz[32]; fmt_bytes(sz, sizeof sz, d->size_kib * 1024ULL);
            snprintf(b, sizeof b, "%s  (no fs)", sz);
            text(C_X0, y + 74, 1.7f, UN_DIM, b);
        }
        y += 116 + C_GAP;
    }
    return y - y0;
}

#endif /* PANEL_MOD_DISKS_H */
