#include "imxrt.h"
#include "uart_serial.h"
#include <FreeRTOS.h>
#include <task.h>

static constexpr uint32_t LED_BIT = 1u << 3;

static void task_blink(void *)
{
    for (;;) {
        GPIO7_DR_TOGGLE = LED_BIT;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void task_uart(void *)
{
    for (;;) {
        if (uart_serial_rxready(&uart_serial1))
            uart_serial_putc(&uart_serial1, (char)uart_serial_getc(&uart_serial1));
        vTaskDelay(1);
    }
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char *)
{
    __asm__ volatile("bkpt #0");
    for (;;) __asm__ volatile("wfi");
}

extern "C" void vApplicationMallocFailedHook(void)
{
    __asm__ volatile("bkpt #0");
    for (;;) __asm__ volatile("wfi");
}

extern "C" int main()
{
    // LED: Teensy pin 13 → GPIO_B0_03 → GPIO7 fast mirror
    CCM_CCGR0 |= CCM_CCGR0_GPIO2(CCM_CCGR_ON);
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 5;
    GPIO7_GDIR |= LED_BIT;

    uart_serial_init(&uart_serial1, 921600);
    uart_serial_puts(&uart_serial1, "NautSyn FreeRTOS booting\r\n");

    xTaskCreate(task_blink, "blink", 256, nullptr, 1, nullptr);
    xTaskCreate(task_uart,  "uart",  256, nullptr, 1, nullptr);

    vTaskStartScheduler();
    for (;;) __asm__ volatile("wfi");
}
