/**
 * @file line_buffer.c
 * @brief Assembles a stream of characters into lines.
 */
#include "line_buffer.h"

#define ASCII_BACKSPACE '\b'
#define ASCII_DEL       ((char)0x7F)

void line_buffer_init(line_buffer_t *lb)
{
    lb->len        = 0U;
    lb->buf[0]     = '\0';
    lb->overflowed = false;
    lb->ready      = false;
}

line_result_t line_buffer_push(line_buffer_t *lb, char c)
{
    /* A previously returned line is consumed now. */
    if (lb->ready) {
        lb->ready  = false;
        lb->len    = 0U;
        lb->buf[0] = '\0';
    }

    if ((c == '\r') || (c == '\n')) {
        if (lb->overflowed) {
            lb->overflowed = false;
            lb->len = 0U;
            lb->buf[0] = '\0';
            return LINE_OVERFLOW;
        }
        if (lb->len == 0U) {
            return LINE_PENDING;  /* empty line, or LF of a CRLF pair */
        }
        lb->buf[lb->len] = '\0';
        lb->ready = true;
        return LINE_READY;
    }

    if (lb->overflowed) {
        return LINE_PENDING;  /* drop the rest of the overlong line */
    }

    if ((c == ASCII_BACKSPACE) || (c == ASCII_DEL)) {
        if (lb->len > 0U) {
            lb->len--;
        }
        return LINE_PENDING;
    }

    /* Ignore other control characters. */
    if ((unsigned char)c < 0x20U) {
        return LINE_PENDING;
    }

    if (lb->len >= (LINE_BUFFER_SIZE - 1U)) {
        lb->overflowed = true;
        return LINE_PENDING;
    }

    lb->buf[lb->len] = c;
    lb->len++;
    return LINE_PENDING;
}

const char *line_buffer_get(const line_buffer_t *lb)
{
    return lb->buf;
}
