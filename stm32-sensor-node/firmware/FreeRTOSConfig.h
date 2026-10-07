/**
 * @file FreeRTOSConfig.h
 * @brief FreeRTOS configuration for the STM32F446 sensor node.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ---- Scheduler ------------------------------------------------------------ */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      16000000UL
#define configTICK_RATE_HZ                      1000U
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128U
#define configMAX_TASK_NAME_LEN                 12
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TIME_SLICING                  1

/* ---- Synchronisation ------------------------------------------------------ */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES            0
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_STREAM_BUFFERS                1

/* ---- Memory --------------------------------------------------------------- */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   (16U * 1024U)
#define configAPPLICATION_ALLOCATED_HEAP        0

/* ---- Hooks and debugging -------------------------------------------------- */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_TRACE_FACILITY                0
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0

/* Halt on assertion so the debugger shows where it failed. */
#define configASSERT(x)                                   \
    do {                                                  \
        if ((x) == 0) {                                   \
            __asm volatile ("cpsid i" ::: "memory");      \
            for (;;) { }                                  \
        }                                                 \
    } while (0)

/* ---- Optional API functions ----------------------------------------------- */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetSchedulerState          1

/* ---- Cortex-M interrupt priorities ---------------------------------------- */
/* STM32F4 implements 4 priority bits (0 = highest, 15 = lowest). */
#define configPRIO_BITS                         4

/* Kernel runs at the lowest priority. */
#define configKERNEL_INTERRUPT_PRIORITY         (15U << (8U - configPRIO_BITS))

/* ISRs with priority 5..15 may call FreeRTOS "FromISR" APIs.
 * ISRs with priority 0..4 are never masked by the kernel. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5U << (8U - configPRIO_BITS))

/* ---- Map port handlers to the names in the vector table ------------------- */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
