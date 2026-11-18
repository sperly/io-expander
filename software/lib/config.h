/*
 * Persistent configuration stored in the 24AA02 EEPROM (256 bytes).
 *
 * Two slots at offsets 0 and 128. Each slot:
 *   [0]  'I'  [1] '8'  [2] format version  [3] sequence  [4] payload length
 *   [5 .. 5+len)  payload
 *   [5+len .. +2) CRC-16/MODBUS over bytes 0 .. 5+len-1 (low byte first)
 * A save goes to the slot that does not hold the newest valid copy, so a
 * power failure during a write always leaves the previous copy intact.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "io_model.h"

#define CFG_SLOT_SIZE 128u
#define CFG_SLOT_COUNT 2u
#define CFG_VERSION 2u
#define CFG_DEBOUNCE_MAX_MS 250u

enum { CFG_BAUD_9600 = 0, CFG_BAUD_19200, CFG_BAUD_38400, CFG_BAUD_57600, CFG_BAUD_115200, CFG_BAUD_COUNT };
enum { CFG_PARITY_NONE = 0, CFG_PARITY_EVEN, CFG_PARITY_ODD, CFG_PARITY_COUNT };

typedef struct {
    uint8_t base_addr;   /* Modbus address = base_addr + strap offset (0..3) */
    uint8_t baud;        /* CFG_BAUD_x */
    uint8_t parity;      /* CFG_PARITY_x */
    uint16_t timeout_ms; /* comm watchdog, 0 = off */
    io_config_t io;      /* power-on state of the outputs */
    uint8_t in_invert;   /* bit n set: input n+1 reads 1 when its opto is off (NC contacts) */
    uint8_t in_debounce_ms; /* input debounce time, 0..250 ms */
} cfg_t;

typedef struct {
    void *ctx;
    int (*read)(void *ctx, uint16_t offset, uint8_t *buf, uint16_t len);        /* 0 = ok */
    int (*write)(void *ctx, uint16_t offset, const uint8_t *buf, uint16_t len); /* 0 = ok */
} cfg_storage_t;

void cfg_defaults(cfg_t *c);
bool cfg_valid(const cfg_t *c);
uint32_t cfg_baud_rate(uint8_t code);

/* Serialise into a slot image (CFG_SLOT_SIZE bytes, unused bytes 0xFF). */
void cfg_encode(const cfg_t *c, uint8_t seq, uint8_t slot[CFG_SLOT_SIZE]);
/* Returns true and fills c / seq if the slot image is valid. */
bool cfg_decode(const uint8_t slot[CFG_SLOT_SIZE], cfg_t *c, uint8_t *seq);

/* Load the newest valid copy. Returns false (and loads defaults) if none. */
bool cfg_load(const cfg_storage_t *st, cfg_t *c);
/* Save into the older slot. Returns 0 on success. */
int cfg_save(const cfg_storage_t *st, const cfg_t *c);

#endif
