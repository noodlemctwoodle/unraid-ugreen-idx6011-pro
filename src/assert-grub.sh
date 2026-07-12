#!/bin/bash
# assert-grub.sh — self-heal the NVMe boot chain that powers the front panel.
# Idempotent; called at every boot by start-panel.sh. Only ever ADDS our menu
# entry / default; never modifies the vendor UGOS entry (its PARTUUID is
# per-device). A UGOS firmware update rewriting grub.cfg is the case this heals.
ESP=/dev/nvme0n1p1
MNT=/tmp/.ugos-esp
CFG=$MNT/EFI/debian/grub.cfg

[ -b $ESP ] || exit 0
mkdir -p $MNT
mountpoint -q $MNT || mount $ESP $MNT 2>/dev/null || exit 0
trap 'umount $MNT 2>/dev/null' EXIT
[ -f "$CFG" ] || exit 0

if ! grep -q 'UNRAID (kernel from USB flash)' "$CFG"; then
    cp "$CFG" "$CFG.pre-unraid-$(date +%Y%m%d)" 2>/dev/null
    cat >> "$CFG" <<'ENTRY'
menuentry "UNRAID (kernel from USB flash)" {
	insmod part_msdos
	insmod fat
	search --no-floppy --file --set=root /bzimage
	linux /bzimage
	initrd /bzroot /bzroot-wakefix
}
ENTRY
    logger -t ugreen-panel "grub.cfg: UNRAID entry re-added"
fi

# default = the UNRAID entry (index = count of menuentries before it)
IDX=$(grep -n '^menuentry' "$CFG" | grep -n 'UNRAID (kernel from USB flash)' | cut -d: -f1)
IDX=$((IDX - 1))
if ! grep -q "^set default=\"$IDX\"" "$CFG"; then
    sed -i "s/^set default=.*/set default=\"$IDX\"/" "$CFG"
fi
grep -q '^set timeout=' "$CFG" || sed -i '1i set timeout="3"' "$CFG"

# BootOrder: the NVMe "debian" entry first (BIOS powers the panel only for it)
if command -v efibootmgr >/dev/null; then
    DEB=$(efibootmgr | awk '/^Boot[0-9A-F]{4}\* debian/{print substr($1,5,4); exit}')
    CUR=$(efibootmgr | awk -F': ' '/^BootOrder/{print $2}')
    if [ -n "$DEB" ] && [ "${CUR%%,*}" != "$DEB" ]; then
        REST=$(echo "$CUR" | tr ',' '\n' | grep -v "^$DEB\$" | paste -sd, -)
        efibootmgr -o "$DEB${REST:+,$REST}" >/dev/null 2>&1 \
            && logger -t ugreen-panel "BootOrder: $DEB moved first"
    fi
fi
exit 0
