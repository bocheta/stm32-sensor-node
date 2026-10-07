/**
 * @file stm32f446_regs.h
 * @brief Minimal register definitions for the STM32F446 peripherals used
 *        by this project (RCC, GPIO, USART, I2C, NVIC, SCB).
 *
 * Written from RM0390 instead of pulling in the full vendor HAL, so every
 * register access in the BSP is explicit and easy to review.
 */
#ifndef STM32F446_REGS_H
#define STM32F446_REGS_H

#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ---- Base addresses ----------------------------------------------------- */
#define PERIPH_BASE        0x40000000UL
#define APB1_BASE          (PERIPH_BASE + 0x00000000UL)
#define AHB1_BASE          (PERIPH_BASE + 0x00020000UL)

#define USART2_BASE        (APB1_BASE + 0x4400UL)
#define I2C1_BASE          (APB1_BASE + 0x5400UL)
#define GPIOA_BASE         (AHB1_BASE + 0x0000UL)
#define GPIOB_BASE         (AHB1_BASE + 0x0400UL)
#define RCC_BASE           (AHB1_BASE + 0x3800UL)

/* ---- RCC ------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t CR, PLLCFGR, CFGR, CIR;
    volatile uint32_t AHB1RSTR, AHB2RSTR, AHB3RSTR, RESERVED0;
    volatile uint32_t APB1RSTR, APB2RSTR, RESERVED1[2];
    volatile uint32_t AHB1ENR, AHB2ENR, AHB3ENR, RESERVED2;
    volatile uint32_t APB1ENR, APB2ENR;
} rcc_regs_t;
#define RCC ((rcc_regs_t *)RCC_BASE)

#define RCC_AHB1ENR_GPIOAEN  (1UL << 0)
#define RCC_AHB1ENR_GPIOBEN  (1UL << 1)
#define RCC_APB1ENR_USART2EN (1UL << 17)
#define RCC_APB1ENR_I2C1EN   (1UL << 21)
#define RCC_APB1RSTR_I2C1RST (1UL << 21)

/* ---- GPIO ----------------------------------------------------------------- */
typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR;
    volatile uint32_t IDR, ODR, BSRR, LCKR;
    volatile uint32_t AFR[2];
} gpio_regs_t;
#define GPIOA ((gpio_regs_t *)GPIOA_BASE)
#define GPIOB ((gpio_regs_t *)GPIOB_BASE)

#define GPIO_MODE_INPUT   0UL
#define GPIO_MODE_OUTPUT  1UL
#define GPIO_MODE_AF      2UL
#define GPIO_PULL_UP      1UL
#define GPIO_SPEED_HIGH   2UL

/* ---- USART ---------------------------------------------------------------- */
typedef struct {
    volatile uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
} usart_regs_t;
#define USART2 ((usart_regs_t *)USART2_BASE)

#define USART_SR_ORE    (1UL << 3)
#define USART_SR_RXNE   (1UL << 5)
#define USART_SR_TC     (1UL << 6)
#define USART_SR_TXE    (1UL << 7)
#define USART_CR1_RE    (1UL << 2)
#define USART_CR1_TE    (1UL << 3)
#define USART_CR1_RXNEIE (1UL << 5)
#define USART_CR1_UE    (1UL << 13)

/* ---- I2C ------------------------------------------------------------------ */
typedef struct {
    volatile uint32_t CR1, CR2, OAR1, OAR2, DR, SR1, SR2, CCR, TRISE, FLTR;
} i2c_regs_t;
#define I2C1 ((i2c_regs_t *)I2C1_BASE)

#define I2C_CR1_PE      (1UL << 0)
#define I2C_CR1_START   (1UL << 8)
#define I2C_CR1_STOP    (1UL << 9)
#define I2C_CR1_ACK     (1UL << 10)
#define I2C_CR1_POS     (1UL << 11)
#define I2C_CR1_SWRST   (1UL << 15)

#define I2C_SR1_SB      (1UL << 0)
#define I2C_SR1_ADDR    (1UL << 1)
#define I2C_SR1_BTF     (1UL << 2)
#define I2C_SR1_RXNE    (1UL << 6)
#define I2C_SR1_TXE     (1UL << 7)
#define I2C_SR1_BERR    (1UL << 8)
#define I2C_SR1_ARLO    (1UL << 9)
#define I2C_SR1_AF      (1UL << 10)

#define I2C_SR2_BUSY    (1UL << 1)

/* ---- Cortex-M4 core ------------------------------------------------------- */
#define NVIC_ISER_BASE  0xE000E100UL
#define NVIC_IPR_BASE   0xE000E400UL
#define SCB_CPACR       REG32(0xE000ED88UL)

#define NVIC_PRIO_BITS  4U

#define USART2_IRQn     38U

static inline void nvic_set_priority(uint32_t irqn, uint32_t prio)
{
    volatile uint8_t *ipr = (volatile uint8_t *)NVIC_IPR_BASE;
    ipr[irqn] = (uint8_t)((prio << (8U - NVIC_PRIO_BITS)) & 0xFFU);
}

static inline void nvic_enable_irq(uint32_t irqn)
{
    REG32(NVIC_ISER_BASE + ((irqn >> 5U) * 4U)) = 1UL << (irqn & 0x1FU);
}

#endif /* STM32F446_REGS_H */
