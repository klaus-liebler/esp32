/*C Includes*/
#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include <string.h>
/*RTOS Includes*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"


#include "nvs_flash.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_event_loop.h"


#include "sdkconfig.h"
#include "esp_err.h"

#include "hx711.h"

#include "math.h"



void task_readSensors(void *ignore)
{

    constexpr long UMIN = 900000;
    constexpr long UMAX = 8000000;
    constexpr float RMIN = 55.0;
    constexpr float RMAX = 400.0;

    constexpr long Uu = 1013784; // Rohmesswert unteres Ende
    constexpr long Uo = 6968595; // Rohmesswert oberes Ende
    constexpr float Ru = 56.0;   // Widerstandswert unteres Ende
    constexpr float Ro = 390.0;  // Widerstandswert oberes Ende

    long Umess;
    float Rx, T;

    whs::HX711 get_U(GPIO_NUM_19, GPIO_NUM_18, RMT_CHANNEL_0, RMT_CHANNEL_1, whs::InputAndGain::B32);

    int i;
    long buf = 0;
    long U = 0;

    printf("HX711 Temperaturmessung mit Pt100 Widerstandsthermometer");

    // Abgleich Messbereichsgrenze unten
    if (Uu <= UMIN || Ru == RMIN)
    {
        printf("Widerstand fuer untere Messbereichsgrenze eingesetzt ??\n");
        printf("Messung fuer Messbereichsgrenze unten laeuft - bitte warten ...\n");
        for (i = 0; i < 5; i++)
        {
            U = get_U.read_average(40);
            buf = buf + U;
            printf("%ld\n", U);
        }
        if (buf / 5 < UMIN || buf / 5 > UMAX)
        {
            printf("\n\n");
            printf("Messfehler, bitte Schaltung ueberpruefen");
            while (1)
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            };
        }
        else
        {
            printf("\n\nBitte %ld bei Variable 'Uu' und den Wert des eingesetzten Widerstandes in Ohm bei Variable 'Ru' eintragen. Dann das Programm neu hochladen.", buf / 5);
            while (1)
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            };
        }
    }

    // Abgleich Messbereichsgrenze oben
    if (Uo <= UMIN || Ro == RMIN)
    {
        printf("Widerstand fuer obere Messbereichsgrenze eingesetzt ??");
        printf("Messung fuer Messbereichsgrenze oben laeuft - bitte warten ...");
        for (i = 0; i < 5; i++)
        {
            U = get_U.read_average(40);
            buf = buf + U;
            printf("%ld\n", U);
        }
        if (buf / 5 < UMIN || buf / 5 > UMAX)
        {
            printf("\n\n");
            printf("Messfehler, bitte Schaltung ueberpruefen");
            while (true)
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            };
        }
        else
        {
            printf("\n\nBitte %ld bei Variable 'Uo' und den Wert des eingesetzten Widerstandes in Ohm  bei Variable 'Ro' eintragen. Dann das Programm neu hochladen.", buf/5);
            while (1)
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            };
        }
    }

    // Prüfung der Abgleichwerte
    if (Uu < UMIN || Uu > UMAX || Uo < UMIN || Uo > UMAX || Ru < RMIN || Ru > RMAX || Ro < RMIN || Ro > RMAX ||
        Uu > Uo || Uo < Uu || Ru > Ro || Ro < Ru)
    {
        printf("Abgleichfehler - Bitte Abgleich wiederholen");
        while (1)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        };
    }
    else
    {
        printf("Abgleich plausibel\n\n");
    }

    float k1, k2, k3, k4, k5, k6, k7, k8, k9;
    int cnt=0;
    while (cnt<3)
    {
        cnt++;
        // Messwert für die Spannung an Rx einlesen
        Umess = get_U.read();

        // Aus der gemessenen Spannung den Widerstand Rx ausrechnen
        if (Umess >= UMIN && Umess <= UMAX)
        {
            Rx = ((((Ro - Ru) / (Uo - Uu)) * (Umess - Uu)) + Ru);

            // Temperatur für Rx >= 100 Ohm berechnen
            if (Rx >= 100.0)
            {
                k1 = 3.90802 * pow(10, -1);
                k2 = 2 * 5.802 * pow(10, -5);
                k3 = pow(3.90802 * pow(10, -1), 2);
                k4 = 4.0 * (pow(5.802 * pow(10, -5), 2));
                k5 = Rx - 100.0;
                k6 = 5.802 * pow(10, -5);

                k7 = k1 / k2;
                k8 = (k3 / k4) - (k5 / k6);
                k9 = sqrt(k8);

                T = k7 - k9;
            }
            // Temperatur für Rx < 100 Ohm berechnen
            else
            {
                k1 = pow(Rx, 5) * 1.597 * pow(10, -10);
                k2 = pow(Rx, 4) * 2.951 * pow(10, -8);
                k3 = pow(Rx, 3) * 4.784 * pow(10, -6);
                k4 = pow(Rx, 2) * 2.613 * pow(10, -3);
                k5 = 2.219 * Rx - 241.9;

                T = k1 - k2 - k3 + k4 + k5;
            }
            printf("R = %f Ohm ->T= %f °C\n", Rx, T);
        }
        else
        {
            printf("Messfehler! Umess = %ld\n", Umess);
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    while(1)
    {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

extern "C"
{
    void app_main()
    {
        xTaskCreatePinnedToCore(&task_readSensors, "task_readSensors", 4096, NULL, 6, NULL, 1);
    }
}
