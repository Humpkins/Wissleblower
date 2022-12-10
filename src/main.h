#include <Arduino.h>

TaskHandle_t xMediumFreq;
TaskHandle_t xHighFreq;
TaskHandle_t xMQTTDeliver;
TaskHandle_t xMQTTLoop;
TaskHandle_t xHeartBeat;

SemaphoreHandle_t xModem = NULL;