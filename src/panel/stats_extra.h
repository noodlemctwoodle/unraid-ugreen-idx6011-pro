/*
 * plugin/src/panel/stats_extra.h
 *
 * Created by noodlemctwoodle on 15/07/2026.
 *
 * Additional stat readers for the newer cards — disk I/O, load average, shares,
 * pools, unassigned devices and UPS. Each degrades gracefully when its source is
 * absent (public plugin: any Unraid environment). Part of panel_dash: #included
 * by panel_dash.c after the other stats_*.h, before ui.h.
 */
#ifndef PANEL_STATS_EXTRA_H
#define PANEL_STATS_EXTRA_H

static void read_diskio(stats_t *st){
    st->io_rd_mbs = 0; st->io_wr_mbs = 0;
    static unsigned long long prev_rd, prev_wr;
    static struct timespec prev_t;
    static int primed = 0;
    unsigned long long sum_rd = 0, sum_wr = 0;
    FILE *f = fopen("/proc/diskstats", "r");
    if (f){
        char line[512];
        while (fgets(line, sizeof line, f)){
            int mj, mn; char name[64];
            unsigned long long rc, rm, sr, tr, wc, wm, sw;
            /* major minor name reads_completed reads_merged sectors_read
             * ms_reading writes_completed writes_merged sectors_written */
            if (sscanf(line, "%d %d %63s %llu %llu %llu %llu %llu %llu %llu",
                       &mj, &mn, name, &rc, &rm, &sr, &tr, &wc, &wm, &sw) != 10) continue;
            /* whole physical devices only (^sd[a-z]+$ or ^nvme[0-9]+n[0-9]+$),
             * never partitions, so read/write sectors aren't double-counted */
            size_t l = strlen(name); int whole = 0;
            if (l >= 3 && name[0] == 's' && name[1] == 'd'){
                whole = 1;
                for (size_t i = 2; i < l; i++)
                    if (name[i] < 'a' || name[i] > 'z'){ whole = 0; break; }
            } else if (l >= 6 && !strncmp(name, "nvme", 4)){
                size_t i = 4;
                if (isdigit((unsigned char)name[i])){
                    while (i < l && isdigit((unsigned char)name[i])) i++;
                    if (i < l && name[i] == 'n'){
                        i++;
                        if (i < l && isdigit((unsigned char)name[i])){
                            while (i < l && isdigit((unsigned char)name[i])) i++;
                            whole = (i == l);
                        }
                    }
                }
            }
            if (!whole) continue;
            sum_rd += sr; sum_wr += sw;
        }
        fclose(f);
    }
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    if (primed){
        double dt = (now.tv_sec - prev_t.tv_sec) + (now.tv_nsec - prev_t.tv_nsec) / 1e9;
        if (dt > 0.05){                                    /* sector = 512 bytes */
            if (sum_rd >= prev_rd) st->io_rd_mbs = (double)(sum_rd - prev_rd) * 512.0 / 1e6 / dt;
            if (sum_wr >= prev_wr) st->io_wr_mbs = (double)(sum_wr - prev_wr) * 512.0 / 1e6 / dt;
        }
    }
    prev_rd = sum_rd; prev_wr = sum_wr; prev_t = now; primed = 1;
}

static void read_load(stats_t *st){
    st->load1 = st->load5 = st->load15 = 0.0;
    FILE *f = fopen("/proc/loadavg", "r");
    if (!f) return;                                  /* file absent -> stays 0.00 */
    double a = 0, b = 0, c = 0;
    if (fscanf(f, "%lf %lf %lf", &a, &b, &c) == 3){
        st->load1 = a; st->load5 = b; st->load15 = c;
    }
    fclose(f);
}

/* --- user shares from /var/local/emhttp/shares.ini ---
 * Blocks like ["Media"] with key="value" lines. NOTE: shares.ini "used"/"free"
 * are the used/free of the POOL the share lives on (identical for every share on
 * that pool), NOT the share's own data — so we surface the share's PLACEMENT
 * (its cache pool, or the array) + the pool's free + health, which is accurate.
 * where: cachePool for cache-only/prefer shares, else "array". health: 0 green,
 * 2 red, else 1. Missing file / no blocks -> n_shares = 0 -> dim idle line. */
static void shares_commit(stats_t *st, const char *name, const char *pool,
                          const char *cache, double free_gb, int health){
    if (!name[0] || st->n_shares >= MAX_SHARES) return;
    const char *where = (pool[0] && (!strcmp(cache, "only") || !strcmp(cache, "prefer")))
                        ? pool : "array";
    int i = st->n_shares++;
    snprintf(st->shares[i].name,  sizeof st->shares[i].name,  "%s", name);
    snprintf(st->shares[i].where, sizeof st->shares[i].where, "%s", where);
    st->shares[i].free_gb = free_gb;
    st->shares[i].health  = health;
}
static void read_shares(stats_t *st){
    st->n_shares = 0;
    FILE *f = fopen("/var/local/emhttp/shares.ini", "r"); if (!f) return;
    char line[512], name[28] = "", pool[16] = "", cache[8] = ""; double free_gb = 0;
    int health = 1, have = 0;
    while (fgets(line, sizeof line, f)){
        if (line[0] == '['){
            if (have) shares_commit(st, name, pool, cache, free_gb, health);
            name[0] = 0; pool[0] = 0; cache[0] = 0; free_gb = 0; health = 1; have = 1;
            sscanf(line, "[\"%27[^\"]\"]", name);
            continue;
        }
        if (!have) continue;
        char *k, *v;
        if (!ini_kv(line, &k, &v)) continue;
        if      (!strcmp(k, "name"))      snprintf(name, sizeof name, "%s", v);
        else if (!strcmp(k, "free"))      free_gb = strtod(v, NULL) * 1024.0 / 1e9;
        else if (!strcmp(k, "useCache"))  snprintf(cache, sizeof cache, "%s", v);
        else if (!strcmp(k, "cachePool")) snprintf(pool, sizeof pool, "%s", v);
        else if (!strcmp(k, "color")){
            if      (strstr(v, "green")) health = 0;
            else if (strstr(v, "red"))   health = 2;
            else                         health = 1;
        }
    }
    if (have) shares_commit(st, name, pool, cache, free_gb, health);
    fclose(f);
}

static void read_pools(stats_t *st){
    st->n_pools = 0;
    FILE *f = fopen("/var/local/emhttp/disks.ini", "r"); if (!f) return;
    char line[512], name[24] = ""; int is_cache = 0;
    while (fgets(line, sizeof line, f)){
        if (line[0] == '['){                                 /* new block: commit the previous */
            if (is_cache && name[0] && st->n_pools < MAX_POOLS){
                char mnt[64]; struct statvfs vfs;
                snprintf(mnt, sizeof mnt, "/mnt/%s", name);
                if (statvfs(mnt, &vfs) == 0 && vfs.f_blocks > 0 && vfs.f_frsize > 0){
                    double tot  = (double)vfs.f_blocks * (double)vfs.f_frsize;
                    double used = tot - (double)vfs.f_bfree * (double)vfs.f_frsize;
                    if (tot > 0){
                        int i = st->n_pools++;
                        snprintf(st->pools[i].name, sizeof st->pools[i].name, "%s", name);
                        st->pools[i].tot_gb  = tot  / 1e9;
                        st->pools[i].used_gb = used / 1e9;
                        st->pools[i].pct     = 100.0 * used / tot;
                    }
                }
            }
            name[0] = 0; is_cache = 0;
            sscanf(line, "[\"%23[^\"]\"]", name);
            continue;
        }
        char *k, *v;
        if (!ini_kv(line, &k, &v)) continue;
        if (!strcmp(k, "type") && !strcmp(v, "Cache")) is_cache = 1;
    }
    if (is_cache && name[0] && st->n_pools < MAX_POOLS){     /* commit the final block */
        char mnt[64]; struct statvfs vfs;
        snprintf(mnt, sizeof mnt, "/mnt/%s", name);
        if (statvfs(mnt, &vfs) == 0 && vfs.f_blocks > 0 && vfs.f_frsize > 0){
            double tot  = (double)vfs.f_blocks * (double)vfs.f_frsize;
            double used = tot - (double)vfs.f_bfree * (double)vfs.f_frsize;
            if (tot > 0){
                int i = st->n_pools++;
                snprintf(st->pools[i].name, sizeof st->pools[i].name, "%s", name);
                st->pools[i].tot_gb  = tot  / 1e9;
                st->pools[i].used_gb = used / 1e9;
                st->pools[i].pct     = 100.0 * used / tot;
            }
        }
    }
    fclose(f);
}

static void read_unassigned(stats_t *st){
    st->n_ud = 0;
    DIR *d = opendir("/mnt/disks"); if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && st->n_ud < MAX_UD){
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char p[512]; snprintf(p, sizeof p, "/mnt/disks/%s", e->d_name);
        struct stat sb;
        if (stat(p, &sb) != 0 || !S_ISDIR(sb.st_mode)) continue;   /* directory = a UD mount point */
        struct statvfs v;
        if (statvfs(p, &v) != 0 || v.f_blocks == 0) continue;      /* skip failures / no fs */
        double tot = (double)v.f_blocks * v.f_frsize;
        double used = tot - (double)v.f_bfree * v.f_frsize;
        int i = st->n_ud++;
        snprintf(st->ud[i].name, sizeof st->ud[i].name, "%s", e->d_name);
        st->ud[i].tot_gb  = tot / 1e9;
        st->ud[i].used_gb = used / 1e9;
        st->ud[i].pct     = tot > 0 ? 100.0 * used / tot : 0;
    }
    closedir(d);
}

static void read_ups(stats_t *st){
    st->ups_present = 0;
    st->ups_status[0] = 0;
    st->ups_charge = st->ups_load = st->ups_runtime = st->ups_linev = 0;

    /* Cheap guard for the common no-UPS box: apcupsd not running -> no pid file. */
    struct stat sb;
    if (stat("/var/run/apcupsd.pid", &sb) != 0) return;

    FILE *f = popen("apcaccess status 2>/dev/null", "r");
    if (!f) return;

    char line[256];
    int seen_status = 0;
    while (fgets(line, sizeof line, f)){
        char *c = strchr(line, ':');
        if (!c) continue;
        int kl = (int)(c - line);
        while (kl > 0 && (line[kl - 1] == ' ' || line[kl - 1] == '\t')) kl--;
        if (kl > (int)sizeof(char[16]) - 1) kl = 15;
        char key[16];
        snprintf(key, sizeof key, "%.*s", kl, line);
        char *v = c + 1;
        while (*v == ' ' || *v == '\t') v++;
        char *e = v + strlen(v);
        while (e > v && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;

        if      (!strcmp(key, "STATUS")){ snprintf(st->ups_status, sizeof st->ups_status, "%s", v); seen_status = 1; }
        else if (!strcmp(key, "BCHARGE"))  st->ups_charge  = atof(v);
        else if (!strcmp(key, "LOADPCT"))  st->ups_load    = atof(v);
        else if (!strcmp(key, "TIMELEFT")) st->ups_runtime = atof(v);
        else if (!strcmp(key, "LINEV"))    st->ups_linev   = atof(v);
    }
    pclose(f);

    st->ups_present = seen_status ? 1 : 0;   /* only real if apcupsd answered with STATUS */
}

#endif /* PANEL_STATS_EXTRA_H */
