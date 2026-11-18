/*
 * Model of the 8 outputs and its translation into DRV8912-Q1 register
 * values. Pure logic, no hardware access (host-testable).
 *
 * Output terminal OUTn is wired to DRV8912 OUTn (half-bridge n).
 */
#ifndef IO_MODEL_H
#define IO_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IO_CHANNELS   8u
#define IO_PWM_GENS   4u /* the DRV8912 has four PWM generators */

typedef enum {
    IO_MODE_OFF = 0,       /* both switches off (high impedance)              */
    IO_MODE_LOW_SIDE = 1,  /* out = 1: low-side switch on (sinks to GND)     */
    IO_MODE_HIGH_SIDE = 2, /* out = 1: high-side switch on (sources VIO)     */
    IO_MODE_PUSH_PULL = 3, /* out = 1: drives VIO, out = 0: drives GND       */
    IO_MODE_COUNT
} io_mode_t;

typedef enum { IO_PWM_80HZ = 0, IO_PWM_100HZ = 1, IO_PWM_200HZ = 2, IO_PWM_2KHZ = 3 } io_pwm_freq_t;

typedef struct {
    uint8_t mode; /* io_mode_t */
    uint8_t out;  /* 0 / 1 */
    uint8_t pwm;  /* 0 = no PWM, 1..4 = PWM generator used while out = 1 */
} io_channel_t;

typedef struct {
    io_channel_t ch[IO_CHANNELS];
    uint8_t duty[IO_PWM_GENS]; /* 0..255 = 0..100 % */
    uint8_t freq[IO_PWM_GENS]; /* io_pwm_freq_t */
} io_config_t;

typedef struct {
    uint8_t addr;
    uint8_t val;
} drv_reg_t;

/* Number of entries produced by io_model_registers(). */
#define IO_MODEL_REG_COUNT 16u

void io_config_defaults(io_config_t *cfg);
bool io_channel_valid(const io_channel_t *ch);

/*
 * Build the DRV8912 register image for the configuration. With
 * outputs_enabled == false every switch is turned off (fail-safe), the PWM
 * settings are kept. Registers are listed in a safe write order (PWM set-up
 * first, switch enables last). Returns the number of entries written.
 */
size_t io_model_registers(const io_config_t *cfg, bool outputs_enabled, drv_reg_t out[IO_MODEL_REG_COUNT]);

/* Static register set-up done once after the driver wakes up. */
#define IO_MODEL_INIT_REG_COUNT 8u
size_t io_model_init_registers(drv_reg_t out[IO_MODEL_INIT_REG_COUNT]);

#endif
