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
# it was already found unnecessary on this box (NEEDS_FLAG), or eDP is already
# connected while running on the stock driver (no overlay currently staged).
#
# Upgrades from older plugin versions may already have the overlay staged
# unconditionally, with no chance yet to have proven it's actually required — for
# those, EVALUATED_FLAG makes the check run exactly once: the overlay is pulled for
# one boot so the NEXT run can observe the TRUE stock-driver eDP status and either
# confirm the overlay is needed (re-staged, EVALUATED_FLAG set) or mark it
# unnecessary for good (NEEDS_FLAG set instead). A fresh install that stages the
# overlay for the first time sets EVALUATED_FLAG immediately, since it was just
# proven necessary and needs no separate re-test. If hardware changes later
# require the overlay again after being marked unnecessary, delete NEEDS_FLAG.
NEEDS_FLAG="$PANEL/.wakefix-not-needed"
EVALUATED_FLAG="$PANEL/.wakefix-evaluated"
edp_status(){ cat /sys/class/drm/card*-eDP-1/status 2>/dev/null | head -1; }
if [ "$DISABLE_WAKEFIX" = "1" ]; then
    [ -f /boot/bzroot-wakefix ] && rm -f /boot/bzroot-wakefix \
        && echo "$(date) display wake overlay disabled by setting; stock i915 restored (reboot to apply)" >> $LOG
elif [ -f "$NEEDS_FLAG" ]; then
    rm -f /boot/bzroot-wakefix
elif [ ! -f /boot/bzroot-wakefix ]; then
    if [ "$(edp_status)" = "connected" ]; then
        touch "$NEEDS_FLAG" 2>/dev/null
        echo "$(date) eDP already connected on the stock i915 driver - display wake overlay not needed, will not be staged" >> $LOG
    elif [ -f "$PANEL/overlay/$KV/bzroot-wakefix" ]; then
        cp "$PANEL/overlay/$KV/bzroot-wakefix" /boot/bzroot-wakefix
        touch "$EVALUATED_FLAG" 2>/dev/null   # confirmed needed just now — no redundant retest later
        echo "$(date) staged overlay for $KV" >> $LOG
    else
        notify warning "No display-module overlay for kernel $KV. Run plugin/boot/build-overlay.sh, then reboot."
    fi
elif [ ! -f "$EVALUATED_FLAG" ]; then
    touch "$EVALUATED_FLAG" 2>/dev/null
    rm -f /boot/bzroot-wakefix
    echo "$(date) re-testing whether the display wake overlay is actually required (one-time, needs a reboot)" >> $LOG
else
    cmp -s "$PANEL/overlay/$KV/bzroot-wakefix" /boot/bzroot-wakefix 2>/dev/null || {
        cp "$PANEL/overlay/$KV/bzroot-wakefix" /boot/bzroot-wakefix 2>/dev/null
        echo "$(date) staged overlay for $KV" >> $LOG
    }
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
