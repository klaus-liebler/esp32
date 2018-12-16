#pragma once

class httpserver{
    private:
        uint16_t port;
        gpio_num_t blinkgpio;
    public:
        httpserver(uint16_t port, gpio_num_t blinkgpio);
        esp_err_t  Task(void);
        esp_err_t  NetconnServe(struct netconn *conn);
    
}