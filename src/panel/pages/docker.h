/*
 * plugin/src/panel/pages/docker.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * DOCKER page — config-driven module list (LAYOUT_DOCKER); default "containers,vms".
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_DOCKER_H
#define PANEL_PAGE_DOCKER_H

static void page_docker(stats_t *st){
    render_modules(cfg_layout_docker, st);
}

#endif /* PANEL_PAGE_DOCKER_H */
