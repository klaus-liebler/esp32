set(COMPONENT_SRCS GPIO "I2C.cpp" "System.cpp" "GeneralUtils.cpp")
set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_REQUIRES mbedtls nvs_flash)
register_component()