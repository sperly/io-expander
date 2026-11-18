#include "app.h"

#include <string.h>

#include "drv8912_regs.h"

uint8_t app_address(const cfg_t *cfg, uint8_t straps)
{
    unsigned a = (unsigned)cfg->base_addr + (straps & 3u);
    return (uint8_t)(a > 247u ? 247u : a);
}

static void apply(app_t *a)
{
    a->hw.apply_outputs(a->hw.ctx, &a->cfg.io, a->outputs_enabled);
}

/* ------------------------------------------------------------ Modbus tables */
static int read_bits(void *ctx, int table, uint16_t addr, uint16_t count, uint8_t *bits)
{
    app_t *a = ctx;
    if ((uint32_t)addr + count > IO_CHANNELS)
        return MB_EX_ILLEGAL_ADDRESS;
    for (uint16_t i = 0; i < count; i++) {
        unsigned ch = addr + i;
        unsigned v = table == MB_TABLE_COILS ? a->cfg.io.ch[ch].out : (a->inputs >> ch) & 1u;
        if (v)
            bits[i / 8u] |= (uint8_t)(1u << (i % 8u));
    }
    return MB_OK;
}

static int write_coils(void *ctx, uint16_t addr, uint16_t count, const uint8_t *bits)
{
    app_t *a = ctx;
    if ((uint32_t)addr + count > IO_CHANNELS)
        return MB_EX_ILLEGAL_ADDRESS;
    for (uint16_t i = 0; i < count; i++)
        a->cfg.io.ch[addr + i].out = (bits[i / 8u] >> (i % 8u)) & 1u;
    apply(a);
    return MB_OK;
}

static uint16_t holding(const app_t *a, unsigned r)
{
    const io_config_t *io = &a->cfg.io;
    if (r < APP_HR_OUT)
        return io->ch[r - APP_HR_MODE].mode;
    if (r < APP_HR_PWM)
        return io->ch[r - APP_HR_OUT].out;
    if (r < APP_HR_DUTY)
        return io->ch[r - APP_HR_PWM].pwm;
    if (r < APP_HR_FREQ)
        return io->duty[r - APP_HR_DUTY];
    if (r < APP_HR_IN_INVERT)
        return io->freq[r - APP_HR_FREQ];
    switch (r) {
    case APP_HR_IN_INVERT: return a->cfg.in_invert;
    case APP_HR_IN_DEBOUNCE: return a->cfg.in_debounce_ms;
    case APP_HR_BASE_ADDR: return a->cfg.base_addr;
    case APP_HR_BAUD: return a->cfg.baud;
    case APP_HR_PARITY: return a->cfg.parity;
    case APP_HR_TIMEOUT: return a->cfg.timeout_ms;
    default: return 0; /* command register reads 0 */
    }
}

static uint16_t input_reg(const app_t *a, unsigned r)
{
    uint32_t up = a->now_ms / 1000u;
    switch (r) {
    case 0: return (uint16_t)((FW_VERSION_MAJOR << 8) | FW_VERSION_MINOR);
    case 1: return a->inputs;
    case 2: return a->vio_mv;
    case 3: return a->drv_stat;
    case 4: return (uint16_t)(a->drv_ocp1 | (a->drv_ocp2 << 8));
    case 5: return a->flags;
    case 6: return a->address;
    case 7: return a->straps;
    case 8: return (uint16_t)(up >> 16);
    case 9: return (uint16_t)up;
    case 18: return a->in_raw;
    default: return a->counters[r - 10u];
    }
}

static int read_regs(void *ctx, int table, uint16_t addr, uint16_t count, uint16_t *regs)
{
    app_t *a = ctx;
    unsigned limit = table == MB_TABLE_HOLDING ? APP_HR_COUNT : APP_IR_COUNT;
    if ((uint32_t)addr + count > limit)
        return MB_EX_ILLEGAL_ADDRESS;
    for (uint16_t i = 0; i < count; i++)
        regs[i] = table == MB_TABLE_HOLDING ? holding(a, addr + i) : input_reg(a, addr + i);
    return MB_OK;
}

static int check_value(unsigned r, uint16_t v)
{
    if (r < APP_HR_OUT)
        return v < IO_MODE_COUNT;
    if (r < APP_HR_PWM)
        return v <= 1u;
    if (r < APP_HR_DUTY)
        return v <= IO_PWM_GENS;
    if (r < APP_HR_FREQ)
        return v <= 255u;
    if (r < APP_HR_IN_INVERT)
        return v <= IO_PWM_2KHZ;
    switch (r) {
    case APP_HR_IN_INVERT: return v <= 0xFFu;
    case APP_HR_IN_DEBOUNCE: return v <= CFG_DEBOUNCE_MAX_MS;
    case APP_HR_BASE_ADDR: return v >= 1u && v <= 247u;
    case APP_HR_BAUD: return v < CFG_BAUD_COUNT;
    case APP_HR_PARITY: return v < CFG_PARITY_COUNT;
    case APP_HR_TIMEOUT: return 1;
    case APP_HR_COMMAND:
        return v == APP_CMD_SAVE || v == APP_CMD_DEFAULTS || v == APP_CMD_CLEAR_FAULTS || v == APP_CMD_REBOOT ||
               v == APP_CMD_CLEAR_COUNTERS;
    default: return 0;
    }
}

static void command(app_t *a, uint16_t cmd)
{
    switch (cmd) {
    case APP_CMD_SAVE:
        if (a->hw.save_config(a->hw.ctx, &a->cfg) == 0)
            a->flags &= (uint16_t)~APP_FLAG_EEPROM_ERROR;
        else
            a->flags |= APP_FLAG_EEPROM_ERROR;
        break;
    case APP_CMD_DEFAULTS:
        cfg_defaults(&a->cfg);
        break;
    case APP_CMD_CLEAR_FAULTS:
        a->hw.clear_faults(a->hw.ctx);
        break;
    case APP_CMD_REBOOT:
        a->reboot_pending = true; /* executed after the response is sent */
        break;
    case APP_CMD_CLEAR_COUNTERS:
        memset(a->counters, 0, sizeof(a->counters));
        break;
    default:
        break;
    }
}

static int write_regs(void *ctx, uint16_t addr, uint16_t count, const uint16_t *regs)
{
    app_t *a = ctx;
    if ((uint32_t)addr + count > APP_HR_COUNT)
        return MB_EX_ILLEGAL_ADDRESS;
    for (uint16_t i = 0; i < count; i++) /* all-or-nothing */
        if (!check_value(addr + i, regs[i]))
            return MB_EX_ILLEGAL_VALUE;

    io_config_t *io = &a->cfg.io;
    bool io_changed = false;
    for (uint16_t i = 0; i < count; i++) {
        unsigned r = addr + i;
        uint16_t v = regs[i];
        if (r < APP_HR_OUT) {
            io->ch[r - APP_HR_MODE].mode = (uint8_t)v;
            io_changed = true;
        } else if (r < APP_HR_PWM) {
            io->ch[r - APP_HR_OUT].out = (uint8_t)v;
            io_changed = true;
        } else if (r < APP_HR_DUTY) {
            io->ch[r - APP_HR_PWM].pwm = (uint8_t)v;
            io_changed = true;
        } else if (r < APP_HR_FREQ) {
            io->duty[r - APP_HR_DUTY] = (uint8_t)v;
            io_changed = true;
        } else if (r < APP_HR_IN_INVERT) {
            io->freq[r - APP_HR_FREQ] = (uint8_t)v;
            io_changed = true;
        } else if (r == APP_HR_IN_INVERT) {
            a->cfg.in_invert = (uint8_t)v;
        } else if (r == APP_HR_IN_DEBOUNCE) {
            a->cfg.in_debounce_ms = (uint8_t)v;
        } else if (r == APP_HR_BASE_ADDR) {
            a->cfg.base_addr = (uint8_t)v;
        } else if (r == APP_HR_BAUD) {
            a->cfg.baud = (uint8_t)v;
        } else if (r == APP_HR_PARITY) {
            a->cfg.parity = (uint8_t)v;
        } else if (r == APP_HR_TIMEOUT) {
            a->cfg.timeout_ms = v;
        } else if (r == APP_HR_COMMAND) {
            command(a, v);
            if (v == APP_CMD_DEFAULTS)
                io_changed = true;
        }
    }
    if (io_changed)
        apply(a);
    return MB_OK;
}

/* ------------------------------------------------------------------- public */
void app_init(app_t *a, const app_hw_t *hw, const cfg_t *cfg, bool cfg_loaded, uint8_t straps)
{
    memset(a, 0, sizeof(*a));
    a->hw = *hw;
    a->cfg = *cfg;
    a->straps = straps;
    a->address = app_address(cfg, straps);
    a->outputs_enabled = true;
    a->flags = cfg_loaded ? APP_FLAG_CFG_LOADED : 0;
    a->map = (modbus_map_t){a, read_bits, write_coils, read_regs, write_regs};
    apply(a);
}

size_t app_handle_frame(app_t *a, const uint8_t *req, size_t len, uint8_t *resp)
{
    size_t n = 0;
    if (modbus_process(&a->map, a->address, req, len, resp, &n) == MB_FRAME_HANDLED) {
        a->last_rx_ms = a->now_ms;
        if (!a->outputs_enabled) { /* comm restored */
            a->outputs_enabled = true;
            a->flags &= (uint16_t)~APP_FLAG_COMM_TIMEOUT;
            apply(a);
        }
    }
    return n;
}

void app_tick(app_t *a, uint32_t now_ms)
{
    a->now_ms = now_ms;
    if (a->cfg.timeout_ms && a->outputs_enabled && (uint32_t)(now_ms - a->last_rx_ms) > a->cfg.timeout_ms) {
        a->outputs_enabled = false;
        a->flags |= APP_FLAG_COMM_TIMEOUT;
        apply(a);
    }
    if (a->reboot_pending)
        a->hw.reboot(a->hw.ctx);
}

void app_set_inputs(app_t *a, uint8_t raw)
{
    uint8_t sample = (uint8_t)(raw ^ a->cfg.in_invert);
    a->in_raw = raw;
    if (!a->inputs_valid) { /* first sample: take it as is, no edges */
        a->inputs_valid = true;
        a->inputs = a->in_candidate = sample;
        for (unsigned i = 0; i < APP_INPUTS; i++)
            a->in_since[i] = a->now_ms;
        return;
    }
    for (unsigned i = 0; i < APP_INPUTS; i++) {
        uint8_t m = (uint8_t)(1u << i);
        if ((sample ^ a->in_candidate) & m) { /* level changed: restart the debounce timer */
            a->in_candidate ^= m;
            a->in_since[i] = a->now_ms;
        }
        if (((a->in_candidate ^ a->inputs) & m) &&
            (uint32_t)(a->now_ms - a->in_since[i]) >= a->cfg.in_debounce_ms) {
            a->inputs ^= m;
            if (a->inputs & m)
                a->counters[i]++;
        }
    }
}

void app_set_vio(app_t *a, uint16_t mv) { a->vio_mv = mv; }

void app_set_driver_status(app_t *a, uint8_t ic_stat, uint8_t ocp1, uint8_t ocp2, bool present)
{
    a->drv_stat = ic_stat;
    a->drv_ocp1 = ocp1;
    a->drv_ocp2 = ocp2;
    if (ic_stat & (DRV_STAT_OCP | DRV_STAT_OTSD | DRV_STAT_UVLO | DRV_STAT_OVP))
        a->flags |= APP_FLAG_DRV_FAULT;
    else
        a->flags &= (uint16_t)~APP_FLAG_DRV_FAULT;
    if (present)
        a->flags &= (uint16_t)~APP_FLAG_DRV_MISSING;
    else
        a->flags |= APP_FLAG_DRV_MISSING;
}
