/**
 * @file alarm.h
 * @brief Threshold monitor with hysteresis (NORMAL / LOW / HIGH).
 *
 * A value must cross a limit to enter an alarm state, and must come back
 * past (limit -/+ hysteresis) to leave it. This prevents the alarm from
 * chattering when the value sits right at a limit.
 */
#ifndef ALARM_H
#define ALARM_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ALARM_NORMAL = 0,
    ALARM_LOW,
    ALARM_HIGH
} alarm_state_t;

typedef struct {
    int32_t       low_limit;
    int32_t       high_limit;
    int32_t       hysteresis;
    alarm_state_t state;
} alarm_t;

/**
 * @brief Configure the monitor and reset it to ALARM_NORMAL.
 * @return false if low >= high, hysteresis < 0, or the hysteresis band
 *         is wider than the gap between the limits.
 */
bool alarm_configure(alarm_t *a, int32_t low_limit, int32_t high_limit,
                     int32_t hysteresis);

/**
 * @brief Feed a new value.
 * @return The (possibly new) state.
 */
alarm_state_t alarm_update(alarm_t *a, int32_t value);

/** @brief Human-readable state name. */
const char *alarm_state_name(alarm_state_t state);

#endif /* ALARM_H */
