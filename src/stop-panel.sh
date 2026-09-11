#!/bin/bash
# stop-panel.sh — stop the dashboard daemon (LCD shows console again after fbcon
# reclaims the CRTC on next VT activity; backlight left as-is).
# NOTE: deliberately does NOT touch the boot chain (/boot/bzroot-wakefix, the EFI
# entry, BootOrder) — the flash's syslinux default loads bzroot-wakefix, so removing
# that file would break the boot. Full manual uninstall is documented in
# docs/front-panel-blueprint.md.
P=/boot/config/plugins/ugreen-idx6011-pro
PIDFILE=/run/ugreen-panel.pid
is_keeper(){
    [ "$(tr '\0' '\n' 2>/dev/null < "/proc/$1/cmdline" | sed -n '2p')" = "$P/keep-panel.sh" ]
}

pid=$(cat "$PIDFILE" 2>/dev/null)
is_keeper "$pid" && pids=$pid ||
    pids=$(pgrep -f "^bash $P/keep-panel[.]sh( |$)")
for pid in $pids; do is_keeper "$pid" && kill "$pid" 2>/dev/null; done
for _ in 1 2 3 4 5; do
    running=
    for pid in $pids; do is_keeper "$pid" && running=1; done
    [ -z "$running" ] && break
    sleep 1
done
for pid in $pids; do is_keeper "$pid" && kill -9 "$pid" 2>/dev/null; done
for pid in $pids; do
        [ "$(cat "$PIDFILE" 2>/dev/null)" = "$pid" ] && rm -f "$PIDFILE"
done
pkill -x panel_dash 2>/dev/null
rm -f /usr/local/bin/panel_dash
# rebind fbcon so the text console returns on the LCD (keep-panel.sh unbound it)
for v in /sys/class/vtconsole/vtcon*; do
    grep -q "frame buffer device" "$v/name" 2>/dev/null && echo 1 > "$v/bind" 2>/dev/null
done
exit 0
