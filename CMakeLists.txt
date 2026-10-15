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

# W1E1: application, platform and FreeRTOS sources.
add_executable(w1e1
    platform/startup.c
    platform/system.c
    platform/gpio.c
    week1/ex1/main.c
    week1/ex1/blinky.c
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

find_program(OPENOCD_EXECUTABLE NAMES openocd)

if(OPENOCD_EXECUTABLE)
    add_custom_target(w1e1_flash
        COMMAND "${OPENOCD_EXECUTABLE}"
            -f interface/stlink.cfg
            -c "transport select swd"
            -f target/stm32f1x.cfg
            -c "adapter speed 100"
            -c "reset_config none"
            -c "program {$<TARGET_FILE:w1e1>} verify reset exit"
        COMMENT "Flashing w1e1 with ST-Link"
        VERBATIM
        USES_TERMINAL
    )

    add_dependencies(w1e1_flash w1e1)
else()
    message(STATUS
        "OpenOCD not found: build available, w1e1_flash disabled"
    )
endif()


# W1E2: application, platform and FreeRTOS sources.
add_executable(w1e2
    platform/startup.c
    platform/system.c
    platform/gpio.c
    platform/uart.c

    week1/ex2/main.c
    week1/ex2/trace_app.c
    week1/ex2/tracer.c

    third_party/FreeRTOS-Kernel/tasks.c
    third_party/FreeRTOS-Kernel/list.c
    third_party/FreeRTOS-Kernel/queue.c
    third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c
    third_party/FreeRTOS-Kernel/portable/MemMang/heap_4.c
)

set_target_properties(w1e2 PROPERTIES
    SUFFIX ".elf"
)

# Khai báo header directories và chip
target_include_directories(w1e2 PRIVATE
    platform/include
    week1/ex2/include

    third_party/CMSIS_5/CMSIS/Core/Include
    third_party/cmsis-device-f1/Include

    third_party/FreeRTOS-Kernel/include
    third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3
)

target_compile_definitions(w1e2 PRIVATE
    STM32F103xB
)

# Khai báo linker options
target_link_options(w1e2 PRIVATE
    "-T${CMAKE_SOURCE_DIR}/platform/linker/stm32f103c8.ld"
    -nostartfiles
)

# Khai báo OpenOCD flash target
if(OPENOCD_EXECUTABLE)
    add_custom_target(w1e2_flash
        COMMAND "${OPENOCD_EXECUTABLE}"
            -f interface/stlink.cfg
            -c "transport select swd"
            -f target/stm32f1x.cfg
            -c "adapter speed 100"
            -c "reset_config none"
            -c "program {$<TARGET_FILE:w1e2>} verify reset exit"
        COMMENT "Flashing w1e2 with ST-Link"
        VERBATIM
        USES_TERMINAL
    )

    add_dependencies(w1e2_flash w1e2)
endif()

add_executable(w1e3
    platform/startup.c
    platform/system.c
    platform/gpio.c
    platform/uart.c
    week1/ex2/tracer.c
    week1/ex3/main.c
    week1/ex3/rta_app.c
    week1/ex3/rta_measure.c
    third_party/FreeRTOS-Kernel/tasks.c
    third_party/FreeRTOS-Kernel/list.c
    third_party/FreeRTOS-Kernel/queue.c
    third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c
    third_party/FreeRTOS-Kernel/portable/MemMang/heap_4.c
)
set_target_properties(w1e3 PROPERTIES SUFFIX ".elf")
target_include_directories(w1e3 PRIVATE
    week1/ex3/include
    week1/ex2/include
    platform/include
    third_party/CMSIS_5/CMSIS/Core/Include
    third_party/cmsis-device-f1/Include
    third_party/FreeRTOS-Kernel/include
    third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3
)
target_compile_definitions(w1e3 PRIVATE STM32F103xB TRACER_FIRST_PIN=4u TRACER_LAST_PIN=8u)
target_link_options(w1e3 PRIVATE
    "-T${CMAKE_SOURCE_DIR}/platform/linker/stm32f103c8.ld"
    -nostartfiles
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/w1e3.map"
    -Wl,--print-memory-usage
)
if(OPENOCD_EXECUTABLE)
    add_custom_target(w1e3_flash
        COMMAND "${OPENOCD_EXECUTABLE}"
            -f interface/stlink.cfg -c "transport select swd"
            -f target/stm32f1x.cfg -c "adapter speed 100"
            -c "reset_config none"
            -c "program {$<TARGET_FILE:w1e3>} verify reset exit"
        DEPENDS w1e3 VERBATIM USES_TERMINAL
    )
endif()

