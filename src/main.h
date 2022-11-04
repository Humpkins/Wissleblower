#include <Arduino.h>

TaskHandle_t xMediumFreq;
TaskHandle_t xHighFreq;
TaskHandle_t xMQTTDeliver;

SemaphoreHandle_t xModem = NULL;