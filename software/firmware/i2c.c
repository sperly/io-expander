/* I2C1 master on PB6 (SCL) / PB7 (SDA), 100 kHz, polled with timeouts. */
#include "board.h"
#include "gpio.h"
#include "platform.h"

/* RM0454 timing example for I2CCLK = 16 MHz, standard mode 100 kHz:
 * PRESC 3, SCLDEL 4, SDADEL 2, SCLH 0x0F, SCLL 0x13 */
#define I2C_TIMING_100K 0x30420F13u
#define TIMEOUT_MS 5u

void i2c_init(void)
{
    RCC->APBENR1 |= RCC_APBENR1_I2C1EN;
    gpio_open_drain(GPIOB, PIN_I2C_SCL);
    gpio_open_drain(GPIOB, PIN_I2C_SDA);
    gpio_af(GPIOB, PIN_I2C_SCL, 6);
    gpio_af(GPIOB, PIN_I2C_SDA, 6);
    I2C1->CR1 = 0;
    I2C1->TIMINGR = I2C_TIMING_100K;
    I2C1->CR1 = I2C_CR1_PE;
}

static int wait_flag(uint32_t flag)
{
    uint32_t t0 = sys_millis();
    while (!(I2C1->ISR & flag)) {
        if (I2C1->ISR & I2C_ISR_NACKF) {
            I2C1->ICR = I2C_ICR_NACKCF;
            return -1;
        }
        if ((uint32_t)(sys_millis() - t0) > TIMEOUT_MS)
            return -2;
    }
    return 0;
}

static int stop(void)
{
    int r = wait_flag(I2C_ISR_STOPF);
    I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF;
    I2C1->CR2 = 0;
    return r;
}

static int recover(int err)
{
    /* reset the peripheral state machine after an error */
    I2C1->CR1 &= ~I2C_CR1_PE;
    while (I2C1->CR1 & I2C_CR1_PE) {
    }
    I2C1->CR1 |= I2C_CR1_PE;
    return err;
}

int i2c_write(uint8_t addr7, const uint8_t *data, size_t len)
{
    if (len == 0 || len > 255)
        return -3;
    I2C1->CR2 = ((uint32_t)addr7 << 1) | (len << I2C_CR2_NBYTES_Pos) | I2C_CR2_AUTOEND | I2C_CR2_START;
    for (size_t i = 0; i < len; i++) {
        int r = wait_flag(I2C_ISR_TXIS);
        if (r)
            return recover(r);
        I2C1->TXDR = data[i];
    }
    return stop();
}

int i2c_write_read(uint8_t addr7, const uint8_t *wr, size_t wlen, uint8_t *rd, size_t rlen)
{
    if (wlen == 0 || wlen > 255 || rlen == 0 || rlen > 255)
        return -3;
    I2C1->CR2 = ((uint32_t)addr7 << 1) | (wlen << I2C_CR2_NBYTES_Pos) | I2C_CR2_START;
    for (size_t i = 0; i < wlen; i++) {
        int r = wait_flag(I2C_ISR_TXIS);
        if (r)
            return recover(r);
        I2C1->TXDR = wr[i];
    }
    int r = wait_flag(I2C_ISR_TC);
    if (r)
        return recover(r);
    I2C1->CR2 = ((uint32_t)addr7 << 1) | I2C_CR2_RD_WRN | (rlen << I2C_CR2_NBYTES_Pos) | I2C_CR2_AUTOEND |
                I2C_CR2_START;
    for (size_t i = 0; i < rlen; i++) {
        r = wait_flag(I2C_ISR_RXNE);
        if (r)
            return recover(r);
        rd[i] = (uint8_t)I2C1->RXDR;
    }
    return stop();
}
