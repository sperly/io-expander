/*
 * Modbus RTU slave protocol engine (transport independent, host-testable).
 *
 * Supported function codes: 01 read coils, 02 read discrete inputs,
 * 03 read holding registers, 04 read input registers, 05 write single coil,
 * 06 write single register, 15 write multiple coils, 16 write multiple
 * registers. Broadcast (address 0) is accepted for write functions and never
 * answered.
 */
#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include <stddef.h>
#include <stdint.h>

#define MODBUS_MAX_ADU 256u

enum {
    MB_OK = 0,
    MB_EX_ILLEGAL_FUNCTION = 1,
    MB_EX_ILLEGAL_ADDRESS = 2,
    MB_EX_ILLEGAL_VALUE = 3,
    MB_EX_DEVICE_FAILURE = 4,
};

enum { MB_TABLE_COILS = 0, MB_TABLE_DISCRETE = 1, MB_TABLE_HOLDING = 3, MB_TABLE_INPUT = 4 };

typedef struct {
    void *ctx;
    /* bits[] is packed LSB first, as on the wire */
    int (*read_bits)(void *ctx, int table, uint16_t addr, uint16_t count, uint8_t *bits);
    int (*write_coils)(void *ctx, uint16_t addr, uint16_t count, const uint8_t *bits);
    int (*read_regs)(void *ctx, int table, uint16_t addr, uint16_t count, uint16_t *regs);
    int (*write_regs)(void *ctx, uint16_t addr, uint16_t count, const uint16_t *regs);
} modbus_map_t;

typedef enum {
    MB_FRAME_IGNORED = 0, /* CRC error, too short, or for another address */
    MB_FRAME_HANDLED = 1, /* addressed to us (or broadcast) and processed */
} modbus_result_t;

/*
 * Process one received RTU frame. *resp_len is set to the response length
 * (0 = no response, e.g. broadcast). resp must hold MODBUS_MAX_ADU bytes.
 */
modbus_result_t modbus_process(const modbus_map_t *map, uint8_t my_addr, const uint8_t *req, size_t req_len,
                               uint8_t *resp, size_t *resp_len);

#endif
