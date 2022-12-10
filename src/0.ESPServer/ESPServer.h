#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#define IS_WIFI_AP

#ifndef IS_WIFI_AP
    WiFiClient wifiClient;
#endif

AsyncWebServer server(80);

class ESP_Server {
    public:
        void setup(){
            SPIFFS.begin(true);

            Serial1.begin(9600);

            // Wait a bit before scanning again
            delay(5000);

            Serial.println("Setting up the ESP Server");

            #ifdef IS_WIFI_AP
            
                Serial.print("Connecting on Access Point mode");
                // WiFi.mode(WIFI_AP);
                WiFi.softAP( g_states.AP_SSID_local, g_states.AP_PSW );
                Serial.println("       [OK]");

            #else

                printf("SSID: %s PWD: %s \n", g_states.STA_SSID, g_states.STA_PSW);
                Serial.print("Connecting on Station mode");
                WiFi.mode(WIFI_STA);
                WiFi.begin( g_states.STA_SSID, g_states.STA_PSW );
                while( WiFi.status() != WL_CONNECTED ){
                    Serial.print(".");
                    delay(1000);
                }
                Serial.println("       [OK]");

                Serial.print("Connected STA with the following IP: ");
                Serial.println(WiFi.localIP().toString().c_str());

            #endif

            if (!MDNS.begin(g_states.HOST)){
                Serial.println("[ERROR]    Handdle MDNS fail");
            } else {
                Serial.print("mDNS: ");
                Serial.println(g_states.HOST);
            }

            DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
            DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, PUT");
            DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

            server.on(
                "/info",
                HTTP_GET,
                []( AsyncWebServerRequest *request ){
                    
                    StaticJsonDocument<100> rootDoc;

                    Serial.println("/info requested...");
                    AsyncResponseStream *response = request -> beginResponseStream("application/json");

                    rootDoc["APN"] = g_states.APN_GPRS;
                    rootDoc["MQTT_Client_ID"] = g_states.MQTTclientID;
                    rootDoc["MQTT_Host"] = g_states.MQTTHost;

                    rootDoc["MQTT_Low_Period_Freq"] = g_states.MQTTLowPeriod;
                    rootDoc["MQTT_Medium_Period_Freq"] = g_states.MQTTMediumPeriod;
                    rootDoc["MQTT_High_Period_Freq"] = g_states.MQTTHighPeriod;

                    rootDoc["Wheel_diameter"] = g_states.DRIVEN_WHEEL_MAX_DIAMETER_M;

                    serializeJson( rootDoc, *response );

                    request->send(response);
                }
            );

            server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
            server.serveStatic("/favicon.ico", SPIFFS, "/").setDefaultFile("favicon.ico");

            server.begin();
        }
};

ESP_Server ESPServer;