/* End-to-end tests of the Modbus register map through app_handle_frame(). */
#include <string.h>

#include "app.h"
#include "crc16.h"
#include "unit.h"

typedef struct {
    io_config_t io;
    bool enabled;
    int applies, saves, clears, reboots;
    cfg_t saved;
    int save_result;
} fake_hw_t;

static void f_apply(void *ctx, const io_config_t *io, bool en)
{
    fake_hw_t *f = ctx;
    f->io = *io;
    f->enabled = en;
    f->applies++;
}
static int f_save(void *ctx, const cfg_t *c)
{
    fake_hw_t *f = ctx;
    f->saves++;
    f->saved = *c;
    return f->save_result;
}
static void f_clear(void *ctx) { ((fake_hw_t *)ctx)->clears++; }
static void f_reboot(void *ctx) { ((fake_hw_t *)ctx)->reboots++; }

static app_t app;
static fake_hw_t hw;
static uint8_t resp[MODBUS_MAX_ADU];

static size_t xfer(const uint8_t *pdu, size_t n)
{
    uint8_t req[MODBUS_MAX_ADU];
    memcpy(req, pdu, n);
    uint16_t c = crc16_modbus(req, n);
    req[n] = (uint8_t)c;
    req[n + 1] = (uint8_t)(c >> 8);
    return app_handle_frame(&app, req, n + 2, resp);
}

static uint16_t rd16(size_t off) { return (uint16_t)((resp[off] << 8) | resp[off + 1]); }

/* feed the same raw sample every millisecond from t0 for ms milliseconds */
static void hold_inputs(uint8_t raw, uint32_t *t, uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        app_tick(&app, ++*t);
        app_set_inputs(&app, raw);
    }
}

static void test_inputs(void)
{
    cfg_t cfg;
    cfg_defaults(&cfg);
    cfg.in_debounce_ms = 10;
    app_hw_t h = {&hw, f_apply, f_save, f_clear, f_reboot};
    app_init(&app, &h, &cfg, false, 0);
    uint32_t t = 1000;
    app_tick(&app, t);

    /* first sample is taken directly, no edge counted */
    app_set_inputs(&app, 0x01);
    CHECK_EQ(app.inputs, 0x01);
    CHECK_EQ(app.counters[0], 0);

    /* IN2 energised: reported once the level has been stable for 10 ms (11th sample) */
    hold_inputs(0x03, &t, 10);
    CHECK_EQ(app.inputs, 0x01);
    hold_inputs(0x03, &t, 1);
    CHECK_EQ(app.inputs, 0x03);
    CHECK_EQ(app.counters[1], 1);

    /* a 5 ms glitch on IN3 is filtered out */
    hold_inputs(0x07, &t, 5);
    hold_inputs(0x03, &t, 20);
    CHECK_EQ(app.inputs, 0x03);
    CHECK_EQ(app.counters[2], 0);

    /* release of IN2 is debounced too and is not counted */
    hold_inputs(0x01, &t, 11);
    CHECK_EQ(app.inputs, 0x01);
    CHECK_EQ(app.counters[1], 1);

    /* invert IN8 (normally-closed contact): opto off -> active */
    size_t n = xfer((const uint8_t[]){1, 0x06, 0x00, APP_HR_IN_INVERT, 0x00, 0x80}, 6);
    CHECK_EQ(n, 8);
    hold_inputs(0x01, &t, 11);
    CHECK_EQ(app.inputs, 0x81);
    CHECK_EQ(app.counters[7], 1);

    /* discrete inputs, input bitmap, raw bitmap */
    xfer((const uint8_t[]){1, 0x02, 0x00, 0x00, 0x00, 0x08}, 6);
    CHECK_EQ(resp[3], 0x81);
    xfer((const uint8_t[]){1, 0x04, 0x00, 0x01, 0x00, 0x01}, 6);
    CHECK_EQ(rd16(3), 0x81);
    xfer((const uint8_t[]){1, 0x04, 0x00, 18, 0x00, 0x01}, 6);
    CHECK_EQ(rd16(3), 0x01);

    /* debounce 0: immediate */
    xfer((const uint8_t[]){1, 0x06, 0x00, APP_HR_IN_DEBOUNCE, 0x00, 0x00}, 6);
    hold_inputs(0x11, &t, 1);
    CHECK_EQ(app.inputs, 0x91);
    /* debounce above 250 rejected */
    xfer((const uint8_t[]){1, 0x06, 0x00, APP_HR_IN_DEBOUNCE, 0x00, 251}, 6);
    CHECK_EQ(resp[1], 0x86);

    /* counters: read IR 10..17, then clear */
    xfer((const uint8_t[]){1, 0x04, 0x00, 10, 0x00, 0x08}, 6);
    CHECK_EQ(rd16(3 + 2 * 1), 1);
    CHECK_EQ(rd16(3 + 2 * 4), 1);
    CHECK_EQ(rd16(3 + 2 * 7), 1);
    xfer((const uint8_t[]){1, 0x06, 0x00, APP_HR_COMMAND, 0xC0, 0xDE}, 6);
    CHECK_EQ(app.counters[1], 0);
    CHECK_EQ(app.counters[7], 0);
}

void test_app(void)
{
    cfg_t cfg;
    cfg_defaults(&cfg);
    cfg.base_addr = 10;
    memset(&hw, 0, sizeof hw);
    app_hw_t h = {&hw, f_apply, f_save, f_clear, f_reboot};

    /* address = base + straps (ADDR0 fitted -> +1, ADDR1 fitted -> +2) */
    CHECK_EQ(app_address(&cfg, 0), 10);
    CHECK_EQ(app_address(&cfg, 3), 13);
    cfg.base_addr = 246;
    CHECK_EQ(app_address(&cfg, 3), 247);
    cfg.base_addr = 10;

    app_init(&app, &h, &cfg, true, 1);
    CHECK_EQ(app.address, 11);
    CHECK_EQ(hw.applies, 1);
    CHECK(hw.enabled);

    /* read input registers 0..9 */
    app_tick(&app, 5000);
    app_set_vio(&app, 12000);
    app_set_inputs(&app, 0x81);
    size_t n = xfer((const uint8_t[]){11, 0x04, 0x00, 0x00, 0x00, 0x0A}, 6);
    CHECK_EQ(n, 3 + 20 + 2);
    CHECK_EQ(rd16(3), (FW_VERSION_MAJOR << 8) | FW_VERSION_MINOR);
    CHECK_EQ(rd16(5), 0x81);
    CHECK_EQ(rd16(7), 12000);
    CHECK_EQ(rd16(13) & APP_FLAG_CFG_LOADED, APP_FLAG_CFG_LOADED);
    CHECK_EQ(rd16(15), 11);
    CHECK_EQ(rd16(17), 1);
    CHECK_EQ(rd16(21), 5);
    n = xfer((const uint8_t[]){11, 0x04, 0x00, 0x00, 0x00, APP_IR_COUNT}, 6);
    CHECK_EQ(n, 3 + 2 * APP_IR_COUNT + 2);
    n = xfer((const uint8_t[]){11, 0x04, 0x00, 0x00, 0x00, APP_IR_COUNT + 1}, 6);
    CHECK_EQ(resp[2], MB_EX_ILLEGAL_ADDRESS);

    /* configure OUT1 as push-pull and switch it on through a coil */
    xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_MODE + 0, 0x00, IO_MODE_PUSH_PULL}, 6);
    CHECK_EQ(hw.io.ch[0].mode, IO_MODE_PUSH_PULL);
    xfer((const uint8_t[]){11, 0x05, 0x00, 0x00, 0xFF, 0x00}, 6);
    CHECK_EQ(hw.io.ch[0].out, 1);
    n = xfer((const uint8_t[]){11, 0x03, 0x00, APP_HR_OUT, 0x00, 0x01}, 6);
    CHECK_EQ(rd16(3), 1);
    n = xfer((const uint8_t[]){11, 0x01, 0x00, 0x00, 0x00, 0x08}, 6);
    CHECK_EQ(resp[3], 0x01);

    /* PWM on OUT2: high-side, gen 3; HR 26..31 = duty3 100, duty4 0, freq1 0, freq2 0, freq3 2, freq4 3 */
    xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_MODE + 1, 0x00, IO_MODE_HIGH_SIDE}, 6);
    xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_PWM + 1, 0x00, 3}, 6);
    xfer((const uint8_t[]){11, 0x10, 0x00, APP_HR_DUTY + 2, 0x00, 0x06, 0x0C,
                           0x00, 100, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x03}, 19);
    CHECK_EQ(hw.io.duty[2], 100);
    CHECK_EQ(hw.io.freq[2], IO_PWM_200HZ);
    CHECK_EQ(hw.io.freq[3], IO_PWM_2KHZ);
    CHECK_EQ(hw.io.ch[1].pwm, 3);

    /* invalid values -> exception 03, nothing changed (all-or-nothing) */
    int before = hw.applies;
    n = xfer((const uint8_t[]){11, 0x10, 0x00, APP_HR_MODE, 0x00, 0x02, 0x04, 0x00, 0x01, 0x00, 0x09}, 11);
    CHECK_EQ(n, 5);
    CHECK_EQ(resp[1], 0x90);
    CHECK_EQ(resp[2], MB_EX_ILLEGAL_VALUE);
    CHECK_EQ(hw.applies, before);
    CHECK_EQ(app.cfg.io.ch[0].mode, IO_MODE_PUSH_PULL);
    n = xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_BASE_ADDR, 0x00, 0x00}, 6); /* address 0 */
    CHECK_EQ(resp[2], MB_EX_ILLEGAL_VALUE);
    n = xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_COMMAND, 0x12, 0x34}, 6); /* unknown command */
    CHECK_EQ(resp[2], MB_EX_ILLEGAL_VALUE);
    n = xfer((const uint8_t[]){11, 0x03, 0x00, APP_HR_COUNT, 0x00, 0x01}, 6); /* past the end */
    CHECK_EQ(resp[2], MB_EX_ILLEGAL_ADDRESS);
    n = xfer((const uint8_t[]){11, 0x05, 0x00, 0x08, 0xFF, 0x00}, 6); /* coil 8 does not exist */
    CHECK_EQ(resp[2], MB_EX_ILLEGAL_ADDRESS);

    /* comm settings + save */
    xfer((const uint8_t[]){11, 0x10, 0x00, APP_HR_BASE_ADDR, 0x00, 0x03, 0x06, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00},
         13);
    CHECK_EQ(app.cfg.base_addr, 0x20);
    CHECK_EQ(app.cfg.baud, CFG_BAUD_115200);
    CHECK_EQ(app.cfg.parity, CFG_PARITY_NONE);
    CHECK_EQ(app.address, 11); /* unchanged until reboot */
    xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_IN_INVERT, 0x00, 0x0F}, 6);
    xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_COMMAND, 0xA5, 0x5A}, 6);
    CHECK_EQ(hw.saves, 1);
    CHECK_EQ(hw.saved.base_addr, 0x20);
    CHECK_EQ(hw.saved.in_invert, 0x0F);
    CHECK_EQ(hw.saved.io.ch[0].mode, IO_MODE_PUSH_PULL);
    n = xfer((const uint8_t[]){11, 0x03, 0x00, APP_HR_IN_INVERT, 0x00, 0x06}, 6);
    CHECK_EQ(rd16(3), 0x0F);
    CHECK_EQ(rd16(5), 10);   /* debounce default */
    CHECK_EQ(rd16(7), 0x20); /* base address */
    hw.save_result = -1;
    xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_COMMAND, 0xA5, 0x5A}, 6);
    CHECK(app.flags & APP_FLAG_EEPROM_ERROR);
    hw.save_result = 0;

    /* clear faults, reboot after response */
    xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_COMMAND, 0xC1, 0xF1}, 6);
    CHECK_EQ(hw.clears, 1);
    n = xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_COMMAND, 0xB0, 0x07}, 6);
    CHECK_EQ(n, 8);
    CHECK_EQ(hw.reboots, 0);
    app_tick(&app, 5001);
    CHECK_EQ(hw.reboots, 1);
    app.reboot_pending = false;

    /* comm watchdog: 200 ms timeout */
    xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_TIMEOUT, 0x00, 200}, 6); /* at t = 5001 */
    app_tick(&app, 5100);
    CHECK(hw.enabled);
    app_tick(&app, 5300);
    CHECK(!hw.enabled);
    CHECK(app.flags & APP_FLAG_COMM_TIMEOUT);
    xfer((const uint8_t[]){12, 0x03, 0x00, 0x00, 0x00, 0x01}, 6); /* other address: no effect */
    CHECK(!hw.enabled);
    xfer((const uint8_t[]){11, 0x03, 0x00, 0x00, 0x00, 0x01}, 6);
    CHECK(hw.enabled);
    CHECK(!(app.flags & APP_FLAG_COMM_TIMEOUT));

    /* driver status flags */
    app_set_driver_status(&app, 0x08, 0x02, 0x00, true);
    CHECK(app.flags & APP_FLAG_DRV_FAULT);
    app_set_driver_status(&app, 0x00, 0x00, 0x00, false);
    CHECK(!(app.flags & APP_FLAG_DRV_FAULT));
    CHECK(app.flags & APP_FLAG_DRV_MISSING);

    /* factory defaults command resets the outputs (not saved) */
    xfer((const uint8_t[]){11, 0x06, 0x00, APP_HR_COMMAND, 0x5A, 0xA5}, 6);
    CHECK_EQ(hw.io.ch[0].mode, IO_MODE_OFF);
    CHECK_EQ(app.cfg.base_addr, 1);
    CHECK_EQ(app.cfg.in_invert, 0);

    test_inputs();
}
