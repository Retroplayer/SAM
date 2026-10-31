cmake_minimum_required(VERSION 3.20)

if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if(NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../.deps/pico-sdk")
endif()
set(PICO_BOARD waveshare_rp2040_zero CACHE STRING "Target board")
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(robot_sam_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(AUDIO_OUTPUT PWM CACHE STRING "Audio backend: PWM or I2S")
set_property(CACHE AUDIO_OUTPUT PROPERTY STRINGS PWM I2S)
string(TOUPPER "${AUDIO_OUTPUT}" AUDIO_OUTPUT)
if(NOT AUDIO_OUTPUT MATCHES "^(PWM|I2S)$")
    message(FATAL_ERROR "AUDIO_OUTPUT must be PWM or I2S")
endif()

set(ROBOT_PWM_PIN 14 CACHE STRING "PWM audio GPIO (even/A-channel)")
set(ROBOT_I2S_DATA_PIN 10 CACHE STRING "I2S data GPIO")
set(ROBOT_I2S_CLOCK_PIN 9 CACHE STRING "I2S BCLK GPIO; LRCLK is this - 1")
set(ROBOT_UART_TX_PIN 0 CACHE STRING "UART0 TX GPIO")
set(ROBOT_UART_RX_PIN 1 CACHE STRING "UART0 RX GPIO")

foreach(pin ROBOT_PWM_PIN ROBOT_I2S_DATA_PIN ROBOT_I2S_CLOCK_PIN ROBOT_UART_TX_PIN ROBOT_UART_RX_PIN)
    if(NOT "${${pin}}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR "${pin} must be a GPIO number")
    endif()
    if(${${pin}} GREATER 29)
        message(FATAL_ERROR "${pin} must be between 0 and 29")
    endif()
endforeach()
if(NOT ROBOT_UART_TX_PIN MATCHES "^(0|12|16|28)$" OR NOT ROBOT_UART_RX_PIN MATCHES "^(1|13|17|29)$")
    message(FATAL_ERROR "Pins must select UART0 TX (0/12/16/28) and RX (1/13/17/29)")
endif()
set(active_pins ${ROBOT_UART_TX_PIN} ${ROBOT_UART_RX_PIN})
if(AUDIO_OUTPUT STREQUAL "PWM")
    math(EXPR pwm_channel "${ROBOT_PWM_PIN} % 2")
    if(pwm_channel)
        message(FATAL_ERROR "ROBOT_PWM_PIN must be even (PWM channel A)")
    endif()
    list(APPEND active_pins ${ROBOT_PWM_PIN})
    set(expected_pins 3)
else()
    if(ROBOT_I2S_CLOCK_PIN LESS 1)
        message(FATAL_ERROR "I2S BCLK must be at least 1; LRCLK is BCLK - 1")
    endif()
    math(EXPR lrclk "${ROBOT_I2S_CLOCK_PIN} - 1")
    list(APPEND active_pins ${ROBOT_I2S_DATA_PIN} ${ROBOT_I2S_CLOCK_PIN} ${lrclk})
    set(expected_pins 5)
endif()
list(REMOVE_DUPLICATES active_pins)
list(LENGTH active_pins unique_pins)
if(NOT unique_pins EQUAL expected_pins)
    message(FATAL_ERROR "Audio and UART GPIO assignments must not overlap")
endif()

set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(REF "${ROOT}/.deps/arduino-SAM/src")
if(NOT EXISTS "${REF}/sam.c")
    message(FATAL_ERROR "Run make reference in the repository root first")
endif()

add_executable(robot-sam
    main.c audio.c
    "${ROOT}/src/robot.c" "${ROOT}/src/render.c"
    "${REF}/sam.c" "${REF}/render.c" "${REF}/reciter.c" "${REF}/sam_debug.c")
target_include_directories(robot-sam PRIVATE "${ROOT}/include" "${ROOT}/compat" "${REF}")
target_compile_definitions(robot-sam PRIVATE
    ROBOT_AUDIO_${AUDIO_OUTPUT}=1
    ROBOT_PWM_PIN=${ROBOT_PWM_PIN}
    ROBOT_I2S_DATA_PIN=${ROBOT_I2S_DATA_PIN}
    ROBOT_I2S_CLOCK_PIN=${ROBOT_I2S_CLOCK_PIN}
    ROBOT_UART_TX_PIN=${ROBOT_UART_TX_PIN}
    ROBOT_UART_RX_PIN=${ROBOT_UART_RX_PIN}
    PICO_STDIO_USB_STDOUT_TIMEOUT_US=1000
    PICO_STACK_SIZE=0x1000)
target_compile_options(robot-sam PRIVATE -fno-strict-aliasing)
set_source_files_properties(main.c audio.c "${ROOT}/src/robot.c" "${ROOT}/src/render.c"
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror")
target_link_libraries(robot-sam pico_stdlib pico_multicore hardware_dma hardware_pwm
    hardware_pio hardware_clocks hardware_uart pico_atomic)
if(AUDIO_OUTPUT STREQUAL "I2S")
    pico_generate_pio_header(robot-sam "${CMAKE_CURRENT_LIST_DIR}/i2s.pio")
endif()
pico_enable_stdio_usb(robot-sam 1)
pico_enable_stdio_uart(robot-sam 0)
pico_set_program_name(robot-sam "Robot SAM ${AUDIO_OUTPUT}")
pico_set_program_version(robot-sam "0.2.1")
set_target_properties(robot-sam PROPERTIES OUTPUT_NAME "robot-sam-${AUDIO_OUTPUT}")
pico_add_extra_outputs(robot-sam)
