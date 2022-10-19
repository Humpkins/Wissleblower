#include <Arduino.h>

#include "../1.MCP2515/MCP2515.h"
#include "../4.SIM7000G/SIM7000G.h"
#include "../3.MQTT/MQTT.h"

#include "ArduinoJson.h"

#include "../config.h"

StaticJsonDocument<64> medium_freq_root;

class highFrequencyMQTT {

    public:

        // Send Medium frequency data to the MQTT Broker
        bool sendMediumFrequencyDataOverMQTT(){

            // Pack Battery data in JSON
            medium_freq_root["BMS1_Current"] = MCP_DATA.batteries[0].current;
            medium_freq_root["BMS2_Current"] = MCP_DATA.batteries[1].current;

            // Pack powertrain data in JSON
            medium_freq_root["Motor_Speed_RPM"] =    MCP_DATA.CurrentPowertrainData.motorSpeedRPM;
            medium_freq_root["Motor_Torque_Nm"] =    MCP_DATA.CurrentPowertrainData.motorTorque;

            // Serialize JSON Object to array of string
            char HighFrequencyDataBuffer[64];
            serializeJson(medium_freq_root, HighFrequencyDataBuffer, measureJson(medium_freq_root) + 1);

            // Send data to Medium Frequency topic
            char topic[31];
            sprintf(topic, "%s/%s", g_states.MQTTclientID, g_states.MQTTHighFrequencyTopic);
            mqtt.publish(topic, HighFrequencyDataBuffer);

            return true;
        }

};

highFrequencyMQTT highFrequency;