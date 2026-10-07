/**
 * @file crc8.h
 * @brief CRC-8 as used by Sensirion sensors (poly 0x31, init 0xFF).
 */
#ifndef CRC8_H
#define CRC8_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Compute the Sensirion CRC-8 over a buffer.
 *
 * Polynomial x^8 + x^5 + x^4 + 1 (0x31), initial value 0xFF,
 * no reflection, no final XOR.
 *
 * @param data Pointer to the data (may be NULL only if len == 0).
 * @param len  Number of bytes.
 * @return CRC-8 value.
 */
uint8_t crc8_sensirion(const uint8_t *data, size_t len);

#endif /* CRC8_H */
