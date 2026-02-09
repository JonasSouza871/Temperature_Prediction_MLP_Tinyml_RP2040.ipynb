# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
cmake_minimum_required(VERSION 3.13)
# Pico SDK
include(pico_sdk_import.cmake)

project(temperature_prediction C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD pico_w CACHE STRING "Pico board")

pico_sdk_init()
# TensorFlow Lite Micro
set(BUILD_TESTING OFF CACHE BOOL "Disable building tests" FORCE)
set(PICO_TFLMICRO_BUILD_TESTS OFF CACHE BOOL "Disable pico-tflmicro tests" FORCE)
set(PICO_TFLMICRO_BUILD_EXAMPLES OFF CACHE BOOL "Disable pico-tflmicro examples" FORCE)
set(PICO_TFLMICRO_BUILD_BENCHMARKS OFF CACHE BOOL "Disable pico-tflmicro benchmarks" FORCE)

add_subdirectory(pico-tflmicro pico-tflmicro-build EXCLUDE_FROM_ALL)

set(TFLM_TARGET "")
foreach(candidate IN ITEMS pico_tflmicro pico-tflmicro tflmicro pico_tflmicro_lib)
    if(TARGET ${candidate})
        set(TFLM_TARGET ${candidate})
        break()
    endif()
endforeach()

if(TFLM_TARGET STREQUAL "")
    message(FATAL_ERROR "Could not find a pico-tflmicro library target.")
endif()

# Biblioteca SSD1306
add_library(ssd1306 STATIC firmware/lib/ssd1306.c)
target_include_directories(ssd1306 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/firmware/lib)
target_link_libraries(ssd1306 PUBLIC hardware_i2c hardware_gpio)

# Biblioteca dos sensores (AHT20 e BMP280)
add_library(sensors STATIC
    firmware/lib/aht20.c
    firmware/lib/bmp280.c
)
target_include_directories(sensors PUBLIC ${CMAKE_CURRENT_LIST_DIR}/firmware/lib)
target_link_libraries(sensors PUBLIC hardware_i2c pico_stdlib)

# Executável principal
add_executable(temperature_prediction
    firmware/main.c
    firmware/tflm_wrapper.cpp
)

pico_set_program_name(temperature_prediction "Temperature Prediction")
pico_set_program_version(temperature_prediction "1.0")
pico_enable_stdio_uart(temperature_prediction 1)
pico_enable_stdio_usb(temperature_prediction 1)

target_include_directories(temperature_prediction PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/firmware
    ${CMAKE_CURRENT_LIST_DIR}/firmware/lib
)

target_link_libraries(temperature_prediction PRIVATE
    pico_stdlib
    hardware_i2c
    hardware_gpio
    ssd1306
    sensors
    ${TFLM_TARGET}
)

pico_add_extra_outputs(temperature_prediction)
