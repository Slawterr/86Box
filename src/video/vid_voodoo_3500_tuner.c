/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Voodoo3 3500 TV RF tuner emulation.
 *
 * The production NTSC board used a Philips-compatible four-byte PLL tuner.
 * Guest software writes the divider followed by control and band bytes, and
 * reads a status byte containing PLL-lock and AFC information.  Network and
 * media access deliberately live outside this synchronous I2C model; the
 * configured RF frequency cache is the boundary used by a host tuner backend.
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <86box/i2c.h>
#include <86box/vid_voodoo_3500_tuner.h>

#define V3TV_TUNER_I2C_ADDR 0x60
#define V3TV_TUNER_I2C_SIZE 4
#define V3TV_TUNER_MAX_CHANNELS 256
#define V3TV_TUNER_LOCK          0x40
#define V3TV_TUNER_AFC_CENTER    0x02
#define V3TV_TUNER_MATCH_HZ      1000000U

typedef struct voodoo_3500_tuner_t {
    void    *i2c;
    uint8_t  bytes[4];
    uint8_t  byte_count;
    uint8_t  standard;
    uint32_t frequency_hz;
    uint32_t channels[V3TV_TUNER_MAX_CHANNELS];
    unsigned channel_count;
    uint8_t  locked;
} voodoo_3500_tuner_t;

static void
voodoo_3500_tuner_update_lock(voodoo_3500_tuner_t *tuner)
{
    tuner->locked = 0;

    for (unsigned i = 0; i < tuner->channel_count; i++) {
        const uint32_t frequency = tuner->channels[i];
        const uint32_t delta = (frequency > tuner->frequency_hz) ?
                               (frequency - tuner->frequency_hz) :
                               (tuner->frequency_hz - frequency);

        if (delta <= V3TV_TUNER_MATCH_HZ) {
            tuner->locked = 1;
            break;
        }
    }
}

static void
voodoo_3500_tuner_decode(voodoo_3500_tuner_t *tuner)
{
    uint16_t divider;

    /* Philips tuners accept control-first writes while tuning downward. */
    if ((tuner->bytes[0] & 0x80) && !(tuner->bytes[2] & 0x80))
        divider = ((uint16_t) (tuner->bytes[2] & 0x7f) << 8) | tuner->bytes[3];
    else
        divider = ((uint16_t) (tuner->bytes[0] & 0x7f) << 8) | tuner->bytes[1];

    const uint16_t if_offset = (tuner->standard == VOODOO_3500_TUNER_PAL) ? 623 : 732;
    tuner->frequency_hz = (divider > if_offset) ?
                          ((uint32_t) (divider - if_offset) * 62500U) : 0;
    voodoo_3500_tuner_update_lock(tuner);
}

static uint8_t
voodoo_3500_tuner_start(void *bus, uint8_t addr, uint8_t read, void *priv)
{
    voodoo_3500_tuner_t *tuner = (voodoo_3500_tuner_t *) priv;
    (void) bus;
    (void) addr;

    if (!read)
        tuner->byte_count = 0;

    return 1;
}

static uint8_t
voodoo_3500_tuner_read(void *bus, uint8_t addr, void *priv)
{
    const voodoo_3500_tuner_t *tuner = (const voodoo_3500_tuner_t *) priv;
    (void) bus;
    (void) addr;

    return V3TV_TUNER_AFC_CENTER | (tuner->locked ? V3TV_TUNER_LOCK : 0);
}

static uint8_t
voodoo_3500_tuner_write(void *bus, uint8_t addr, uint8_t data, void *priv)
{
    voodoo_3500_tuner_t *tuner = (voodoo_3500_tuner_t *) priv;
    (void) bus;
    (void) addr;

    if (tuner->byte_count < sizeof(tuner->bytes))
        tuner->bytes[tuner->byte_count++] = data;

    if (tuner->byte_count == sizeof(tuner->bytes))
        voodoo_3500_tuner_decode(tuner);

    return 1;
}

static void
voodoo_3500_tuner_parse_channels(voodoo_3500_tuner_t *tuner, const char *rf_channels)
{
    if (!rf_channels || !rf_channels[0])
        return;

    const char *next = rf_channels;
    while (*next && (tuner->channel_count < V3TV_TUNER_MAX_CHANNELS)) {
        char  *end = NULL;
        double mhz = strtod(next, &end);

        if (end == next)
            break;
        if ((mhz >= 40.0) && (mhz <= 1000.0))
            tuner->channels[tuner->channel_count++] = (uint32_t) llround(mhz * 1000000.0);

        next = end;
        while ((*next == ',') || (*next == ';') || (*next == ' ') || (*next == '\t'))
            next++;
    }
}

void *
voodoo_3500_tuner_init(void *i2c, int standard, const char *rf_channels)
{
    voodoo_3500_tuner_t *tuner = (voodoo_3500_tuner_t *) calloc(1, sizeof(voodoo_3500_tuner_t));

    if (!tuner)
        return NULL;

    tuner->i2c      = i2c;
    tuner->standard = standard;
    voodoo_3500_tuner_parse_channels(tuner, rf_channels);
    /* The module address is selected by two address pins (0x60-0x63). */
    i2c_sethandler(i2c, V3TV_TUNER_I2C_ADDR, V3TV_TUNER_I2C_SIZE, voodoo_3500_tuner_start,
                   voodoo_3500_tuner_read, voodoo_3500_tuner_write, NULL, tuner);

    return tuner;
}

void
voodoo_3500_tuner_close(void *priv)
{
    voodoo_3500_tuner_t *tuner = (voodoo_3500_tuner_t *) priv;

    if (!tuner)
        return;

    i2c_removehandler(tuner->i2c, V3TV_TUNER_I2C_ADDR, V3TV_TUNER_I2C_SIZE, voodoo_3500_tuner_start,
                      voodoo_3500_tuner_read, voodoo_3500_tuner_write, NULL, tuner);
    free(tuner);
}
