# BrewOS - Pico Firmware
# CMake configuration with multi-machine support
#
# Build single machine type:
#   cmake .. -DMACHINE_TYPE=DUAL_BOILER
#   make
#
# Build all machine types:
#   cmake .. -DBUILD_ALL_MACHINES=ON
#   make
#
# Output binaries:
#   brewos_dual_boiler.uf2
#   brewos_single_boiler.uf2
#   brewos_heat_exchanger.uf2

cmake_minimum_required(VERSION 3.13)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Board Selection: Raspberry Pi Pico 2 (RP2350)
# =============================================================================
# The Pico 2 uses the RP2350 chip with ARM Cortex-M33 cores.
# Key differences from RP2040:
#   - Better atomic instructions (LDREX/STREX)
#   - Stricter memory alignment requirements
#   - Improved ADC (12-bit effective vs ~9-bit on RP2040)
#   - TrustZone security support (optional)
#
# Ensure you have Pico SDK 2.0.0 or later for RP2350 support.
set(PICO_BOARD pico2)
set(PICO_PLATFORM rp2350-arm-s)  # Use ARM cores (not RISC-V)

# Pull in SDK (must be before project)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
include(pico_sdk_import.cmake)

project(brewos_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# -----------------------------------------------------------------------------
# Machine Type Configuration
# -----------------------------------------------------------------------------

# Valid machine types
set(VALID_MACHINE_TYPES "DUAL_BOILER" "SINGLE_BOILER" "HEAT_EXCHANGER")

# Option to build all machine types
option(BUILD_ALL_MACHINES "Build firmware for all machine types" OFF)

# Default machine type
if(NOT DEFINED MACHINE_TYPE)
    set(MACHINE_TYPE "DUAL_BOILER")
endif()

# Validate machine type
if(NOT MACHINE_TYPE IN_LIST VALID_MACHINE_TYPES)
    message(FATAL_ERROR "Invalid MACHINE_TYPE: ${MACHINE_TYPE}. Valid options: ${VALID_MACHINE_TYPES}")
endif()

# -----------------------------------------------------------------------------
# Build Type Configuration  
# -----------------------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG_BUILD=1)
    message(STATUS "Debug build enabled")
else()
    add_compile_definitions(DEBUG_BUILD=0)
endif()

# -----------------------------------------------------------------------------
# PCB Configuration
# -----------------------------------------------------------------------------

if(NOT DEFINED PCB_TYPE)
    set(PCB_TYPE "PCB_TYPE_ECM_V1")
endif()
message(STATUS "Building for PCB: ${PCB_TYPE}")

# -----------------------------------------------------------------------------
# Common Source Files (shared by all machine types)
# -----------------------------------------------------------------------------

set(COMMON_SOURCES
    src/main.c
    src/protocol.c
    src/safety.c
    src/sensors.c
    src/control_common.c
    src/state.c
    src/environmental_config.c
    src/pcb_config.c
    src/machine_config.c
    src/gpio_init.c
    src/hardware.c
    src/sensor_utils.c
    src/water_management.c
    src/config_persistence.c
    src/flash_safe.c          # Flash safety utilities (XIP crash prevention)
    src/power_meter.c         # Generic power meter (PZEM, JSY, Eastron)
    src/cleaning.c
    src/bootloader.c
    src/diagnostics.c         # Hardware diagnostics tests
    src/class_b.c             # Class B safety self-tests (IEC 60730)
)

# -----------------------------------------------------------------------------
# Machine-Specific Control Files
# -----------------------------------------------------------------------------

set(CONTROL_DUAL_BOILER src/control_dual_boiler.c)
set(CONTROL_SINGLE_BOILER src/control_single_boiler.c)
set(CONTROL_HEAT_EXCHANGER src/control_heat_exchanger.c)

# -----------------------------------------------------------------------------
# Common Libraries
# -----------------------------------------------------------------------------

set(COMMON_LIBS
    pico_stdlib
    pico_multicore
    pico_flash          # For flash_safe_execute() on SDK 2.0+
    hardware_uart
    hardware_gpio
    hardware_adc
    hardware_spi
    hardware_i2c
    hardware_flash
    hardware_pwm
    hardware_watchdog
    hardware_sync       # For atomic operations and critical sections
)

# -----------------------------------------------------------------------------
# Function to create executable for a machine type
# -----------------------------------------------------------------------------

function(create_machine_executable MACHINE_TYPE_NAME)
    string(TOLOWER ${MACHINE_TYPE_NAME} MACHINE_TYPE_LOWER)
    set(TARGET_NAME "brewos_${MACHINE_TYPE_LOWER}")
    
    # Select control implementation
    if(MACHINE_TYPE_NAME STREQUAL "DUAL_BOILER")
        set(CONTROL_IMPL ${CONTROL_DUAL_BOILER})
    elseif(MACHINE_TYPE_NAME STREQUAL "SINGLE_BOILER")
        set(CONTROL_IMPL ${CONTROL_SINGLE_BOILER})
    elseif(MACHINE_TYPE_NAME STREQUAL "HEAT_EXCHANGER")
        set(CONTROL_IMPL ${CONTROL_HEAT_EXCHANGER})
    else()
        message(FATAL_ERROR "Unknown machine type: ${MACHINE_TYPE_NAME}")
    endif()
    
    message(STATUS "Creating target: ${TARGET_NAME} (${MACHINE_TYPE_NAME})")
    
    # Create executable
    add_executable(${TARGET_NAME}
        ${COMMON_SOURCES}
        ${CONTROL_IMPL}
    )
    
    # Add compile definitions
    target_compile_definitions(${TARGET_NAME} PRIVATE
        MACHINE_TYPE=${MACHINE_TYPE_NAME}
        PCB_TYPE=${PCB_TYPE}
    )
    
    # Include directories
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../shared
    )
    
    # Link libraries
    target_link_libraries(${TARGET_NAME} ${COMMON_LIBS})
    
    # Enable USB output for debugging
    pico_enable_stdio_usb(${TARGET_NAME} 1)
    pico_enable_stdio_uart(${TARGET_NAME} 0)
    
    # Create UF2 file
    pico_add_extra_outputs(${TARGET_NAME})
endfunction()

# -----------------------------------------------------------------------------
# Build Configuration
# -----------------------------------------------------------------------------

if(BUILD_ALL_MACHINES)
    # Build all machine types
    message(STATUS "Building ALL machine types")
    foreach(MACHINE IN LISTS VALID_MACHINE_TYPES)
        create_machine_executable(${MACHINE})
    endforeach()
else()
    # Build single machine type
    message(STATUS "Building for machine type: ${MACHINE_TYPE}")
    create_machine_executable(${MACHINE_TYPE})
    
    # Create alias 'brewos' pointing to the selected machine type
    string(TOLOWER ${MACHINE_TYPE} MACHINE_TYPE_LOWER)
    add_custom_target(brewos
        DEPENDS brewos_${MACHINE_TYPE_LOWER}
        COMMENT "Building brewos (${MACHINE_TYPE})"
    )
endif()

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "========================================")
message(STATUS "BrewOS Pico Firmware Build Configuration")
message(STATUS "========================================")
if(BUILD_ALL_MACHINES)
    message(STATUS "  Mode: Build ALL machine types")
    message(STATUS "  Targets:")
    foreach(MACHINE IN LISTS VALID_MACHINE_TYPES)
        string(TOLOWER ${MACHINE} MACHINE_LOWER)
        message(STATUS "    - brewos_${MACHINE_LOWER}")
    endforeach()
else()
    message(STATUS "  Mode: Single machine type")
    message(STATUS "  Machine Type: ${MACHINE_TYPE}")
    string(TOLOWER ${MACHINE_TYPE} MACHINE_TYPE_LOWER)
    message(STATUS "  Target: brewos_${MACHINE_TYPE_LOWER}")
endif()
message(STATUS "  PCB Type: ${PCB_TYPE}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")
message(STATUS "")
