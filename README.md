# unraid-ugreen-idx6011-pro

Smart front-panel **LED control** for the **UGREEN NASync iDX6011 Pro** running **Unraid**.

The iDX6011 Pro's front panel has 9 addressable RGB LEDs — **Power**, **LAN1**, **LAN2**, and
**6 disk bays**. Under UGOS these are driven by the on-board MCU; on Unraid they just run the
MCU's boot animation. This plugin takes control of them and makes them mean something.

> ⚠️ Lights only. Front **LCD touchscreen** support is planned separately (see [Roadmap](#roadmap)).

## What it does

A small monitor daemon updates the panel live (~2 s reaction):

| LED | Behaviour |
|-----|-----------|
| Power | solid **white** |
| LAN1 / LAN2 | **blue** when the link is up, **off** when down |
| Disk bay (empty) | **off** |
| Disk bay (healthy) | **green** |
| Disk bay (pending/reallocated sectors) | **amber** |
| Disk bay (SMART fail / not responding) | **red** |

Plug a cable or reseat a drive and the LED follows on its own.

## Why it's built this way

The iDX6011 Pro is new and uses a **different LED protocol** (SMBus block-write) than the older
DX/DXP models. The stock `ugreenleds-driver` kernel module (miskcoo/ich777, 2024) predates this
box and drives it incorrectly.

- **Disk + LAN LEDs** are driven by **[klein0r's `ugreen_leds_cli`](https://github.com/klein0r/ugreen_leds_controller)**,
  which implements the iDX6011 protocol and auto-detects the model (`UGREEN_MODEL=idx6011`).
- **The power LED** cannot be set by the CLI on this model (its init probe zeroes the power
  register), so it is driven with a **raw i2c sequence** documented in the fork
  (`docs/iDX6011Pro-LED-Protocol.md`). Because every CLI call re-zeroes the power LED, the daemon
  always re-asserts power **after** any CLI write.

`diskN` LED == physical bay N (top = 1 … bottom = 6) on this unit.

## Requirements

- UGREEN NASync **iDX6011 Pro**, Unraid **6.12+** (developed on 7.3.2, kernel 6.18)
- **BIOS Watchdog Timer disabled** (mandatory for any non-UGOS OS, or the box hard-resets
  every ~2 min)
- Docker available (used once, to compile the CLI)

## Install

There is no packaged release yet, so the CLI binary is built locally and staged on the flash.

```bash
PLG=/boot/config/plugins/ugreen-idx6011-pro
mkdir -p "$PLG"

# 1. build the static ugreen_leds_cli in a throwaway gcc container
cd /tmp
wget -O src.tgz https://github.com/klein0r/ugreen_leds_controller/archive/refs/heads/master.tar.gz
tar xzf src.tgz && cd ugreen_leds_controller-master/cli
docker run --rm -v "$PWD":/src -w /src gcc:13 \
  bash -c "apt-get update -qq && apt-get install -y -qq libi2c-dev >/dev/null && make"
cp ugreen_leds_cli "$PLG/"

# 2. stage i2c-tools (any Slackware i2c-tools .txz works) into "$PLG"
#    e.g. from the old ugreenleds-driver plugin, or slackpkg

# 3. copy this repo's plugin files into "$PLG" (monitor.sh, start.sh, stop.sh)
#    and the manifest to /boot/config/plugins/ugreen-idx6011-pro.plg

# 4. install
plugin install /boot/config/plugins/ugreen-idx6011-pro.plg
```

The plugin runs `start.sh` on install and on every boot, which loads `i2c-dev`, unloads the
incompatible `led_ugreen` module, and launches the monitor.

## Configuration

Edit `monitor.sh` (top of file):

- **Bay mapping** — `HCTL_TO_BAY` maps each disk's HCTL (from `lsblk -S -o name,hctl,serial`)
  to a physical bay. Extend it when you populate more bays.
- **Colours** — `COL_HEALTHY`, `COL_WARN`, `COL_ERROR`, `COL_LAN` (R G B).
- **Timing** — `REFRESH` (loop interval), `SMART_INTERVAL` (health re-check).

## Files

```
ugreen-idx6011-pro.plg   Unraid plugin manifest
src/monitor.sh           the live LED daemon (presence + health + LAN)
src/start.sh             boot/install: prep i2c + launch the daemon
src/stop.sh              remove: stop the daemon, blank the LEDs
src/leds.sh              optional one-shot "set static states" helper
```

## Known limitations

- **No disk-activity blink yet.** Blinking needs frequent CLI writes, which fight the power-LED
  workaround; the clean fix is a small CLI patch (skip the init probe) — planned.
- **Bay mapping is manual** (`HCTL_TO_BAY`), verified by eye.
- **Not yet a downloadable release** — the CLI is built locally (see Install).

## Roadmap

- Disk-activity blink (patch `ugreen_leds_cli` to stop it zeroing the power LED).
- Front **LCD** support via [Incipiens/ugreen-idx6011-pro-nas-display](https://github.com/Incipiens/ugreen-idx6011-pro-nas-display) (`ug-paneld`).
- Package a proper release so the `.plg` installs without a local build.
- Settings page in the Unraid UI.

## Credits

- [klein0r/ugreen_leds_controller](https://github.com/klein0r/ugreen_leds_controller) and
  [miskcoo/ugreen_leds_controller](https://github.com/miskcoo/ugreen_leds_controller) — the LED
  protocol and CLI (GPL-2.0).
- [ich777/unraid-ugreenleds-driver](https://github.com/ich777/unraid-ugreenleds-driver) — the
  original Unraid packaging approach.

## License

Plugin scripts: MIT. The bundled `ugreen_leds_cli` is GPL-2.0 (see its upstream repo).
