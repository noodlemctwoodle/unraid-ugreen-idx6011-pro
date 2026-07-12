/*
 * plugin/src/panel/pages/home.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * HOME page — CPU ring gauge + stacked stat tiles
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_HOME_H
#define PANEL_PAGE_HOME_H

/* ---------- pages ---------- */
/* one full-width stat tile: label left, big value right, bar underneath */
static void home_tile(int y, int h, const char *label, const char *val,
                      uint32_t vcol, double barpct, uint32_t barcol){
    int x = 10, w = W - 20;
    tile(x, y, w, h, label);
    text(x + w - 14 - text_w(3.0f, val), y + 24, 3.0f, vcol, val);
    if (barpct >= 0)
        bar(x + 14, y + h - 22, w - 28, 12, barpct, barcol);
}

static void page_home(stats_t *st){
    char b[64], r1[24], r2[24];
    int y = body_top;

    /* CPU ring gauge is the hero (header already shows clock + date) */
    int cx = W / 2, cy = y + 82, ro = 74, ri = 56;
    ring_gauge(cx, cy, ro, ri, st->cpu, level_col(st->cpu));
    snprintf(b, sizeof b, "%.0f%%", st->cpu);
    text_bold(cx - text_w(3.4f, b) / 2 - 1, cy - 22, 3.4f, UN_TEXT, b);
    text(cx - text_w(1.6f, "CPU") / 2, cy + 16, 1.6f, UN_DIM, "CPU");
    y += 178;

    int th = 80;

    double mp = st->mem_tot_mb ? 100.0 * st->mem_used_mb / st->mem_tot_mb : 0;
    snprintf(b, sizeof b, "%.0f%%", mp);
    home_tile(y, th, "RAM", b, UN_TEXT, mp, level_col(mp)); y += th + 10;

    if (st->temp_c > 0) snprintf(b, sizeof b, "%dC", st->temp_c);
    else                snprintf(b, sizeof b, "--");
    home_tile(y, th, "CPU TEMP", b, st->temp_c > 85 ? UN_BAD : UN_TEXT,
              st->temp_c > 0 ? st->temp_c : 0, st->temp_c > 85 ? UN_BAD : UN_ORANGE_M);
    y += th + 10;

    fmt_rate_s(r1, sizeof r1, st->rx_kbs);
    fmt_rate_s(r2, sizeof r2, st->tx_kbs);
    int nth = th + 12;   /* taller: label + two rate lines need padding both ends */
    tile(10, y, W - 20, nth, "NETWORK");
    snprintf(b, sizeof b, "D %s", r1); text(24, y + 38, 2.2f, UN_TEXT, b);
    snprintf(b, sizeof b, "U %s", r2); text(24, y + 64, 2.2f, UN_TEXT, b);
    y += nth + 10;

    snprintf(b, sizeof b, "%.0f%%", st->disk_used_pct);
    home_tile(y, th, "STORAGE", b, UN_TEXT, st->disk_used_pct, level_col(st->disk_used_pct));
    y += th + 10;

    /* power draw (RAPL package; psys is miscalibrated on this board) */
    double pw = st->pwr_pkg_w;
    if (st->pwr_sys_w > st->pwr_pkg_w) pw = st->pwr_sys_w;
    if (pw >= 0) snprintf(b, sizeof b, "%.1f W", pw);
    else         snprintf(b, sizeof b, "n/a");
    home_tile(y, th, "POWER", b, UN_ORANGE, -1, 0);
    y += th + 10;

    fmt_up(b, sizeof b, st->up_s);
    home_tile(y, th, "UPTIME", strncmp(b, "up ", 3) ? b : b + 3, UN_TEXT, -1, 0);
    y += th + 14;
    page_end = y;
}


#endif /* PANEL_PAGE_HOME_H */
