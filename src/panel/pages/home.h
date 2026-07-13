/*
 * plugin/src/panel/pages/home.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * HOME page — a fully user-composable page: add ANY module you like via
 * LAYOUT_HOME (edited from the web layout editor). Default is a distinct
 * ring-based starter; nothing here is fixed.
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_HOME_H
#define PANEL_PAGE_HOME_H

static void page_home(stats_t *st){
    render_modules(cfg_layout_home, st);
}

#endif /* PANEL_PAGE_HOME_H */
