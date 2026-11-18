cmake_minimum_required(VERSION 3.20)

# Two build flavours from one tree:
#   firmware   - cross-compiled with cmake/arm-none-eabi.cmake (preset "firmware")
#   host tests - native compiler, unit tests of lib/ (preset "host-tests")
project(io8 VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(WARNINGS -Wall -Wextra -Wshadow -Wundef -Wdouble-promotion -Wno-unused-parameter)

# ------------------------------------------------------------------ portable core
add_library(io8_core STATIC
  lib/app.c
  lib/config.c
  lib/crc16.c
  lib/io_model.c
  lib/modbus_rtu.c)
target_include_directories(io8_core PUBLIC lib)
target_compile_options(io8_core PRIVATE ${WARNINGS})

if(CMAKE_CROSSCOMPILING)
  # ---------------------------------------------------------------- firmware
  include(cmake/stm32g0_cmsis.cmake)

  add_executable(io8_fw
    firmware/adc.c
    firmware/devices.c
    firmware/i2c.c
    firmware/main.c
    firmware/spi.c
    firmware/startup_stm32g030.c
    firmware/system.c
    firmware/uart.c)
  set_target_properties(io8_fw PROPERTIES SUFFIX ".elf")
  target_include_directories(io8_fw PRIVATE firmware)
  target_link_libraries(io8_fw PRIVATE io8_core stm32g0_cmsis)
  target_compile_options(io8_fw PRIVATE ${WARNINGS})
  set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32g030f6.ld)
  target_link_options(io8_fw PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/io8_fw.map
    -Wl,--print-memory-usage)
  set_target_properties(io8_fw PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

  add_custom_command(TARGET io8_fw POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:io8_fw> ${CMAKE_CURRENT_BINARY_DIR}/io8_fw.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:io8_fw> ${CMAKE_CURRENT_BINARY_DIR}/io8_fw.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:io8_fw>
    COMMENT "Creating io8_fw.hex / io8_fw.bin")

  # cmake --build <dir> --target flash   (OpenOCD + ST-LINK, or STM32CubeProgrammer CLI)
  find_program(OPENOCD openocd)
  find_program(STM32_PROGRAMMER_CLI STM32_Programmer_CLI)
  if(OPENOCD)
    add_custom_target(flash
      COMMAND ${OPENOCD} -f interface/stlink.cfg -f target/stm32g0x.cfg
              -c "program $<TARGET_FILE:io8_fw> verify reset exit"
      DEPENDS io8_fw USES_TERMINAL)
  elseif(STM32_PROGRAMMER_CLI)
    add_custom_target(flash
      COMMAND ${STM32_PROGRAMMER_CLI} -c port=SWD -w ${CMAKE_CURRENT_BINARY_DIR}/io8_fw.hex -v -rst
      DEPENDS io8_fw USES_TERMINAL)
  endif()
else()
  # -------------------------------------------------------------- host tests
  enable_testing()
  add_executable(io8_tests
    tests/test_main.c
    tests/test_crc_modbus.c
    tests/test_config.c
    tests/test_io_model.c
    tests/test_app.c)
  target_link_libraries(io8_tests PRIVATE io8_core)
  target_compile_options(io8_tests PRIVATE ${WARNINGS})
  if(NOT MSVC)
    target_compile_options(io8_core PRIVATE -fsanitize=address,undefined)
    target_compile_options(io8_tests PRIVATE -fsanitize=address,undefined)
    target_link_options(io8_tests PRIVATE -fsanitize=address,undefined)
  endif()
  add_test(NAME io8_tests COMMAND io8_tests)
endif()
