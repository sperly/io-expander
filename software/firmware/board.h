/*
 * RS485_IO8 rev B pin map (8 isolated inputs, 8 outputs) - STM32G030F6P6 (TSSOP-20).
 * Must match hardware/mcu.kicad_sch.
 *
 *  pin  signal        function
 *   1   PB7           I2C1_SDA (AF6)   24AA02 EEPROM
 *   2   PC14          ADDR0 strap (input, external 10k pull-up, fitted = low)
 *   3   PC15          ADDR1 strap
 *   6   NRST          reset (also resets the MCP23S08)
 *   7   PA0           ADC_IN0  VIO monitor, VIO = Vadc * 122 / 22
 *   8   PA1           SPI1_SCK  (AF0)
 *   9   PA2           SPI1_MOSI (AF0)
 *  10   PA3           IN_IRQ  MCP23S08 INT (opto inputs), open-drain active low (EXTI3)
 *  11   PA4           DRV_nCS
 *  12   PA5           IN_nCS
 *  13   PA6           SPI1_MISO (AF0)
 *  14   PA7           DRV_nSLEEP (external 100k pull-down)
 *  15   PB0           SER_DE  MAX3160 DE485/T2IN (external 10k pull-down)
 *  16   PA9  (remap)  USART1_TX (AF1)  -> MAX3160 DI/T1IN
 *  17   PA10 (remap)  USART1_RX (AF1)  <- MAX3160 RO/R2OUT
 *  18   PA13          SWDIO
 *  19   PA14          SWCLK
 *  20   PB6           I2C1_SCL (AF6)
 *
 * Pads shared by several GPIOs on this package (PB3/4/5/6, PB7/PB8,
 * PB9/PC14, PA8/PB0/PB1/PB2, PA11/PA9, PA12/PA10) - the unused GPIOs keep
 * their reset state (analog mode). LSE is never enabled (PC14/PC15 used).
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32g0xx.h"

#define SYSCLK_HZ 16000000u /* HSI16, no PLL */

#define PIN_ADDR0_PORT GPIOC
#define PIN_ADDR0 14u
#define PIN_ADDR1_PORT GPIOC
#define PIN_ADDR1 15u

#define PIN_VIO_ADC_PORT GPIOA
#define PIN_VIO_ADC 0u
#define ADC_CH_VIO 0u
#define ADC_CH_VREFINT 13u

#define PIN_SCK 1u   /* GPIOA */
#define PIN_MOSI 2u  /* GPIOA */
#define PIN_MISO 6u  /* GPIOA */
#define PIN_IN_IRQ 3u
#define PIN_DRV_NCS 4u
#define PIN_IN_NCS 5u
#define PIN_DRV_NSLEEP 7u

#define PIN_SER_DE_PORT GPIOB
#define PIN_SER_DE 0u
#define PIN_UART_TX 9u  /* GPIOA, remapped to pad PA11 */
#define PIN_UART_RX 10u /* GPIOA, remapped to pad PA12 */

#define PIN_I2C_SCL 6u /* GPIOB */
#define PIN_I2C_SDA 7u /* GPIOB */

/* VIO divider 100k / 22k */
#define VIO_DIV_NUM 122u
#define VIO_DIV_DEN 22u

#endif
