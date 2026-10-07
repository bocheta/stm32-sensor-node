#include <string.h>

#include "unity.h"
#include "node.h"

static node_t node;
static char out[160];

void setUp(void)
{
    node_init(&node);
    memset(out, 0, sizeof(out));
}

void tearDown(void) {}

static void feed_temperature(int32_t cdeg, unsigned times)
{
    const sht3x_reading_t r = { cdeg, 4500 };
    for (unsigned i = 0U; i < times; i++) {
        (void)node_on_measurement(&node, SHT3X_OK, &r);
    }
}

/* ---- helpers ---------------------------------------------------------- */

static void test_format_fixed2(void)
{
    char buf[16];
    node_format_fixed2(2345, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("23.45", buf);
    node_format_fixed2(-50, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-0.50", buf);
    node_format_fixed2(5, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0.05", buf);
    node_format_fixed2(-4500, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-45.00", buf);
    node_format_fixed2(INT32_MIN, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-21474836.48", buf);
}

static void test_parse_int32(void)
{
    int32_t v = 0;
    TEST_ASSERT_TRUE(node_parse_int32("1234", 4U, &v));
    TEST_ASSERT_EQUAL_INT32(1234, v);
    TEST_ASSERT_TRUE(node_parse_int32("-40", 3U, &v));
    TEST_ASSERT_EQUAL_INT32(-40, v);
    TEST_ASSERT_TRUE(node_parse_int32("2147483647", 10U, &v));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, v);
    TEST_ASSERT_TRUE(node_parse_int32("-2147483648", 11U, &v));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, v);

    TEST_ASSERT_FALSE(node_parse_int32("2147483648", 10U, &v));
    TEST_ASSERT_FALSE(node_parse_int32("99999999999999999999", 20U, &v));
    TEST_ASSERT_FALSE(node_parse_int32("12a", 3U, &v));
    TEST_ASSERT_FALSE(node_parse_int32("-", 1U, &v));
    TEST_ASSERT_FALSE(node_parse_int32("", 0U, &v));
}

/* ---- measurement handling -------------------------------------------- */

static void test_defaults(void)
{
    TEST_ASSERT_EQUAL_UINT32(NODE_PERIOD_DEFAULT_MS, node.period_ms);
    TEST_ASSERT_TRUE(node.streaming);
    TEST_ASSERT_FALSE(node.has_reading);
    node_format_reading(&node, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("no data yet", out);
}

static void test_measurement_is_filtered_and_formatted(void)
{
    const sht3x_reading_t a = { 2000, 4000 };
    const sht3x_reading_t b = { 2100, 4200 };
    (void)node_on_measurement(&node, SHT3X_OK, &a);
    (void)node_on_measurement(&node, SHT3X_OK, &b);

    TEST_ASSERT_EQUAL_INT32(2050, node.filtered.temperature_cdeg);
    node_format_reading(&node, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("T=20.50C RH=41.00% ALARM=NORMAL", out);
}

static void test_errors_are_counted_not_filtered(void)
{
    const sht3x_reading_t junk = { 99999, 99999 };
    TEST_ASSERT_FALSE(node_on_measurement(&node, SHT3X_ERR_CRC, &junk));
    TEST_ASSERT_EQUAL_UINT32(1U, node.error_count);
    TEST_ASSERT_EQUAL_UINT32(0U, node.sample_count);
    TEST_ASSERT_FALSE(node.has_reading);
}

static void test_alarm_change_is_reported_once(void)
{
    const sht3x_reading_t hot = { 4000, 4000 };
    bool changed = false;
    unsigned changes = 0U;

    /* The filter starts empty, so the first 40 degC sample already trips
     * the alarm; later samples must not report another change. */
    for (unsigned i = 0U; i < 6U; i++) {
        changed = node_on_measurement(&node, SHT3X_OK, &hot);
        if (changed) {
            changes++;
        }
    }
    TEST_ASSERT_EQUAL_UINT(1U, changes);
    TEST_ASSERT_EQUAL(ALARM_HIGH, node.temp_alarm.state);
}

/* ---- commands --------------------------------------------------------- */

static void test_cmd_help(void)
{
    TEST_ASSERT_TRUE(node_handle_command(&node, "help", out, sizeof(out)));
    TEST_ASSERT_NOT_NULL(strstr(out, "limits"));
}

static void test_cmd_unknown_and_empty(void)
{
    TEST_ASSERT_FALSE(node_handle_command(&node, "reboot", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("ERR unknown command, type 'help'", out);
    TEST_ASSERT_FALSE(node_handle_command(&node, "   ", out, sizeof(out)));
    TEST_ASSERT_FALSE(node_handle_command(&node, "help me", out, sizeof(out)));
}

static void test_cmd_rate(void)
{
    TEST_ASSERT_TRUE(node_handle_command(&node, "rate 250", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("OK rate=250ms", out);
    TEST_ASSERT_EQUAL_UINT32(250U, node.period_ms);

    TEST_ASSERT_TRUE(node_handle_command(&node, "  rate   60000 ", out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT32(60000U, node.period_ms);
}

static void test_cmd_rate_rejects_bad_values(void)
{
    TEST_ASSERT_FALSE(node_handle_command(&node, "rate 99", out, sizeof(out)));
    TEST_ASSERT_FALSE(node_handle_command(&node, "rate 60001", out, sizeof(out)));
    TEST_ASSERT_FALSE(node_handle_command(&node, "rate fast", out, sizeof(out)));
    TEST_ASSERT_FALSE(node_handle_command(&node, "rate", out, sizeof(out)));
    TEST_ASSERT_FALSE(node_handle_command(&node, "rate -500", out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT32(NODE_PERIOD_DEFAULT_MS, node.period_ms);
}

static void test_cmd_stream(void)
{
    TEST_ASSERT_TRUE(node_handle_command(&node, "stream off", out, sizeof(out)));
    TEST_ASSERT_FALSE(node.streaming);
    TEST_ASSERT_EQUAL_STRING("OK stream=off", out);
    TEST_ASSERT_TRUE(node_handle_command(&node, "stream on", out, sizeof(out)));
    TEST_ASSERT_TRUE(node.streaming);
    TEST_ASSERT_FALSE(node_handle_command(&node, "stream maybe", out, sizeof(out)));
}

static void test_cmd_limits(void)
{
    TEST_ASSERT_TRUE(node_handle_command(&node, "limits -5 40", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("OK limits=-5..40C", out);
    TEST_ASSERT_EQUAL_INT32(-500, node.temp_alarm.low_limit);
    TEST_ASSERT_EQUAL_INT32(4000, node.temp_alarm.high_limit);

    feed_temperature(4500, 4U);
    TEST_ASSERT_EQUAL(ALARM_HIGH, node.temp_alarm.state);
}

static void test_cmd_limits_rejects_bad_values(void)
{
    TEST_ASSERT_FALSE(node_handle_command(&node, "limits 40 -5", out, sizeof(out)));
    TEST_ASSERT_FALSE(node_handle_command(&node, "limits 20 20", out, sizeof(out)));
    TEST_ASSERT_FALSE(node_handle_command(&node, "limits -50 20", out, sizeof(out)));
    TEST_ASSERT_FALSE(node_handle_command(&node, "limits 0 200", out, sizeof(out)));
    TEST_ASSERT_FALSE(node_handle_command(&node, "limits 10", out, sizeof(out)));
    /* previous limits kept */
    TEST_ASSERT_EQUAL_INT32(NODE_TEMP_LOW_DEFAULT, node.temp_alarm.low_limit);
    TEST_ASSERT_EQUAL_INT32(NODE_TEMP_HIGH_DEFAULT, node.temp_alarm.high_limit);
}

static void test_cmd_read_and_status(void)
{
    TEST_ASSERT_FALSE(node_handle_command(&node, "read", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("no data yet", out);

    feed_temperature(2345, 4U);
    TEST_ASSERT_TRUE(node_handle_command(&node, "read", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("T=23.45C RH=45.00% ALARM=NORMAL", out);

    TEST_ASSERT_TRUE(node_handle_command(&node, "status", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(
        "rate=1000ms stream=on limits=10.00..30.00C samples=4 errors=0 alarm=NORMAL",
        out);
}

static void test_small_output_buffer_is_safe(void)
{
    char tiny[8];
    memset(tiny, 'Z', sizeof(tiny));
    (void)node_handle_command(&node, "help", tiny, sizeof(tiny));
    TEST_ASSERT_EQUAL_CHAR('\0', tiny[sizeof(tiny) - 1U]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_format_fixed2);
    RUN_TEST(test_parse_int32);
    RUN_TEST(test_defaults);
    RUN_TEST(test_measurement_is_filtered_and_formatted);
    RUN_TEST(test_errors_are_counted_not_filtered);
    RUN_TEST(test_alarm_change_is_reported_once);
    RUN_TEST(test_cmd_help);
    RUN_TEST(test_cmd_unknown_and_empty);
    RUN_TEST(test_cmd_rate);
    RUN_TEST(test_cmd_rate_rejects_bad_values);
    RUN_TEST(test_cmd_stream);
    RUN_TEST(test_cmd_limits);
    RUN_TEST(test_cmd_limits_rejects_bad_values);
    RUN_TEST(test_cmd_read_and_status);
    RUN_TEST(test_small_output_buffer_is_safe);
    return UNITY_END();
}
