#include <PubSubClient.h>
PubSubClient mqtt(GSMclient);

#include "../6.OTA/OTA.h"

#ifndef TINY_GSM_MODEM_SIM7000
    #include "../4.SIM7000G/SIM7000G.h"
#endif

#ifndef RECONNECT_ATTEMPT_LIMIT
    #define RECONNECT_ATTEMPT_LIMIT 3
#endif

#define TIME_BETWEEN_RECONNECTS_ATTEMPTS 1000
#define RECONNECT_THREAD_TIME 10000

//  Callback function
void someoneIsListenToUs(  char * topic, byte * payload, unsigned int length ) {

    //  Convert the incomming byte array
    char * incommingPayload = reinterpret_cast<char*>(payload);
    incommingPayload[length] = '\0';

    char topic_Wake[ sizeof(g_states.MQTTclientID) + sizeof(g_states.MQTTListenTopic) + 2 ];
    sprintf( topic_Wake, "%s/%s", g_states.MQTTclientID, g_states.MQTTListenTopic );

    //  If it is related to wake the sensors up or sleep them
    if ( strcmp( topic, topic_Wake ) == 0 ) {

        if ( strcmp( incommingPayload, "getUp!" ) == 0 ) {

            mqtt.publish( topic_Wake, "Yes master! Resuming the message system" );

            vTaskResume( xHighFreq );

            sim_7000g.turnGPSOn();
            vTaskResume( xMediumFreq );

            vTaskResume( xMQTTDeliver );

        } else if ( strcmp( incommingPayload, "goSleep" ) == 0 ) {

            mqtt.publish( topic_Wake, "Yes master! Suspending the message system" );

            vTaskSuspend( xHighFreq );

            vTaskSuspend( xMediumFreq );
            sim_7000g.turnGPSOff();

            vTaskSuspend( xMQTTDeliver );

        } else if ( strcmp( incommingPayload, "resetUrSelf" ) == 0 ) {
            mqtt.publish( topic_Wake, "Yes master! restarting the system. I'll be back in 1 minute" );
            ESP.restart();
        } else if ( strcmp( incommingPayload, "updateUrSelf" ) == 0 ) {
            mqtt.publish( topic_Wake, "Yes master! Starting FOTA sequence" );

            OTA.loop();
        }
    }

    return;
}

class MQTT {

    public:
        // Function to setup the first connection with the MQTT Broker
        void setup() {

            mqtt.setServer( g_states.MQTTHost, g_states.MQTTPort );
            mqtt.setCallback( someoneIsListenToUs );

            Serial.print("Connecting to MQTT Host ");
            Serial.print(g_states.MQTTHost);
            Serial.print(" with the following client ID ");
            Serial.print(g_states.MQTTclientID);

            // Connect to MQTT Broker without username and password
            mqtt.setBufferSize(1024);
            bool status = mqtt.connect( g_states.MQTTclientID, "Humpkinz", "BrokerPrivadoTCCTCU" );

            // Or, if you want to authenticate MQTT:
            //   bool status = mqtt.connect("GsmClientN", mqttUsername, mqttPassword);
            if ( !mqtt.connect( g_states.MQTTclientID ) ){
                Serial.println("       [FAIL]");
                Serial.println("[ERROR]    Handdle Broker connection fail");
                ESP.restart();
                while(1);
            } else {
                Serial.println("       [OK]");

                //  Subscribe to topic
                char topic[sizeof(g_states.MQTTclientID) + sizeof(g_states.MQTTListenTopic) + 2];
                sprintf( topic, "%s/%s", g_states.MQTTclientID, g_states.MQTTListenTopic );

                Serial.print("Subscribing to ");
                Serial.print(topic);
                if ( !mqtt.subscribe(topic, 0) ) Serial.println("      [FAIL]");
                else Serial.println("      [OK]");
            }

        }

        // Function to reconect to MQTT broker if it is disconnected
        void maintainMQTTConnection() {
            // If there is no mqtt conection, then try to connect

            for ( int attempt = 0; attempt < RECONNECT_ATTEMPT_LIMIT; attempt++ ){
                Serial.print(attempt);
                Serial.print("  Reconecting to MQTT broker");

                if ( mqtt.connect( g_states.MQTTclientID, "Humpkinz", "BrokerPrivadoTCCTCU" ) ){
                    Serial.println("       [OK]");
                    
                    mqtt.setCallback( someoneIsListenToUs );
                    mqtt.setBufferSize(450);

                    //  Subscribe to topic
                    char topic[sizeof(g_states.MQTTclientID) + sizeof(g_states.MQTTListenTopic) + 2];
                    sprintf( topic, "%s/%s", g_states.MQTTclientID, g_states.MQTTListenTopic );
                    mqtt.subscribe(topic, 0);

                    return;
                } else Serial.println("       [FAIL]");
                
                // this->printMQTTstatus();
            }

            Serial.println("[ERROR]    Handdle Broker re-connection fail");
            ESP.restart();
            while(1);
        }

        void printMQTTstatus(){
            const int state = mqtt.state();

            switch( state ){
                case -4:
                    Serial.println("MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time");
                    break;
                case -3:
                    Serial.println("MQTT_CONNECTION_LOST - the network connection was broken");
                    break;
                case -2:
                    Serial.println("MQTT_CONNECT_FAILED - the network connection failed");
                    break;
                case -1:
                    Serial.println("MQTT_DISCONNECTED - the client is disconnected cleanly");
                    break;
                case 0:
                    Serial.println("MQTT_CONNECTED - the client is connected");
                    break;
                case 1:
                    Serial.println("MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT");
                    break;
                case 2:
                    Serial.println("MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier");
                    break;
                case 3:
                    Serial.println("MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection");
                    break;
                case 4:
                    Serial.println("MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected");
                    break;
                case 5:
                    Serial.println("MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect");
                    break;
                    
                default:
                    Serial.println("Non detected status");
                    break;
            }
        }

};

MQTT mqtt_com;