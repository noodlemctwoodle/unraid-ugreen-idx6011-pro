# unraid-ugreen-idx6011-pro

Touch **LCD dashboard** + smart **front-panel LEDs** for the **UGREEN NASync iDX6011 Pro**
running **Unraid** — the full front panel, working natively, no UGOS required at runtime.

[![Latest release](https://img.shields.io/github/v/release/noodlemctwoodle/unraid-ugreen-idx6011-pro?display_name=tag&sort=date&label=release&color=f15a2c)](https://github.com/noodlemctwoodle/unraid-ugreen-idx6011-pro/releases/latest)
[![Release build](https://img.shields.io/github/actions/workflow/status/noodlemctwoodle/unraid-ugreen-idx6011-pro/release.yml?label=release%20build)](https://github.com/noodlemctwoodle/unraid-ugreen-idx6011-pro/actions/workflows/release.yml)
[![Downloads](https://img.shields.io/github/downloads/noodlemctwoodle/unraid-ugreen-idx6011-pro/total?label=downloads&color=44cc11)](https://github.com/noodlemctwoodle/unraid-ugreen-idx6011-pro/releases)
[![Unraid 7.3.x](https://img.shields.io/badge/Unraid-7.3.x-e22828)](https://unraid.net)
[![UGREEN iDX6011 Pro](https://img.shields.io/badge/UGREEN-iDX6011%20Pro-1793d1)](https://nas.ugreen.com)
[![License](https://img.shields.io/badge/license-MIT%20%2F%20GPL--2.0-blue)](#license)

> Community project. Not affiliated with or endorsed by UGREEN.

**Jump to:** [Features](#features) · [Requirements](#requirements) · [How it works](#how-it-works) ·
[Install](#installation) · [Dashboard](#the-dashboard) · [LEDs](#front-panel-leds) ·
[Configuration](#configuration) · [Upgrades](#after-an-unraid-os-upgrade) ·
[Troubleshooting](#troubleshooting) · [Building](#building-from-source) · [Credits](#credits)

## Features

- **Swipeable touch dashboard** on the 258×960 front LCD — seven pages: Home
  (CPU ring gauge), Overview, Hardware (sparklines), Network, Disks, Docker, and an
  interactive Settings page
- **Touch navigation**: swipe left/right for pages, up/down to scroll, tap the
  footer for next page, long-press to dim, tap to wake
- **Live stats**: CPU %/temp, memory, per-interface network rates + totals,
  per-disk temps/usage/health, GPU & NPU utilisation, **power draw** (Intel RAPL),
  array/parity state, docker count, Unraid **notifications** badge + banner
- **Smart front LEDs** (9× RGB): power white; LAN1/2 blue on link; each disk bay
  green/amber/red from SMART health, off when empty — updates live (~2 s)
- **Wallpaper support** — drop `wallpaper.png` in the plugin's `panel/` dir, auto-scaled to the panel
- Backlight control, optional auto page rotation
- **Survives reboots and self-heals**: proper Unraid plugin (verified against the
  plugin-manager schema), re-asserts its boot chain at every start
- Everything needed is in this repo — sources, the kernel wake-probe patch + overlay
  builder, and prebuilt binaries for disaster recovery

## Requirements

- UGREEN NASync **iDX6011 Pro** (model-gated via DMI — the plugin no-ops elsewhere)
- **Unraid 7.3.x** on a USB flash labelled `UNRAID` (developed on 7.3.2 / kernel 6.18.38)
- **UGOS not required** — the plugin self-registers its own EFI boot entry, so a box
  with UGOS wiped from the NVMe works identically (see below)
- **BIOS Watchdog Timer disabled** — **mandatory** for any non-UGOS OS, or the box
  hard-resets every ~2 minutes. The setting is hidden by default: on the BIOS setup
  screen press **Ctrl+F1** to reveal the advanced menus, then find and disable the
  **Watchdog Timer**.

## How it works

Three discoveries make this possible (full story: [docs/SOLUTION.md](docs/SOLUTION.md)):

1. **The BIOS powers the front-panel rail only when it boots a *registered* EFI
   entry** — not the auto-generated removable-media fallback. So the plugin
   registers a named EFI entry (`Unraid (iDX6011 panel)`) for the USB flash with
   `efibootmgr` and keeps it first in the boot order. **No UGOS or NVMe required.**
   A wiped-UGOS box works identically — our registered entry is then the only one.
   This registered-vs-fallback insight builds directly on
   [Reevoy24/ugreen-idx6011-panel](https://github.com/Reevoy24/ugreen-idx6011-panel).
2. **Kernels ≥ 6.17 dropped the DPCD wake-probe** this panel's eDP bridge needs.
   Two one-line module patches (shipped as a small overlay initrd, `bzroot-wakefix` —
   the kernel image itself is untouched) restore it.
3. **The touchscreen (AXS15231B) needs no kernel driver**: the dashboard polls it
   directly over I²C with the chip's native command protocol.

```
BIOS (panel rail ON for a *registered* EFI entry)
 └─ "Unraid (iDX6011 panel)" EFI entry → USB flash → syslinux
     └─ /bzimage + /bzroot + /bzroot-wakefix
         └─ plugin at boot: re-asserts its EFI entry (assert-boot.sh)
                            → loads touch I²C stack
                            → starts panel_dash (dashboard) + LED monitor
```

## Installation

> [!NOTE]
> **Self-contained**: the plugin downloads a SHA256-verified payload from the GitHub
> release and stages everything itself (scripts, binaries, touch modules, display
> overlay, icon). The from-source / manual runbook — and how to rebuild for a
> different kernel — is **[docs/front-panel-blueprint.md](docs/front-panel-blueprint.md)**.

1. **Plugins → Install Plugin** in the Unraid UI, paste this URL, install:
   ```
   https://raw.githubusercontent.com/noodlemctwoodle/unraid-ugreen-idx6011-pro/main/ugreen-idx6011-pro.plg
   ```
   It downloads its payload, stages everything, and registers its EFI boot entry
   (`assert-boot.sh`).
2. **Reboot.** The firmware boots the registered `Unraid (iDX6011 panel)` entry (the
   BIOS powers the panel rail only for a registered entry) — the panel lights, the
   dashboard starts, and the LEDs go live.

⚠️ **Disable the BIOS Watchdog Timer first** (see [Requirements](#requirements)) —
otherwise the box hard-resets every ~2 minutes on any non-UGOS OS.

## The Dashboard

<table align="center">
  <tr>
    <td align="center" valign="top"><img src="images/screenshots/home.png"     width="130" alt="Home page"><br><sub><b>Home</b></sub></td>
    <td align="center" valign="top"><img src="images/screenshots/overview.png" width="130" alt="Overview page"><br><sub><b>Overview</b></sub></td>
    <td align="center" valign="top"><img src="images/screenshots/hardware.png" width="130" alt="Hardware page"><br><sub><b>Hardware</b></sub></td>
    <td align="center" valign="top"><img src="images/screenshots/network.png"  width="130" alt="Network page"><br><sub><b>Network</b></sub></td>
  </tr>
  <tr>
    <td align="center" valign="top"><img src="images/screenshots/disks.png"    width="130" alt="Disks page"><br><sub><b>Disks</b></sub></td>
    <td align="center" valign="top"><img src="images/screenshots/docker.png"   width="130" alt="Docker page"><br><sub><b>Docker</b></sub></td>
    <td align="center" valign="top"><img src="images/screenshots/settings.png" width="130" alt="Settings page"><br><sub><b>Settings</b></sub></td>
    <td align="center" valign="top"></td>
  </tr>
</table>

<p align="center"><sub>Live 258×960 front panel, rendered on-device.</sub></p>

| Page | Contents |
|------|----------|
| **HOME** | CPU ring gauge + stacked RAM / CPU-temp / network / storage tiles + uptime |
| **OVERVIEW** | hostname/IP, array + parity state, CPU %+temp+watts, RAM, net rates, storage |
| **HARDWARE** | CPU %+temp with 60-sample sparkline, memory, GPU %+MHz sparkline, NPU, power |
| **NETWORK** | one card per interface: rates, IPv4/IPv6, RX/TX totals since boot |
| **DISKS** | one card per disk: health dot, temp / SLEEP, errors, usage bar, capacity |
| **DOCKER** | running/total count + one row per container (status dot, name, status) |
| **SETTINGS** | **touch controls**: brightness slider, screen-off timer, auto-rotate, LED toggle, night mode, restart, reboot/shutdown (hold-to-confirm) |

| Gesture | Action |
|---------|--------|
| Swipe left / right | next / previous page |
| Drag up / down | scroll within the page (1:1) |
| Tap footer dots | next page |
| Tap | wake backlight |
| Long-press | dim toggle |

## Front-panel LEDs

| LED | Behaviour |
|-----|-----------|
| Power | solid **white** |
| LAN1 / LAN2 | **blue** when link up, off when down |
| Disk bay (empty) | off |
| Disk bay (healthy) | **green** |
| Disk bay (pending/reallocated sectors) | **amber** |
| Disk bay (SMART fail / not responding) | **red** |

<details>
<summary>Why a special CLI build is required on this model</summary>

The iDX6011 Pro uses a **different LED protocol** (SMBus block-write) than older
DX/DXP models — the stock `ugreenleds-driver` kernel module and the upstream
`ugreen_leds_cli` predate this box and cannot drive its disk/netdev LEDs.
This plugin uses **[klein0r's fork](https://github.com/klein0r/ugreen_leds_controller)**
(iDX6011 protocol; LED names `power / network_stat / network_stat2 / disk1-6`).
The **power LED** can't be set by the CLI at all on this model (its init probe zeroes
the power register), so it's driven by a raw i2c sequence, re-asserted after every
CLI write. The SMBus bus number is auto-detected by adapter name (`SMBus I801`) —
it shifts when other I²C drivers load.
</details>

**Bay mapping**: the correct port→bay map ships built in. To verify or fix it, run the
one-time wizard — it lights each bay and asks which disk is in it, then writes
`mapping.conf` (picked up live, no restart):

```bash
bash /boot/config/plugins/ugreen-idx6011-pro/calibrate.sh
```

## Configuration

`/boot/config/plugins/ugreen-idx6011-pro/panel/settings.cfg` (user edits survive updates):

| Key | Default | Meaning |
|-----|---------|---------|
| `BRIGHTNESS` | `75` | backlight percent (5–100) — driven via the EC |
| `INTERVAL` | `1` | stats refresh seconds |
| `ROTATE` | `0` | auto-rotate pages every N seconds; `0` = off |
| `SCREEN_OFF_MIN` | `0` | blank the panel after N idle minutes; `0` = never |
| `NIGHT` | `0` | night mode: clamp brightness to 15% |
| `LEDS` | `1` | front LEDs on/off |

All of these are editable live from the **SETTINGS** page on the panel itself.

- **Wallpaper**: place a file named `wallpaper.png` in `…/ugreen-idx6011-pro/panel/`
  (any size — auto-scaled to 258×960). The decoder reads PNG or JPEG data, but the
  file must carry the `wallpaper.png` name to be picked up; restart the daemon (or
  reboot) after adding it.
  into `/boot/config/plugins/ugreen-idx6011-pro/panel/`.
- **LED colours/brightness/timing**: variables at the top of `src/monitor.sh`.
- Dashboard CLI flags (for manual runs): `--bg <img>` `--backlight <pct>`
  `--interval <s>` `--rotate <s>` `--touch </dev/i2c-N>` `--no-touch`
  `--cal <s|x|y>` `--once`; `TOUCH_DEBUG=1` traces touch frames.

## After an Unraid OS upgrade

> [!NOTE]
> A new Unraid release means a new kernel, and the display-module overlay is
> kernel-bound. The panel stays dark (with an Unraid notification; nothing breaks)
> until you rebuild — one command, then reboot:
>
> ```bash
> bash plugin/boot/build-overlay.sh plugin/boot/i915-edp-wakeprobe-<kver>.patch
> ```

## Troubleshooting

| Symptom | Cause → fix |
|---------|-------------|
| Boots the wrong OS / not Unraid | Reinsert the USB flash, then in the BIOS put the panel entry first: **Boot → UEFI USB Hard Disk Drive BBS Priorities → `Unraid (iDX6011 panel)`** |
| Panel dark, dmesg shows `PP_STATUS: 0x00000000` | Booted via the removable-media fallback, not the registered entry. The OS-set BootOrder (`efibootmgr -o`) may not override the per-class USB priority on this firmware — set it in the BIOS: **Boot → UEFI USB Hard Disk Drive BBS Priorities → `Unraid (iDX6011 panel)`** |
| Panel dark after an Unraid upgrade | Kernel changed → rebuild the overlay (above) |
| Dashboard frozen but process running | i915 framebuffer compression serving a stale frame → update to a build with per-frame `DirtyFB` (any current one) |
| Touch dead | `ls /sys/bus/i2c/devices/` should list `i2c-CUST0000:00` — if not, the LPSS module chain didn't load; re-run the plugin install |
| LEDs dark or partially working | Wrong CLI variant — must be the klein0r iDX6011 build ([prebuilt/](prebuilt/)) |
| Box hard-resets every ~2 min | BIOS watchdog enabled → disable it |

More: [docs/SOLUTION.md section 8](docs/SOLUTION.md) (recovery procedures table).

## Building from source

Everything builds in a Debian container on the box (the plugin's own docs assume
`i915build` with `/mnt/cache/build` mounted at `/build`):

- **Dashboard**: `src/panel/` (modular C99; `panel_dash.c` aggregates the modules + vendored stb headers) → [src/panel/build.sh](src/panel/build.sh)
- **Display-module overlay + touch-stack modules**: [boot/build-overlay.sh](boot/build-overlay.sh)
  (applies [boot/i915-edp-wakeprobe-6.18.38.patch](boot/i915-edp-wakeprobe-6.18.38.patch),
  builds against Unraid's own kernel config, packs the overlay with the required
  `xz --check=crc32` compression)
- **LED CLI**: `make` in klein0r's `cli/` dir (needs `g++`, `libi2c-dev`)
- **i2c-tools**: static build from kernel.org source, packed as an
  `installpkg`-able txz (not available in Slackware repos)

## Repository layout

```
ugreen-idx6011-pro.plg   Unraid plugin manifest (schema-verified, lifecycle-tested)
src/                     shell scripts (LED + LCD + boot heal); src/panel/ = modular dashboard C
boot/                    kernel wake-probe patch + overlay build script
prebuilt/                verified binaries: panel_dash, bzroot-wakefix (kernel-bound),
                         ugreen_leds_cli (klein0r/iDX6011), i2c-tools txz
images/                  plugin icon (transparent PNG)
docs/SOLUTION.md         architecture + every hardware discovery, with evidence
docs/front-panel-blueprint.md   numbered install runbook + hardware reference appendix
docs/PLUGIN.md           Unraid plugin-schema conformance + release process
```

## Known limitations / roadmap

- **Fan monitoring/control** — the fan curves live in the EC; register mapping on
  this model is not done yet (the kindred
  [Reevoy24/ugreen-idx6011-panel](https://github.com/Reevoy24/ugreen-idx6011-panel)
  project documents EC fan work for Proxmox/TrueNAS — a good starting point)
- No disk-activity blink yet (needs a small CLI patch to stop the init probe
  zeroing the power LED)
- Unraid webGUI settings page + packaged one-URL release (plg + txz) — planned;
  the conformance groundwork is done ([docs/PLUGIN.md](docs/PLUGIN.md))

## Credits

Built on the work of others — this would not exist without:

- [Reevoy24/ugreen-idx6011-panel](https://github.com/Reevoy24/ugreen-idx6011-panel) —
  the **registered-EFI-entry boot method** this whole plugin rests on (it's why the panel
  lights with no UGOS/NVMe), plus EC fan/backlight register documentation. A kindred
  multi-platform (Proxmox/TrueNAS) panel project.
- [klein0r/ugreen_leds_controller](https://github.com/klein0r/ugreen_leds_controller) and
  [miskcoo/ugreen_leds_controller](https://github.com/miskcoo/ugreen_leds_controller) —
  LED protocol + CLI; the bundled `ugreen_leds_cli` is klein0r's iDX6011 fork (GPL-2.0)
- [ich777/unraid-ugreenleds-driver](https://github.com/ich777/unraid-ugreenleds-driver) —
  original Unraid LED packaging approach
- [Incipiens/ugreen-idx6011-pro-nas-display](https://github.com/Incipiens/ugreen-idx6011-pro-nas-display) —
  early panel reverse-engineering groundwork
- The [Linux kernel](https://www.kernel.org) (Intel i915 / DRM DisplayPort + Intel LPSS /
  DesignWare I²C drivers) — the shipped `bzroot-wakefix` display overlay and the touch-stack
  modules are compiled, patched builds of these **GPL-2.0** sources; the exact source diff
  is in [boot/i915-edp-wakeprobe-6.18.38.patch](boot/i915-edp-wakeprobe-6.18.38.patch)
- [i2c-tools](https://git.kernel.org/pub/scm/utils/i2c-tools/i2c-tools.git) (kernel.org /
  lm-sensors) — bundled static `i2cset/i2cget/i2ctransfer` for the raw power-LED I²C
  sequence the CLI can't drive (GPL-2.0)
- [nothings/stb](https://github.com/nothings/stb) — `stb_easy_font`, `stb_image` and
  `stb_image_write` (public domain / MIT)
- [homarr-labs/dashboard-icons](https://github.com/homarr-labs/dashboard-icons) — the
  official Unraid header mark embedded in the dashboard (Apache-2.0 icon set)
- [libdrm](https://gitlab.freedesktop.org/mesa/drm) — DRM dumb-buffer rendering the
  dashboard links against at runtime (MIT)

Related upstream work: the eDP DPCD wake fix *"drm/i915/dp: On DPCD init wake the DPRx for
eDP"* (Arun R Murthy) addresses the same kernel regression — see
[docs/SOLUTION.md](docs/SOLUTION.md).

## License

Plugin scripts and dashboard source: **MIT**.

Bundled and derived components keep their upstream licenses:

- `ugreen_leds_cli` (klein0r / miskcoo): **GPL-2.0**
- the `bzroot-wakefix` display overlay and the touch-stack `.ko` modules are **GPL-2.0**
  derivatives of the Linux kernel (Intel i915 / DRM DP, Intel LPSS / DesignWare I²C), built
  from the patch in [`boot/`](boot/)
- the static `i2c-tools` build: **GPL-2.0**
- stb headers (`stb_easy_font`, `stb_image`, `stb_image_write`): public domain / MIT
- the embedded Unraid header icon ([homarr-labs/dashboard-icons](https://github.com/homarr-labs/dashboard-icons)): Apache-2.0

**Trademarks:** *"Unraid"* and the Unraid mark are trademarks of Lime Technology, Inc.;
*"UGREEN"* / *"NASync"* and the product-photo plugin icon (`images/ugreen-idx6011-pro.png`)
are trademarks/property of UGREEN. This is an unaffiliated community project; marks are
used for identification only.
