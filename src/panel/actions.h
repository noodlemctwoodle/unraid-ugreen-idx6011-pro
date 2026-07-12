/*
 * plugin/src/panel/actions.h
 *
 * Created by Toby G on 12/07/2026.
 *
 * non-blocking action spawns (restart / reboot / shutdown / leds)
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_ACTIONS_H
#define PANEL_ACTIONS_H

/* ---------- non-blocking action spawns ---------- */
static char **g_argv;               /* saved for RESTART DASH re-exec */

static void spawn_shell(const char *cmd){
    pid_t pid = fork();
    if (pid == 0){
        setsid();
        int nul = open("/dev/null", O_RDWR);
        if (nul >= 0){ dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); if (nul > 2) close(nul); }
        execl("/bin/bash", "bash", "-c", cmd, (char*)NULL);
        _exit(127);
    }
}
static void do_restart_dash(void){
    execv("/proc/self/exe", g_argv);   /* fds are all O_CLOEXEC */
    execv(g_argv[0], g_argv);          /* fallback */
}
static void do_reboot(void){ spawn_shell("/sbin/reboot"); }
static void do_shutdown(void){
    if (access("/usr/local/sbin/powerdown", X_OK) == 0)
        spawn_shell("/usr/local/sbin/powerdown");
    else
        spawn_shell("/sbin/poweroff");
}
static void apply_leds(void){
    spawn_shell(cfg_leds
        ? "bash /boot/config/plugins/ugreen-idx6011-pro/start.sh"
        : "bash /boot/config/plugins/ugreen-idx6011-pro/stop.sh");
}


#endif /* PANEL_ACTIONS_H */
