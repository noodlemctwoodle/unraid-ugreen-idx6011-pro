/*
 * plugin/src/panel/render.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * full-frame compositor (background + page body + chrome)
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_RENDER_H
#define PANEL_RENDER_H

/* ---------- render ---------- */
static void render(stats_t *st){
    g_anim = 0;                                       /* a live card re-arms this while drawing */
    g_text_shadow = g_has_wallpaper;                  /* legibility drop shadow only over a wallpaper */
    /* effective card opacity this frame: a per-page override wins over the global
     * theme value (in preview mode the JS already folded that into cfg_card_opacity). */
    g_card_opacity_eff = cfg_card_opacity;
    if (!g_is_preview && cur_page >= 0 && cur_page < g_ncpage && g_cpage[cur_page].card_opacity >= 10)
        g_card_opacity_eff = g_cpage[cur_page].card_opacity;
    if (g_card_opacity_eff < 10)  g_card_opacity_eff = 10;
    if (g_card_opacity_eff > 100) g_card_opacity_eff = 100;
    for (int y = 0; y < H; y++)
        memcpy(fbmem + y * fbpitch, bg + y * W, (size_t)W * 4);
    hdr_bottom = header_h();                          /* title card grows with heading size */
    int base = hdr_bottom + (st->notif_count > 0 ? gy(44) : 0);
    int botlim = page_show_dots(cur_page) ? FOOTER_Y : H;   /* dots hidden -> content fills to the bottom */
    int visible = botlim - 12 - base;
    int maxs = content_h[cur_page] - visible;
    if (maxs < 0) maxs = 0;
    if (scrolly[cur_page] > maxs) scrolly[cur_page] = maxs;
    if (scrolly[cur_page] < 0)    scrolly[cur_page] = 0;
    body_top = base - scrolly[cur_page];
    page_end = body_top;
    g_clip_top = base; g_clip_bot = botlim - 4;             /* clip the body to the content band */
    if (g_is_preview)                                        /* web preview: a draft page is ALWAYS content
                                                              * (never the settings page it may index-collide
                                                              * with); an empty layout draws an empty page */
        render_modules(g_preview_layout ? g_preview_layout : "", st);
    else if (is_settings_page(cur_page)) page_settings(st);  /* built-in interactive page */
    else render_modules(g_cpage[cur_page].layout, st);       /* user content page */
    g_clip_top = 0; g_clip_bot = 1 << 20;                   /* header/footer draw unclipped */
    content_h[cur_page] = page_end - body_top;
    draw_header(st);
    if (st->notif_count > 0) draw_banner(st);
    draw_footer();
}

/* offscreen: render every page to a PNG in `dir` (no DRM / no panel needed).
 * Used by --shot to produce README screenshots on the box with live stats. */
static int write_shots(const char *dir, const char *bgpath){
    W = 258; H = 960; fbpitch = W;
    fbmem = malloc((size_t)W * H * 4);
    if (!fbmem){ fprintf(stderr, "shot: oom\n"); return 1; }
    make_bg(resolve_wallpaper(bgpath));
    load_logo(resolve_logo());

    stats_t st; memset(&st, 0, sizeof st);
    read_cpu(&st); read_net(&st); read_gpu(&st); read_npu(&st); read_power(&st);
    usleep(400 * 1000);                              /* let deltas settle */
    read_cpu(&st);  read_mem(&st);  read_net(&st);  read_disk(&st);
    read_temp(&st); read_misc(&st); read_disks(&st); read_gpu(&st);
    read_npu(&st);  read_fans(&st); read_fan_rpm(&st); read_zones(&st); read_docker(&st);
    read_power(&st); read_about(&st); read_notif(&st); read_updates(&st); read_transfer(&st);
    hist_seed_demo(&st);                            /* representative graph history for shots */

    unsigned char *rgba = malloc((size_t)W * H * 4);
    if (!rgba){ free(fbmem); return 1; }
    for (int p = 0; p < n_total(); p++){
        cur_page = p; scrolly[p] = 0;
        render(&st);
        for (int i = 0; i < W * H; i++){            /* XRGB8888 -> RGBA8888 */
            uint32_t c = fbmem[i];
            rgba[i * 4 + 0] = (c >> 16) & 0xff;
            rgba[i * 4 + 1] = (c >>  8) & 0xff;
            rgba[i * 4 + 2] =  c        & 0xff;
            rgba[i * 4 + 3] = 255;
        }
        char nm[32]; snprintf(nm, sizeof nm, "%s", page_title(p));
        for (char *q = nm; *q; q++) *q = (char)tolower((unsigned char)*q);
        char path[512]; snprintf(path, sizeof path, "%s/%d-%s.png", dir, p + 1, nm);
        if (stbi_write_png(path, W, H, 4, rgba, W * 4))
            fprintf(stderr, "shot: %s\n", path);
        else
            fprintf(stderr, "shot: FAILED %s\n", path);
    }
    free(rgba); free(fbmem);
    return 0;
}

/* offscreen: render ONE page with a DRAFT layout to a PNG — the web layout editor's
 * live preview. Uses live stats + the current theme + the wallpaper if present, so
 * the preview is pixel-identical to what the panel will show. */
static int write_preview(int page, const char *layout, const char *outfile){
    W = 258; H = 960; fbpitch = W;
    fbmem = malloc((size_t)W * H * 4);
    if (!fbmem){ fprintf(stderr, "preview: oom\n"); return 1; }
    make_bg(resolve_wallpaper(NULL));
    load_logo(resolve_logo());

    stats_t st; memset(&st, 0, sizeof st);
    read_cpu(&st); read_net(&st); read_gpu(&st); read_npu(&st); read_power(&st);
    usleep(400 * 1000);
    read_cpu(&st);  read_mem(&st);  read_net(&st);  read_disk(&st);
    read_temp(&st); read_misc(&st); read_disks(&st); read_gpu(&st);
    read_npu(&st);  read_fans(&st); read_fan_rpm(&st); read_zones(&st); read_docker(&st);
    read_power(&st); read_about(&st); read_notif(&st); read_updates(&st); read_transfer(&st);
    hist_seed_demo(&st);

    if (page < 0) page = 0;
    if (page >= MAX_CPAGES) page = MAX_CPAGES - 1;     /* any draft page index (rendered as content) */
    g_is_preview = 1;                                  /* draft page: content, draft name/position */
    { const char *e;                                   /* draft per-page chrome toggles + identity */
      if ((e = getenv("PANEL_PAGE_HEADER"))) g_prev_header = atoi(e) != 0;
      if ((e = getenv("PANEL_PAGE_TITLE")))  g_prev_title  = atoi(e) != 0;
      if ((e = getenv("PANEL_PAGE_DOTS")))   g_prev_dots   = atoi(e) != 0;
      if ((e = getenv("PANEL_PAGE_NAME")))   snprintf(g_preview_name, sizeof g_preview_name, "%s", e);
      if ((e = getenv("PANEL_PAGE_POS")))    g_preview_pos = atoi(e);
      if ((e = getenv("PANEL_PAGE_TOTAL")))  g_preview_total = atoi(e);
    }
    g_preview_layout = (layout && *layout) ? layout : NULL;
    cur_page = page; scrolly[page] = 0;
    render(&st);
    const char *sc = getenv("PANEL_SCROLL");                 /* debug: preview a scrolled frame */
    if (sc && *sc){ scrolly[page] = atoi(sc); render(&st); }
    g_preview_layout = NULL;

    unsigned char *rgba = malloc((size_t)W * H * 4);
    if (!rgba){ free(fbmem); return 1; }
    for (int i = 0; i < W * H; i++){
        uint32_t c = fbmem[i];
        rgba[i * 4 + 0] = (c >> 16) & 0xff;
        rgba[i * 4 + 1] = (c >>  8) & 0xff;
        rgba[i * 4 + 2] =  c        & 0xff;
        rgba[i * 4 + 3] = 255;
    }
    int ok = stbi_write_png(outfile, W, H, 4, rgba, W * 4);
    fprintf(stderr, "preview: %s %s\n", ok ? "wrote" : "FAILED", outfile);
    free(rgba); free(fbmem);
    return ok ? 0 : 1;
}


#endif /* PANEL_RENDER_H */
