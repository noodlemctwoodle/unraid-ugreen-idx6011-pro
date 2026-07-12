#!/bin/bash
#
# plugin/src/panel/build.sh
#
# Created by Toby G on 12/07/2026.
#
# Build the panel_dash dashboard for Unraid. Run inside the i915build container
# on the box (or any glibc <= Unraid's, x86_64, with libdrm-dev):
#
#   docker exec i915build bash /build/dash/build.sh
#
# panel_dash.c is a thin aggregator: it #includes the modules in this directory
# (draw.h, stats*.h, ui.h, prefs.h, pageframe.h, pages/*.h, render.h, touch.h)
# plus the vendored stb single-headers, and defines main(). It is one single
# translation unit — the modules are NOT compiled separately.
#
# NOTE: gcc treats some -Wall findings in stb_easy_font.h as errors on newer
# toolchains — build with -w (warnings are known-cosmetic).
set -e
cd "$(dirname "$0")"
gcc -O2 -w -o panel_dash panel_dash.c -I/usr/include/libdrm -ldrm -lm
echo "built: $(ls -la panel_dash)"
# deploy (on the box):
#   cp panel_dash /usr/local/bin/panel_dash
#   cp panel_dash /boot/config/plugins/ugreen-idx6011-pro/panel/panel_dash   # persistence
#   pkill -x panel_dash; setsid /usr/local/bin/panel_dash --backlight 75 --interval 1 &
