#include "../4.SIM7000G/SIM7000G.h"
#include "../1.MCP2515/MCP2515.h"
#include "../3.MQTT/MQTT.h"

StaticJsonDocument<384> high_freq_root;

class mediumFrequencyMQTT {

    public:

        // Send Medium frequency data to the MQTT Broker
        bool sendMediumFrequencyDataOverMQTT(){

            // Pack GPS data in JSON
            high_freq_root["Latitude"] =          sim_7000g.CurrentGPSData.latitude;
            high_freq_root["Longitude"] =         sim_7000g.CurrentGPSData.longitude;
            high_freq_root["GPS_Speed"] =         sim_7000g.CurrentGPSData.speed;
            high_freq_root["Altitude"] =          sim_7000g.CurrentGPSData.altitude;
            high_freq_root["SatelitesInView"] =   sim_7000g.CurrentGPSData.vSat;
            high_freq_root["SatelitesInUse"] =    sim_7000g.CurrentGPSData.uSat;

            // Pack Battery data in JSON
            high_freq_root["BMS1_Voltage"] =      MCP_DATA.batteries[0].voltage;
            high_freq_root["BMS1_SoC"] =          MCP_DATA.batteries[0].SoC;
            high_freq_root["BMS1_Temperature"] =  MCP_DATA.batteries[0].temperature;
            high_freq_root["BMS1_capacity"] =     MCP_DATA.batteries[0].capacity;

            high_freq_root["BMS2_Voltage"] =      MCP_DATA.batteries[1].voltage;
            high_freq_root["BMS2_SoC"] =          MCP_DATA.batteries[1].SoC;
            high_freq_root["BMS2_Temperature"] =  MCP_DATA.batteries[1].temperature;
            high_freq_root["BMS2_capacity"] =     MCP_DATA.batteries[1].capacity;

            // Pack GPRS data in JSON
            high_freq_root["GPRS_SingalQuality"] = sim_7000g.CurrentGPRSData.signalQlty;
            high_freq_root["cellID"] =             sim_7000g.CurrentGPRSData.cellID;
            high_freq_root["MCC"] =                sim_7000g.CurrentGPRSData.MCC;
            high_freq_root["MNC"] =                sim_7000g.CurrentGPRSData.MNC;
            high_freq_root["LAC"] =                sim_7000g.CurrentGPRSData.LAC;

            // Pack powertrain data in JSON
            high_freq_root["Motor_Temperature"] =         MCP_DATA.CurrentPowertrainData.motorTemperature;
            high_freq_root["Controller_Temperature"] =    MCP_DATA.CurrentPowertrainData.controllerTemperature;

            // Serialize JSON Object to array of string
            char MediumFrequencyDataBuffer[384];
            serializeJson(high_freq_root, MediumFrequencyDataBuffer, measureJson(high_freq_root) + 1);

            // Send data to Medium Frequency topic
            char topic[31];
            sprintf(topic, "%s/%s", g_states.MQTTclientID, g_states.MQTTMediumFrequencyTopic);
            mqtt.publish(topic, MediumFrequencyDataBuffer);

            return true;
        } 
};

mediumFrequencyMQTT mediumFrequency;