#!/bin/bash
#
# ugreen-idx6011-pro / monitor.sh
#
# Smart front-panel LED daemon for the UGREEN NASync iDX6011 Pro under Unraid.
# Live: empty bay -> off, healthy -> green, pending/realloc -> amber,
#       SMART fail/not-responding -> red, spun-down -> white, LAN link -> blue,
#       power -> white. Colours + power/activity toggles come from settings.cfg.
#
# diskN LED == physical bay N (top=1 .. bottom=6), confirmed on this unit.
# Activity mode (LED_ACTIVITY=1): a disk/LAN LED hardware-blinks (controller-driven,
# via the CLI -blink mode) while it moved data in the last tick, and shows a solid
# health colour when idle — never dark. Set once per tick, so power_refresh keeps
# the power LED lit through the state changes.
# Spun-down disks are detected with hdparm -C (no wake) and shown in COL_STANDBY.
#
set -u

BIN=/usr/local/bin/ugreen_leds_cli
# SMBus I801 bus number varies with other i2c drivers loaded (i915 DDC, LPSS
# touch stack...) — detect it by adapter name; fall back to 0.
BUS=$(for d in /sys/bus/i2c/devices/i2c-*; do
        grep -q "^SMBus I801" "$d/name" 2>/dev/null && { basename "$d" | cut -d- -f2; break; }
      done)
BUS=${BUS:-0}
A=0x3a
export UGREEN_MODEL=idx6011

PIDFILE=/run/ugreen-leds.pid
echo $$ > "$PIDFILE" 2>/dev/null
trap 'rm -f "$PIDFILE"' EXIT

REFRESH=2
SMART_INTERVAL=300

# ---- bay mapping by PHYSICAL SATA PORT (stable across reboots + disk swaps) --------
# Users never edit this: the map comes from mapping.conf (written by calibrate.sh);
# if that's absent, the built-in iDX6011 Pro default is used (same wiring on every unit).
# /dev/sdX and HCTL both reshuffle across reboots, so we key on the fixed port path.
declare -A PORT_TO_BAY
MAP_CONF=/boot/config/plugins/ugreen-idx6011-pro/mapping.conf
MAP_MTIME=""
load_map(){   # (re)load the bay map from mapping.conf; falls back to the built-in iDX6011 Pro default.
  local m; m=$(stat -c %Y "$MAP_CONF" 2>/dev/null || echo none)
  [ "$m" = "$MAP_MTIME" ] && return 1          # unchanged since last load
  MAP_MTIME="$m"
  PORT_TO_BAY=()
  if [ -f "$MAP_CONF" ]; then
    while read -r port bay _; do
      [ -z "$port" ] && continue; [ "${port:0:1}" = "#" ] && continue
      [ -n "$bay" ] && PORT_TO_BAY["$port"]="$bay"
    done < "$MAP_CONF"
  fi
  if [ ${#PORT_TO_BAY[@]} -eq 0 ]; then
    # Built-in iDX6011 Pro map (identical SATA wiring on every unit; verified by calibration).
    PORT_TO_BAY=(
      ["0000:56:00.0/ata3"]=1 ["0000:56:00.0/ata4"]=2   # bays 1-2  (ASMedia ASM1164)
      ["0000:00:17.0/ata1"]=3 ["0000:00:17.0/ata2"]=4   # bays 3-4  (Intel AHCI)
      ["0000:56:00.0/ata5"]=5 ["0000:56:00.0/ata6"]=6   # bays 5-6  (ASMedia ASM1164)
    )
  fi
  return 0
}
load_map

# LED colours + power toggle, from the dashboard's settings.cfg (empty/invalid ->
# the defaults below). Colours are stored as hex; converted to "R G B" for the CLI.
CFG=/boot/config/plugins/ugreen-idx6011-pro/panel/settings.cfg
cfg_get(){ [ -f "$CFG" ] && grep -E "^$1=" "$CFG" 2>/dev/null | tail -1 | cut -d= -f2- | tr -d '"\r'; }
hex2rgb(){                                   # "rrggbb"/"#rrggbb" -> "R G B"; $2 = default
  local h=${1#\#}
  [[ "$h" =~ ^[0-9a-fA-F]{6}$ ]] || { echo "$2"; return; }
  printf '%d %d %d' "0x${h:0:2}" "0x${h:2:2}" "0x${h:4:2}"
}
COL_HEALTHY=$(hex2rgb "$(cfg_get LED_DISK_OK)"   "0 255 0")
COL_WARN=$(hex2rgb    "$(cfg_get LED_DISK_WARN)" "255 120 0")
COL_ERROR=$(hex2rgb   "$(cfg_get LED_DISK_BAD)"  "255 0 0")
COL_LAN=$(hex2rgb     "$(cfg_get LED_LAN)"       "0 100 255")
POWER_LED=$(cfg_get LED_POWER); POWER_LED=${POWER_LED:-1}   # 1 = white power LED, 0 = off
COL_STANDBY=$(hex2rgb "$(cfg_get LED_DISK_STANDBY)" "255 255 255")  # spun-down/standby disks
ACTIVITY=$(cfg_get LED_ACTIVITY); ACTIVITY=${ACTIVITY:-0}           # 1 = blink disk/LAN on I/O
BLINK_ON=60; BLINK_OFF=60                                           # hardware blink timing (ms)
BR_DISK=110
BR_LAN=150

LED(){ "$BIN" "$@" >/dev/null 2>&1; }

# power LED (raw i2c — the CLI can't drive it on this model).
# Full host-takeover incl. mode-reset (0x04); required to light it from a cold/off state.
power_takeover(){
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x04 0x00 0x00 0x00 0x00 0x00 0xa5 s; sleep 0.03
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x01 0xff 0x00 0x00 0x00 0x01 0xa1 s; sleep 0.03
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x02 0xff 0xff 0xff 0x00 0x03 0xa0 s; sleep 0.03
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x03 0xff 0x00 0x00 0x00 0x01 0xa3 s; sleep 0.03
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x01 0xff 0x00 0x00 0x00 0x01 0xa1 s
}
# re-assert every loop (no mode-reset, so no flicker) so a CLI init-probe can never leave power dark
power_refresh(){
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x02 0xff 0xff 0xff 0x00 0x03 0xa0 s
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x03 0xff 0x00 0x00 0x00 0x01 0xa3 s
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x01 0xff 0x00 0x00 0x00 0x01 0xa1 s
}

present_bays(){  # echo "<bay> /dev/<name>" for each SATA disk mapped to a bay
  local name tran port bay
  while read -r name tran; do
    [ "$tran" = "sata" ] || continue
    port=$(readlink -f /sys/block/"$name"/device 2>/dev/null | grep -oE "0000:[0-9a-f]{2}:[0-9a-f]{2}\.[0-9]/ata[0-9]+")
    bay="${PORT_TO_BAY[$port]:-}"
    [ -n "$bay" ] && echo "$bay /dev/$name"
  done < <(lsblk -Sndo name,tran 2>/dev/null)
}

disk_colour(){
  local d="$1" out
  out=$(timeout 8 smartctl -H "$d" 2>&1)
  echo "$out" | grep -qiE "INQUIRY failed|Unable to detect|open device.*failed" && { echo "$COL_ERROR"; return; }
  echo "$out" | grep -qiE "FAILED|failing" && { echo "$COL_ERROR"; return; }
  echo "$out" | grep -qiE "PASSED" || { echo "$COL_ERROR"; return; }
  local pend
  pend=$(timeout 8 smartctl -A "$d" 2>/dev/null | awk '/Current_Pending_Sector|Reallocated_Sector_Ct|Offline_Uncorrectable/{s+=$10} END{print s+0}')
  [ "${pend:-0}" -gt 0 ] && { echo "$COL_WARN"; return; }
  echo "$COL_HEALTHY"
}

declare -A HEALTH_COL LAST_LED DISK_IO NET_IO
last_smart=0
# spun-down check that does NOT wake the disk (smartctl would) — needs hdparm.
disk_standby(){ hdparm -C "$1" 2>/dev/null | grep -qiE "standby|sleeping"; }
# rc 0 if the disk did I/O since the previous call; reads /sys counters, never wakes it.
disk_active(){
  local n=${1#/dev/} cur
  cur=$(awk '{print $3+$7}' /sys/block/"$n"/stat 2>/dev/null); cur=${cur:-0}
  local prev=${DISK_IO[$n]:-$cur}; DISK_IO[$n]=$cur; [ "$cur" != "$prev" ]
}
# rc 0 if the interface moved bytes since the previous call.
net_active(){
  local rx tx
  rx=$(cat /sys/class/net/"$1"/statistics/rx_bytes 2>/dev/null)
  tx=$(cat /sys/class/net/"$1"/statistics/tx_bytes 2>/dev/null)
  local cur=$(( ${rx:-0} + ${tx:-0} ))
  local prev=${NET_IO[$1]:-$cur}; NET_IO[$1]=$cur; [ "$cur" != "$prev" ]
}
set_changed(){ [ "${LAST_LED[$1]:-}" = "$2" ] && return 1; LAST_LED[$1]="$2"; return 0; }

lan_led(){   # down -> off; up -> solid LAN colour; activity mode -> hardware-blink while traffic flows
  if [ "$(cat /sys/class/net/$1/carrier 2>/dev/null)" = "1" ]; then
    if [ "$ACTIVITY" = 1 ] && net_active "$1"; then
      set_changed "$2" blink && LED "$2" -blink $BLINK_ON $BLINK_OFF -color $COL_LAN -brightness $BR_LAN
    else
      set_changed "$2" up    && LED "$2" -on -color $COL_LAN -brightness $BR_LAN
    fi
  else
    set_changed "$2" down && LED "$2" -off
  fi
}

[ "$POWER_LED" = 0 ] || power_takeover   # once, from cold (skipped when power LED is off)

while true; do
  now=$(date +%s 2>/dev/null || echo 0)
  load_map && LAST_LED=()      # calibration changed mapping.conf -> reload + re-evaluate all LEDs
  declare -A present=()
  while read -r bay dev; do present[$bay]="$dev"; done < <(present_bays)

  # refresh SMART health periodically — but only for AWAKE disks (reading SMART
  # wakes a spun-down drive; hdparm -C does not, so sleeping disks are skipped).
  refresh=0
  [ $((now - last_smart)) -ge $SMART_INTERVAL ] && refresh=1
  for b in "${!present[@]}"; do [ -n "${HEALTH_COL[$b]:-}" ] || refresh=1; done
  if [ "$refresh" = 1 ]; then
    for b in "${!present[@]}"; do
      disk_standby "${present[$b]}" || HEALTH_COL[$b]="$(disk_colour "${present[$b]}")"
    done
    last_smart=$now
  fi

  for b in 1 2 3 4 5 6; do
    d="${present[$b]:-}"
    col="${HEALTH_COL[$b]:-$COL_HEALTHY}"
    if [ -z "$d" ]; then
      set_changed "disk$b" off && LED "disk$b" -off
    elif disk_standby "$d"; then                          # spun down -> solid standby colour
      set_changed "disk$b" "sb:$COL_STANDBY" && LED "disk$b" -on -color $COL_STANDBY -brightness $BR_DISK
    elif [ "$ACTIVITY" = 1 ] && disk_active "$d"; then    # awake + activity + I/O -> blink health colour
      set_changed "disk$b" "bl:$col" && LED "disk$b" -blink $BLINK_ON $BLINK_OFF -color $col -brightness $BR_DISK
    else                                                  # awake, idle (or activity off) -> solid health colour
      set_changed "disk$b" "on:$col" && LED "disk$b" -on -color $col -brightness $BR_DISK
    fi
  done

  lan_led eth0 network_stat
  lan_led eth1 network_stat2

  [ "$POWER_LED" = 0 ] || power_refresh   # off => stop re-asserting; CLI writes leave it dark
  sleep $REFRESH
done
