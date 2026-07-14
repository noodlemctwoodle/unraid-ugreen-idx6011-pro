/*
 * plugin/src/panel/modules/mod_spacer.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Spacer — an empty, transparent gap that just advances the layout. Useful for
 * pushing content down so the wallpaper shows at the top, or opening a gap
 * between cards. Draws nothing (the background / wallpaper shows through).
 * Variants: 0 small, 1 medium, 2 big. Heights scale with the text size, like
 * cards, so spacing stays proportional.
 */
#ifndef PANEL_MOD_SPACER_H
#define PANEL_MOD_SPACER_H

static int mod_spacer(int y, stats_t *st, int variant){
    (void)y; (void)st;
    int h = variant == 2 ? 184 : variant == 1 ? 96 : 48;   /* big / medium / small */
    return gy(h);
}

#endif /* PANEL_MOD_SPACER_H */
