/*
 * plugin/src/panel/modules/mod_transfer.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "transfer" module — live progress of a Dynamix File Manager copy/move (the file
 * manager built into Unraid). Shows the operation, destination folder, a progress
 * bar with the overall percent, and the current rate + ETA; when nothing is
 * copying it shows a dim idle line. Data comes from read_transfer() (stats_array.h).
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_TRANSFER_H
#define PANEL_MOD_TRANSFER_H

static int mod_transfer(int y, stats_t *st, int variant){
    (void)variant;
    if (!st->transfer_active)
        return value_card(y, 84, "TRANSFER", "No active copy", UN_DIM);

    double p = st->transfer_pct; if (p < 0) p = 0; if (p > 100) p = 100;
    int h = 122;

    char title[16]; snprintf(title, sizeof title, "%s", st->transfer_op);
    for (char *c = title; *c; c++) *c = (char)toupper((unsigned char)*c);
    card(y, gy(h), title);

    /* destination folder (left) + overall percent (right) */
    char pc[8]; snprintf(pc, sizeof pc, "%.0f%%", p);
    int pcw = text_w(2.0f, pc);
    text(C_R - pcw, y + gy(44), 2.0f, UN_ORANGE_M, pc);
    if (st->transfer_dest[0]){
        char dst[48]; snprintf(dst, sizeof dst, "%s", st->transfer_dest);
        trunc_fit(dst, 2.0f, C_W - pcw - gy(10));
        text(C_X0, y + gy(44), 2.0f, UN_TEXT, dst);
    }

    /* progress bar (accent fill — higher is better, so not the load palette) */
    bar(C_X0, y + gy(74), C_W, gy(22), p, UN_ORANGE_M);

    /* rate (left) + ETA (right) */
    if (st->transfer_speed[0]) text(C_X0, y + gy(100), C_SUB, UN_DIM, st->transfer_speed);
    if (st->transfer_eta[0]){
        char e[24]; snprintf(e, sizeof e, "ETA %s", st->transfer_eta);
        text(C_R - text_w(C_SUB, e), y + gy(100), C_SUB, UN_DIM, e);
    }
    return gy(h) + gy(C_GAP);
}

#endif /* PANEL_MOD_TRANSFER_H */
