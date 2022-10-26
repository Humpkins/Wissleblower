#include <PubSubClient.h>

#ifndef TINY_GSM_MODEM_SIM7000
    #include "../4.SIM7000G/SIM7000G.h"
#endif

#ifndef RECONNECT_ATTEMPT_LIMIT
    #define RECONNECT_ATTEMPT_LIMIT 3
#endif

#define TIME_BETWEEN_RECONNECTS_ATTEMPTS 1000
#define RECONNECT_THREAD_TIME 10000

PubSubClient mqtt(GSMclient);

class MQTT {

    public:
        // Function to setup the first connection with the MQTT Broker
        void setup() {

            Serial.print("Connecting to MQTT Host ");
            Serial.print(g_states.MQTTHost);
            Serial.print(" with the following client ID ");
            Serial.println(g_states.MQTTclientID);
            Serial.print("Trying");

            mqtt.setServer( g_states.MQTTHost, g_states.MQTTPort );

            // Connect to MQTT Broker without username and password
            bool status = mqtt.connect( g_states.MQTTclientID );
            mqtt.setBufferSize(400);

            // Or, if you want to authenticate MQTT:
            //   bool status = mqtt.connect("GsmClientN", mqttUsername, mqttPassword);

            if ( status == false ){
                Serial.println("       [FAIL]");
                Serial.println("[ERROR]    Handdle Broker connection fail");
                while(1);
            } else Serial.println("       [OK]");

            // this->printMQTTstatus();

        }

        // Function to reconect to MQTT broker if it is disconnected
        void maintainMQTTConnection() {
            // If there is no mqtt conection, then try to connect

            for ( int attempt = 0; attempt < RECONNECT_ATTEMPT_LIMIT; attempt++ ){
                Serial.print(attempt);
                Serial.print("  Reconecting to MQTT broker");

                if ( mqtt.connect( g_states.MQTTclientID ) ){
                    Serial.println("       [OK]");
                    return;
                } else Serial.println("       [FAIL]");
                
                // this->printMQTTstatus();
            }

            Serial.println("[ERROR]    Handdle Broker re-connection fail");
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