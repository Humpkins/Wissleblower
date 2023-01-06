#include <Arduino.h>

TaskHandle_t xMediumFreq;
TaskHandle_t xHighFreq;
TaskHandle_t xMQTTDeliver;
TaskHandle_t xMQTTLoop;
TaskHandle_t xHeartBeat;
TaskHandle_t xLog;

SemaphoreHandle_t xModem = NULL;
SemaphoreHandle_t xSD = NULL;

void toggleHighFreq( bool state );
void toggleMediumFreq( bool state );
void toggleMQTTFreq( bool state );