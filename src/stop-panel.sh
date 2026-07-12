#!/bin/bash
# stop-panel.sh — stop the dashboard daemon (LCD shows console again after fbcon
# reclaims the CRTC on next VT activity; backlight left as-is).
# NOTE: deliberately does NOT touch the boot chain (/boot/bzroot-wakefix, grub
# entry, BootOrder) — the grub entry references bzroot-wakefix, so removing that
# file would break the default boot. Full manual uninstall is documented in
# docs/front-panel-blueprint.md.
pkill -x panel_dash 2>/dev/null
rm -f /usr/local/bin/panel_dash
exit 0
