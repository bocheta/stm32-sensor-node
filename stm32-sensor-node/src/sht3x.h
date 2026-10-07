/**
 * @file sht3x.h
 * @brief Hardware-independent driver for the Sensirion SHT3x
 *        temperature / humidity sensor.
 *
 * The driver does not touch any hardware directly. All bus access goes
 * through the function pointers in ::sht3x_bus_t, so the same code runs on
 * the STM32 target and in host unit tests with a fake bus.
 */
#ifndef SHT3X_H
#define SHT3X_H

#include <stddef.h>
#include <stdint.h>

/** Default 7-bit I2C address (ADDR pin low). */
#define SHT3X_ADDR_DEFAULT  0x44U
/** Alternative 7-bit I2C address (ADDR pin high). */
#define SHT3X_ADDR_ALT      0x45U

/** Result codes. */
typedef enum {
    SHT3X_OK = 0,
    SHT3X_ERR_PARAM,   /**< Invalid argument. */
    SHT3X_ERR_BUS,     /**< I2C transfer failed (NACK, timeout, ...). */
    SHT3X_ERR_CRC      /**< Data received but checksum mismatch. */
} sht3x_status_t;

/**
 * @brief Bus abstraction. Each function returns 0 on success.
 */
typedef struct {
    int  (*write)(void *ctx, uint8_t addr, const uint8_t *data, size_t len);
    int  (*read)(void *ctx, uint8_t addr, uint8_t *data, size_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;  /**< User context passed to every callback. */
} sht3x_bus_t;

/** Driver instance. */
typedef struct {
    const sht3x_bus_t *bus;
    uint8_t            addr;
} sht3x_t;

/** One measurement in fixed point. */
typedef struct {
    int32_t temperature_cdeg; /**< Temperature in 0.01 degC. */
    int32_t humidity_cpct;    /**< Relative humidity in 0.01 %RH. */
} sht3x_reading_t;

/**
 * @brief Initialise a driver instance.
 */
sht3x_status_t sht3x_init(sht3x_t *dev, const sht3x_bus_t *bus, uint8_t addr);

/**
 * @brief Issue a soft reset and wait until the sensor is ready.
 */
sht3x_status_t sht3x_soft_reset(const sht3x_t *dev);

/**
 * @brief Single-shot measurement, high repeatability, no clock stretching.
 *
 * Blocks for the conversion time (~16 ms) using bus->delay_ms.
 */
sht3x_status_t sht3x_measure(const sht3x_t *dev, sht3x_reading_t *out);

/**
 * @brief Convert a raw 16-bit temperature word to 0.01 degC.
 */
int32_t sht3x_raw_to_cdeg(uint16_t raw);

/**
 * @brief Convert a raw 16-bit humidity word to 0.01 %RH.
 */
int32_t sht3x_raw_to_cpct(uint16_t raw);

#endif /* SHT3X_H */
