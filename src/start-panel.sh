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
BRIGHTNESS=75; INTERVAL=1; ROTATE=0; DISABLE_WAKEFIX=0
[ -f $PANEL/settings.cfg ] && . $PANEL/settings.cfg

# ---- display wake-probe overlay: only ever staged when actually needed ----
# The overlay swaps the stock i915 driver for a patched one so this panel's eDP
# wakes on kernels that dropped the DPCD probe it depends on (docs/SOLUTION.md).
# Because it REPLACES i915 system-wide, it can break other iGPU consumers (hardware
# transcoding, intel_gpu_top, etc — see issue #20) on units that don't actually need
# it. So it is skipped/removed whenever: the user disabled it (DISABLE_WAKEFIX=1),
# it was already found unnecessary on this box, or eDP is already connected on the
# stock driver with no overlay staged yet (BIOS pre-trained the bridge fine).
NEEDS_FLAG="$PANEL/.wakefix-not-needed"
edp_status(){ cat /sys/class/drm/card*-eDP-1/status 2>/dev/null | head -1; }
if [ "$DISABLE_WAKEFIX" = "1" ]; then
    [ -f /boot/bzroot-wakefix ] && rm -f /boot/bzroot-wakefix \
        && echo "$(date) display wake overlay disabled by setting; stock i915 restored (reboot to apply)" >> $LOG
elif [ -f "$NEEDS_FLAG" ]; then
    rm -f /boot/bzroot-wakefix
elif [ ! -f /boot/bzroot-wakefix ] && [ "$(edp_status)" = "connected" ]; then
    touch "$NEEDS_FLAG" 2>/dev/null
    echo "$(date) eDP already connected on the stock i915 driver - display wake overlay not needed, will not be staged" >> $LOG
elif [ -f "$PANEL/overlay/$KV/bzroot-wakefix" ]; then
    cmp -s "$PANEL/overlay/$KV/bzroot-wakefix" /boot/bzroot-wakefix || {
        cp "$PANEL/overlay/$KV/bzroot-wakefix" /boot/bzroot-wakefix
        echo "$(date) staged overlay for $KV" >> $LOG
    }
elif [ ! -f /boot/bzroot-wakefix ]; then
    notify warning "No display-module overlay for kernel $KV. Run plugin/boot/build-overlay.sh, then reboot."
fi

# ---- keep the panel boot path healthy (self-registered EFI entry) ----
bash $P/assert-boot.sh 2>>$LOG

# ---- touch I2C stack (per-kernel out-of-tree modules) ----
modprobe mfd_core 2>/dev/null
for m in intel-lpss intel-lpss-pci i2c-designware-core i2c-designware-platform; do
    [ -f "$PANEL/modules/$KV/$m.ko" ] && insmod "$PANEL/modules/$KV/$m.ko" 2>/dev/null
done
modprobe i2c-dev 2>/dev/null

# ---- start the dashboard only if the panel actually came up ----
if [ "$(edp_status)" = "connected" ]; then
    pkill -f "keep-panel.sh" 2>/dev/null; pkill -x panel_dash 2>/dev/null; sleep 1
    cp $PANEL/panel_dash /usr/local/bin/panel_dash && chmod +x /usr/local/bin/panel_dash
    ARGS="--backlight $BRIGHTNESS --interval $INTERVAL"
    [ "$ROTATE" -gt 0 ] 2>/dev/null && ARGS="$ARGS --rotate $ROTATE"
    # a keeper owns the launch: it unbinds fbcon so the LCD is a DEDICATED DRM panel
    # (never the flooding text console when the dashboard isn't drawing) and respawns
    # panel_dash if it dies, with a backoff so a crash can't tight-loop.
    ( sleep 5; setsid bash $P/keep-panel.sh $ARGS </dev/null >>$LOG 2>&1 ) </dev/null >/dev/null 2>&1 &
    disown 2>/dev/null
    echo "$(date) panel keeper starting ($ARGS)" >> $LOG
else
    echo "$(date) eDP-1 not connected — booted via USB path or overlay missing; dashboard skipped" >> $LOG
    notify warning "Reboot to activate the LCD. If it stays dark, set the BIOS boot priority: Boot > UEFI USB Hard Disk Drive BBS Priorities > 'Unraid (iDX6011 panel)', then reboot."
fi
exit 0
