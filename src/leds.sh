#!/bin/bash
#
# ugreen-idx6011-pro / leds.sh
#
# Front-panel LED control for the UGREEN NASync iDX6011 Pro under Unraid.
# Deployed by the plugin to /usr/local/bin/ugreen-idx6011-leds and run at every boot.
#
# Disk + LAN LEDs are driven by klein0r's ugreen_leds_cli (static build, iDX6011 protocol).
# The POWER LED can't be set by the CLI on this model (brightness stays 0), so it is driven
# with the documented raw i2c sequence (register 0x00 on the 0x3a MCU, SMBus I801 / bus 0).
#
set -u

PLGDIR=/boot/config/plugins/ugreen-idx6011-pro
BIN=/usr/local/bin/ugreen_leds_cli
BUS=0
A=0x3a
export UGREEN_MODEL=idx6011

log(){ logger -t ugreen-leds "$*"; echo "ugreen-leds: $*"; }

# --- prerequisites -----------------------------------------------------------
# i2c-tools provides i2cset (used only for the raw power-LED sequence)
command -v i2cset >/dev/null 2>&1 || {
  [ -f "$PLGDIR"/i2c-tools-*.txz ] && installpkg "$PLGDIR"/i2c-tools-*.txz >/dev/null 2>&1
}
modprobe i2c-dev 2>/dev/null
modprobe -r led_ugreen 2>/dev/null      # keep the incompatible kernel module off 0x3a

# stage the static CLI onto an exec-capable path (the flash is vfat/noexec)
[ -f "$PLGDIR/ugreen_leds_cli" ] && install -m0755 "$PLGDIR/ugreen_leds_cli" "$BIN"
[ -x "$BIN" ] || { log "ERROR: CLI binary missing ($BIN)"; exit 1; }

LED(){ "$BIN" "$@" >/dev/null 2>&1; }

# --- power LED (raw i2c: enable->brightness->colour->on->brightness) ----------
power_on_white(){
  command -v i2cset >/dev/null 2>&1 || { log "WARN: i2cset missing, power LED skipped"; return; }
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x04 0x00 0x00 0x00 0x00 0x00 0xa5 s; sleep 0.05
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x01 0xff 0x00 0x00 0x00 0x01 0xa1 s; sleep 0.05
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x02 0xff 0xff 0xff 0x00 0x03 0xa0 s; sleep 0.05
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x03 0xff 0x00 0x00 0x00 0x01 0xa3 s; sleep 0.05
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x01 0xff 0x00 0x00 0x00 0x01 0xa1 s
}

# --- LAN LEDs: green on carrier, off otherwise -------------------------------
lan(){ # $1=iface $2=led
  if [ "$(cat /sys/class/net/$1/carrier 2>/dev/null)" = "1" ]; then
    LED "$2" -on -color 0 255 0 -brightness 128
  else
    LED "$2" -off
  fi
}

# --- apply -------------------------------------------------------------------
# ORDER MATTERS: every ugreen_leds_cli call runs the MCU init handshake, which writes
# brightness 0 to the power LED's register (0x00) as a probe. So drive all CLI LEDs
# FIRST, then set the power LED LAST via the raw sequence — otherwise the CLI blanks it.
lan eth0 network_stat      # LAN1
lan eth1 network_stat2     # LAN2
for d in disk1 disk2 disk3 disk4 disk5 disk6; do
  LED "$d" -on -color 0 255 0 -brightness 96
done
power_on_white             # MUST run last (CLI init probes zero the power LED)

log "front-panel LEDs initialised (power + 2x LAN + 6x disk)"
