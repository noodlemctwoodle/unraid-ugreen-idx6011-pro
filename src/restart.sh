#!/bin/bash
#
# ugreen-idx6011-pro / restart.sh
#
# Light re-apply after a config-page change (run as the /update.php #command):
# restart the LED monitor and relaunch the dashboard so both re-read settings.cfg.
# Skips the EFI/overlay/module provisioning that start-panel.sh does at boot.
#
P=/boot/config/plugins/ugreen-idx6011-pro
PANEL=$P/panel

# LED monitor — start.sh kills the old instance via its pidfile and relaunches
# monitor.sh, which re-reads the LED colours + power toggle from settings.cfg.
[ -f "$P/start.sh" ] && bash "$P/start.sh"

# Dashboard — relaunch the keep-panel.sh SUPERVISOR (not panel_dash directly) with
# current settings (no boot re-assert). Killing only panel_dash while keep-panel.sh
# keeps running would leave two processes racing to (re)spawn/own the dashboard: the
# keeper's own respawn AND this script's — both fighting for DRM master, causing a
# setcrtc crash-loop that resets the in-process idle timer and can lose settings
# changes made in the meantime. Always stop the keeper first (see stop-panel.sh).
[ "$(cat /sys/class/dmi/id/product_name 2>/dev/null)" = "iDX6011 Pro" ] || exit 0
BRIGHTNESS=75; INTERVAL=1; ROTATE=0
[ -f "$PANEL/settings.cfg" ] && . "$PANEL/settings.cfg"
pkill -f "keep-panel.sh" 2>/dev/null; pkill -x panel_dash 2>/dev/null; sleep 1
if [ "$(cat /sys/class/drm/card*-eDP-1/status 2>/dev/null | head -1)" = "connected" ]; then
    [ -f "$PANEL/panel_dash" ] && { cp "$PANEL/panel_dash" /usr/local/bin/panel_dash; chmod +x /usr/local/bin/panel_dash; }
    ARGS="--backlight $BRIGHTNESS --interval $INTERVAL"
    [ "${ROTATE:-0}" -gt 0 ] 2>/dev/null && ARGS="$ARGS --rotate $ROTATE"
    # wallpaper/logo come from settings.cfg (panel_dash reads + hot-reloads them).
    # A single keeper owns the launch, same as start-panel.sh, so there is never
    # more than one process contending for the eDP's DRM master.
    ( setsid bash "$P/keep-panel.sh" $ARGS </dev/null >>/var/log/panel_dash.log 2>&1 ) </dev/null >/dev/null 2>&1 &
    disown 2>/dev/null
fi
exit 0
