cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS "../components")
set(COMPONENTS main bootloader esptool_py freertos driver cxx bme280bosch spi_flash nvs_flash)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sensors)


