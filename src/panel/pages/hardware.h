/*
 * plugin/src/panel/pages/hardware.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * HARDWARE page — config-driven module list (LAYOUT_HARDWARE). Widgets live in
 * src/panel/modules/; default matches the original (cpu/gpu sparklines, cpu temp,
 * memory, npu, power).
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_HARDWARE_H
#define PANEL_PAGE_HARDWARE_H

static void page_hardware(stats_t *st){
    render_modules(cfg_layout_hardware, st);
}

#endif /* PANEL_PAGE_HARDWARE_H */
