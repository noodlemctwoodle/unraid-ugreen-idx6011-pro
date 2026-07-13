/*
 * plugin/src/panel/pages/overview.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * OVERVIEW page — now a config-driven module list. The order, per-module display
 * variant (e.g. cpu bar|ring) and which modules appear all come from
 * LAYOUT_OVERVIEW in settings.cfg (parsed into cfg_layout_overview); the widgets
 * themselves live in src/panel/modules/. Default matches the original layout.
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_OVERVIEW_H
#define PANEL_PAGE_OVERVIEW_H

static void page_overview(stats_t *st){
    render_modules(cfg_layout_overview, st);
}

#endif /* PANEL_PAGE_OVERVIEW_H */
