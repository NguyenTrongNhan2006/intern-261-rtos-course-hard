# Toolchain file for ARM Cortex-M using arm-none-eabi

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(ARM_GCC arm-none-eabi-gcc REQUIRED)
find_program(ARM_OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(ARM_SIZE arm-none-eabi-size REQUIRED)

set(CMAKE_C_COMPILER "${ARM_GCC}")
set(CMAKE_ASM_COMPILER "${ARM_GCC}")
set(CMAKE_OBJCOPY "${ARM_OBJCOPY}")
set(CMAKE_SIZE "${ARM_SIZE}")

# Set the compiler flags for ARM Cortex-M

find_program(ARM_AR arm-none-eabi-ar REQUIRED)
find_program(ARM_RANLIB arm-none-eabi-ranlib REQUIRED)

set(CMAKE_AR "${ARM_AR}" CACHE FILEPATH "ARM archiver" FORCE)
set(CMAKE_RANLIB "${ARM_RANLIB}" CACHE FILEPATH "ARM archive index tool" FORCE)