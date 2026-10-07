/**
 * @file startup_stm32f446.c
 * @brief Vector table and reset handler for STM32F446, written in C.
 *
 * Reset_Handler copies .data from flash to RAM, zeroes .bss, enables the
 * FPU and calls main(). Every unused interrupt points to Default_Handler,
 * so an unexpected IRQ halts in a known place instead of jumping to 0.
 */
#include <stdint.h>

#include "stm32f446_regs.h"

/* Symbols from the linker script. */
extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* Core exceptions. Weak, so the RTOS port and the BSP can override them. */
void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__((weak, alias("Default_Handler")));

/* Peripheral interrupts used by this project. */
void USART2_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));

#define CORE_VECTORS   16U
#define DEVICE_VECTORS 97U   /* IRQ 0 .. 96 on STM32F446 */

typedef void (*vector_t)(void);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"

__attribute__((section(".isr_vector"), used))
const vector_t g_vector_table[CORE_VECTORS + DEVICE_VECTORS] = {
    [0]  = (vector_t)(uintptr_t)&_estack,
    [1]  = Reset_Handler,
    [2]  = NMI_Handler,
    [3]  = HardFault_Handler,
    [4]  = MemManage_Handler,
    [5]  = BusFault_Handler,
    [6]  = UsageFault_Handler,
    /* 7..10 reserved */
    [11] = SVC_Handler,
    [12] = DebugMon_Handler,
    /* 13 reserved */
    [14] = PendSV_Handler,
    [15] = SysTick_Handler,

    /* All device interrupts default to Default_Handler ... */
    [CORE_VECTORS ... (CORE_VECTORS + DEVICE_VECTORS - 1U)] = Default_Handler,
    /* ... except the ones we use. */
    [CORE_VECTORS + USART2_IRQn] = USART2_IRQHandler,
};

#pragma GCC diagnostic pop

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {
        *dst++ = *src++;
    }
    for (dst = &_sbss; dst < &_ebss; dst++) {
        *dst = 0U;
    }

    /* Full access to coprocessors CP10/CP11 (single-precision FPU). */
    SCB_CPACR |= (0xFUL << 20U);
    __asm volatile ("dsb\n\tisb" ::: "memory");

    (void)main();

    for (;;) {
        /* main() must never return */
    }
}

void Default_Handler(void)
{
    __asm volatile ("cpsid i" ::: "memory");
    for (;;) {
        /* Unexpected interrupt or fault: halt here for the debugger. */
    }
}
