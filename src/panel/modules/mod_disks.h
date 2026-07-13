/*
 * plugin/src/panel/modules/mod_disks.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Disk cards. "disks" draws one card per array/pool disk; "disk" (indexed, e.g.
 * disk:0) draws a single disk so individual disks can be placed/reordered. Both
 * share disk_card() and the card style shim + semantic colours.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_DISKS_H
#define PANEL_MOD_DISKS_H

/* one disk card; returns the advance (height + gap) */
static int disk_card(int y, disk_t *d){
    char b[128];
    card(y, gy(116), NULL);
    uint32_t dot = d->health ? col_health(d->health) : (d->spun ? 0x2a6b38 : UN_OK);
    char tmp[16];
    if (d->spun)           snprintf(tmp, sizeof tmp, "SLEEP");
    else if (d->temp >= 0) snprintf(tmp, sizeof tmp, "%dC", d->temp);
    else                   snprintf(tmp, sizeof tmp, "--");
    item_head(y, dot, d->name, 2.3f, tmp, 2.2f, d->spun ? UN_DIM : col_disktemp(d->temp));
    if (d->errors > 0){
        snprintf(b, sizeof b, "%s  %lld errs", d->type, d->errors);
        text(C_X0, y + gy(42), 1.7f, UN_BAD, b);
    } else {
        text(C_X0, y + gy(42), 1.7f, UN_DIM, d->type[0] ? d->type : "-");
    }
    if (d->fs_size > 0){
        double pct = 100.0 * (double)d->fs_used / (double)d->fs_size;
        bar(C_X0, y + gy(68), W - 92, gy(14), pct, col_load(pct));
        snprintf(b, sizeof b, "%.0f%%", pct);
        text(C_R - text_w(1.8f, b), y + gy(67), 1.8f, UN_DIM, b);
        char u[32], t[32];
        fmt_bytes(u, sizeof u, d->fs_used * 1024ULL);
        fmt_bytes(t, sizeof t, d->fs_size * 1024ULL);
        snprintf(b, sizeof b, "%s / %s", u, t);
        text(C_X0, y + gy(92), 1.6f, UN_DIM, b);
    } else if (d->size_kib > 0){
        char sz[32]; fmt_bytes(sz, sizeof sz, d->size_kib * 1024ULL);
        snprintf(b, sizeof b, "%s  (no fs)", sz);
        text(C_X0, y + gy(74), 1.7f, UN_DIM, b);
    }
    return gy(116) + gy(C_GAP);
}

static int mod_disks(int y, stats_t *st, int variant){       /* all disks */
    (void)variant;
    if (st->n_disks == 0){
        card(y, gy(100), "DISKS");
        text(C_X0, y + gy(42), 2.0f, UN_DIM, "no disk info");
        text(C_X0, y + gy(66), 1.8f, UN_DIM, "(array stopped?)");
        return gy(100) + gy(C_GAP);
    }
    int y0 = y;
    for (int i = 0; i < st->n_disks; i++) y += disk_card(y, &st->disks[i]);
    return y - y0;
}

static int mod_disk(int y, stats_t *st, int variant){        /* one disk, picked by name */
    (void)variant;
    int i = -1;
    if (g_item_key[0]){
        for (int k = 0; k < st->n_disks; k++) if (!strcmp(st->disks[k].name, g_item_key)){ i = k; break; }
        if (i < 0 && g_item_key[0] >= '0' && g_item_key[0] <= '9'){ int n = atoi(g_item_key); if (n >= 0 && n < st->n_disks) i = n; }
    } else if (st->n_disks > 0) i = 0;
    if (i < 0) return value_card(y, 76, "DISK", "not present", UN_DIM);
    return disk_card(y, &st->disks[i]);
}

#endif /* PANEL_MOD_DISKS_H */
