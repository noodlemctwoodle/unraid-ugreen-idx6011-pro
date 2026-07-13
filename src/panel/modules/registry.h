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
    { "cpu",     "CPU",      mod_cpu,     5, { "bar", "ring", "graph", "big", "gauge" } },
    { "mem",     "Memory",   mod_mem,     4, { "bar", "ring", "big", "gauge" } },
    { "net",     "Network",  mod_net,     1, { "rows" } },
    { "storage", "Storage",  mod_storage, 4, { "bar", "ring", "big", "gauge" } },
    { "uptime",  "Uptime",   mod_uptime,  1, { "card" } },
    { "gpu",     "GPU",        mod_gpu,        4, { "bar", "graph", "big", "gauge" } },
    { "cputemp", "CPU Temp",   mod_cputemp,    1, { "bar" } },
    { "npu",     "NPU",        mod_npu,        1, { "card" } },
    { "power",   "Power",      mod_power,      1, { "card" } },
    { "ifaces",  "Interfaces", mod_ifaces,     4, { "full", "compact", "mini", "big" } },
    { "iface",   "Interface",  mod_iface,      1, { "card" }, 1 },
    { "disks",   "Disks",      mod_disks,      1, { "list" } },
    { "disk",    "Disk",       mod_disk,       1, { "card" }, 1 },
    { "containers","Containers",mod_containers,1, { "list" } },
    { "container","Container", mod_container,  1, { "card" }, 1 },
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
    if (v[0] >= '0' && v[0] <= '9') return atoi(v);  /* per-item instance index (disk:2, ...) */
    return 0;
}

/* when set (by --preview), render_modules uses this layout instead of the page's
 * configured one — lets the web editor render a DRAFT layout via the real renderer */
static const char *g_preview_layout = NULL;

/* draw a page body from an ordered "id[:variant],id[:variant],..." layout.
 * Unknown ids are skipped. Sets page_end for the scroll/height bookkeeping. */
static void render_modules(const char *layout, stats_t *st){
    if (g_preview_layout) layout = g_preview_layout;
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

/* print the module registry as JSON on stdout (for the web layout editor's palette,
 * so it stays in sync with this table). Shape:
 * [{"id":"cpu","label":"CPU","variants":["bar","ring","graph"]},...] */
static int write_modules_json(void){
    printf("[");
    for (int i = 0; i < N_MODULES; i++){
        printf("%s{\"id\":\"%s\",\"label\":\"%s\",\"indexed\":%d,\"variants\":[",
               i ? "," : "", MODULES[i].id, MODULES[i].label, MODULES[i].indexed);
        for (int v = 0; v < MODULES[i].nvariants; v++)
            printf("%s\"%s\"", v ? "," : "", MODULES[i].variants[v]);
        printf("]}");
    }
    printf("]\n");
    return 0;
}

/* print the current effective per-page layouts + enable toggles as JSON (for the
 * web layout editor to load real state without duplicating the defaults). Layout
 * strings are [a-z0-9,:] so need no JSON escaping. */
static int write_layouts_json(void){
    printf("{\"home\":\"%s\",\"overview\":\"%s\",\"hardware\":\"%s\","
           "\"network\":\"%s\",\"disks\":\"%s\",\"docker\":\"%s\","
           "\"on\":[%d,%d,%d,%d,%d,%d]}\n",
           cfg_layout_home, cfg_layout_overview, cfg_layout_hardware,
           cfg_layout_network, cfg_layout_disks, cfg_layout_docker,
           cfg_page_on[0], cfg_page_on[1], cfg_page_on[2],
           cfg_page_on[3], cfg_page_on[4], cfg_page_on[5]);
    return 0;
}

#endif /* PANEL_MODULE_REGISTRY_H */
