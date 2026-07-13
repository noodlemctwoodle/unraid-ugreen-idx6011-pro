/*
 * plugin/src/panel/modules/mod_vms.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "vms" module — virtual-machine count (value card).
 * Composes the card style shim.
 * Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_VMS_H
#define PANEL_MOD_VMS_H

static int mod_vms(int y, stats_t *st, int variant){
    (void)variant;
    if (!st->vm_enabled) return value_card(y, 76, "VMS", "disabled", UN_DIM);
    char b[32]; snprintf(b, sizeof b, "%d running", st->vm_count);
    return value_card(y, 76, "VMS", b, UN_TEXT);
}

#endif /* PANEL_MOD_VMS_H */
