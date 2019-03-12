cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS "../../components")
set(COMPONENTS main bootloader esptool_py freertos driver lwip cxx bacnet-stack nvs_flash json websocket bme280 openssl)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bacnetsmartactor)
target_compile_definitions(bacnet-stack PUBLIC BACDL_BIP)

