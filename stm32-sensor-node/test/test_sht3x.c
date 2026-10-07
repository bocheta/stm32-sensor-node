#include <string.h>

#include "unity.h"
#include "crc8.h"
#include "sht3x.h"

/* ---------------------------------------------------------------------- */
/* Fake I2C bus: records what the driver writes, replays a canned frame.  */
/* ---------------------------------------------------------------------- */

typedef struct {
    uint8_t  last_addr;
    uint8_t  written[4];
    size_t   written_len;
    uint8_t  response[6];
    int      write_result;
    int      read_result;
    uint32_t total_delay_ms;
    unsigned write_calls;
    unsigned read_calls;
} fake_bus_t;

static fake_bus_t fake;

static int fake_write(void *ctx, uint8_t addr, const uint8_t *data, size_t len)
{
    fake_bus_t *f = (fake_bus_t *)ctx;
    f->write_calls++;
    f->last_addr = addr;
    f->written_len = (len <= sizeof(f->written)) ? len : sizeof(f->written);
    memcpy(f->written, data, f->written_len);
    return f->write_result;
}

static int fake_read(void *ctx, uint8_t addr, uint8_t *data, size_t len)
{
    fake_bus_t *f = (fake_bus_t *)ctx;
    f->read_calls++;
    f->last_addr = addr;
    if (len <= sizeof(f->response)) {
        memcpy(data, f->response, len);
    }
    return f->read_result;
}

static void fake_delay(void *ctx, uint32_t ms)
{
    ((fake_bus_t *)ctx)->total_delay_ms += ms;
}

static const sht3x_bus_t bus = { fake_write, fake_read, fake_delay, &fake };
static sht3x_t dev;

static void set_response(uint16_t raw_t, uint16_t raw_rh)
{
    fake.response[0] = (uint8_t)(raw_t >> 8);
    fake.response[1] = (uint8_t)(raw_t & 0xFF);
    fake.response[2] = crc8_sensirion(&fake.response[0], 2U);
    fake.response[3] = (uint8_t)(raw_rh >> 8);
    fake.response[4] = (uint8_t)(raw_rh & 0xFF);
    fake.response[5] = crc8_sensirion(&fake.response[3], 2U);
}

void setUp(void)
{
    memset(&fake, 0, sizeof(fake));
    TEST_ASSERT_EQUAL(SHT3X_OK, sht3x_init(&dev, &bus, SHT3X_ADDR_DEFAULT));
}

void tearDown(void) {}

/* ---------------------------------------------------------------------- */

static void test_init_rejects_bad_arguments(void)
{
    sht3x_t d;
    const sht3x_bus_t incomplete = { fake_write, NULL, fake_delay, &fake };

    TEST_ASSERT_EQUAL(SHT3X_ERR_PARAM, sht3x_init(NULL, &bus, SHT3X_ADDR_DEFAULT));
    TEST_ASSERT_EQUAL(SHT3X_ERR_PARAM, sht3x_init(&d, NULL, SHT3X_ADDR_DEFAULT));
    TEST_ASSERT_EQUAL(SHT3X_ERR_PARAM, sht3x_init(&d, &incomplete, SHT3X_ADDR_DEFAULT));
    TEST_ASSERT_EQUAL(SHT3X_ERR_PARAM, sht3x_init(&d, &bus, 0x10));
    TEST_ASSERT_EQUAL(SHT3X_OK, sht3x_init(&d, &bus, SHT3X_ADDR_ALT));
}

static void test_conversion_endpoints(void)
{
    TEST_ASSERT_EQUAL_INT32(-4500, sht3x_raw_to_cdeg(0x0000));
    TEST_ASSERT_EQUAL_INT32(13000, sht3x_raw_to_cdeg(0xFFFF));
    TEST_ASSERT_EQUAL_INT32(2500,  sht3x_raw_to_cdeg(0x6666));  /* 25.00 degC */
    TEST_ASSERT_EQUAL_INT32(0,     sht3x_raw_to_cpct(0x0000));
    TEST_ASSERT_EQUAL_INT32(10000, sht3x_raw_to_cpct(0xFFFF));
    TEST_ASSERT_EQUAL_INT32(5000,  sht3x_raw_to_cpct(0x8000));  /* 50.00 %RH */
}

static void test_measure_sends_command_waits_and_converts(void)
{
    sht3x_reading_t r;
    set_response(0x6666, 0x8000);

    TEST_ASSERT_EQUAL(SHT3X_OK, sht3x_measure(&dev, &r));

    /* Single shot, high repeatability, clock stretching disabled: 0x2400 */
    TEST_ASSERT_EQUAL_size_t(2U, fake.written_len);
    TEST_ASSERT_EQUAL_HEX8(0x24, fake.written[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, fake.written[1]);
    TEST_ASSERT_EQUAL_HEX8(SHT3X_ADDR_DEFAULT, fake.last_addr);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(16U, fake.total_delay_ms);

    TEST_ASSERT_EQUAL_INT32(2500, r.temperature_cdeg);
    TEST_ASSERT_EQUAL_INT32(5000, r.humidity_cpct);
}

static void test_measure_detects_temperature_crc_error(void)
{
    sht3x_reading_t r;
    set_response(0x6666, 0x8000);
    fake.response[2] ^= 0x01U;
    TEST_ASSERT_EQUAL(SHT3X_ERR_CRC, sht3x_measure(&dev, &r));
}

static void test_measure_detects_humidity_crc_error(void)
{
    sht3x_reading_t r;
    set_response(0x6666, 0x8000);
    fake.response[4] ^= 0x80U;   /* corrupt data, keep old CRC */
    TEST_ASSERT_EQUAL(SHT3X_ERR_CRC, sht3x_measure(&dev, &r));
}

static void test_measure_write_nack_skips_read(void)
{
    sht3x_reading_t r;
    fake.write_result = -1;
    TEST_ASSERT_EQUAL(SHT3X_ERR_BUS, sht3x_measure(&dev, &r));
    TEST_ASSERT_EQUAL_UINT(0U, fake.read_calls);
}

static void test_measure_read_failure(void)
{
    sht3x_reading_t r;
    set_response(0x6666, 0x8000);
    fake.read_result = -1;
    TEST_ASSERT_EQUAL(SHT3X_ERR_BUS, sht3x_measure(&dev, &r));
}

static void test_soft_reset_sends_command(void)
{
    TEST_ASSERT_EQUAL(SHT3X_OK, sht3x_soft_reset(&dev));
    TEST_ASSERT_EQUAL_HEX8(0x30, fake.written[0]);
    TEST_ASSERT_EQUAL_HEX8(0xA2, fake.written[1]);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, fake.total_delay_ms);
}

static void test_measure_null_output(void)
{
    TEST_ASSERT_EQUAL(SHT3X_ERR_PARAM, sht3x_measure(&dev, NULL));
    TEST_ASSERT_EQUAL_UINT(0U, fake.write_calls);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_rejects_bad_arguments);
    RUN_TEST(test_conversion_endpoints);
    RUN_TEST(test_measure_sends_command_waits_and_converts);
    RUN_TEST(test_measure_detects_temperature_crc_error);
    RUN_TEST(test_measure_detects_humidity_crc_error);
    RUN_TEST(test_measure_write_nack_skips_read);
    RUN_TEST(test_measure_read_failure);
    RUN_TEST(test_soft_reset_sends_command);
    RUN_TEST(test_measure_null_output);
    return UNITY_END();
}
