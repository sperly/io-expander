/*
 * DRV8912-Q1 register map (TI SLVSEC9C, section 8.6.1, Table 17).
 * Only the DRV8912-Q1 / DRV8910-Q1 map is valid here - the DRV8908 uses
 * different addresses from 0x0F upwards.
 */
#ifndef DRV8912_REGS_H
#define DRV8912_REGS_H

/* status (read only) */
#define DRV_IC_STAT        0x00u
#define DRV_OCP_STAT_1     0x01u /* HB1..HB4 HS/LS */
#define DRV_OCP_STAT_2     0x02u /* HB5..HB8 */
#define DRV_OCP_STAT_3     0x03u /* HB9..HB12 */
#define DRV_OLD_STAT_1     0x04u
#define DRV_OLD_STAT_2     0x05u
#define DRV_OLD_STAT_3     0x06u
/* control */
#define DRV_CONFIG_CTRL    0x07u
#define DRV_OP_CTRL_1      0x08u /* bit 2n = HBn+1 LS_EN, bit 2n+1 = HS_EN, HB1..HB4 */
#define DRV_OP_CTRL_2      0x09u /* HB5..HB8 */
#define DRV_OP_CTRL_3      0x0Au /* HB9..HB12 */
#define DRV_PWM_CTRL_1     0x0Bu /* HBn_PWM, HB1..HB8 */
#define DRV_PWM_CTRL_2     0x0Cu /* [7:4] PWM_CH4..1_DIS, [3:0] HB12..HB9_PWM */
#define DRV_FW_CTRL_1      0x0Du /* HBn_FW (synchronous rectification), HB1..HB8 */
#define DRV_FW_CTRL_2      0x0Eu
#define DRV_PWM_MAP_CTRL_1 0x0Fu /* 2 bits per half-bridge, HB1..HB4 */
#define DRV_PWM_MAP_CTRL_2 0x10u /* HB5..HB8 */
#define DRV_PWM_MAP_CTRL_3 0x11u /* HB9..HB12 */
#define DRV_PWM_FREQ_CTRL  0x12u /* 2 bits per generator, CH1 in [1:0] */
#define DRV_PWM_DUTY_CTRL_1 0x13u /* 0..255 = 0..100 % */
#define DRV_PWM_DUTY_CTRL_2 0x14u
#define DRV_PWM_DUTY_CTRL_3 0x15u
#define DRV_PWM_DUTY_CTRL_4 0x16u
#define DRV_SR_CTRL_1      0x17u /* 0 = 0.6 V/us, 1 = 2.5 V/us */
#define DRV_SR_CTRL_2      0x18u
#define DRV_OLD_CTRL_1     0x19u /* HBn_OLD_DIS, HB1..HB8 */
#define DRV_OLD_CTRL_2     0x1Au /* OLD_REP, OLD_OP, PL_MODE_EN, HB12..9_OLD_DIS */
#define DRV_OLD_CTRL_3     0x1Bu /* OCP_DEG, OLD_NEG_EN, HB12..9_LOLD_EN */
#define DRV_OLD_CTRL_4     0x24u /* HB8..1_LOLD_EN */

/* IC_STAT bits */
#define DRV_STAT_OTSD (1u << 6)
#define DRV_STAT_OTW  (1u << 5)
#define DRV_STAT_OLD  (1u << 4)
#define DRV_STAT_OCP  (1u << 3)
#define DRV_STAT_UVLO (1u << 2)
#define DRV_STAT_OVP  (1u << 1)
#define DRV_STAT_NPOR (1u << 0)

/* CONFIG_CTRL bits */
#define DRV_CFG_IC_ID_SHIFT 4u
#define DRV_CFG_IC_ID_MASK  (7u << 4)
#define DRV_IC_ID_DRV8912   0u
#define DRV_CFG_OCP_REP     (1u << 3)
#define DRV_CFG_OTW_REP     (1u << 2)
#define DRV_CFG_EXT_OVP     (1u << 1)
#define DRV_CFG_CLR_FLT     (1u << 0)

/* OLD_CTRL_2 bits */
#define DRV_OLD2_OLD_REP (1u << 7) /* 1 = open load not reported on nFAULT */
#define DRV_OLD2_OLD_OP  (1u << 6) /* 1 = half-bridges stay active after OLD */

/* SPI frame: B14 = 1 for read, B13..B8 address, B7..B0 data */
#define DRV_SPI_READ(addr)       ((uint16_t)(0x4000u | (((addr) & 0x3Fu) << 8)))
#define DRV_SPI_WRITE(addr, val) ((uint16_t)((((addr) & 0x3Fu) << 8) | ((val) & 0xFFu)))

#endif
