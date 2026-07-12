/*
 * plugin/src/panel/ui.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * sparkline history + drawing widgets (bars, ring gauge, cards, tiles)
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_UI_H
#define PANEL_UI_H

/* one font size for every card/tile section title, across all pages */
#define TITLE_SCALE 1.9f

/* ---------- sparkline history (60 samples, pushed each stats tick) ---------- */
#define SPARK_N 60
static float h_cpu[SPARK_N], h_gpu[SPARK_N];
static int h_cnt, h_pos;
static void hist_push(double cpu, double gpu){
    h_cpu[h_pos] = (float)cpu; h_gpu[h_pos] = (float)gpu;
    h_pos = (h_pos + 1) % SPARK_N;
    if (h_cnt < SPARK_N) h_cnt++;
}

/* ---------- UI primitives ---------- */
static void bar(int x, int y, int w, int h, double pct, uint32_t col){
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    rect(x, y, w, h, 0x2a2a2a, 255);
    rect(x, y, (int)(w * pct / 100.0), h, col, 255);
}
static uint32_t level_col(double pct){
    return pct > 90 ? UN_BAD : pct > 75 ? UN_WARN : UN_ORANGE_M;
}
/* Network rates are shown in bits/s (Kbps/Mbps/Gbps) to match Unraid and the usual
 * network convention. Input kbs is KiB/s (bytes/s / 1024); * 8.192 => decimal kb/s. */
static void fmt_rate(char *out, size_t n, double kbs){
    if (cfg_net_bits){                          /* bits: Kbps/Mbps/Gbps (matches Unraid) */
        double kbps = kbs * 8.192;
        if      (kbps >= 1000000) snprintf(out, n, "%.2f Gbps", kbps / 1000000);
        else if (kbps >= 1000)    snprintf(out, n, "%.1f Mbps", kbps / 1000);
        else                      snprintf(out, n, "%.0f Kbps", kbps);
    } else {                                    /* bytes: KB/s / MB/s / GB/s */
        if      (kbs >= 1048576.0) snprintf(out, n, "%.2f GB/s", kbs / 1048576.0);
        else if (kbs >= 1024.0)    snprintf(out, n, "%.1f MB/s", kbs / 1024.0);
        else                       snprintf(out, n, "%.0f KB/s", kbs);
    }
}
static void fmt_rate_s(char *out, size_t n, double kbs){   /* compact number, full unit */
    if (cfg_net_bits){
        double kbps = kbs * 8.192;
        if      (kbps >= 1000000) snprintf(out, n, "%.1f Gbps", kbps / 1000000);
        else if (kbps >= 1000)    snprintf(out, n, "%.1f Mbps", kbps / 1000);
        else                      snprintf(out, n, "%.0f Kbps", kbps);
    } else {
        if      (kbs >= 1048576.0) snprintf(out, n, "%.1f GB/s", kbs / 1048576.0);
        else if (kbs >= 1024.0)    snprintf(out, n, "%.1f MB/s", kbs / 1024.0);
        else                       snprintf(out, n, "%.0f KB/s", kbs);
    }
}
static void fmt_bytes(char *out, size_t n, unsigned long long b){
    double v = (double)b;
    if      (v >= 1e12) snprintf(out, n, "%.2f TB", v / 1e12);
    else if (v >= 1e9)  snprintf(out, n, "%.1f GB", v / 1e9);
    else if (v >= 1e6)  snprintf(out, n, "%.0f MB", v / 1e6);
    else                snprintf(out, n, "%.0f KB", v / 1e3);
}
static void fmt_up(char *b, size_t n, long up){
    long d = up / 86400, hh = up % 86400 / 3600, mm = up % 3600 / 60;
    if (d) snprintf(b, n, "up %ldd %ldh %ldm", d, hh, mm);
    else   snprintf(b, n, "up %ldh %ldm", hh, mm);
}
static void card(int y, int h, const char *title){
    rect(10, y, W - 20, h, UN_GREY_80, 230);
    rect(10, y, W - 20, 1, UN_GREY_70, 255);
    rect(10, y, 3, h, UN_ORANGE_M, 255);          /* webGUI-style orange spine */
    if (title) text(22, y + 10, TITLE_SCALE, UN_DIM, title);
}

/* ring gauge — filled annulus, clockwise from 12 o'clock; no libs */
static void ring_gauge(int cx, int cy, int ro, int ri, double pct, uint32_t col){
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    float lim = (float)(pct / 100.0 * 2.0 * M_PI);
    int ro2 = ro * ro, ri2 = ri * ri;
    for (int dy = -ro; dy <= ro; dy++)
        for (int dx = -ro; dx <= ro; dx++){
            int d2 = dx * dx + dy * dy;
            if (d2 > ro2 || d2 < ri2) continue;
            float ang = atan2f((float)dx, (float)-dy);   /* 0 at top, cw + */
            if (ang < 0) ang += 2.0f * (float)M_PI;
            px(cx + dx, cy + dy, ang <= lim ? col : 0x2a2a2a);
        }
}

/* sparkline — connected 2px columns over a 0..100 ring buffer */
static void spark(int x, int y, int w, int h, const float *vals, int cnt, int pos,
                  uint32_t col){
    rect(x, y, w, h, 0x202020, 255);
    hline(x, y + h - 1, w, 0x383838);
    if (cnt < 2) return;
    int prev_yv = 0;
    for (int i = 0; i < cnt; i++){
        float v = vals[(pos - cnt + i + 2 * SPARK_N) % SPARK_N];
        if (v < 0) v = 0; if (v > 100) v = 100;
        int yv = y + h - 1 - (int)(v / 100.0f * (h - 2));
        int xc = x + (int)((float)i * (w - 2) / (SPARK_N - 1));
        if (i > 0){
            int y0 = yv < prev_yv ? yv : prev_yv;
            int y1 = yv > prev_yv ? yv : prev_yv;
            rect(xc, y0, 2, y1 - y0 + 2, col, 255);
        }
        prev_yv = yv;
    }
}

/* small home-page tile */
static void tile(int x, int y, int w, int h, const char *label){
    rect(x, y, w, h, UN_GREY_80, 230);
    rect(x, y, w, 1, UN_GREY_70, 255);
    text(x + 10, y + 8, TITLE_SCALE, UN_DIM, label);
}


#endif /* PANEL_UI_H */
