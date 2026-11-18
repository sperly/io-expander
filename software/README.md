# RS485_IO8 firmware

Firmware for the RS485_IO8 rev B board (`../hardware`): STM32G030F6P6, MAX3160
RS-485 / RS-232 transceiver, DRV8912-Q1 driver for the **8 outputs**,
TLP290-4 optocouplers + MCP23S08 expander for the **8 isolated inputs**, and a
24AA02 configuration EEPROM. The board is a **Modbus RTU slave**.

Register-level C11 on the ST CMSIS headers. It doesn't use the HAL. The
portable part (`lib/`) is unit-tested on the host.

## Build

Requirements: CMake ≥ 3.20, Ninja, `arm-none-eabi-gcc` with newlib-nano, and
the ST CMSIS headers.

```bash
cmake --preset firmware          # finds ~/STM32Cube/Repository/STM32Cube_FW_G0_V* automatically
cmake --build --preset firmware  # -> build/firmware/io8_fw.elf / .hex / .bin
cmake --build --preset firmware --target flash   # OpenOCD + ST-LINK (or STM32_Programmer_CLI)
```

If the STM32CubeG0 package is somewhere else:

```bash
cmake --preset firmware -DSTM32CUBE_G0_PATH=/path/to/STM32Cube_FW_G0_V1.6.3
# or, with git clones of STMicroelectronics/cmsis-device-g0 and cmsis-core:
cmake --preset firmware -DCMSIS_DEVICE_G0_INCLUDE=.../cmsis-device-g0/Include \
                        -DCMSIS_CORE_INCLUDE=.../cmsis-core/Include
```

Host unit tests (Modbus engine, CRC, EEPROM slot logic, DRV8912 register
encoding, input debounce/invert/counters, full register map), built with
ASan/UBSan:

```bash
cmake --preset host-tests && cmake --build --preset host-tests && ctest --preset host-tests
```

Current size (MinSizeRel): about 7.1 KB flash and 1.7 KB RAM of 32 KB / 8 KB.

## Layout

| Path | Contents |
|---|---|
| `lib/` | hardware-independent: `modbus_rtu`, `crc16`, `config` (EEPROM slots), `io_model` (outputs → DRV8912 registers), `app` (register map, input debounce, comm watchdog) |
| `firmware/` | STM32G030 code: start-up, linker script, clocks/SysTick/IWDG, USART + DE, SPI, I2C, ADC, device drivers, `main.c` |
| `tests/` | host unit tests |
| `tools/io8.py` | command-line Modbus client (pyserial) |
| `cmake/` | toolchain file, CMSIS lookup |

## Communication

* Modbus RTU, default **address 1 + straps, 19200 baud, 8E1**.
* Address = base address (HR 32, default 1) + strap offset: ADDR0 fitted
  adds 1, ADDR1 fitted adds 2. With the default base address, the straps
  give addresses 1–4.
* RS-485 or RS-232 is selected only by the MODE strap on the board. The
  firmware is the same for both: it turns its receiver off while it
  transmits, which drops the RS-485 echo, and drives DE around every
  response. In RS-232 mode, DE only toggles the unused T2OUT pin.
* Frame end = 3.5 character times of silence (at least 1.75 ms), detected
  by the USART receiver-timeout hardware.

## Register map

The full table is in `lib/app.h`.

| Table | Address | Meaning |
|---|---|---|
| Coil | 0–7 | output value OUT1–OUT8 |
| Discrete input | 0–7 | input IN1–IN8 (1 = active, after invert and debounce) |
| Holding | 0–7 | output mode: 0 off, 1 low-side, 2 high-side, 3 push-pull |
| Holding | 8–15 | output value 0/1 |
| Holding | 16–23 | PWM generator used by the output (0 = none, 1–4) |
| Holding | 24–27 | duty of PWM generators 1–4 (0–255) |
| Holding | 28–31 | frequency of generators 1–4: 0 = 80 Hz, 1 = 100 Hz, 2 = 200 Hz, 3 = 2 kHz |
| Holding | 32 | input invert mask (bit n = IN(n+1); set for normally-closed contacts) |
| Holding | 33 | input debounce in ms (0–250, default 10) |
| Holding | 34–36 | base address, baud code (0 9600 … 4 115200), parity (0 N, 1 E, 2 O); applied after save + reboot |
| Holding | 37 | comm timeout in ms (0 = off) |
| Holding | 38 | command: 0xA55A save, 0x5AA5 defaults, 0xC1F1 clear driver faults, 0xB007 reboot, 0xC0DE clear input counters |
| Input | 0–9 | version, input bitmap, VIO mV, driver IC_STAT, OCP status, flags, address, straps, uptime |
| Input | 10–17 | activation counters IN1–IN8 (inactive → active edges) |
| Input | 18 | raw opto bitmap (before invert and debounce) |

### Outputs

| Mode | value = 0 | value = 1 | value = 1 with PWM generator g |
|---|---|---|---|
| off | both switches off | both switches off | – |
| low-side | off | sinks to GND | low-side switch PWM |
| high-side | off | sources VIO | high-side switch PWM |
| push-pull | drives GND | drives VIO | push-pull PWM (synchronous rectification) |

**PWM limitation:** the DRV8912 has **four** PWM generators. Any number of
outputs can use PWM, but at most four different duty/frequency settings exist
at the same time. Outputs that share a generator switch in sync.

### Inputs

An input is *active* when current flows through its opto, which happens when
the contact or sensor is closed. This holds for both wirings: COM to GND with
the switch to +V (source), or COM to +V with the switch to GND (sink). No
polarity setting is needed. Use the invert mask for normally-closed contacts.

The optos are sampled every millisecond and immediately on an expander
interrupt. A change is accepted once the level has been stable for the
debounce time. The counters count inactive → active transitions, so they
suit slow events (buttons, sensors, meters up to roughly 1 / (2 × debounce)).
They are not a high-speed pulse counter.

## Safety behaviour

* At reset the driver stays asleep (nSLEEP low, also held by a pull-down),
  both chip selects are high and DE is low. Outputs are enabled only after the
  configuration has been loaded and the driver set up.
* Open-load detection is disabled, since the loads are logic inputs.
  Overcurrent, overtemperature, UVLO and OVP stay active in the driver.
  Faults appear in IR 3–5 and are cleared with command 0xC1F1.
* Comm watchdog (HR 35): with no valid request to this address within the
  timeout, all switches turn off. The next valid request restores them.
* The IWDG resets the MCU if the main loop stalls for about 1 s.
* A configuration save writes to the older of two EEPROM slots and verifies
  it. A power failure during a save keeps the previous copy.

## Quick start with `tools/io8.py`

```bash
pip install pyserial
python3 tools/io8.py /dev/ttyUSB0 status
python3 tools/io8.py /dev/ttyUSB0 mode 1 push-pull
python3 tools/io8.py /dev/ttyUSB0 set 1 on
python3 tools/io8.py /dev/ttyUSB0 mode 2 low-side
python3 tools/io8.py /dev/ttyUSB0 pwm 2 --gen 1 --duty 25 --freq 2k
python3 tools/io8.py /dev/ttyUSB0 set 2 on
python3 tools/io8.py /dev/ttyUSB0 invert 0x80   # IN8 is a normally-closed contact
python3 tools/io8.py /dev/ttyUSB0 save          # current setup becomes the power-on state
```

## Status

The code compiles cleanly and the host tests pass. It has **not run on
hardware yet**. Check these on the first board:

* USART pins on pads 16/17 (SYSCFG PA11/PA12 remap)
* DRV8912 SPI mode 1 and the IC_ID read-back
* opto input switching at 5 V and 24 V, in both sink and source wiring
* ADC scaling of the VIO monitor
* I2C timing to the EEPROM
* RS-485 turnaround at the chosen baud rate
