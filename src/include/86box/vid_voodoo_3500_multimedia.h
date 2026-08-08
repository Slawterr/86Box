/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          Voodoo3 3500 TV multimedia I2C devices.
 */

#ifndef EMU_VID_VOODOO_3500_MULTIMEDIA_H
#define EMU_VID_VOODOO_3500_MULTIMEDIA_H

extern void *voodoo_3500_multimedia_init(void *i2c, int standard, const char *rf_channels);
extern void  voodoo_3500_multimedia_close(void *priv);

#endif /* EMU_VID_VOODOO_3500_MULTIMEDIA_H */
