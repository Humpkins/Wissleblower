#include <Arduino.h>

#include "./config.h"

#include "./1.MCP2515/MCP2515.h"
#include "./2.MPU6050/MPU6050.h"
#include "./4.SIM7000G/SIM7000G.h"
#include "./3.MQTT/MQTT.h"

// #include "./highFrequencyMQTT/highFrequencyMQTT.h"
// #include "./mediumFrequencyMQTT/mediumFrequencyMQTT.h"

#define MCP_INT_PIN 13
#define MPU_INT_PIN 36
#define LED_PIN 12

#define TASK_QTY 2

TaskHandle_t * xMediumFreq;
TaskHandle_t * xHighFreq;
TaskHandle_t * xGPSTaskHanddler;

SemaphoreHandle_t xExternal_connection_semaphore = NULL;


//  Sends the high frequency data over mqtt
void TaskHighFreq( void * parameters ){

    static TickType_t xDelay = g_states.MQTTHighPeriod / portTICK_PERIOD_MS;

    for (;;) {

        // If all external connection is set
        if ( xSemaphoreTake( xExternal_connection_semaphore, 0 ) == pdTRUE ) {

            // Update the HIGH frequency data
            if ( !MCP_DATA.UpdateSamples() ) Serial.println("[ERROR]  Handdle MCP packet loss");
            if ( !MPU_DATA.UpdateSamples() ) Serial.println("[ERROR]  Handdle MPU sampling error");

            // Send high frequency data through mqtt
            Serial.println("HIGH freq sent");

            xSemaphoreGive( xExternal_connection_semaphore );

        } else {
            Serial.println("High frequency task was blocked by Mutex");
        }

        vTaskDelay( xDelay );
    }
}

//  Sends the medium frequency data over mqtt
void TaskMediumFreq( void * parameters ){

    static TickType_t xDelay = g_states.MQTTMediumPeriod / portTICK_PERIOD_MS;

    for (;;) {

        // If all external connection is set
        if ( xSemaphoreTake( xExternal_connection_semaphore, 0 ) == pdTRUE ) {

            // Update only the MEDIUM frequency data as the high frequency data is already updated by this point
            if ( !sim_7000g.update_GPS_data() ) Serial.println("[ERROR]  Handdle GPS sampling error");
            if ( !sim_7000g.update_GPRS_data() ) Serial.println("[ERROR]  Handdle GPRS sampling error");

            // Send medium frequency data through mqtt
            Serial.println("MEDIUM freq sent");
            
            xSemaphoreGive( xExternal_connection_semaphore );

        } else {
            Serial.println("Medium frequency task was blocked by Mutex");
        }

        vTaskDelay( xDelay );
    }
}

// Task to orchestrate mqtt connection
void maintainExternalConnection( void * parameters ){
    static TickType_t xDelay = 3000 / portTICK_PERIOD_MS;

    for (;;) {

        if ( !modem.isGprsConnected() || !mqtt.connected() ){

            // Locks the remaining tasks so they don't try to send data over MQTT
            if ( xSemaphoreTake(xExternal_connection_semaphore, portMAX_DELAY ) == pdTRUE ){
                if ( xSemaphoreTake(xExternal_connection_semaphore, portMAX_DELAY ) == pdTRUE ){
                
                //  Check if GPRS connection is close, if it is, re-opens
                if ( !modem.isGprsConnected() ) sim_7000g.maintainGPRSconnection();

                vTaskDelay( 2500 / portTICK_PERIOD_MS );

                //  Check if MQTT connection is close, if it is, re-opens
                if ( !mqtt.connected() ) mqtt_com.maintainMQTTConnection();
                
                // Frees the remaining tasks to comunicate over GPRS and MQTT
                xSemaphoreGive(xExternal_connection_semaphore);
                }
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

    xExternal_connection_semaphore = xSemaphoreCreateCounting( TASK_QTY, TASK_QTY);
    
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

    xTaskCreatePinnedToCore( heartBeat, "Blinks the LED", 1024, NULL, 1, NULL, 1 );

    xTaskCreatePinnedToCore( TaskMediumFreq, "Medium frequency data task", 4096, NULL, 2, xMediumFreq, 1 );
    // vTaskSuspend( xMediumFreq );

    xTaskCreatePinnedToCore( TaskHighFreq, "High frequency data task", 2048, NULL, 2, xHighFreq, 1 );
    // vTaskSuspend( xHighFreq );

    xTaskCreatePinnedToCore( maintainExternalConnection, "GPRS MQTT connection", 2048, NULL, 3, NULL, 1 );
    
}

void loop(){ vTaskDelete(NULL); };