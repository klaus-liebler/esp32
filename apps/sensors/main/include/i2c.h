#pragma once
#include "driver/i2c.h"
namespace WHS_MBFM{
    class I2CUtils
    {
    int gx;
    int gy;

    public:
    static esp_err_t MasterInit(i2c_port_t i2c_num, gpio_num_t sda, gpio_num_t scl);
    static esp_err_t ReadSlave(i2c_port_t i2c_num, uint8_t slave_addr7bit, uint8_t *data_rd, size_t size);
    static esp_err_t WriteSlave(i2c_port_t i2c_num, uint8_t slave_addr7bit, uint8_t *data_wr, size_t size);
    };
}