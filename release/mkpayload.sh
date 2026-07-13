#!/bin/bash
#
# plugin/release/mkpayload.sh
#
# Created by noodlemctwoodle on 12/07/2026.
#
# Assemble the flash-payload .txz that the .plg downloads, verifies (SHA256) and
# extracts to /boot/config/plugins/ugreen-idx6011-pro/ on install. Produces
# release/<name>-<version>.txz plus a .sha256 sidecar, built entirely from the
# repo (prebuilt/ binaries + kernel-bound modules/overlay, src/ scripts, icon).
#
#   bash release/mkpayload.sh 2026.07.12f            # default kver 6.18.38-Unraid
#   bash release/mkpayload.sh 2026.07.12f 6.18.38-Unraid
#
# The modules + overlay are kernel-specific: prebuilt/modules/<kver>/*.ko and
# prebuilt/bzroot-wakefix must match KVER. For a different Unraid kernel, rebuild
# them with boot/build-overlay.sh, refresh prebuilt/, then re-run this.
set -euo pipefail
cd "$(dirname "$0")/.."                       # repo root (plugin/)
VER=${1:?usage: mkpayload.sh <version> [kver]}
KVER=${2:-6.18.38-Unraid}
NAME=ugreen-idx6011-pro
OUT="release/$NAME-$VER.txz"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

# --- flat scripts (LED + LCD + boot + Apply-restart) -------------------------
install -m0755 src/start.sh src/stop.sh src/monitor.sh src/calibrate.sh \
               src/start-panel.sh src/stop-panel.sh src/assert-boot.sh src/restart.sh "$STAGE/"
# --- shared binaries ---------------------------------------------------------
install -m0755 prebuilt/ugreen_leds_cli "$STAGE/"
install -m0644 prebuilt/i2c-tools-*.txz "$STAGE/"
# --- webGUI icon + settings page + colour-sync script ------------------------
# (the .plg install copies the page/JS/restart.sh into the webGUI plugin dir)
install -d "$STAGE/images"
install -m0644 "images/$NAME.png" "$STAGE/images/"
install -m0644 src/UgreenIDX6011Pro.page "$STAGE/"
install -m0644 src/webgui/idxcp.js "$STAGE/"
# --- dashboard binary + per-kernel touch modules + display overlay -----------
install -d "$STAGE/panel/modules/$KVER" "$STAGE/panel/overlay/$KVER"
# dashboard binary: prefer a freshly-built one (CI / src/panel/build.sh); the
# committed prebuilt/panel_dash is the fallback for hosts without a toolchain.
DASH=prebuilt/panel_dash
[ -x src/panel/panel_dash ] && DASH=src/panel/panel_dash
echo "  panel_dash from: $DASH"
install -m0755 "$DASH" "$STAGE/panel/panel_dash"
install -m0644 "prebuilt/modules/$KVER"/*.ko "$STAGE/panel/modules/$KVER/"
install -m0644 prebuilt/bzroot-wakefix "$STAGE/panel/overlay/$KVER/bzroot-wakefix"

# --- pack (deterministic-ish: sorted, xz) ------------------------------------
install -d release
( cd "$STAGE" && find . -type f | sort | tar cJf - -T - ) > "$OUT"

sha() { if command -v sha256sum >/dev/null; then sha256sum "$1" | awk '{print $1}'
        else shasum -a 256 "$1" | awk '{print $1}'; fi; }
sha "$OUT" > "$OUT.sha256"

echo "built:   $OUT ($(du -h "$OUT" | cut -f1))"
echo "sha256:  $(cat "$OUT.sha256")"
echo "contents:"; tar tJf "$OUT" | sed 's/^/  /'
