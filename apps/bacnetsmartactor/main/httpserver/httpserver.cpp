#include <stdint.h>
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "string.h"
#include "httpserver/httpserver.h"
//set(COMPONENT_EMBED_TXTFILES html/index.html)

static constexpr char *TAG = "HTTPSERV";

constexpr static char http_html_hdr[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";

extern const uint8_t index_html_start[] asm("_binary_html_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_html_index_html_end");
size_t index_hmtl_size = index_html_end - index_html_start;
char buffer[2048];

httpserver::httpserver(uint16_t port, gpio_num_t blinkgpio) : port(port), blinkgpio(blinkgpio)
{
}

esp_err_t httpserver::Task()
{
  struct netconn *conn, *newconn;
  err_t err;
  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, this->port);
  netconn_listen(conn);
  do
  {
    err = netconn_accept(conn, &newconn);
    if (err == ERR_OK)
    {
      NetconnServe(newconn);
      netconn_delete(newconn);
    }
  } while (err == ERR_OK);
  netconn_close(conn);
  netconn_delete(conn);
  return ESP_OK;
}

static void servertask()
{
}

static void generate_json()
{
  cJSON *root, *info, *d;
  root = cJSON_CreateObject();

  cJSON_AddItemToObject(root, "d", d = cJSON_CreateObject());
  cJSON_AddItemToObject(root, "info", info = cJSON_CreateObject());

  cJSON_AddStringToObject(d, "myName", "CMMC-ESP32-NANO");
  cJSON_AddNumberToObject(d, "temperature", 30.100);
  cJSON_AddNumberToObject(d, "humidity", 70.123);

  cJSON_AddStringToObject(info, "ssid", "dummy");
  cJSON_AddNumberToObject(info, "heap", system_get_free_heap_size());
  cJSON_AddStringToObject(info, "sdk", system_get_sdk_version());
  cJSON_AddNumberToObject(info, "time", system_get_time());

  while (true)
  {
    cJSON_ReplaceItemInObject(info, "heap",
                              cJSON_CreateNumber(system_get_free_heap_size()));
    cJSON_ReplaceItemInObject(info, "time",
                              cJSON_CreateNumber(system_get_time()));
    cJSON_ReplaceItemInObject(info, "sdk",
                              cJSON_CreateString(system_get_sdk_version()));
    cJSON_PrintPreallocated(root, buffer, sizeof(buffer) / sizeof(buffer[0]), false);
    printf("[len = %d]  ", strlen(buffer));
  }
}

esp_err_t httpserver::NetconnServe(struct netconn *conn)
{
  struct netbuf *inbuf;
  char *buf;
  u16_t buflen;
  err_t err;

  /* Read the data from the port, blocking if nothing yet there.
   We assume the request (the part we care about) is in one netbuf */
  err = netconn_recv(conn, &inbuf);

  if (err == ERR_OK)
  {
    netbuf_data(inbuf, (void **)&buf, &buflen);

    // strncpy(_mBuffer, buf, buflen);

    /* Is this an HTTP GET command? (only check the first 5 chars, since
    there are other formats for GET, and we're keeping it very simple )*/
    ESP_LOGI(TAG, "buffer = %s \n", buf);
    if (buflen >= 5 &&
        buf[0] == 'G' &&
        buf[1] == 'E' &&
        buf[2] == 'T' &&
        buf[3] == ' ' &&
        buf[4] == '/')
    {
      printf("buf[5] = %c\n", buf[5]);
      /* Send the HTML header
             * subtract 1 from the size, since we dont send the \0 in the string
             * NETCONN_NOCOPY: our data is const static, so no need to copy it
       */

      netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);

      if (buf[5] == 'h')
      {
        gpio_set_level(blinkgpio, 0);
        /* Send our HTML page */
        netconn_write(conn, index_html_start, index_hmtl_size, NETCONN_NOCOPY);
      }
      else if (buf[5] == 'l')
      {
        gpio_set_level(blinkgpio, 1);

        /* Send our HTML page */
        netconn_write(conn, index_html_start, index_hmtl_size, NETCONN_NOCOPY);
      }
      else if (buf[5] == 'j')
      {
        generate_json();
        netconn_write(conn, buffer, strlen(buffer), NETCONN_NOCOPY);
      }
      else
      {
        netconn_write(conn, index_html_start, index_hmtl_size, NETCONN_NOCOPY);
      }
    }
  }
  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);

  /* Delete the buffer (netconn_recv gives us ownership,
   so we have to make sure to deallocate the buffer) */
  netbuf_delete(inbuf);
}