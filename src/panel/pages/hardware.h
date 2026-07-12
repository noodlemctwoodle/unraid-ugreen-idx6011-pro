/*
 * plugin/src/panel/pages/hardware.h
 *
 * Created by Toby G on 12/07/2026.
 *
 * HARDWARE page — cpu/gpu sparklines, npu, power
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_HARDWARE_H
#define PANEL_PAGE_HARDWARE_H

static void page_hardware(stats_t *st){
    char b[128];
    int y = body_top;

    card(y, 152, "CPU");
    snprintf(b, sizeof b, "%.0f%%", st->cpu);
    text(22, y + 44, 3.6f, UN_TEXT, b);
    if (st->cpu_threads > 0){
        snprintf(b, sizeof b, "%dT", st->cpu_threads);
        text(W - 22 - text_w(1.8f, b), y + 12, 1.8f, UN_DIM, b);
    }
    spark(22, y + 84, W - 44, 62, h_cpu, h_cnt, h_pos, UN_ORANGE_M);
    y += 164;

    card(y, 84, "CPU TEMP");
    if (st->temp_c > 0){
        snprintf(b, sizeof b, "%dC", st->temp_c);
        text(W - 22 - text_w(2.4f, b), y + 10, 2.4f,
             st->temp_c > 85 ? UN_BAD : UN_TEXT, b);
        bar(22, y + 46, W - 44, 18, st->temp_c,
            st->temp_c > 85 ? UN_BAD : st->temp_c > 70 ? UN_WARN : UN_ORANGE_M);
    } else {
        text(22, y + 44, 2.0f, UN_DIM, "n/a");
    }
    y += 96;

    card(y, 118, "MEMORY");
    double mp = st->mem_tot_mb ? 100.0 * st->mem_used_mb / st->mem_tot_mb : 0;
    snprintf(b, sizeof b, "%.1f / %.1f GB", st->mem_used_mb / 1024.0, st->mem_tot_mb / 1024.0);
    text(22, y + 44, 2.4f, UN_TEXT, b);
    snprintf(b, sizeof b, "%.0f%%", mp);
    text(W - 22 - text_w(1.9f, b), y + 47, 1.9f, UN_DIM, b);
    bar(22, y + 84, W - 44, 16, mp, level_col(mp));
    y += 130;

    card(y, 152, "GPU");
    if (st->gpu_avail){
        snprintf(b, sizeof b, "%.0f%%", st->gpu_busy);
        text(22, y + 44, 3.6f, UN_TEXT, b);
        if (st->gpu_freq >= 0){
            snprintf(b, sizeof b, "%d MHz", st->gpu_freq);
            text(W - 22 - text_w(1.9f, b), y + 12, 1.9f, UN_DIM, b);
        }
        spark(22, y + 84, W - 44, 62, h_gpu, h_cnt, h_pos, UN_ORANGE);
    } else {
        text(22, y + 70, 2.0f, UN_DIM, "not available");
    }
    y += 164;

    card(y, 76, "NPU");
    if (st->npu_avail){
        if (st->npu_freq > 0) snprintf(b, sizeof b, "busy %.0f%%  %d/%d MHz",
                                       st->npu_busy, st->npu_freq, st->npu_max_freq);
        else                  snprintf(b, sizeof b, "present  (idle)");
        text(22, y + 44, 1.9f, UN_TEXT, b);
    } else {
        text(22, y + 44, 1.9f, UN_DIM, "not present");
    }
    y += 88;

    card(y, 76, "POWER");
    if (st->pwr_pkg_w >= 0){
        if (st->pwr_sys_w > st->pwr_pkg_w)
            snprintf(b, sizeof b, "%.1f W  (cpu %.1f W)", st->pwr_sys_w, st->pwr_pkg_w);
        else
            snprintf(b, sizeof b, "%.1f W", st->pwr_pkg_w);
        text(22, y + 44, 2.2f, UN_TEXT, b);
    } else {
        text(22, y + 44, 1.9f, UN_DIM, "n/a");
    }
    page_end = y + 88;
}


#endif /* PANEL_PAGE_HARDWARE_H */
