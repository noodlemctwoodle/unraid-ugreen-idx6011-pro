/*
 * plugin/src/panel/modules/registry.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * The module table (stable id -> draw fn + display variants) and the generic
 * page renderer. A page is an ordered "id[:variant],..." layout string in
 * settings.cfg (LAYOUT_<PAGE>); render_modules() parses it and draws each module
 * in order, advancing y by the height each module reports.
 *
 * Included after every mod_*.h so their functions are visible. Part of
 * panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MODULE_REGISTRY_H
#define PANEL_MODULE_REGISTRY_H

static const modinfo_t MODULES[] = {
    { "host",    "Host",     mod_host,    1, { "card" } },
    { "cpu",     "CPU",      mod_cpu,     3, { "bar", "ring", "graph" } },
    { "mem",     "Memory",   mod_mem,     2, { "bar", "ring" } },
    { "net",     "Network",  mod_net,     1, { "rows" } },
    { "storage", "Storage",  mod_storage, 2, { "bar", "ring" } },
    { "uptime",  "Uptime",   mod_uptime,  1, { "card" } },
    { "gpu",     "GPU",        mod_gpu,        2, { "bar", "graph" } },
    { "cputemp", "CPU Temp",   mod_cputemp,    1, { "bar" } },
    { "npu",     "NPU",        mod_npu,        1, { "card" } },
    { "power",   "Power",      mod_power,      1, { "card" } },
    { "ifaces",  "Interfaces", mod_ifaces,     1, { "list" } },
    { "disks",   "Disks",      mod_disks,      1, { "list" } },
    { "containers","Containers",mod_containers,1, { "list" } },
    { "vms",     "VMs",        mod_vms,        1, { "card" } },
};
static const int N_MODULES = (int)(sizeof MODULES / sizeof MODULES[0]);

static const modinfo_t *mod_find(const char *id){
    for (int i = 0; i < N_MODULES; i++)
        if (!strcmp(MODULES[i].id, id)) return &MODULES[i];
    return NULL;
}
static int mod_variant_idx(const modinfo_t *m, const char *v){
    if (!v || !*v) return 0;
    for (int i = 0; i < m->nvariants; i++)
        if (!strcmp(m->variants[i], v)) return i;
    return 0;
}

/* draw a page body from an ordered "id[:variant],id[:variant],..." layout.
 * Unknown ids are skipped. Sets page_end for the scroll/height bookkeeping. */
static void render_modules(const char *layout, stats_t *st){
    char buf[256];
    snprintf(buf, sizeof buf, "%s", layout ? layout : "");
    int y = body_top;
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)){
        while (*tok == ' ') tok++;
        char *colon = strchr(tok, ':');
        char *var = NULL;
        if (colon){ *colon = 0; var = colon + 1; }
        const modinfo_t *m = mod_find(tok);
        if (m) y += m->fn(y, st, mod_variant_idx(m, var));
    }
    page_end = y;
}

#endif /* PANEL_MODULE_REGISTRY_H */
