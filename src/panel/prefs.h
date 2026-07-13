/*
 * plugin/src/panel/prefs.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * flash-persistent, webGUI-editable settings (load / save)
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PREFS_H
#define PANEL_PREFS_H

/* ---------- settings (flash-persistent, webGUI-editable) ---------- */
#define CFG_DIR1 "/boot/config/plugins/ugreen-idx6011-pro"
#define CFG_DIR2 "/boot/config/plugins/ugreen-idx6011-pro/panel"
#define CFG_PATH "/boot/config/plugins/ugreen-idx6011-pro/panel/settings.cfg"

/* dynamic content pages (name + module layout + enable). Populated from
 * settings.cfg — either the new model (N_PAGES / PAGE<n>_NAME|LAYOUT|ON) or, for
 * older configs, migrated from the fixed LAYOUT_ and PAGE_ keys by pages_finalize().
 * pageframe.h consumes g_cpage / g_ncpage; SETTINGS is appended there. */
#define MAX_CPAGES 12
typedef struct { char name[24]; char layout[256]; int on; } cpage_t;
static cpage_t g_cpage[MAX_CPAGES];
static int g_ncpage = 0;
static int g_pages_from_cfg = 0;        /* 1 once N_PAGES / PAGE<n>_ is seen */
static int g_pmax = -1;                 /* highest PAGE<n>_ index seen (fallback count) */

static int cfg_brightness = 75;     /* 5..100 */
static int cfg_interval   = 1;      /* stats seconds */
static int cfg_rotate     = 0;      /* auto-rotate seconds: 0/10/20/60 */
static int cfg_screen_off = 0;      /* minutes: 0(never)/1/5/15 */
static int cfg_night      = 0;      /* 1 = clamp brightness to 15% */
static int cfg_leds       = 1;      /* chassis LEDs on/off */
static int cfg_fan_mode   = 0;      /* fan curve: 0 auto, 1 silent, 2 quiet, 3 turbo, 4 max */
static char cfg_primary_if[32] = "";/* Overview/Home "primary" iface; empty = default-route auto-pick */
static int  cfg_net_bits  = 1;      /* 1 = net rates in bits (Kbps), 0 = bytes (KB/s) */
static char cfg_font[32]  = "RobotoCondensed"; /* fonts/<name>.ttf; empty = built-in easy_font */
static int  cfg_head_pct  = 100;    /* heading size %  (section titles)  */
static int  cfg_text_pct  = 100;    /* body text size % (values, labels) */
/* per-page module layout: ordered "id[:variant],..." list drawn by render_modules().
 * Default matches the original hardcoded Overview (all default/bar variants). */
static char cfg_layout_overview[256] = "host,cpu,mem,net,storage,uptime";
static char cfg_layout_hardware[256] = "cpu:graph,cputemp,mem,gpu:graph,npu,power";
static char cfg_layout_network[256]  = "ifaces";
static char cfg_layout_disks[256]    = "disks";
static char cfg_layout_docker[256]   = "containers,vms";
/* HOME is a fully user-composable page — a distinct ring-based starter by default */
static char cfg_layout_home[256]     = "cpu:ring,mem:ring,net,storage:ring,power,uptime";
/* per-page enable (PAGE_<X>); index order matches pages[] in pageframe.h.
 * All on by default; the SETTINGS page (index 6) is always kept on. */
static int  cfg_page_on[7] = { 1, 1, 1, 1, 1, 1, 1 };

/* parse a "#rrggbb" / "rrggbb" hex colour; returns def on empty/invalid */
static uint32_t parse_hexcol(const char *v, uint32_t def){
    if (*v == '#') v++;
    if (!*v) return def;
    char *end; unsigned long c = strtoul(v, &end, 16);
    return end != v ? (uint32_t)(c & 0xffffffu) : def;
}

static void settings_load(void){
    FILE *f = fopen(CFG_PATH, "r"); if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)){
        char *k, *v;
        if (!ini_kv(line, &k, &v)) continue;
        if      (!strcmp(k, "BRIGHTNESS"))     cfg_brightness = atoi(v);
        else if (!strcmp(k, "INTERVAL"))       cfg_interval   = atoi(v);
        else if (!strcmp(k, "ROTATE"))         cfg_rotate     = atoi(v);
        else if (!strcmp(k, "SCREEN_OFF_MIN")) cfg_screen_off = atoi(v);
        else if (!strcmp(k, "NIGHT"))          cfg_night      = atoi(v) != 0;
        else if (!strcmp(k, "LEDS"))           cfg_leds       = atoi(v) != 0;
        else if (!strcmp(k, "FAN_MODE"))       cfg_fan_mode = !strcmp(v, "silent") ? 1 : !strcmp(v, "quiet") ? 2 : !strcmp(v, "turbo") ? 3 : !strcmp(v, "max") ? 4 : 0;
        else if (!strcmp(k, "PRIMARY_IFACE"))  snprintf(cfg_primary_if, sizeof cfg_primary_if, "%s", v);
        else if (!strcmp(k, "NET_UNITS"))      cfg_net_bits = strcmp(v, "bytes") != 0;
        else if (!strcmp(k, "FONT"))           snprintf(cfg_font, sizeof cfg_font, "%s", v);
        else if (!strcmp(k, "HEAD_SCALE"))     cfg_head_pct = atoi(v);
        else if (!strcmp(k, "TEXT_SCALE"))     cfg_text_pct = atoi(v);
        else if (!strcmp(k, "LAYOUT_OVERVIEW")) snprintf(cfg_layout_overview, sizeof cfg_layout_overview, "%s", v);
        else if (!strcmp(k, "LAYOUT_HARDWARE")) snprintf(cfg_layout_hardware, sizeof cfg_layout_hardware, "%s", v);
        else if (!strcmp(k, "LAYOUT_NETWORK"))  snprintf(cfg_layout_network,  sizeof cfg_layout_network,  "%s", v);
        else if (!strcmp(k, "LAYOUT_DISKS"))    snprintf(cfg_layout_disks,    sizeof cfg_layout_disks,    "%s", v);
        else if (!strcmp(k, "LAYOUT_DOCKER"))   snprintf(cfg_layout_docker,   sizeof cfg_layout_docker,   "%s", v);
        else if (!strcmp(k, "LAYOUT_HOME"))     snprintf(cfg_layout_home,     sizeof cfg_layout_home,     "%s", v);
        else if (!strcmp(k, "N_PAGES")){ int n = atoi(v); if (n < 0) n = 0; if (n > MAX_CPAGES) n = MAX_CPAGES;
                                         g_ncpage = n; g_pages_from_cfg = 1; }
        else if (!strncmp(k, "PAGE", 4) && k[4] >= '0' && k[4] <= '9'){   /* PAGE<n>_NAME|LAYOUT|ON */
            g_pages_from_cfg = 1;
            int idx = atoi(k + 4); const char *u = strchr(k + 4, '_');
            if (u && idx >= 0 && idx < MAX_CPAGES){
                if (idx > g_pmax) g_pmax = idx;
                if      (!strcmp(u, "_NAME"))   snprintf(g_cpage[idx].name,   sizeof g_cpage[idx].name,   "%s", v);
                else if (!strcmp(u, "_LAYOUT")) snprintf(g_cpage[idx].layout, sizeof g_cpage[idx].layout, "%s", v);
                else if (!strcmp(u, "_ON"))     g_cpage[idx].on = atoi(v) != 0;
            }
        }
        else if (!strcmp(k, "PAGE_HOME"))       cfg_page_on[0] = atoi(v) != 0;
        else if (!strcmp(k, "PAGE_OVERVIEW"))   cfg_page_on[1] = atoi(v) != 0;
        else if (!strcmp(k, "PAGE_HARDWARE"))   cfg_page_on[2] = atoi(v) != 0;
        else if (!strcmp(k, "PAGE_NETWORK"))    cfg_page_on[3] = atoi(v) != 0;
        else if (!strcmp(k, "PAGE_DISKS"))      cfg_page_on[4] = atoi(v) != 0;
        else if (!strcmp(k, "PAGE_DOCKER"))     cfg_page_on[5] = atoi(v) != 0;
        else if (!strcmp(k, "COL_ACCENT"))     UN_ORANGE_M = parse_hexcol(v, UN_ORANGE_M);
        else if (!strcmp(k, "COL_GRAD_A"))     UN_RED      = parse_hexcol(v, UN_RED);
        else if (!strcmp(k, "COL_GRAD_B"))     UN_ORANGE   = parse_hexcol(v, UN_ORANGE);
        else if (!strcmp(k, "COL_BG"))         UN_BLACK    = parse_hexcol(v, UN_BLACK);
        else if (!strcmp(k, "COL_CARD"))       UN_GREY_80  = parse_hexcol(v, UN_GREY_80);
        else if (!strcmp(k, "COL_TEXT"))       UN_TEXT     = parse_hexcol(v, UN_TEXT);
        else if (!strcmp(k, "COL_DIM"))        UN_DIM      = parse_hexcol(v, UN_DIM);
        else if (!strcmp(k, "COL_OK"))         UN_OK       = parse_hexcol(v, UN_OK);
        else if (!strcmp(k, "COL_WARN"))       UN_WARN     = parse_hexcol(v, UN_WARN);
        else if (!strcmp(k, "COL_BAD"))        UN_BAD      = parse_hexcol(v, UN_BAD);
    }
    fclose(f);
    if (cfg_brightness < 5)   cfg_brightness = 5;
    if (cfg_brightness > 100) cfg_brightness = 100;
    if (cfg_interval < 1)     cfg_interval = 1;
    if (cfg_head_pct < 70) cfg_head_pct = 70; if (cfg_head_pct > 200) cfg_head_pct = 200;
    if (cfg_text_pct < 70) cfg_text_pct = 70; if (cfg_text_pct > 200) cfg_text_pct = 200;
    g_head_scale  = cfg_head_pct / 100.0f;
    g_text_scale  = cfg_text_pct / 100.0f;
    g_geom_scale  = g_head_scale > g_text_scale ? g_head_scale : g_text_scale;
}

/* env overrides applied AFTER settings_load — used ONLY by the web live-preview
 * (preview.php) so the Theme tab can preview draft colour / size choices without
 * saving them. Not set for the live panel, so it has no effect there. */
static void settings_env_overrides(void){
    const char *v;
    if ((v = getenv("PANEL_HEAD")) && *v){ cfg_head_pct = atoi(v);
        if (cfg_head_pct < 70) cfg_head_pct = 70; if (cfg_head_pct > 200) cfg_head_pct = 200;
        g_head_scale = cfg_head_pct / 100.0f; }
    if ((v = getenv("PANEL_TEXT")) && *v){ cfg_text_pct = atoi(v);
        if (cfg_text_pct < 70) cfg_text_pct = 70; if (cfg_text_pct > 200) cfg_text_pct = 200;
        g_text_scale = cfg_text_pct / 100.0f; }
    g_geom_scale = g_head_scale > g_text_scale ? g_head_scale : g_text_scale;
    if ((v = getenv("PANEL_COL_ACCENT")) && *v) UN_ORANGE_M = parse_hexcol(v, UN_ORANGE_M);
    if ((v = getenv("PANEL_COL_GRAD_A")) && *v) UN_RED      = parse_hexcol(v, UN_RED);
    if ((v = getenv("PANEL_COL_GRAD_B")) && *v) UN_ORANGE   = parse_hexcol(v, UN_ORANGE);
    if ((v = getenv("PANEL_COL_BG"))     && *v) UN_BLACK    = parse_hexcol(v, UN_BLACK);
    if ((v = getenv("PANEL_COL_CARD"))   && *v) UN_GREY_80  = parse_hexcol(v, UN_GREY_80);
    if ((v = getenv("PANEL_COL_TEXT"))   && *v) UN_TEXT     = parse_hexcol(v, UN_TEXT);
    if ((v = getenv("PANEL_COL_DIM"))    && *v) UN_DIM      = parse_hexcol(v, UN_DIM);
    if ((v = getenv("PANEL_COL_OK"))     && *v) UN_OK       = parse_hexcol(v, UN_OK);
    if ((v = getenv("PANEL_COL_WARN"))   && *v) UN_WARN     = parse_hexcol(v, UN_WARN);
    if ((v = getenv("PANEL_COL_BAD"))    && *v) UN_BAD      = parse_hexcol(v, UN_BAD);
}

/* build the runtime content-page list: from the new N_PAGES / PAGE<n>_ model if
 * the cfg used it, otherwise migrate from the older fixed per-page keys. Always
 * leaves at least one content page. Called once after settings_load + overrides. */
static void pages_finalize(void){
    if (g_pages_from_cfg){
        if (g_ncpage <= 0) g_ncpage = g_pmax + 1;          /* no N_PAGES -> infer from indices */
        if (g_ncpage > MAX_CPAGES) g_ncpage = MAX_CPAGES;
        for (int i = 0; i < g_ncpage; i++)
            if (!g_cpage[i].name[0]) snprintf(g_cpage[i].name, sizeof g_cpage[i].name, "Page %d", i + 1);
    } else {                                               /* migrate the fixed six */
        const char *nm[6] = { "Home", "Overview", "Hardware", "Network", "Disks", "Docker" };
        const char *ly[6] = { cfg_layout_home, cfg_layout_overview, cfg_layout_hardware,
                              cfg_layout_network, cfg_layout_disks, cfg_layout_docker };
        g_ncpage = 6;
        for (int i = 0; i < 6; i++){
            snprintf(g_cpage[i].name,   sizeof g_cpage[i].name,   "%s", nm[i]);
            snprintf(g_cpage[i].layout, sizeof g_cpage[i].layout, "%s", ly[i]);
            g_cpage[i].on = cfg_page_on[i];
        }
    }
    if (g_ncpage < 1){                                     /* never zero pages */
        g_ncpage = 1;
        snprintf(g_cpage[0].name,   sizeof g_cpage[0].name,   "Overview");
        snprintf(g_cpage[0].layout, sizeof g_cpage[0].layout, "host,cpu,mem,net,storage,uptime");
        g_cpage[0].on = 1;
    }
}

/* keys the dashboard owns; anything else in settings.cfg (LED_* for monitor.sh,
 * future keys) is preserved verbatim across a save. */
static int is_managed_key(const char *k){
    static const char *m[] = {
        "BRIGHTNESS","INTERVAL","ROTATE","SCREEN_OFF_MIN","NIGHT","LEDS","FAN_MODE",
        "PRIMARY_IFACE","NET_UNITS","FONT","HEAD_SCALE","TEXT_SCALE",
        "COL_ACCENT","COL_GRAD_A","COL_GRAD_B",
        "COL_BG","COL_CARD","COL_TEXT","COL_DIM","COL_OK","COL_WARN","COL_BAD", 0 };
    for (int i = 0; m[i]; i++) if (!strcmp(k, m[i])) return 1;
    return 0;
}

/* atomic write (tmp + rename); called only when a value actually changed */
static void settings_save(void){
    mkdir(CFG_DIR1, 0755); mkdir(CFG_DIR2, 0755);
    /* carry over unmanaged keys (LED colours, power toggle, ...) unchanged */
    char keep[2048]; size_t kl = 0; keep[0] = 0;
    FILE *r = fopen(CFG_PATH, "r");
    if (r){
        char line[256];
        while (fgets(line, sizeof line, r)){
            char key[64]; int i = 0;
            while (line[i] && line[i] != '=' && i < 63){ key[i] = line[i]; i++; }
            key[i] = 0;
            if (line[i] == '=' && !is_managed_key(key)){
                size_t n = strlen(line);
                if (kl + n < sizeof keep - 1){ memcpy(keep + kl, line, n); kl += n; keep[kl] = 0; }
            }
        }
        fclose(r);
    }
    char tmp[sizeof CFG_PATH + 8];
    snprintf(tmp, sizeof tmp, "%s.tmp", CFG_PATH);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    fprintf(f, "BRIGHTNESS=%d\nINTERVAL=%d\nROTATE=%d\n"
               "SCREEN_OFF_MIN=%d\nNIGHT=%d\nLEDS=%d\nFAN_MODE=%s\nPRIMARY_IFACE=%s\nNET_UNITS=%s\nFONT=%s\n"
               "HEAD_SCALE=%d\nTEXT_SCALE=%d\n"
               "COL_ACCENT=%06x\nCOL_GRAD_A=%06x\nCOL_GRAD_B=%06x\nCOL_BG=%06x\n"
               "COL_CARD=%06x\nCOL_TEXT=%06x\nCOL_DIM=%06x\n"
               "COL_OK=%06x\nCOL_WARN=%06x\nCOL_BAD=%06x\n",
            cfg_brightness, cfg_interval, cfg_rotate,
            cfg_screen_off, cfg_night, cfg_leds,
            cfg_fan_mode == 1 ? "silent" : cfg_fan_mode == 2 ? "quiet" : cfg_fan_mode == 3 ? "turbo" : cfg_fan_mode == 4 ? "max" : "auto",
            cfg_primary_if,
            cfg_net_bits ? "bits" : "bytes", cfg_font,
            cfg_head_pct, cfg_text_pct,
            (unsigned)UN_ORANGE_M, (unsigned)UN_RED, (unsigned)UN_ORANGE, (unsigned)UN_BLACK,
            (unsigned)UN_GREY_80, (unsigned)UN_TEXT, (unsigned)UN_DIM,
            (unsigned)UN_OK, (unsigned)UN_WARN, (unsigned)UN_BAD);
    fputs(keep, f);                            /* preserved unmanaged keys (LED_* etc.) */
    fclose(f);
    rename(tmp, CFG_PATH);
}


#endif /* PANEL_PREFS_H */
