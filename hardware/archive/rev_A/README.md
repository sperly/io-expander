# RS485_IO8 — revision A

Six-sheet engineering schematic for eight individually configurable 5 V / 12 V logic I/Os, controlled over half-duplex RS-485. Open `RS485_IO8.kicad_pro` in KiCad 9 or 10. All custom symbols are included in `IO8.kicad_sym`; the project uses standard KiCad footprints.

## Hardware

- U2: STM32G030F6P6, 32 KB flash, TSSOP-20. Internal clock; no crystal required.
- U4: THVD1410D, 3.3 V RS-485 transceiver.
- U5: DRV8908QPWPRQ1, eight software-controlled half-bridge outputs, with eight internal PWM generators.
- U6: MCP23S08-E/SS, SPI digital input expander, SSOP-20.
- U3: 24AA02T-I/OT, 256-byte configuration EEPROM, SOT-23-5.
- U1: AP2112K-3.3TRG1, 5 V to 3.3 V regulator.
- Q1–Q8: BC847B input sensing stages. These invert the external signal before it reaches the expander. Each external terminal has an independent readback path.

The DRV8908 outputs connect directly to J5: there are **no series resistors in any output-to-terminal path**. Input sensing components are on separate branches. The expander is always used as an input device; the driver selects whether a terminal is an input or an output.

## Connectors

| Connector | Pins |
|---|---|
| J1, regulated logic power | 1 = +5 V; 2 = GND |
| J2, external output supply | 1 = EXT_VIO; 2 = GND |
| J3, standard Cortex SWD | See pin table below |
| J4, RS-485 | 1 = A (noninverting); 2 = B (inverting); 3 = GND |
| J5, external I/O | 1 = selected VIO; 2 = GND; 3–10 = IO1–IO8 |
| J6, driver fault test header | 1 = active-low nFAULT; 2 = GND |

J3 is a **2×5, 1.27 mm** ARM Cortex debug header. Pin 7 is the mechanical key position: omit that physical pin if a blocked-key cable is used. The footprint supplied is a generic double-row header; select the desired keyed/shrouded connector during PCB layout.

| J3 pin | Signal | J3 pin | Signal |
|---|---|---|---|
| 1 | VTref, 3.3 V sense | 2 | SWDIO |
| 3 | GND | 4 | SWCLK |
| 5 | GND | 6 | NC — SWO not used |
| 7 | KEY / NC | 8 | NC |
| 9 | GNDDetect, connected to GND | 10 | nRESET |

VTref is a voltage reference for the probe, not a board power input. Power the board through J1.

## Supply and strap settings

Supply J1 with regulated 5 V (nominally 4.75–5.25 V). J2 supplies only the output domain; it does not power the MCU. The intended selected VIO range for this design is 4.5–14 V, covering regulated 5 V and nominal 12 V systems. All channels share the selected VIO and ground.

| Jumper | Setting |
|---|---|
| JP1 pins 1–2 | Select onboard +5 V as VIO |
| JP1 pins 2–3 | Select external J2 supply as VIO |
| JP2 | Ground ADDR0 when fitted |
| JP3 | Ground ADDR1 when fitted |
| JP4 | Enable the 120 Ω RS-485 termination |
| JP5 and JP6 | Fit both to enable this node's bus bias network |

Use only one shunt on JP1; move it with power removed. Enable bus termination at the two physical bus ends. Enable bus bias at one node only, normally the host. A/B labels differ between equipment vendors, so verify polarity against the host.

| JP3 / ADDR1 | JP2 / ADDR0 | Proposed software address |
|---|---|---|
| Open | Open | 1 |
| Open | Fitted | 2 |
| Fitted | Open | 3 |
| Fitted | Fitted | 4 |

Read straps during startup. Their mapping is defined in firmware and can be changed.

## External signal behavior

Each output may be disabled, used as a low-side switch, used as a high-side switch, or driven high/low. PWM is generated inside U5 at nominal 2 kHz with 8-bit duty control. Input mode means both output switches are disabled; the input sensing branch remains connected. Released high-side/low-side outputs may require an external bias resistor supplied by the attached equipment.

The transistor sensing stages target LOW = 0–0.8 V and HIGH = 3.5 V to selected VIO. They have an approximate switching region near 2 V, dependent on temperature and transistor characteristics; this is not a precision-threshold industrial input. These targets require prototype validation. Input loading is approximately 0.24 mA at 12 V, and an undriven input is weakly biased low.

External inputs must stay within GND to the **selected VIO**, including when the output driver is disabled. For example, do not apply a 12 V signal with VIO selected to 5 V. Output body diodes remain connected in input mode. Apply the 5 V logic supply and selected VIO before external driven signals; this schematic does not provide power-off isolation, galvanic isolation, reverse polarity protection or automotive load-dump protection.

Input readback is digital through U6, not eight ADC voltage measurements. Interrupt capture records a port state, not a queue of edges; high-rate pulse counting or simultaneous PWM capture is not guaranteed. PWM **generation** is independent of input polling.

## MCU pin assignment

| Physical TSSOP pin | MCU signal selected | Function |
|---|---|---|
| 1 | PB7, AF6 | I2C1 SDA, EEPROM |
| 2 | PC14, input | ADDR0 |
| 3 | PC15, input | ADDR1 |
| 4 | VDD / VDDA | 3.3 V |
| 5 | VSS / VSSA | GND |
| 6 | NRST | Reset; also resets U6 |
| 7 | PA0, ADC_IN0 | Selected VIO monitor |
| 8 | PA1, AF0 | SPI1 SCK |
| 9 | PA2, AF0 | SPI1 MOSI |
| 10 | PA3, EXTI3 | Input expander IRQ |
| 11 | PA4, GPIO | Driver nCS |
| 12 | PA5, GPIO | Input expander nCS |
| 13 | PA6, AF0 | SPI1 MISO |
| 14 | PA7, GPIO | Driver nSLEEP |
| 15 | PB0, GPIO | RS-485 DE and nRE |
| 16 | PA9, AF1, remapped | USART1 TX |
| 17 | PA10, AF1, remapped | USART1 RX |
| 18 | PA13 | SWDIO |
| 19 | PA14 | SWCLK |
| 20 | PB6, AF6 | I2C1 SCL, EEPROM |

**Package-specific firmware setup is essential:** configure the SYSCFG PA11/PA12 remap to expose PA9/PA10 on pins 16/17. The TSSOP package bonds several GPIOs to common pads: keep every unused GPIO sharing these pads in analog mode. Pin 2 uses PC14, not PB9; leave PB9 inactive. Do not enable LSE on PC14/PC15. Keep PA15 inactive so SWCLK on PA14 remains usable. Preserve the default SWD function on PA13/PA14. See STM32 datasheet DS12991, package pinout and alternate-function tables.

VIO monitor: `VIO_ADC = VIO × 22/122`. Use a long ADC acquisition time for the 100 kΩ / 22 kΩ divider and 1 nF filter; it is only a slow supply monitor. D1 provides rail clamps, not power-off isolation.

## Firmware requirements

1. Start with driver nSLEEP low, both SPI chip selects high and RS-485 TXEN low. Load and validate EEPROM configuration and read address straps before enabling outputs.
2. Use SPI1 with separate CS pins. Start conservatively at 1 MHz. Change SPI mode with **both devices deselected**: U5 captures on falling SCLK edges (mode 1 with clock idling low); U6 supports mode 0. Complete a transaction before selecting the other device.
3. Keep all U6 GP pins configured as inputs. Invert their readings in software, or configure the polarity inversion register. Enable interrupt-on-change only for channels used as inputs; mask outputs/PWM channels to avoid interrupt flooding.
4. Wake U5 and wait its datasheet startup interval. Disable active and passive open-load detection before enabling logic outputs. Select output modes and independent PWM settings. For complementary high/low PWM use the driver's freewheeling control; do not enable both switches continuously.
5. Poll driver fault/status registers over SPI; the separate nFAULT test header is not connected to an MCU GPIO. On bus timeout, invalid configuration or software fault, disable outputs or pull nSLEEP low.
6. Wait for USART transmission-complete before lowering RS-485 TXEN. The receiver is disabled while transmitting.
7. Store configuration only when it changes. Suggested EEPROM format: two 128-byte slots, each with version, sequence counter, payload length and checksum. Retain the last valid copy if power fails during an update.

No application firmware is included. The RS-485 packet protocol (for example Modbus RTU) remains a firmware choice.

## Validation and next steps

KiCad 9.0.2 exports all six sheets to PDF and reports **zero ERC errors or warnings**. The exported netlist was checked against device pin assignments, direct driver-to-terminal connections, all eight transistor sensing circuits and the Cortex SWD pinout. Every assigned footprint exists in the installed standard library. Details are in `VALIDATION.txt` and `ERC.json`.

This is an engineering schematic, not a fabricated or hardware-tested design. Before PCB release, select the actual connector parts, check U5's exposed-pad pattern against the ordered TI PWP package variant, verify capacitor effective capacitance under DC bias, and validate input thresholds, startup sequencing and output behavior on a prototype. The output driver's protection does not limit fault current to a logic-safe value; externally connected equipment determines normal loading and any additional protection requirements.

## Source documents

- STM32G030x6/x8, ST DS12991 Rev 6: https://www.st.com/resource/en/datasheet/stm32g030f6.pdf
- TI DRV89xx-Q1: https://www.ti.com/lit/ds/symlink/drv8908-q1.pdf
- TI THVD14xx: https://www.ti.com/product/THVD1410
- Microchip MCP23008/MCP23S08: https://ww1.microchip.com/downloads/en/DeviceDoc/21919e.pdf
- Microchip 24AA02: https://www.microchip.com/en-us/product/24AA02
- Diodes AP2112: https://www.diodes.com/assets/Datasheets/AP2112.pdf
- Nexperia BC847: https://assets.nexperia.com/documents/data-sheet/BC847_SER.pdf
- ARM Cortex debug connector: https://documentation-service.arm.com/static/5fce6c49e167456a35b36af1
