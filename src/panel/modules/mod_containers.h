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
/* one container card. style: 0=card (name + ip + status + update), 1=compact
 * (a single row: dot + name + ip / UPDATE). Returns the advance (height + gap). */
static int ctr_card(int y, ctr_t *c, int style){
    char b[128];
    if (style == 1){                                     /* compact — one row */
        int ch = 44;
        card(y, gy(ch), NULL);
        const char *right = c->update ? "UPDATE" : (c->ip[0] ? c->ip : NULL);
        uint32_t rcol = c->update ? UN_WARN : UN_DIM;
        item_head(y, col_ctr(c->state), c->name, 1.9f, right, 1.5f, rcol);
        return gy(ch) + gy(C_GAP);
    }
    card(y, gy(64), NULL);
    /* header row: state dot + name (left), IP (right) */
    item_head(y, col_ctr(c->state), c->name, 2.1f, c->ip[0] ? c->ip : NULL, 1.6f, UN_DIM);
    /* second row: status text (left), UPDATE flag (right) when an image update is ready */
    snprintf(b, sizeof b, "%s", c->status[0] ? c->status : c->state);
    trunc_fit(b, 1.6f, W - 66 - (c->update ? 64 : 0));
    text(C_X0 + gy(18), y + gy(38), 1.6f, UN_DIM, b);
    if (c->update) text(C_R - text_w(1.6f, "UPDATE"), y + gy(38), 1.6f, UN_WARN, "UPDATE");
    return gy(64) + gy(C_GAP);
}

static int mod_containers(int y, stats_t *st, int variant){  /* summary + all containers, styled */
    char b[128];
    int y0 = y;
    if (st->docker >= 0){
        int upds = 0;
        for (int i = 0; i < st->n_ctrs; i++) if (st->ctrs[i].update) upds++;
        if (upds) snprintf(b, sizeof b, "%d/%d up  %d update%s", st->docker, st->docker_total, upds, upds > 1 ? "s" : "");
        else      snprintf(b, sizeof b, "%d / %d running", st->docker, st->docker_total);
        y += value_card(y, 76, "DOCKER", b, upds ? UN_WARN : UN_TEXT);
    } else {
        y += value_card(y, 76, "DOCKER", "docker n/a", UN_DIM);
    }
    for (int i = 0; i < st->n_ctrs; i++) y += ctr_card(y, &st->ctrs[i], variant);
    if (st->docker >= 0 && st->n_ctrs == 0)
        y += value_card(y, 56, "DOCKER", "no containers", UN_DIM);
    return y - y0;
}

static int mod_container(int y, stats_t *st, int variant){   /* one container, "name" or "name:style" */
    (void)variant;
    char key[64]; snprintf(key, sizeof key, "%s", g_item_key);
    char *style = strchr(key, ':'); if (style) *style++ = 0;
    int sidx = (style && !strcmp(style, "compact")) ? 1 : 0;
    int i = -1;
    if (key[0]){
        for (int k = 0; k < st->n_ctrs; k++) if (!strcmp(st->ctrs[k].name, key)){ i = k; break; }
        if (i < 0 && key[0] >= '0' && key[0] <= '9'){ int n = atoi(key); if (n >= 0 && n < st->n_ctrs) i = n; }
    } else if (st->n_ctrs > 0) i = 0;
    if (i < 0) return value_card(y, 64, "CONTAINER", "not present", UN_DIM);
    return ctr_card(y, &st->ctrs[i], sidx);
}

#endif /* PANEL_MOD_CONTAINERS_H */
