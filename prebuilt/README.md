# Prebuilt artifacts — disaster recovery copies

These are the EXACT binaries deployed and verified working on 2026-07-12.

| File | What | Bound to |
|---|---|---|
| `panel_dash2` | Dashboard daemon (x86_64, links `libdrm.so.2`, glibc ≥2.41) | Any Unraid ≥7.3.x |
| `bzroot-wakefix` | Overlay initrd: patched `i915.ko.xz` + `drm_display_helper.ko.xz` | **kernel 6.18.38-Unraid ONLY** (Unraid 7.3.2) |
| `ugreen_leds_cli` | LED MCU CLI — **klein0r fork, iDX6011 protocol** (static build from github.com/klein0r/ugreen_leds_controller; the upstream miskcoo build canNOT drive this model's disk/netdev LEDs) | iDX6011 (Pro) |
| `i2c-tools-4.4-x86_64-1ug.txz` | Static `i2cset/i2cget/i2cdetect/i2ctransfer` built from kernel.org i2c-tools 4.4 (Slackware-layout txz; `installpkg`-able). Needed for the power-LED raw sequence. | Any x86_64 Unraid |

Restore = copy back to the flash:

```sh
cp bzroot-wakefix    /boot/bzroot-wakefix
cp panel_dash2       /boot/config/plugins/ugreen-idx6011-pro/panel_dash2
cp ugreen_leds_cli   /boot/config/plugins/ugreen-idx6011-pro/ugreen_leds_cli
cp i2c-tools-*.txz   /boot/config/plugins/ugreen-idx6011-pro/
```

⚠️ After ANY Unraid upgrade the kernel version changes and `bzroot-wakefix` must be
REBUILT (`../boot/build-overlay.sh`) — a stale overlay's modules simply won't load
(vermagic mismatch; the panel stays dark, nothing else breaks). `panel_dash2` itself
survives upgrades.
