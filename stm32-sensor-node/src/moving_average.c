/**
 * @file moving_average.c
 * @brief Integer moving-average filter with a fixed maximum window.
 */
#include "moving_average.h"

#include <stddef.h>

bool moving_average_init(moving_average_t *f, uint8_t window)
{
    if ((f == NULL) || (window == 0U) || (window > MOVING_AVERAGE_MAX_WINDOW)) {
        return false;
    }
    f->window = window;
    moving_average_reset(f);
    return true;
}

void moving_average_reset(moving_average_t *f)
{
    if (f == NULL) {
        return;
    }
    for (size_t i = 0U; i < MOVING_AVERAGE_MAX_WINDOW; i++) {
        f->samples[i] = 0;
    }
    f->sum   = 0;
    f->count = 0U;
    f->head  = 0U;
}

int32_t moving_average_add(moving_average_t *f, int32_t sample)
{
    if (f->count == f->window) {
        f->sum -= f->samples[f->head];   /* drop the oldest sample */
    } else {
        f->count++;
    }

    f->samples[f->head] = sample;
    f->sum += sample;
    f->head = (uint8_t)((f->head + 1U) % f->window);

    /* Round half away from zero. */
    const int64_t n = (int64_t)f->count;
    const int64_t half = n / 2;
    const int64_t avg = (f->sum >= 0) ? ((f->sum + half) / n)
                                      : ((f->sum - half) / n);
    return (int32_t)avg;
}
