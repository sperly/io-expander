#include "modbus_rtu.h"

#include <string.h>

#include "crc16.h"

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static size_t finish(uint8_t *resp, size_t n)
{
    uint16_t crc = crc16_modbus(resp, n);
    resp[n++] = (uint8_t)crc; /* CRC is sent low byte first */
    resp[n++] = (uint8_t)(crc >> 8);
    return n;
}

static size_t exception(uint8_t *resp, uint8_t addr, uint8_t fc, int code)
{
    resp[0] = addr;
    resp[1] = (uint8_t)(fc | 0x80u);
    resp[2] = (uint8_t)code;
    return finish(resp, 3);
}

modbus_result_t modbus_process(const modbus_map_t *map, uint8_t my_addr, const uint8_t *req, size_t len,
                               uint8_t *resp, size_t *resp_len)
{
    *resp_len = 0;
    if (len < 4 || len > MODBUS_MAX_ADU)
        return MB_FRAME_IGNORED;
    uint16_t crc = crc16_modbus(req, len - 2);
    if (req[len - 2] != (uint8_t)crc || req[len - 1] != (uint8_t)(crc >> 8))
        return MB_FRAME_IGNORED;
    uint8_t addr = req[0];
    if (addr != my_addr && addr != 0)
        return MB_FRAME_IGNORED;

    int broadcast = addr == 0;
    uint8_t fc = req[1];
    const uint8_t *pdu = req + 2;
    size_t pdu_len = len - 4; /* data after function code, without CRC */
    int ex = MB_OK;
    size_t n = 0;

    resp[0] = addr;
    resp[1] = fc;

    switch (fc) {
    case 0x01: /* read coils */
    case 0x02: /* read discrete inputs */
    {
        if (broadcast)
            return MB_FRAME_HANDLED;
        if (pdu_len != 4) {
            ex = MB_EX_ILLEGAL_VALUE;
            break;
        }
        uint16_t start = be16(pdu), cnt = be16(pdu + 2);
        if (cnt < 1 || cnt > 2000) {
            ex = MB_EX_ILLEGAL_VALUE;
            break;
        }
        uint8_t nbytes = (uint8_t)((cnt + 7u) / 8u);
        memset(resp + 3, 0, nbytes);
        ex = map->read_bits(map->ctx, fc == 0x01 ? MB_TABLE_COILS : MB_TABLE_DISCRETE, start, cnt, resp + 3);
        resp[2] = nbytes;
        n = 3u + nbytes;
        break;
    }
    case 0x03: /* read holding registers */
    case 0x04: /* read input registers */
    {
        if (broadcast)
            return MB_FRAME_HANDLED;
        if (pdu_len != 4) {
            ex = MB_EX_ILLEGAL_VALUE;
            break;
        }
        uint16_t start = be16(pdu), cnt = be16(pdu + 2);
        if (cnt < 1 || cnt > 125) {
            ex = MB_EX_ILLEGAL_VALUE;
            break;
        }
        uint16_t regs[125];
        ex = map->read_regs(map->ctx, fc == 0x03 ? MB_TABLE_HOLDING : MB_TABLE_INPUT, start, cnt, regs);
        if (ex)
            break;
        resp[2] = (uint8_t)(cnt * 2u);
        for (unsigned i = 0; i < cnt; i++)
            put16(resp + 3 + 2 * i, regs[i]);
        n = 3u + cnt * 2u;
        break;
    }
    case 0x05: /* write single coil */
    {
        if (pdu_len != 4) {
            ex = MB_EX_ILLEGAL_VALUE;
            break;
        }
        uint16_t a = be16(pdu), v = be16(pdu + 2);
        if (v != 0x0000 && v != 0xFF00) {
            ex = MB_EX_ILLEGAL_VALUE;
            break;
        }
        uint8_t bit = v ? 1u : 0u;
        ex = map->write_coils(map->ctx, a, 1, &bit);
        memcpy(resp + 2, pdu, 4); /* echo */
        n = 6;
        break;
    }
    case 0x06: /* write single register */
    {
        if (pdu_len != 4) {
            ex = MB_EX_ILLEGAL_VALUE;
            break;
        }
        uint16_t a = be16(pdu), v = be16(pdu + 2);
        ex = map->write_regs(map->ctx, a, 1, &v);
        memcpy(resp + 2, pdu, 4);
        n = 6;
        break;
    }
    case 0x0F: /* write multiple coils */
    {
        if (pdu_len < 6) {
            ex = MB_EX_ILLEGAL_VALUE;
            break;
        }
        uint16_t a = be16(pdu), cnt = be16(pdu + 2);
        uint8_t nbytes = pdu[4];
        if (cnt < 1 || cnt > 1968 || nbytes != (cnt + 7u) / 8u || pdu_len != 5u + nbytes) {
            ex = MB_EX_ILLEGAL_VALUE;
            break;
        }
        ex = map->write_coils(map->ctx, a, cnt, pdu + 5);
        memcpy(resp + 2, pdu, 4);
        n = 6;
        break;
    }
    case 0x10: /* write multiple registers */
    {
        if (pdu_len < 7) {
            ex = MB_EX_ILLEGAL_VALUE;
            break;
        }
        uint16_t a = be16(pdu), cnt = be16(pdu + 2);
        uint8_t nbytes = pdu[4];
        if (cnt < 1 || cnt > 123 || nbytes != cnt * 2u || pdu_len != 5u + nbytes) {
            ex = MB_EX_ILLEGAL_VALUE;
            break;
        }
        uint16_t regs[123];
        for (unsigned i = 0; i < cnt; i++)
            regs[i] = be16(pdu + 5 + 2 * i);
        ex = map->write_regs(map->ctx, a, cnt, regs);
        memcpy(resp + 2, pdu, 4);
        n = 6;
        break;
    }
    default:
        ex = MB_EX_ILLEGAL_FUNCTION;
        break;
    }

    if (broadcast)
        return MB_FRAME_HANDLED;
    *resp_len = ex ? exception(resp, addr, fc, ex) : finish(resp, n);
    return MB_FRAME_HANDLED;
}
