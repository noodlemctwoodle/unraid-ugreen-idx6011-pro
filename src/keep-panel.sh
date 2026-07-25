#!/bin/bash
# keep-panel.sh — keep the LCD dashboard alive AND keep the eDP a dedicated panel.
#
# The front LCD is an fbcon-bound framebuffer, so whenever panel_dash isn't drawing
# the Linux text console + getty render on it and loop/garble on the 258px width.
# This keeper stops that and self-heals the dashboard:
#
#   * fbcon BOUND while panel_dash modesets — panel_dash takes the eDP over from a
#     bound fbcon (its proven path); unbinding first can wedge the i915 modeset.
#   * once panel_dash owns the display, UNBIND fbcon — so if it later exits, the LCD
#     just holds its last frame instead of falling back to the flooding console.
#   * (re)launch panel_dash whenever it exits, with a backoff so a crash can't
#     tight-loop (a persistent fast-exit backs off to once a minute).
#
# Launched (setsid, backgrounded) by start-panel.sh; args pass through to panel_dash.
# Degrades safely: if the eDP isn't present it just idles.
BIN=/usr/local/bin/panel_dash
LOG=/var/log/panel_dash.log

edp_connected(){ [ "$(cat /sys/class/drm/card*-eDP-1/status 2>/dev/null | head -1)" = "connected" ]; }
fbcon_set(){   # $1 = 1 (bind, console on the LCD) | 0 (unbind, LCD is a DRM panel)
    for v in /sys/class/vtconsole/vtcon*; do
        grep -q "frame buffer device" "$v/name" 2>/dev/null &&
            [ "$(cat "$v/bind" 2>/dev/null)" != "$1" ] && echo "$1" > "$v/bind" 2>/dev/null
    done
}

fails=0
while :; do
    if ! edp_connected; then sleep 30; continue; fi
    [ -x "$BIN" ] || { sleep 10; continue; }
    fbcon_set 1                                   # bound -> clean modeset for panel_dash
    start=$(date +%s 2>/dev/null || echo 0)
    "$BIN" "$@" >>"$LOG" 2>&1 &                    # launch the dashboard
    pd=$!
    sleep 4                                        # let it modeset + own the eDP
    kill -0 "$pd" 2>/dev/null && fbcon_set 0       # up -> unbind so a later exit can't flood
    wait "$pd"                                     # block until the dashboard exits
    run=$(( $(date +%s 2>/dev/null || echo 0) - start ))
    # exited: fbcon is unbound, so the LCD holds its last frame (no console flood).
    [ "$run" -lt 6 ] && fails=$((fails + 1)) || fails=0
    if [ "$fails" -ge 5 ]; then sleep 60; else sleep 3; fi   # back off a crash-loop
done
