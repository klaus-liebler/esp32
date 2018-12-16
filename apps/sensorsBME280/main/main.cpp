#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include <BME280.h>


/* singed integer type*/
typedef int8_t s8;   /**< used for signed 8bit */
typedef int16_t s16; /**< used for signed 16bit */
typedef int32_t s32; /**< used for signed 32bit */
typedef int64_t s64; /**< used for signed 64bit */

typedef uint8_t u8;   /**< used for unsigned 8bit */
typedef uint16_t u16; /**< used for unsigned 16bit */
typedef uint32_t u32; /**< used for unsigned 32bit */
typedef uint64_t u64; /**< used for unsigned 64bit */

#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22
#define BME280_ADDR 0x76






void task_bme280_normal_mode(void *ignore)
{
    BME280 bme280 = BME280();
    bme280.setDebug(true);
    bme280.init(SDA_PIN, SCL_PIN, BME280_ADDR);

    while (1) {
        bme280_reading_data sensor_data = bme280.readSensorData();

        printf("Temperature: %.2foC, Humidity: %.2f%%, Pressure: %.2fPa\n",
               (double) sensor_data.temperature,
               (double) sensor_data.humidity,
               (double) sensor_data.pressure
        );
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}



extern "C"
{
    void app_main()
    {
        xTaskCreate(&task_bme280_normal_mode, "bme280_normal_mode", 2048, NULL, 6, NULL);
    }
}