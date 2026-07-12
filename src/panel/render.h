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

/* offscreen: render every page to a PNG in `dir` (no DRM / no panel needed).
 * Used by --shot to produce README screenshots on the box with live stats. */
static int write_shots(const char *dir, const char *bgpath){
    W = 258; H = 960; fbpitch = W;
    fbmem = malloc((size_t)W * H * 4);
    if (!fbmem){ fprintf(stderr, "shot: oom\n"); return 1; }
    make_bg(bgpath);

    stats_t st; memset(&st, 0, sizeof st);
    read_cpu(&st); read_net(&st); read_gpu(&st); read_npu(&st); read_power(&st);
    usleep(400 * 1000);                              /* let deltas settle */
    read_cpu(&st);  read_mem(&st);  read_net(&st);  read_disk(&st);
    read_temp(&st); read_misc(&st); read_disks(&st); read_gpu(&st);
    read_npu(&st);  read_fans(&st); read_zones(&st); read_docker(&st);
    read_power(&st); read_about(&st); read_notif(&st);
    for (int i = 0; i < SPARK_N; i++)               /* seed sparklines */
        hist_push(st.cpu, st.gpu_avail ? st.gpu_busy : 0);

    unsigned char *rgba = malloc((size_t)W * H * 4);
    if (!rgba){ free(fbmem); return 1; }
    for (int p = 0; p < NPAGES; p++){
        cur_page = p; scrolly[p] = 0;
        render(&st);
        for (int i = 0; i < W * H; i++){            /* XRGB8888 -> RGBA8888 */
            uint32_t c = fbmem[i];
            rgba[i * 4 + 0] = (c >> 16) & 0xff;
            rgba[i * 4 + 1] = (c >>  8) & 0xff;
            rgba[i * 4 + 2] =  c        & 0xff;
            rgba[i * 4 + 3] = 255;
        }
        char nm[32]; snprintf(nm, sizeof nm, "%s", pages[p].title);
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


#endif /* PANEL_RENDER_H */
