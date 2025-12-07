# BrewOS Pico Firmware - Unit Tests
# 
# These tests run on the host machine (not the Pico) using mock
# implementations of hardware functions.
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make
#
# Run:
#   ./brewos_tests
#
# Or using CTest:
#   ctest -V

cmake_minimum_required(VERSION 3.13)

project(brewos_pico_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# =============================================================================
# Compiler Flags
# =============================================================================

# Warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Enable debugging symbols
set(CMAKE_BUILD_TYPE Debug)

# Define that we're running tests (disables hardware-specific code)
add_compile_definitions(UNIT_TEST=1)

# =============================================================================
# Include Directories
# =============================================================================

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}              # Test directory (for unity, mocks)
    ${CMAKE_CURRENT_SOURCE_DIR}/../include   # Main include directory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared # Shared protocol definitions
)

# =============================================================================
# Source Files
# =============================================================================

set(UNITY_SOURCES
    unity/unity.c
)

set(TEST_SOURCES
    test_main.c
    test_sensor_utils.c
    test_protocol.c
    test_pid.c
    test_cleaning.c
    test_config_validation.c
    test_power_meter.c
)

# =============================================================================
# Test Executable
# =============================================================================

add_executable(brewos_tests
    ${UNITY_SOURCES}
    ${TEST_SOURCES}
)

# Link math library (for NTC calculations, etc.)
target_link_libraries(brewos_tests m)

# =============================================================================
# CTest Integration
# =============================================================================

add_test(NAME BrewOS_Unit_Tests COMMAND brewos_tests)

# =============================================================================
# Custom Targets
# =============================================================================

# Run tests with verbose output
add_custom_target(run_tests
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/brewos_tests
    DEPENDS brewos_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running BrewOS unit tests..."
)

# Run tests with valgrind (memory leak detection)
find_program(VALGRIND valgrind)
if(VALGRIND)
    add_custom_target(run_tests_valgrind
        COMMAND ${VALGRIND} --leak-check=full --error-exitcode=1 ${CMAKE_CURRENT_BINARY_DIR}/brewos_tests
        DEPENDS brewos_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running BrewOS unit tests with Valgrind..."
    )
endif()

# =============================================================================
# Output
# =============================================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "BrewOS Pico Unit Tests Configuration")
message(STATUS "========================================")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Test Executable: brewos_tests")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "Build with: make")
message(STATUS "Run tests with: ./brewos_tests or make run_tests")
message(STATUS "")

