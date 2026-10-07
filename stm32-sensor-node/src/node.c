/**
 * @file node.c
 * @brief Application logic of the sensor node.
 */
#include "node.h"

#include <stdio.h>
#include <string.h>

#define NODE_MAX_TOKENS 4U

typedef struct {
    const char *start;
    size_t      len;
} token_t;

/* ---------------------------------------------------------------------- */
/* Helpers                                                                */
/* ---------------------------------------------------------------------- */

static size_t tokenize(const char *line, token_t *tokens, size_t max_tokens)
{
    size_t count = 0U;
    const char *p = line;

    while (*p != '\0') {
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (count == max_tokens) {
            return max_tokens + 1U;   /* too many arguments */
        }
        tokens[count].start = p;
        while ((*p != ' ') && (*p != '\0')) {
            p++;
        }
        tokens[count].len = (size_t)(p - tokens[count].start);
        count++;
    }
    return count;
}

static bool token_equals(const token_t *t, const char *word)
{
    const size_t n = strlen(word);
    return (t->len == n) && (strncmp(t->start, word, n) == 0);
}

bool node_parse_int32(const char *str, size_t len, int32_t *out)
{
    size_t i = 0U;
    bool negative = false;
    int64_t value = 0;

    if ((str == NULL) || (out == NULL) || (len == 0U)) {
        return false;
    }
    if (str[0] == '-') {
        negative = true;
        i = 1U;
        if (len == 1U) {
            return false;
        }
    }

    for (; i < len; i++) {
        if ((str[i] < '0') || (str[i] > '9')) {
            return false;
        }
        value = (value * 10) + (int64_t)(str[i] - '0');
        if (value > ((int64_t)INT32_MAX + 1)) {
            return false;
        }
    }

    if (negative) {
        value = -value;
    }
    if ((value > INT32_MAX) || (value < INT32_MIN)) {
        return false;
    }
    *out = (int32_t)value;
    return true;
}

void node_format_fixed2(int32_t value, char *out, size_t out_len)
{
    const bool negative = (value < 0);
    /* Work in int64 so INT32_MIN can be negated safely. */
    const int64_t abs_val = negative ? -(int64_t)value : (int64_t)value;

    (void)snprintf(out, out_len, "%s%ld.%02ld", negative ? "-" : "",
                   (long)(abs_val / 100), (long)(abs_val % 100));
}

/* ---------------------------------------------------------------------- */
/* Public API                                                             */
/* ---------------------------------------------------------------------- */

void node_init(node_t *node)
{
    (void)memset(node, 0, sizeof(*node));
    node->period_ms   = NODE_PERIOD_DEFAULT_MS;
    node->streaming   = true;
    node->last_status = SHT3X_OK;
    (void)moving_average_init(&node->temp_filter, NODE_FILTER_WINDOW);
    (void)moving_average_init(&node->hum_filter, NODE_FILTER_WINDOW);
    (void)alarm_configure(&node->temp_alarm, NODE_TEMP_LOW_DEFAULT,
                          NODE_TEMP_HIGH_DEFAULT, NODE_TEMP_HYSTERESIS);
}

bool node_on_measurement(node_t *node, sht3x_status_t status,
                         const sht3x_reading_t *raw)
{
    node->last_status = status;

    if ((status != SHT3X_OK) || (raw == NULL)) {
        node->error_count++;
        return false;
    }

    node->sample_count++;
    node->filtered.temperature_cdeg =
        moving_average_add(&node->temp_filter, raw->temperature_cdeg);
    node->filtered.humidity_cpct =
        moving_average_add(&node->hum_filter, raw->humidity_cpct);
    node->has_reading = true;

    const alarm_state_t before = node->temp_alarm.state;
    const alarm_state_t after =
        alarm_update(&node->temp_alarm, node->filtered.temperature_cdeg);
    return before != after;
}

void node_format_reading(const node_t *node, char *out, size_t out_len)
{
    char t[16];
    char rh[16];

    if (!node->has_reading) {
        (void)snprintf(out, out_len, "no data yet");
        return;
    }

    node_format_fixed2(node->filtered.temperature_cdeg, t, sizeof(t));
    node_format_fixed2(node->filtered.humidity_cpct, rh, sizeof(rh));
    (void)snprintf(out, out_len, "T=%sC RH=%s%% ALARM=%s", t, rh,
                   alarm_state_name(node->temp_alarm.state));
}

static bool cmd_rate(node_t *node, const token_t *tok, size_t n,
                     char *out, size_t out_len)
{
    int32_t ms = 0;

    if ((n != 2U) || !node_parse_int32(tok[1].start, tok[1].len, &ms) ||
        (ms < (int32_t)NODE_PERIOD_MIN_MS) || (ms > (int32_t)NODE_PERIOD_MAX_MS)) {
        (void)snprintf(out, out_len, "ERR usage: rate <%u..%u ms>",
                       (unsigned)NODE_PERIOD_MIN_MS, (unsigned)NODE_PERIOD_MAX_MS);
        return false;
    }
    node->period_ms = (uint32_t)ms;
    (void)snprintf(out, out_len, "OK rate=%ldms", (long)ms);
    return true;
}

static bool cmd_stream(node_t *node, const token_t *tok, size_t n,
                       char *out, size_t out_len)
{
    if ((n == 2U) && token_equals(&tok[1], "on")) {
        node->streaming = true;
    } else if ((n == 2U) && token_equals(&tok[1], "off")) {
        node->streaming = false;
    } else {
        (void)snprintf(out, out_len, "ERR usage: stream on|off");
        return false;
    }
    (void)snprintf(out, out_len, "OK stream=%s", node->streaming ? "on" : "off");
    return true;
}

static bool cmd_limits(node_t *node, const token_t *tok, size_t n,
                       char *out, size_t out_len)
{
    int32_t low = 0;
    int32_t high = 0;

    if ((n != 3U) ||
        !node_parse_int32(tok[1].start, tok[1].len, &low) ||
        !node_parse_int32(tok[2].start, tok[2].len, &high) ||
        (low < NODE_LIMIT_MIN_DEG) || (high > NODE_LIMIT_MAX_DEG) ||
        !alarm_configure(&node->temp_alarm, low * 100, high * 100,
                         NODE_TEMP_HYSTERESIS)) {
        (void)snprintf(out, out_len,
                       "ERR usage: limits <low> <high> (degC, %d..%d, low < high)",
                       NODE_LIMIT_MIN_DEG, NODE_LIMIT_MAX_DEG);
        return false;
    }
    (void)snprintf(out, out_len, "OK limits=%ld..%ldC", (long)low, (long)high);
    return true;
}

static bool cmd_status(const node_t *node, char *out, size_t out_len)
{
    char lo[16];
    char hi[16];

    node_format_fixed2(node->temp_alarm.low_limit, lo, sizeof(lo));
    node_format_fixed2(node->temp_alarm.high_limit, hi, sizeof(hi));
    (void)snprintf(out, out_len,
                   "rate=%lums stream=%s limits=%s..%sC samples=%lu errors=%lu alarm=%s",
                   (unsigned long)node->period_ms,
                   node->streaming ? "on" : "off", lo, hi,
                   (unsigned long)node->sample_count,
                   (unsigned long)node->error_count,
                   alarm_state_name(node->temp_alarm.state));
    return true;
}

bool node_handle_command(node_t *node, const char *line,
                         char *out, size_t out_len)
{
    token_t tok[NODE_MAX_TOKENS];
    const size_t n = tokenize(line, tok, NODE_MAX_TOKENS);

    if ((n == 0U) || (n > NODE_MAX_TOKENS)) {
        (void)snprintf(out, out_len, "ERR unknown command, type 'help'");
        return false;
    }

    if (token_equals(&tok[0], "help") && (n == 1U)) {
        (void)snprintf(out, out_len,
                       "commands: help | status | read | rate <ms> | "
                       "stream on|off | limits <low> <high>");
        return true;
    }
    if (token_equals(&tok[0], "status") && (n == 1U)) {
        return cmd_status(node, out, out_len);
    }
    if (token_equals(&tok[0], "read") && (n == 1U)) {
        node_format_reading(node, out, out_len);
        return node->has_reading;
    }
    if (token_equals(&tok[0], "rate")) {
        return cmd_rate(node, tok, n, out, out_len);
    }
    if (token_equals(&tok[0], "stream")) {
        return cmd_stream(node, tok, n, out, out_len);
    }
    if (token_equals(&tok[0], "limits")) {
        return cmd_limits(node, tok, n, out, out_len);
    }

    (void)snprintf(out, out_len, "ERR unknown command, type 'help'");
    return false;
}
