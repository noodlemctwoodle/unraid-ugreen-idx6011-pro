/*
 * plugin/src/panel/modules/mod_vms.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * VM cards. "vms" draws a summary + one card per VM; "vm" (indexed, e.g. vm:Win11)
 * draws a single VM picked by name. Data from read_vms() (libvirt domain files).
 * Composes the card style shim. Part of panel_dash: #included in a fixed order.
 */
#ifndef PANEL_MOD_VMS_H
#define PANEL_MOD_VMS_H

static int vm_card(int y, vm_t *vm){
    card(y, gy(64), NULL);
    item_head(y, vm->running ? UN_OK : 0x555555, vm->name, 2.1f,
              vm->running ? "RUNNING" : "STOPPED", 1.5f, vm->running ? UN_OK : UN_DIM);
    return gy(64) + gy(C_GAP);
}

static int mod_vms(int y, stats_t *st, int variant){         /* summary + all VMs */
    (void)variant;
    if (!st->vm_enabled) return value_card(y, 76, "VMS", "disabled", UN_DIM);
    char b[32]; snprintf(b, sizeof b, "%d / %d running", st->vm_count, st->n_vms);
    if (!strcmp(g_item_key, "compact"))                      /* summary only, no per-VM cards */
        return value_card(y, 60, "VMS", b, UN_TEXT);
    int y0 = y;
    y += value_card(y, 76, "VMS", b, UN_TEXT);
    for (int i = 0; i < st->n_vms; i++) y += vm_card(y, &st->vms[i]);
    if (st->n_vms == 0) y += value_card(y, 56, "VMS", "no VMs", UN_DIM);
    return y - y0;
}

static int mod_vm(int y, stats_t *st, int variant){          /* one VM, picked by name */
    (void)variant;
    if (!st->vm_enabled) return value_card(y, 76, "VM", "VMs off", UN_DIM);
    int i = -1;
    if (g_item_key[0]){
        for (int k = 0; k < st->n_vms; k++) if (!strcmp(st->vms[k].name, g_item_key)){ i = k; break; }
        if (i < 0 && g_item_key[0] >= '0' && g_item_key[0] <= '9'){ int n = atoi(g_item_key); if (n >= 0 && n < st->n_vms) i = n; }
    } else if (st->n_vms > 0) i = 0;
    if (i < 0) return value_card(y, 76, "VM", "not present", UN_DIM);
    return vm_card(y, &st->vms[i]);
}

#endif /* PANEL_MOD_VMS_H */
