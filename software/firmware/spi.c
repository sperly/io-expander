/*
 * SPI1 master, 1 MHz, 8-bit frames, software chip selects.
 * DRV8912: data captured on the falling SCLK edge, shifted out on the rising
 * edge -> CPOL 0 / CPHA 1 (mode 1). MCP23S08: mode 0.
 * The mode is changed only with SPE = 0 and both chip selects high.
 */
#include "board.h"
#include "gpio.h"
#include "platform.h"

void spi_init(void)
{
    RCC->APBENR2 |= RCC_APBENR2_SPI1EN;
    gpio_af(GPIOA, PIN_SCK, 0);
    gpio_af(GPIOA, PIN_MOSI, 0);
    gpio_af(GPIOA, PIN_MISO, 0);
    SPI1->CR1 = 0;
    /* 8-bit data, RXNE at 8 bits */
    SPI1->CR2 = (7u << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;
}

static void configure(spi_dev_t dev)
{
    uint32_t cr1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (3u << SPI_CR1_BR_Pos); /* 16 MHz / 16 */
    if (dev == SPI_DEV_DRV)
        cr1 |= SPI_CR1_CPHA;
    if ((SPI1->CR1 & ~SPI_CR1_SPE) != cr1 || !(SPI1->CR1 & SPI_CR1_SPE)) {
        SPI1->CR1 &= ~SPI_CR1_SPE; /* clock phase may only change while disabled */
        SPI1->CR1 = cr1;
        SPI1->CR1 = cr1 | SPI_CR1_SPE;
    }
}

void spi_transfer(spi_dev_t dev, const uint8_t *tx, uint8_t *rx, size_t len)
{
    unsigned cs = dev == SPI_DEV_DRV ? PIN_DRV_NCS : PIN_IN_NCS;
    configure(dev);
    gpio_clr(GPIOA, cs);
    for (size_t i = 0; i < len; i++) {
        while (!(SPI1->SR & SPI_SR_TXE)) {
        }
        *(volatile uint8_t *)&SPI1->DR = tx ? tx[i] : 0u;
        while (!(SPI1->SR & SPI_SR_RXNE)) {
        }
        uint8_t b = *(volatile uint8_t *)&SPI1->DR;
        if (rx)
            rx[i] = b;
    }
    while (SPI1->SR & SPI_SR_BSY) {
    }
    gpio_set(GPIOA, cs);
}
