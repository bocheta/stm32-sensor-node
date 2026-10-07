/**
 * @file bsp.h
 * @brief Board support for NUCLEO-F446RE: LED, console UART, I2C bus.
 *
 * Pin map:
 *   LD2 (green LED)    PA5
 *   USART2 TX / RX     PA2 / PA3   (ST-LINK virtual COM port, 115200 8N1)
 *   I2C1 SCL / SDA     PB8 / PB9   (Arduino D15 / D14)
 *
 * The core runs on the internal 16 MHz HSI oscillator (reset default).
 */
#ifndef BSP_H
#define BSP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BSP_SYSCLK_HZ     16000000UL
#define BSP_UART_BAUD     115200UL

/** NVIC priority of the UART RX interrupt. Must be numerically >=
 *  configMAX_SYSCALL_INTERRUPT_PRIORITY >> 4 to use FreeRTOS FromISR APIs. */
#define BSP_UART_IRQ_PRIO 6U

void bsp_init(void);

void bsp_led_set(bool on);
void bsp_led_toggle(void);

/** Blocking transmit. Not reentrant: the caller serialises access. */
void bsp_uart_write(const char *data, size_t len);

/** Enable the RX interrupt. Each received byte is passed to
 *  bsp_uart_rx_isr_hook() in interrupt context. */
void bsp_uart_enable_rx_irq(void);

/** Implemented by the application; called from the UART ISR. */
void bsp_uart_rx_isr_hook(uint8_t byte);

/** Blocking I2C write of @p len bytes to 7-bit address @p addr.
 *  @return 0 on success, negative on NACK / timeout / bus error. */
int bsp_i2c_write(uint8_t addr, const uint8_t *data, size_t len);

/** Blocking I2C read of @p len bytes from 7-bit address @p addr.
 *  @return 0 on success, negative on NACK / timeout / bus error. */
int bsp_i2c_read(uint8_t addr, uint8_t *data, size_t len);

#endif /* BSP_H */
