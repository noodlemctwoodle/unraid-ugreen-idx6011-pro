/*
 * plugin/src/panel/modules/mod_license.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * "license" module — Unraid licence type + registered-to (regTy / regTo from
 * var.ini, parsed by read_misc). One text line; "unregistered" when absent.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_LICENSE_H
#define PANEL_MOD_LICENSE_H

static int mod_license(int y, stats_t *st, int variant){
    (void)variant;
    char v[64];
    if (st->reg_type[0] && st->reg_to[0])
        snprintf(v, sizeof v, "%s \xC2\xB7 %s", st->reg_type, st->reg_to);
    else if (st->reg_type[0])
        snprintf(v, sizeof v, "%s", st->reg_type);
    else
        snprintf(v, sizeof v, "unregistered");
    return value_card(y, 84, "LICENCE", v, UN_DIM);
}

#endif /* PANEL_MOD_LICENSE_H */
