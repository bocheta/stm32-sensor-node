/**
 * @file moving_average.h
 * @brief Integer moving-average filter with a fixed maximum window.
 *
 * No dynamic memory. Running sum is kept in 64 bit so it cannot overflow
 * for any int32 input within the window size.
 */
#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <stdbool.h>
#include <stdint.h>

#define MOVING_AVERAGE_MAX_WINDOW 16U

typedef struct {
    int32_t  samples[MOVING_AVERAGE_MAX_WINDOW];
    int64_t  sum;
    uint8_t  window;  /**< Configured window size (1..MAX). */
    uint8_t  count;   /**< Samples currently in the buffer. */
    uint8_t  head;    /**< Next write position. */
} moving_average_t;

/**
 * @brief Initialise the filter.
 * @return false if window is 0 or larger than MOVING_AVERAGE_MAX_WINDOW.
 */
bool moving_average_init(moving_average_t *f, uint8_t window);

/**
 * @brief Add a sample and return the current average (rounded to nearest).
 *
 * Until the window is full, the average is taken over the samples seen so far.
 */
int32_t moving_average_add(moving_average_t *f, int32_t sample);

/** @brief Discard all samples, keep the window size. */
void moving_average_reset(moving_average_t *f);

#endif /* MOVING_AVERAGE_H */
