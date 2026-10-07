/**
 * @file main.c
 * @brief FreeRTOS application: sensor sampling, serial console, heartbeat.
 *
 * Tasks
 *   sensor    (prio 3)  periodic SHT3x measurement, filtering, alarm
 *   console   (prio 2)  line-based command interface over the VCP UART
 *   heartbeat (prio 1)  LED: slow blink = normal, fast blink = alarm
 *
 * Shared state lives in one node_t guarded by g_node_mutex. Console output
 * is serialised by g_uart_mutex. UART RX bytes travel from the ISR to the
 * console task through a stream buffer.
 */
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "task.h"

#include "bsp.h"
#include "line_buffer.h"
#include "node.h"
#include "sht3x.h"

#define SENSOR_TASK_PRIO     (tskIDLE_PRIORITY + 3U)
#define CONSOLE_TASK_PRIO    (tskIDLE_PRIORITY + 2U)
#define HEARTBEAT_TASK_PRIO  (tskIDLE_PRIORITY + 1U)

#define SENSOR_TASK_STACK    512U   /* words */
#define CONSOLE_TASK_STACK   512U
#define HEARTBEAT_TASK_STACK 128U

#define RX_STREAM_SIZE       64U
#define OUT_LINE_LEN         160U

#define HEARTBEAT_NORMAL_MS  500U
#define HEARTBEAT_ALARM_MS   100U

static node_t               g_node;
static SemaphoreHandle_t    g_node_mutex;
static SemaphoreHandle_t    g_uart_mutex;
static StreamBufferHandle_t g_rx_stream;

/* ---------------------------------------------------------------------- */
/* Console output                                                         */
/* ---------------------------------------------------------------------- */

static void console_println(const char *text)
{
    (void)xSemaphoreTake(g_uart_mutex, portMAX_DELAY);
    bsp_uart_write(text, strlen(text));
    bsp_uart_write("\r\n", 2U);
    (void)xSemaphoreGive(g_uart_mutex);
}

static void console_echo(char c)
{
    (void)xSemaphoreTake(g_uart_mutex, portMAX_DELAY);
    if ((c == '\r') || (c == '\n')) {
        bsp_uart_write("\r\n", 2U);
    } else if ((c == '\b') || (c == (char)0x7F)) {
        bsp_uart_write("\b \b", 3U);
    } else {
        bsp_uart_write(&c, 1U);
    }
    (void)xSemaphoreGive(g_uart_mutex);
}

/* Called from USART2_IRQHandler. */
void bsp_uart_rx_isr_hook(uint8_t byte)
{
    BaseType_t woken = pdFALSE;
    (void)xStreamBufferSendFromISR(g_rx_stream, &byte, 1U, &woken);
    portYIELD_FROM_ISR(woken);
}

/* ---------------------------------------------------------------------- */
/* SHT3x bus adapter                                                      */
/* ---------------------------------------------------------------------- */

static int bus_write(void *ctx, uint8_t addr, const uint8_t *data, size_t len)
{
    (void)ctx;
    return bsp_i2c_write(addr, data, len);
}

static int bus_read(void *ctx, uint8_t addr, uint8_t *data, size_t len)
{
    (void)ctx;
    return bsp_i2c_read(addr, data, len);
}

static void bus_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    vTaskDelay(pdMS_TO_TICKS(ms) + 1U);  /* +1: at least the requested time */
}

static const sht3x_bus_t g_sht3x_bus = {
    .write    = bus_write,
    .read     = bus_read,
    .delay_ms = bus_delay_ms,
    .ctx      = NULL,
};

/* ---------------------------------------------------------------------- */
/* Tasks                                                                  */
/* ---------------------------------------------------------------------- */

static void sensor_task(void *arg)
{
    (void)arg;
    sht3x_t sensor;
    sht3x_reading_t raw;
    char line[OUT_LINE_LEN];
    TickType_t last_wake = xTaskGetTickCount();

    const sht3x_status_t init_st =
        sht3x_init(&sensor, &g_sht3x_bus, SHT3X_ADDR_DEFAULT);
    configASSERT(init_st == SHT3X_OK);
    if (sht3x_soft_reset(&sensor) != SHT3X_OK) {
        console_println("WARN sensor not responding, will keep retrying");
    }

    for (;;) {
        const sht3x_status_t st = sht3x_measure(&sensor, &raw);

        (void)xSemaphoreTake(g_node_mutex, portMAX_DELAY);
        const bool alarm_changed = node_on_measurement(&g_node, st, &raw);
        const bool streaming = g_node.streaming;
        const uint32_t period_ms = g_node.period_ms;
        const alarm_state_t alarm = g_node.temp_alarm.state;
        if (streaming && (st == SHT3X_OK)) {
            node_format_reading(&g_node, line, sizeof(line));
        }
        (void)xSemaphoreGive(g_node_mutex);

        if (st != SHT3X_OK) {
            (void)snprintf(line, sizeof(line), "ERR sensor read failed (%s)",
                           (st == SHT3X_ERR_CRC) ? "crc" : "bus");
            console_println(line);
        } else if (streaming) {
            console_println(line);
        } else {
            /* streaming disabled: stay quiet */
        }

        if (alarm_changed) {
            (void)snprintf(line, sizeof(line), "EVENT alarm=%s",
                           alarm_state_name(alarm));
            console_println(line);
        }

        /* Fixed-rate sampling, independent of how long the measurement took. */
        (void)xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(period_ms));
    }
}

static void console_task(void *arg)
{
    (void)arg;
    line_buffer_t lb;
    char response[OUT_LINE_LEN];
    char c;

    line_buffer_init(&lb);
    console_println("");
    console_println("stm32-sensor-node ready. Type 'help'.");
    bsp_uart_enable_rx_irq();

    for (;;) {
        if (xStreamBufferReceive(g_rx_stream, &c, 1U, portMAX_DELAY) != 1U) {
            continue;
        }
        console_echo(c);

        switch (line_buffer_push(&lb, c)) {
        case LINE_READY:
            (void)xSemaphoreTake(g_node_mutex, portMAX_DELAY);
            (void)node_handle_command(&g_node, line_buffer_get(&lb),
                                      response, sizeof(response));
            (void)xSemaphoreGive(g_node_mutex);
            console_println(response);
            break;

        case LINE_OVERFLOW:
            console_println("ERR line too long");
            break;

        case LINE_PENDING:
        default:
            break;
        }
    }
}

static void heartbeat_task(void *arg)
{
    (void)arg;

    for (;;) {
        (void)xSemaphoreTake(g_node_mutex, portMAX_DELAY);
        const bool alarm = (g_node.temp_alarm.state != ALARM_NORMAL);
        (void)xSemaphoreGive(g_node_mutex);

        bsp_led_toggle();
        vTaskDelay(pdMS_TO_TICKS(alarm ? HEARTBEAT_ALARM_MS : HEARTBEAT_NORMAL_MS));
    }
}

/* ---------------------------------------------------------------------- */
/* FreeRTOS hooks                                                         */
/* ---------------------------------------------------------------------- */

void vApplicationStackOverflowHook(TaskHandle_t task, char *name);
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    configASSERT(0);
}

void vApplicationMallocFailedHook(void);
void vApplicationMallocFailedHook(void)
{
    configASSERT(0);
}

/* ---------------------------------------------------------------------- */

int main(void)
{
    bsp_init();
    node_init(&g_node);

    g_node_mutex = xSemaphoreCreateMutex();
    g_uart_mutex = xSemaphoreCreateMutex();
    g_rx_stream  = xStreamBufferCreate(RX_STREAM_SIZE, 1U);
    configASSERT((g_node_mutex != NULL) && (g_uart_mutex != NULL) &&
                 (g_rx_stream != NULL));

    BaseType_t ok = pdPASS;
    ok &= xTaskCreate(sensor_task, "sensor", SENSOR_TASK_STACK, NULL,
                      SENSOR_TASK_PRIO, NULL);
    ok &= xTaskCreate(console_task, "console", CONSOLE_TASK_STACK, NULL,
                      CONSOLE_TASK_PRIO, NULL);
    ok &= xTaskCreate(heartbeat_task, "heartbeat", HEARTBEAT_TASK_STACK, NULL,
                      HEARTBEAT_TASK_PRIO, NULL);
    configASSERT(ok == pdPASS);

    vTaskStartScheduler();

    /* Only reached if there was not enough heap for the idle task. */
    for (;;) {
    }
}
