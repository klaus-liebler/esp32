#pragma once

#include "websocket_server.h"

class httpserver{
    private:
        uint16_t port;
        gpio_num_t blinkgpio;
        char buffer[2048];
        void (*websocketcallback)(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len);
        QueueHandle_t client_queue;
        esp_err_t  NetconnServe(struct netconn *conn);
        void generateJson();
    public:
        httpserver(uint16_t port, gpio_num_t blinkgpio, void (*websocketcallback)(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len));
        esp_err_t  TaskServer(void);
        esp_err_t  TaskServerHandler(void);
        uint16_t GetPort(){return port;}
};