# RS485_IO8 I/O expansion board

An STM32G030-based Modbus RTU I/O expansion board with **8 isolated inputs**
and **8 configurable outputs**. A hardware strap selects RS-485 or RS-232
communication. Outputs support low-side, high-side, push-pull and PWM modes.

| Directory | Contents |
|---|---|
| [hardware/](hardware/README.md) | Revision B KiCad project, schematics, PCB layout, BOMs, generation and validation tools, and archived revision A files |
| [software/](software/README.md) | C11 firmware, portable Modbus and I/O logic, host unit tests, and a Python Modbus client |

Open `hardware/RS485_IO8.kicad_pro` in KiCad 9 or 10. See the
[hardware README](hardware/README.md) for wiring, straps, electrical limits
and design checks. The schematics are generated; make schematic changes
through the scripts in `hardware/tools/`.

To build and run the host unit tests, install CMake, Ninja and a native C
compiler, then run from the repository root:

```sh
cd software
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

Firmware builds also require `arm-none-eabi-gcc` with newlib-nano and ST CMSIS
headers. See the [software README](software/README.md) for toolchain setup,
flashing, the register map and client examples.

The PCB is placed but not routed. The design and firmware have not yet been
tested on hardware; see the subdirectory READMEs for validation status and
remaining prototype checks.
