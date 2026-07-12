/*
 * plugin/src/panel/stats_array.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * per-disk rows, notifications, host / ip / uptime / var.ini
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_STATS_ARRAY_H
#define PANEL_STATS_ARRAY_H

/* --- per-disk rows from /var/local/emhttp/disks.ini --- */
static void disks_commit(stats_t *st, disk_t *cur, int have){
    if (have && cur->present && !cur->np && cur->health != 3 && st->n_disks < MAX_DISKS)
        st->disks[st->n_disks++] = *cur;
}
static void read_disks(stats_t *st){
    st->n_disks = 0;
    FILE *f = fopen("/var/local/emhttp/disks.ini", "r"); if (!f) return;
    char line[512]; disk_t cur; int have = 0;
    memset(&cur, 0, sizeof cur);
    while (fgets(line, sizeof line, f)){
        if (line[0] == '['){
            disks_commit(st, &cur, have);
            memset(&cur, 0, sizeof cur); cur.temp = -1; have = 1;
            if (sscanf(line, "[\"%31[^\"]\"]", cur.name) != 1) have = 0;
            continue;
        }
        if (!have) continue;
        char *k, *v;
        if (!ini_kv(line, &k, &v)) continue;
        if      (!strcmp(k, "device"))    cur.present = v[0] != 0;
        else if (!strcmp(k, "status"))    cur.np = !strcmp(v, "DISK_NP");
        else if (!strcmp(k, "type"))      snprintf(cur.type, sizeof cur.type, "%s", v);
        else if (!strcmp(k, "temp"))      cur.temp = isdigit((unsigned char)v[0]) ? atoi(v) : -1;
        else if (!strcmp(k, "spundown"))  cur.spun = v[0] == '1';
        else if (!strcmp(k, "numErrors")) cur.errors = atoll(v);
        else if (!strcmp(k, "size"))      cur.size_kib = strtoull(v, NULL, 10);
        else if (!strcmp(k, "fsSize"))    cur.fs_size = strtoull(v, NULL, 10);
        else if (!strcmp(k, "fsUsed"))    cur.fs_used = strtoull(v, NULL, 10);
        else if (!strcmp(k, "color")){
            if      (!strncmp(v, "green", 5)) cur.health = 0;
            else if (strstr(v, "red"))        cur.health = 2;
            else if (!strncmp(v, "grey", 4))  cur.health = 3;
            else                              cur.health = 1;
        }
    }
    disks_commit(st, &cur, have);
    fclose(f);
}

/* --- notifications: /tmp/notifications/unread --- */
static void read_notif(stats_t *st){
    st->notif_count = 0; st->notif_imp = 0; st->notif_subj[0] = 0;
    DIR *d = opendir("/tmp/notifications/unread"); if (!d) return;
    struct dirent *e; time_t best = 0; char bestp[512] = "";
    while ((e = readdir(d))){
        size_t l = strlen(e->d_name);
        if (l < 8 || strcmp(e->d_name + l - 7, ".notify")) continue;
        char p[512]; snprintf(p, sizeof p, "/tmp/notifications/unread/%s", e->d_name);
        struct stat sb;
        if (stat(p, &sb) || !S_ISREG(sb.st_mode)) continue;
        if ((sb.st_mode & 0777) == 0400) continue;      /* hidden in WebGUI */
        st->notif_count++;
        if (sb.st_mtime >= best){ best = sb.st_mtime; snprintf(bestp, sizeof bestp, "%s", p); }
    }
    closedir(d);
    if (!st->notif_count || !bestp[0]) return;
    FILE *f = fopen(bestp, "r"); if (!f) return;
    char line[512], event[64] = "";
    while (fgets(line, sizeof line, f)){
        char *k, *v;
        if (!ini_kv(line, &k, &v)) continue;
        if      (!strcmp(k, "subject")) snprintf(st->notif_subj, sizeof st->notif_subj, "%s", v);
        else if (!strcmp(k, "event"))   snprintf(event, sizeof event, "%s", v);
        else if (!strcmp(k, "importance"))
            st->notif_imp = !strcmp(v, "alert") ? 2 : !strcmp(v, "warning") ? 1 : 0;
    }
    fclose(f);
    if (!st->notif_subj[0]) snprintf(st->notif_subj, sizeof st->notif_subj, "%s", event);
}

/* --- host / ip (v4 + global v6) / uptime / var.ini --- */
static void read_misc(stats_t *st){
    gethostname(st->host, sizeof(st->host) - 1);
    st->ip[0] = 0;
    struct ifaddrs *ifa, *p;
    if (getifaddrs(&ifa) == 0){
        char fallback[48] = "";
        for (p = ifa; p; p = p->ifa_next){
            if (!p->ifa_addr || !strcmp(p->ifa_name, "lo")) continue;
            if (p->ifa_addr->sa_family == AF_INET){
                char ipb[48];
                inet_ntop(AF_INET, &((struct sockaddr_in*)p->ifa_addr)->sin_addr, ipb, sizeof ipb);
                if (!fallback[0]) snprintf(fallback, sizeof fallback, "%s", ipb);
                if (!strcmp(p->ifa_name, st->prim_if) && !st->ip[0])
                    snprintf(st->ip, sizeof st->ip, "%s", ipb);
                for (int i = 0; i < st->n_ifaces; i++)
                    if (!strcmp(st->ifc[i].name, p->ifa_name) && !st->ifc[i].ip[0])
                        snprintf(st->ifc[i].ip, sizeof st->ifc[i].ip, "%s", ipb);
            } else if (p->ifa_addr->sa_family == AF_INET6){
                struct sockaddr_in6 *s6 = (struct sockaddr_in6*)p->ifa_addr;
                if (IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr) ||
                    IN6_IS_ADDR_LOOPBACK(&s6->sin6_addr)) continue;
                char ipb[48];
                inet_ntop(AF_INET6, &s6->sin6_addr, ipb, sizeof ipb);
                for (int i = 0; i < st->n_ifaces; i++)
                    if (!strcmp(st->ifc[i].name, p->ifa_name) && !st->ifc[i].ip6[0])
                        snprintf(st->ifc[i].ip6, sizeof st->ifc[i].ip6, "%s", ipb);
            }
        }
        if (!st->ip[0]) snprintf(st->ip, sizeof st->ip, "%s", fallback);
        freeifaddrs(ifa);
    }
    FILE *f = fopen("/proc/uptime", "r");
    if (f){ double u; if (fscanf(f, "%lf", &u) == 1) st->up_s = (long)u; fclose(f); }

    st->arr[0] = 0; st->resync = st->resync_pos = 0; st->resync_act[0] = 0;
    st->resync_mbs = 0; st->md_bad = 0; st->sync_errs = 0; st->mover = 0;
    f = fopen("/var/local/emhttp/var.ini", "r");
    if (f){
        char line[512];
        unsigned long long dt = 0, db = 0;
        while (fgets(line, sizeof line, f)){
            char *k, *v;
            if (!ini_kv(line, &k, &v)) continue;
            if      (!strcmp(k, "mdState"))          snprintf(st->arr, sizeof st->arr, "%s", v);
            else if (!strcmp(k, "version"))          snprintf(st->version, sizeof st->version, "%s", v);
            else if (!strcmp(k, "mdResync"))         st->resync = strtoull(v, NULL, 10);
            else if (!strcmp(k, "mdResyncPos"))      st->resync_pos = strtoull(v, NULL, 10);
            else if (!strcmp(k, "mdResyncAction"))   snprintf(st->resync_act, sizeof st->resync_act, "%s", v);
            else if (!strcmp(k, "mdResyncDt"))       dt = strtoull(v, NULL, 10);
            else if (!strcmp(k, "mdResyncDb"))       db = strtoull(v, NULL, 10);
            else if (!strcmp(k, "mdNumDisabled"))    st->md_bad += atoi(v);
            else if (!strcmp(k, "mdNumInvalid"))     st->md_bad += atoi(v);
            else if (!strcmp(k, "mdNumMissing"))     st->md_bad += atoi(v);
            else if (!strcmp(k, "sbSyncErrs"))       st->sync_errs = atoll(v);
            else if (!strcmp(k, "shareMoverActive")) st->mover = !strcmp(v, "yes");
        }
        if (dt > 0) st->resync_mbs = (double)db / (double)dt / 1024.0;
        fclose(f);
    }
    static int kdone; static char krnl[64];
    if (!kdone){
        struct utsname u;
        if (!uname(&u)) snprintf(krnl, sizeof krnl, "%s", u.release);
        kdone = 1;
    }
    snprintf(st->kernel, sizeof st->kernel, "%s", krnl);
}


#endif /* PANEL_STATS_ARRAY_H */
