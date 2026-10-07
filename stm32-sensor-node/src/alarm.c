/**
 * @file alarm.c
 * @brief Threshold monitor with hysteresis (NORMAL / LOW / HIGH).
 */
#include "alarm.h"

#include <stddef.h>

bool alarm_configure(alarm_t *a, int32_t low_limit, int32_t high_limit,
                     int32_t hysteresis)
{
    if ((a == NULL) || (low_limit >= high_limit) || (hysteresis < 0)) {
        return false;
    }
    /* Both release points must stay inside the normal band. */
    if ((int64_t)hysteresis * 2 > ((int64_t)high_limit - (int64_t)low_limit)) {
        return false;
    }

    a->low_limit  = low_limit;
    a->high_limit = high_limit;
    a->hysteresis = hysteresis;
    a->state      = ALARM_NORMAL;
    return true;
}

alarm_state_t alarm_update(alarm_t *a, int32_t value)
{
    switch (a->state) {
    case ALARM_NORMAL:
        if (value > a->high_limit) {
            a->state = ALARM_HIGH;
        } else if (value < a->low_limit) {
            a->state = ALARM_LOW;
        } else {
            /* stay NORMAL */
        }
        break;

    case ALARM_HIGH:
        if (value < a->low_limit) {
            a->state = ALARM_LOW;   /* large jump straight across the band */
        } else if (value <= (a->high_limit - a->hysteresis)) {
            a->state = ALARM_NORMAL;
        } else {
            /* stay HIGH */
        }
        break;

    case ALARM_LOW:
        if (value > a->high_limit) {
            a->state = ALARM_HIGH;
        } else if (value >= (a->low_limit + a->hysteresis)) {
            a->state = ALARM_NORMAL;
        } else {
            /* stay LOW */
        }
        break;

    default:
        a->state = ALARM_NORMAL;  /* defensive: recover from corruption */
        break;
    }

    return a->state;
}

const char *alarm_state_name(alarm_state_t state)
{
    switch (state) {
    case ALARM_NORMAL: return "NORMAL";
    case ALARM_LOW:    return "LOW";
    case ALARM_HIGH:   return "HIGH";
    default:           return "INVALID";
    }
}
