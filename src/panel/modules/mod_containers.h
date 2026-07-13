/*
 * plugin/src/panel/modules/mod_containers.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Docker cards. "containers" draws a summary + one card per container; "container"
 * (indexed, e.g. container:2) draws a single container so they can be reordered.
 * Share ctr_card() + the card style shim. Part of panel_dash: #included in order.
 */
#ifndef PANEL_MOD_CONTAINERS_H
#define PANEL_MOD_CONTAINERS_H

/* container state -> dot colour (specific to Docker states) */
static uint32_t col_ctr(const char *state){
    if (!strcmp(state, "running")) return UN_OK;
    if (!strcmp(state, "restarting") || !strcmp(state, "paused")) return UN_WARN;
    return 0x777777;
}
static int ctr_card(int y, ctr_t *c){
    char b[128];
    card(y, 64, NULL);
    item_head(y, col_ctr(c->state), c->name, 2.1f, NULL, 0, 0);
    snprintf(b, sizeof b, "%s", c->status[0] ? c->status : c->state);
    trunc_fit(b, 1.6f, W - 66);
    text(C_X0 + 18, y + 38, 1.6f, UN_DIM, b);
    return 64 + C_GAP;
}

static int mod_containers(int y, stats_t *st, int variant){  /* summary + all containers */
    (void)variant;
    char b[128];
    int y0 = y;
    if (st->docker >= 0){
        snprintf(b, sizeof b, "%d / %d running", st->docker, st->docker_total);
        y += value_card(y, 76, "DOCKER", b, UN_TEXT);
    } else {
        y += value_card(y, 76, "DOCKER", "docker n/a", UN_DIM);
    }
    for (int i = 0; i < st->n_ctrs; i++) y += ctr_card(y, &st->ctrs[i]);
    if (st->docker >= 0 && st->n_ctrs == 0)
        y += value_card(y, 56, "DOCKER", "no containers", UN_DIM);
    return y - y0;
}

static int mod_container(int y, stats_t *st, int variant){   /* one container by index */
    if (variant < 0 || variant >= st->n_ctrs)
        return value_card(y, 64, "CONTAINER", "not present", UN_DIM);
    return ctr_card(y, &st->ctrs[variant]);
}

#endif /* PANEL_MOD_CONTAINERS_H */
