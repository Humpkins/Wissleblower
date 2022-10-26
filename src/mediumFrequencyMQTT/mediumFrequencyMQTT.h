#include <Arduino.h>
#include <ArduinoJson.h>

StaticJsonDocument<256> high_freq_root;

class mediumFrequencyMQTT {

    public:

        // Send Medium frequency data to the MQTT Broker
        void sendMediumFrequencyDataOverMQTT(){

            // Pack GPS data in JSON
            high_freq_root["Latitude"] =          sim_7000g.CurrentGPSData.latitude;
            high_freq_root["Longitude"] =         sim_7000g.CurrentGPSData.longitude;
            high_freq_root["GPS_Speed"] =         sim_7000g.CurrentGPSData.speed;
            high_freq_root["Altitude"] =          sim_7000g.CurrentGPSData.altitude;
            high_freq_root["Orientation"] =          sim_7000g.CurrentGPSData.orientation;
            high_freq_root["SatelitesInViewGNSS"] =   sim_7000g.CurrentGPSData.vSatGNSS;
            high_freq_root["SatelitesInUseGPS"] =    sim_7000g.CurrentGPSData.uSatGPS;
            high_freq_root["SatelitesInUseGLONASS"] =    sim_7000g.CurrentGPSData.uSatGLONASS;

            // Pack GPRS data in JSON
            high_freq_root["GPRS_SingalQuality"] =      sim_7000g.CurrentGPRSData.signalQlty;
            high_freq_root["GPRS_Operational_Mode"] =   sim_7000g.CurrentGPRSData.operationalMode;
            high_freq_root["cellID"] =                  sim_7000g.CurrentGPRSData.cellID;
            high_freq_root["MCC"] =                     sim_7000g.CurrentGPRSData.MCC;
            high_freq_root["MNC"] =                     sim_7000g.CurrentGPRSData.MNC;
            high_freq_root["LAC"] =                     sim_7000g.CurrentGPRSData.LAC;

            // Serialize JSON Object to array of string
            char MediumFrequencyDataBuffer[350];
            serializeJson(high_freq_root, MediumFrequencyDataBuffer, measureJson(high_freq_root) + 1);

            // Send data to Medium Frequency topic
            char topic[31];
            sprintf(topic, "%s/%s", g_states.MQTTclientID, g_states.MQTTMediumFrequencyTopic);

            if ( !mqtt.publish(topic, MediumFrequencyDataBuffer) ) {
                Serial.println("     [ERROR]");

                Serial.println();
                mqtt_com.printMQTTstatus();
                Serial.println();
            }
            else Serial.println("     [OK]");
        } 
};

mediumFrequencyMQTT mediumFrequency;