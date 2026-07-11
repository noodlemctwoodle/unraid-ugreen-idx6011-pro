#!/bin/bash
#
# ugreen-idx6011-pro / stop.sh
#
# Runs on plugin remove: stops the daemon and blanks the LEDs.
#
[ -f /run/ugreen-leds.pid ] && kill "$(cat /run/ugreen-leds.pid)" 2>/dev/null
rm -f /run/ugreen-leds.pid

BIN=/usr/local/bin/ugreen_leds_cli
[ -x "$BIN" ] && UGREEN_MODEL=idx6011 "$BIN" all -off 2>/dev/null

rm -f /usr/local/bin/ugreen-idx6011-monitor
logger -t ugreen-leds "monitor stopped, LEDs off"
