# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(eeprom_programmer)

pico_sdk_init()

add_executable(eeprom_programmer
    main.c
)

target_link_libraries(eeprom_programmer 
    pico_stdlib
    hardware_i2c
)

pico_enable_stdio_usb(eeprom_programmer 1)
pico_enable_stdio_uart(eeprom_programmer 1)

pico_add_extra_outputs(eeprom_programmer)