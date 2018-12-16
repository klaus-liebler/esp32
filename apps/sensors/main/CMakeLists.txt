set(COMPONENT_SRCS "main.cpp" 
    "hx711/hx711.cpp"
    "lcd/adaptation.cpp" "lcd/font7s.c" "lcd/glcdfont.c" "lcd/iot_lcd.cpp" "lcd/spi_lcd.c" 
    "lcd/Adafruit-GFX-Library/Adafruit_GFX.cpp")
set(COMPONENT_ADD_INCLUDEDIRS "." "include" "hx711" "lcd/include" "lcd/Adafruit-GFX-Library" "lcd/Adafruit-GFX-Library/Fonts")
register_component()
