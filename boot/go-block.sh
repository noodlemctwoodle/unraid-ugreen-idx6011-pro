# EXACT block appended to /boot/config/go on the Unraid flash.
# Loads the touch I2C stack (modules shipped on the flash) and starts the dashboard.
# Requires: /boot/config/plugins/ugreen-idx6011-pro/{panel_dash2,modules/*.ko}

# UGREEN iDX6011 Pro front-panel dashboard v2 (requires NVMe-grub boot path)
UGP=/boot/config/plugins/ugreen-idx6011-pro
if [ -f $UGP/panel_dash2 ]; then
  # touch stack: LPSS I2C controllers + designware + i2c-dev (userspace AXS polling)
  modprobe mfd_core 2>/dev/null
  for m in intel-lpss intel-lpss-pci i2c-designware-core i2c-designware-platform; do
    insmod $UGP/modules/$m.ko 2>/dev/null
  done
  modprobe i2c-dev 2>/dev/null
  cp $UGP/panel_dash2 /usr/local/bin/panel_dash2
  chmod +x /usr/local/bin/panel_dash2
  BG=$UGP/wallpaper.png
  [ -f "$BG" ] && BGA="--bg $BG" || BGA=""
  ( sleep 5; setsid /usr/local/bin/panel_dash2 --backlight 75 --interval 1 $BGA </dev/null >>/var/log/panel_dash2.log 2>&1 ) &
fi
