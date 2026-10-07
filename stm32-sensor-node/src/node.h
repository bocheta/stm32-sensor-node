/**
 * @file node.h
 * @brief Application logic of the sensor node, independent of the RTOS
 *        and the hardware.
 *
 * Holds the configuration and the latest filtered reading, processes new
 * measurements (filtering + alarm monitoring) and executes text commands
 * received over the serial console.
 *
 * Not thread-safe by itself: the firmware protects a node_t with a mutex.
 */
#ifndef NODE_H
#define NODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "alarm.h"
#include "moving_average.h"
#include "sht3x.h"

#define NODE_PERIOD_MIN_MS      100U
#define NODE_PERIOD_MAX_MS      60000U
#define NODE_PERIOD_DEFAULT_MS  1000U
#define NODE_FILTER_WINDOW      4U
#define NODE_TEMP_LOW_DEFAULT   1000   /* 10.00 degC */
#define NODE_TEMP_HIGH_DEFAULT  3000   /* 30.00 degC */
#define NODE_TEMP_HYSTERESIS    50     /*  0.50 degC */
#define NODE_LIMIT_MIN_DEG      (-40)  /* sensor operating range */
#define NODE_LIMIT_MAX_DEG      125

typedef struct {
    uint32_t         period_ms;
    bool             streaming;
    bool             has_reading;
    sht3x_reading_t  filtered;
    uint32_t         sample_count;
    uint32_t         error_count;
    sht3x_status_t   last_status;
    moving_average_t temp_filter;
    moving_average_t hum_filter;
    alarm_t          temp_alarm;
} node_t;

/** @brief Reset the node to its default configuration. */
void node_init(node_t *node);

/**
 * @brief Process the result of one measurement.
 * @param status Result of sht3x_measure().
 * @param raw    Unfiltered reading (ignored if status != SHT3X_OK).
 * @return true if the alarm state changed.
 */
bool node_on_measurement(node_t *node, sht3x_status_t status,
                         const sht3x_reading_t *raw);

/**
 * @brief Format the latest filtered reading, e.g.
 *        "T=23.45C RH=41.20% ALARM=NORMAL".
 */
void node_format_reading(const node_t *node, char *out, size_t out_len);

/**
 * @brief Execute one command line and write the response into @p out.
 *
 * Supported commands: help, status, read, rate <ms>, stream on|off,
 * limits <low_degC> <high_degC>.
 *
 * @return true if the command was recognised and executed successfully.
 */
bool node_handle_command(node_t *node, const char *line,
                         char *out, size_t out_len);

/**
 * @brief Format a value with two implied decimals, e.g. -50 -> "-0.50".
 */
void node_format_fixed2(int32_t value, char *out, size_t out_len);

/**
 * @brief Strictly parse a decimal integer (optional leading '-').
 * @return false on empty input, trailing garbage or overflow.
 */
bool node_parse_int32(const char *str, size_t len, int32_t *out);

#endif /* NODE_H */
