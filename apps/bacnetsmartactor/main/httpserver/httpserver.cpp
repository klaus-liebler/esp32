#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "httpserver/httpserver.h"
#include "websocket_server.h"

constexpr static char LOG_TAG[]          = "HTTP_SERV";
constexpr static char hdr_http200[]      = "HTTP/1.1 200 OK\r\nContent-type: ";
constexpr static char hdr_contentHTML[]  = "text/html\r\n\r\n";
constexpr static char hdr_contentICO[]   = "image/x-icon\r\n\r\n";
constexpr static char hdr_contentJS[]    = "text/javascript\r\n\r\n";
static char slash_w[]          = "/w";

httpserver::httpserver(uint16_t port, gpio_num_t blinkgpio, void (*websocketcallback)(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len)): port(port), blinkgpio(blinkgpio), websocketcallback(websocketcallback)
{
  this->client_queue = xQueueCreate(CONFIG_WEBSOCKET_SERVER_QUEUE_SIZE, sizeof(struct netconn*));
}

extern const uint8_t store_index_start[] asm("_binary_index_html_start");
extern const uint8_t store_index_end[] asm("_binary_index_html_end");
const size_t store_index_size = store_index_end - store_index_start;
extern const uint8_t store_favicon_start[] asm("_binary_favicon_ico_start");
extern const uint8_t store_favicon_end[] asm("_binary_favicon_ico_end");
const size_t store_favicon_size = store_favicon_end - store_favicon_start;


esp_err_t httpserver::TaskServer()
{
  struct netconn *conn, *newconn;
  err_t err = ESP_OK;
  conn = netconn_new(NETCONN_TCP);
  err=netconn_bind(conn, IP_ADDR_ANY, this->port);
  if(err!=ESP_OK) return err;
  err=netconn_listen(conn);
  if(err!=ESP_OK) return err;
  while ((err=netconn_accept(conn, &newconn)) == ERR_OK)
  {
    xQueueSendToBack(this->client_queue,&newconn,portMAX_DELAY);
  }
  //close connection
  netconn_close(conn);
  netconn_delete(conn);
  ESP_LOGE(LOG_TAG,"TaskServer ending, rebooting board");
  esp_restart();
}

esp_err_t httpserver::TaskServerHandler()
{
  err_t err = ESP_OK;
  struct netconn *conn;
  while (xQueueReceive(this->client_queue,&conn,portMAX_DELAY)==pdTRUE)
  {
    if(!conn) continue;
    NetconnServe(conn);
  }
  vTaskDelete(NULL);
  return err;
}


void httpserver::generateJson()
{
  cJSON *root, *info, *d;
  root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "d", d = cJSON_CreateObject());
  cJSON_AddItemToObject(root, "info", info = cJSON_CreateObject());
  cJSON_AddStringToObject(d, "myName", "CMMC-ESP32-NANO");
  cJSON_AddNumberToObject(d, "temperature", 30.100);
  cJSON_AddNumberToObject(d, "humidity", 70.123);
  cJSON_AddStringToObject(info, "ssid", "dummy");
  cJSON_AddNumberToObject(info, "heap", esp_get_free_heap_size());
  cJSON_AddStringToObject(info, "sdk", esp_get_idf_version());
  cJSON_AddNumberToObject(info, "time", esp_log_timestamp());
  cJSON_PrintPreallocated(root, buffer, sizeof(buffer) / sizeof(buffer[0]), false);
  cJSON_Delete(root);
}

//Runs in a queued thread
esp_err_t httpserver::NetconnServe(struct netconn *conn)
{
  struct netbuf *inbuf;
  char *buf; 
  u16_t buflen;
  err_t err;

  netconn_set_recvtimeout(conn,1000); // allow a connection timeout of 1 second
  err = netconn_recv(conn, &inbuf);
  if (err != ERR_OK)
    goto CLOSE;

  err = netbuf_data(inbuf, (void **)&buf, &buflen);
  if (err != ERR_OK)
    goto CLOSE;

  ESP_LOGI(LOG_TAG, "Processing Request %.20s", buf);
  if (!(buflen >= 5 && buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T' && buf[3] == ' ' && buf[4] == '/'))
    goto CLOSE;

  if(buf[5]=='w')//handle very specific websocket case
  {
        ESP_LOGI(LOG_TAG,"Requesting websocket on /w");
        ws_server_add_client(conn, buf, buflen, slash_w, this->websocketcallback);
        netbuf_delete(inbuf);
        return ESP_OK;
  }

  err=netconn_write(conn, hdr_http200, sizeof(hdr_http200)-1, NETCONN_NOCOPY);
  if (err != ERR_OK)
    goto CLOSE;
  
  switch (buf[5])
  {
  case 'f'://f for favicon.ico
    err=netconn_write(conn, hdr_contentICO, sizeof(hdr_contentICO)-1, NETCONN_NOCOPY);
    err=netconn_write(conn, store_favicon_start, store_favicon_size, NETCONN_NOCOPY);
    break;
  case 'j':
    generateJson();
    err=netconn_write(conn, hdr_contentJS, sizeof(hdr_contentJS)-1, NETCONN_NOCOPY);
    err=netconn_write(conn, buffer, strlen(buffer), NETCONN_NOCOPY);
    break;
  case 'h':
    gpio_set_level(blinkgpio, 0);
    err=netconn_write(conn, hdr_contentHTML, sizeof(hdr_contentHTML)-1, NETCONN_NOCOPY);
    err=netconn_write(conn, store_index_start, store_index_size, NETCONN_NOCOPY);
    break;
  case 'l':
    gpio_set_level(blinkgpio, 1);
    err=netconn_write(conn, hdr_contentHTML, sizeof(hdr_contentHTML)-1, NETCONN_NOCOPY);
    err=netconn_write(conn, store_index_start, store_index_size, NETCONN_NOCOPY);
    break;
  default:
    //no change in blinkgpio 
    err=netconn_write(conn, hdr_contentHTML, sizeof(hdr_contentHTML)-1, NETCONN_NOCOPY);
    err=netconn_write(conn, store_index_start, store_index_size, NETCONN_NOCOPY);
    break;
  }
CLOSE:
  netconn_close(conn);
  netconn_delete(conn);
  netbuf_delete(inbuf);
  return err;
}