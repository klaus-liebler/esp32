#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "bacdef.h"

#include "config.h"
#include "txbuf.h"
#include "client.h"

#include "handlers.h"
#include "datalink.h"
#include "dcc.h"
#include "tsm.h"
// conflict filename address.h with another file in default include paths
#include "../../../components/bacnet-stack/include/address.h"
#include "bip.h"

#include "device.h"
#include "ai.h"
#include "bo.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "driver/gpio.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "httpserver/httpserver.h"

#define TAG = "MAIN";

// hidden function not in any .h files
extern uint8_t temprature_sens_read();
extern uint32_t hall_sens_read();


#include "passwords.h" //defines structure wifi_config_t wifi_config ...

// ESP32 DevKit has LED on GPIO2
constexpr gpio_num_t BACNET_LED = GPIO_NUM_2;

uint8_t Handler_Transmit_Buffer[MAX_PDU] = { 0 };
uint8_t Rx_Buf[MAX_MPDU] = { 0 };

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

static httpserver httpd(80, BACNET_LED);

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
        switch(event->event_id) {
            case SYSTEM_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d", MAC2STR(event->event_info.sta_connected.mac), event->event_info.sta_connected.aid);
                break;
            case SYSTEM_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d", MAC2STR(event->event_info.sta_disconnected.mac), event->event_info.sta_disconnected.aid);
                break;
            case SYSTEM_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case SYSTEM_EVENT_STA_CONNECTED:
                break ;
            case SYSTEM_EVENT_STA_GOT_IP:
                ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
                if (xEventGroupGetBits(s_wifi_event_group)!=WIFI_CONNECTED_BIT)
                {            
                    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                    StartBACnet();
                    httpd.Start()
                }
                break;
            case SYSTEM_EVENT_STA_DISCONNECTED:
                esp_wifi_connect(); 
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                ESP_LOGI(TAG,"retry to connect to the AP");
                bip_cleanup();
                break;
            default:
                break;
        }
        return ESP_OK;
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL) );


    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             wifi_config.sta.ssid, wifi_config.sta.password);
}

void wifi_init_softap()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

void Httpd80_Task(void *pvParameters)
{
    httpd.Task();
}


/* Bacnet Task */
void BACnetTask(void *pvParameters)
{  
    uint16_t pdu_len = 0;
    BACNET_ADDRESS src = {
        0
    };
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
        ESP_LOGI(TAG, "app_main reached");
        nvs_flash_init();
        system_init();
        setup_peripherials();
        
        wifi_init_sta(); 
        // Cannot run BACnet code here, the default stack size is to small : 4096 byte
        xTaskCreate(
            BACnetTask,     /* Function to implement the task */
            "BACnetTask",   /* Name of the task */
            10000,          /* Stack size in words */
            NULL,           /* Task input parameter */
            20,             /* Priority of the task */
            NULL);          /* Task handle. */   
        xTaskCreate(
            Httpd80_Task,     /* Function to implement the task */
            "Httpd80_Task",   /* Name of the task */
            4096,          /* Stack size in words */
            NULL,           /* Task input parameter */
            20,             /* Priority of the task */
            NULL);          /* Task handle. */   
    }
    }
}
