#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <AsyncJson.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include "../config.h"

// #define IS_WIFI_AP

#ifdef IS_WIFI_AP
    #define ssid g_states.AP_SSID_local
    #define password g_states.AP_PSW
#else
    #define ssid g_states.STA_SSID
    #define password g_states.STA_PSW

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
                WiFi.mode(WIFI_AP);
                WiFi.softAP( ssid, password );
                Serial.println("       [OK]");

            #else

                printf("SSID: %s PWD: %s \n", ssid, password);
                Serial.print("Connecting on Station mode");
                WiFi.mode(WIFI_STA);
                WiFi.begin( ssid, password );
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

                    rootDoc["APN"] = g_states.APN;
                    rootDoc["MQTT_Client_ID"] = g_states.MQTTclientID;
                    rootDoc["MQTT_Topics"] = g_states.MQTTDataTopic;

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