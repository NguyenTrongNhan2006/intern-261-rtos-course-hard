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
    third_party/CMSIS_5/CMSIS/Core/Include
    third_party/cmsis-device-f1/Include
)

target_compile_definitions(w1e1 PRIVATE
    STM32F103xB
)

target_link_options(w1e1 PRIVATE
    -T${CMAKE_SOURCE_DIR}/platform/linker/stm32f103c8.ld
    -nostartfiles
    -Wl,--gc-sections
)

target_sources(w1e1 PRIVATE
    third_party/FreeRTOS-Kernel/tasks.c
    third_party/FreeRTOS-Kernel/list.c
    third_party/FreeRTOS-Kernel/queue.c
    third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c
    third_party/FreeRTOS-Kernel/portable/MemMang/heap_4.c
)

target_include_directories(w1e1 PRIVATE
    third_party/FreeRTOS-Kernel/include
    third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3
)