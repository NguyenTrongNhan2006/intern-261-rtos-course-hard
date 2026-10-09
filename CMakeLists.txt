cmake_minimum_required(VERSION 3.20)

project(intern_261_rtos_course_hard
    LANGUAGES C ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(
    -mcpu=cortex-m3
    -mthumb
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Werror
)

add_link_options(
    -mcpu=cortex-m3
    -mthumb
    -Wl,--gc-sections
)

add_executable(w1e1
    platform/startup.c
    platform/system.c
    platform/gpio.c
    week1/ex1/main.c
)

set_target_properties(w1e1 PROPERTIES
    SUFFIX ".elf"
)

target_include_directories(w1e1 PRIVATE
    platform/include
    week1/ex1/include
)

target_compile_definitions(w1e1 PRIVATE
    STM32F103xB
)

target_link_options(w1e1 PRIVATE
    -T${CMAKE_SOURCE_DIR}/platform/linker/stm32f103c8.ld
    -nostartfiles
    -Wl,--gc-sections
)