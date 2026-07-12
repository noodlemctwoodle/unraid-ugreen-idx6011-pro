/*
 * plugin/src/panel/pages/docker.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * DOCKER page — container list + VM state
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_PAGE_DOCKER_H
#define PANEL_PAGE_DOCKER_H

static void page_docker(stats_t *st){
    char b[128];
    int y = body_top;

    card(y, 76, "DOCKER");
    if (st->docker >= 0){
        snprintf(b, sizeof b, "%d / %d running", st->docker, st->docker_total);
        trunc_fit(b, 2.3f, W - 44);
        text(22, y + 44, 2.3f, UN_TEXT, b);
    } else {
        text(22, y + 44, 2.0f, UN_DIM, "docker n/a");
    }
    y += 88;

    for (int i = 0; i < st->n_ctrs; i++){
        ctr_t *c = &st->ctrs[i];
        card(y, 64, NULL);
        uint32_t dot = !strcmp(c->state, "running") ? UN_OK
                     : (!strcmp(c->state, "restarting") || !strcmp(c->state, "paused"))
                       ? UN_WARN : 0x777777;
        rect(22, y + 14, 10, 10, dot, 255);
        snprintf(b, sizeof b, "%s", c->name);
        trunc_fit(b, 2.1f, W - 66);
        text(40, y + 10, 2.1f, UN_TEXT, b);
        snprintf(b, sizeof b, "%s", c->status[0] ? c->status : c->state);
        trunc_fit(b, 1.6f, W - 66);
        text(40, y + 38, 1.6f, UN_DIM, b);
        y += 74;
    }
    if (st->docker >= 0 && st->n_ctrs == 0){
        card(y, 56, NULL);
        text(22, y + 22, 1.8f, UN_DIM, "no containers");
        y += 66;
    }

    card(y, 56, NULL);
    if (!st->vm_enabled)
        snprintf(b, sizeof b, "VMs: disabled");
    else
        snprintf(b, sizeof b, "VMs: %d running", st->vm_count);
    text(22, y + 22, 2.0f, st->vm_enabled ? UN_TEXT : UN_DIM, b);
    page_end = y + 66;
}


#endif /* PANEL_PAGE_DOCKER_H */
