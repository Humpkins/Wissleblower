#include <Arduino.h>
#include <main.h>
#include "secrets.h"

#include "./config.h"
#include "./5.Utils/utils.h"

#include "./0.ESPServer/ESPServer.h"
#include "./2.MPU6050/MPU6050.h"
#include "./8.TJA1050/TJA1050.h"
#include "./4.SIM7000G/SIM7000G.h"
#include "./9.LOGGER/logger.h"
#include "./3.MQTT/MQTT.h"
#include "./7.Watchers/overcurrent.h"

#include "./highFrequencyMQTT/highFrequencyMQTT.h"
#include "./mediumFrequencyMQTT/mediumFrequencyMQTT.h"

#define LED_PIN     GPIO_NUM_12

#define TASK_QTY 2

//  Toggle the high frequency task state
void toggleHighFreq( bool state ) {
    if ( state ) vTaskResume( xHighFreq );
    else vTaskSuspend( xHighFreq );
}

//  Sends the high frequency data over mqtt
void TaskHighFreq( void * parameters ){
    //  Suspend itself and wait to be awaken
    vTaskSuspend(NULL);

    static TickType_t xDelay = g_states.MQTTHighPeriod / portTICK_PERIOD_MS;

    for (;;) {
        // Serial.print("Getting TJA data");
        if ( !TJA_DATA.UpdateSamples() ) Serial.println("       [TJA ERROR]  Handdle TJA packet loss");

        // Serial.print("Getting MPU data");
        if ( !MPU_DATA.UpdateSamples() ) Serial.println("       [TJA ERROR]  Handdle MPU sampling error");

        vTaskDelay( xDelay );
    }
}

//  Toggle the medium frequency task state
void toggleMediumFreq( bool state ) {
    if ( state ) {
        sim_7000g.turnGPSOn();
        vTaskResume( xMediumFreq );
    } else {
        vTaskSuspend( xMediumFreq );
        sim_7000g.turnGPSOff();
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
    // if ( !modem.isGprsConnected() ) sim_7000g.maintainGPRSconnection();
    Serial.print("GPRS connection");
    if ( !sim_7000g.checkConnection() ) {
        Serial.println("        [FALSE]");
        sim_7000g.maintainGPRSconnection();
    } else Serial.println("        [OK]");
    
    //  Check if MQTT connection is close, if it is, re-opens
    Serial.print("MQTT connection");
    if ( !mqtt.connected() ) {
        Serial.println("        [FALSE]");
        mqtt_com.maintainMQTTConnection();
    } else Serial.println("        [OK]");

}

//  Toggle the MQTT delivery task state
void toggleMQTTFreq( bool state ) {
    ( state )?vTaskResume( xMQTTDeliver ):vTaskSuspend( xMQTTDeliver );
}

// Task that sends data over MQTT
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

//  Watch and logs for vehicle state warnings
void TaskWatchers ( void * parameters ) {

    //  All watchers methods
    for(;;){
        if ( !WatcherCurrent.isSetedUp ){
            // Sets up the watchers class
            if ( xSemaphoreTake( xSD, portMAX_DELAY ) == pdTRUE) {
                WatcherCurrent.setup();
                WatcherCurrent.isSetedUp = 1;
                xSemaphoreGive(xSD);
            }
        } else {
            if ( xSemaphoreTake( xSD, portMAX_DELAY ) == pdTRUE ){
                WatcherCurrent.current();
                xSemaphoreGive(xSD);
            }
        }
        vTaskDelay( 3000 / portTICK_PERIOD_MS );
    }
}

//  Task that Logs the data
void TasklogData ( void * parameters ) {
    // Suspend itself and wait to be awaken
    vTaskSuspend( NULL );

    for (;;) {
        if ( xSemaphoreTake( xSD, portMAX_DELAY ) == pdTRUE ) {
            logger.updateFile();
            xSemaphoreGive(xSD);
        }
        vTaskDelay( 1000 / portTICK_PERIOD_MS );
    }
}

//  Task that update board timestamp
void taskUpdateTimesTamp( void * parameters ){
    for (;;){
        if ( xSemaphoreTake( xModem, portMAX_DELAY ) == pdTRUE ) {
            sim_7000g.updateDateTime();
            xSemaphoreGive( xModem );
        }
        vTaskDelay( (750) / portTICK_PERIOD_MS );
    }
}

void taskLoopClientMQTT ( void * parameters ) {
    char wakeTopic[51];
    sprintf( wakeTopic, "%s/%s", g_states.MQTTclientID, g_states.MQTTWakeTopic );
    TickType_t maintainConnectionUp_Time = xTaskGetTickCount();
    TickType_t updateTimestamp = xTaskGetTickCount();

    for(;;){

        if ( xSemaphoreTake( xModem, portMAX_DELAY ) == pdTRUE ) {
            mqtt.loop();

            //  Prevent from Idle disconnection
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

    //  Sets up the MPU6050 class
    MPU_DATA.setup();
    //  Sets up the TJA1050 class
    TJA_DATA.setup();
    //  Sets up the SD class
    logger.setup();

    xModem = xSemaphoreCreateMutex();
    xSD = xSemaphoreCreateMutex();

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
    // Sets up the OTA class
    OTA.setup();

    //  Sendo good morning message
    char topic_Wake[ sizeof(g_states.MQTTclientID) + sizeof(g_states.MQTTWakeTopic) + 2 ];
    sprintf( topic_Wake, "%s/%s", g_states.MQTTclientID, g_states.MQTTWakeTopic );
    mqtt.publish( topic_Wake, "Just woke. Good morning!" );

    delay(1000);

    xTaskCreatePinnedToCore( TaskMediumFreq, "Medium frequency data task", 4096, NULL, 2, &xMediumFreq, 0 );
    xTaskCreatePinnedToCore( TaskHighFreq, "High frequency data task", 2048, NULL, 2, &xHighFreq, 0 );
    // xTaskCreatePinnedToCore( TaskWatchers, "Watcher for state warnings", 2048, NULL, 3, NULL, 0 );
    xTaskCreatePinnedToCore( TasklogData, "Logs data to SD card", 3072, NULL, 3, &xLog, 0 );

    xTaskCreatePinnedToCore( taskSendOverMQTT, "MQTT data delivery task", 4096, NULL, 2, &xMQTTDeliver, 1 );
    xTaskCreatePinnedToCore( taskLoopClientMQTT, "MQTT client loop", 9216, NULL, 1, &xMQTTLoop, 1 );

    xTaskCreatePinnedToCore( taskUpdateTimesTamp, "Update board's Timestamp", 2048, NULL, 4, NULL, 0 );
    xTaskCreatePinnedToCore( heartBeat, "Blinks the LED", 1024, NULL, 1, &xHeartBeat, 0 );
    
}

//  We'll not need this task, so we delete it
void loop(){ vTaskDelete(NULL); };