#include "unity.h"
#include "moving_average.h"

static moving_average_t f;

void setUp(void)
{
    TEST_ASSERT_TRUE(moving_average_init(&f, 4U));
}

void tearDown(void) {}

static void test_init_rejects_invalid_window(void)
{
    moving_average_t g;
    TEST_ASSERT_FALSE(moving_average_init(&g, 0U));
    TEST_ASSERT_FALSE(moving_average_init(&g, MOVING_AVERAGE_MAX_WINDOW + 1U));
    TEST_ASSERT_FALSE(moving_average_init(NULL, 4U));
    TEST_ASSERT_TRUE(moving_average_init(&g, MOVING_AVERAGE_MAX_WINDOW));
}

static void test_partial_window_averages_seen_samples(void)
{
    TEST_ASSERT_EQUAL_INT32(10, moving_average_add(&f, 10));
    TEST_ASSERT_EQUAL_INT32(15, moving_average_add(&f, 20));
    TEST_ASSERT_EQUAL_INT32(20, moving_average_add(&f, 30));
}

static void test_full_window_drops_oldest(void)
{
    (void)moving_average_add(&f, 100);
    (void)moving_average_add(&f, 0);
    (void)moving_average_add(&f, 0);
    TEST_ASSERT_EQUAL_INT32(25, moving_average_add(&f, 0));   /* 100,0,0,0 */
    TEST_ASSERT_EQUAL_INT32(0, moving_average_add(&f, 0));    /* 0,0,0,0   */
}

static void test_rounding_positive_and_negative(void)
{
    moving_average_t g;
    TEST_ASSERT_TRUE(moving_average_init(&g, 2U));
    (void)moving_average_add(&g, 1);
    TEST_ASSERT_EQUAL_INT32(2, moving_average_add(&g, 2));    /*  1.5 ->  2 */

    TEST_ASSERT_TRUE(moving_average_init(&g, 2U));
    (void)moving_average_add(&g, -1);
    TEST_ASSERT_EQUAL_INT32(-2, moving_average_add(&g, -2));  /* -1.5 -> -2 */
}

static void test_extreme_values_do_not_overflow(void)
{
    moving_average_t g;
    TEST_ASSERT_TRUE(moving_average_init(&g, MOVING_AVERAGE_MAX_WINDOW));
    int32_t avg = 0;
    for (unsigned i = 0U; i < MOVING_AVERAGE_MAX_WINDOW; i++) {
        avg = moving_average_add(&g, INT32_MAX);
    }
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, avg);
}

static void test_reset_clears_history(void)
{
    (void)moving_average_add(&f, 1000);
    moving_average_reset(&f);
    TEST_ASSERT_EQUAL_INT32(4, moving_average_add(&f, 4));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_rejects_invalid_window);
    RUN_TEST(test_partial_window_averages_seen_samples);
    RUN_TEST(test_full_window_drops_oldest);
    RUN_TEST(test_rounding_positive_and_negative);
    RUN_TEST(test_extreme_values_do_not_overflow);
    RUN_TEST(test_reset_clears_history);
    return UNITY_END();
}
