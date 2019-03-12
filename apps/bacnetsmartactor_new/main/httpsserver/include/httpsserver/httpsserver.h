#pragma once
#include "websocket_server.h"
#include "openssl/ssl.h"

class httpsserver{
    private:
        static constexpr size_t RECV_BUF_LEN = 2048;
        uint16_t port;
        char buffer[2048];
        void (*websocketcallback)(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len);
        QueueHandle_t client_queue;
        esp_err_t TaskServerLoop(SSL_CTX *ctx, int sockfd, struct sockaddr_in sock_addr);
    public:
        httpsserver(uint16_t port, void (*websocketcallback)(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len));
        esp_err_t  TaskServer(void);
        
        esp_err_t  TaskServerHandler(void);
        uint16_t GetPort(){return port;}
};