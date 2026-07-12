/*
 * plugin/src/panel/render.h
 *
 * Created by Toby G on 12/07/2026.
 *
 * full-frame compositor (background + page body + chrome)
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_RENDER_H
#define PANEL_RENDER_H

/* ---------- render ---------- */
static void render(stats_t *st){
    for (int y = 0; y < H; y++)
        memcpy(fbmem + y * fbpitch, bg + y * W, (size_t)W * 4);
    int base = BODY_Y0 + (st->notif_count > 0 ? 44 : 0);
    int visible = FOOTER_Y - 12 - base;
    int maxs = content_h[cur_page] - visible;
    if (maxs < 0) maxs = 0;
    if (scrolly[cur_page] > maxs) scrolly[cur_page] = maxs;
    if (scrolly[cur_page] < 0)    scrolly[cur_page] = 0;
    body_top = base - scrolly[cur_page];
    page_end = body_top;
    pages[cur_page].body(st);
    content_h[cur_page] = page_end - body_top;
    draw_header(st);
    if (st->notif_count > 0) draw_banner(st);
    draw_footer();
}


#endif /* PANEL_RENDER_H */
