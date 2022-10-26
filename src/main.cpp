#include <Arduino.h>

#include "./config.h"

#include "./1.MCP2515/MCP2515.h"
#include "./2.MPU6050/MPU6050.h"
#include "./4.SIM7000G/SIM7000G.h"
#include "./3.MQTT/MQTT.h"

#include "./highFrequencyMQTT/highFrequencyMQTT.h"
#include "./mediumFrequencyMQTT/mediumFrequencyMQTT.h"

#define MCP_INT_PIN 13
#define MPU_INT_PIN 36
#define LED_PIN 12

#define TASK_QTY 2

TaskHandle_t xMediumFreq;
TaskHandle_t xHighFreq;
TaskHandle_t xMQTTMediumFreq;
TaskHandle_t xMQTTHighFreq;

SemaphoreHandle_t xExternal_connection_semaphore = NULL;


//  Sends the high frequency data over mqtt
void TaskHighFreq( void * parameters ){

    static TickType_t xDelay = g_states.MQTTHighPeriod / portTICK_PERIOD_MS;

    for (;;) {
        for ( int i = 0; i < 2; i ++ ){
            // Update the HIGH frequency data
            Serial.print("Getting MCP data");
            if ( !MCP_DATA.UpdateSamples() ) Serial.println("       [ERROR]  Handdle MCP packet loss");
            else Serial.println("       [OK]");

            Serial.print("Getting MCP data");
            if ( !MPU_DATA.UpdateSamples() ) Serial.println("       [ERROR]  Handdle MPU sampling error");
            else Serial.println("       [OK]");

            vTaskDelay( xDelay );
        }

        vTaskResume(xMediumFreq);
        vTaskSuspend(NULL);

    }
}

//  Sends the medium frequency data over mqtt
void TaskMediumFreq( void * parameters ){

    static TickType_t xDelay = g_states.MQTTMediumPeriod / portTICK_PERIOD_MS;
    vTaskSuspend(NULL);

    for (;;) {

        // Update only the MEDIUM frequency data as the high frequency data is already updated by this point
        Serial.println("Getting GPS data");
        if ( !sim_7000g.updateGPSData() ) Serial.println("        [ERROR]  Handdle GPS sampling error");
        else Serial.println("       [OK]");
        vTaskResume(xHighFreq);
        vTaskSuspend(NULL);

        Serial.println("Getting GPRS data");
        if ( !sim_7000g.update_GPRS_data() ) Serial.println("       [ERROR]  Handdle GPRS sampling error");
        else Serial.println("       [OK]");
        vTaskResume(xHighFreq);
        vTaskSuspend(NULL);
    }
}

void taskMediumFreqMQTT( void * parameters ){
    
    vTaskSuspend(NULL);
    static TickType_t xDelay = g_states.MQTTMediumPeriod / portTICK_PERIOD_MS;

    for (;;){
        if ( xSemaphoreTake(xExternal_connection_semaphore, (1500 / portTICK_PERIOD_MS) ) == pdTRUE ){
            for ( int i = 0; i < 1; i++ ){
                // Send medium frequency data through mqtt
                Serial.print("MEDIUM freq");
                mediumFrequency.sendMediumFrequencyDataOverMQTT();

                vTaskDelay( xDelay );
            }

            xSemaphoreGive(xExternal_connection_semaphore);

            vTaskResume(xMQTTHighFreq);
            vTaskSuspend(NULL);
        }
    }
}

void taskHighFreqMQTT( void * parameters ){
    static TickType_t xDelay = g_states.MQTTHighPeriod / portTICK_PERIOD_MS;

    for (;;){
        if ( xSemaphoreTake(xExternal_connection_semaphore, (1500 / portTICK_PERIOD_MS) ) == pdTRUE ){
            for ( int i = 0; i < 2; i++ ){                
                // Send high frequency data through mqtt
                Serial.print("HIGH freq");
                highFrequency.sendMHighFrequencyDataOverMQTT();

                vTaskDelay( xDelay );
            }

            xSemaphoreGive(xExternal_connection_semaphore);

            vTaskResume(xMQTTMediumFreq);
            vTaskSuspend(NULL);
        }
    }
}

void maintainExternalConnection( void * parameters ){
    static TickType_t xDelay = 60000 / portTICK_PERIOD_MS;

    for (;;) {
                            
        //  Check if GPRS connection is close, if it is, re-opens
        if ( !modem.isGprsConnected() ) {
            if ( xSemaphoreTake(xExternal_connection_semaphore, portMAX_DELAY ) == pdTRUE ) {
                sim_7000g.maintainGPRSconnection();
                xSemaphoreGive(xExternal_connection_semaphore);
            }
        }

        //  Check if MQTT connection is close, if it is, re-opens
        if ( !mqtt.connected() ) {
            if ( xSemaphoreTake(xExternal_connection_semaphore, portMAX_DELAY ) == pdTRUE ) {
                mqtt_com.maintainMQTTConnection();
                xSemaphoreGive(xExternal_connection_semaphore);
            }
        }

        vTaskDelay( xDelay );
    }
        
}

// Task to hartbeat by buildint LED
void heartBeat ( void * parameters ) {
    digitalWrite( LED_PIN, HIGH );

    for(;;){
        if ( digitalRead( LED_PIN ) ){
            digitalWrite( LED_PIN, LOW );
            vTaskDelay( 100 / portTICK_PERIOD_MS );
        } else {
            digitalWrite( LED_PIN, HIGH );
            vTaskDelay( 1000 / portTICK_PERIOD_MS );
        };
    }
}

void setup() {
    
    pinMode(LED_PIN, OUTPUT);

    xExternal_connection_semaphore = xSemaphoreCreateMutex();
    
    //  Sets up the MPU6050 class
    MPU_DATA.setup();
    // Sets up the MCP2515 class
    MCP_DATA.setup();

    Serial.begin(115200);

    // Prints the current CPU frequency
    uint32_t freq = getCpuFrequencyMhz();
    Serial.print("Current CPU frequency: ");
    Serial.print(freq);
    Serial.println(" MHz");

    // Sets up the SIM7000G GPRS class
    sim_7000g.setup();
    // Sets up the MQTT class
    mqtt_com.setup();

    delay(1000);

    xTaskCreatePinnedToCore( maintainExternalConnection, "GPRS MQTT connection", 2048, NULL, 3, NULL, 1 );

    // xTaskCreatePinnedToCore( TaskMediumFreq, "Medium frequency data task", 4096, NULL, 2, &xMediumFreq, 0 );
    // xTaskCreatePinnedToCore( TaskHighFreq, "High frequency data task", 2048, NULL, 2, &xHighFreq, 0 );

    xTaskCreatePinnedToCore( taskMediumFreqMQTT, "MQTT medium frequency data task", 2048, NULL, 2, &xMQTTMediumFreq, 1);
    xTaskCreatePinnedToCore( taskHighFreqMQTT, "MQTT high frequency data task", 2048, NULL, 2, &xMQTTHighFreq, 1);

    xTaskCreatePinnedToCore( heartBeat, "Blinks the LED", 1024, NULL, 1, NULL, 1 );
    
}

void loop(){ vTaskDelete(NULL); };