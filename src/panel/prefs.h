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
               "SCREEN_OFF_MIN=%d\nNIGHT=%d\nLEDS=%d\n",
            cfg_brightness, cfg_interval, cfg_rotate,
            cfg_screen_off, cfg_night, cfg_leds);
    fclose(f);
    rename(tmp, CFG_PATH);
}


#endif /* PANEL_PREFS_H */
