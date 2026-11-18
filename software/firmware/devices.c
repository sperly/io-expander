#include "devices.h"

#include "drv8912_regs.h"
#include "platform.h"

/* ================================================================ DRV8912-Q1 */
static uint16_t drv_xfer(uint16_t word, uint8_t *status)
{
    uint8_t tx[2] = {(uint8_t)(word >> 8), (uint8_t)word}, rx[2];
    spi_transfer(SPI_DEV_DRV, tx, rx, 2);
    if (status)
        *status = rx[0]; /* 1 1 OT OLD OCP UVLO OVP NPOR */
    return rx[1];
}

uint8_t drv_read(uint8_t addr, uint8_t *status) { return (uint8_t)drv_xfer(DRV_SPI_READ(addr), status); }

void drv_write(uint8_t addr, uint8_t val) { (void)drv_xfer(DRV_SPI_WRITE(addr, val), NULL); }

bool drv_start(void)
{
    sys_drv_sleep(false);
    sys_delay_ms(2); /* tWAKE 200 us, tREADY 1 ms */
    drv_reg_t init[IO_MODEL_INIT_REG_COUNT];
    size_t n = io_model_init_registers(init);
    for (size_t i = 0; i < n; i++)
        drv_write(init[i].addr, init[i].val);
    uint8_t id = (uint8_t)((drv_read(DRV_CONFIG_CTRL, NULL) & DRV_CFG_IC_ID_MASK) >> DRV_CFG_IC_ID_SHIFT);
    return id == DRV_IC_ID_DRV8912;
}

void drv_apply(const io_config_t *io, bool enabled)
{
    drv_reg_t regs[IO_MODEL_REG_COUNT];
    size_t n = io_model_registers(io, enabled, regs);
    for (size_t i = 0; i < n; i++)
        drv_write(regs[i].addr, regs[i].val);
}

void drv_clear_faults(void) { drv_write(DRV_CONFIG_CTRL, DRV_CFG_CLR_FLT); }

/* ================================================================== MCP23S08 */
#define EXP_OPCODE_W 0x40u /* 0100 A1 A0 R/W, A1 = A0 = 0 */
#define EXP_OPCODE_R 0x41u
#define EXP_IODIR 0x00u
#define EXP_IPOL 0x01u
#define EXP_GPINTEN 0x02u
#define EXP_INTCON 0x04u
#define EXP_IOCON 0x05u
#define EXP_GPPU 0x06u
#define EXP_GPIO 0x09u
#define EXP_IOCON_ODR (1u << 2)

static void exp_write(uint8_t reg, uint8_t val)
{
    uint8_t tx[3] = {EXP_OPCODE_W, reg, val};
    spi_transfer(SPI_DEV_EXP, tx, NULL, 3);
}

static uint8_t exp_read(uint8_t reg)
{
    uint8_t tx[3] = {EXP_OPCODE_R, reg, 0}, rx[3];
    spi_transfer(SPI_DEV_EXP, tx, rx, 3);
    return rx[2];
}

void exp_init(void)
{
    exp_write(EXP_IOCON, EXP_IOCON_ODR); /* INT open-drain (external pull-up R) */
    exp_write(EXP_IODIR, 0xFF);          /* all inputs, always */
    exp_write(EXP_IPOL, 0xFF);           /* energised opto pulls GPx low: GPIO now reads 1 = energised */
    exp_write(EXP_GPPU, 0x00);           /* 22k collector pull-ups are on the board */
    exp_write(EXP_INTCON, 0x00);         /* interrupt on any change */
    exp_write(EXP_GPINTEN, 0x00);
    (void)exp_read(EXP_GPIO);            /* clear pending interrupt */
}

uint8_t exp_read_levels(void) { return exp_read(EXP_GPIO); }

void exp_set_irq_mask(uint8_t mask) { exp_write(EXP_GPINTEN, mask); }

/* =================================================================== 24AA02 */
#define EE_ADDR 0x50u
#define EE_PAGE 8u

int eeprom_read(void *ctx, uint16_t offset, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    if (offset + len > 256u)
        return -1;
    uint8_t a = (uint8_t)offset;
    return i2c_write_read(EE_ADDR, &a, 1, buf, len);
}

int eeprom_write(void *ctx, uint16_t offset, const uint8_t *buf, uint16_t len)
{
    (void)ctx;
    if (offset + len > 256u)
        return -1;
    while (len) {
        uint16_t chunk = (uint16_t)(EE_PAGE - (offset % EE_PAGE));
        if (chunk > len)
            chunk = len;
        uint8_t tmp[1 + EE_PAGE];
        tmp[0] = (uint8_t)offset;
        for (uint16_t i = 0; i < chunk; i++)
            tmp[1 + i] = buf[i];
        if (i2c_write(EE_ADDR, tmp, 1u + chunk))
            return -2;
        sys_delay_ms(6); /* tWC 5 ms */
        sys_watchdog_kick();
        offset = (uint16_t)(offset + chunk);
        buf += chunk;
        len = (uint16_t)(len - chunk);
    }
    return 0;
}
