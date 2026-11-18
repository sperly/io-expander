#include <string.h>

#include "config.h"
#include "unit.h"

typedef struct {
    uint8_t mem[256];
    int fail_write_after; /* simulate power loss: stop writing after n bytes (-1 = never) */
} fake_ee_t;

static int ee_read(void *ctx, uint16_t off, uint8_t *buf, uint16_t len)
{
    fake_ee_t *e = ctx;
    memcpy(buf, e->mem + off, len);
    return 0;
}

static int ee_write(void *ctx, uint16_t off, const uint8_t *buf, uint16_t len)
{
    fake_ee_t *e = ctx;
    for (uint16_t i = 0; i < len; i++) {
        if (e->fail_write_after == 0)
            return -1;
        if (e->fail_write_after > 0)
            e->fail_write_after--;
        e->mem[off + i] = buf[i];
    }
    return 0;
}

void test_config(void)
{
    fake_ee_t ee;
    memset(ee.mem, 0xFF, sizeof ee.mem); /* blank EEPROM */
    ee.fail_write_after = -1;
    cfg_storage_t st = {&ee, ee_read, ee_write};
    cfg_t c, d;

    /* blank -> defaults, not loaded */
    CHECK(!cfg_load(&st, &c));
    CHECK_EQ(c.base_addr, 1);
    CHECK_EQ(c.baud, CFG_BAUD_19200);
    CHECK_EQ(c.io.ch[3].mode, IO_MODE_OFF);
    CHECK_EQ(c.in_debounce_ms, 10);

    /* encode / decode round trip */
    c.base_addr = 17;
    c.timeout_ms = 1500;
    c.io.ch[2] = (io_channel_t){IO_MODE_PUSH_PULL, 1, 3};
    c.io.duty[2] = 77;
    c.in_invert = 0xA5;
    c.in_debounce_ms = 40;
    uint8_t slot[CFG_SLOT_SIZE], seq;
    cfg_encode(&c, 9, slot);
    CHECK(cfg_decode(slot, &d, &seq));
    CHECK_EQ(seq, 9);
    CHECK_EQ(d.base_addr, 17);
    CHECK_EQ(d.timeout_ms, 1500);
    CHECK_EQ(d.io.ch[2].mode, IO_MODE_PUSH_PULL);
    CHECK_EQ(d.io.ch[2].pwm, 3);
    CHECK_EQ(d.io.duty[2], 77);
    CHECK_EQ(d.in_invert, 0xA5);
    CHECK_EQ(d.in_debounce_ms, 40);
    slot[10] ^= 0x40; /* corrupt */
    CHECK(!cfg_decode(slot, &d, &seq));

    /* first save goes to slot 0, second to slot 1, load returns the newest */
    CHECK_EQ(cfg_save(&st, &c), 0);
    CHECK_EQ(ee.mem[0], 'I');
    CHECK_EQ(ee.mem[128], 0xFF);
    c.base_addr = 18;
    CHECK_EQ(cfg_save(&st, &c), 0);
    CHECK_EQ(ee.mem[128], 'I');
    CHECK(cfg_load(&st, &d));
    CHECK_EQ(d.base_addr, 18);

    /* third save overwrites slot 0 (the older one) */
    c.base_addr = 19;
    CHECK_EQ(cfg_save(&st, &c), 0);
    CHECK(cfg_load(&st, &d));
    CHECK_EQ(d.base_addr, 19);
    CHECK_EQ(ee.mem[3], 2); /* slot 0 now has sequence 2 */

    /* power loss in the middle of the fourth save: the previous copy survives */
    c.base_addr = 20;
    ee.fail_write_after = 20;
    CHECK(cfg_save(&st, &c) != 0);
    ee.fail_write_after = -1;
    CHECK(cfg_load(&st, &d));
    CHECK_EQ(d.base_addr, 19);

    /* sequence wrap-around: 255 -> 0 is newer */
    cfg_t e1 = c, e2 = c;
    e1.base_addr = 30;
    e2.base_addr = 31;
    cfg_encode(&e1, 255, ee.mem);
    cfg_encode(&e2, 0, ee.mem + 128);
    CHECK(cfg_load(&st, &d));
    CHECK_EQ(d.base_addr, 31);

    /* invalid values are rejected */
    c.base_addr = 0;
    CHECK(!cfg_valid(&c));
    CHECK(cfg_save(&st, &c) != 0);
    cfg_defaults(&c);
    c.io.ch[0].pwm = 5;
    CHECK(!cfg_valid(&c));
    cfg_defaults(&c);
    c.in_debounce_ms = 251;
    CHECK(!cfg_valid(&c));
    /* a version-1 image (rev B first firmware) is not accepted: defaults are used */
    cfg_defaults(&c);
    cfg_encode(&c, 0, slot);
    slot[2] = 1;
    CHECK(!cfg_decode(slot, &d, &seq));
    CHECK_EQ(cfg_baud_rate(CFG_BAUD_115200), 115200);
}
