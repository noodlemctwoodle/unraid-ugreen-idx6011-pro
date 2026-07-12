# iDX6011 Pro Front Panel on Unraid — THE COMPLETE WORKING SOLUTION

**Status: WORKING as of 2026-07-12.** The 258×960 front touchscreen runs a live,
Unraid-branded, touch-navigable dashboard on stock Unraid 7.3.2 (kernel 6.18.38-Unraid),
with a valid license. No OS downgrade, no custom kernel image.

This document is the single source of truth. Everything needed to understand, rebuild,
or recover the setup is here or linked from here.

---

## 1. Architecture at a glance

```
BIOS (powers panel rail ONLY for a *registered* EFI entry, not the removable fallback)
 └─ EFI entry "Unraid (iDX6011 panel)"  ← plugin self-registers it (efibootmgr); no UGOS
     └─ USB flash → syslinux → /bzimage + /bzroot + /bzroot-wakefix
         ├─ bzroot-wakefix overlay = patched i915.ko + drm_display_helper.ko
         │    (restores the DPCD_REV wake-probe that kernel 6.17+ removed)
         ├─ plugin at boot: assert-boot.sh (re-assert EFI entry) + load touch I2C stack
         └─ panel_dash (userspace, v3):
              ├─ DRM dumb buffer on eDP-1 @258x960 (+ DirtyFB flush per frame — FBC!)
              ├─ 7 pages incl. touch Settings; EC backlight (0xA5); drag-scroll
              ├─ stats from /proc, /sys, emhttp ini files, docker socket
              └─ touch via raw I2C polling of AXS15231B @0x3b on /dev/i2c-1x
```

The USB flash stays plugged in — Unraid finds it by label for the license and /boot.
UGOS is no longer required; if present its grub is used only as an extra fallback.

## 2. Why each piece exists (the discoveries)

| Fact (all empirically proven) | Consequence |
|---|---|
| The BIOS powers the front-panel rail **only when it boots a *registered* EFI NVRAM entry** — NOT the auto-generated removable-media "UEFI OS" fallback. Confirmed 2026-07-12: a named `efibootmgr -c` entry pointing at the USB flash lights the panel on a pure USB boot (`BootCurrent=0000`, eDP-1 connected, 0 AUX timeouts). Every earlier "USB = dark" test had gone through the *fallback* entry — the boot **device** was never the variable. (Original NVMe-grub finding still works; it's just no longer required.) | Plugin self-registers a named EFI entry (`assert-boot.sh`) — **no UGOS/NVMe/grub dependency**. UGOS grub, if present, is an opportunistic fallback. |
| Kernels ≥6.17 removed the DPCD wake-probe eDP relies on here: `d34d6feaf4a7` moved the probe address `DPCD_REV(0x000)` → `TRAINING_PATTERN_SET(0x102)`; `ed3648b9ec4c` disabled the probe for eDP entirely. This panel's eDP→DSI bridge **only wakes on a 0x000 read** (UGOS golden trace: first transaction `0x00000 AUX -> (ret=1) 12`). | Two-line patch, shipped as overlay initrd `bzroot-wakefix` (see §4). Upstream is aware (unmerged fix: "drm/i915/dp: On DPCD init wake the DPRx for eDP", Arun R Murthy, 2026-02). |
| UGOS works because it boots via the NVMe entry **and** its kernel (pinned to 6.12.30, rebuilt as recently as 2026-06) predates the probe change. UGOS's i915/DRM binaries are bit-stock. There is no vendor display patch. | Nothing to port from UGOS for the display. |
| Unraid's kernel omits `GPIOLIB_IRQCHIP`, `PINCTRL_METEORLAKE`, `MFD_INTEL_LPSS*`, `I2C_DESIGNWARE*` — so the vendor touch driver *and* i2c-hid can never bind (the HID probe defers forever on GpioInt). The touch chip's `PNP0C50` HID claim is also false (no HID descriptor at any register). | Touch = LPSS/designware modules built out-of-tree (loadable; ABI-safe) + **userspace AXS15231B polling** in panel_dash2. No kernel touch driver. |
| AXS15231B protocol: write 11-byte command `b5 ab a5 5a 00 00 <len_hi> <len_lo> 00 00 00`, then read `len` bytes in one I2C transaction. Frame: `[0]=gesture [1]=count(low nibble) [2..3]=X(12bit) [4..5]=Y(12bit) [6..7]=frame counter … [14]=checksum(sum of 0..13)`. All-`0x10` frame = idle/release keepalive. Coordinates are already in panel space (0..257 × 0..959), identity orientation. Plain (un-commanded) reads return zeros — that's why naive probing "found nothing". | `touch_init`/`touch_poll` in panel_dash2.c. Chip at address 0x3b on the DesignWare adapter of PCI `00:15.1` (usually `/dev/i2c-17`; auto-discovered by adapter name). |
| i915 FBC (framebuffer compression) is active on the eDP plane. CPU writes to a dumb buffer do **not** invalidate the compressed copy — the panel freezes on the first frame while memory updates happily. | `drmModeDirtyFB()` after every render (panel_dash2 does this). If you ever see a frozen-but-running dashboard, think FBC. |

Ruled out along the way (each empirically, so nobody re-chases them): kernel version
(6.12.30/6.12.87/6.18 all behave identically given the same boot path), i915 load
timing (5.6 s load failed; UGOS at 7.6 s works), PPS/VDD power-cycling (even 30 s
off + probe storm), the EC/SIO vendor driver (its init does no EC writes), external
display present/absent, headless boots, and the bootloader binary itself (standalone
GRUB from USB = still dark). It was never the OS; it was the boot path + the probe
address.

## 3. The boot path (registered EFI entry — UGOS-independent)

**Primary (no UGOS needed):** the plugin's `assert-boot.sh` registers and re-orders a
named EFI entry for the USB flash and points the flash's default syslinux entry at
`initrd=/bzroot,/bzroot-wakefix`:

```sh
efibootmgr -c -d <usb-disk> -p 1 -L "Unraid (iDX6011 panel)" -l '\\EFI\\BOOT\\BOOTX64.EFI'
# then keep it first: efibootmgr -o <num>,<rest>
```

The firmware may reshuffle BootOrder, so the plugin re-asserts every boot; on a
wiped-UGOS box the named entry is the only one, so it always wins. The i915
wake-probe overlay (§4) is still required either way.

### Fallback: UGOS NVMe grub entry (only if UGOS is present)

File: `nvme0n1p1:/EFI/debian/grub.cfg` (UGOS ESP; backup of original at `grub.cfg.pre-unraid`)

```
set default="1"
set timeout="3"

menuentry "UGOSPRO-NAS" {
	linux /boot/vmlinuz2 root=PARTUUID=bf054169-c92b-f24e-9eae-c336b9186abb rootwait console=tty0 overlay=/dev/mmcblk0p7 overlayfs=ext4 net.ifnames=0 quiet splash biosdevname=0
	initrd	/boot/initrd2.img
}
menuentry "UNRAID (kernel from USB flash)" {
	insmod part_msdos
	insmod fat
	search --no-floppy --file --set=root /bzimage
	linux /bzimage drm.debug=0x106 log_buf_len=64M
	initrd /bzroot /bzroot-wakefix
}
```

- BootOrder must have the NVMe entry (`Boot0001* debian`) first: `efibootmgr -o 0001,0002,...`
- UGOS remains selectable (entry 0). If the USB is absent, grub's `search` fails → boot UGOS.
- TODO (cleanup): drop `drm.debug=0x106 log_buf_len=64M` once fully bedded in.
- **Risk**: a UGOS firmware update may regenerate grub.cfg (their template is `grub.am`).
  The future .plg should re-assert this entry on every Unraid boot.
- The USB's own syslinux is intact and standard — a direct USB boot still works fine
  (headless/no-panel, e.g. for recovery).

## 4. The i915 wake-probe overlay (`/boot/bzroot-wakefix`)

Two one-line patches against kernel 6.18.38 sources, shipped as loose modules in a
cpio overlay appended to the initrd chain (no bzroot/bzimage modification):

1. `drivers/gpu/drm/display/drm_dp_helper.c` line ~745:
   `drm_dp_dpcd_probe(aux, DP_TRAINING_PATTERN_SET)` → `drm_dp_dpcd_probe(aux, DP_DPCD_REV)`
2. `drivers/gpu/drm/i915/display/intel_dp.c`, `intel_dp_needs_dpcd_probe()`:
   the `if (intel_dp_is_edp(intel_dp)) return false;` branch → `return true;`
   (Also present in the current build: a power-cycle-and-retry fallback in
   `intel_edp_init_connector` — harmless, only fires when the first DPCD read fails,
   but it adds ~50s to boot in that failure case. Remove on next rebuild.)

Build recipe (container `i915build`, see §7):
```sh
cd /build/linux-6.18.38
cp /usr/src/linux-6.18.38-Unraid/config .config    # NOTE: filename is 'config', no dot!
make olddefconfig && make -j$(nproc) modules_prepare
make -j$(nproc) M=drivers/gpu/drm/display KBUILD_MODPOST_WARN=1
make -j$(nproc) M=drivers/gpu/drm/i915    KBUILD_MODPOST_WARN=1
# compress EXACTLY like this — default xz (CRC64) makes the kernel reject the module:
xz --check=crc32 --lzma2=dict=1MiB -c i915.ko > i915.ko.xz
```
Overlay layout (paths must mirror the stock tree; later initrd members override earlier):
```
lib/modules/6.18.38-Unraid/kernel/drivers/gpu/drm/i915/i915.ko.xz
lib/modules/6.18.38-Unraid/kernel/drivers/gpu/drm/display/drm_display_helper.ko.xz
```
`find . | cpio -o -H newc > /boot/bzroot-wakefix` (plain cpio, uncompressed).
Loaded via grub: `initrd /bzroot /bzroot-wakefix`.

**Kernel-update procedure**: any Unraid upgrade changes the kernel → rebuild both
modules against the new source with the new `config`, regenerate the overlay.
Verify vermagic: `modinfo -F vermagic i915.ko` must equal `uname -r` + ` SMP preempt mod_unload`.

## 5. The dashboard daemon (`panel_dash2`)

Source: `plugin/src/panel_dash2.c` (single-file C99; v1 kept as `panel_dash.c`).
Build: `gcc -O2 -w -o panel_dash2 panel_dash2.c -I/usr/include/libdrm -ldrm -lm`
(with `stb_easy_font.h` + `stb_image.h` alongside — both vendored on the box at
`/mnt/cache/build/dash/`). Links against Unraid's own `/usr/lib64/libdrm.so.2`.
Container glibc (2.41) < Unraid glibc (2.43) so binaries port cleanly.

Runtime essentials:
- Finds the eDP connector on `/dev/dri/card*`, sets 258×960 via dumb buffer + SetCrtc.
- **`drmModeDirtyFB` after every render** (FBC — see §2).
- Re-asserts its framebuffer if fbcon stomps the CRTC.
- Backlight: `/sys/class/backlight/intel_backlight` (0..192000) + `bl_power`.
  (There is also an EC-driven `mipi_backlight` under UGOS; not needed here.)
- **Five pages**: OVERVIEW / NETWORK / DISKS / SYSTEM / ABOUT.
  Gestures: swipe **left/right = change page**, swipe **up/down = scroll within the
  page** (content slides under the opaque header/footer; scroll clamps to content and
  resets on page change), footer-tap = next page, tap = wake backlight,
  long-press = dim toggle. **No auto-rotation by default** (opt-in: `--rotate <sec>`).
- NETWORK gives every interface its own card (status dot, live rates, IP, RX/TX
  totals); DISKS one card per disk — both rely on scrolling, no row caps.
- ABOUT: machine identity (CPU model/threads), memory breakdown (System/Cache/Free
  with bars), usage meters (RAM, boot device, log fs, docker vdisk), host info.
- **Power draw via Intel RAPL** (`/sys/class/powercap/intel-rapl:*/energy_uj` deltas):
  CPU **package** is the primary reading — it matches the webUI. This board's `psys`
  domain is miscalibrated (reads ~0 W) and is only shown if it ever exceeds package.
  Shown on OVERVIEW (CPU card) and SYSTEM (SENSORS row).
- Stats sources: `/proc/stat|meminfo|cpuinfo|net/dev|net/route`, `statvfs`, hwmon,
  thermal zones, powercap/RAPL, `/var/local/emhttp/disks.ini` + `var.ini`
  (array/parity), `/var/run/docker.sock` (container count),
  `/tmp/notifications/unread` (badge + banner), GPU busy = gt0 `rc6_residency_ms`
  delta, NPU = `/sys/class/accel/accel0` busy-time.
- Touch: AXS15231B native polling (§2). `TOUCH_DEBUG=1` env prints frames/gestures.
- Flags: `--bg <png/jpg>` `--backlight <pct>` `--interval <s>` `--once`
  `--touch </dev/i2c-N>` `--no-touch` `--cal <s|x|y>` `--rotate <sec>`.

## 6. Boot persistence (what runs on every boot)

- Binary + modules on the flash: `/boot/config/plugins/ugreen-idx6011-pro/`
  (`panel_dash2`, `modules/intel-lpss.ko`, `modules/intel-lpss-pci.ko`,
  `modules/i2c-designware-core.ko`, `modules/i2c-designware-platform.ko`).
- `/boot/config/go` block: modprobes stock `mfd_core` + `i2c-dev`, insmods the four
  out-of-tree LPSS/designware modules, copies `panel_dash2` to `/usr/local/bin`,
  starts it (5 s delayed) with optional wallpaper at
  `/boot/config/plugins/ugreen-idx6011-pro/wallpaper.png`.
- Touch modules were built with the same harness as §4 (config symbols:
  `MFD_INTEL_LPSS(_PCI)`, `I2C_DESIGNWARE_CORE/PLATFORM` as `m`). `pinctrl-meteorlake`
  and `i2c-hid` are NOT loadable on this kernel (missing `GPIOLIB_IRQCHIP` — ABI
  mismatch danger; do not try to insmod pinctrl modules built with IRQCHIP enabled).

## 7. Build environment

Docker container `i915build` on the box (auto-restarts): `debian:trixie-slim` +
`gcc make flex bison bc libelf-dev libssl-dev kmod xz-utils python3 cpio zstd
initramfs-tools-core acpica-tools grub-efi-amd64-bin binutils curl`, with
`/mnt/cache/build` mounted at `/build`. Contents:
- `linux-6.18.38/` prepared kernel tree (patched as §4), plus pristine
  `linux-6.12.30/` and `linux-6.12.87/` trees and their i915 diffs
- `dash/` — panel_dash sources, stb headers, binaries, probes
  (`axs_probe`, `touch_probe*`, `hid_probe` — protocol archaeology tools)
- `touchmods/` — the built touch-stack modules
- `ugos-forensics/` — UGOS's kernel/initrd/DRM modules (reference)
- `DSDT.dsl` — decompiled ACPI tables (touch device at `\_SB_.PC00.I2C1.TPL1`)

## 8. Recovery procedures

| Scenario | Fix |
|---|---|
| Box boots UGOS instead of Unraid | USB missing/unreadable → grub `search` failed. Reinsert USB, reboot. |
| Grub entry lost (UGOS update) | Re-add the §3 menuentry to `nvme0n1p1:/EFI/debian/grub.cfg` (backup: `grub.cfg.pre-unraid` alongside). |
| Need Unraid without the NVMe path | Plain USB boot still works (BIOS pin/F11) — panel stays dark, everything else normal. |
| Panel dark after Unraid kernel update | Rebuild overlay modules (§4) for the new kernel version. Panel dark ≠ broken box. |
| Dashboard frozen but process alive | FBC staleness — make sure the running build issues `drmModeDirtyFB` per frame. |
| Touch dead | Check the LPSS chain: `ls /sys/bus/i2c/devices/` should list `i2c-CUST0000:00` and DesignWare adapters; re-run the go-script insmod block; `TOUCH_DEBUG=1 panel_dash2` to trace. |
| Wipe/reinstall everything | Flash config backup procedure as documented in the repo `BACKUP-KEEP-2026-07-12/README.txt`. |

## 9. Open items (tracked)

1. Remove `drm.debug=0x106 log_buf_len=64M` from the grub entry (diagnostic leftovers).
2. Test stock (unpatched) modules on the NVMe boot path — if the BIOS pre-trains the
   bridge, the wake-probe overlay may be redundant; would simplify to "grub entry only".
3. Rebuild i915 without the FIX-D retry block (50 s boot penalty in failure cases).
4. Package as a proper `.plg` plugin: assert grub entry + BootOrder, install overlay,
   deploy binary/modules, manage the go-script block, plugin settings page
   (wallpaper, brightness, page config).
5. Optional: gesture support (chip reports a gesture byte), multi-touch (frame len 21+),
   Unraid notification actions on tap, drag-scrolling (currently swipe-step).

## 10. Repo map (everything needed lives in `plugin/`)

- **`docs/SOLUTION.md`** — this file: architecture + facts (the *why*).
- **`docs/front-panel-blueprint.md`** — the implementation runbook: numbered,
  copy-paste steps (the *how*), incl. upgrade + troubleshooting procedures and the
  hardware-reference appendix (touch protocol, EC, panel specs).
- **`boot/i915-edp-wakeprobe-6.18.38.patch`** — the exact deployed kernel-module
  source diff (generated from the build tree, not hand-written).
- **`boot/grub-menuentry.cfg`** — exact NVMe-ESP grub config.
- **`boot/go-block.sh`** — exact `/boot/config/go` autostart block.
- **`boot/build-overlay.sh`** — one-command module/overlay/touch-stack rebuild
  (run after every Unraid upgrade).
- **`src/panel_dash2.c`** + vendored `stb_easy_font.h`/`stb_image.h` + `build.sh`
  — the dashboard daemon, fully self-contained (v1 `panel_dash.c` kept as the
  minimal DRM-core reference).
- **`prebuilt/`** — the verified binaries (`panel_dash2`, `bzroot-wakefix`
  ⚠️ kernel-6.18.38-bound) for disaster recovery; see its README.
- Removed as superseded: `OVERNIGHT-REPORT-2026-07-12.md`, `dashboard-customization.md`,
  `edp-regression-fix.md` (their still-true content is absorbed here).
