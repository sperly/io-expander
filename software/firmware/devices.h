/* External devices on SPI1 / I2C1. */
#ifndef DEVICES_H
#define DEVICES_H

#include <stdbool.h>
#include <stdint.h>

#include "io_model.h"

/* ---- DRV8912-Q1 (SPI mode 1, 16-bit frames) */
uint8_t drv_read(uint8_t addr, uint8_t *status);           /* returns register value */
void drv_write(uint8_t addr, uint8_t val);
bool drv_start(void);                                       /* wake + static set-up, true if ID = DRV8912 */
void drv_apply(const io_config_t *io, bool enabled);        /* write the channel register image */
void drv_clear_faults(void);

/* ---- MCP23S08 (SPI mode 0, hardware address 0) */
void exp_init(void);
uint8_t exp_read_levels(void);                              /* bit n = IN(n+1) opto energised */
void exp_set_irq_mask(uint8_t mask);                        /* interrupt-on-change for these inputs */

/* ---- 24AA02 EEPROM (I2C 0x50, 8-byte pages, 5 ms write cycle) */
int eeprom_read(void *ctx, uint16_t offset, uint8_t *buf, uint16_t len);
int eeprom_write(void *ctx, uint16_t offset, const uint8_t *buf, uint16_t len);

#endif
