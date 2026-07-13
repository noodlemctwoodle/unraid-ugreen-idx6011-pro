/*
 * plugin/src/panel/modules/module.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Dashboard module framework. A "module" is a self-contained widget: it draws
 * itself starting at screen-y `y` using the shared primitives (card/bar/ring/
 * text from draw.h + ui.h) and the live stats, then returns the vertical space
 * it consumed so the page renderer can advance to the next module.
 *
 * `variant` selects a display style (e.g. cpu: 0=bar, 1=ring). Modules with a
 * single style ignore it. Pages are just ordered module lists in settings.cfg
 * (LAYOUT_<PAGE>), parsed + dispatched by render_modules() in registry.h.
 *
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_MODULE_H
#define PANEL_MODULE_H

typedef int (*modfn)(int y, stats_t *st, int variant);

/* the raw variant token of the module currently rendering (e.g. "eth0" for
 * "iface:eth0"), set by render_modules(). Indexed modules read it to pick their
 * instance by NAME (a numeric value is treated as a legacy index; empty = first). */
static const char *g_item_key = "";

typedef struct {
    const char *id;          /* stable config id, e.g. "cpu"                 */
    const char *label;       /* human label for the web layout editor        */
    modfn       fn;          /* draw function (returns height consumed)       */
    int         nvariants;   /* number of display styles (>= 1)              */
    const char *variants[12];/* style ids, e.g. {"bar","ring","graph","area"} */
    int         indexed;     /* 1 = per-item: the variant selects one instance */
                             /*   by NAME (e.g. iface:eth0, disk:cache), with  */
                             /*   a numeric index as a legacy fallback         */
    const char *itemsrc;     /* for indexed modules: which --items list to     */
                             /*   populate the editor dropdown from            */
                             /*   ("ifaces" / "disks" / "containers")          */
} modinfo_t;

#endif /* PANEL_MODULE_H */
