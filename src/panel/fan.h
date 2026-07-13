/*
 * plugin/src/panel/fan.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Fan monitoring + control via the ITE EC (ec.h). Drives ALL four fans on the
 * iDX6011 Pro. Register map (verified on hardware, matches the ug-fand reference):
 *   tach  (hi,lo) : cpufan1 0x34/35  cpufan2 0x36/37  sysfan1 0x38/39  sysfan2 0x3A/3B
 *   duty  (per fan: ENABLE byte = 1, then DUTY byte = 0..198 where 198 = 100%):
 *                   cpufan1 0xB0/B1  cpufan2 0xB2/B3   sysfan1 0xB4/B5  sysfan2 0xB6/B7
 *   To release a fan back to EC auto, write its ENABLE byte = 0.
 * (non-Pro: 2 system fans, tach 0x96, duty 0x9C.)
 *
 * CPU fans follow the CPU temp; the case/system fans follow the hottest disk.
 * A 25% floor keeps every fan spinning (below ~20% a case fan stalls on this
 * unit), and a hard clamp forces 100% at critical temps.
 *
 * WARNING: manual control means an uncatchable kill leaves the fans at their last
 * (>=25%, still spinning) duty until a restart re-asserts the curve. Default mode
 * is auto (we touch nothing).
 */
#ifndef PANEL_FAN_H
#define PANEL_FAN_H

#define FAN_CPU_BASE  0xB0     /* CPU fan pair enable/duty base */
#define FAN_SYS_BASE  0xB4     /* case/system fan pair enable/duty base */
#define FAN_DUTY_MAX  198      /* raw EC duty for 100% (0xC6) */
#define FAN_FLOOR_PCT 25       /* keep-spinning floor (verified on this unit) */
#define FAN_CPU_CRIT  88       /* CPU temp C -> force 100% */
#define FAN_DISK_CRIT 60       /* disk temp C -> force 100% */

/* read all fan tachometers (read-only). Pro: 4 fans @ 0x34; non-Pro: 2 @ 0x96. */
static void read_fan_rpm(stats_t *st){
    static int base = -1, nfan = 0;
    if (base < 0){
        char pn[64] = ""; FILE *f = fopen("/sys/class/dmi/id/product_name", "r");
        if (f){ if (fgets(pn, sizeof pn, f)) pn[strcspn(pn, "\n")] = 0; fclose(f); }
        if (strstr(pn, "Pro")){ base = 0x34; nfan = 4; }
        else                  { base = 0x96; nfan = 2; }
    }
    st->n_fan_rpm = nfan > 4 ? 4 : nfan;
    for (int i = 0; i < st->n_fan_rpm; i++)
        st->fan_rpm[i] = ec_read(base + i * 2) * 256 + ec_read(base + i * 2 + 1);
}

/* ---- curve control (cfg_fan_mode: 0 auto, 1 silent, 2 quiet, 3 turbo) ---- */
static int g_fan_manual = 0;

static int fan_curve(const int *c, int n, int t){   /* interpolate {temp,pct} points */
    if (t <= c[0]) return c[1];
    for (int i = 1; i < n; i++)
        if (t <= c[i * 2]){ int t0 = c[(i-1)*2], d0 = c[(i-1)*2+1], t1 = c[i*2], d1 = c[i*2+1];
                            return d0 + (d1 - d0) * (t - t0) / (t1 - t0); }
    return c[(n - 1) * 2 + 1];
}
static void set_fan_pair(unsigned char base, int pct){   /* enable + duty for both fans */
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    int raw = pct * FAN_DUTY_MAX / 100;
    /* write DUTY before ENABLE so enable=1 never coincides with a stale 0 duty
     * (that combo is exactly what stops a fan). */
    ec_write(base + 1, (unsigned char)raw); ec_write(base,     1);
    ec_write(base + 3, (unsigned char)raw); ec_write(base + 2, 1);
}
static void fan_free(unsigned char base){ ec_write(base, 0); ec_write(base + 2, 0); }  /* -> EC auto */

static int fan_max_disk_temp(stats_t *st){
    int m = -1;
    for (int i = 0; i < st->n_disks; i++) if (st->disks[i].temp > m) m = st->disks[i].temp;
    return m;
}
static void fan_apply(stats_t *st){
    if (cfg_fan_mode <= 0){                          /* auto — hand both pairs back to the EC */
        if (g_fan_manual){ fan_free(FAN_CPU_BASE); fan_free(FAN_SYS_BASE); g_fan_manual = 0; }
        return;
    }
    /* {temp,pct} curves. CPU pair follows CPU temp; system pair follows disk temp. */
    static const int cpu_silent[] = {0,25, 60,25, 72,50, 80,75, 88,100};
    static const int cpu_quiet[]  = {0,30, 55,30, 68,55, 78,80, 86,100};
    static const int cpu_turbo[]  = {0,45, 50,45, 65,75, 75,95, 82,100};
    static const int sys_silent[] = {0,25, 45,25, 52,50, 58,80, 62,100};
    static const int sys_quiet[]  = {0,30, 42,30, 50,55, 56,82, 62,100};
    static const int sys_turbo[]  = {0,45, 40,45, 48,78, 54,96, 60,100};
    const int *cc = cfg_fan_mode == 1 ? cpu_silent : cfg_fan_mode == 3 ? cpu_turbo : cpu_quiet;
    const int *sc = cfg_fan_mode == 1 ? sys_silent : cfg_fan_mode == 3 ? sys_turbo : sys_quiet;
    int ct = st->temp_c, dt = fan_max_disk_temp(st);
    int cp = fan_curve(cc, 5, ct);
    int sp = dt < 0 ? FAN_FLOOR_PCT : fan_curve(sc, 5, dt);
    if (ct >= FAN_CPU_CRIT)  cp = 100;               /* hard clamps */
    if (dt >= FAN_DISK_CRIT) sp = 100;
    if (cp < FAN_FLOOR_PCT) cp = FAN_FLOOR_PCT;      /* never stall */
    if (sp < FAN_FLOOR_PCT) sp = FAN_FLOOR_PCT;
    /* RPM watchdog: if a fan we're already driving reads a dead stop, force its
     * pair to 100% until it spins again. Last line of defence over the duty floor
     * for a stuck/failing fan; readings settle nonzero at any steady duty, so this
     * only ever fires on a genuine stall (or a brief ramp transient — harmless). */
    if (g_fan_manual){
        if (st->n_fan_rpm >= 2 && (st->fan_rpm[0] == 0 || st->fan_rpm[1] == 0)) cp = 100;
        if (st->n_fan_rpm >= 4 && (st->fan_rpm[2] == 0 || st->fan_rpm[3] == 0)) sp = 100;
    }
    set_fan_pair(FAN_CPU_BASE, cp);
    set_fan_pair(FAN_SYS_BASE, sp);
    g_fan_manual = 1;
}
static void fan_restore(void){   /* hand all fans back to EC auto on clean exit */
    if (g_fan_manual){ fan_free(FAN_CPU_BASE); fan_free(FAN_SYS_BASE); g_fan_manual = 0; }
}

#endif /* PANEL_FAN_H */
