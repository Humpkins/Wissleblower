#include <Arduino.h>
#include <ArduinoJson.h>

StaticJsonDocument<192> medium_freq_root;

class highFrequencyMQTT {

    public:

        // Send Medium frequency data to the MQTT Broker
        void sendMHighFrequencyDataOverMQTT(){

            // Pack Battery data in JSON
            medium_freq_root["BMS1_Current"] = MCP_DATA.batteries[0].current;
            medium_freq_root["BMS1_Voltage"] = MCP_DATA.batteries[0].voltage;
            medium_freq_root["BMS1_SoC"] = MCP_DATA.batteries[0].SoC;
            medium_freq_root["BMS1_Temperature"] = MCP_DATA.batteries[0].temperature;

            medium_freq_root["BMS2_Current"] = MCP_DATA.batteries[1].current;
            medium_freq_root["BMS2_Voltage"] = MCP_DATA.batteries[1].voltage;
            medium_freq_root["BMS2_SoC"] = MCP_DATA.batteries[1].SoC;
            medium_freq_root["BMS2_Temperature"] = MCP_DATA.batteries[1].temperature;

            // Pack powertrain data in JSON
            medium_freq_root["Motor_Speed_RPM"] =    MCP_DATA.CurrentPowertrainData.motorSpeedRPM;
            medium_freq_root["Motor_Torque_Nm"] =    MCP_DATA.CurrentPowertrainData.motorTorque;
            medium_freq_root["Motor_Temperature_C"] =    MCP_DATA.CurrentPowertrainData.motorTemperature;
            medium_freq_root["Controller_Temperature_C"] =    MCP_DATA.CurrentPowertrainData.controllerTemperature;

            //  Serialize JSON Object to array of string
            char HighFrequencyDataBuffer[250];
            serializeJson(medium_freq_root, HighFrequencyDataBuffer, measureJson(medium_freq_root) + 1);

            // Send data to Medium Frequency topic

            char topic[31];
            sprintf(topic, "%s/%s", g_states.MQTTclientID, g_states.MQTTHighFrequencyTopic);

            int status = mqtt.publish(topic, HighFrequencyDataBuffer);

            if ( !status ) {
                Serial.println("       [ERROR]");

                Serial.println();
                mqtt_com.printMQTTstatus();
                Serial.println();
            }
            else Serial.println("       [OK]");

        }

};

highFrequencyMQTT highFrequency;