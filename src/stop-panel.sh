#!/bin/bash
# stop-panel.sh — stop the dashboard daemon (LCD shows console again after fbcon
# reclaims the CRTC on next VT activity; backlight left as-is).
# NOTE: deliberately does NOT touch the boot chain (/boot/bzroot-wakefix, the EFI
# entry, BootOrder) — the flash's syslinux default loads bzroot-wakefix, so removing
# that file would break the boot. Full manual uninstall is documented in
# docs/front-panel-blueprint.md.
pkill -f "keep-panel.sh" 2>/dev/null   # stop the keeper first, or it respawns panel_dash
pkill -x panel_dash 2>/dev/null
rm -f /usr/local/bin/panel_dash
# rebind fbcon so the text console returns on the LCD (keep-panel.sh unbound it)
for v in /sys/class/vtconsole/vtcon*; do
    grep -q "frame buffer device" "$v/name" 2>/dev/null && echo 1 > "$v/bind" 2>/dev/null
done
exit 0
