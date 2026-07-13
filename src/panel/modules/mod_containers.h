/*
 * plugin/src/panel/modules/mod_containers.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * "containers" module — a Docker summary card + one card per container (state
 * dot, name, status). Draws all and returns the combined height. Composes the
 * card style shim. Part of panel_dash: #included by panel_dash.c in a fixed order.
 */
#ifndef PANEL_MOD_CONTAINERS_H
#define PANEL_MOD_CONTAINERS_H

/* container state -> dot colour (specific to Docker states) */
static uint32_t col_ctr(const char *state){
    if (!strcmp(state, "running")) return UN_OK;
    if (!strcmp(state, "restarting") || !strcmp(state, "paused")) return UN_WARN;
    return 0x777777;
}

static int mod_containers(int y, stats_t *st, int variant){
    (void)variant;
    char b[128];
    int y0 = y;
    if (st->docker >= 0){
        snprintf(b, sizeof b, "%d / %d running", st->docker, st->docker_total);
        y += value_card(y, 76, "DOCKER", b, UN_TEXT);
    } else {
        y += value_card(y, 76, "DOCKER", "docker n/a", UN_DIM);
    }
    for (int i = 0; i < st->n_ctrs; i++){
        ctr_t *c = &st->ctrs[i];
        card(y, 64, NULL);
        item_head(y, col_ctr(c->state), c->name, 2.1f, NULL, 0, 0);
        snprintf(b, sizeof b, "%s", c->status[0] ? c->status : c->state);
        trunc_fit(b, 1.6f, W - 66);
        text(C_X0 + 18, y + 38, 1.6f, UN_DIM, b);
        y += 64 + C_GAP;
    }
    if (st->docker >= 0 && st->n_ctrs == 0)
        y += value_card(y, 56, "DOCKER", "no containers", UN_DIM);
    return y - y0;
}

#endif /* PANEL_MOD_CONTAINERS_H */
