//standard includes
#include <stdint.h>
#include <stdio.h>
#include <string.h>

//freertos includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"


//ESP32 includes
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
//ESP32 peripherials include
#include "nvs_flash.h"
#include "driver/gpio.h"

//LWip includes
#include "lwip/api.h"


//BACnet includes
#include "bacdef.h"
#include "config.h"
#include "txbuf.h"
#include "client.h"
#include "handlers.h"
#include "datalink.h"
#include "dcc.h"
#include "tsm.h"
#include "../../../components/bacnet-stack/include/address.h"// conflict filename address.h with another file in default include paths
#include "bip.h"
#include "device.h"
#include "ai.h"
#include "bo.h"

//HTTP-Server and WebsocketServer includes
#include "httpserver/httpserver.h"

//external components include
#include <BME280.h>

//Helpers
#include "cJSON.h"


static char const* const LOG_TAG = "MAIN";

extern "C" {
    // hidden function not in any .h files
    extern uint8_t temprature_sens_read();
    extern uint32_t hall_sens_read();
}


extern const uint8_t store_index_start[] asm("_binary_index_html_start");
extern const uint8_t store_index_end[]   asm("_binary_index_html_end");

#include "sdkconfig.h"



wifi_config_t wifi_config = {};

// ESP32 DevKit has LED on GPIO2
constexpr gpio_num_t BACNET_LED = GPIO_NUM_2;
constexpr gpio_num_t SDA_PIN    = GPIO_NUM_21;
constexpr gpio_num_t SCL_PIN    = GPIO_NUM_22;
constexpr uint8_t BME280_ADDR   = 0x76;

uint8_t Handler_Transmit_Buffer[MAX_PDU] = { 0 };
uint8_t Rx_Buf[MAX_MPDU] = { 0 };

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

struct ProcessImage
{
    float temp;
    float humid;
} processImage;

// handles websocket events
void websocket_callback(uint8_t num,WEBSOCKET_TYPE_t type,char* msg,uint64_t len) {
  const static char* TAG = "websocket_callback";
  int value;
  switch(type) {
    case WEBSOCKET_CONNECT:
      ESP_LOGI(TAG,"client %i connected!",num);
      break;
    case WEBSOCKET_DISCONNECT_EXTERNAL:
      ESP_LOGI(TAG,"client %i sent a disconnect message",num);
      gpio_set_level(BACNET_LED, 0);
      break;
    case WEBSOCKET_DISCONNECT_INTERNAL:
      ESP_LOGI(TAG,"client %i was disconnected",num);
      break;
    case WEBSOCKET_DISCONNECT_ERROR:
      ESP_LOGI(TAG,"client %i was disconnected due to an error",num);
      gpio_set_level(BACNET_LED, 0);
      break;
    case WEBSOCKET_TEXT:
      if(len) {
        switch(msg[0]) {
          case 'L':
            if(sscanf(msg,"L%i",&value)) {
              ESP_LOGI(TAG,"LED value: %i",value);
              if(value==0){
                  gpio_set_level(BACNET_LED, 0);
              }
              else
              {
                  gpio_set_level(BACNET_LED, 1);
              }
              ws_server_send_text_all_from_callback(msg,len); // broadcast it!
            }
        }
      }
      break;
    case WEBSOCKET_BIN:
      ESP_LOGI(TAG,"client %i sent binary message of size %i:\n%s",num,(uint32_t)len,msg);
      break;
    case WEBSOCKET_PING:
      ESP_LOGI(TAG,"client %i pinged us with message of size %i:\n%s",num,(uint32_t)len,msg);
      break;
    case WEBSOCKET_PONG:
      ESP_LOGI(TAG,"client %i responded to the ping",num);
      break;
  }
}

static httpserver httpd80(80, BACNET_LED, websocket_callback);


/* BACnet handler, stack init, IAm */
void StartBACnet()
{
    /* we need to handle who-is to support dynamic device binding */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);

    /* set the handler for all the services we don't implement */
    /* It is required to send the proper reject message... */ 
    apdu_set_unrecognized_service_handler_handler
       (handler_unrecognized_service);
    /* Set the handlers for any confirmed services that we support. */
    /* We must implement read property - it's required! */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY,
        handler_read_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROP_MULTIPLE,
        handler_read_property_multiple);
    
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROPERTY,
        handler_write_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_SUBSCRIBE_COV,
        handler_cov_subscribe);    
     
    address_init();   
    bip_init(NULL); 
    Send_I_Am(&Handler_Transmit_Buffer[0]);
}

/* wifi events handler : start & stop bacnet with an event  */
extern "C" {
    esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
    {
        const char* LOG_TAG = "WIFI_EVT_HDLR";
        switch(event->event_id) {
            case SYSTEM_EVENT_AP_START:
                ESP_LOGI(LOG_TAG,"SYSTEM_EVENT_AP_START");
                break;
            case SYSTEM_EVENT_AP_STOP:
                ESP_LOGI(LOG_TAG,"SYSTEM_EVENT_AP_STOP");
                break;
            case SYSTEM_EVENT_AP_STACONNECTED:
                ESP_LOGI(LOG_TAG, "SYSTEM_EVENT_AP_STACONNECTED station:" MACSTR " join, AID=%d", MAC2STR(event->event_info.sta_connected.mac), event->event_info.sta_connected.aid);
                break;
            case SYSTEM_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(LOG_TAG, "SYSTEM_EVENT_AP_STADISCONNECTED station:" MACSTR "leave, AID=%d", MAC2STR(event->event_info.sta_disconnected.mac), event->event_info.sta_disconnected.aid);
                break;
            case SYSTEM_EVENT_AP_PROBEREQRECVED:
                ESP_LOGI(LOG_TAG,"SYSTEM_EVENT_AP_PROBEREQRECVED");
                break;
            case SYSTEM_EVENT_STA_START:
                ESP_LOGI(LOG_TAG, "SYSTEM_EVENT_STA_START");
                ESP_ERROR_CHECK(esp_wifi_connect());
                break;
            case SYSTEM_EVENT_STA_CONNECTED:
                break ;
            case SYSTEM_EVENT_STA_GOT_IP:
                ESP_LOGI(LOG_TAG, "SYSTEM_EVENT_STA_GOT_IP;\n  ip:%s\n  m:%s\n  gw:%s", 
                    ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip),
                    ip4addr_ntoa(&event->event_info.got_ip.ip_info.netmask),
                    ip4addr_ntoa(&event->event_info.got_ip.ip_info.gw)
                    );
                if (xEventGroupGetBits(s_wifi_event_group)!=WIFI_CONNECTED_BIT)
                {            
                    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                    StartBACnet();
                }
                break;
            case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(LOG_TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
                ESP_ERROR_CHECK(esp_wifi_connect()); 
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                bip_cleanup();
                break;
            default:
                break;
        }
        return ESP_OK;
    }
}

void wifi_init_sta(void *arg)
{
    s_wifi_event_group = xEventGroupCreate();
    
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, arg));

    strcpy((char*)wifi_config.ap.ssid, CONFIG_WIFI_SSID);
    strcpy((char*)wifi_config.ap.password, CONFIG_WIFI_PASSWORD);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(LOG_TAG, "wifi init finished, trying to connect to ap SSID:%s password:%s",
             wifi_config.sta.ssid, wifi_config.sta.password);
}

void wifi_init_softap(void *arg)
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    
    //Setting Access points IP things manualle
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    tcpip_adapter_ip_info_t info;
    memset(&info, 0, sizeof(info));
    IP4_ADDR(&info.ip, 192, 168, 192, 1);
    IP4_ADDR(&info.gw, 192, 168, 192, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    ESP_LOGI(LOG_TAG,"setting gateway IP");
    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
    ESP_LOGI(LOG_TAG,"starting DHCPS adapter");
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));


    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, arg));

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    strcpy((char*)wifi_config.ap.ssid, CONFIG_WIFI_SSID);
    strcpy((char*)wifi_config.ap.password, CONFIG_WIFI_PASSWORD);
    wifi_config.ap.channel=0;
    wifi_config.ap.authmode=WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.ssid_hidden=0;
    wifi_config.ap.max_connection=4;
    wifi_config.ap.beacon_interval=180;


    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(LOG_TAG,"WiFi set up finished");
}

void SensorsTask(void *pvParameters)
{
    char jsonbuffer[512];
    BME280 bme280 = BME280();
    bme280.setDebug(true);
    bme280.init(SDA_PIN, SCL_PIN, BME280_ADDR);

    cJSON *root, *info, *d;
    while (1) {
        bme280_reading_data sensor_data = bme280.readSensorData();
        processImage.temp=sensor_data.temperature;
        processImage.humid= sensor_data.humidity;
        
        root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "d", d = cJSON_CreateObject());
        cJSON_AddItemToObject(root, "info", info = cJSON_CreateObject());
        cJSON_AddStringToObject(d, "myName", "BacnetSmartActor");
        cJSON_AddNumberToObject(d, "temperature", processImage.temp);
        cJSON_AddNumberToObject(d, "humidity", processImage.humid);
        cJSON_AddNumberToObject(info, "heap", esp_get_free_heap_size());
        cJSON_AddStringToObject(info, "sdk", esp_get_idf_version());
        cJSON_AddNumberToObject(info, "time", esp_log_timestamp());

        cJSON_PrintPreallocated(root, jsonbuffer, sizeof(jsonbuffer) / sizeof(jsonbuffer[0]), false);
        cJSON_Delete(root);

        printf("Temperature:%.2foC, Humidity: %.2f%%, Pressure: %.2fPa\n",
               (double) sensor_data.temperature,
               (double) sensor_data.humidity,
               (double) sensor_data.pressure
        );
        ws_server_send_text_all(jsonbuffer, strlen(jsonbuffer));
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

void HttpdTask(void *pvParameters)
{
    httpserver *myhttpd = (httpserver *)pvParameters;
    ESP_LOGI(LOG_TAG, "Starting the HttpdTask on port %d", myhttpd->GetPort());
    myhttpd->TaskServer();
}

void HttpdHandlerTask(void *pvParameters)
{
    httpserver *myhttpd = (httpserver *)pvParameters;
    ESP_LOGI(LOG_TAG, "Starting the HttpdHandlerTask on port %d", myhttpd->GetPort());
    myhttpd->TaskServerHandler();
}

/* Bacnet Task */
void BACnetTask(void *pvParameters)
{  
    uint16_t pdu_len = 0;
    BACNET_ADDRESS src = {};
    unsigned timeout = 1;  

    Device_Init(NULL);
    Device_Set_Object_Instance_Number(12);
   
    uint32_t tickcount=xTaskGetTickCount();

    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS); // could be remove to speed the code

        // do nothing if not connected to wifi
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
        { 
            uint32_t newtick=xTaskGetTickCount();

            // one second elapse at least (maybe much more if Wifi was deconnected for a long)
            if ((newtick<tickcount)||((newtick-tickcount)>=configTICK_RATE_HZ))
            {
                tickcount=newtick;
                dcc_timer_seconds(1);
                bvlc_maintenance_timer(1); 
                handler_cov_timer_seconds(1);
                tsm_timer_milliseconds(1000);

                // Read analog values from internal sensors
                Analog_Input_Present_Value_Set(0,temprature_sens_read());
                Analog_Input_Present_Value_Set(1,hall_sens_read());

            }

            pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout);
            if (pdu_len) 
            {                
                npdu_handler(&src, &Rx_Buf[0], pdu_len);

                if(Binary_Output_Present_Value(0)==BINARY_ACTIVE)
                    gpio_set_level(BACNET_LED,1);
                else
                    gpio_set_level(BACNET_LED,0);       
            }

            handler_cov_task();
        }
    }
}

/* setup gpio & nv flash, call wifi init code */
esp_err_t setup_peripherials()
{
    gpio_pad_select_gpio(BACNET_LED);
    gpio_set_direction(BACNET_LED, GPIO_MODE_OUTPUT);

    gpio_set_level(BACNET_LED,0);

    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_OK)
    {
        return ret;
    }
    nvs_flash_erase();
    ret = nvs_flash_init();
    
    return ret;
}

/* Entry point */
extern "C" {
    void app_main()
    {    
        ESP_LOGI(LOG_TAG, "app_main reached");
        nvs_flash_init();
        setup_peripherials();
        
        
        wifi_init_softap(NULL); 

        ws_server_start();
        xTaskCreate(
            BACnetTask,     /* Function to implement the task */
            "BACnetTask",   /* Name of the task */
            10000,          /* Stack size in words */
            NULL,           /* Task input parameter */
            20,             /* Priority of the task */
            NULL);          /* Task handle. */ 
        xTaskCreate(
            HttpdTask,     /* Function to implement the task */
            "HttpdTask80",   /* Name of the task */
            3000,          /* Stack size in words */
            &httpd80,           /* Task input parameter */
            9,             /* Priority of the task */
            NULL);          /* Task handle. */ 
        xTaskCreate(
            HttpdHandlerTask,     /* Function to implement the task */
            "HttpdHandlerTask80",   /* Name of the task */
            4000,          /* Stack size in words */
            &httpd80,           /* Task input parameter */
            6,             /* Priority of the task */
            NULL);          /* Task handle. */ 
        xTaskCreate(
            SensorsTask,     /* Function to implement the task */
            "SensorsTask",   /* Name of the task */
            4000,          /* Stack size in words */
            NULL,           /* Task input parameter */
            6,             /* Priority of the task */
            NULL);          /* Task handle. */ 
    }
}
