# Locate the ST CMSIS device header (stm32g0xx.h) and the ARM CMSIS core headers.
#
# Either point STM32CUBE_G0_PATH at an STM32CubeG0 firmware package
# (STM32CubeMX downloads it to ~/STM32Cube/Repository/STM32Cube_FW_G0_Vx.y.z),
# or set CMSIS_DEVICE_G0_INCLUDE and CMSIS_CORE_INCLUDE directly, e.g. from
#   git clone https://github.com/STMicroelectronics/cmsis-device-g0
#   git clone https://github.com/STMicroelectronics/cmsis-core

set(STM32CUBE_G0_PATH "" CACHE PATH "STM32CubeG0 firmware package root")
set(CMSIS_DEVICE_G0_INCLUDE "" CACHE PATH "Directory containing stm32g0xx.h")
set(CMSIS_CORE_INCLUDE "" CACHE PATH "Directory containing core_cm0plus.h")

if(NOT STM32CUBE_G0_PATH AND NOT CMSIS_DEVICE_G0_INCLUDE)
  file(GLOB _cube_candidates "$ENV{HOME}/STM32Cube/Repository/STM32Cube_FW_G0_V*")
  list(SORT _cube_candidates COMPARE NATURAL ORDER DESCENDING)
  foreach(_c IN LISTS _cube_candidates)
    if(IS_DIRECTORY "${_c}/Drivers/CMSIS")
      set(STM32CUBE_G0_PATH "${_c}" CACHE PATH "STM32CubeG0 firmware package root" FORCE)
      break()
    endif()
  endforeach()
endif()

if(STM32CUBE_G0_PATH)
  if(NOT CMSIS_DEVICE_G0_INCLUDE)
    set(CMSIS_DEVICE_G0_INCLUDE "${STM32CUBE_G0_PATH}/Drivers/CMSIS/Device/ST/STM32G0xx/Include")
  endif()
  if(NOT CMSIS_CORE_INCLUDE)
    set(CMSIS_CORE_INCLUDE "${STM32CUBE_G0_PATH}/Drivers/CMSIS/Include")
  endif()
endif()

if(NOT EXISTS "${CMSIS_DEVICE_G0_INCLUDE}/stm32g0xx.h" OR NOT EXISTS "${CMSIS_CORE_INCLUDE}/core_cm0plus.h")
  message(FATAL_ERROR
    "CMSIS headers not found.\n"
    "Set -DSTM32CUBE_G0_PATH=<STM32Cube_FW_G0 root> or\n"
    "    -DCMSIS_DEVICE_G0_INCLUDE=<dir with stm32g0xx.h> -DCMSIS_CORE_INCLUDE=<dir with core_cm0plus.h>")
endif()
message(STATUS "CMSIS device: ${CMSIS_DEVICE_G0_INCLUDE}")
message(STATUS "CMSIS core:   ${CMSIS_CORE_INCLUDE}")

add_library(stm32g0_cmsis INTERFACE)
target_include_directories(stm32g0_cmsis SYSTEM INTERFACE ${CMSIS_DEVICE_G0_INCLUDE} ${CMSIS_CORE_INCLUDE})
target_compile_definitions(stm32g0_cmsis INTERFACE STM32G030xx)
