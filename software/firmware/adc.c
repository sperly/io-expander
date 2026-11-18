/*
 * ADC: VIO monitor on PA0 (ADC_IN0) through the 100k / 22k divider, VDDA
 * measured with VREFINT (channel 13) and its factory calibration value.
 */
#include "board.h"
#include "gpio.h"
#include "platform.h"

#define VREFINT_CAL (*(const uint16_t *)0x1FFF75AAu) /* raw value at VDDA = 3.0 V, 30 C */

void adc_init(void)
{
    gpio_mode(PIN_VIO_ADC_PORT, PIN_VIO_ADC, GPIO_ANALOG);
    RCC->APBENR2 |= RCC_APBENR2_ADCEN;
    ADC1->CFGR2 = 1u << ADC_CFGR2_CKMODE_Pos; /* PCLK / 2 = 8 MHz */
    ADC1->CR = ADC_CR_ADVREGEN;
    sys_delay_us(30);
    ADC1->CR |= ADC_CR_ADCAL;
    while (ADC1->CR & ADC_CR_ADCAL) {
    }
    ADC->CCR |= ADC_CCR_VREFEN;
    ADC1->SMPR = 7u; /* 160.5 cycles: divider source impedance ~18k, plus VREFINT requirement */
    ADC1->ISR = ADC_ISR_ADRDY;
    ADC1->CR |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY)) {
    }
}

static uint16_t convert(unsigned ch)
{
    ADC1->CHSELR = 1u << ch;
    while (!(ADC1->ISR & ADC_ISR_CCRDY)) {
    }
    ADC1->ISR = ADC_ISR_CCRDY;
    uint32_t acc = 0;
    for (int i = 0; i < 8; i++) {
        ADC1->CR |= ADC_CR_ADSTART;
        while (!(ADC1->ISR & ADC_ISR_EOC)) {
        }
        acc += ADC1->DR;
    }
    return (uint16_t)(acc / 8u);
}

uint16_t adc_read_vio_mv(void)
{
    uint32_t vref = convert(ADC_CH_VREFINT);
    uint32_t raw = convert(ADC_CH_VIO);
    if (!vref)
        return 0;
    uint32_t vdda_mv = 3000u * VREFINT_CAL / vref;
    uint32_t pin_mv = raw * vdda_mv / 4095u;
    return (uint16_t)(pin_mv * VIO_DIV_NUM / VIO_DIV_DEN);
}
