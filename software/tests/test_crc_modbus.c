#include <string.h>

#include "crc16.h"
#include "modbus_rtu.h"
#include "unit.h"

/* tiny map: 10 holding registers, 4 coils */
static uint16_t hr[10];
static uint8_t coils;

static int rb(void *ctx, int table, uint16_t a, uint16_t n, uint8_t *bits)
{
    if (a + n > 4)
        return MB_EX_ILLEGAL_ADDRESS;
    for (uint16_t i = 0; i < n; i++)
        if ((coils >> (a + i)) & 1)
            bits[i / 8] |= (uint8_t)(1u << (i % 8));
    return 0;
}

static int wc(void *ctx, uint16_t a, uint16_t n, const uint8_t *bits)
{
    if (a + n > 4)
        return MB_EX_ILLEGAL_ADDRESS;
    for (uint16_t i = 0; i < n; i++) {
        coils &= (uint8_t)~(1u << (a + i));
        coils |= (uint8_t)(((bits[i / 8] >> (i % 8)) & 1u) << (a + i));
    }
    return 0;
}

static int rr(void *ctx, int table, uint16_t a, uint16_t n, uint16_t *regs)
{
    if (a + n > 10)
        return MB_EX_ILLEGAL_ADDRESS;
    memcpy(regs, hr + a, n * 2u);
    return 0;
}

static int wr(void *ctx, uint16_t a, uint16_t n, const uint16_t *regs)
{
    if (a + n > 10)
        return MB_EX_ILLEGAL_ADDRESS;
    memcpy(hr + a, regs, n * 2u);
    return 0;
}

static const modbus_map_t map = {NULL, rb, wc, rr, wr};

static size_t frame(uint8_t *buf, const uint8_t *pdu, size_t n)
{
    memcpy(buf, pdu, n);
    uint16_t c = crc16_modbus(buf, n);
    buf[n] = (uint8_t)c;
    buf[n + 1] = (uint8_t)(c >> 8);
    return n + 2;
}

void test_crc_modbus(void)
{
    /* reference vector: "123456789" -> 0x4B37 (CRC-16/MODBUS check value) */
    CHECK_EQ(crc16_modbus((const uint8_t *)"123456789", 9), 0x4B37);
    /* classic example: 01 03 00 00 00 0A -> CRC C5 CD */
    const uint8_t ex[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint16_t c = crc16_modbus(ex, sizeof ex);
    CHECK_EQ(c & 0xFF, 0xC5);
    CHECK_EQ(c >> 8, 0xCD);

    uint8_t req[64], resp[MODBUS_MAX_ADU];
    size_t n, rn;
    for (int i = 0; i < 10; i++)
        hr[i] = (uint16_t)(0x1000 + i);

    /* read 3 holding registers from address 2 */
    n = frame(req, (const uint8_t[]){0x05, 0x03, 0x00, 0x02, 0x00, 0x03}, 6);
    CHECK_EQ(modbus_process(&map, 5, req, n, resp, &rn), MB_FRAME_HANDLED);
    CHECK_EQ(rn, 3 + 6 + 2);
    CHECK_EQ(resp[2], 6);
    CHECK_EQ(resp[3], 0x10);
    CHECK_EQ(resp[4], 0x02);
    CHECK_EQ(resp[8], 0x04);
    CHECK_EQ(crc16_modbus(resp, rn), 0); /* CRC over frame + CRC = 0 */

    /* other address: ignored */
    n = frame(req, (const uint8_t[]){0x06, 0x03, 0x00, 0x00, 0x00, 0x01}, 6);
    CHECK_EQ(modbus_process(&map, 5, req, n, resp, &rn), MB_FRAME_IGNORED);
    CHECK_EQ(rn, 0);

    /* bad CRC: ignored */
    n = frame(req, (const uint8_t[]){0x05, 0x03, 0x00, 0x00, 0x00, 0x01}, 6);
    req[n - 1] ^= 1;
    CHECK_EQ(modbus_process(&map, 5, req, n, resp, &rn), MB_FRAME_IGNORED);

    /* out of range -> exception 02 */
    n = frame(req, (const uint8_t[]){0x05, 0x03, 0x00, 0x09, 0x00, 0x02}, 6);
    modbus_process(&map, 5, req, n, resp, &rn);
    CHECK_EQ(rn, 5);
    CHECK_EQ(resp[1], 0x83);
    CHECK_EQ(resp[2], 0x02);

    /* unknown function -> exception 01 */
    n = frame(req, (const uint8_t[]){0x05, 0x2B, 0x0E, 0x01}, 4);
    modbus_process(&map, 5, req, n, resp, &rn);
    CHECK_EQ(resp[1], 0xAB);
    CHECK_EQ(resp[2], 0x01);

    /* write single register, echo */
    n = frame(req, (const uint8_t[]){0x05, 0x06, 0x00, 0x04, 0xBE, 0xEF}, 6);
    modbus_process(&map, 5, req, n, resp, &rn);
    CHECK_EQ(hr[4], 0xBEEF);
    CHECK_EQ(rn, 8);
    CHECK(memcmp(resp, req, 8) == 0);

    /* write multiple registers */
    n = frame(req, (const uint8_t[]){0x05, 0x10, 0x00, 0x00, 0x00, 0x02, 0x04, 0x12, 0x34, 0x56, 0x78}, 11);
    modbus_process(&map, 5, req, n, resp, &rn);
    CHECK_EQ(hr[0], 0x1234);
    CHECK_EQ(hr[1], 0x5678);
    CHECK_EQ(rn, 8);

    /* byte count mismatch -> exception 03 */
    n = frame(req, (const uint8_t[]){0x05, 0x10, 0x00, 0x00, 0x00, 0x02, 0x03, 0x12, 0x34, 0x56}, 10);
    modbus_process(&map, 5, req, n, resp, &rn);
    CHECK_EQ(resp[1], 0x90);
    CHECK_EQ(resp[2], 0x03);

    /* coils: write single, write multiple, read */
    coils = 0;
    n = frame(req, (const uint8_t[]){0x05, 0x05, 0x00, 0x02, 0xFF, 0x00}, 6);
    modbus_process(&map, 5, req, n, resp, &rn);
    CHECK_EQ(coils, 0x04);
    n = frame(req, (const uint8_t[]){0x05, 0x05, 0x00, 0x02, 0x12, 0x34}, 6); /* invalid value */
    modbus_process(&map, 5, req, n, resp, &rn);
    CHECK_EQ(resp[1], 0x85);
    n = frame(req, (const uint8_t[]){0x05, 0x0F, 0x00, 0x00, 0x00, 0x04, 0x01, 0x09}, 8);
    modbus_process(&map, 5, req, n, resp, &rn);
    CHECK_EQ(coils, 0x09);
    n = frame(req, (const uint8_t[]){0x05, 0x01, 0x00, 0x00, 0x00, 0x04}, 6);
    modbus_process(&map, 5, req, n, resp, &rn);
    CHECK_EQ(resp[2], 1);
    CHECK_EQ(resp[3], 0x09);

    /* broadcast write: executed, no response; broadcast read: no response */
    n = frame(req, (const uint8_t[]){0x00, 0x06, 0x00, 0x05, 0x00, 0x2A}, 6);
    CHECK_EQ(modbus_process(&map, 5, req, n, resp, &rn), MB_FRAME_HANDLED);
    CHECK_EQ(rn, 0);
    CHECK_EQ(hr[5], 42);
    n = frame(req, (const uint8_t[]){0x00, 0x03, 0x00, 0x00, 0x00, 0x01}, 6);
    modbus_process(&map, 5, req, n, resp, &rn);
    CHECK_EQ(rn, 0);
}
