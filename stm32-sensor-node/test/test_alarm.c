#include "unity.h"
#include "alarm.h"

static alarm_t a;

void setUp(void)
{
    /* 10.00 .. 30.00 degC, 0.50 degC hysteresis */
    TEST_ASSERT_TRUE(alarm_configure(&a, 1000, 3000, 50));
}

void tearDown(void) {}

static void test_configure_rejects_invalid_limits(void)
{
    alarm_t b;
    TEST_ASSERT_FALSE(alarm_configure(&b, 3000, 1000, 50));  /* low > high   */
    TEST_ASSERT_FALSE(alarm_configure(&b, 1000, 1000, 0));   /* low == high  */
    TEST_ASSERT_FALSE(alarm_configure(&b, 1000, 3000, -1));  /* negative hys */
    TEST_ASSERT_FALSE(alarm_configure(&b, 1000, 1080, 50));  /* band too small */
    TEST_ASSERT_FALSE(alarm_configure(NULL, 1000, 3000, 50));
}

static void test_starts_normal(void)
{
    TEST_ASSERT_EQUAL(ALARM_NORMAL, a.state);
    TEST_ASSERT_EQUAL(ALARM_NORMAL, alarm_update(&a, 2000));
}

static void test_limit_itself_is_not_an_alarm(void)
{
    TEST_ASSERT_EQUAL(ALARM_NORMAL, alarm_update(&a, 3000));
    TEST_ASSERT_EQUAL(ALARM_NORMAL, alarm_update(&a, 1000));
}

static void test_high_alarm_with_hysteresis(void)
{
    TEST_ASSERT_EQUAL(ALARM_HIGH, alarm_update(&a, 3001));
    TEST_ASSERT_EQUAL(ALARM_HIGH, alarm_update(&a, 2990));  /* inside band: stay */
    TEST_ASSERT_EQUAL(ALARM_HIGH, alarm_update(&a, 2951));
    TEST_ASSERT_EQUAL(ALARM_NORMAL, alarm_update(&a, 2950));
}

static void test_low_alarm_with_hysteresis(void)
{
    TEST_ASSERT_EQUAL(ALARM_LOW, alarm_update(&a, 999));
    TEST_ASSERT_EQUAL(ALARM_LOW, alarm_update(&a, 1049));
    TEST_ASSERT_EQUAL(ALARM_NORMAL, alarm_update(&a, 1050));
}

static void test_direct_jump_between_alarms(void)
{
    TEST_ASSERT_EQUAL(ALARM_HIGH, alarm_update(&a, 5000));
    TEST_ASSERT_EQUAL(ALARM_LOW, alarm_update(&a, -500));
    TEST_ASSERT_EQUAL(ALARM_HIGH, alarm_update(&a, 4000));
}

static void test_corrupted_state_recovers(void)
{
    a.state = (alarm_state_t)42;
    TEST_ASSERT_EQUAL(ALARM_NORMAL, alarm_update(&a, 2000));
}

static void test_state_names(void)
{
    TEST_ASSERT_EQUAL_STRING("NORMAL", alarm_state_name(ALARM_NORMAL));
    TEST_ASSERT_EQUAL_STRING("LOW", alarm_state_name(ALARM_LOW));
    TEST_ASSERT_EQUAL_STRING("HIGH", alarm_state_name(ALARM_HIGH));
    TEST_ASSERT_EQUAL_STRING("INVALID", alarm_state_name((alarm_state_t)99));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_configure_rejects_invalid_limits);
    RUN_TEST(test_starts_normal);
    RUN_TEST(test_limit_itself_is_not_an_alarm);
    RUN_TEST(test_high_alarm_with_hysteresis);
    RUN_TEST(test_low_alarm_with_hysteresis);
    RUN_TEST(test_direct_jump_between_alarms);
    RUN_TEST(test_corrupted_state_recovers);
    RUN_TEST(test_state_names);
    return UNITY_END();
}
