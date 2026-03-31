#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ── Scheduler ──────────────────────────────────────────────────────────────*/
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1   /* CLZ instruction on M7  */
#define configUSE_TICKLESS_IDLE                 0
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    8
#define configMINIMAL_STACK_SIZE                256 /* words; M7 + FPU context */
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0

/* ── Clock ──────────────────────────────────────────────────────────────────*/
/* Set_arm_clock() in clockspeed.c may change the actual frequency; if your
 * application calls it, update this or derive it from F_CPU_ACTUAL at
 * runtime via vTaskSetTimeOutState / similar.  For SysTick calibration the
 * ROM already ran at 600 MHz, so this default is correct for a 600 MHz boot.*/
#define configCPU_CLOCK_HZ                      600000000UL

/* ── Memory ─────────────────────────────────────────────────────────────────*/
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configTOTAL_HEAP_SIZE                   (128 * 1024)  /* 128 KB      */

/* ── FPU (Cortex-M7 lazy stacking) ─────────────────────────────────────────*/
#define configUSE_TASK_FPU_SUPPORT              1

/* ── Interrupt priorities (IMXRT1062: 4 priority bits, stored in MSB) ───────
 *
 *  Numerically lower value = higher urgency.
 *  Kernel and syscall priorities use raw 8-bit register values; with 4-bit
 *  hardware the value must be left-aligned: value = (level << 4).
 *
 *  KERNEL_INTERRUPT_PRIORITY   : must be the lowest possible priority (0xF0).
 *  MAX_SYSCALL_INTERRUPT_PRIORITY : highest priority from which FreeRTOS API
 *                                   may be called (e.g. from an ISR).
 *                                   Any ISR at a numerically lower value
 *                                   (higher urgency) must NOT call the API.  */
#define configKERNEL_INTERRUPT_PRIORITY         ( 15 << 4 )   /* 0xF0 = 240 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (  5 << 4 )   /* 0x50 =  80 */

/* ── Cortex-M7 errata 837070 ────────────────────────────────────────────────
 *  "Increasing priority via BASEPRI does not take effect immediately."
 *  Safe to enable on all silicon revisions.                                 */
#define configENABLE_ERRATA_837070_WORKAROUND   1

/* ── Hooks ───────────────────────────────────────────────────────────────────*/
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2   /* method 2: watermark    */
#define configUSE_MALLOC_FAILED_HOOK            1

/* ── Debug / trace ──────────────────────────────────────────────────────────*/
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configGENERATE_RUN_TIME_STATS           0

/* ── Assert ─────────────────────────────────────────────────────────────────*/
#define configASSERT(x)  do { if (!(x)) { __asm__ volatile ("bkpt #0"); for(;;); } } while(0)

/* ── Optional API ───────────────────────────────────────────────────────────*/
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xTaskGetHandle                  0

/* ── Map FreeRTOS handler names to CMSIS / our vector table names ───────────*/
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
