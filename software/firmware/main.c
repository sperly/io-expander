/*
 * RS485_IO8 rev B firmware - STM32G030F6P6.
 *
 * Start-up sequence (outputs can never glitch on):
 *   1. GPIO safe states: driver asleep (nSLEEP low), chip selects high, DE low.
 *   2. Load configuration from the EEPROM (defaults if none valid), read straps.
 *   3. Wake the DRV8912, disable open-load detection, clear power-on flags.
 *   4. Initialise the MCP23S08 (8 opto inputs, interrupt on change).
 *   5. Apply the stored output configuration, start Modbus RTU and the watchdog.
 */
#include "app.h"
#include "board.h"
#include "config.h"
#include "devices.h"
#include "drv8912_regs.h"
#include "platform.h"

#define INPUT_POLL_MS 1u /* debounce runs in app_set_inputs() */
#define STATUS_POLL_MS 50u

static app_t s_app;
static uint8_t s_resp[MODBUS_MAX_ADU];
static bool s_drv_ok;

static const cfg_storage_t s_storage = {NULL, eeprom_read, eeprom_write};

/* ------------------------------------------------------------- app_hw_t */
static void hw_apply(void *ctx, const io_config_t *io, bool enabled)
{
    (void)ctx;
    drv_apply(io, enabled);
}

static int hw_save(void *ctx, const cfg_t *cfg)
{
    (void)ctx;
    return cfg_save(&s_storage, cfg);
}

static void hw_clear_faults(void *ctx)
{
    (void)ctx;
    drv_clear_faults();
}

static void hw_reboot(void *ctx)
{
    (void)ctx;
    while (uart_tx_busy()) {
    }
    sys_reboot();
}

static const app_hw_t s_hw = {NULL, hw_apply, hw_save, hw_clear_faults, hw_reboot};

/* ------------------------------------------------------------------ main */
int main(void)
{
    sys_init();
    i2c_init();
    spi_init();
    adc_init();

    cfg_t cfg;
    bool loaded = cfg_load(&s_storage, &cfg);
    uint8_t straps = sys_read_straps();

    s_drv_ok = drv_start();
    exp_init();
    exp_set_irq_mask(0xFF); /* all eight opto inputs */

    app_init(&s_app, &s_hw, &cfg, loaded, straps); /* applies the channel configuration */
    app_set_inputs(&s_app, exp_read_levels());
    uart_init(cfg_baud_rate(cfg.baud), cfg.parity);
    sys_watchdog_start();

    uint32_t t_in = 0, t_stat = 0;
    for (;;) {
        uint32_t now = sys_millis();

        const uint8_t *frame;
        size_t len = uart_frame(&frame);
        if (len) {
            size_t n = app_handle_frame(&s_app, frame, len, s_resp);
            uart_release_frame();
            if (n)
                uart_send(s_resp, n);
        }

        if (sys_take_input_irq() || (uint32_t)(now - t_in) >= INPUT_POLL_MS) {
            t_in = now;
            app_set_inputs(&s_app, exp_read_levels());
        }

        if ((uint32_t)(now - t_stat) >= STATUS_POLL_MS) {
            t_stat = now;
            uint8_t st = 0;
            uint8_t ocp1 = drv_read(DRV_OCP_STAT_1, &st);
            uint8_t ocp2 = drv_read(DRV_OCP_STAT_2, NULL);
            uint8_t ic = drv_read(DRV_IC_STAT, NULL);
            /* the status byte of a healthy device starts with 0b11 */
            app_set_driver_status(&s_app, ic, ocp1, ocp2, s_drv_ok && (st & 0xC0u) == 0xC0u);
            app_set_vio(&s_app, adc_read_vio_mv());
        }

        app_tick(&s_app, now);
        sys_watchdog_kick();
        __WFI(); /* SysTick or UART wakes us at least every millisecond */
    }
}
