/* Low-level peripheral drivers for the STM32G030 (CMSIS register access). */
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* system.c */
void sys_init(void);              /* clocks, SysTick 1 ms, GPIO safe states */
uint32_t sys_millis(void);
void sys_delay_ms(uint32_t ms);
void sys_delay_us(uint32_t us);
void sys_watchdog_start(void);    /* IWDG, about 1 s */
void sys_watchdog_kick(void);
void sys_reboot(void);
uint8_t sys_read_straps(void);    /* bit0 = ADDR0 fitted, bit1 = ADDR1 fitted */
void sys_drv_sleep(bool sleep);   /* DRV8912 nSLEEP */
bool sys_take_input_irq(void);    /* MCP23S08 INT seen since last call */

/* uart.c - USART1 + MAX3160 DE, Modbus RTU framing by receiver timeout */
void uart_init(uint32_t baud, uint8_t parity /* CFG_PARITY_x */);
/* returns length of a complete received frame (0 = none); frame stays valid
 * until uart_release_frame() */
size_t uart_frame(const uint8_t **data);
void uart_release_frame(void);
void uart_send(const uint8_t *data, size_t len); /* non-blocking */
bool uart_tx_busy(void);

/* spi.c - SPI1 shared by DRV8912 (mode 1) and MCP23S08 (mode 0) */
typedef enum { SPI_DEV_DRV = 0, SPI_DEV_EXP = 1 } spi_dev_t;
void spi_init(void);
void spi_transfer(spi_dev_t dev, const uint8_t *tx, uint8_t *rx, size_t len);

/* i2c.c - I2C1 master, 100 kHz, polled */
void i2c_init(void);
int i2c_write(uint8_t addr7, const uint8_t *data, size_t len);
int i2c_write_read(uint8_t addr7, const uint8_t *wr, size_t wlen, uint8_t *rd, size_t rlen);

/* adc.c */
void adc_init(void);
uint16_t adc_read_vio_mv(void);

#endif
