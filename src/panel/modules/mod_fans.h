/*
 * plugin/src/panel/modules/mod_fans.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "fans" module — chassis fan speeds (RPM) read from the ITE EC tachometers by
 * read_fan_rpm() (backlight.h). Read-only; the panel does not control fan duty.
 * Composes the card style shim. Part of panel_dash: #included in a fixed order.
 */
#ifndef PANEL_MOD_FANS_H
#define PANEL_MOD_FANS_H

static int mod_fans(int y, stats_t *st, int variant){
    char b[32];
    int n = st->n_fan_rpm;
    if (n <= 0) return value_card(y, 76, "FANS", "n/a", UN_DIM);

    if (variant == 1){                                   /* compact — count spinning + max RPM */
        int on = 0, mx = 0;
        for (int i = 0; i < n; i++){ if (st->fan_rpm[i] > 0) on++; if (st->fan_rpm[i] > mx) mx = st->fan_rpm[i]; }
        snprintf(b, sizeof b, "%d/%d  %d rpm", on, n, mx);
        return value_card(y, 60, "FANS", b, UN_TEXT);
    }

    int h = 40 + n * 28;
    card(y, gy(h), "FANS");
    for (int i = 0; i < n; i++){
        int ry = y + gy(40 + i * 28);
        snprintf(b, sizeof b, "Fan %d", i + 1);
        text(C_X0, ry, 1.8f, UN_DIM, b);
        int rpm = st->fan_rpm[i];
        if (rpm > 0) snprintf(b, sizeof b, "%d rpm", rpm);
        else         snprintf(b, sizeof b, "stopped");
        text(C_R - text_w(1.8f, b), ry, 1.8f, rpm > 0 ? UN_TEXT : UN_DIM, b);
    }
    return gy(h) + gy(C_GAP);
}

#endif /* PANEL_MOD_FANS_H */
