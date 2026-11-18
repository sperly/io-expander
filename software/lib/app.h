/*
 * Application core: Modbus register map, outputs, inputs, comm watchdog.
 * Hardware access goes through app_hw_t, so this module runs unchanged in
 * the host unit tests.
 *
 * ---------------------------------------------------------------- register map
 * Coils (01/05/15)            0..7    output value OUT1..OUT8 (same as HR 8..15)
 * Discrete inputs (02)        0..7    input IN1..IN8 (1 = active, after invert + debounce)
 *
 * Holding registers (03/06/16)
 *   0..7    output mode OUT1..8   0 off, 1 low-side, 2 high-side, 3 push-pull
 *   8..15   output value OUT1..8  0 / 1
 *   16..23  PWM gen OUT1..8       0 = none, 1..4 = PWM generator (applies while value = 1)
 *   24..27  PWM duty gen 1..4     0..255 (= 0..100 %)
 *   28..31  PWM freq gen 1..4     0 = 80 Hz, 1 = 100 Hz, 2 = 200 Hz, 3 = 2 kHz
 *   32      input invert mask     bit n: IN(n+1) reads 1 when NOT energised (NC contacts)
 *   33      input debounce ms     0..250
 *   34      base address          1..247 (address = base + strap offset)  *
 *   35      baud rate             0 9600, 1 19200, 2 38400, 3 57600, 4 115200 *
 *   36      parity                0 none, 1 even, 2 odd                    *
 *   37      comm timeout ms       0 = off; outputs switch off when no valid
 *                                 request arrives within this time
 *   38      command (write)       0xA55A save configuration to EEPROM
 *                                 0x5AA5 load factory defaults (not saved)
 *                                 0xC1F1 clear driver faults
 *                                 0xB007 reboot
 *                                 0xC0DE clear the input counters
 *   * takes effect after save + reboot
 *
 * Input registers (04)
 *   0  firmware version (major << 8 | minor)
 *   1  input bitmap (bit n = IN(n+1) active)
 *   2  VIO in mV
 *   3  DRV8912 IC_STAT
 *   4  DRV8912 OCP_STAT_1 | OCP_STAT_2 << 8
 *   5  status flags (see APP_FLAG_x)
 *   6  Modbus address in use
 *   7  strap value (bit0 ADDR0 fitted, bit1 ADDR1 fitted)
 *   8  uptime seconds, high word
 *   9  uptime seconds, low word
 *   10..17 activation counter IN1..IN8 (inactive -> active edges, wraps at 65535)
 *   18 raw opto bitmap (before invert and debounce)
 */
#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "io_model.h"
#include "modbus_rtu.h"

#define FW_VERSION_MAJOR 2
#define FW_VERSION_MINOR 0

#define APP_INPUTS 8u

#define APP_HR_MODE 0u
#define APP_HR_OUT 8u
#define APP_HR_PWM 16u
#define APP_HR_DUTY 24u
#define APP_HR_FREQ 28u
#define APP_HR_IN_INVERT 32u
#define APP_HR_IN_DEBOUNCE 33u
#define APP_HR_BASE_ADDR 34u
#define APP_HR_BAUD 35u
#define APP_HR_PARITY 36u
#define APP_HR_TIMEOUT 37u
#define APP_HR_COMMAND 38u
#define APP_HR_COUNT 39u

#define APP_IR_COUNT 19u

#define APP_CMD_SAVE 0xA55Au
#define APP_CMD_DEFAULTS 0x5AA5u
#define APP_CMD_CLEAR_FAULTS 0xC1F1u
#define APP_CMD_REBOOT 0xB007u
#define APP_CMD_CLEAR_COUNTERS 0xC0DEu

#define APP_FLAG_COMM_TIMEOUT (1u << 0) /* outputs forced off by the comm watchdog */
#define APP_FLAG_DRV_FAULT (1u << 1)    /* DRV8912 reports OCP / OTSD / UVLO / OVP */
#define APP_FLAG_CFG_LOADED (1u << 2)   /* configuration came from EEPROM */
#define APP_FLAG_EEPROM_ERROR (1u << 3) /* last save failed */
#define APP_FLAG_DRV_MISSING (1u << 4)  /* driver did not answer with the DRV8912 ID */

typedef struct {
    void *ctx;
    void (*apply_outputs)(void *ctx, const io_config_t *io, bool enabled); /* push to DRV8912 */
    int (*save_config)(void *ctx, const cfg_t *cfg);                       /* 0 = ok */
    void (*clear_faults)(void *ctx);
    void (*reboot)(void *ctx);
} app_hw_t;

typedef struct {
    app_hw_t hw;
    cfg_t cfg;        /* configuration image (saved on APP_CMD_SAVE) */
    uint8_t address;  /* Modbus address in use */
    uint8_t straps;
    bool outputs_enabled;
    uint32_t last_rx_ms;
    uint32_t now_ms;
    /* inputs */
    bool inputs_valid;
    uint8_t in_raw;       /* last raw opto sample (1 = energised) */
    uint8_t in_candidate; /* inverted sample waiting for debounce */
    uint32_t in_since[APP_INPUTS];
    uint8_t inputs;       /* debounced, inverted: 1 = active */
    uint16_t counters[APP_INPUTS];
    /* status */
    uint16_t vio_mv;
    uint8_t drv_stat, drv_ocp1, drv_ocp2;
    uint16_t flags;
    bool reboot_pending;
    modbus_map_t map;
} app_t;

void app_init(app_t *a, const app_hw_t *hw, const cfg_t *cfg, bool cfg_loaded, uint8_t straps);
uint8_t app_address(const cfg_t *cfg, uint8_t straps);

/* Feed a received RTU frame; returns the response length in resp (0 = none). */
size_t app_handle_frame(app_t *a, const uint8_t *req, size_t len, uint8_t *resp);

void app_tick(app_t *a, uint32_t now_ms);
/* raw = opto states, bit n = IN(n+1) energised. Call at least every millisecond. */
void app_set_inputs(app_t *a, uint8_t raw);
void app_set_vio(app_t *a, uint16_t mv);
void app_set_driver_status(app_t *a, uint8_t ic_stat, uint8_t ocp1, uint8_t ocp2, bool present);

#endif
