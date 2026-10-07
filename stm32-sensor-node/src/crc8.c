/**
 * @file crc8.c
 * @brief CRC-8 as used by Sensirion sensors (poly 0x31, init 0xFF).
 */
#include "crc8.h"

#define CRC8_POLYNOMIAL ((uint8_t)0x31U)
#define CRC8_INIT       ((uint8_t)0xFFU)

uint8_t crc8_sensirion(const uint8_t *data, size_t len)
{
    uint8_t crc = CRC8_INIT;

    for (size_t i = 0U; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            if ((crc & 0x80U) != 0U) {
                crc = (uint8_t)((uint8_t)(crc << 1U) ^ CRC8_POLYNOMIAL);
            } else {
                crc = (uint8_t)(crc << 1U);
            }
        }
    }

    return crc;
}
