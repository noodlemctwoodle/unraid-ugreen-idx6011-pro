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

static int cfg_brightness = 75;     /* 5..100 */
static int cfg_interval   = 1;      /* stats seconds */
static int cfg_rotate     = 0;      /* auto-rotate seconds: 0/10/20/60 */
static int cfg_screen_off = 0;      /* minutes: 0(never)/1/5/15 */
static int cfg_night      = 0;      /* 1 = clamp brightness to 15% */
static int cfg_leds       = 1;      /* chassis LEDs on/off */
static char cfg_primary_if[32] = "";/* Overview/Home "primary" iface; empty = default-route auto-pick */
static int  cfg_net_bits  = 1;      /* 1 = net rates in bits (Kbps), 0 = bytes (KB/s) */

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
        else if (!strcmp(k, "PRIMARY_IFACE"))  snprintf(cfg_primary_if, sizeof cfg_primary_if, "%s", v);
        else if (!strcmp(k, "NET_UNITS"))      cfg_net_bits = strcmp(v, "bytes") != 0;
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
}

/* atomic write (tmp + rename); called only when a value actually changed */
static void settings_save(void){
    mkdir(CFG_DIR1, 0755); mkdir(CFG_DIR2, 0755);
    char tmp[sizeof CFG_PATH + 8];
    snprintf(tmp, sizeof tmp, "%s.tmp", CFG_PATH);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    fprintf(f, "BRIGHTNESS=%d\nINTERVAL=%d\nROTATE=%d\n"
               "SCREEN_OFF_MIN=%d\nNIGHT=%d\nLEDS=%d\nPRIMARY_IFACE=%s\nNET_UNITS=%s\n"
               "COL_ACCENT=%06x\nCOL_GRAD_A=%06x\nCOL_GRAD_B=%06x\nCOL_BG=%06x\n"
               "COL_CARD=%06x\nCOL_TEXT=%06x\nCOL_DIM=%06x\n"
               "COL_OK=%06x\nCOL_WARN=%06x\nCOL_BAD=%06x\n",
            cfg_brightness, cfg_interval, cfg_rotate,
            cfg_screen_off, cfg_night, cfg_leds, cfg_primary_if,
            cfg_net_bits ? "bits" : "bytes",
            (unsigned)UN_ORANGE_M, (unsigned)UN_RED, (unsigned)UN_ORANGE, (unsigned)UN_BLACK,
            (unsigned)UN_GREY_80, (unsigned)UN_TEXT, (unsigned)UN_DIM,
            (unsigned)UN_OK, (unsigned)UN_WARN, (unsigned)UN_BAD);
    fclose(f);
    rename(tmp, CFG_PATH);
}


#endif /* PANEL_PREFS_H */
