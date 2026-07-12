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

/* ---------- text via stb_easy_font (unchanged from v1) ---------- */
static char qbuf[64000];
static void text(int x, int y, float scale, uint32_t c, const char *s){
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
static int text_w(float scale, const char *s){
    return (int)(stb_easy_font_width((char*)s) * scale);
}
static void text_c(int y, float scale, uint32_t c, const char *s){ /* centered */
    text((W - text_w(scale, s)) / 2, y, scale, c, s);
}
static void text_bold(int x, int y, float s, uint32_t c, const char *str){
    text(x, y, s, c, str);
    text(x + 1, y, s, c, str);
}
static void trunc_fit(char *s, float scale, int maxw){
    size_t n = strlen(s);
    while (n > 0 && text_w(scale, s) > maxw) s[--n] = 0;
}

/* ---------- Unraid brand palette ---------- */
#define UN_RED       0xe22828   /* signature gradient start (red)      */
#define UN_ORANGE    0xff8c2f   /* signature gradient end (orange)     */
#define UN_ORANGE_M  0xf15a2c   /* midpoint — single-colour accent     */
#define UN_BLACK     0x1b1b1b   /* webGUI black-theme background       */
#define UN_GREY_90   0x141414
#define UN_GREY_80   0x262626   /* card fill                           */
#define UN_GREY_70   0x333333   /* dividers / card top edge            */
#define UN_TEXT      0xf2f2f2
#define UN_DIM       0x999999
#define UN_OK        0x3fb950   /* array STARTED                       */
#define UN_WARN      0xf0a020
#define UN_BAD       0xe22828

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
