#ifndef GPIO_H
#define GPIO_H

#include "stm32g0xx.h"

enum { GPIO_IN = 0u, GPIO_OUT = 1u, GPIO_AF = 2u, GPIO_ANALOG = 3u };

static inline void gpio_mode(GPIO_TypeDef *p, unsigned pin, unsigned mode)
{
    p->MODER = (p->MODER & ~(3u << (2u * pin))) | (mode << (2u * pin));
}

static inline void gpio_af(GPIO_TypeDef *p, unsigned pin, unsigned af)
{
    volatile uint32_t *r = &p->AFR[pin >> 3];
    unsigned sh = (pin & 7u) * 4u;
    *r = (*r & ~(0xFu << sh)) | (af << sh);
    gpio_mode(p, pin, GPIO_AF);
}

static inline void gpio_open_drain(GPIO_TypeDef *p, unsigned pin) { p->OTYPER |= 1u << pin; }
static inline void gpio_speed_high(GPIO_TypeDef *p, unsigned pin) { p->OSPEEDR |= 2u << (2u * pin); }
static inline void gpio_set(GPIO_TypeDef *p, unsigned pin) { p->BSRR = 1u << pin; }
static inline void gpio_clr(GPIO_TypeDef *p, unsigned pin) { p->BRR = 1u << pin; }
static inline unsigned gpio_read(GPIO_TypeDef *p, unsigned pin) { return (p->IDR >> pin) & 1u; }

#endif
