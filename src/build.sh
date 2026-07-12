#!/bin/bash
# Build panel_dash2 for Unraid. Run inside the i915build container on the box
# (or any glibc <= Unraid's, x86_64, with libdrm-dev):
#
#   docker exec i915build bash /build/dash/build.sh
#
# stb_easy_font.h and stb_image.h must sit next to panel_dash2.c (vendored in
# this directory; originals: github.com/nothings/stb).
#
# NOTE: gcc treats some -Wall findings in stb_easy_font.h as errors on newer
# toolchains — build with -w (warnings are known-cosmetic).
set -e
cd "$(dirname "$0")"
gcc -O2 -w -o panel_dash2 panel_dash2.c -I/usr/include/libdrm -ldrm -lm
echo "built: $(ls -la panel_dash2)"
# deploy (on the box):
#   cp panel_dash2 /usr/local/bin/panel_dash2
#   cp panel_dash2 /boot/config/plugins/ugreen-idx6011-pro/panel_dash2   # persistence
#   pkill -x panel_dash2; setsid /usr/local/bin/panel_dash2 --backlight 75 --interval 1 &
