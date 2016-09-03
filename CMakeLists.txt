project(f107-fft C ASM)
cmake_minimum_required(VERSION 3.5.0)

file(GLOB_RECURSE USER_SOURCES "User/*.c")
file(GLOB_RECURSE MX_SOURCES "Src/*.c")
file(GLOB_RECURSE HAL_SOURCES "Drivers/STM32F1xx_HAL_Driver/Src/*.c")
file(GLOB_RECURSE CMSIS_SOURCES "Drivers/CMSIS/DSP_Lib/Source/*.c")

add_library(HAL ${HAL_SOURCES})
add_library(CMSIS ${CMSIS_SOURCES}
        Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c
        Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f107xc.s)

include_directories(Inc)
include_directories(Drivers/STM32F1xx_HAL_Driver/Inc)
include_directories(Drivers/CMSIS/Include)
include_directories(Drivers/CMSIS/Device/ST/STM32F1xx/Include)
include_directories(User)

# stdlib path
include_directories(/usr/arm-none-eabi/include)
include_directories(/usr/lib/gcc/arm-none-eabi/6.1.1/include/)

# defines for the used libraries
add_definitions(-DSTM32F107xC)
add_definitions(-DARM_MATH_CM3)
add_definitions(-DF_CPU=72000000UL)
add_definitions(-DUSE_FULL_ASSERT)

add_executable(${PROJECT_NAME}.elf ${MX_SOURCES} ${USER_SOURCES} ${LINKER_SCRIPT})

target_link_libraries(${PROJECT_NAME}.elf HAL CMSIS)

set(HEX_FILE ${PROJECT_SOURCE_DIR}/build/${PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_SOURCE_DIR}/build/${PROJECT_NAME}.bin)
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
        COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
        COMMENT "Building ${HEX_FILE} \nBuilding ${BIN_FILE}")
