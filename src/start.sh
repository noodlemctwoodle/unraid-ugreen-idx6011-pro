#!/bin/bash
#
# ugreen-idx6011-pro / start.sh
#
# Runs on plugin install and at every boot: prepares the hardware and (re)launches
# the LED monitor daemon. Unraid runs from RAM, so this re-establishes everything each boot.
#
PLG=/boot/config/plugins/ugreen-idx6011-pro

# i2c-tools provides i2cset, used for the raw power-LED sequence
command -v i2cset >/dev/null 2>&1 || {
  [ -f "$PLG"/i2c-tools-*.txz ] && installpkg "$PLG"/i2c-tools-*.txz >/dev/null 2>&1
}

modprobe i2c-dev 2>/dev/null
modprobe -r led_ugreen 2>/dev/null    # keep the incompatible stock kernel module off 0x3a

if [ ! -f "$PLG/ugreen_leds_cli" ]; then
  logger -t ugreen-leds "ERROR: ugreen_leds_cli missing under $PLG — build it first (see README)"
  exit 1
fi

# stage onto an exec-capable path (the flash is vfat/noexec)
install -m0755 "$PLG/ugreen_leds_cli" /usr/local/bin/ugreen_leds_cli
install -m0755 "$PLG/monitor.sh"      /usr/local/bin/ugreen-idx6011-monitor

# (re)start the daemon — stop any previous instance via its pidfile (never pkill -f:
# the pattern would also match the shell launching it)
[ -f /run/ugreen-leds.pid ] && kill "$(cat /run/ugreen-leds.pid)" 2>/dev/null
sleep 1
( setsid /usr/local/bin/ugreen-idx6011-monitor >/var/log/ugreen-leds-monitor.log 2>&1 </dev/null & )
logger -t ugreen-leds "monitor started"
