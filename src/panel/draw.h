/*
 * plugin/src/panel/draw.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * framebuffer, text, Unraid brand palette, gradients, logo, background
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_DRAW_H
#define PANEL_DRAW_H

/* ---------- framebuffer (unchanged from v1) ---------- */
static int W = 258, H = 960;
static uint32_t *fbmem;            /* mapped dumb buffer */
static int fbpitch;                /* in pixels */
static uint32_t *bg;               /* prerendered background */
static volatile sig_atomic_t stop_flag;

static void on_sig(int s){ (void)s; stop_flag = 1; }

static inline void px(int x, int y, uint32_t c){
    if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H)
        fbmem[y * fbpitch + x] = c;
}
static inline uint32_t blend(uint32_t dst, uint32_t src, uint8_t a){
    uint32_t rb = ((src & 0xff00ff) * a + (dst & 0xff00ff) * (255 - a)) >> 8;
    uint32_t g  = ((src & 0x00ff00) * a + (dst & 0x00ff00) * (255 - a)) >> 8;
    return (rb & 0xff00ff) | (g & 0x00ff00);
}
static void rect(int x, int y, int w, int h, uint32_t c, uint8_t a){
    if (x < 0){ w += x; x = 0; } if (y < 0){ h += y; y = 0; }
    if (x + w > W) w = W - x; if (y + h > H) h = H - y;
    for (int j = 0; j < h; j++){
        uint32_t *row = fbmem + (y + j) * fbpitch + x;
        if (a == 255) for (int i = 0; i < w; i++) row[i] = c;
        else          for (int i = 0; i < w; i++) row[i] = blend(row[i], c, a);
    }
}
static void hline(int x, int y, int w, uint32_t c){ rect(x, y, w, 1, c, 255); }

/* ---------- text ----------
 * Real anti-aliased type via stb_truetype when a TTF is loaded (font_init);
 * falls back to the vendored stb_easy_font vector font if none. A real font
 * gives full glyph coverage (em-dash, degree sign, arrows) and proper shapes.
 * The `scale` API is preserved: it maps to a pixel height so the existing
 * per-card y-offsets and text_w()-based alignment keep working unchanged. */

/* stb_easy_font fallback (the original v1 renderer) */
static char qbuf[64000];
static void text_easy(int x, int y, float scale, uint32_t c, const char *s){
    int n = stb_easy_font_print(0, 0, (char*)s, NULL, qbuf, sizeof(qbuf));
    float *v = (float*)qbuf;                    /* 16B/vertex: x,y,z,color */
    for (int q = 0; q < n; q++){
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (int k = 0; k < 4; k++){
            float vx = v[(q * 4 + k) * 4 + 0], vy = v[(q * 4 + k) * 4 + 1];
            if (vx < x0) x0 = vx; if (vx > x1) x1 = vx;
            if (vy < y0) y0 = vy; if (vy > y1) y1 = vy;
        }
        int rx = x + (int)floorf(x0 * scale), ry = y + (int)floorf(y0 * scale);
        int rw = (int)ceilf((x1 - x0) * scale), rh = (int)ceilf((y1 - y0) * scale);
        if (rw < 1) rw = 1; if (rh < 1) rh = 1;
        rect(rx, ry, rw, rh, c, 255);
    }
}
static int text_w_easy(float scale, const char *s){
    return (int)(stb_easy_font_width((char*)s) * scale);
}

/* ---- stb_truetype path ---- */
static stbtt_fontinfo g_font;
static int   g_font_ok = 0;
static unsigned char *g_font_data = NULL;
static float FONT_K = 9.0f;               /* scale unit -> pixel height (tunable) */
static int   g_font_asc, g_font_desc, g_font_gap;

/* load a TTF from disk; falls back silently (g_font_ok stays 0) on any error. */
static int font_init(const char *path){
    if (!path || !*path) return 0;
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0){ fclose(f); return 0; }
    g_font_data = (unsigned char*)malloc((size_t)n);
    if (!g_font_data){ fclose(f); return 0; }
    size_t rd = fread(g_font_data, 1, (size_t)n, f); fclose(f);
    if (rd != (size_t)n){ free(g_font_data); g_font_data = NULL; return 0; }
    int off = stbtt_GetFontOffsetForIndex(g_font_data, 0);
    if (off < 0 || !stbtt_InitFont(&g_font, g_font_data, off)){
        free(g_font_data); g_font_data = NULL; return 0;
    }
    stbtt_GetFontVMetrics(&g_font, &g_font_asc, &g_font_desc, &g_font_gap);
    const char *k = getenv("PANEL_FONT_K"); if (k && *k) FONT_K = (float)atof(k);
    g_font_ok = 1;
    return 1;
}

/* minimal UTF-8 decode: returns next codepoint, advances *p past it */
static unsigned utf8_next(const char **p){
    const unsigned char *s = (const unsigned char*)*p;
    unsigned c = s[0];
    if (c < 0x80){ *p += 1; return c; }
    if ((c >> 5) == 0x6  && s[1]){ *p += 2; return ((c & 0x1f) << 6) | (s[1] & 0x3f); }
    if ((c >> 4) == 0xe  && s[1] && s[2]){ *p += 3;
        return ((c & 0x0f) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f); }
    if ((c >> 3) == 0x1e && s[1] && s[2] && s[3]){ *p += 4;
        return ((c & 0x07) << 18) | ((s[1] & 0x3f) << 12) | ((s[2] & 0x3f) << 6) | (s[3] & 0x3f); }
    *p += 1; return c;                        /* invalid lead byte: skip one */
}

/* glyph cache: rasterize each (codepoint, pixel-size) once and reuse. The panel
 * redraws every interval; without this it re-rasterizes every glyph every frame
 * (~5% of a core). The set of (glyph,size) pairs is small and bounded, so cached
 * bitmaps are never freed. */
typedef struct { unsigned cp; int px, used; int w, h, xoff, yoff; float adv; unsigned char *bmp; } glyph_t;
#define GLYPH_CACHE 4096
static glyph_t g_glyphs[GLYPH_CACHE];

static glyph_t *glyph_get(unsigned cp, int px, float sc){
    unsigned h = (cp * 131u + (unsigned)px * 2654435761u) & (GLYPH_CACHE - 1);
    for (int probe = 0; probe < GLYPH_CACHE; probe++){
        glyph_t *g = &g_glyphs[(h + probe) & (GLYPH_CACHE - 1)];
        if (g->used){ if (g->cp == cp && g->px == px) return g; continue; }
        int adv, lsb; stbtt_GetCodepointHMetrics(&g_font, (int)cp, &adv, &lsb);
        g->bmp = stbtt_GetCodepointBitmap(&g_font, sc, sc, (int)cp, &g->w, &g->h, &g->xoff, &g->yoff);
        g->adv = adv * sc; g->cp = cp; g->px = px; g->used = 1;
        return g;
    }
    return NULL;                                   /* cache full (bounded set: never) */
}
static void px_scale(float scale, int *px, float *sc){
    int p = (int)(scale * FONT_K + 0.5f); if (p < 1) p = 1;
    *px = p; *sc = stbtt_ScaleForPixelHeight(&g_font, (float)p);
}

static int text_w_ttf(float scale, const char *s){
    int px; float sc; px_scale(scale, &px, &sc);
    float w = 0; const char *p = s; unsigned prev = 0;
    while (*p){
        unsigned cp = utf8_next(&p);
        if (prev) w += stbtt_GetCodepointKernAdvance(&g_font, (int)prev, (int)cp) * sc;
        glyph_t *g = glyph_get(cp, px, sc); if (g) w += g->adv; prev = cp;
    }
    return (int)(w + 0.5f);
}
static void text_ttf(int x, int y, float scale, uint32_t c, const char *s){
    int px; float sc; px_scale(scale, &px, &sc);
    int baseline = y + (int)(g_font_asc * sc + 0.5f);
    float penx = (float)x; const char *p = s; unsigned prev = 0;
    while (*p){
        unsigned cp = utf8_next(&p);
        if (prev) penx += stbtt_GetCodepointKernAdvance(&g_font, (int)prev, (int)cp) * sc;
        glyph_t *g = glyph_get(cp, px, sc);
        if (g && g->bmp){
            int gx = (int)(penx + 0.5f) + g->xoff, gy = baseline + g->yoff;
            for (int j = 0; j < g->h; j++){
                int py = gy + j; if ((unsigned)py >= (unsigned)H) continue;
                uint32_t *drow = fbmem + py * fbpitch;
                const unsigned char *brow = g->bmp + j * g->w;
                for (int i = 0; i < g->w; i++){
                    int pxx = gx + i; if ((unsigned)pxx >= (unsigned)W) continue;
                    uint8_t a = brow[i]; if (!a) continue;
                    drow[pxx] = (a == 255) ? c : blend(drow[pxx], c, a);
                }
            }
        }
        if (g) penx += g->adv; prev = cp;
    }
}

/* raw renderer (no user size multiplier) — used for fixed header chrome */
static void text_raw(int x, int y, float scale, uint32_t c, const char *s){
    if (g_font_ok) text_ttf(x, y, scale, c, s);
    else           text_easy(x, y, scale, c, s);
}
static int text_w_raw(float scale, const char *s){
    return g_font_ok ? text_w_ttf(scale, s) : text_w_easy(scale, s);
}

/* user-tunable size multipliers (Theme settings): body vs section headings.
 * text()/text_w() scale body content; htext()/htext_w() scale titles. */
static float g_text_scale = 1.0f;
static float g_head_scale = 1.0f;

static void text(int x, int y, float scale, uint32_t c, const char *s){ text_raw(x, y, scale * g_text_scale, c, s); }
static int  text_w(float scale, const char *s){ return text_w_raw(scale * g_text_scale, s); }
static void htext(int x, int y, float scale, uint32_t c, const char *s){ text_raw(x, y, scale * g_head_scale, c, s); }
static int  htext_w(float scale, const char *s){ return text_w_raw(scale * g_head_scale, s); }

static void text_c(int y, float scale, uint32_t c, const char *s){ /* centered body */
    text((W - text_w(scale, s)) / 2, y, scale, c, s);
}
static void htext_c(int y, float scale, uint32_t c, const char *s){ /* centered heading */
    htext((W - htext_w(scale, s)) / 2, y, scale, c, s);
}
static void text_bold(int x, int y, float s, uint32_t c, const char *str){
    text(x, y, s, c, str);
    text(x + 1, y, s, c, str);
}
static void trunc_fit(char *s, float scale, int maxw){
    size_t n = strlen(s);
    while (n > 0 && text_w(scale, s) > maxw) s[--n] = 0;
}

/* ---------- brand palette (runtime — overridable from settings.cfg) ----------
 * These default to the Unraid palette; settings_load() (prefs.h) may recolour
 * them from the config page. Not #defines so they can be themed at runtime. */
static uint32_t UN_RED      = 0xe22828;  /* signature gradient start (red)  */
static uint32_t UN_ORANGE   = 0xff8c2f;  /* signature gradient end (orange) */
static uint32_t UN_ORANGE_M = 0xf15a2c;  /* single-colour accent (spine/gauge) */
static uint32_t UN_BLACK    = 0x1b1b1b;  /* background                      */
static uint32_t UN_GREY_90  = 0x141414;
static uint32_t UN_GREY_80  = 0x262626;  /* card fill                       */
static uint32_t UN_GREY_70  = 0x333333;  /* dividers / card top edge        */
static uint32_t UN_TEXT     = 0xf2f2f2;  /* primary text                    */
static uint32_t UN_DIM      = 0x999999;  /* dim/label text                  */
static uint32_t UN_OK       = 0x3fb950;  /* healthy / array STARTED         */
static uint32_t UN_WARN     = 0xf0a020;  /* warning                         */
static uint32_t UN_BAD      = 0xe22828;  /* error / critical                */

static uint32_t lerp_rgb(uint32_t a, uint32_t b, float t){
    if (t < 0) t = 0; if (t > 1) t = 1;
    int r  = (int)((a >> 16 & 255) + t * ((float)(b >> 16 & 255) - (float)(a >> 16 & 255)));
    int g  = (int)((a >>  8 & 255) + t * ((float)(b >>  8 & 255) - (float)(a >>  8 & 255)));
    int bl = (int)((a       & 255) + t * ((float)(b       & 255) - (float)(a       & 255)));
    return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)bl;
}

/* horizontal red->orange gradient strip — the signature Unraid rule */
static void hgrad(int x, int y, int w, int h, uint32_t c0, uint32_t c1){
    for (int i = 0; i < w; i++)
        rect(x + i, y, 1, h, lerp_rgb(c0, c1, w > 1 ? (float)i / (w - 1) : 0), 255);
}

/*
 * draw_unraid_logo — simplified geometric Unraid "glitch bars" mark.
 * Bounding box: 120*scale wide, 96*scale tall; (x,y) = top-left.
 */
static void draw_unraid_logo(int x, int y, float s){
    static const int up[7] = { 10, 30, 18, 48, 38, 14, 22 };
    static const int dn[7] = { 22, 14, 38, 48, 18, 30, 10 };
    int bw   = (int)(12 * s + 0.5f); if (bw < 2) bw = 2;
    int step = (int)(18 * s + 0.5f); if (step <= bw) step = bw + 1;
    int cy   = y + (int)(48 * s + 0.5f);
    int top  = y, bot = y + (int)(96 * s + 0.5f);
    if (bot <= top) bot = top + 1;
    for (int i = 0; i < 7; i++){
        int bx = x + i * step;
        int y0 = cy - (int)(up[i] * s + 0.5f);
        int y1 = cy + (int)(dn[i] * s + 0.5f);
        for (int yy = y0; yy < y1; yy++){
            uint32_t c = lerp_rgb(UN_ORANGE, UN_RED,
                                  (float)(yy - top) / (float)(bot - top));
            int in = (bw > 3 && (yy == y0 || yy == y1 - 1)) ? 1 : 0;
            rect(bx + in, yy, bw - 2 * in, 1, c, 255);
        }
    }
}

/* official Unraid mark: embedded PNG (unraid_logo.h) decoded once, blitted w/ alpha */
static void draw_unraid_icon(int x, int y){
    static int tried; static unsigned char *img; static int iw, ih;
    if (!tried){
        tried = 1;
        img = stbi_load_from_memory(unraid_logo_png, (int)unraid_logo_png_len,
                                    &iw, &ih, NULL, 4);
    }
    if (!img){ draw_unraid_logo(x, y, 0.5f); return; }   /* fallback to the drawn mark */
    for (int j = 0; j < ih; j++){
        int py = y + j; if ((unsigned)py >= (unsigned)H) continue;
        for (int i = 0; i < iw; i++){
            int pxx = x + i; if ((unsigned)pxx >= (unsigned)W) continue;
            unsigned char *p = img + (j * iw + i) * 4;
            uint8_t a = p[3]; if (!a) continue;
            uint32_t c = (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
            uint32_t *d = &fbmem[py * fbpitch + pxx];
            *d = (a == 255) ? c : blend(*d, c, a);
        }
    }
}

/* optional custom header logo (panel/logo.png) — scaled to fit the header mark area
 * (max 84x44), blitted with alpha. Returns 1 if drawn, 0 to fall back to the Unraid
 * mark. Loaded once; a panel restart picks up a newly-uploaded logo. */
static int draw_custom_logo(int x, int y){
    static int tried; static unsigned char *img; static int iw, ih;
    if (!tried){ tried = 1;
        img = stbi_load("/boot/config/plugins/ugreen-idx6011-pro/panel/logo.png", &iw, &ih, NULL, 4);
    }
    if (!img || iw <= 0 || ih <= 0) return 0;
    float sc = 1.0f;
    if (iw > 84) sc = 84.0f / iw;
    if (ih * sc > 44) sc = 44.0f / ih;
    int dw = (int)(iw * sc), dh = (int)(ih * sc);
    for (int j = 0; j < dh; j++){
        int py = y + j; if ((unsigned)py >= (unsigned)H) continue;
        int sy = (int)(j / sc); if (sy >= ih) sy = ih - 1;
        for (int i = 0; i < dw; i++){
            int pxx = x + i; if ((unsigned)pxx >= (unsigned)W) continue;
            int sx = (int)(i / sc); if (sx >= iw) sx = iw - 1;
            unsigned char *p = img + (sy * iw + sx) * 4;
            uint8_t a = p[3]; if (!a) continue;
            uint32_t c = (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
            uint32_t *d = &fbmem[py * fbpitch + pxx];
            *d = (a == 255) ? c : blend(*d, c, a);
        }
    }
    return 1;
}

/* ---------- background (v1 image path; brand-dark fallback) ---------- */
static void make_bg(const char *path){
    bg = malloc((size_t)W * H * 4);
    int iw, ih, comp; unsigned char *img = NULL;
    if (path) img = stbi_load(path, &iw, &ih, &comp, 3);
    if (img){
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++){
            int sx = x * iw / W, sy = y * ih / H;
            unsigned char *p = img + (sy * iw + sx) * 3;
            bg[y * W + x] = (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
        }
        stbi_image_free(img);
        fprintf(stderr, "background: %s (%dx%d)\n", path, iw, ih);
    } else {
        for (int y = 0; y < H; y++){            /* brand black -> near black */
            float t = (float)y / H;
            uint8_t g = (uint8_t)(27 - 14 * t);  /* 0x1b -> 0x0d */
            for (int x = 0; x < W; x++) bg[y * W + x] = (uint32_t)g << 16 | (uint32_t)g << 8 | g;
        }
    }
}


#endif /* PANEL_DRAW_H */
