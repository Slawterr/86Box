/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          Voodoo3 3500 TV multimedia I2C devices.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <86box/i2c.h>
#include <86box/vid_voodoo_3500_multimedia.h>
#include <86box/vid_voodoo_3500_tuner.h>

#define V3TV_MSP_ADDR       0x40
#define V3TV_VPX_ADDR       0x43
#define V3TV_BT869_ADDR     0x44

#define VPX_R_JEDEC         0x00
#define VPX_R_PARTLOW       0x01
#define VPX_R_PARTHIGH      0x02
#define VPX_R_JEDEC2        0x03
#define VPX_R_FPSTA         0x35
#define VPX_R_FPRD          0x36
#define VPX_R_FPWR          0x37
#define VPX_R_FPDAT         0x38

#define VPX_FP_ASR          0x013
#define VPX_FP_SDT          0x020
#define VPX_FP_NLPF         0x0cb
#define VPX_FP_VERSION      0x0f0
#define VPX_FP_INFOWORD     0x141

#define MSP_DEM_WRITE       0x10
#define MSP_DEM_READ        0x11
#define MSP_DFP_WRITE       0x12
#define MSP_DFP_READ        0x13

typedef struct v3tv_bt869_t {
    uint8_t regs[256];
    uint8_t reg;
    uint8_t count;
} v3tv_bt869_t;

typedef struct v3tv_vpx_t {
    uint8_t  regs[256];
    uint16_t fp[0x200];
    uint16_t fp_read_addr;
    uint16_t fp_write_addr;
    uint8_t  reg;
    uint8_t  count;
    uint8_t  first_data;
    uint8_t  read_count;
} v3tv_vpx_t;

typedef struct v3tv_msp_t {
    uint16_t dem[0x400];
    uint16_t dfp[0x100];
    uint16_t reg;
    uint8_t  command;
    uint8_t  count;
    uint8_t  value_hi;
    uint8_t  read_count;
} v3tv_msp_t;

typedef struct voodoo_3500_multimedia_t {
    void          *i2c;
    void          *tuner;
    uint8_t        standard;
    v3tv_bt869_t   bt869;
    v3tv_vpx_t     vpx;
    v3tv_msp_t     msp;
} voodoo_3500_multimedia_t;

static uint8_t
v3tv_bt869_start(void *bus, uint8_t addr, uint8_t read, void *priv)
{
    voodoo_3500_multimedia_t *dev = (voodoo_3500_multimedia_t *) priv;
    (void) bus;
    (void) addr;

    if (!read)
        dev->bt869.count = 0;
    return 1;
}

static uint8_t
v3tv_bt869_read(void *bus, uint8_t addr, void *priv)
{
    voodoo_3500_multimedia_t *dev = (voodoo_3500_multimedia_t *) priv;
    const uint8_t bank = (dev->bt869.regs[0xc4] >> 6) & 3;
    (void) bus;
    (void) addr;

    /*
     * Status bank zero identifies a Bt869 in bits 7:5.  The remaining
     * banks report no PLL, timing, or DAC faults until signal generation
     * is connected to the TV-output backend.
     */
    return bank ? 0x00 : 0x20;
}

static uint8_t
v3tv_bt869_write(void *bus, uint8_t addr, uint8_t data, void *priv)
{
    voodoo_3500_multimedia_t *dev = (voodoo_3500_multimedia_t *) priv;
    v3tv_bt869_t *bt = &dev->bt869;
    (void) bus;
    (void) addr;

    if (bt->count++ == 0)
        bt->reg = data;
    else
        bt->regs[bt->reg] = data;
    return 1;
}

static uint8_t
v3tv_vpx_start(void *bus, uint8_t addr, uint8_t read, void *priv)
{
    voodoo_3500_multimedia_t *dev = (voodoo_3500_multimedia_t *) priv;
    (void) bus;
    (void) addr;

    if (!read)
        dev->vpx.count = 0;
    dev->vpx.read_count = 0;
    return 1;
}

static uint8_t
v3tv_vpx_read(void *bus, uint8_t addr, void *priv)
{
    voodoo_3500_multimedia_t *dev = (voodoo_3500_multimedia_t *) priv;
    v3tv_vpx_t *vpx = &dev->vpx;
    uint8_t ret;
    (void) bus;
    (void) addr;

    if (vpx->reg == VPX_R_FPDAT) {
        const uint16_t value = vpx->fp[vpx->fp_read_addr & 0x1ff];
        ret = (vpx->read_count++ & 1) ? (uint8_t) value : (uint8_t) (value >> 8);
    } else {
        ret = vpx->regs[vpx->reg];
    }
    return ret;
}

static uint8_t
v3tv_vpx_write(void *bus, uint8_t addr, uint8_t data, void *priv)
{
    voodoo_3500_multimedia_t *dev = (voodoo_3500_multimedia_t *) priv;
    v3tv_vpx_t *vpx = &dev->vpx;
    (void) bus;
    (void) addr;

    if (vpx->count == 0) {
        vpx->reg = data;
    } else if (vpx->count == 1) {
        vpx->first_data = data;
        if ((vpx->reg != VPX_R_FPRD) && (vpx->reg != VPX_R_FPWR) &&
            (vpx->reg != VPX_R_FPDAT))
            vpx->regs[vpx->reg] = data;
    } else if (vpx->count == 2) {
        const uint16_t value = ((uint16_t) vpx->first_data << 8) | data;

        if (vpx->reg == VPX_R_FPRD)
            vpx->fp_read_addr = value;
        else if (vpx->reg == VPX_R_FPWR)
            vpx->fp_write_addr = value;
        else if (vpx->reg == VPX_R_FPDAT)
            vpx->fp[vpx->fp_write_addr & 0x1ff] = value;
    }
    vpx->count++;
    return 1;
}

static uint16_t *
v3tv_msp_register(v3tv_msp_t *msp)
{
    if ((msp->command == MSP_DEM_WRITE) || (msp->command == MSP_DEM_READ))
        return &msp->dem[msp->reg & 0x3ff];
    return &msp->dfp[msp->reg & 0xff];
}

static uint8_t
v3tv_msp_start(void *bus, uint8_t addr, uint8_t read, void *priv)
{
    voodoo_3500_multimedia_t *dev = (voodoo_3500_multimedia_t *) priv;
    (void) bus;
    (void) addr;

    if (!read)
        dev->msp.count = 0;
    dev->msp.read_count = 0;
    return 1;
}

static uint8_t
v3tv_msp_read(void *bus, uint8_t addr, void *priv)
{
    voodoo_3500_multimedia_t *dev = (voodoo_3500_multimedia_t *) priv;
    v3tv_msp_t *msp = &dev->msp;
    const uint16_t value = *v3tv_msp_register(msp);
    (void) bus;
    (void) addr;

    return (msp->read_count++ & 1) ? (uint8_t) value : (uint8_t) (value >> 8);
}

static void
v3tv_msp_reset(v3tv_msp_t *msp)
{
    memset(msp->dem, 0, sizeof(msp->dem));
    memset(msp->dfp, 0, sizeof(msp->dfp));

    /* MSP3430G-B5 identification returned through DFP 0x001e/0x001f. */
    msp->dfp[0x1e] = 0x0207;
    msp->dfp[0x1f] = 0x1e05;
}

static uint8_t
v3tv_msp_write(void *bus, uint8_t addr, uint8_t data, void *priv)
{
    voodoo_3500_multimedia_t *dev = (voodoo_3500_multimedia_t *) priv;
    v3tv_msp_t *msp = &dev->msp;
    (void) bus;
    (void) addr;

    switch (msp->count) {
        case 0:
            msp->command = data;
            break;
        case 1:
            msp->reg = (uint16_t) data << 8;
            break;
        case 2:
            msp->reg |= data;
            if ((msp->command == 0x00) && (msp->reg == 0x0000))
                v3tv_msp_reset(msp);
            break;
        case 3:
            msp->value_hi = data;
            break;
        case 4:
            if ((msp->command == MSP_DEM_WRITE) || (msp->command == MSP_DFP_WRITE))
                *v3tv_msp_register(msp) = ((uint16_t) msp->value_hi << 8) | data;
            break;
        default:
            break;
    }
    msp->count++;
    return 1;
}

static void
v3tv_bt869_reset(v3tv_bt869_t *bt)
{
    memset(bt, 0, sizeof(*bt));
    bt->regs[0xa0] = 0x80;
    bt->regs[0xba] = 0x20;
    bt->regs[0xc4] = 0x00;
}

static void
v3tv_vpx_reset(v3tv_vpx_t *vpx, uint8_t standard)
{
    memset(vpx, 0, sizeof(*vpx));
    vpx->regs[VPX_R_JEDEC]    = 0xec;
    vpx->regs[VPX_R_PARTLOW]  = 0x31;
    vpx->regs[VPX_R_PARTHIGH] = 0x72;
    vpx->regs[VPX_R_JEDEC2]   = 0x00;
    vpx->regs[VPX_R_FPSTA]    = 0x00;

    vpx->fp[VPX_FP_ASR]      = 0x0000;
    vpx->fp[VPX_FP_SDT]      = standard ? 0x0000 : 0x0001;
    vpx->fp[VPX_FP_NLPF]     = standard ? 0x0138 : 0x0106;
    vpx->fp[VPX_FP_VERSION]  = 0x0100;
    vpx->fp[VPX_FP_INFOWORD] = 0x0000;
}

void *
voodoo_3500_multimedia_init(void *i2c, int standard, const char *rf_channels)
{
    voodoo_3500_multimedia_t *dev =
        (voodoo_3500_multimedia_t *) calloc(1, sizeof(voodoo_3500_multimedia_t));

    if (!dev)
        return NULL;

    dev->i2c      = i2c;
    dev->standard = standard;
    v3tv_bt869_reset(&dev->bt869);
    v3tv_vpx_reset(&dev->vpx, standard);
    v3tv_msp_reset(&dev->msp);

    i2c_sethandler(i2c, V3TV_MSP_ADDR, 1, v3tv_msp_start,
                   v3tv_msp_read, v3tv_msp_write, NULL, dev);
    i2c_sethandler(i2c, V3TV_VPX_ADDR, 1, v3tv_vpx_start,
                   v3tv_vpx_read, v3tv_vpx_write, NULL, dev);
    i2c_sethandler(i2c, V3TV_BT869_ADDR, 1, v3tv_bt869_start,
                   v3tv_bt869_read, v3tv_bt869_write, NULL, dev);

    dev->tuner = voodoo_3500_tuner_init(i2c, standard, rf_channels);
    return dev;
}

void
voodoo_3500_multimedia_close(void *priv)
{
    voodoo_3500_multimedia_t *dev = (voodoo_3500_multimedia_t *) priv;

    if (!dev)
        return;

    voodoo_3500_tuner_close(dev->tuner);
    i2c_removehandler(dev->i2c, V3TV_BT869_ADDR, 1, v3tv_bt869_start,
                      v3tv_bt869_read, v3tv_bt869_write, NULL, dev);
    i2c_removehandler(dev->i2c, V3TV_VPX_ADDR, 1, v3tv_vpx_start,
                      v3tv_vpx_read, v3tv_vpx_write, NULL, dev);
    i2c_removehandler(dev->i2c, V3TV_MSP_ADDR, 1, v3tv_msp_start,
                      v3tv_msp_read, v3tv_msp_write, NULL, dev);
    free(dev);
}
