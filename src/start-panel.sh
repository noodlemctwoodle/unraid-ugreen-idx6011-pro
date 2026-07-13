#!/bin/bash
# start-panel.sh — LCD dashboard bring-up. Run by the .plg on install and on every boot.
# Safe everywhere: hard-gated to the UGREEN iDX6011 Pro, and every step degrades
# gracefully (no panel -> no daemon, never an error that blocks boot).
P=/boot/config/plugins/ugreen-idx6011-pro
PANEL=$P/panel
KV=$(uname -r)
LOG=/var/log/panel_dash.log

notify(){ /usr/local/emhttp/webGui/scripts/notify -i "$1" -s "Front panel" -d "$2" 2>/dev/null; }

# ---- model gate ----
[ "$(cat /sys/class/dmi/id/sys_vendor 2>/dev/null)" = "UGREEN" ] || exit 0
[ "$(cat /sys/class/dmi/id/product_name 2>/dev/null)" = "iDX6011 Pro" ] || exit 0

# ---- settings (flash-persistent, webGUI-editable) ----
# panel_dash reads settings.cfg itself (incl. the WALLPAPER / LOGO image paths) and
# hot-reloads them live, so no --bg is passed here.
BRIGHTNESS=75; INTERVAL=1; ROTATE=0
[ -f $PANEL/settings.cfg ] && . $PANEL/settings.cfg

# ---- keep the panel boot path healthy (self-registered EFI entry) ----
bash $P/assert-boot.sh 2>>$LOG

# ---- stage the overlay matching THIS Unraid release for the next boot ----
if [ -f "$PANEL/overlay/$KV/bzroot-wakefix" ]; then
    cmp -s "$PANEL/overlay/$KV/bzroot-wakefix" /boot/bzroot-wakefix || {
        cp "$PANEL/overlay/$KV/bzroot-wakefix" /boot/bzroot-wakefix
        echo "$(date) staged overlay for $KV" >> $LOG
    }
elif [ ! -f /boot/bzroot-wakefix ]; then
    notify warning "No display-module overlay for kernel $KV. Run plugin/boot/build-overlay.sh, then reboot."
fi

# ---- touch I2C stack (per-kernel out-of-tree modules) ----
modprobe mfd_core 2>/dev/null
for m in intel-lpss intel-lpss-pci i2c-designware-core i2c-designware-platform; do
    [ -f "$PANEL/modules/$KV/$m.ko" ] && insmod "$PANEL/modules/$KV/$m.ko" 2>/dev/null
done
modprobe i2c-dev 2>/dev/null

# ---- start the dashboard only if the panel actually came up ----
if [ "$(cat /sys/class/drm/card*-eDP-1/status 2>/dev/null | head -1)" = "connected" ]; then
    pkill -x panel_dash 2>/dev/null; sleep 1
    cp $PANEL/panel_dash /usr/local/bin/panel_dash && chmod +x /usr/local/bin/panel_dash
    ARGS="--backlight $BRIGHTNESS --interval $INTERVAL"
    [ "$ROTATE" -gt 0 ] 2>/dev/null && ARGS="$ARGS --rotate $ROTATE"
    ( sleep 5; setsid /usr/local/bin/panel_dash $ARGS </dev/null >>$LOG 2>&1 ) </dev/null >/dev/null 2>&1 &
    disown 2>/dev/null
    echo "$(date) panel_dash starting ($ARGS)" >> $LOG
else
    echo "$(date) eDP-1 not connected — booted via USB path or overlay missing; dashboard skipped" >> $LOG
    notify warning "Reboot to activate the LCD. If it stays dark, set the BIOS boot priority: Boot > UEFI USB Hard Disk Drive BBS Priorities > 'Unraid (iDX6011 panel)', then reboot."
fi
exit 0
