#include "unity.h"
#include "crc8.h"

void setUp(void) {}
void tearDown(void) {}

static void test_datasheet_example(void)
{
    /* Example from the Sensirion SHT3x datasheet: 0xBEEF -> 0x92 */
    const uint8_t data[] = { 0xBE, 0xEF };
    TEST_ASSERT_EQUAL_HEX8(0x92, crc8_sensirion(data, sizeof(data)));
}

static void test_empty_buffer_returns_init_value(void)
{
    TEST_ASSERT_EQUAL_HEX8(0xFF, crc8_sensirion(NULL, 0U));
}

static void test_single_zero_byte(void)
{
    const uint8_t data[] = { 0x00 };
    TEST_ASSERT_EQUAL_HEX8(0xAC, crc8_sensirion(data, sizeof(data)));
}

static void test_bit_flip_changes_crc(void)
{
    const uint8_t a[] = { 0x66, 0x66 };
    const uint8_t b[] = { 0x66, 0x67 };
    TEST_ASSERT_NOT_EQUAL(crc8_sensirion(a, 2U), crc8_sensirion(b, 2U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_datasheet_example);
    RUN_TEST(test_empty_buffer_returns_init_value);
    RUN_TEST(test_single_zero_byte);
    RUN_TEST(test_bit_flip_changes_crc);
    return UNITY_END();
}
