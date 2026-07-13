/*
 * plugin/src/panel/fan.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Fan monitoring + control via the ITE EC (ec.h). READS all fan tachometers;
 * WRITES duty only for the CPU fans.
 *
 * Verified on the iDX6011 Pro (EC probe):
 *   tach  = 0x34 + fan*2  (16-bit big-endian RPM), duty = 0xB0 + fan*2 (0-255,
 *   0 = EC auto). Fan 0/1 = CPU fans; Fan 2/3 = system fans.
 *   * CPU fans (0xB0/0xB2): duty writes give reliable RPM (ec30->674 .. ec255->3600),
 *     min-spin ~ec30 (11%).
 *   * System fans (0xB4/0xB6): ANY manual duty drops their tach to 0 (can't confirm
 *     they keep spinning) — and they cool the disks. So we DO NOT drive them; they
 *     stay on EC auto (a safe ~750 rpm). "never stall" wins over "control all".
 *
 * WARNING: manual CPU-fan control means an uncatchable kill leaves the CPU fans at
 * their last duty until a restart re-asserts the curve. Default mode is auto (we
 * touch nothing), a hard clamp forces 100% above 87 C, and the floor never stalls.
 */
#ifndef PANEL_FAN_H
#define PANEL_FAN_H

#define FAN_DUTY0 0xB0                          /* CPU fan 0 duty */
#define FAN_DUTY1 0xB2                          /* CPU fan 1 duty */

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

/* ---- CPU-fan curve control (cfg_fan_mode: 0 auto, 1 silent, 2 quiet, 3 turbo) ---- */
static int g_fan_manual = 0;

static int fan_curve(const int *c, int n, int t){   /* interpolate {temp,pct} points */
    if (t <= c[0]) return c[1];
    for (int i = 1; i < n; i++)
        if (t <= c[i * 2]){ int t0 = c[(i-1)*2], d0 = c[(i-1)*2+1], t1 = c[i*2], d1 = c[i*2+1];
                            return d0 + (d1 - d0) * (t - t0) / (t1 - t0); }
    return c[(n - 1) * 2 + 1];
}
static void fan_apply(int cpu_temp){
    if (cfg_fan_mode <= 0){                          /* auto — release to the EC */
        if (g_fan_manual){ ec_write(FAN_DUTY0, 0); ec_write(FAN_DUTY1, 0); g_fan_manual = 0; }
        return;
    }
    static const int silent[] = {0,15, 60,15, 72,35, 80,65, 87,100};
    static const int quiet[]  = {0,18, 58,18, 68,40, 77,70, 85,100};
    static const int turbo[]  = {0,30, 55,30, 65,60, 74,90, 82,100};
    const int *c = cfg_fan_mode == 1 ? silent : cfg_fan_mode == 3 ? turbo : quiet;
    int pct = fan_curve(c, 5, cpu_temp);
    if (cpu_temp >= 87) pct = 100;                   /* hard safety clamp */
    if (pct < 15) pct = 15;                          /* floor — CPU fan min-spin ~11% */
    int ec = pct * 255 / 100; if (ec < 1) ec = 1; if (ec > 255) ec = 255;
    ec_write(FAN_DUTY0, (unsigned char)ec);
    ec_write(FAN_DUTY1, (unsigned char)ec);
    g_fan_manual = 1;
}
static void fan_restore(void){   /* hand the CPU fans back to EC auto */
    if (g_fan_manual){ ec_write(FAN_DUTY0, 0); ec_write(FAN_DUTY1, 0); g_fan_manual = 0; }
}

#endif /* PANEL_FAN_H */
