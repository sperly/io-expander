#include "io_model.h"

#include "drv8912_regs.h"

void io_config_defaults(io_config_t *cfg)
{
    for (unsigned i = 0; i < IO_CHANNELS; i++) {
        cfg->ch[i].mode = IO_MODE_OFF;
        cfg->ch[i].out = 0;
        cfg->ch[i].pwm = 0;
    }
    for (unsigned g = 0; g < IO_PWM_GENS; g++) {
        cfg->duty[g] = 128;
        cfg->freq[g] = IO_PWM_2KHZ;
    }
}

bool io_channel_valid(const io_channel_t *ch)
{
    return ch->mode < IO_MODE_COUNT && ch->out <= 1u && ch->pwm <= IO_PWM_GENS;
}

/* switch state of one half-bridge */
typedef struct {
    bool hs, ls, pwm, fw;
} hb_t;

static hb_t channel_state(const io_channel_t *c)
{
    hb_t h = {false, false, false, false};
    bool pwm = c->pwm != 0u;
    switch (c->mode) {
    case IO_MODE_LOW_SIDE:
        if (c->out) {
            h.ls = true;
            h.pwm = pwm;
        }
        break;
    case IO_MODE_HIGH_SIDE:
        if (c->out) {
            h.hs = true;
            h.pwm = pwm;
        }
        break;
    case IO_MODE_PUSH_PULL:
        if (c->out) {
            /* PWM on the high side, synchronous rectification turns the
             * low side on during the PWM off time -> push-pull PWM */
            h.hs = true;
            h.pwm = pwm;
            h.fw = pwm;
        } else {
            h.ls = true;
        }
        break;
    default: /* off: both switches off */
        break;
    }
    return h;
}

size_t io_model_registers(const io_config_t *cfg, bool outputs_enabled, drv_reg_t out[IO_MODEL_REG_COUNT])
{
    uint8_t op[2] = {0, 0}, pwm_ctrl1 = 0, fw1 = 0, map[2] = {0, 0}, freq = 0;
    uint8_t gens_used = 0;

    for (unsigned i = 0; i < IO_CHANNELS; i++) {
        const io_channel_t *c = &cfg->ch[i];
        hb_t h = outputs_enabled ? channel_state(c) : (hb_t){false, false, false, false};
        unsigned reg = i / 4u, pos = (i % 4u) * 2u;
        if (h.ls)
            op[reg] |= (uint8_t)(1u << pos);
        if (h.hs)
            op[reg] |= (uint8_t)(1u << (pos + 1u));
        if (h.pwm) {
            unsigned gen = (unsigned)c->pwm - 1u;
            pwm_ctrl1 |= (uint8_t)(1u << i);
            map[reg] |= (uint8_t)(gen << pos);
            gens_used |= (uint8_t)(1u << gen);
        }
        if (h.fw)
            fw1 |= (uint8_t)(1u << i);
    }
    for (unsigned g = 0; g < IO_PWM_GENS; g++)
        freq |= (uint8_t)((cfg->freq[g] & 3u) << (2u * g));

    size_t n = 0;
    out[n++] = (drv_reg_t){DRV_PWM_FREQ_CTRL, freq};
    for (unsigned g = 0; g < IO_PWM_GENS; g++)
        out[n++] = (drv_reg_t){(uint8_t)(DRV_PWM_DUTY_CTRL_1 + g), cfg->duty[g]};
    out[n++] = (drv_reg_t){DRV_PWM_MAP_CTRL_1, map[0]};
    out[n++] = (drv_reg_t){DRV_PWM_MAP_CTRL_2, map[1]};
    out[n++] = (drv_reg_t){DRV_FW_CTRL_1, fw1};
    out[n++] = (drv_reg_t){DRV_PWM_CTRL_1, pwm_ctrl1};
    /* unused generators disabled, HB9..HB12 never in PWM mode */
    out[n++] = (drv_reg_t){DRV_PWM_CTRL_2, (uint8_t)((~gens_used & 0x0Fu) << 4)};
    out[n++] = (drv_reg_t){DRV_OP_CTRL_1, op[0]};
    out[n++] = (drv_reg_t){DRV_OP_CTRL_2, op[1]};
    out[n++] = (drv_reg_t){DRV_OP_CTRL_3, 0x00}; /* OUT9..OUT12 unused */
    /* pad with harmless re-writes so the count is constant */
    while (n < IO_MODEL_REG_COUNT)
        out[n++] = (drv_reg_t){DRV_OP_CTRL_3, 0x00};
    return n;
}

size_t io_model_init_registers(drv_reg_t out[IO_MODEL_INIT_REG_COUNT])
{
    size_t n = 0;
    /* logic loads: open-load detection makes no sense, disable it on every
     * half-bridge and keep the bridges active if it ever triggers */
    out[n++] = (drv_reg_t){DRV_OLD_CTRL_1, 0xFF};
    out[n++] = (drv_reg_t){DRV_OLD_CTRL_2, DRV_OLD2_OLD_REP | DRV_OLD2_OLD_OP | 0x0Fu};
    out[n++] = (drv_reg_t){DRV_OLD_CTRL_3, 0x00}; /* OCP deglitch 10 us, no low-current OLD */
    out[n++] = (drv_reg_t){DRV_OLD_CTRL_4, 0x00};
    out[n++] = (drv_reg_t){DRV_SR_CTRL_1, 0xFF};  /* 2.5 V/us: clean logic edges */
    out[n++] = (drv_reg_t){DRV_FW_CTRL_2, 0x00};
    out[n++] = (drv_reg_t){DRV_PWM_MAP_CTRL_3, 0x00};
    /* OVP at 21 V (VIO max 14 V), clear the power-on-reset flag */
    out[n++] = (drv_reg_t){DRV_CONFIG_CTRL, DRV_CFG_CLR_FLT};
    return n;
}
