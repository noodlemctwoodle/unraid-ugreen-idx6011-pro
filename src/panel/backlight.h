/*
 * plugin/src/panel/backlight.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * EC + i915 backlight control (dimming, screen-off)
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_BACKLIGHT_H
#define PANEL_BACKLIGHT_H

/* ---------- backlight (v1 core + bl_power for screen-off) ----------
 * Brightness on this panel is EC-controlled (ITE IT55xx, EC-RAM 0xA5: 1 = full
 * ... 198 = off — see ec.h). The i915 intel_backlight PWM only gates on/off, so
 * levels are written to the EC; intel_backlight is pinned to max + unblanked so
 * the EC is the sole dimmer. */
static void set_backlight(int pct){
    const char *base = "/sys/class/backlight/intel_backlight";
    char p[256]; long max = 0;
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    /* EC: the real dimmer. 100% -> 1, 1% -> ~196, 0% -> 198 (off) */
    unsigned char v = pct == 0 ? 198 : (unsigned char)(1 + (100 - pct) * 197 / 100);
    ec_write(0xA5, v);
    /* i915 PWM pinned to max + unblanked */
    snprintf(p, sizeof(p), "%s/max_brightness", base);
    FILE *f = fopen(p, "r"); if (!f) return;
    if (fscanf(f, "%ld", &max) != 1) max = 0;
    fclose(f);
    if (max <= 0) return;
    snprintf(p, sizeof(p), "%s/brightness", base);
    f = fopen(p, "w"); if (!f) return;
    fprintf(f, "%ld", max);
    fclose(f);
    snprintf(p, sizeof(p), "%s/bl_power", base);
    f = fopen(p, "w"); if (f){ fputs("0", f); fclose(f); }
}
/* FB_BLANK: 0 = unblank (on), 4 = powerdown (off) */
static void set_bl_power(int on){
    FILE *f = fopen("/sys/class/backlight/intel_backlight/bl_power", "w");
    if (f){ fputs(on ? "0" : "4", f); fclose(f); }
}
static int eff_brightness(void){
    int b = cfg_brightness;
    if (cfg_night && b > 15) b = 15;
    return b;
}
static void apply_brightness(void){ set_backlight(eff_brightness()); }


#endif /* PANEL_BACKLIGHT_H */
