/**
 * @file sht3x.c
 * @brief Hardware-independent driver for the Sensirion SHT3x.
 */
#include "sht3x.h"
#include "crc8.h"

#define SHT3X_CMD_MEASURE_HIGH_NO_STRETCH 0x2400U
#define SHT3X_CMD_SOFT_RESET              0x30A2U

#define SHT3X_MEASURE_TIME_MS   16U  /* datasheet max 15.5 ms */
#define SHT3X_RESET_TIME_MS     2U   /* datasheet max 1.5 ms  */
#define SHT3X_FRAME_LEN         6U   /* T_msb T_lsb T_crc RH_msb RH_lsb RH_crc */

static sht3x_status_t send_command(const sht3x_t *dev, uint16_t cmd)
{
    const uint8_t buf[2] = { (uint8_t)(cmd >> 8U), (uint8_t)(cmd & 0xFFU) };

    if (dev->bus->write(dev->bus->ctx, dev->addr, buf, sizeof(buf)) != 0) {
        return SHT3X_ERR_BUS;
    }
    return SHT3X_OK;
}

int32_t sht3x_raw_to_cdeg(uint16_t raw)
{
    /* T = -45 + 175 * raw / 65535   (in 0.01 degC, rounded) */
    return -4500 + (int32_t)((17500U * (uint32_t)raw + 32767U) / 65535U);
}

int32_t sht3x_raw_to_cpct(uint16_t raw)
{
    /* RH = 100 * raw / 65535   (in 0.01 %RH, rounded) */
    return (int32_t)((10000U * (uint32_t)raw + 32767U) / 65535U);
}

sht3x_status_t sht3x_init(sht3x_t *dev, const sht3x_bus_t *bus, uint8_t addr)
{
    if ((dev == NULL) || (bus == NULL) || (bus->write == NULL) ||
        (bus->read == NULL) || (bus->delay_ms == NULL)) {
        return SHT3X_ERR_PARAM;
    }
    if ((addr != SHT3X_ADDR_DEFAULT) && (addr != SHT3X_ADDR_ALT)) {
        return SHT3X_ERR_PARAM;
    }

    dev->bus  = bus;
    dev->addr = addr;
    return SHT3X_OK;
}

sht3x_status_t sht3x_soft_reset(const sht3x_t *dev)
{
    if ((dev == NULL) || (dev->bus == NULL)) {
        return SHT3X_ERR_PARAM;
    }

    sht3x_status_t st = send_command(dev, SHT3X_CMD_SOFT_RESET);
    if (st == SHT3X_OK) {
        dev->bus->delay_ms(dev->bus->ctx, SHT3X_RESET_TIME_MS);
    }
    return st;
}

sht3x_status_t sht3x_measure(const sht3x_t *dev, sht3x_reading_t *out)
{
    uint8_t frame[SHT3X_FRAME_LEN];

    if ((dev == NULL) || (dev->bus == NULL) || (out == NULL)) {
        return SHT3X_ERR_PARAM;
    }

    sht3x_status_t st = send_command(dev, SHT3X_CMD_MEASURE_HIGH_NO_STRETCH);
    if (st != SHT3X_OK) {
        return st;
    }

    dev->bus->delay_ms(dev->bus->ctx, SHT3X_MEASURE_TIME_MS);

    if (dev->bus->read(dev->bus->ctx, dev->addr, frame, sizeof(frame)) != 0) {
        return SHT3X_ERR_BUS;
    }

    if ((crc8_sensirion(&frame[0], 2U) != frame[2]) ||
        (crc8_sensirion(&frame[3], 2U) != frame[5])) {
        return SHT3X_ERR_CRC;
    }

    const uint16_t raw_t  = (uint16_t)(((uint16_t)frame[0] << 8U) | frame[1]);
    const uint16_t raw_rh = (uint16_t)(((uint16_t)frame[3] << 8U) | frame[4]);

    out->temperature_cdeg = sht3x_raw_to_cdeg(raw_t);
    out->humidity_cpct    = sht3x_raw_to_cpct(raw_rh);
    return SHT3X_OK;
}
