#include <string.h>

#include "drv8912_regs.h"
#include "io_model.h"
#include "unit.h"

static int reg(const drv_reg_t *r, size_t n, uint8_t addr)
{
    int v = -1;
    for (size_t i = 0; i < n; i++)
        if (r[i].addr == addr)
            v = r[i].val; /* last write wins */
    return v;
}

static size_t last_index(const drv_reg_t *r, size_t n, uint8_t addr)
{
    size_t idx = 0;
    for (size_t i = 0; i < n; i++)
        if (r[i].addr == addr)
            idx = i;
    return idx;
}

void test_io_model(void)
{
    io_config_t c;
    drv_reg_t r[IO_MODEL_REG_COUNT];
    io_config_defaults(&c);

    /* defaults: everything off */
    size_t n = io_model_registers(&c, true, r);
    CHECK_EQ(n, IO_MODEL_REG_COUNT);
    CHECK_EQ(reg(r, n, DRV_OP_CTRL_1), 0);
    CHECK_EQ(reg(r, n, DRV_OP_CTRL_2), 0);
    CHECK_EQ(reg(r, n, DRV_OP_CTRL_3), 0);
    CHECK_EQ(reg(r, n, DRV_PWM_CTRL_1), 0);
    CHECK_EQ(reg(r, n, DRV_PWM_CTRL_2), 0xF0); /* all generators disabled */

    /* OUT1 low-side on, OUT2 high-side on, OUT5 push-pull high, OUT6 push-pull low */
    c.ch[0] = (io_channel_t){IO_MODE_LOW_SIDE, 1, 0};
    c.ch[1] = (io_channel_t){IO_MODE_HIGH_SIDE, 1, 0};
    c.ch[4] = (io_channel_t){IO_MODE_PUSH_PULL, 1, 0};
    c.ch[5] = (io_channel_t){IO_MODE_PUSH_PULL, 0, 0};
    c.ch[2] = (io_channel_t){IO_MODE_LOW_SIDE, 0, 0}; /* off */
    n = io_model_registers(&c, true, r);
    /* OP_CTRL_1: HB1 LS = bit0, HB2 HS = bit3 */
    CHECK_EQ(reg(r, n, DRV_OP_CTRL_1), 0x01 | 0x08);
    /* OP_CTRL_2: HB5 HS = bit1, HB6 LS = bit2 */
    CHECK_EQ(reg(r, n, DRV_OP_CTRL_2), 0x02 | 0x04);

    /* never both switches of one half-bridge */
    for (unsigned m = 0; m < IO_MODE_COUNT; m++)
        for (unsigned o = 0; o < 2; o++)
            for (unsigned p = 0; p <= IO_PWM_GENS; p++) {
                io_config_t t;
                io_config_defaults(&t);
                for (unsigned i = 0; i < IO_CHANNELS; i++)
                    t.ch[i] = (io_channel_t){(uint8_t)m, (uint8_t)o, (uint8_t)p};
                drv_reg_t rr[IO_MODEL_REG_COUNT];
                size_t k = io_model_registers(&t, true, rr);
                for (unsigned reg_i = 0; reg_i < 2; reg_i++) {
                    int v = reg(rr, k, (uint8_t)(DRV_OP_CTRL_1 + reg_i));
                    CHECK(((v >> 1) & v & 0x55) == 0);
                }
            }

    /* PWM: OUT3 low-side on gen 2, OUT8 push-pull on gen 4 (sync rectification) */
    io_config_defaults(&c);
    c.ch[2] = (io_channel_t){IO_MODE_LOW_SIDE, 1, 2};
    c.ch[7] = (io_channel_t){IO_MODE_PUSH_PULL, 1, 4};
    c.duty[1] = 64;
    c.duty[3] = 200;
    c.freq[1] = IO_PWM_200HZ;
    c.freq[3] = IO_PWM_80HZ;
    n = io_model_registers(&c, true, r);
    CHECK_EQ(reg(r, n, DRV_PWM_CTRL_1), 0x04 | 0x80);
    CHECK_EQ(reg(r, n, DRV_FW_CTRL_1), 0x80);
    CHECK_EQ(reg(r, n, DRV_PWM_MAP_CTRL_1), 1 << 4); /* HB3 -> gen 2 (01b) at bits 5:4 */
    CHECK_EQ(reg(r, n, DRV_PWM_MAP_CTRL_2), 3 << 6); /* HB8 -> gen 4 (11b) at bits 7:6 */
    CHECK_EQ(reg(r, n, DRV_PWM_CTRL_2), 0x50);        /* gens 1 and 3 disabled */
    CHECK_EQ(reg(r, n, DRV_PWM_DUTY_CTRL_2), 64);
    CHECK_EQ(reg(r, n, DRV_PWM_DUTY_CTRL_4), 200);
    /* defaults 2 kHz = 3 for gens 1 and 3, 200 Hz = 2 for gen 2, 80 Hz = 0 for gen 4 */
    CHECK_EQ(reg(r, n, DRV_PWM_FREQ_CTRL), 0x03 | (0x02 << 2) | (0x03 << 4) | (0x00 << 6));
    CHECK_EQ(reg(r, n, DRV_OP_CTRL_1), 0x10);         /* HB3 LS */
    CHECK_EQ(reg(r, n, DRV_OP_CTRL_2), 0x80);         /* HB8 HS */
    /* PWM set-up is written before the switch enables */
    CHECK(last_index(r, n, DRV_PWM_CTRL_1) < last_index(r, n, DRV_OP_CTRL_1));
    CHECK(last_index(r, n, DRV_PWM_MAP_CTRL_2) < last_index(r, n, DRV_OP_CTRL_2));

    /* PWM channel with output 0 is off, and its generator is not enabled */
    c.ch[2].out = 0;
    n = io_model_registers(&c, true, r);
    CHECK_EQ(reg(r, n, DRV_PWM_CTRL_1), 0x80);
    CHECK_EQ(reg(r, n, DRV_OP_CTRL_1), 0x00);
    CHECK_EQ(reg(r, n, DRV_PWM_CTRL_2), 0x70);

    /* fail-safe: everything off but PWM settings kept */
    n = io_model_registers(&c, false, r);
    CHECK_EQ(reg(r, n, DRV_OP_CTRL_1), 0);
    CHECK_EQ(reg(r, n, DRV_OP_CTRL_2), 0);
    CHECK_EQ(reg(r, n, DRV_PWM_CTRL_1), 0);
    CHECK_EQ(reg(r, n, DRV_PWM_DUTY_CTRL_4), 200);

    /* static init disables open-load detection on all bridges */
    drv_reg_t ir[IO_MODEL_INIT_REG_COUNT];
    size_t k = io_model_init_registers(ir);
    CHECK_EQ(reg(ir, k, DRV_OLD_CTRL_1), 0xFF);
    CHECK_EQ(reg(ir, k, DRV_OLD_CTRL_2) & 0x0F, 0x0F);
    CHECK_EQ(reg(ir, k, DRV_CONFIG_CTRL), DRV_CFG_CLR_FLT);

    /* SPI frame encoding */
    CHECK_EQ(DRV_SPI_READ(0x07), 0x4700);
    CHECK_EQ(DRV_SPI_WRITE(0x24, 0x5A), 0x245A);
}
