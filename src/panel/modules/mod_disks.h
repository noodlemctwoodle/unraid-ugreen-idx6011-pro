/*
 * plugin/src/panel/modules/mod_disks.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Disk cards. "disks" draws one card per array/pool disk; "disk" (indexed, e.g.
 * disk:cache or disk:cache:bar) draws a single disk so individual disks can be
 * placed/reordered/restyled. Both share disk_card() + the shim. Styles: full,
 * compact, bar. Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_DISKS_H
#define PANEL_MOD_DISKS_H

/* one disk card. style: 0=full (usage + used/total), 1=compact (header + bar),
 * 2=bar (name + temp + one usage bar). Returns the advance (height + gap). */
static int disk_card(int y, disk_t *d, int style){
    char b[128];
    uint32_t dot = d->health ? col_health(d->health) : (d->spun ? 0x2a6b38 : UN_OK);
    char tmp[16];
    if (d->spun)           snprintf(tmp, sizeof tmp, "SLEEP");
    else if (d->temp >= 0) snprintf(tmp, sizeof tmp, "%dC", d->temp);
    else                   snprintf(tmp, sizeof tmp, "--");
    uint32_t tcol = d->spun ? UN_DIM : col_disktemp(d->temp);
    double pct = d->fs_size > 0 ? 100.0 * (double)d->fs_used / (double)d->fs_size : -1;

    if (style == 2){                                     /* bar — name + temp + usage bar */
        char v[32];
        if (pct >= 0) snprintf(v, sizeof v, "%s  %.0f%%", tmp, pct);
        else          snprintf(v, sizeof v, "%s", tmp);
        return bar_card(y, 62, d->name, v, pct < 0 ? 0 : pct, col_load(pct < 0 ? 0 : pct));
    }
    if (style == 1){                                     /* compact — header + usage bar */
        int ch = 82;
        card(y, gy(ch), NULL);
        item_head(y, dot, d->name, 2.1f, tmp, 1.8f, tcol);
        if (pct >= 0){
            bar(C_X0, y + gy(46), W - 92, gy(12), pct, col_load(pct));
            snprintf(b, sizeof b, "%.0f%%", pct);
            text(C_R - text_w(1.7f, b), y + gy(45), 1.7f, UN_DIM, b);
        } else {
            text(C_X0, y + gy(48), 1.6f, UN_DIM, d->type[0] ? d->type : "-");
        }
        return gy(ch) + gy(C_GAP);
    }

    /* full (default) — the detailed card */
    char sm[48] = ""; long long bad = 0;   /* SMART summary: age · life · bad sectors */
    {
        char *p = sm; size_t n = sizeof sm; int any = 0;
        if (d->poh >= 0){ int w = snprintf(p, n, "%.1fy", d->poh / 8760.0); if (w > 0){ p += w; n -= (size_t)w; } any = 1; }
        if (d->wear >= 0){ int w = snprintf(p, n, "%s%d%% life", any ? " \xc2\xb7 " : "", d->wear); if (w > 0){ p += w; n -= (size_t)w; } any = 1; }
        bad = (d->realloc > 0 ? d->realloc : 0) + (d->pending > 0 ? d->pending : 0);
        if (bad > 0) snprintf(p, n, "%s%lld bad", any ? " \xc2\xb7 " : "", bad);
    }
    int ch = sm[0] ? 138 : 116;
    card(y, gy(ch), NULL);
    item_head(y, dot, d->name, 2.3f, tmp, 2.2f, tcol);
    if (d->errors > 0){
        snprintf(b, sizeof b, "%s  %lld errs", d->type, d->errors);
        text(C_X0, y + gy(42), 1.7f, UN_BAD, b);
    } else {
        text(C_X0, y + gy(42), 1.7f, UN_DIM, d->type[0] ? d->type : "-");
    }
    if (pct >= 0){
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
    if (sm[0]) text(C_X0, y + gy(ch - 22), 1.5f, bad > 0 ? UN_WARN : UN_DIM, sm);
    return gy(ch) + gy(C_GAP);
}

static int mod_disks(int y, stats_t *st, int variant){       /* all disks, styled by variant */
    if (st->n_disks == 0){
        card(y, gy(100), "DISKS");
        text(C_X0, y + gy(42), 2.0f, UN_DIM, "no disk info");
        text(C_X0, y + gy(66), 1.8f, UN_DIM, "(array stopped?)");
        return gy(100) + gy(C_GAP);
    }
    int y0 = y;
    for (int i = 0; i < st->n_disks; i++) y += disk_card(y, &st->disks[i], variant);
    return y - y0;
}

static int mod_disk(int y, stats_t *st, int variant){        /* one disk, "name" or "name:style" */
    (void)variant;
    char key[64]; snprintf(key, sizeof key, "%s", g_item_key);
    char *style = strchr(key, ':'); if (style) *style++ = 0;
    int sidx = 0;
    if (style){ if (!strcmp(style, "compact")) sidx = 1; else if (!strcmp(style, "bar")) sidx = 2; }
    int i = -1;
    if (key[0]){
        for (int k = 0; k < st->n_disks; k++) if (!strcmp(st->disks[k].name, key)){ i = k; break; }
        if (i < 0 && key[0] >= '0' && key[0] <= '9'){ int n = atoi(key); if (n >= 0 && n < st->n_disks) i = n; }
    } else if (st->n_disks > 0) i = 0;
    if (i < 0) return value_card(y, 76, "DISK", "not present", UN_DIM);
    return disk_card(y, &st->disks[i], sidx);
}

#endif /* PANEL_MOD_DISKS_H */
