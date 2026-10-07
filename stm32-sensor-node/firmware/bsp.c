/**
 * @file bsp.c
 * @brief Register-level board support for NUCLEO-F446RE.
 */
#include "bsp.h"
#include "stm32f446_regs.h"

#define LED_PIN        5U   /* PA5 */
#define UART_TX_PIN    2U   /* PA2 */
#define UART_RX_PIN    3U   /* PA3 */
#define I2C_SCL_PIN    8U   /* PB8 */
#define I2C_SDA_PIN    9U   /* PB9 */

#define AF_USART2      7U
#define AF_I2C1        4U

#define APB1_CLK_HZ    BSP_SYSCLK_HZ   /* no prescaler at reset */
#define APB1_CLK_MHZ   (APB1_CLK_HZ / 1000000UL)
#define I2C_SPEED_HZ   100000UL        /* standard mode */

/* Upper bound for busy-waiting on a flag. One byte at 100 kHz takes
 * ~90 us; this is several milliseconds at 16 MHz. */
#define I2C_TIMEOUT_LOOPS 50000UL

/* ---------------------------------------------------------------------- */
/* GPIO helpers                                                           */
/* ---------------------------------------------------------------------- */

static void gpio_set_mode(gpio_regs_t *port, uint32_t pin, uint32_t mode)
{
    const uint32_t shift = pin * 2U;
    port->MODER = (port->MODER & ~(3UL << shift)) | (mode << shift);
}

static void gpio_set_af(gpio_regs_t *port, uint32_t pin, uint32_t af)
{
    const uint32_t idx = pin >> 3U;
    const uint32_t shift = (pin & 7U) * 4U;
    port->AFR[idx] = (port->AFR[idx] & ~(0xFUL << shift)) | (af << shift);
    gpio_set_mode(port, pin, GPIO_MODE_AF);
}

static void gpio_set_pullup(gpio_regs_t *port, uint32_t pin)
{
    const uint32_t shift = pin * 2U;
    port->PUPDR = (port->PUPDR & ~(3UL << shift)) | (GPIO_PULL_UP << shift);
}

static void gpio_set_speed_high(gpio_regs_t *port, uint32_t pin)
{
    const uint32_t shift = pin * 2U;
    port->OSPEEDR = (port->OSPEEDR & ~(3UL << shift)) | (GPIO_SPEED_HIGH << shift);
}

/* ---------------------------------------------------------------------- */
/* LED                                                                    */
/* ---------------------------------------------------------------------- */

void bsp_led_set(bool on)
{
    GPIOA->BSRR = on ? (1UL << LED_PIN) : (1UL << (LED_PIN + 16U));
}

void bsp_led_toggle(void)
{
    bsp_led_set((GPIOA->ODR & (1UL << LED_PIN)) == 0U);
}

/* ---------------------------------------------------------------------- */
/* UART (USART2 -> ST-LINK virtual COM port)                              */
/* ---------------------------------------------------------------------- */

static void uart_init(void)
{
    gpio_set_af(GPIOA, UART_TX_PIN, AF_USART2);
    gpio_set_af(GPIOA, UART_RX_PIN, AF_USART2);
    gpio_set_pullup(GPIOA, UART_RX_PIN);

    /* Oversampling by 16: BRR = fck / baud, rounded. */
    USART2->BRR = (APB1_CLK_HZ + (BSP_UART_BAUD / 2U)) / BSP_UART_BAUD;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void bsp_uart_write(const char *data, size_t len)
{
    for (size_t i = 0U; i < len; i++) {
        while ((USART2->SR & USART_SR_TXE) == 0U) {
            /* wait for empty transmit register */
        }
        USART2->DR = (uint8_t)data[i];
    }
    while ((USART2->SR & USART_SR_TC) == 0U) {
        /* wait until the last byte has left the shift register */
    }
}

void bsp_uart_enable_rx_irq(void)
{
    nvic_set_priority(USART2_IRQn, BSP_UART_IRQ_PRIO);
    USART2->CR1 |= USART_CR1_RXNEIE;
    nvic_enable_irq(USART2_IRQn);
}

void USART2_IRQHandler(void);
void USART2_IRQHandler(void)
{
    const uint32_t sr = USART2->SR;

    /* Reading DR clears RXNE and, after reading SR, also ORE. */
    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0U) {
        const uint8_t byte = (uint8_t)(USART2->DR & 0xFFU);
        if ((sr & USART_SR_RXNE) != 0U) {
            bsp_uart_rx_isr_hook(byte);
        }
    }
}

/* ---------------------------------------------------------------------- */
/* I2C1 (polling, 100 kHz)                                                */
/* ---------------------------------------------------------------------- */

static void i2c_configure(void)
{
    I2C1->CR1   = 0U;
    I2C1->CR2   = APB1_CLK_MHZ;                         /* FREQ field */
    I2C1->CCR   = APB1_CLK_HZ / (2U * I2C_SPEED_HZ);    /* Sm, Thigh = Tlow */
    I2C1->TRISE = APB1_CLK_MHZ + 1U;                    /* 1000 ns max rise */
    I2C1->CR1   = I2C_CR1_PE;
}

static void i2c_init(void)
{
    gpio_set_af(GPIOB, I2C_SCL_PIN, AF_I2C1);
    gpio_set_af(GPIOB, I2C_SDA_PIN, AF_I2C1);
    GPIOB->OTYPER |= (1UL << I2C_SCL_PIN) | (1UL << I2C_SDA_PIN);  /* open drain */
    gpio_set_pullup(GPIOB, I2C_SCL_PIN);   /* weak; use external 4.7k on the board */
    gpio_set_pullup(GPIOB, I2C_SDA_PIN);
    gpio_set_speed_high(GPIOB, I2C_SCL_PIN);
    gpio_set_speed_high(GPIOB, I2C_SDA_PIN);

    i2c_configure();
}

/** Reset the peripheral after a timeout or bus error. */
static void i2c_recover(void)
{
    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    i2c_configure();
}

static void i2c_stop(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
    for (uint32_t n = 0U; n < I2C_TIMEOUT_LOOPS; n++) {
        if ((I2C1->CR1 & I2C_CR1_STOP) == 0U) {
            return;
        }
    }
    i2c_recover();
}

/** Wait until all bits in @p mask are set in SR1. Fails early on NACK. */
static int i2c_wait_sr1(uint32_t mask)
{
    for (uint32_t n = 0U; n < I2C_TIMEOUT_LOOPS; n++) {
        const uint32_t sr1 = I2C1->SR1;
        if ((sr1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) != 0U) {
            I2C1->SR1 = ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);  /* rc_w0 */
            return -2;
        }
        if ((sr1 & mask) == mask) {
            return 0;
        }
    }
    return -1;
}

static int i2c_wait_not_busy(void)
{
    for (uint32_t n = 0U; n < I2C_TIMEOUT_LOOPS; n++) {
        if ((I2C1->SR2 & I2C_SR2_BUSY) == 0U) {
            return 0;
        }
    }
    i2c_recover();
    return -1;
}

static void i2c_clear_addr(void)
{
    volatile uint32_t tmp = I2C1->SR1;
    tmp = I2C1->SR2;
    (void)tmp;
}

/** START + address phase. On failure the bus is released. */
static int i2c_start(uint8_t addr, bool read)
{
    if (i2c_wait_not_busy() != 0) {
        return -1;
    }

    I2C1->CR1 |= I2C_CR1_START;
    if (i2c_wait_sr1(I2C_SR1_SB) != 0) {
        i2c_recover();
        return -1;
    }

    I2C1->DR = ((uint32_t)addr << 1U) | (read ? 1U : 0U);
    if (i2c_wait_sr1(I2C_SR1_ADDR) != 0) {
        i2c_stop();     /* address NACK: device absent or busy */
        return -2;
    }
    return 0;
}

int bsp_i2c_write(uint8_t addr, const uint8_t *data, size_t len)
{
    int rc = i2c_start(addr, false);
    if (rc != 0) {
        return rc;
    }
    i2c_clear_addr();

    for (size_t i = 0U; i < len; i++) {
        if (i2c_wait_sr1(I2C_SR1_TXE) != 0) {
            i2c_stop();
            return -3;
        }
        I2C1->DR = data[i];
    }

    rc = (i2c_wait_sr1(I2C_SR1_BTF) == 0) ? 0 : -3;
    i2c_stop();
    return rc;
}

int bsp_i2c_read(uint8_t addr, uint8_t *data, size_t len)
{
    if (len == 0U) {
        return 0;
    }

    I2C1->CR1 |= I2C_CR1_ACK;
    I2C1->CR1 &= ~I2C_CR1_POS;

    int rc = i2c_start(addr, true);
    if (rc != 0) {
        return rc;
    }

    /* Reception sequences follow RM0390, section "Master receiver". */
    if (len == 1U) {
        I2C1->CR1 &= ~I2C_CR1_ACK;
        i2c_clear_addr();
        I2C1->CR1 |= I2C_CR1_STOP;
        if (i2c_wait_sr1(I2C_SR1_RXNE) != 0) {
            i2c_recover();
            return -4;
        }
        data[0] = (uint8_t)I2C1->DR;
        return 0;
    }

    if (len == 2U) {
        I2C1->CR1 &= ~I2C_CR1_ACK;
        I2C1->CR1 |= I2C_CR1_POS;
        i2c_clear_addr();
        if (i2c_wait_sr1(I2C_SR1_BTF) != 0) {
            i2c_recover();
            return -4;
        }
        I2C1->CR1 |= I2C_CR1_STOP;
        data[0] = (uint8_t)I2C1->DR;
        data[1] = (uint8_t)I2C1->DR;
        I2C1->CR1 &= ~I2C_CR1_POS;
        return 0;
    }

    /* len >= 3 */
    i2c_clear_addr();
    size_t i = 0U;
    while ((len - i) > 3U) {
        if (i2c_wait_sr1(I2C_SR1_RXNE) != 0) {
            i2c_recover();
            return -4;
        }
        data[i++] = (uint8_t)I2C1->DR;
    }

    /* Last three bytes: N-2 in DR, N-1 in the shift register. */
    if (i2c_wait_sr1(I2C_SR1_BTF) != 0) {
        i2c_recover();
        return -4;
    }
    I2C1->CR1 &= ~I2C_CR1_ACK;
    data[i++] = (uint8_t)I2C1->DR;
    if (i2c_wait_sr1(I2C_SR1_BTF) != 0) {
        i2c_recover();
        return -4;
    }
    I2C1->CR1 |= I2C_CR1_STOP;
    data[i++] = (uint8_t)I2C1->DR;
    data[i]   = (uint8_t)I2C1->DR;
    return 0;
}

/* ---------------------------------------------------------------------- */

void bsp_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN | RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;   /* read back: let the clock enable settle */

    gpio_set_mode(GPIOA, LED_PIN, GPIO_MODE_OUTPUT);
    bsp_led_set(false);

    uart_init();
    i2c_init();
}
