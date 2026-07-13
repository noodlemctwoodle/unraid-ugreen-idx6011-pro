/*
 * plugin/src/panel/pages/network.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * NETWORK page — config-driven module list (LAYOUT_NETWORK); default "ifaces".
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_NETWORK_H
#define PANEL_PAGE_NETWORK_H

static void page_network(stats_t *st){
    render_modules(cfg_layout_network, st);
}

#endif /* PANEL_PAGE_NETWORK_H */
