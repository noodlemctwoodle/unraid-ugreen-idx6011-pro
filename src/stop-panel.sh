#!/bin/bash
# stop-panel.sh — stop the dashboard daemon (LCD shows console again after fbcon
# reclaims the CRTC on next VT activity; backlight left as-is).
# NOTE: deliberately does NOT touch the boot chain (/boot/bzroot-wakefix, the EFI
# entry, BootOrder) — the flash's syslinux default loads bzroot-wakefix, so removing
# that file would break the boot. Full manual uninstall is documented in
# docs/front-panel-blueprint.md.
PIDFILE=/run/ugreen-panel.pid
pid=$(cat "$PIDFILE" 2>/dev/null)
case "$pid" in
    ''|*[!0-9]*) ;;
    *)
        kill "$pid" 2>/dev/null
        for _ in 1 2 3 4 5; do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
        kill -9 "$pid" 2>/dev/null
        [ "$(cat "$PIDFILE" 2>/dev/null)" = "$pid" ] && rm -f "$PIDFILE"
        ;;
esac
pkill -x panel_dash 2>/dev/null
rm -f /usr/local/bin/panel_dash
# rebind fbcon so the text console returns on the LCD (keep-panel.sh unbound it)
for v in /sys/class/vtconsole/vtcon*; do
    grep -q "frame buffer device" "$v/name" 2>/dev/null && echo 1 > "$v/bind" 2>/dev/null
done
exit 0
