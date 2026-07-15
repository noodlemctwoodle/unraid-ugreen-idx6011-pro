/*
 * plugin/src/panel/stats_system.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * cpu / mem / net / storage / temps / fans / RAPL power probes
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_STATS_SYSTEM_H
#define PANEL_STATS_SYSTEM_H

/* --- CPU (v1) --- */
static unsigned long long pc_busy, pc_tot;
static void read_cpu(stats_t *st){
    FILE *f = fopen("/proc/stat", "r"); if (!f) return;
    unsigned long long u,n,s,i,io,irq,sirq,steal;
    if (fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &u,&n,&s,&i,&io,&irq,&sirq,&steal) == 8){
        unsigned long long busy = u+n+s+irq+sirq+steal, tot = busy+i+io;
        if (pc_tot && tot > pc_tot)
            st->cpu = 100.0 * (double)(busy - pc_busy) / (double)(tot - pc_tot);
        pc_busy = busy; pc_tot = tot;
    }
    fclose(f);
}

/* --- memory (v1) --- */
static void read_mem(stats_t *st){
    FILE *f = fopen("/proc/meminfo", "r"); if (!f) return;
    char k[64]; long v; char u[16]; long tot = 0, avail = 0;
    while (fscanf(f, "%63s %ld %15s", k, &v, u) >= 2){
        if (!strcmp(k, "MemTotal:")) tot = v;
        else if (!strcmp(k, "MemAvailable:")){ avail = v; break; }
    }
    fclose(f);
    st->mem_tot_mb = tot / 1024; st->mem_used_mb = (tot - avail) / 1024;
}

/* --- network: default-route iface + per-iface rows --- */
/* iface names are short and use a restricted charset; reject anything else so a
 * hand-edited PRIMARY_IFACE can't be interpolated into an unexpected /sys path. */
static int iface_name_ok(const char *s){
    if (!s || !*s || strlen(s) > 31) return 0;
    for (const char *p = s; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '.' || *p == '-' ||
              *p == '_' || *p == ':' || *p == '@')) return 0;
    return 1;
}
static void primary_iface(char out[32]){
    /* explicit override from settings.cfg (PRIMARY_IFACE), if that iface exists */
    if (iface_name_ok(cfg_primary_if)){
        char q[64]; snprintf(q, sizeof q, "/sys/class/net/%s", cfg_primary_if);
        if (access(q, F_OK) == 0){ snprintf(out, 32, "%s", cfg_primary_if); return; }
    }
    strcpy(out, "br0");                        /* fallback (default-route auto-pick) */
    FILE *f = fopen("/proc/net/route", "r"); if (!f) return;
    char line[256];
    if (!fgets(line, sizeof line, f)){ fclose(f); return; }   /* header */
    while (fgets(line, sizeof line, f)){
        char ifn[32]; unsigned long dest;
        if (sscanf(line, "%31s %lx", ifn, &dest) == 2 && dest == 0){
            snprintf(out, 32, "%s", ifn); break;
        }
    }
    fclose(f);
}
static int iface_skip(const char *n){
    return !strcmp(n, "lo") || !strncmp(n, "tunl", 4) ||
           !strncmp(n, "sit", 3) || !strncmp(n, "veth", 4);
}
static struct { char name[32]; unsigned long long rx, tx; } pnet[MAX_IFACES];
static int pnet_n; static struct timespec pnet_t;

static void read_net(stats_t *st){
    primary_iface(st->prim_if);
    st->n_ifaces = 0;
    FILE *f = fopen("/proc/net/dev", "r"); if (!f) return;
    char line[512];
    if (!fgets(line, sizeof line, f) || !fgets(line, sizeof line, f)){ fclose(f); return; }
    while (fgets(line, sizeof line, f) && st->n_ifaces < MAX_IFACES){
        char *c = strchr(line, ':'); if (!c) continue;
        *c = ' ';
        char ifn[32]; unsigned long long rx, d1,d2,d3,d4,d5,d6,d7, tx;
        if (sscanf(line, "%31s %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   ifn, &rx,&d1,&d2,&d3,&d4,&d5,&d6,&d7,&tx) < 10) continue;
        if (iface_skip(ifn)) continue;
        iface_t *ic = &st->ifc[st->n_ifaces++];
        memset(ic, 0, sizeof *ic);
        snprintf(ic->name, sizeof ic->name, "%s", ifn);
        ic->rx_tot = rx; ic->tx_tot = tx;
        char p[128]; snprintf(p, sizeof p, "/sys/class/net/%s/operstate", ifn);
        FILE *g = fopen(p, "r");
        if (g){ char stbuf[16] = ""; if (fgets(stbuf, sizeof stbuf, g)) ic->up = !strncmp(stbuf, "up", 2); fclose(g); }
        if (ic->up){                                     /* link speed (Mbps); -1/absent when down */
            snprintf(p, sizeof p, "/sys/class/net/%s/speed", ifn);
            g = fopen(p, "r");
            if (g){ int sp = 0; if (fscanf(g, "%d", &sp) == 1 && sp > 0) ic->link_mbps = sp; fclose(g); }
        }
    }
    fclose(f);

    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    double dt = pnet_t.tv_sec
        ? (now.tv_sec - pnet_t.tv_sec) + (now.tv_nsec - pnet_t.tv_nsec) / 1e9 : 0;
    for (int i = 0; i < st->n_ifaces; i++){
        for (int j = 0; j < pnet_n; j++){
            if (strcmp(st->ifc[i].name, pnet[j].name)) continue;
            if (dt > 0.05 && st->ifc[i].rx_tot >= pnet[j].rx && st->ifc[i].tx_tot >= pnet[j].tx){
                st->ifc[i].rx_kbs = (st->ifc[i].rx_tot - pnet[j].rx) / dt / 1024.0;
                st->ifc[i].tx_kbs = (st->ifc[i].tx_tot - pnet[j].tx) / dt / 1024.0;
            }
            break;
        }
    }
    pnet_n = st->n_ifaces;
    for (int i = 0; i < pnet_n; i++){
        snprintf(pnet[i].name, sizeof pnet[i].name, "%s", st->ifc[i].name);
        pnet[i].rx = st->ifc[i].rx_tot; pnet[i].tx = st->ifc[i].tx_tot;
    }
    pnet_t = now;

    st->rx_kbs = st->tx_kbs = 0;               /* overview = primary iface only */
    int found = 0;
    for (int i = 0; i < st->n_ifaces; i++)
        if (!strcmp(st->ifc[i].name, st->prim_if)){
            st->rx_kbs = st->ifc[i].rx_kbs; st->tx_kbs = st->ifc[i].tx_kbs;
            found = 1; break;
        }
    if (!found && st->n_ifaces > 0){
        st->rx_kbs = st->ifc[0].rx_kbs; st->tx_kbs = st->ifc[0].tx_kbs;
    }
}

/* --- storage summary via statvfs: the whole ARRAY, not a single pool ---
 * /mnt/user0 is the array-only user view (data disks, no cache/pools) — the total
 * the Unraid Main page shows; /mnt/user (union) is the fallback. Deliberately NOT
 * /mnt/cache: that is one pool, so the card would report a single disk, not the array. */
static void read_disk(stats_t *st){
    struct statvfs v; const char *cand[] = { "/mnt/user0", "/mnt/user", NULL };
    int ok = 0;
    for (int i = 0; cand[i]; i++)
        if (statvfs(cand[i], &v) == 0 && v.f_blocks > 0){ ok = 1; break; }
    if (!ok) return;
    double tot = (double)v.f_blocks * v.f_frsize, freeb = (double)v.f_bfree * v.f_frsize;
    st->disk_tot_gb  = tot / 1e9; st->disk_used_gb = (tot - freeb) / 1e9;
    st->disk_used_pct = tot > 0 ? 100.0 * (tot - freeb) / tot : 0;
}

/* --- CPU temp via hwmon (v1) --- */
static void read_temp(stats_t *st){
    DIR *d = opendir("/sys/class/hwmon"); if (!d) return;
    struct dirent *e; int best = 0;
    while ((e = readdir(d))){
        if (e->d_name[0] == '.') continue;
        char p[512]; snprintf(p, sizeof(p), "/sys/class/hwmon/%s/name", e->d_name);
        FILE *f = fopen(p, "r"); if (!f) continue;
        char name[64] = ""; if (fgets(name, sizeof(name), f)) name[strcspn(name, "\n")] = 0;
        fclose(f);
        if (strcmp(name, "coretemp") && strcmp(name, "k10temp") && strcmp(name, "acpitz")) continue;
        snprintf(p, sizeof(p), "/sys/class/hwmon/%s/temp1_input", e->d_name);
        f = fopen(p, "r"); if (!f) continue;
        int t = 0; if (fscanf(f, "%d", &t) == 1 && t / 1000 > best) best = t / 1000;
        fclose(f);
    }
    closedir(d);
    st->temp_c = best;
}

/* --- extra temps: x86 pkg (thermal zones), nvme + board (hwmon) --- */
static void read_zones(stats_t *st){
    st->pkg_temp = st->nvme_temp = st->board_temp = -1;
    DIR *d = opendir("/sys/class/thermal");
    if (d){
        struct dirent *e;
        while ((e = readdir(d))){
            if (strncmp(e->d_name, "thermal_zone", 12)) continue;
            char p[512]; snprintf(p, sizeof p, "/sys/class/thermal/%s/type", e->d_name);
            FILE *f = fopen(p, "r"); if (!f) continue;
            char ty[64] = ""; if (fgets(ty, sizeof ty, f)) ty[strcspn(ty, "\n")] = 0;
            fclose(f);
            if (strcmp(ty, "x86_pkg_temp")) continue;
            snprintf(p, sizeof p, "/sys/class/thermal/%s/temp", e->d_name);
            f = fopen(p, "r");
            if (f){ int t; if (fscanf(f, "%d", &t) == 1) st->pkg_temp = t / 1000; fclose(f); }
            break;
        }
        closedir(d);
    }
    d = opendir("/sys/class/hwmon");
    if (d){
        struct dirent *e;
        while ((e = readdir(d))){
            if (e->d_name[0] == '.') continue;
            char p[512]; snprintf(p, sizeof p, "/sys/class/hwmon/%s/name", e->d_name);
            FILE *f = fopen(p, "r"); if (!f) continue;
            char name[64] = ""; if (fgets(name, sizeof name, f)) name[strcspn(name, "\n")] = 0;
            fclose(f);
            int want_nvme = !strcmp(name, "nvme"), want_board = !strcmp(name, "acpitz");
            if (!want_nvme && !want_board) continue;
            snprintf(p, sizeof p, "/sys/class/hwmon/%s/temp1_input", e->d_name);
            f = fopen(p, "r"); if (!f) continue;
            int t;
            if (fscanf(f, "%d", &t) == 1){
                if (want_nvme  && st->nvme_temp  < 0) st->nvme_temp  = t / 1000;
                if (want_board && st->board_temp < 0) st->board_temp = t / 1000;
            }
            fclose(f);
        }
        closedir(d);
    }
    if (st->pkg_temp > 0) st->temp_c = st->pkg_temp;   /* prefer pkg zone */
}

/* --- fans: ACPI cooling devices (on/off only — no RPM on this box) --- */
static void read_fans(stats_t *st){
    st->fans_total = st->fans_on = 0;
    DIR *d = opendir("/sys/class/thermal"); if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))){
        if (strncmp(e->d_name, "cooling_device", 14)) continue;
        char p[512]; snprintf(p, sizeof p, "/sys/class/thermal/%s/type", e->d_name);
        FILE *f = fopen(p, "r"); if (!f) continue;
        char ty[64] = ""; if (fgets(ty, sizeof ty, f)) ty[strcspn(ty, "\n")] = 0;
        fclose(f);
        if (strcmp(ty, "Fan")) continue;
        st->fans_total++;
        snprintf(p, sizeof p, "/sys/class/thermal/%s/cur_state", e->d_name);
        f = fopen(p, "r");
        if (f){ int cs; if (fscanf(f, "%d", &cs) == 1 && cs > 0) st->fans_on++; fclose(f); }
    }
    closedir(d);
}

/* --- GPU busy via RC6 residency delta (gt0 ONLY — gt1 reads constant 0) --- */
static unsigned long long prev_rc6; static struct timespec prev_rc6_t;
/* --- power draw via Intel RAPL energy counters (psys = whole platform) --- */
static unsigned long long p_pkg_uj, p_sys_uj; static struct timespec p_pwr_t;
static void read_power(stats_t *st){
    st->pwr_sys_w = st->pwr_pkg_w = -1;
    unsigned long long pkg = 0, sys = 0;
    char path[128], name[32];
    for (int i = 0; i < 4; i++){
        snprintf(path, sizeof path, "/sys/class/powercap/intel-rapl:%d/name", i);
        FILE *f = fopen(path, "r"); if (!f) continue;
        name[0] = 0; if (fgets(name, sizeof name, f)) name[strcspn(name, "\n")] = 0;
        fclose(f);
        snprintf(path, sizeof path, "/sys/class/powercap/intel-rapl:%d/energy_uj", i);
        f = fopen(path, "r"); if (!f) continue;
        unsigned long long uj = 0;
        if (fscanf(f, "%llu", &uj) == 1){
            if (!strncmp(name, "package", 7)) pkg = uj;
            else if (!strcmp(name, "psys"))   sys = uj;
        }
        fclose(f);
    }
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    if (p_pwr_t.tv_sec){
        double dt = (now.tv_sec - p_pwr_t.tv_sec) + (now.tv_nsec - p_pwr_t.tv_nsec) / 1e9;
        if (dt > 0.2){
            if (pkg > p_pkg_uj) st->pwr_pkg_w = (pkg - p_pkg_uj) / dt / 1e6;
            if (sys > p_sys_uj) st->pwr_sys_w = (sys - p_sys_uj) / dt / 1e6;
        }
    }
    p_pkg_uj = pkg; p_sys_uj = sys; p_pwr_t = now;
}


#endif /* PANEL_STATS_SYSTEM_H */
