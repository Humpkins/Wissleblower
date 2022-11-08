#include <Arduino.h>
#include <main.h>

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

//  Sends the high frequency data over mqtt
void TaskHighFreq( void * parameters ){
    //  Suspend itself and wait to be awaken
    vTaskSuspend(NULL);

    static TickType_t xDelay = g_states.MQTTHighPeriod / portTICK_PERIOD_MS;

    for (;;) {
        // Serial.print("Getting MCP data");
        if ( !MCP_DATA.UpdateSamples() ) Serial.println("       [MCP ERROR]  Handdle MCP packet loss");
        // else Serial.println("       [MCP OK]");

        // Serial.print("Getting MPU data");
        if ( !MPU_DATA.UpdateSamples() ) Serial.println("       [MCP ERROR]  Handdle MPU sampling error");
        // else Serial.println("       [MPU OK]");

        vTaskDelay( xDelay );
    }
}

//  Sends the medium frequency data over mqtt
void TaskMediumFreq( void * parameters ){
    //  Suspend itself and wait to be awaken
    vTaskSuspend(NULL);

    static TickType_t xDelay = g_states.MQTTMediumPeriod / portTICK_PERIOD_MS;

    for (;;) {

        // Update only the MEDIUM frequency data as the high frequency data is already updated by this point
        if ( xSemaphoreTake( xModem, ( 250 / portTICK_PERIOD_MS ) ) == pdTRUE ){
            // Serial.print("Getting GPS data");
            if ( !sim_7000g.updateGPSData() ) Serial.println("        [GPS ERROR]");
            // else Serial.println("       [GPS OK]");

            xSemaphoreGive( xModem );
        }

        if ( xSemaphoreTake( xModem, ( 250 / portTICK_PERIOD_MS ) ) == pdTRUE ){
            // Serial.print("Getting GPRS data");
            if ( !sim_7000g.update_GPRS_data() ) Serial.println("       [GPRS ERROR]");
            // else Serial.println("       [GPRS OK]");

            xSemaphoreGive( xModem );
        }

        vTaskDelay(xDelay);
    }
}

void reconnect () {

    //  Check if GPRS connection is close, if it is, re-opens
    if ( !modem.isGprsConnected() ) sim_7000g.maintainGPRSconnection();

    //  Check if MQTT connection is close, if it is, re-opens
    if ( !mqtt.connected() ) mqtt_com.maintainMQTTConnection();

}

void taskSendOverMQTT ( void * parameters ){
    //  Suspend itself and wait to be awaken
    vTaskSuspend(NULL);

    static TickType_t xHighDelay = g_states.MQTTHighPeriod / portTICK_PERIOD_MS;
    static TickType_t xMediumDelay = g_states.MQTTMediumPeriod / portTICK_PERIOD_MS;

    TickType_t itsHighTime = xTaskGetTickCount();
    TickType_t itsMediumTime = xTaskGetTickCount();

    for (;;) {

        if ( xTaskGetTickCount() - itsHighTime > xHighDelay ) {
            if ( xSemaphoreTake( xModem, ( 200 / portTICK_PERIOD_MS ) ) == pdTRUE ) {
                // Send high frequency data through mqtt
                if ( !highFrequency.sendMHighFrequencyDataOverMQTT() ) Serial.println("     [HIGH ERROR]");
                itsHighTime = xTaskGetTickCount();

                xSemaphoreGive( xModem );
            }
        }

        vTaskDelay( 10 / portTICK_PERIOD_MS );

        if ( xTaskGetTickCount() - itsMediumTime > xMediumDelay ) {
            if ( xSemaphoreTake( xModem, ( 200 / portTICK_PERIOD_MS ) ) == pdTRUE ) {
                // Send medium frequency data through mqtt
                if ( !mediumFrequency.sendMediumFrequencyDataOverMQTT() ) Serial.println("     [HIGH ERROR]");

                itsMediumTime = xTaskGetTickCount();

                xSemaphoreGive( xModem );
            }
        }

        vTaskDelay( 10 / portTICK_PERIOD_MS );
    }
}

void taskLoopClientMQTT ( void * parameters ) {
    char wakeTopic[51];
    sprintf( wakeTopic, "%s/%s", g_states.MQTTclientID, g_states.MQTTWakeTopic );
    TickType_t maintainConnectionUp_Time = xTaskGetTickCount();

    for(;;){

        if ( xSemaphoreTake( xModem, portMAX_DELAY ) == pdTRUE ) {
            mqtt.loop();

            // Prevent from Idle disconnection
            if ( xTaskGetTickCount() - maintainConnectionUp_Time > (7500 / portTICK_PERIOD_MS) ){
                if ( !mqtt.publish( wakeTopic, "I'm up..." ) ) reconnect();
                maintainConnectionUp_Time = xTaskGetTickCount();
            }

            xSemaphoreGive( xModem );
        }

        vTaskDelay( 1000 / portTICK_PERIOD_MS );
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

    xModem = xSemaphoreCreateMutex();
    
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

    //  Sendo good morning message
    char topic_Wake[ sizeof(g_states.MQTTclientID) + sizeof(g_states.MQTTWakeTopic) + 2 ];
    sprintf( topic_Wake, "%s/%s", g_states.MQTTclientID, g_states.MQTTWakeTopic );
    mqtt.publish( topic_Wake, "Just woke. Good morning!" );

    delay(1000);

    xTaskCreatePinnedToCore( TaskMediumFreq, "Medium frequency data task", 4096, NULL, 2, &xMediumFreq, 0 );
    xTaskCreatePinnedToCore( TaskHighFreq, "High frequency data task", 2048, NULL, 2, &xHighFreq, 0 );

    xTaskCreatePinnedToCore( taskSendOverMQTT, "MQTT data task", 4096, NULL, 2, &xMQTTDeliver, 1 );
    xTaskCreatePinnedToCore( taskLoopClientMQTT, "MQTT client loop", 4096, NULL, 1, NULL, 1 );

    xTaskCreatePinnedToCore( heartBeat, "Blinks the LED", 1024, NULL, 1, NULL, 0 );
    
}

//  We'll not need this task, so we delete it
void loop(){ vTaskDelete(NULL); };