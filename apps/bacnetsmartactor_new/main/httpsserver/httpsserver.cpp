#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "openssl/ssl.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "httpsserver/httpsserver.h"
#include "websocket_server.h"

constexpr static char LOG_TAG[] = "HTTPS_SERV";
constexpr static char hdr_http200[] = "HTTP/1.1 200 OK\r\nContent-type: ";
constexpr static char hdr_contentHTML[] = "text/html\r\n\r\n";
constexpr static char hdr_contentICO[] = "image/x-icon\r\n\r\n";



httpsserver::httpsserver(uint16_t port, void (*websocketcallback)(uint8_t num, WEBSOCKET_TYPE_t type, char *msg, uint64_t len)) : port(port), websocketcallback(websocketcallback)
{
  this->client_queue = xQueueCreate(CONFIG_WEBSOCKET_SERVER_QUEUE_SIZE, sizeof(struct netconn *));
}

extern const uint8_t store_index_start[] asm("_binary_index_html_start");
extern const uint8_t store_index_end[] asm("_binary_index_html_end");
const size_t store_index_size = store_index_end - store_index_start;

extern const uint8_t store_favicon_start[] asm("_binary_favicon_ico_start");
extern const uint8_t store_favicon_end[] asm("_binary_favicon_ico_end");
const size_t store_favicon_size = store_favicon_end - store_favicon_start;

extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
extern const unsigned char cacert_pem_end[] asm("_binary_cacert_pem_end");
const unsigned int cacert_pem_bytes = cacert_pem_end - cacert_pem_start;

extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
extern const unsigned char prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");
const unsigned int prvtkey_pem_bytes = prvtkey_pem_end - prvtkey_pem_start;

esp_err_t httpsserver::TaskServerLoop(SSL_CTX *ctx, int sockfd, struct sockaddr_in sock_addr)
{
  int new_sockfd;
  SSL *ssl;
  socklen_t addr_len;
  int ret;
    
  ESP_LOGI(LOG_TAG, "SSL server create ...");
  ssl = SSL_new(ctx);
  if (ssl==NULL)
  {
    ESP_LOGE(LOG_TAG, "SSL server create failed.");
    return ESP_FAIL;
  }
  ESP_LOGI(LOG_TAG, "SSL server create OK. Next: SSL_set_fd ...");
  new_sockfd = accept(sockfd, (struct sockaddr *)&sock_addr, &addr_len);
  if (new_sockfd < 0) goto SSL_FREE;

  ret = SSL_set_fd(ssl, new_sockfd);
  if (ret == 0){
    ESP_LOGE(LOG_TAG, "SSL_set_fd failed with error %d", ret);
    goto CLOSE_SOCKET;
  }
  ESP_LOGI(LOG_TAG, "SSL_set_fd OK. Next: SSL_accept ...");
  ret = SSL_accept(ssl);
  if(ret==0)
  {
    //as ret==0, there is no need to shutdown
    ret = SSL_get_error(ssl, ret);
    ESP_LOGE(LOG_TAG, "SSL_accept failed in a controlled manner with error %d", ret);
    goto CLOSE_SOCKET;
  }
  if (ret<0)
  {
    ret = SSL_get_error(ssl, ret);
    SSL_shutdown(ssl);
    ESP_LOGE(LOG_TAG, "SSL_accept failed with error %d", ret);
    goto CLOSE_SOCKET;
  }
  ESP_LOGI(LOG_TAG, "SSL_accept OK. Next: xQueueSendToBack ...");
  ret = xQueueSendToBack(client_queue, &ssl, portMAX_DELAY);
  if(ret != pdTRUE)
  {
    ESP_LOGE(LOG_TAG, "xQueueSendToBack failed with error %d", ret);
    goto CLOSE_SOCKET;
  }
  return ESP_OK;
CLOSE_SOCKET:
  close(new_sockfd);
SSL_FREE:
  SSL_free(ssl);
  return ESP_OK; //"ESP_OK", because the caller should not handle these errors and continue with loop
}

esp_err_t httpsserver::TaskServer()
{
  int ret;
  err_t err = ESP_OK;
  SSL_CTX *ctx;
  int sockfd;
  
  struct sockaddr_in sock_addr;
 
  //TLS stuff
  ESP_LOGI(LOG_TAG, "SSL server context create ......");
  ctx = SSL_CTX_new(TLSv1_2_server_method());
  if (!ctx)
  {
    ESP_LOGI(LOG_TAG, "failed");
    goto SSL_SERVER_CONTEXT_FAILED;
  }
  ESP_LOGI(LOG_TAG, "OK");

  ESP_LOGI(LOG_TAG, "SSL server context set own certification......");
  ret = SSL_CTX_use_certificate_ASN1(ctx, cacert_pem_bytes, cacert_pem_start);
  if (!ret)
  {
    ESP_LOGI(LOG_TAG, "failed");
    goto CERT_OR_KEY_FAILED;
  }
  ESP_LOGI(LOG_TAG, "OK");

  ESP_LOGI(LOG_TAG, "SSL server context set private key......");
  ret = SSL_CTX_use_PrivateKey_ASN1(0, ctx, prvtkey_pem_start, prvtkey_pem_bytes);
  if (!ret)
  {
    ESP_LOGI(LOG_TAG, "failed");
    goto CERT_OR_KEY_FAILED;
  }
  ESP_LOGI(LOG_TAG, "OK");
  //socket stuff
  ESP_LOGI(LOG_TAG, "SSL server create socket ......");
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    ESP_LOGI(LOG_TAG, "failed");
    goto SOCKETCREATION_FAILED;
  }
  ESP_LOGI(LOG_TAG, "OK");

  ESP_LOGI(LOG_TAG, "SSL server socket bind ......");
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = 0;
  sock_addr.sin_port = htons(this->port);
  ret = bind(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
  if (ret)
  {
    ESP_LOGI(LOG_TAG, "failed");
    goto BIND_FAILED;
  }
  ESP_LOGI(LOG_TAG, "OK");

  ESP_LOGI(LOG_TAG, "SSL server socket listen ......");
  ret = listen(sockfd, 32);
  if (ret)
  {
    ESP_LOGI(LOG_TAG, "failed");
    goto BIND_FAILED;
  }
  ESP_LOGI(LOG_TAG, "OK");
  
  while (err == ERR_OK)
  {
    err=this->TaskServerLoop(ctx, sockfd, sock_addr);
  }
BIND_FAILED:
  close(sockfd);
  sockfd = -1;
SOCKETCREATION_FAILED:
CERT_OR_KEY_FAILED:
  SSL_CTX_free(ctx);
  ctx = NULL;
SSL_SERVER_CONTEXT_FAILED:
  vTaskDelete(NULL);
  return err;
 
}

esp_err_t httpsserver::TaskServerHandler()
{
  err_t err = ESP_OK;
  SSL *ssl;
  while (xQueueReceive(client_queue, &ssl, portMAX_DELAY) == pdTRUE)
  {
    if (!ssl)
      continue;
    int  new_sockfd = SSL_get_fd(ssl);
    ESP_LOGI(LOG_TAG, "SSL server read message ......");
    while(true)
    {
      char buf[this->RECV_BUF_LEN];
      size_t bytesProcessed = SSL_read(ssl, buf, this->RECV_BUF_LEN - 1);
      if (bytesProcessed <= 0)
      {
        break;
      }
      ESP_LOGI(LOG_TAG, "Processing Request %.20s", buf);
      if (!(bytesProcessed >= 5 && buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T' && buf[3] == ' ' && buf[4] == '/'))
      {
        continue;
      }
      bytesProcessed = SSL_write(ssl, hdr_http200, sizeof(hdr_http200) - 1);
      if (bytesProcessed != sizeof(hdr_http200) - 1)
        break;

      switch (buf[5])
      {
      case 'f': //f for favicon.ico
        err = SSL_write(ssl, hdr_contentICO, sizeof(hdr_contentICO) - 1);
        err = SSL_write(ssl, store_favicon_start, store_favicon_size);
        break;
      default:
        //no change in blinkgpio
        err = SSL_write(ssl, hdr_contentHTML, sizeof(hdr_contentHTML) - 1);
        err = SSL_write(ssl, store_index_start, store_index_size);
        break;
      }
      break;
    }
    SSL_shutdown(ssl);
    close(new_sockfd);
  }
  //close connection
  vTaskDelete(NULL);
  return err;
}
