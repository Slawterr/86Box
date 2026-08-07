/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Voodoo3 3500 TV RF tuner emulation.
 */

#ifndef EMU_VID_VOODOO_3500_TUNER_H
#define EMU_VID_VOODOO_3500_TUNER_H

enum {
    VOODOO_3500_TUNER_NTSC = 0,
    VOODOO_3500_TUNER_PAL
};

extern void *voodoo_3500_tuner_init(void *i2c, int standard, const char *rf_channels);
extern void  voodoo_3500_tuner_close(void *priv);

#endif /* EMU_VID_VOODOO_3500_TUNER_H */
