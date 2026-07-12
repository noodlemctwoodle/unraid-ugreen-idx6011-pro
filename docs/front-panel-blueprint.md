# Front Panel on Unraid — Implementation Runbook

Step-by-step, copy-paste instructions for exactly how the iDX6011 Pro front
touchscreen is made to work on Unraid. **Why** each step exists is in
[`SOLUTION.md`](SOLUTION.md) — this file is the **how**.

Everything here was executed and verified on 2026-07-12 against **Unraid 7.3.2 /
kernel 6.18.38-Unraid**. Version-specific values are marked ⚠️.

---

## What you end up with

- Front 258×960 LCD lit, running `panel_dash`: 7 pages (HOME / OVERVIEW / HARDWARE /
  NETWORK / DISKS / DOCKER / SETTINGS), touch: swipe L/R = page, drag = scroll, tap
  footer = next page, long-press = dim, on-screen Settings. Live stats incl. power
  draw (RAPL). Optional wallpaper. Survives reboots.
- Unraid boots from / licenses against the USB flash. UGOS is not required.

## Prerequisites

- Unraid installed on a USB flash **labelled `UNRAID`**, plugged in permanently.
- `efibootmgr` available (standard on Unraid) to register the boot entry. No UGOS/NVMe needed.
- Array started once so Docker works (needed only for builds).
- Repo files: `plugin/boot/*`, `plugin/src/*`, `plugin/prebuilt/*`.

---

## Step 1 — Register the EFI boot entry that powers the panel

The BIOS powers the panel rail **only** when it boots a *registered* EFI NVRAM entry
(not the auto-generated removable-media fallback). Register a named entry for the USB
flash and keep it first in the boot order — this is exactly what the plugin's
`assert-boot.sh` does at every boot; the manual equivalent:

```sh
blkid -L UNRAID                 # find the USB flash partition, e.g. /dev/sda1
# register a named entry pointing at the flash's EFI bootloader:
efibootmgr -c -d /dev/sda -p 1 -L "Unraid (iDX6011 panel)" -l '\EFI\BOOT\BOOTX64.EFI'
efibootmgr                      # note the new BootNNNN number
efibootmgr -o NNNN,<rest>       # keep our entry first
```

⚠️ If the BIOS has a boot-override "pin", clear it in BIOS setup (F11/Del) so the
BootOrder is honoured. `BootNext` is unreliable on this firmware — set the permanent
order. The firmware may reshuffle BootOrder after updates, so `assert-boot.sh`
re-asserts it every boot.

No UGOS, NVMe, or grub is involved — a box with UGOS wiped boots the panel
identically. The USB's own syslinux is left standard, so a plain USB boot still works
for headless recovery (it goes through the removable-media fallback, which does not
power the panel).

## Step 2 — Build the patched modules + overlay + touch modules

One script does all of it (container build, correct config, correct xz flags):

```sh
# on the box, array started:
bash plugin/boot/build-overlay.sh plugin/boot/i915-edp-wakeprobe-6.18.38.patch
```

What it does (details in the script, every flag mandatory):
1. Ensures the `i915build` Debian container exists (auto-restart,
   `/mnt/cache/build` mounted at `/build`).
2. Downloads vanilla `linux-<ver>.tar.xz` matching `uname -r`, copies Unraid's
   config from `/usr/src/linux-<ver>-Unraid/config` (⚠️ **filename has no dot**).
3. Applies `i915-edp-wakeprobe-<ver>.patch` — the exact deployed source diff:
   probe address `DP_TRAINING_PATTERN_SET`→`DP_DPCD_REV` in the DRM helper;
   `intel_dp_needs_dpcd_probe()` returns `true` for eDP; plus a power-cycle-retry
   fallback in `intel_edp_init_connector` that only runs if the first DPCD read
   fails (it never fires on the working boot path).
4. Builds `drivers/gpu/drm/display`, `drivers/gpu/drm/i915`, `drivers/mfd`,
   `drivers/i2c/busses` with `KBUILD_MODPOST_WARN=1`, and verifies vermagic ==
   `<uname -r> SMP preempt mod_unload`.
5. Packs the two display modules into `/boot/bzroot-wakefix` — plain newc cpio of
   `lib/modules/<kver>/kernel/...` paths; module compression **must** be
   `xz --check=crc32 --lzma2=dict=1MiB` (kernel rejects default CRC64).
6. Copies `intel-lpss.ko intel-lpss-pci.ko i2c-designware-core.ko
   i2c-designware-platform.ko` to `/boot/config/plugins/ugreen-idx6011-pro/modules/`.

⚠️ For a NEW kernel version, first check the patch still applies; the three
changes are small and their contexts rarely move — regenerate against the new
tree if needed (the .patch file is self-describing).

## Step 3 — Stage the plugin files on the flash

```sh
P=/boot/config/plugins/ugreen-idx6011-pro
mkdir -p $P/panel
cp plugin/prebuilt/panel_dash $P/panel/panel_dash
cp plugin/src/start-panel.sh plugin/src/stop-panel.sh plugin/src/assert-boot.sh $P/
# (LED feature too, if used: start.sh stop.sh monitor.sh ugreen_leds_cli i2c-tools-*.txz)
```

`start-panel.sh` (run by the .plg on install and every boot, or by hand) re-asserts
the EFI entry via `assert-boot.sh`, loads `mfd_core` (stock) + the four flash modules
+ `i2c-dev`, copies the daemon to `/usr/local/bin`, and starts it after 5 s with
`--backlight 75 --interval 1` and optional `$P/panel/wallpaper.png`. Installing the
`.plg` does all of this for you.

## Step 4 — (Re)build the daemon from source (optional; prebuilt provided)

```sh
# source of truth: plugin/src/panel/ (modular; panel_dash.c + *.h modules + pages/ + vendored stb)
cp -r plugin/src/panel/* /mnt/cache/build/dash/     # includes pages/ subdir and build.sh
docker exec i915build bash /build/dash/build.sh
cp /mnt/cache/build/dash/panel_dash /boot/config/plugins/ugreen-idx6011-pro/panel_dash
```

CLI: `--bg <png/jpg>` `--backlight <pct>` `--interval <s>` `--once`
`--touch </dev/i2c-N>` `--no-touch` `--cal <s|x|y>` `--rotate <sec>`
(auto-rotate is OFF unless `--rotate` given). Debug: `TOUCH_DEBUG=1` env.

## Step 5 — Reboot and verify

Reboot. Expected sequence: firmware boots the `Unraid (iDX6011 panel)` EFI entry →
Unraid boots → panel lights within ~30 s of boot.

Verification checklist (all over SSH):

```sh
cat /sys/class/drm/card0-eDP-1/status          # -> "connected"
dmesg | grep -c "AUX A.*timeout"               # -> 0
cat /sys/class/backlight/intel_backlight/brightness   # -> 144000 (75%)
pgrep -x panel_dash                           # -> pid
ls /sys/bus/i2c/devices/ | grep CUST           # -> i2c-CUST0000:00  (touch enumerated)
head -2 /var/log/panel_dash.log               # -> "eDP-1: 258x960@60" + "touch: AXS15231B on /dev/i2c-NN"
```

Touch test: swipe the panel left/right — pages change; up/down — page scrolls.

## After an Unraid upgrade

The kernel version changes → the overlay's modules no longer load → panel dark
(everything else fine). Fix = rerun **Step 2** (and only Step 2), then reboot.

## Troubleshooting

Full table in [`SOLUTION.md` §8](SOLUTION.md). The three most likely:

| Symptom | Cause → fix |
|---|---|
| Boots the wrong OS / USB not found | Reinsert the USB flash; ensure the `Unraid (iDX6011 panel)` EFI entry is first (`efibootmgr -o`) → Step 1 |
| Panel dark, `PP_STATUS 0x00000000` in dmesg | Booted via the removable-media fallback, not the registered entry → Step 1 |
| Dashboard frozen but process running | Build lacks per-frame `drmModeDirtyFB` (i915 FBC) → rebuild from current source |

---

## Appendix — hardware reference (verified by RE)

- **Panel**: 258×960@60 eDP (mode from panel EDID, mfr `LEN`/0x30ae) behind an
  eDP→MIPI-DSI bridge on DDI-A/PHY-A/AUX-A. Bridge wakes ONLY on a DPCD_REV
  (0x000) AUX read; link trains at 2.7 Gbps ×1 lane.
- **Touch**: AiXieSheng AXS15231B at I²C addr `0x3b` on the LPSS DesignWare
  adapter of PCI `00:15.1` (ACPI `\_SB_.PC00.I2C1.TPL1`, `CUST0000`).
  Protocol: write 11 B `b5 ab a5 5a 00 00 <len_hi> <len_lo> 00 00 00`, then read
  `len` B in the same I²C transaction. 15 B frame: `[1]&0x0F`=points,
  `[2..3]`=X (12 bit), `[4..5]`=Y, `[6..7]`=frame counter, `[14]`=sum of `[0..13]`.
  All-`0x10` frame = idle/release. Coordinates are panel-space (identity mapping).
  Plain reads (no command) return zeros. Its ACPI `PNP0C50` HID claim is false.
- **Backlight**: `intel_backlight` (i915 PWM, 0..192000). The EC (ITE IT55xx,
  ports 0x62/0x66, EC-RAM offset 0xA5, 1=full…198=off) drives a second
  `mipi_backlight` path under UGOS — not needed on Unraid.
- **LEDs**: 9× RGB via on-board MCU — handled by the existing LED plugin
  (`src/start.sh` stages the daemon `src/monitor.sh`).
- **Do-not**: never downgrade Unraid on this box (burns the trial license);
  never insmod pinctrl/gpio modules built with `GPIOLIB_IRQCHIP` (running kernel
  lacks it — `struct gpio_chip` ABI mismatch).
