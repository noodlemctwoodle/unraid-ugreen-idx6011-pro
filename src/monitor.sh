#!/bin/bash
#
# ugreen-idx6011-pro / monitor.sh
#
# Smart front-panel LED daemon for the UGREEN NASync iDX6011 Pro under Unraid.
# Runs continuously and reacts live:
#   * empty bay        -> off
#   * healthy disk     -> green
#   * pending/realloc  -> amber
#   * SMART fail / not responding -> red
#   * LAN link up      -> blue      (LAN down -> off)
#   * power LED        -> stays white (re-asserted after any CLI write)
#
# diskN LED == physical bay N (top=1 .. bottom=6), confirmed on this unit.
# Disk-activity BLINK is intentionally not here yet — that needs a CLI patch so the
# frequent writes don't zero the power LED (see runbook §5a, phase 2).
#
set -u

BIN=/usr/local/bin/ugreen_leds_cli
BUS=0
A=0x3a
export UGREEN_MODEL=idx6011

# single-instance via pidfile (never stop this daemon with pkill -f — the pattern
# would also match the shell that launched it)
PIDFILE=/run/ugreen-leds.pid
echo $$ > "$PIDFILE" 2>/dev/null
trap 'rm -f "$PIDFILE"' EXIT

REFRESH=2             # main loop interval (seconds) — presence + link react this fast
SMART_INTERVAL=300    # re-check SMART health every N seconds

# ---- bay mapping: HCTL -> physical bay (1=top). diskN LED == bay N ------------
# Get HCTL with:  lsblk -S -o name,hctl,serial,tran
# EDIT these to match the physical slots. Extend when the 22 TB drives go in.
declare -A HCTL_TO_BAY=(
  ["2:0:0:0"]=1     # sda
  ["3:0:0:0"]=2     # sdb
)

# ---- colours (R G B) ----
COL_HEALTHY="0 255 0"
COL_WARN="255 120 0"
COL_ERROR="255 0 0"
COL_LAN="0 100 255"
BR_DISK=110
BR_LAN=150

LED(){ "$BIN" "$@" >/dev/null 2>&1; }

# power LED: raw sequence (CLI can't drive it on this model)
power_white(){
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x04 0x00 0x00 0x00 0x00 0x00 0xa5 s
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x02 0xff 0xff 0xff 0x00 0x03 0xa0 s
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x03 0xff 0x00 0x00 0x00 0x01 0xa3 s
  i2cset -y $BUS $A 0x00 0xa0 0x01 0x00 0x00 0x01 0xff 0x00 0x00 0x00 0x01 0xa1 s
}

# echo "<bay> /dev/<name>" for each present SATA disk that maps to a bay
present_bays(){
  local name hctl
  while read -r name hctl _; do
    [ -n "$hctl" ] || continue
    local bay="${HCTL_TO_BAY[$hctl]:-}"
    [ -n "$bay" ] && echo "$bay /dev/$name"
  done < <(lsblk -S -n -o name,hctl,tran 2>/dev/null | awk '$3=="sata"{print $1,$2}')
}

# disk device -> health colour
disk_colour(){
  local d="$1" out
  out=$(smartctl -H "$d" 2>&1)
  echo "$out" | grep -qiE 'INQUIRY failed|Unable to detect|Smartctl open device.*failed' && { echo "$COL_ERROR"; return; }
  echo "$out" | grep -qiE 'FAILED|failing' && { echo "$COL_ERROR"; return; }
  echo "$out" | grep -qiE 'PASSED' || { echo "$COL_ERROR"; return; }
  local pend
  pend=$(smartctl -A "$d" 2>/dev/null | awk '/Current_Pending_Sector|Reallocated_Sector_Ct|Offline_Uncorrectable/{s+=$10} END{print s+0}')
  [ "${pend:-0}" -gt 0 ] && { echo "$COL_WARN"; return; }
  echo "$COL_HEALTHY"
}

declare -A HEALTH_COL     # cached health colour per bay
declare -A LAST_LED       # last applied state per LED (avoid redundant CLI writes)
last_smart=0

set_changed(){ # $1=led $2=state ; returns 0 (and marks) if state differs
  [ "${LAST_LED[$1]:-}" = "$2" ] && return 1
  LAST_LED[$1]="$2"; return 0
}

lan_led(){ # $1=iface $2=led
  if [ "$(cat /sys/class/net/$1/carrier 2>/dev/null)" = "1" ]; then
    set_changed "$2" "up"   && { LED "$2" -on -color $COL_LAN -brightness $BR_LAN; return 0; }
  else
    set_changed "$2" "down" && { LED "$2" -off; return 0; }
  fi
  return 1
}

power_white   # ensure power on at start

while true; do
  changed=0
  now=$(date +%s 2>/dev/null || echo 0)

  declare -A present=()
  while read -r bay dev; do present[$bay]="$dev"; done < <(present_bays)

  # health: refresh periodically, and immediately for any newly-seen bay
  refresh=0
  [ $((now - last_smart)) -ge $SMART_INTERVAL ] && refresh=1
  for b in "${!present[@]}"; do [ -n "${HEALTH_COL[$b]:-}" ] || refresh=1; done
  if [ "$refresh" = 1 ]; then
    for b in "${!present[@]}"; do HEALTH_COL[$b]="$(disk_colour "${present[$b]}")"; done
    last_smart=$now
  fi

  for b in 1 2 3 4 5 6; do
    if [ -n "${present[$b]:-}" ]; then
      col="${HEALTH_COL[$b]:-$COL_HEALTHY}"
      if set_changed "disk$b" "on:$col"; then LED "disk$b" -on -color $col -brightness $BR_DISK; changed=1; fi
    else
      if set_changed "disk$b" "off"; then LED "disk$b" -off; changed=1; fi
    fi
  done

  lan_led eth0 network_stat  && changed=1
  lan_led eth1 network_stat2 && changed=1

  # any CLI write runs the MCU init probe, which zeros the power LED — re-assert it
  [ "$changed" = 1 ] && power_white

  sleep $REFRESH
done
