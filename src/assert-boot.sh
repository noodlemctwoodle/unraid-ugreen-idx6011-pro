#!/bin/bash
# assert-boot.sh — UGOS-INDEPENDENT front-panel boot provisioning.
#
# The BIOS powers the front-panel rail only when it boots a *registered* EFI
# entry — NOT the auto-generated removable-media "UEFI OS" fallback. So we
# register a named NVRAM entry for the boot device itself and keep it first in
# the boot order. On classic USB-flash boots we also make the flash's default
# syslinux entry chain the wake-probe module overlay. This needs no UGOS, no
# NVMe, no grub — a wiped-UGOS box works.
#
# Also supports Unraid 7.x "Internal Boot" (ZFS boot pool on NVMe/SATA, GRUB
# instead of syslinux, no USB flash at all): there is no block device labelled
# UNRAID in that case, so we fall back to the ESP backing the currently booted
# EFI entry (BootCurrent) to register our own named entry against.
#
# Idempotent; run at every boot by start-panel.sh. Best-effort throughout
# (never fails the boot).
set +e
LABEL="Unraid (iDX6011 panel)"

# --- locate the boot device's EFI System Partition ---------------------------
# Preferred: the USB flash labelled UNRAID (classic Unraid boot).
USB=$(blkid -L UNRAID 2>/dev/null)

# Fallback: Unraid 7.x Internal Boot (ZFS/GRUB, no USB flash). Identify the ESP
# backing the currently booted EFI entry (BootCurrent) via its GPT PARTUUID.
if [ ! -b "$USB" ] && command -v efibootmgr >/dev/null; then
  EFIBOOTMGR_V=$(efibootmgr -v 2>/dev/null)
  BOOTCUR=$(printf '%s\n' "$EFIBOOTMGR_V" | awk -F': ' '/^BootCurrent/{print $2}')
  if [ -n "$BOOTCUR" ]; then
    CURLINE=$(printf '%s\n' "$EFIBOOTMGR_V" | grep "^Boot${BOOTCUR}\*")
    PARTUUID=$(printf '%s\n' "$CURLINE" | grep -oE 'HD\([0-9]+,GPT,[0-9A-Fa-f-]+' | sed -E 's/HD\([0-9]+,GPT,//')
    [ -n "$PARTUUID" ] && USB=$(blkid --match-token PARTUUID="$PARTUUID" -o device 2>/dev/null)
  fi
fi

if [ ! -b "$USB" ]; then
  logger -t ugreen-panel "assert-boot: no UNRAID-labelled device and no active ESP found via BootCurrent; skipping EFI registration"
  exit 0
fi
PART=$(basename "$USB")
if [[ "$PART" =~ p[0-9]+$ ]]; then DISK="/dev/${PART%p*}"; PNUM="${PART##*p}"
else                                DISK="/dev/${PART%%[0-9]*}"; PNUM="${PART##*[a-z]}"; fi
if [ ! -b "$DISK" ]; then
  logger -t ugreen-panel "assert-boot: resolved partition $USB but parent disk $DISK missing; skipping EFI registration"
  exit 0
fi

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

# --- 2. register a named EFI entry for the boot device and put it first -----
if ! command -v efibootmgr >/dev/null; then
  logger -t ugreen-panel "assert-boot: efibootmgr not found; skipping EFI registration"
  exit 0
fi
find_entry(){ efibootmgr | grep -F "* $LABEL	" | head -1 | sed 's/^Boot//; s/\*.*//'; }
NUM=$(find_entry)
if [ -z "$NUM" ]; then
  efibootmgr -c -d "$DISK" -p "$PNUM" -L "$LABEL" -l '\EFI\BOOT\BOOTX64.EFI' >/dev/null 2>&1
  NUM=$(find_entry)
  logger -t ugreen-panel "registered EFI entry $NUM ($LABEL) on $DISK p$PNUM"
fi
if [ -z "$NUM" ]; then
  logger -t ugreen-panel "assert-boot: failed to register EFI entry ($LABEL) on $DISK p$PNUM"
  exit 0
fi

# put our entry first (firmware may reshuffle; on a UGOS-wiped box we are the
# only bootable entry, so it wins regardless)
CUR=$(efibootmgr | awk -F': ' '/^BootOrder/{print $2}')
if [ "${CUR%%,*}" != "$NUM" ]; then
  REST=$(echo "$CUR" | tr ',' '\n' | grep -v "^$NUM\$" | paste -sd, -)
  efibootmgr -o "$NUM${REST:+,$REST}" >/dev/null 2>&1 \
    && logger -t ugreen-panel "BootOrder: $NUM first"
fi
exit 0
