#!/bin/bash
# keep-panel.sh — keep the LCD dashboard alive AND keep the eDP quiet when it isn't.
#
#  1) Unbinds the framebuffer console (fbcon) from the eDP, so the front LCD is a
#     DEDICATED DRM panel: the Linux text console / getty can never render on it.
#     That's the fix for the "looping console" — when panel_dash isn't drawing, the
#     LCD just holds its last frame instead of falling back to the flooding login
#     console. (Also removes fbcon/DRM contention, which can crash the dashboard.)
#  2) (Re)launches panel_dash whenever it exits, with a backoff so a crash can't
#     tight-loop: a fast exit is counted, and a persistent crash backs off to once
#     a minute rather than hammering.
#
# Launched (setsid, backgrounded) by start-panel.sh; all args are passed through to
# panel_dash. Degrades safely everywhere — if the eDP isn't present it just idles.
BIN=/usr/local/bin/panel_dash
LOG=/var/log/panel_dash.log

edp_connected(){ [ "$(cat /sys/class/drm/card*-eDP-1/status 2>/dev/null | head -1)" = "connected" ]; }
unbind_fbcon(){
    for v in /sys/class/vtconsole/vtcon*; do
        grep -q "frame buffer device" "$v/name" 2>/dev/null &&
            [ "$(cat "$v/bind" 2>/dev/null)" = "1" ] && echo 0 > "$v/bind" 2>/dev/null
    done
}

fails=0
while :; do
    if ! edp_connected; then sleep 30; continue; fi   # no panel this boot -> idle
    unbind_fbcon                                       # eDP = DRM panel, never a console
    [ -x "$BIN" ] || { sleep 10; continue; }
    start=$(date +%s 2>/dev/null || echo 0)
    "$BIN" "$@" >>"$LOG" 2>&1                           # blocks while the dashboard runs
    run=$(( $(date +%s 2>/dev/null || echo 0) - start ))
    # panel_dash exited: with fbcon unbound the LCD holds its last frame (no flood).
    [ "$run" -lt 5 ] && fails=$((fails + 1)) || fails=0
    if [ "$fails" -ge 5 ]; then sleep 60; else sleep 3; fi   # back off a crash-loop
done
