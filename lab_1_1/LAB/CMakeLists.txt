cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w CACHE STRING "Board type")
set(PROGRAM_NAME LAB)

include(pico_sdk_import.cmake)

project(${PROGRAM_NAME} C CXX ASM)

pico_sdk_init()

add_subdirectory(../lcd_sd_card_example/lib/config lib/config)
add_subdirectory(../lcd_sd_card_example/lib/lcd lib/lcd)
add_subdirectory(../lcd_sd_card_example/lib/font lib/font)
add_subdirectory(../lcd_sd_card_example/lib/sd_card_driver lib/sd_card_driver)
add_subdirectory(../imu_example/icm20948 icm20948)

add_executable(${PROGRAM_NAME}
    ../lcd_sd_card_example/config/hw_config.c
    main.cpp
)

target_include_directories(${PROGRAM_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ../lcd_sd_card_example/lib/config
    ../lcd_sd_card_example/lib/lcd
    ../lcd_sd_card_example/lib/font
    ../lcd_sd_card_example/lib/sd_card_driver/include
    ../imu_example/icm20948
)

target_compile_options(${PROGRAM_NAME} PRIVATE
    -Wall
    -Wextra
    -Wshadow
)

target_link_libraries(${PROGRAM_NAME}
    lcd
    font
    config
    sd_custom_driver
    icm20948
    pico_stdlib
    hardware_i2c
    hardware_spi
    hardware_pwm
    pico_multicore
    pico_util
    hardware_clocks
    hardware_adc
    hardware_pio
    pico_cyw43_arch_none
)

pico_enable_stdio_usb(${PROGRAM_NAME} 1)
pico_enable_stdio_uart(${PROGRAM_NAME} 0)
pico_add_extra_outputs(${PROGRAM_NAME})