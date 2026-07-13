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
    { "host",    "Host / array",       mod_host,     2, { "card", "compact" } },
    { "array",   "Array status",       mod_array,    2, { "card", "compact" } },
    { "update",  "Updates (OS + plugins)", mod_update, 2, { "card", "compact" } },
    { "cpu",     "CPU",                mod_cpu,      8, { "bar", "ring", "graph", "area", "blocks", "trend", "big", "gauge" } },
    { "mem",     "Memory",             mod_mem,      9, { "bar", "ring", "graph", "area", "blocks", "split", "trend", "big", "gauge" } },
    { "net",     "Network (primary)",  mod_net,      5, { "rows", "compact", "big", "graph", "area" } },
    { "storage", "Storage",            mod_storage,  9, { "bar", "ring", "graph", "area", "blocks", "split", "trend", "big", "gauge" } },
    { "uptime",  "Uptime",             mod_uptime,   2, { "card", "big" } },
    { "gpu",     "GPU",                mod_gpu,      7, { "bar", "graph", "area", "blocks", "trend", "big", "gauge" } },
    { "cputemp", "CPU temperature",    mod_cputemp,  4, { "bar", "graph", "area", "gauge" } },
    { "npu",     "NPU",                mod_npu,      1, { "card" } },
    { "power",   "Power draw",         mod_power,    2, { "card", "big" } },
    { "ifaces",  "Interfaces (all in one card)", mod_ifaces, 4, { "full", "compact", "mini", "big" } },
    { "iface",   "Interface (pick one)",         mod_iface,  4, { "full", "compact", "mini", "big" }, 1, "ifaces" },
    { "disks",   "Disks (all in one card)",      mod_disks,  1, { "list" } },
    { "disk",    "Disk (pick one)",              mod_disk,   1, { "card" }, 1, "disks" },
    { "containers","Containers (all in one card)",mod_containers,1, { "list" } },
    { "container","Container (pick one)",        mod_container,  1, { "card" }, 1, "containers" },
    { "vms",     "VMs (all in one card)",        mod_vms,        1, { "card" } },
    { "vm",      "VM (pick one)",                mod_vm,         1, { "card" }, 1, "vms" },
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
        if (m){ g_item_key = var ? var : ""; y += m->fn(y, st, mod_variant_idx(m, var)); }
    }
    page_end = y;
}

/* print the module registry as JSON on stdout (for the web layout editor's palette,
 * so it stays in sync with this table). Shape:
 * [{"id":"cpu","label":"CPU","variants":["bar","ring","graph"]},...] */
static int write_modules_json(void){
    printf("[");
    for (int i = 0; i < N_MODULES; i++){
        printf("%s{\"id\":\"%s\",\"label\":\"%s\",\"indexed\":%d,\"itemsrc\":\"%s\",\"variants\":[",
               i ? "," : "", MODULES[i].id, MODULES[i].label, MODULES[i].indexed,
               MODULES[i].itemsrc ? MODULES[i].itemsrc : "");
        for (int v = 0; v < MODULES[i].nvariants; v++)
            printf("%s\"%s\"", v ? "," : "", MODULES[i].variants[v]);
        printf("]}");
    }
    printf("]\n");
    return 0;
}

/* JSON-escape a string to stdout (names are user-entered; layouts are [a-z0-9,:]) */
static void json_str(const char *s){
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++){
        if (*p == '"' || *p == '\\'){ putchar('\\'); putchar((char)*p); }
        else if (*p < 0x20)          printf("\\u%04x", *p);
        else                         putchar((char)*p);
    }
    putchar('"');
}
/* print the current content pages (name + module layout + enable) as JSON, for the
 * web layout editor to load real state. Requires pages_finalize() to have run. */
static int write_layouts_json(void){
    printf("{\"pages\":[");
    for (int i = 0; i < g_ncpage; i++){
        printf("%s{\"name\":", i ? "," : "");
        json_str(g_cpage[i].name);
        printf(",\"layout\":");
        json_str(g_cpage[i].layout);
        printf(",\"on\":%d}", g_cpage[i].on);
    }
    printf("]}\n");
    return 0;
}

/* print the live item names for the indexed modules, so the web editor can offer
 * a NAME dropdown (interfaces / disks / containers) instead of a raw index. */
static int write_items_json(void){
    stats_t st; memset(&st, 0, sizeof st);
    read_net(&st); read_disks(&st); read_docker(&st);
    printf("{\"ifaces\":[");
    for (int i = 0; i < st.n_ifaces; i++){ if (i) putchar(','); json_str(st.ifc[i].name); }
    printf("],\"disks\":[");
    for (int i = 0; i < st.n_disks; i++){ if (i) putchar(','); json_str(st.disks[i].name); }
    printf("],\"containers\":[");
    for (int i = 0; i < st.n_ctrs; i++){ if (i) putchar(','); json_str(st.ctrs[i].name); }
    printf("],\"vms\":[");
    for (int i = 0; i < st.n_vms; i++){ if (i) putchar(','); json_str(st.vms[i].name); }
    printf("]}\n");
    return 0;
}

#endif /* PANEL_MODULE_REGISTRY_H */
