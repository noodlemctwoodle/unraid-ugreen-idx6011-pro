/*
 * plugin/src/panel/stats.h
 *
 * Created by noodlemctwoodle on 12/07/2026.
 *
 * stats_t + related structs, size limits, monotonic clock
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_STATS_H
#define PANEL_STATS_H

/* ---------- stats ---------- */
#define MAX_IFACES 6
#define MAX_DISKS  12
#define MAX_CTRS   20
#define MAX_VMS    16
#define MAX_SHARES 32
#define MAX_POOLS  8
#define MAX_UD     16

typedef struct {
    char name[32], ip[44], ip6[48];
    int up;
    int link_mbps;                   /* link speed in Mbps (0 = unknown / down) */
    unsigned long long rx_tot, tx_tot;
    double rx_kbs, tx_kbs;
} iface_t;

typedef struct {
    char name[32], type[16];
    int present, np, spun, health;   /* health: 0 ok, 1 warn, 2 bad, 3 grey */
    int temp;                        /* C, -1 = unavailable */
    long long errors;
    unsigned long long fs_size, fs_used, size_kib;   /* KiB */
} disk_t;

typedef struct {
    char name[40];
    char state[16];                  /* running / exited / paused / ...      */
    char status[48];                 /* "Up 3 hours" free text               */
    char image[72];                  /* image ref, e.g. "linuxserver/plex"   */
    char ip[44];                     /* first non-empty container IP; "" host */
    int  update;                     /* 1 = image update available (Unraid)  */
} ctr_t;

typedef struct {
    char name[48];
    int  running;                    /* 1 = a libvirt .pid exists            */
} vm_t;

typedef struct {
    char host[64], ip[48], arr[32];
    double cpu; long mem_used_mb, mem_tot_mb;
    double rx_kbs, tx_kbs;
    double disk_used_pct; double disk_tot_gb, disk_used_gb;
    int temp_c; long up_s;
    char prim_if[32];
    int n_ifaces; iface_t ifc[MAX_IFACES];
    int n_disks;  disk_t disks[MAX_DISKS];
    int gpu_avail; double gpu_busy; int gpu_freq;
    double pwr_sys_w, pwr_pkg_w;          /* RAPL watts; <0 = n/a */
    char cpu_model[96]; int cpu_threads;
    long mem_free_mb, mem_cache_mb;
    double boot_pct, log_pct, docker_pct;  /* <0 = n/a */
    int npu_avail; double npu_busy; int npu_freq, npu_max_freq;
    unsigned long long npu_mem;
    int fans_total, fans_on;
    int n_fan_rpm; int fan_rpm[4];   /* EC tachometers (RPM), read-only */
    int docker;                      /* running count, -1 = n/a */
    int docker_total;                /* total containers, -1 = n/a */
    int n_ctrs; ctr_t ctrs[MAX_CTRS];
    int vm_enabled, vm_count;
    int n_vms; vm_t vms[MAX_VMS];
    int pkg_temp, nvme_temp, board_temp;
    char version[32], kernel[64];
    char reg_type[16], reg_to[40];   /* Unraid licence type (Pro/Plus/Trial) + registered-to */
    int  os_update;                  /* 1 = a newer Unraid OS is available   */
    char os_new_ver[32];             /* the available OS version             */
    int  plugin_updates;             /* count of plugins with pending updates */
    unsigned long long resync, resync_pos, resync_size; char resync_act[32];
    double resync_mbs;
    int md_bad;                      /* disabled + invalid + missing */
    long long sync_errs;
    int mover;
    int  transfer_active;            /* Dynamix File Manager copy/move in progress */
    char transfer_op[16];            /* operation title, e.g. "Copy" / "Move" */
    char transfer_dest[40];          /* destination folder (basename) */
    double transfer_pct;             /* overall progress, 0..100 */
    char transfer_speed[16];         /* rsync rate token, e.g. "128.42MB/s" */
    char transfer_eta[12];           /* rsync ETA, e.g. "5:33:40" */
    double io_rd_mbs, io_wr_mbs;     /* whole-device disk read/write throughput, MB/s */
    double load1, load5, load15;     /* system load average */
    int n_shares; struct { char name[28]; char where[16]; double free_gb; int health; } shares[MAX_SHARES];
    int n_pools;  struct { char name[24]; double used_gb, tot_gb, pct; } pools[MAX_POOLS];
    int n_ud;     struct { char name[28]; double used_gb, tot_gb, pct; } ud[MAX_UD];
    int ups_present; char ups_status[16]; double ups_charge, ups_load, ups_runtime, ups_linev;
    int notif_count, notif_imp;      /* imp: 0 normal, 1 warning, 2 alert */
    char notif_subj[128];
} stats_t;

static long now_ms(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}


#endif /* PANEL_STATS_H */
