#include "config.h"

#include <string.h>

#include "crc16.h"

#define HDR 5u
#define PAYLOAD_LEN (7u + IO_CHANNELS * 3u + IO_PWM_GENS * 2u)

void cfg_defaults(cfg_t *c)
{
    c->base_addr = 1;
    c->baud = CFG_BAUD_19200;
    c->parity = CFG_PARITY_EVEN; /* Modbus RTU default 8E1 */
    c->timeout_ms = 0;
    c->in_invert = 0;
    c->in_debounce_ms = 10;
    io_config_defaults(&c->io);
}

bool cfg_valid(const cfg_t *c)
{
    if (c->base_addr < 1 || c->base_addr > 247 || c->baud >= CFG_BAUD_COUNT || c->parity >= CFG_PARITY_COUNT ||
        c->in_debounce_ms > CFG_DEBOUNCE_MAX_MS)
        return false;
    for (unsigned i = 0; i < IO_CHANNELS; i++)
        if (!io_channel_valid(&c->io.ch[i]))
            return false;
    for (unsigned g = 0; g < IO_PWM_GENS; g++)
        if (c->io.freq[g] > IO_PWM_2KHZ)
            return false;
    return true;
}

uint32_t cfg_baud_rate(uint8_t code)
{
    static const uint32_t rates[CFG_BAUD_COUNT] = {9600, 19200, 38400, 57600, 115200};
    return code < CFG_BAUD_COUNT ? rates[code] : 19200;
}

void cfg_encode(const cfg_t *c, uint8_t seq, uint8_t slot[CFG_SLOT_SIZE])
{
    memset(slot, 0xFF, CFG_SLOT_SIZE);
    slot[0] = 'I';
    slot[1] = '8';
    slot[2] = CFG_VERSION;
    slot[3] = seq;
    slot[4] = PAYLOAD_LEN;
    uint8_t *p = slot + HDR;
    *p++ = c->base_addr;
    *p++ = c->baud;
    *p++ = c->parity;
    *p++ = (uint8_t)c->timeout_ms;
    *p++ = (uint8_t)(c->timeout_ms >> 8);
    *p++ = c->in_invert;
    *p++ = c->in_debounce_ms;
    for (unsigned i = 0; i < IO_CHANNELS; i++) {
        *p++ = c->io.ch[i].mode;
        *p++ = c->io.ch[i].out;
        *p++ = c->io.ch[i].pwm;
    }
    for (unsigned g = 0; g < IO_PWM_GENS; g++) {
        *p++ = c->io.duty[g];
        *p++ = c->io.freq[g];
    }
    uint16_t crc = crc16_modbus(slot, HDR + PAYLOAD_LEN);
    *p++ = (uint8_t)crc;
    *p = (uint8_t)(crc >> 8);
}

bool cfg_decode(const uint8_t slot[CFG_SLOT_SIZE], cfg_t *c, uint8_t *seq)
{
    if (slot[0] != 'I' || slot[1] != '8' || slot[2] != CFG_VERSION || slot[4] != PAYLOAD_LEN)
        return false;
    uint16_t crc = crc16_modbus(slot, HDR + PAYLOAD_LEN);
    if (slot[HDR + PAYLOAD_LEN] != (uint8_t)crc || slot[HDR + PAYLOAD_LEN + 1] != (uint8_t)(crc >> 8))
        return false;
    cfg_t t;
    const uint8_t *p = slot + HDR;
    t.base_addr = *p++;
    t.baud = *p++;
    t.parity = *p++;
    t.timeout_ms = (uint16_t)(p[0] | (p[1] << 8));
    p += 2;
    t.in_invert = *p++;
    t.in_debounce_ms = *p++;
    for (unsigned i = 0; i < IO_CHANNELS; i++) {
        t.io.ch[i].mode = *p++;
        t.io.ch[i].out = *p++;
        t.io.ch[i].pwm = *p++;
    }
    for (unsigned g = 0; g < IO_PWM_GENS; g++) {
        t.io.duty[g] = *p++;
        t.io.freq[g] = *p++;
    }
    if (!cfg_valid(&t))
        return false;
    *c = t;
    *seq = slot[3];
    return true;
}

/* newest slot index, or -1 if none valid */
static int newest(const cfg_storage_t *st, cfg_t *c, uint8_t *seq_out)
{
    uint8_t buf[CFG_SLOT_SIZE];
    cfg_t cand[CFG_SLOT_COUNT];
    uint8_t seq[CFG_SLOT_COUNT];
    bool ok[CFG_SLOT_COUNT];
    for (unsigned s = 0; s < CFG_SLOT_COUNT; s++) {
        ok[s] = st->read(st->ctx, (uint16_t)(s * CFG_SLOT_SIZE), buf, CFG_SLOT_SIZE) == 0 &&
                cfg_decode(buf, &cand[s], &seq[s]);
    }
    int best = -1;
    if (ok[0] && ok[1])
        best = (int8_t)(seq[1] - seq[0]) > 0 ? 1 : 0; /* wrap-around safe */
    else if (ok[0])
        best = 0;
    else if (ok[1])
        best = 1;
    if (best >= 0) {
        if (c)
            *c = cand[best];
        if (seq_out)
            *seq_out = seq[best];
    }
    return best;
}

bool cfg_load(const cfg_storage_t *st, cfg_t *c)
{
    if (newest(st, c, NULL) >= 0)
        return true;
    cfg_defaults(c);
    return false;
}

int cfg_save(const cfg_storage_t *st, const cfg_t *c)
{
    if (!cfg_valid(c))
        return -1;
    uint8_t seq = 0;
    int best = newest(st, NULL, &seq);
    unsigned slot = best == 0 ? 1u : 0u;
    uint8_t buf[CFG_SLOT_SIZE];
    cfg_encode(c, (uint8_t)(best >= 0 ? seq + 1u : 0u), buf);
    if (st->write(st->ctx, (uint16_t)(slot * CFG_SLOT_SIZE), buf, CFG_SLOT_SIZE))
        return -2;
    /* verify */
    uint8_t chk[CFG_SLOT_SIZE];
    if (st->read(st->ctx, (uint16_t)(slot * CFG_SLOT_SIZE), chk, CFG_SLOT_SIZE) || memcmp(buf, chk, CFG_SLOT_SIZE))
        return -3;
    return 0;
}
