#!/bin/bash
# assert-boot.sh — UGOS-INDEPENDENT front-panel boot provisioning.
#
# The BIOS powers the front-panel rail only when it boots a *registered* EFI
# entry — NOT the auto-generated removable-media "UEFI OS" fallback. So we
# register a named NVRAM entry for the USB flash itself and keep it first in the
# boot order, and make the flash's default syslinux entry chain the wake-probe
# module overlay. This needs no UGOS, no NVMe, no grub — a wiped-UGOS box works.
#
# Idempotent; run at every boot by start-panel.sh. Best-effort throughout
# (never fails the boot). A UGOS grub fallback, if present, is handled separately
# by assert-grub.sh.
set +e
LABEL="Unraid (iDX6011 panel)"

# --- locate the USB flash (Unraid boot device, label UNRAID) ---
USB=$(blkid -L UNRAID 2>/dev/null)
[ -b "$USB" ] || exit 0
PART=$(basename "$USB")
if [[ "$PART" =~ p[0-9]+$ ]]; then DISK="/dev/${PART%p*}"; PNUM="${PART##*p}"
else                                DISK="/dev/${PART%%[0-9]*}"; PNUM="${PART##*[a-z]}"; fi
[ -b "$DISK" ] || exit 0

# --- 1. flash syslinux default must chain the wake-probe overlay -------------
# Only when the matching-kernel overlay is staged (else a missing initrd would
# break boot); plain initrd=/bzroot otherwise (panel dark, boots fine).
CFG=/boot/syslinux/syslinux.cfg
if [ -f "$CFG" ]; then
  if [ -f /boot/bzroot-wakefix ]; then WANT='initrd=/bzroot,/bzroot-wakefix'
  else                                 WANT='initrd=/bzroot'; fi
  if ! grep -q "^  append $WANT\$" "$CFG"; then
    awk -v want="$WANT" '
      /^label / { u = ($0 == "label Unraid OS") }
      /^  menu default[ \t]*$/ { next }               # strip existing defaults
      {
        if (u && $0 ~ /^  append /) {
          print "  menu default"
          print "  append " want
          u = 0; next
        }
        print
      }
    ' "$CFG" > "$CFG.new" && mv "$CFG.new" "$CFG"
    # mirror into the UEFI copy if it is a full config (not an include stub)
    ECFG=/boot/EFI/boot/syslinux.cfg
    if [ -f "$ECFG" ] && ! grep -q '^include ' "$ECFG"; then cp "$CFG" "$ECFG"; fi
    logger -t ugreen-panel "syslinux default set to: $WANT"
    sync
  fi
fi

# --- 2. register a named EFI entry for the USB and put it first -------------
command -v efibootmgr >/dev/null || exit 0
find_entry(){ efibootmgr | grep -F "* $LABEL	" | head -1 | sed 's/^Boot//; s/\*.*//'; }
NUM=$(find_entry)
if [ -z "$NUM" ]; then
  efibootmgr -c -d "$DISK" -p "$PNUM" -L "$LABEL" -l '\EFI\BOOT\BOOTX64.EFI' >/dev/null 2>&1
  NUM=$(find_entry)
  logger -t ugreen-panel "registered EFI entry $NUM ($LABEL)"
fi
[ -n "$NUM" ] || exit 0

# put our entry first (firmware may reshuffle; on a UGOS-wiped box we are the
# only bootable entry, so it wins regardless)
CUR=$(efibootmgr | awk -F': ' '/^BootOrder/{print $2}')
if [ "${CUR%%,*}" != "$NUM" ]; then
  REST=$(echo "$CUR" | tr ',' '\n' | grep -v "^$NUM\$" | paste -sd, -)
  efibootmgr -o "$NUM${REST:+,$REST}" >/dev/null 2>&1 \
    && logger -t ugreen-panel "BootOrder: $NUM first"
fi
exit 0
