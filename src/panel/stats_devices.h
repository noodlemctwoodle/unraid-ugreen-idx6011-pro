/*
 * plugin/src/panel/stats_devices.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * cpu identity, mem breakdown, gpu, npu, docker, vms probes
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_STATS_DEVICES_H
#define PANEL_STATS_DEVICES_H

/* --- cpu identity, memory breakdown, fs usage --- */
static double fs_pct(const char *path){
    struct statvfs v;
    if (statvfs(path, &v) != 0 || v.f_blocks == 0) return -1;
    return 100.0 * (double)(v.f_blocks - v.f_bfree) / (double)v.f_blocks;
}
static void read_about(stats_t *st){
    if (!st->cpu_model[0]){
        FILE *f = fopen("/proc/cpuinfo", "r");
        if (f){
            char line[256];
            while (fgets(line, sizeof line, f)){
                if (!strncmp(line, "model name", 10)){
                    char *c = strchr(line, ':');
                    if (c){
                        c += 2; line[strcspn(line, "\n")] = 0;
                        char *r = strstr(c, "Intel(R) Core(TM) ");
                        if (r) c = r + 18;
                        snprintf(st->cpu_model, sizeof st->cpu_model, "%s", c);
                    }
                    break;
                }
            }
            fclose(f);
        }
        st->cpu_threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    }
    FILE *f = fopen("/proc/meminfo", "r");
    if (f){
        char k[64]; long v; char u[16];
        long freem = 0, cached = 0, buffers = 0;
        while (fscanf(f, "%63s %ld %15s", k, &v, u) >= 2){
            if      (!strcmp(k, "MemFree:")) freem = v;
            else if (!strcmp(k, "Buffers:")) buffers = v;
            else if (!strcmp(k, "Cached:")){ cached = v; break; }
        }
        fclose(f);
        st->mem_free_mb  = freem / 1024;
        st->mem_cache_mb = (cached + buffers) / 1024;
    }
    st->boot_pct   = fs_pct("/boot");
    st->log_pct    = fs_pct("/var/log");
    st->docker_pct = fs_pct("/var/lib/docker");
}

static void read_gpu(stats_t *st){
    unsigned long long rc6;
    if (!read_ull_file("/sys/class/drm/card0/gt/gt0/rc6_residency_ms", &rc6)){
        st->gpu_avail = 0; return;
    }
    st->gpu_avail = 1;
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    if (prev_rc6_t.tv_sec){
        double wall_ms = (now.tv_sec - prev_rc6_t.tv_sec) * 1e3
                       + (now.tv_nsec - prev_rc6_t.tv_nsec) / 1e6;
        if (wall_ms > 50 && rc6 >= prev_rc6){
            double busy = 100.0 * (1.0 - (double)(rc6 - prev_rc6) / wall_ms);
            st->gpu_busy = busy < 0 ? 0 : busy > 100 ? 100 : busy;
        }
    }
    prev_rc6 = rc6; prev_rc6_t = now;
    unsigned long long fr;
    st->gpu_freq = read_ull_file("/sys/class/drm/card0/gt_act_freq_mhz", &fr) ? (int)fr : -1;
}

/* --- NPU busy via cumulative busy-time delta --- */
static unsigned long long prev_npu; static struct timespec prev_npu_t;
static void read_npu(stats_t *st){
    unsigned long long busy_us;
    if (!read_ull_file("/sys/class/accel/accel0/device/npu_busy_time_us", &busy_us)){
        st->npu_avail = 0; return;
    }
    st->npu_avail = 1;
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    if (prev_npu_t.tv_sec){
        double wall_us = (now.tv_sec - prev_npu_t.tv_sec) * 1e6
                       + (now.tv_nsec - prev_npu_t.tv_nsec) / 1e3;
        if (wall_us > 50000 && busy_us >= prev_npu){
            double b = 100.0 * (double)(busy_us - prev_npu) / wall_us;
            st->npu_busy = b > 100 ? 100 : b;
        }
    }
    prev_npu = busy_us; prev_npu_t = now;
    unsigned long long v;
    st->npu_freq     = read_ull_file("/sys/class/accel/accel0/device/npu_current_frequency_mhz", &v) ? (int)v : -1;
    st->npu_max_freq = read_ull_file("/sys/class/accel/accel0/device/npu_max_frequency_mhz", &v) ? (int)v : -1;
    st->npu_mem      = read_ull_file("/sys/class/accel/accel0/device/npu_memory_utilization", &v) ? v : 0;
}

/* --- docker via unix socket (generic GET helper, v3) --- */
static char dkbuf[262144];
static int docker_http_get(const char *path){
    int fd = socket(AF_UNIX, SOCK_STREAM, 0); if (fd < 0) return -1;
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    struct sockaddr_un a; memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    strcpy(a.sun_path, "/var/run/docker.sock");
    if (connect(fd, (struct sockaddr*)&a, sizeof a)){ close(fd); return -1; }
    char req[256];
    snprintf(req, sizeof req, "GET %s HTTP/1.0\r\nHost: docker\r\n\r\n", path);
    if (write(fd, req, strlen(req)) < 0){ close(fd); return -1; }
    int n = 0, r;
    while (n < (int)sizeof dkbuf - 1 && (r = read(fd, dkbuf + n, sizeof dkbuf - 1 - n)) > 0) n += r;
    close(fd); dkbuf[n] = 0;
    if (!strstr(dkbuf, "200 OK")) return -1;
    return n;
}

/* copy a JSON string value that follows `p` up to the closing quote */
static void json_copy_str(const char *p, char *out, size_t outsz){
    size_t i = 0;
    while (*p && *p != '"' && i < outsz - 1){
        if (*p == '\\' && p[1]){ p++; }        /* keep escaped char verbatim */
        out[i++] = *p++;
    }
    out[i] = 0;
}

/*
 * Minimal hand-rolled scan of GET /containers/json?all=1.
 * Per object we extract Names[0] (leading '/' stripped), State, Status.
 * Fields are matched by key between this object's "Names" and the next,
 * so nested Health.Status (which appears later) can't shadow them.
 */
static void docker_parse(stats_t *st){
    st->n_ctrs = 0;
    char *body = strstr(dkbuf, "\r\n\r\n");
    if (!body) return;
    body += 4;
    char *p = body;
    while (st->n_ctrs < MAX_CTRS && (p = strstr(p, "\"Names\":[\""))){
        p += 10;
        if (*p == '/') p++;
        ctr_t *c = &st->ctrs[st->n_ctrs];
        memset(c, 0, sizeof *c);
        json_copy_str(p, c->name, sizeof c->name);
        char *next = strstr(p, "\"Names\":[\"");
        char *s = strstr(p, "\"State\":\"");
        if (s && (!next || s < next)) json_copy_str(s + 9, c->state, sizeof c->state);
        char *t = strstr(p, "\"Status\":\"");
        if (t && (!next || t < next)) json_copy_str(t + 10, c->status, sizeof c->status);
        st->n_ctrs++;
        if (!next) break;
        p = next;
    }
}

/* --- VMs: SERVICE gate in domain.cfg; running count = qemu pidfiles --- */
static void read_vms(stats_t *st){
    st->vm_enabled = 0; st->vm_count = 0;
    FILE *f = fopen("/boot/config/domain.cfg", "r");
    if (f){
        char line[256];
        while (fgets(line, sizeof line, f)){
            char *k, *v;
            if (!ini_kv(line, &k, &v)) continue;
            if (!strcmp(k, "SERVICE")){ st->vm_enabled = !strcmp(v, "enable"); break; }
        }
        fclose(f);
    }
    if (!st->vm_enabled) return;
    DIR *d = opendir("/var/run/libvirt/qemu"); if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))){
        size_t l = strlen(e->d_name);
        if (l > 4 && !strcmp(e->d_name + l - 4, ".pid")) st->vm_count++;
    }
    closedir(d);
}

/* cached 10s: full container list + running/total + VM state */
static void read_docker(stats_t *st){
    static long last = -100000;
    static int c_run = -1, c_tot = -1, c_n;
    static ctr_t c_list[MAX_CTRS];
    static int c_vm_en, c_vm_cnt;
    long nowms = now_ms();
    if (nowms - last >= 10000){
        last = nowms;
        if (docker_http_get("/containers/json?all=1") >= 0){
            docker_parse(st);
            c_n = st->n_ctrs; c_tot = st->n_ctrs; c_run = 0;
            for (int i = 0; i < st->n_ctrs; i++)
                if (!strcmp(st->ctrs[i].state, "running")) c_run++;
            memcpy(c_list, st->ctrs, sizeof c_list);
        } else {
            c_run = c_tot = -1; c_n = 0;
        }
        read_vms(st);
        c_vm_en = st->vm_enabled; c_vm_cnt = st->vm_count;
    }
    st->docker = c_run; st->docker_total = c_tot;
    st->n_ctrs = c_n;
    memcpy(st->ctrs, c_list, sizeof c_list);
    st->vm_enabled = c_vm_en; st->vm_count = c_vm_cnt;
}


#endif /* PANEL_STATS_DEVICES_H */
