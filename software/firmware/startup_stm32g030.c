/* Minimal C start-up for STM32G030 (Cortex-M0+). */
#include <stdint.h>

#include "stm32g0xx.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
int main(void);

void Reset_Handler(void);
void Default_Handler(void);

#define WEAK_ALIAS __attribute__((weak, alias("Default_Handler")))
void NMI_Handler(void) WEAK_ALIAS;
void HardFault_Handler(void) WEAK_ALIAS;
void SVC_Handler(void) WEAK_ALIAS;
void PendSV_Handler(void) WEAK_ALIAS;
void SysTick_Handler(void) WEAK_ALIAS;
void EXTI2_3_IRQHandler(void) WEAK_ALIAS;
void USART1_IRQHandler(void) WEAK_ALIAS;

typedef void (*vector_t)(void);

/* 16 system exceptions + 32 peripheral interrupts. All peripheral vectors
 * default to Default_Handler; the later designators override that range. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
__attribute__((section(".isr_vector"), used)) const vector_t vector_table[16 + 32] = {
    [0] = (vector_t)(uintptr_t)&_estack,
    [1] = Reset_Handler,
    [2] = NMI_Handler,
    [3] = HardFault_Handler,
    [11] = SVC_Handler,
    [14] = PendSV_Handler,
    [15] = SysTick_Handler,
    [16 ... 16 + 31] = Default_Handler,
    [16 + EXTI2_3_IRQn] = EXTI2_3_IRQHandler,
    [16 + USART1_IRQn] = USART1_IRQHandler,
};
#pragma GCC diagnostic pop

void Reset_Handler(void)
{
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata)
        *dst++ = *src++;
    for (dst = &_sbss; dst < &_ebss;)
        *dst++ = 0;
    main();
    for (;;) {
    }
}

void Default_Handler(void)
{
    for (;;) { /* the watchdog resets the MCU */
    }
}
