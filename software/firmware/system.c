#include "board.h"
#include "gpio.h"
#include "platform.h"

static volatile uint32_t s_ms;
static volatile bool s_input_irq;

void SysTick_Handler(void) { s_ms++; }

void EXTI2_3_IRQHandler(void)
{
    if (EXTI->FPR1 & (1u << PIN_IN_IRQ)) {
        EXTI->FPR1 = 1u << PIN_IN_IRQ;
        s_input_irq = true;
    }
}

void sys_init(void)
{
    /* SYSCLK = HSI16 (reset default, HSIDIV = 1); flash 0 wait states is valid up to 24 MHz */
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN | RCC_IOPENR_GPIOCEN;
    RCC->APBENR2 |= RCC_APBENR2_SYSCFGEN;

    /* safe output states before anything else: chip selects high, driver asleep, transmitter off */
    gpio_set(GPIOA, PIN_DRV_NCS);
    gpio_set(GPIOA, PIN_IN_NCS);
    gpio_clr(GPIOA, PIN_DRV_NSLEEP);
    gpio_clr(PIN_SER_DE_PORT, PIN_SER_DE);
    gpio_mode(GPIOA, PIN_DRV_NCS, GPIO_OUT);
    gpio_mode(GPIOA, PIN_IN_NCS, GPIO_OUT);
    gpio_mode(GPIOA, PIN_DRV_NSLEEP, GPIO_OUT);
    gpio_mode(PIN_SER_DE_PORT, PIN_SER_DE, GPIO_OUT);

    /* address straps: external pull-ups */
    gpio_mode(PIN_ADDR0_PORT, PIN_ADDR0, GPIO_IN);
    gpio_mode(PIN_ADDR1_PORT, PIN_ADDR1, GPIO_IN);

    /* expander interrupt on PA3, falling edge */
    gpio_mode(GPIOA, PIN_IN_IRQ, GPIO_IN);
    EXTI->EXTICR[0] &= ~(0xFFu << 24); /* line 3 -> port A */
    EXTI->FTSR1 |= 1u << PIN_IN_IRQ;
    EXTI->IMR1 |= 1u << PIN_IN_IRQ;
    NVIC_SetPriority(EXTI2_3_IRQn, 2);
    NVIC_EnableIRQ(EXTI2_3_IRQn);

    SysTick_Config(SYSCLK_HZ / 1000u);
    NVIC_SetPriority(SysTick_IRQn, 1);
}

uint32_t sys_millis(void) { return s_ms; }

void sys_delay_ms(uint32_t ms)
{
    uint32_t t0 = s_ms;
    while ((uint32_t)(s_ms - t0) <= ms) {
    }
}

void sys_delay_us(uint32_t us)
{
    /* SysTick counts down from LOAD at SYSCLK */
    uint32_t ticks = us * (SYSCLK_HZ / 1000000u), last = SysTick->VAL, elapsed = 0;
    while (elapsed < ticks) {
        uint32_t now = SysTick->VAL;
        elapsed += (last >= now) ? last - now : last + (SysTick->LOAD + 1u) - now;
        last = now;
    }
}

void sys_watchdog_start(void)
{
    IWDG->KR = 0xCCCCu; /* start (LSI ~32 kHz) */
    IWDG->KR = 0x5555u; /* unlock */
    IWDG->PR = 3u;      /* /32 -> ~1 kHz */
    IWDG->RLR = 1000u;  /* ~1 s */
    while (IWDG->SR) {
    }
    IWDG->KR = 0xAAAAu;
}

void sys_watchdog_kick(void) { IWDG->KR = 0xAAAAu; }

void sys_reboot(void) { NVIC_SystemReset(); }

uint8_t sys_read_straps(void)
{
    /* fitted shunt pulls the line low */
    uint8_t v = 0;
    if (!gpio_read(PIN_ADDR0_PORT, PIN_ADDR0))
        v |= 1u;
    if (!gpio_read(PIN_ADDR1_PORT, PIN_ADDR1))
        v |= 2u;
    return v;
}

void sys_drv_sleep(bool sleep)
{
    if (sleep)
        gpio_clr(GPIOA, PIN_DRV_NSLEEP);
    else
        gpio_set(GPIOA, PIN_DRV_NSLEEP);
}

bool sys_take_input_irq(void)
{
    __disable_irq();
    bool v = s_input_irq;
    s_input_irq = false;
    __enable_irq();
    return v;
}
