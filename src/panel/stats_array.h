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

/* --- per-disk SMART detail from Unraid's CACHED reports (/var/local/emhttp/smart/<name>) ---
 * No smartctl call and no spin-up. Fills poh / wear% / reallocated / pending on st->disks[].
 * Handles both the ATA attribute table and NVMe "Key: value" health lines. Absent file -> all -1. */
static void read_smart(stats_t *st){
    for (int d = 0; d < st->n_disks; d++){
        disk_t *dk = &st->disks[d];
        dk->poh = -1; dk->wear = -1; dk->realloc = -1; dk->pending = -1;
        char p[128]; snprintf(p, sizeof p, "/var/local/emhttp/smart/%s", dk->name);
        FILE *f = fopen(p, "r"); if (!f) continue;
        char line[512];
        while (fgets(line, sizeof line, f)){
            if (strstr(line, "Power On Hours")){ char *c = strchr(line, ':'); if (c) dk->poh = atoll(c + 1); continue; }
            if (strstr(line, "Percentage Used")){ char *c = strchr(line, ':'); if (c){ int u = atoi(c + 1); if (u >= 0 && u <= 100) dk->wear = 100 - u; } continue; }
            char tmp[512]; snprintf(tmp, sizeof tmp, "%s", line);
            char *save = NULL, *tok = strtok_r(tmp, " \t\r\n", &save);
            if (!tok || !isdigit((unsigned char)tok[0])) continue;      /* not an ATA attribute row */
            char name[40] = ""; int col = 1, val = -1; char *last = tok;
            while ((tok = strtok_r(NULL, " \t\r\n", &save))){
                col++;
                if (col == 2)      snprintf(name, sizeof name, "%s", tok);
                else if (col == 4) val = atoi(tok);                     /* normalised VALUE */
                last = tok;                                             /* RAW = last column */
            }
            long long raw = last ? atoll(last) : -1;
            if      (!strcmp(name, "Power_On_Hours"))          dk->poh = raw;
            else if (!strcmp(name, "Reallocated_Sector_Ct"))   dk->realloc = raw;
            else if (!strcmp(name, "Current_Pending_Sector"))  dk->pending = raw;
            else if (!strcmp(name, "Wear_Leveling_Count") || !strcmp(name, "Media_Wearout_Indicator") ||
                     !strcmp(name, "SSD_Life_Left")       || !strcmp(name, "Percent_Lifetime_Remain"))
                dk->wear = val;
        }
        fclose(f);
    }
}

/* --- OS + plugin update status (cached 60s) ---
 * Unraid OS: /tmp/unraidcheck/result.json ("isNewer" + target "version").
 * Plugins:   /tmp/plugins/pluginPending (one pending plugin per line). */
static void read_updates(stats_t *st){
    static long last = -1000000;
    static int c_os, c_pl; static char c_ver[32];
    long nowms = now_ms();
    if (nowms - last >= 60000){
        last = nowms;
        c_os = 0; c_pl = 0; c_ver[0] = 0;
        FILE *f = fopen("/tmp/unraidcheck/result.json", "r");
        if (f){
            char buf[2048]; size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
            char *nw = strstr(buf, "\"isNewer\"");
            if (nw){ char *t = strstr(nw, "true"); if (t && t - nw < 20) c_os = 1; }
            char *vp = strstr(buf, "\"version\"");
            if (vp && (vp = strchr(vp, ':')) && (vp = strchr(vp, '"'))){
                vp++; char *e = strchr(vp, '"');
                if (e){ int l = (int)(e - vp); if (l > 31) l = 31; snprintf(c_ver, sizeof c_ver, "%.*s", l, vp); }
            }
        }
        FILE *pf = fopen("/tmp/plugins/pluginPending", "r");
        if (pf){
            char line[256];
            while (fgets(line, sizeof line, pf)){
                char *p = line; while (*p == ' ' || *p == '\t') p++;
                if (*p && *p != '\n' && *p != '\r') c_pl++;
            }
            fclose(pf);
        }
    }
    st->os_update = c_os; st->plugin_updates = c_pl;
    snprintf(st->os_new_ver, sizeof st->os_new_ver, "%s", c_ver);
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

    st->arr[0] = 0; st->resync = st->resync_pos = st->resync_size = 0; st->resync_act[0] = 0;
    st->resync_mbs = 0; st->md_bad = 0; st->sync_errs = 0; st->mover = 0;
    st->reg_type[0] = 0; st->reg_to[0] = 0;
    f = fopen("/var/local/emhttp/var.ini", "r");
    if (f){
        char line[512];
        unsigned long long dt = 0, db = 0;
        while (fgets(line, sizeof line, f)){
            char *k, *v;
            if (!ini_kv(line, &k, &v)) continue;
            if      (!strcmp(k, "mdState"))          snprintf(st->arr, sizeof st->arr, "%s", v);
            else if (!strcmp(k, "version"))          snprintf(st->version, sizeof st->version, "%s", v);
            else if (!strcmp(k, "regTy"))            snprintf(st->reg_type, sizeof st->reg_type, "%s", v);
            else if (!strcmp(k, "regTo"))            snprintf(st->reg_to, sizeof st->reg_to, "%s", v);
            else if (!strcmp(k, "mdResync"))         st->resync = strtoull(v, NULL, 10);
            else if (!strcmp(k, "mdResyncPos"))      st->resync_pos = strtoull(v, NULL, 10);
            else if (!strcmp(k, "mdResyncSize"))     st->resync_size = strtoull(v, NULL, 10);
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

/* --- Dynamix File Manager copy/move progress (built into Unraid) ---
 * The File Manager runs the copy as `rsync --info=...,progress2`, records the
 * operation in /var/tmp/file.manager.active (JSON) and its live rsync output in
 * /var/tmp/file.manager.status, with the running PID in /var/tmp/file.manager.pid.
 * We treat the op as active only while that PID is alive, read the title +
 * destination from the JSON, and take the latest progress line (NN% + rate + ETA)
 * from the tail of the status file. */
static void read_transfer(stats_t *st){
    st->transfer_active = 0; st->transfer_op[0] = 0; st->transfer_dest[0] = 0;
    st->transfer_pct = 0; st->transfer_speed[0] = 0; st->transfer_eta[0] = 0;

    FILE *pf = fopen("/var/tmp/file.manager.pid", "r"); if (!pf) return;
    int pid = 0, got = fscanf(pf, "%d", &pid); fclose(pf);
    if (got != 1 || pid <= 0) return;
    char pp[64]; struct stat sb;
    snprintf(pp, sizeof pp, "/proc/%d", pid);
    if (stat(pp, &sb) != 0) return;                       /* process gone -> idle */

    /* operation title + destination folder from the active JSON (light string parse) */
    FILE *af = fopen("/var/tmp/file.manager.active", "r");
    if (af){
        char b[1400]; size_t n = fread(b, 1, sizeof b - 1, af); b[n] = 0; fclose(af);
        char *t = strstr(b, "\"title\":\"");
        if (t){ t += 9; char *e = strchr(t, '"');
            if (e){ int l = (int)(e - t); if (l > 15) l = 15; snprintf(st->transfer_op, sizeof st->transfer_op, "%.*s", l, t); } }
        char *g = strstr(b, "\"target\":\"");
        if (g){ g += 10; char *e = strchr(g, '"');
            if (e){ char tg[256]; int l = (int)(e - g), j = 0;
                for (int i = 0; i < l && j < (int)sizeof tg - 1; i++){ if (g[i] == '\\' && i + 1 < l) i++; tg[j++] = g[i]; }
                tg[j] = 0;
                while (j > 1 && tg[j - 1] == '/') tg[--j] = 0;   /* strip trailing slash */
                char *bn = strrchr(tg, '/'); bn = bn ? bn + 1 : tg;
                snprintf(st->transfer_dest, sizeof st->transfer_dest, "%s", bn);
            }
        }
    }
    if (!st->transfer_op[0]) snprintf(st->transfer_op, sizeof st->transfer_op, "Transfer");

    /* latest progress line from the tail of the status file (rsync progress2:
     * "<size>  NN%  <rate>/s  <H:MM:SS>", updates separated by \r or \n) */
    FILE *sf = fopen("/var/tmp/file.manager.status", "r");
    if (sf){
        if (fseek(sf, -4096L, SEEK_END) != 0) fseek(sf, 0L, SEEK_SET);
        char buf[4200]; size_t n = fread(buf, 1, sizeof buf - 1, sf); buf[n] = 0; fclose(sf);
        char pct[16] = "", spd[24] = "", eta[16] = "", *save = NULL;
        for (char *line = strtok_r(buf, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)){
            if (!strchr(line, '%')) continue;
            char p2[16] = "", s2[24] = "", e2[16] = "", tmp[512], *ts = NULL;
            snprintf(tmp, sizeof tmp, "%s", line);
            for (char *tok = strtok_r(tmp, " \t", &ts); tok; tok = strtok_r(NULL, " \t", &ts)){
                size_t tl = strlen(tok);
                if (tl && tok[tl - 1] == '%')                       snprintf(p2, sizeof p2, "%s", tok);
                else if (tl >= 2 && !strcmp(tok + tl - 2, "/s"))    snprintf(s2, sizeof s2, "%s", tok);
                else if (tok[0] >= '0' && tok[0] <= '9' && strchr(tok, ':')) snprintf(e2, sizeof e2, "%s", tok);
            }
            if (p2[0] && s2[0]){ snprintf(pct, sizeof pct, "%s", p2);
                                 snprintf(spd, sizeof spd, "%s", s2);
                                 snprintf(eta, sizeof eta, "%s", e2); }
        }
        if (pct[0]){ st->transfer_pct = atof(pct);
                     snprintf(st->transfer_speed, sizeof st->transfer_speed, "%s", spd);
                     snprintf(st->transfer_eta, sizeof st->transfer_eta, "%s", eta); }
    }
    st->transfer_active = 1;
}

#endif /* PANEL_STATS_ARRAY_H */
