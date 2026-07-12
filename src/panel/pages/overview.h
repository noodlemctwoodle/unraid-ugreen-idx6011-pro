/*
 * plugin/src/panel/pages/overview.h
 *
 * Created by Toby G on 12/07/2026.
 *
 * OVERVIEW page — host, cpu, memory, network, storage summary
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_OVERVIEW_H
#define PANEL_PAGE_OVERVIEW_H

static void page_overview(stats_t *st){
    char b[128];
    int y = body_top;

    card(y, 112, NULL);
    text_bold((W - text_w(2.9f, st->host[0] ? st->host : "unraid")) / 2 - 1,
              y + 14, 2.9f, UN_TEXT, st->host[0] ? st->host : "unraid");
    text_c(y + 52, 2.3f, UN_DIM, st->ip[0] ? st->ip : "no ip");
    if (st->arr[0]){
        if (st->resync > 0){
            double pct = 100.0 * (double)st->resync_pos / (double)st->resync;
            snprintf(b, sizeof b, "%s %.1f%%",
                     st->resync_act[0] ? st->resync_act : "sync", pct);
            text_c(y + 82, 1.8f, UN_WARN, b);
        } else {
            snprintf(b, sizeof b, "array: %s", st->arr);
            text_c(y + 82, 1.8f, strcmp(st->arr, "STARTED") ? UN_WARN : UN_OK, b);
        }
    }
    y += 124;

    card(y, 142, "CPU");
    snprintf(b, sizeof b, "%.0f%%", st->cpu);
    text(22, y + 34, 4.8f, UN_TEXT, b);
    if (st->temp_c > 0){
        snprintf(b, sizeof b, "%dC", st->temp_c);
        text(W - 22 - text_w(2.8f, b), y + 44, 2.8f, st->temp_c > 85 ? UN_BAD : UN_DIM, b);
    }
    /* psys RAPL is miscalibrated on this board (reads ~0); package matches the webUI */
    double pw = st->pwr_pkg_w;
    if (st->pwr_sys_w > st->pwr_pkg_w) pw = st->pwr_sys_w;
    if (pw >= 0){
        snprintf(b, sizeof b, "%.1f W", pw);
        text(W - 22 - text_w(2.0f, b), y + 80, 2.0f, UN_DIM, b);
    }
    bar(22, y + 108, W - 44, 18, st->cpu, level_col(st->cpu));
    y += 154;

    card(y, 128, "MEMORY");
    double mp = st->mem_tot_mb ? 100.0 * st->mem_used_mb / st->mem_tot_mb : 0;
    snprintf(b, sizeof b, "%.1f / %.1f GB", st->mem_used_mb / 1024.0, st->mem_tot_mb / 1024.0);
    text(22, y + 40, 2.7f, UN_TEXT, b);
    snprintf(b, sizeof b, "%.0f%%", mp);
    text(W - 22 - text_w(2.0f, b), y + 44, 2.0f, UN_DIM, b);
    bar(22, y + 94, W - 44, 18, mp, level_col(mp));
    y += 140;

    card(y, 128, "NETWORK");
    text(W - 22 - text_w(1.7f, st->prim_if), y + 10, 1.7f, UN_DIM, st->prim_if);
    fmt_rate(b + 4, sizeof(b) - 4, st->rx_kbs); memcpy(b, "DN  ", 4);
    text(22, y + 40, 2.5f, UN_TEXT, b);
    fmt_rate(b + 4, sizeof(b) - 4, st->tx_kbs); memcpy(b, "UP  ", 4);
    text(22, y + 82, 2.5f, UN_TEXT, b);
    y += 140;

    card(y, 128, "STORAGE");
    snprintf(b, sizeof b, "%.0f / %.0f GB", st->disk_used_gb, st->disk_tot_gb);
    text(22, y + 40, 2.7f, UN_TEXT, b);
    snprintf(b, sizeof b, "%.0f%%", st->disk_used_pct);
    text(W - 22 - text_w(2.0f, b), y + 44, 2.0f, UN_DIM, b);
    bar(22, y + 94, W - 44, 18, st->disk_used_pct, level_col(st->disk_used_pct));
    y += 140;

    fmt_up(b, sizeof b, st->up_s);
    text_c(y + 8, 1.9f, UN_DIM, b);
    page_end = y + 34;
}


#endif /* PANEL_PAGE_OVERVIEW_H */
