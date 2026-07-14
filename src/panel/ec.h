/*
 * plugin/src/panel/ec.h
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * ITE IT55xx embedded-controller access (ports 0x62/0x66). Shared low-level
 * primitives used by the backlight (EC-RAM 0xA5) and the fans (tach 0x34,
 * duty 0xB0). Read/write one EC-RAM byte via the 0x80/0x81 commands.
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit.
 */
#ifndef PANEL_EC_H
#define PANEL_EC_H

static int ec_ok = -1;
static void ec_wait_ibf(void){ for (int i = 0; i < 5000 && (inb(0x66) & 2); i++) usleep(100); }
static void ec_wait_obf(void){ for (int i = 0; i < 5000 && !(inb(0x66) & 1); i++) usleep(100); }
static void ec_write(unsigned char addr, unsigned char val){
    if (ec_ok < 0) ec_ok = (ioperm(0x62, 1, 1) == 0 && ioperm(0x66, 1, 1) == 0);
    if (!ec_ok) return;
    ec_wait_ibf(); outb(0x81, 0x66);           /* 0x81 = EC write command */
    ec_wait_ibf(); outb(addr, 0x62);
    ec_wait_ibf(); outb(val, 0x62);
    ec_wait_ibf();
}
static unsigned char ec_read(unsigned char addr){
    if (ec_ok < 0) ec_ok = (ioperm(0x62, 1, 1) == 0 && ioperm(0x66, 1, 1) == 0);
    if (!ec_ok) return 0;
    ec_wait_ibf(); outb(0x80, 0x66);           /* 0x80 = EC read command */
    ec_wait_ibf(); outb(addr, 0x62);
    ec_wait_obf(); return inb(0x62);
}

#endif /* PANEL_EC_H */
