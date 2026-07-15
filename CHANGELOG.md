# Changelog

All notable user-facing changes to the **UGREEN iDX6011 Pro** Unraid plugin.
Versions are date-based (`YYYY.MM.DD`); the same notes drive each GitHub release
and the plugin's in-app `<CHANGES>` list.

## 2026.07.15

- **Transfer card.** A new module: add it to any page to watch a Dynamix File
  Manager copy/move in progress — the operation, destination folder, overall
  progress %, live rate and ETA. It reads Unraid's built-in file manager, so no
  setup is needed; when nothing is copying it shows an idle line.
- **Disk bay LEDs** now light on every populated bay, however the SATA
  controllers enumerate. The LED daemon discovers each controller at runtime
  instead of assuming a fixed PCI address, so a drive in any bay shows its health
  even after a BIOS/PCIe re-enumeration moves a controller — previously up to
  four bays could stay dark. A stale bay calibration can no longer override this.
- **Storage card** now reports the whole array (matching the Unraid Main page)
  instead of the cache pool.
- **Plugins page** now shows a description for the plugin in the Unraid web UI.
- **Docs**: clarified where to disable the BIOS Watchdog Timer (the Advanced tab
  in BIOS setup).

## 2026.07.13

- **Fully customisable dashboard.** Build any page from a library of modules and
  pick each one's look — bar, ring, gauge, history graph, area, segmented blocks,
  used/free split, trend, or big number. New modules: **Array status**, **Updates**
  (Unraid OS + plugins), **Power draw**, **NPU**, **Fans**, **Host**, **VMs**, and
  per-item **Disk / Interface / Container / VM** cards. The Docker cards show each
  container's IP and flag when an image update is available.
- **Web Layout editor with a live preview.** Add, rename, reorder, delete and
  show/hide pages; for each module choose its visualisation (and, for per-item
  cards, which disk/interface/container/VM) — with a live render of the real
  258×960 panel beside you.
- **Fan control from the panel.** Drive all four chassis fans on Auto / Silent /
  Quiet / Turbo temperature curves (CPU fans follow the CPU, case fans the hottest
  disk) or Max. A floor keeps every fan spinning and a critical temperature forces
  100%. Live fan RPM on a Fans card, including an animated spinning-fan view.
- **Theme your dashboard.** Choose the font, heading and text sizes, and every
  palette colour with live colour pickers — now including **card-title** and
  **dim-text** colours plus a **card-opacity** slider. Point the panel at any image
  on the server for the **wallpaper** and **header logo** via a file browser; both
  hot-swap on the panel **live, with no restart**. Pick network-rate units (bits or
  bytes) and the primary network interface. Cards grow with the text size instead of
  overlapping.
- **Wallpapers & full-screen pages.** Make cards translucent so a wallpaper shows
  through — globally or per page — add **Spacer** modules to push content down, and
  hide any page's **header bar / title card / page dots** for a clean **full-screen
  image page**. A text drop shadow, a themeable **card-title** colour and a
  **wallpaper-dim** scrim keep everything readable over even busy images.
- **Theme presets & sharing.** Built-in presets (Unraid, Sakura, Ember, Abyss, Aurora,
  Grape, Cyber, Mono) recolour the whole dashboard in a click; **Save current…** writes a
  theme file to `…/panel/themes/`, and a **Load a theme…** dropdown lists your saved
  themes — share one by copying its `.cfg`.
- **Power-save schedule.** Turn the screen and disk/LAN LEDs off between two local times
  (quiet hours). A tap wakes everything and it re-sleeps 30 seconds after the last touch;
  the timezone comes from Unraid automatically, and the power LED stays on.
- **Smarter front LEDs.** Configurable LAN and per-state disk-light colours, power
  light on/off, and an activity mode that blinks the disk/LAN LEDs on I/O and goes
  solid when idle.
- **Redesigned settings.** Screen, Lighting and Display (theme + layout) tabs now
  share one consistent look; everything is also editable live from the panel's own
  touch Settings page.

## 2026.07.12.1

- New plugin icon: original line-art of the UGREEN iDX6011 Pro by Deivizzz.

## 2026.07.12

- Initial release. The full front panel for the UGREEN NASync iDX6011 Pro on
  Unraid, working natively — no UGOS required.
- 258×960 touch LCD dashboard: seven swipeable pages (Home, Overview, Hardware,
  Network, Disks, Docker, and an interactive touch Settings page) with live
  CPU/memory/network/disk/GPU/power stats, array + notification state, and the
  official Unraid header mark.
- Smart front LEDs (power / 2× LAN / 6× disk): drive presence, SMART health
  (green/amber/red) and LAN link, updated live.
- Self-contained install: downloads a SHA256-verified payload and registers its
  own EFI boot entry (no UGOS/NVMe/grub dependency); the panel lights once the box
  boots that entry. If it stays dark, set that entry first in the BIOS under
  Boot → UEFI USB Hard Disk Drive BBS Priorities, then reboot.
- Bundled touch modules + display wake-probe overlay are built for Unraid 7.3.2
  (kernel 6.18.38-Unraid); rebuild with `boot/build-overlay.sh` for other kernels.
- Requires the BIOS Watchdog Timer disabled (Advanced tab in BIOS setup).
