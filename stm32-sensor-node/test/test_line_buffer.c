#include <string.h>

#include "unity.h"
#include "line_buffer.h"

static line_buffer_t lb;

void setUp(void)
{
    line_buffer_init(&lb);
}

void tearDown(void) {}

static line_result_t feed(const char *s)
{
    line_result_t r = LINE_PENDING;
    while (*s != '\0') {
        r = line_buffer_push(&lb, *s);
        s++;
    }
    return r;
}

static void test_simple_line_lf(void)
{
    TEST_ASSERT_EQUAL(LINE_PENDING, feed("help"));
    TEST_ASSERT_EQUAL(LINE_READY, feed("\n"));
    TEST_ASSERT_EQUAL_STRING("help", line_buffer_get(&lb));
}

static void test_crlf_gives_one_line(void)
{
    TEST_ASSERT_EQUAL(LINE_READY, feed("read\r"));
    TEST_ASSERT_EQUAL_STRING("read", line_buffer_get(&lb));
    TEST_ASSERT_EQUAL(LINE_PENDING, feed("\n"));   /* LF of CRLF ignored */
    TEST_ASSERT_EQUAL(LINE_READY, feed("status\r"));
    TEST_ASSERT_EQUAL_STRING("status", line_buffer_get(&lb));
}

static void test_empty_lines_ignored(void)
{
    TEST_ASSERT_EQUAL(LINE_PENDING, feed("\r\n\r\n"));
}

static void test_backspace_removes_char(void)
{
    feed("raxd");
    line_buffer_push(&lb, '\b');
    line_buffer_push(&lb, 0x7F);
    TEST_ASSERT_EQUAL(LINE_READY, feed("ed\n"));
    TEST_ASSERT_EQUAL_STRING("raed", line_buffer_get(&lb));
}

static void test_backspace_on_empty_is_harmless(void)
{
    line_buffer_push(&lb, '\b');
    TEST_ASSERT_EQUAL(LINE_READY, feed("ok\n"));
    TEST_ASSERT_EQUAL_STRING("ok", line_buffer_get(&lb));
}

static void test_control_chars_ignored(void)
{
    TEST_ASSERT_EQUAL(LINE_READY, feed("a\tb\x1b" "c\n"));
    TEST_ASSERT_EQUAL_STRING("abc", line_buffer_get(&lb));
}

static void test_max_length_line_fits(void)
{
    for (size_t i = 0U; i < (LINE_BUFFER_SIZE - 1U); i++) {
        TEST_ASSERT_EQUAL(LINE_PENDING, line_buffer_push(&lb, 'x'));
    }
    TEST_ASSERT_EQUAL(LINE_READY, line_buffer_push(&lb, '\n'));
    TEST_ASSERT_EQUAL_size_t(LINE_BUFFER_SIZE - 1U, strlen(line_buffer_get(&lb)));
}

static void test_overflow_discards_line_and_recovers(void)
{
    for (size_t i = 0U; i < (LINE_BUFFER_SIZE + 10U); i++) {
        (void)line_buffer_push(&lb, 'x');
    }
    TEST_ASSERT_EQUAL(LINE_OVERFLOW, line_buffer_push(&lb, '\n'));
    TEST_ASSERT_EQUAL(LINE_READY, feed("read\n"));
    TEST_ASSERT_EQUAL_STRING("read", line_buffer_get(&lb));
}

static void test_consecutive_lines(void)
{
    TEST_ASSERT_EQUAL(LINE_READY, feed("one\n"));
    TEST_ASSERT_EQUAL_STRING("one", line_buffer_get(&lb));
    TEST_ASSERT_EQUAL(LINE_READY, feed("two\n"));
    TEST_ASSERT_EQUAL_STRING("two", line_buffer_get(&lb));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_simple_line_lf);
    RUN_TEST(test_crlf_gives_one_line);
    RUN_TEST(test_empty_lines_ignored);
    RUN_TEST(test_backspace_removes_char);
    RUN_TEST(test_backspace_on_empty_is_harmless);
    RUN_TEST(test_control_chars_ignored);
    RUN_TEST(test_max_length_line_fits);
    RUN_TEST(test_overflow_discards_line_and_recovers);
    RUN_TEST(test_consecutive_lines);
    return UNITY_END();
}
