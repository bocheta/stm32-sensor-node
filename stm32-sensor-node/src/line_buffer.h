/**
 * @file line_buffer.h
 * @brief Assembles a stream of characters (e.g. from a UART) into lines.
 *
 * Handles CR, LF and CRLF line endings, backspace/DEL, and overlong lines
 * (which are discarded and reported once, instead of being cut silently).
 */
#ifndef LINE_BUFFER_H
#define LINE_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

#define LINE_BUFFER_SIZE 64U

typedef enum {
    LINE_PENDING = 0,   /**< No complete line yet. */
    LINE_READY,         /**< A line is ready; read it with line_buffer_get(). */
    LINE_OVERFLOW       /**< Line was too long and has been discarded. */
} line_result_t;

typedef struct {
    char   buf[LINE_BUFFER_SIZE];
    size_t len;
    bool   overflowed;
    bool   ready;
} line_buffer_t;

void line_buffer_init(line_buffer_t *lb);

/**
 * @brief Feed one character.
 *
 * After LINE_READY, the line stays available until the next call.
 */
line_result_t line_buffer_push(line_buffer_t *lb, char c);

/** @brief The last completed line (NUL-terminated). */
const char *line_buffer_get(const line_buffer_t *lb);

#endif /* LINE_BUFFER_H */
