/*
 * plugin/src/panel/pages/settings.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * SETTINGS page — rows, buttons, tap/long-press actions
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_SETTINGS_H
#define PANEL_PAGE_SETTINGS_H

/* one tap-to-cycle settings row: label left, value right, whole card tappable */
static int settings_row(int y, int wid, const char *label, const char *val){
    card(y, 52, NULL);
    text(22, y + 18, 2.0f, UN_TEXT, label);
    text(W - 22 - text_w(2.0f, val), y + 18, 2.0f, UN_ORANGE, val);
    hb_add(wid, 10, y, W - 20, 52);
    return y + 62;
}

/* single- or two-line action button (l2 != NULL => taller, subtitle on its own line) */
static int settings_button(int y, int wid, const char *l1, const char *l2,
                           uint32_t fill, uint32_t txt_col){
    int two = (l2 && l2[0]);
    int h = two ? 90 : 76;
    rect(10, y, W - 20, h, fill, 255);
    rect(10, y, W - 20, 1, UN_GREY_70, 255);
    if (two){
        text_bold((W - text_w(2.3f, l1)) / 2 - 1, y + 20, 2.3f, txt_col, l1);
        text_c(y + 58, 1.7f, txt_col, l2);
    } else {
        text_bold((W - text_w(2.4f, l1)) / 2 - 1, y + 28, 2.4f, txt_col, l1);
    }
    hb_add(wid, 10, y - 6, W - 20, h + 12);   /* padded into the gaps for easier tapping */
    return y + h + 14;
}

static void page_settings(stats_t *st){
    (void)st;
    char b[64];
    int y = body_top;
    hb_n = 0;
    long nowms = now_ms();

    /* brightness slider */
    card(y, 84, "BRIGHTNESS");
    snprintf(b, sizeof b, "%d%%", cfg_brightness);
    text(W - 22 - text_w(1.9f, b), y + 10, 1.9f, UN_ORANGE, b);
    rect(SLIDER_X, y + 42, SLIDER_W, 24, 0x2a2a2a, 255);
    int fw = SLIDER_W * cfg_brightness / 100;
    hgrad(SLIDER_X, y + 42, fw, 24, UN_RED, UN_ORANGE);
    rect(SLIDER_X + fw - 2, y + 38, 4, 32, UN_TEXT, 255);
    hb_add(WID_BRIGHT, 10, y + 30, W - 20, 48);
    y += 96;

    /* tap-to-cycle rows */
    if (cfg_screen_off == 0) snprintf(b, sizeof b, "never");
    else                     snprintf(b, sizeof b, "%d min", cfg_screen_off);
    y = settings_row(y, WID_SCROFF, "Screen off", b);

    if (cfg_rotate == 0) snprintf(b, sizeof b, "off");
    else                 snprintf(b, sizeof b, "%ds", cfg_rotate);
    y = settings_row(y, WID_ROTATE, "Auto-rotate", b);

    y = settings_row(y, WID_LEDS,  "LEDs",       cfg_leds  ? "on" : "off");
    y = settings_row(y, WID_NIGHT, "Night mode", cfg_night ? "on" : "off");
    y += 8;

    y = settings_button(y, WID_RESTART, "RESTART DASH", NULL, UN_GREY_80, UN_TEXT);

    if (confirm_which == WID_REBOOT && nowms < confirm_until){
        snprintf(b, sizeof b, "%lds", (confirm_until - nowms) / 1000 + 1);
        y = settings_button(y, WID_REBOOT, "TAP TO CONFIRM", b, UN_BAD, UN_TEXT);
    } else {
        y = settings_button(y, WID_REBOOT, "REBOOT", "hold to confirm", UN_GREY_80, UN_ORANGE);
    }
    if (confirm_which == WID_SHUTDOWN && nowms < confirm_until){
        snprintf(b, sizeof b, "%lds", (confirm_until - nowms) / 1000 + 1);
        y = settings_button(y, WID_SHUTDOWN, "TAP TO CONFIRM", b, UN_BAD, UN_TEXT);
    } else {
        y = settings_button(y, WID_SHUTDOWN, "SHUTDOWN", "hold to confirm", UN_GREY_80, UN_ORANGE);
    }
    y += 4;
    text_c(y, 1.4f, 0x666666, "hold, then tap to confirm");
    page_end = y + 22;
}

/* ---------- settings widget actions ---------- */
static int widget_tap(int x, int y){
    int id = hb_find(x, y);
    switch (id){
    case WID_BRIGHT: {
        int pct = (x - SLIDER_X) * 100 / (SLIDER_W > 0 ? SLIDER_W : 1);
        if (pct < 5) pct = 5; if (pct > 100) pct = 100;
        if (pct != cfg_brightness){
            cfg_brightness = pct;
            apply_brightness();
            settings_save();
        }
        return 1;
    }
    case WID_SCROFF:
        cfg_screen_off = cfg_screen_off == 0 ? 1 : cfg_screen_off == 1 ? 5
                       : cfg_screen_off == 5 ? 15 : 0;
        settings_save();
        return 1;
    case WID_ROTATE:
        cfg_rotate = cfg_rotate == 0 ? 10 : cfg_rotate == 10 ? 20
                   : cfg_rotate == 20 ? 60 : 0;
        rotate_ms = (long)cfg_rotate * 1000L;
        settings_save();
        return 1;
    case WID_LEDS:
        cfg_leds = !cfg_leds;
        apply_leds();
        settings_save();
        return 1;
    case WID_NIGHT:
        cfg_night = !cfg_night;
        apply_brightness();
        settings_save();
        return 1;
    case WID_RESTART:
        do_restart_dash();
        return 1;
    case WID_REBOOT:
        if (confirm_which == WID_REBOOT && now_ms() < confirm_until){
            confirm_which = 0;
            do_reboot();
        }
        return 1;
    case WID_SHUTDOWN:
        if (confirm_which == WID_SHUTDOWN && now_ms() < confirm_until){
            confirm_which = 0;
            do_shutdown();
        }
        return 1;
    default:
        return 0;
    }
}

static int widget_long(int x, int y){
    int id = hb_find(x, y);
    if (id == WID_REBOOT || id == WID_SHUTDOWN){
        confirm_which = id;
        confirm_until = now_ms() + 5000;
        return 1;
    }
    return 0;
}


#endif /* PANEL_PAGE_SETTINGS_H */
