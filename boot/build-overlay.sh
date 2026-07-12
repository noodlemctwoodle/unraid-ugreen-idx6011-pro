#!/bin/bash
# build-overlay.sh — reproducible build of the two patched display modules and the
# bzroot-wakefix overlay initrd, PLUS the four touch-stack modules.
#
# RUN ON: the Unraid box, with the array started (Docker available).
# INPUT:  plugin/boot/i915-edp-wakeprobe-6.18.38.patch (adjust filename per kernel)
# OUTPUT: /boot/bzroot-wakefix
#         /boot/config/plugins/ugreen-idx6011-pro/modules/*.ko
#
# Re-run this after EVERY Unraid upgrade (kernel version changes -> modules must be
# rebuilt; the grub entry and go block are version-independent).
set -e

KV=$(uname -r)                                 # e.g. 6.18.38-Unraid
KVER=${KV%-Unraid}                             # e.g. 6.18.38
B=/mnt/cache/build
PATCH=${1:?usage: build-overlay.sh /path/to/i915-edp-wakeprobe-$KVER.patch}
UGP=/boot/config/plugins/ugreen-idx6011-pro

echo "== kernel $KV (vanilla $KVER) =="

# 1. build container (idempotent)
docker start i915build 2>/dev/null || docker run -d --name i915build \
    --restart unless-stopped -v $B:/build debian:trixie-slim sleep infinity
docker exec i915build bash -c 'which gcc >/dev/null || {
    apt-get update -qq && apt-get install -y -qq gcc make flex bison bc \
        libelf-dev libssl-dev kmod xz-utils cpio curl >/dev/null; }'

# 2. kernel source + Unraid config (NOTE: config file has NO leading dot)
mkdir -p $B && cd $B
[ -f linux-$KVER.tar.xz ] || curl -fsSL -o linux-$KVER.tar.xz \
    https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KVER.tar.xz
[ -d linux-$KVER ] || tar xJf linux-$KVER.tar.xz
cp /usr/src/linux-$KV/config linux-$KVER/.config

# 3. patch + enable touch-stack config symbols + prepare
cp "$PATCH" $B/wakeprobe.patch
docker exec i915build bash -c "
set -e
cd /build/linux-$KVER
patch -p1 -N < /build/wakeprobe.patch || true       # -N: skip if already applied
scripts/config -m MFD_INTEL_LPSS -m MFD_INTEL_LPSS_PCI \
               -m I2C_DESIGNWARE_CORE -m I2C_DESIGNWARE_PLATFORM
make olddefconfig
grep -E '^CONFIG_LOCALVERSION=|^CONFIG_DRM_I915=' .config   # expect \"-Unraid\" and =m
make -j\$(nproc) modules_prepare
make -j\$(nproc) M=drivers/gpu/drm/display KBUILD_MODPOST_WARN=1
make -j\$(nproc) M=drivers/gpu/drm/i915    KBUILD_MODPOST_WARN=1
make -j\$(nproc) M=drivers/mfd             KBUILD_MODPOST_WARN=1
make -j\$(nproc) M=drivers/i2c/busses      KBUILD_MODPOST_WARN=1
modinfo -F vermagic drivers/gpu/drm/i915/i915.ko    # MUST equal '$KV' SMP preempt mod_unload
"

# 4. overlay: xz MUST be --check=crc32 (kernel rejects the default CRC64)
cd $B
rm -rf overlay
mkdir -p overlay/lib/modules/$KV/kernel/drivers/gpu/drm/i915 \
         overlay/lib/modules/$KV/kernel/drivers/gpu/drm/display
xz --check=crc32 --lzma2=dict=1MiB -c linux-$KVER/drivers/gpu/drm/i915/i915.ko \
    > overlay/lib/modules/$KV/kernel/drivers/gpu/drm/i915/i915.ko.xz
xz --check=crc32 --lzma2=dict=1MiB -c linux-$KVER/drivers/gpu/drm/display/drm_display_helper.ko \
    > overlay/lib/modules/$KV/kernel/drivers/gpu/drm/display/drm_display_helper.ko.xz
( cd overlay && find . | cpio -o -H newc ) > /boot/bzroot-wakefix

# 5. touch-stack modules onto the flash (loaded by the go block)
mkdir -p $UGP/modules
cp linux-$KVER/drivers/mfd/intel-lpss.ko            $UGP/modules/
cp linux-$KVER/drivers/mfd/intel-lpss-pci.ko        $UGP/modules/
cp linux-$KVER/drivers/i2c/busses/i2c-designware-core.ko     $UGP/modules/
cp linux-$KVER/drivers/i2c/busses/i2c-designware-platform.ko $UGP/modules/
sync

echo "== DONE =="
echo "overlay:  $(ls -la /boot/bzroot-wakefix)"
echo "touch:    $(ls $UGP/modules/)"
echo "Reboot through the NVMe grub entry to activate."
