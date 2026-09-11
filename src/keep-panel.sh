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
#   * EXCEPTION, opt-in via settings.cfg (EXTMON=1, default 0/off): if an external
#     (non-eDP) monitor is connected, i915 drives both outputs from one framebuffer
#     sized to the smallest connector (the 258x960 panel), so leaving fbcon unbound
#     would either flood the LCD or, on the monitor, freeze a 32x60 console strip
#     that takes no input. With EXTMON=1 the keeper instead keeps fbcon BOUND and
#     fbset's the console to the external connector's preferred mode, so the local
#     text console on the monitor stays live and full-size. panel_dash still holds
#     DRM master on the eDP, so this doesn't disturb the dashboard. Trade-off: a
#     panel_dash crash then shows the (correctly sized) console on the monitor for
#     the few seconds until the keeper respawns it — hence opt-in, default off.
#   * (re)launch panel_dash whenever it exits, with a backoff so a crash can't
#     tight-loop (a persistent fast-exit backs off to once a minute).
#
# Launched (setsid, backgrounded) by start-panel.sh; args pass through to panel_dash.
# Degrades safely: if the eDP isn't present it just idles.
BIN=/usr/local/bin/panel_dash
LOG=/var/log/panel_dash.log
CFG=/boot/config/plugins/ugreen-idx6011-pro/panel/settings.cfg

cfg_get(){ [ -f "$CFG" ] && grep -E "^$1=" "$CFG" 2>/dev/null | tail -1 | cut -d= -f2- | tr -d '"\r'; }
EXTMON=$(cfg_get EXTMON); EXTMON="${EXTMON//[[:space:]]/}"; EXTMON=${EXTMON:-0}

edp_connected(){ [ "$(cat /sys/class/drm/card*-eDP-1/status 2>/dev/null | head -1)" = "connected" ]; }
fbcon_set(){   # $1 = 1 (bind, console on the LCD) | 0 (unbind, LCD is a DRM panel)
    for v in /sys/class/vtconsole/vtcon*; do
        grep -q "frame buffer device" "$v/name" 2>/dev/null &&
            [ "$(cat "$v/bind" 2>/dev/null)" != "$1" ] && echo "$1" > "$v/bind" 2>/dev/null
    done
}
# first CONNECTED DRM connector that isn't the eDP panel, e.g. "card0-DP-1"; empty if none
ext_connector(){
    local c conn
    for c in /sys/class/drm/card*-*; do
        [ -e "$c/status" ] || continue
        conn=$(basename "$c")
        case "$conn" in *-eDP-*|*-Writeback-*) continue ;; esac
        [ "$(cat "$c/status" 2>/dev/null)" = "connected" ] && { echo "$conn"; return 0; }
    done
    return 1
}
# bind fbcon + fbset the console to $1's preferred (first-listed) mode; rc!=0 if unreadable
console_to_external(){
    local mode w h
    mode=$(head -n1 "/sys/class/drm/$1/modes" 2>/dev/null)
    # "<w>x<h>" prefix only; deliberately ignore any suffix (e.g. "i", "-60") since
    # that's just refresh-rate/interlace metadata this call doesn't need.
    [[ "$mode" =~ ^([0-9]+)x([0-9]+) ]] || return 1
    w=${BASH_REMATCH[1]}; h=${BASH_REMATCH[2]}
    [ "$w" -gt 0 ] && [ "$h" -gt 0 ] || return 1
    command -v fbset >/dev/null 2>&1 || return 1   # no fbset -> can't resize; let the caller fall back
    # depth: read the framebuffer's CURRENT bpp rather than assuming 32, so this
    # tracks whatever depth i915 already left the shared framebuffer at; 32 is only
    # the fallback if that's unreadable. Virtual == visible size (no pan/scroll
    # buffer): the console doesn't need one, and it keeps the geometry call simple.
    local bpp; bpp=$(cat /sys/class/graphics/fb0/bits_per_pixel 2>/dev/null); bpp=${bpp:-32}
    fbset -g "$w" "$h" "$w" "$h" "$bpp" 2>/dev/null || return 1
    fbcon_set 1
    return 0   # explicit: fbcon_set's own exit status isn't a reliable success signal
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
    if kill -0 "$pd" 2>/dev/null; then
        ext=""
        [ "$EXTMON" = "1" ] && ext=$(ext_connector)
        if [ -n "$ext" ] && console_to_external "$ext"; then
            :   # left bound + resized to the external monitor's native mode
        else
            fbcon_set 0                             # up -> unbind so a later exit can't flood
        fi
    fi
    wait "$pd"                                     # block until the dashboard exits
    run=$(( $(date +%s 2>/dev/null || echo 0) - start ))
    # exited: fbcon is unbound (default), so the LCD holds its last frame with no
    # console flood; with EXTMON=1 + a monitor it stayed bound, so the console
    # briefly shows on the monitor (not the LCD) until the respawn below.
    [ "$run" -lt 6 ] && fails=$((fails + 1)) || fails=0
    if [ "$fails" -ge 5 ]; then sleep 60; else sleep 3; fi   # back off a crash-loop
done
