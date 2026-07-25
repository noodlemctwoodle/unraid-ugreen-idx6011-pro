/*
 * plugin/src/panel/modules/mod_diskio.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "diskio" module — Disk I/O. Reads read_diskio() (stats_extra.h) and degrades to a
 * dim idle line when its source is absent. Part of panel_dash: #included in order.
 */
#ifndef PANEL_MOD_DISKIO_H
#define PANEL_MOD_DISKIO_H

static int mod_diskio(int y, stats_t *st, int variant){
    (void)variant;
    int h = 104;
    card(y, gy(h), "DISK I/O");
    double rd = st->io_rd_mbs < 0 ? 0 : st->io_rd_mbs;
    double wr = st->io_wr_mbs < 0 ? 0 : st->io_wr_mbs;
    char v[40];
    int lw = text_w(C_BODY, "Write");                      /* reserve label room */
    /* read row: dim label (left) + right-aligned rate (bright), truncated to its zone */
    text(C_X0, y + gy(44), C_BODY, UN_DIM, "Read");
    snprintf(v, sizeof v, "%.1f MB/s", rd);
    trunc_fit(v, C_BODY, C_W - lw - gy(8));
    text(C_R - text_w(C_BODY, v), y + gy(44), C_BODY, UN_TEXT, v);
    /* write row */
    text(C_X0, y + gy(74), C_BODY, UN_DIM, "Write");
    snprintf(v, sizeof v, "%.1f MB/s", wr);
    trunc_fit(v, C_BODY, C_W - lw - gy(8));
    text(C_R - text_w(C_BODY, v), y + gy(74), C_BODY, UN_TEXT, v);
    return gy(h) + gy(C_GAP);
}

#endif /* PANEL_MOD_DISKIO_H */
