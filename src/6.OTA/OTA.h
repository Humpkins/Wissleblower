#include <otadrive_esp.h>

TinyGsmClient OTAclient( modem, 1 );

void update_prgs(size_t current, size_t total){

    Serial.printf( "upgrade %d/%d %.2f%%\n", current, total, (100 * (float)current / (float)total) );
}

class otaClass {

    public:
        void setup(){
            OTADRIVE.setInfo( g_states.APIKey, g_states.Ver );
            OTADRIVE.onUpdateFirmwareProgress(update_prgs);
        }

        void loop() {
            //  Suspend all tasks but the current
            vTaskSuspend(xMediumFreq);
            vTaskSuspend(xHighFreq);
            vTaskSuspend(xHeartBeat);

            char topicOTA[ sizeof(g_states.MQTTclientID) + sizeof(g_states.MQTTOTATopic) + 2 ];
            sprintf( topicOTA, "%s/%s", g_states.MQTTclientID, g_states.MQTTOTATopic );

            char ListenTopic[ sizeof(g_states.MQTTclientID) + sizeof(g_states.MQTTListenTopic) + 2 ];
            sprintf( ListenTopic, "%s/%s", g_states.MQTTclientID, g_states.MQTTListenTopic );

            Serial.println("Starting FOTA sequence...");
            mqtt.publish( topicOTA, "Starting FOTA sqeuence..." );

            auto OTAStatus = OTADRIVE.updateFirmware( OTAclient, false );
            
            int compare = strcmp( OTAStatus.toString().c_str(), "Firmware already uptodate.\n" );
            
            //  If update complete, restar system   
            if ( compare > 0 ) {
                char response[] = "Firmware update complete. Rebooting system. I'll be back in 1 minute";
                Serial.println(response);
                mqtt.publish(ListenTopic, response);
                ESP.restart();
            } else {
                Serial.println(OTAStatus.toString().c_str());
                mqtt.publish(ListenTopic, OTAStatus.toString().c_str());
            }

            vTaskResume(xHeartBeat);
        }
            
};

otaClass OTA;