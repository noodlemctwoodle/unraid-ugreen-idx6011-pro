/*
 * plugin/src/panel/pages/disks.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * DISKS page — config-driven module list (LAYOUT_DISKS); default "disks".
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_DISKS_H
#define PANEL_PAGE_DISKS_H

static void page_disks(stats_t *st){
    render_modules(cfg_layout_disks, st);
}

#endif /* PANEL_PAGE_DISKS_H */
