#include <Arduino.h>
#include <ArduinoJson.h>

StaticJsonDocument<300/*256*/> medium_freq_root;

class mediumFrequencyMQTT {

    public:

        // Send Medium frequency data to the MQTT Broker
        bool sendMediumFrequencyDataOverMQTT(){

            // Pack GPS data in JSON
            medium_freq_root["Latitude"] =          sim_7000g.CurrentGPSData.latitude;
            medium_freq_root["Longitude"] =         sim_7000g.CurrentGPSData.longitude;
            medium_freq_root["GPS_Speed"] =         sim_7000g.CurrentGPSData.speed;
            medium_freq_root["Altitude"] =          sim_7000g.CurrentGPSData.altitude;
            medium_freq_root["Orientation"] =          sim_7000g.CurrentGPSData.orientation;
            medium_freq_root["SatelitesInViewGNSS"] =   sim_7000g.CurrentGPSData.vSatGNSS;
            medium_freq_root["SatelitesInUseGPS"] =    sim_7000g.CurrentGPSData.uSatGPS;
            medium_freq_root["SatelitesInUseGLONASS"] =    sim_7000g.CurrentGPSData.uSatGLONASS;

            // Pack GPRS data in JSON
            medium_freq_root["GPRS_SingalQuality"] =      sim_7000g.CurrentGPRSData.signalQlty;
            medium_freq_root["GPRS_Operational_Mode"] =   sim_7000g.CurrentGPRSData.operationalMode;
            medium_freq_root["cellID"] =                  sim_7000g.CurrentGPRSData.cellID;
            medium_freq_root["MCC"] =                     sim_7000g.CurrentGPRSData.MCC;
            medium_freq_root["MNC"] =                     sim_7000g.CurrentGPRSData.MNC;
            medium_freq_root["LAC"] =                     sim_7000g.CurrentGPRSData.LAC;

            // Serialize JSON Object to array of string
            char MediumFrequencyDataBuffer[350];
            serializeJson(medium_freq_root, MediumFrequencyDataBuffer, measureJson(medium_freq_root) + 1);

            // Send data to Medium Frequency topic
            char topic[31];
            sprintf(topic, "%s/%s", g_states.MQTTclientID, g_states.MQTTMediumFrequencyTopic);

            int status = mqtt.publish(topic, MediumFrequencyDataBuffer);

            if ( !status ) return false;
            else return true;
        } 
};

mediumFrequencyMQTT mediumFrequency;