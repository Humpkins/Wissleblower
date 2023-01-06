#include <Arduino.h>
#include <ArduinoJson.h>

StaticJsonDocument<384> high_freq_root;

class highFrequencyMQTT {

    public:

        // Send Medium frequency data to the MQTT Broker
        int sendMHighFrequencyDataOverMQTT(){
            //  IMU data
            high_freq_root["Yaw"] = MPU_DATA.MPUData.ypr[0];
            high_freq_root["Pitch"] = MPU_DATA.MPUData.ypr[1];
            high_freq_root["Roll"] = MPU_DATA.MPUData.ypr[2];
            high_freq_root["Acell_x"] = MPU_DATA.MPUData.aaWorld.x;
            high_freq_root["Acell_y"] = MPU_DATA.MPUData.aaWorld.y;
            high_freq_root["Acell_z"] = MPU_DATA.MPUData.aaWorld.z;

            // Pack Battery data in JSON
            high_freq_root["BMS1_Current"] = TJA_DATA.batteries[0].current;
            high_freq_root["BMS1_Voltage"] = TJA_DATA.batteries[0].voltage;
            high_freq_root["BMS1_SoC"] = TJA_DATA.batteries[0].SoC;
            high_freq_root["BMS1_SoH"] = TJA_DATA.batteries[0].SoH;
            high_freq_root["BMS1_Temperature"] = TJA_DATA.batteries[0].temperature;

            high_freq_root["BMS2_Current"] = TJA_DATA.batteries[1].current;
            high_freq_root["BMS2_Voltage"] = TJA_DATA.batteries[1].voltage;
            high_freq_root["BMS2_SoC"] = TJA_DATA.batteries[1].SoC;
            high_freq_root["BMS2_SoH"] = TJA_DATA.batteries[1].SoH;
            high_freq_root["BMS2_Temperature"] = TJA_DATA.batteries[1].temperature;

            // Pack powertrain data in JSON
            high_freq_root["Motor_Speed_RPM"] =    TJA_DATA.CurrentPowertrainData.motorSpeedRPM;
            high_freq_root["Motor_Torque_Nm"] =    TJA_DATA.CurrentPowertrainData.motorTorque;
            high_freq_root["Motor_Temperature_C"] =    TJA_DATA.CurrentPowertrainData.motorTemperature;
            high_freq_root["Controller_Temperature_C"] =    TJA_DATA.CurrentPowertrainData.controllerTemperature;

            high_freq_root["recording"] = logger.recording;

            //  Serialize JSON Object to array of string
            char HighFrequencyDataBuffer[measureJson(high_freq_root) + 1];
            serializeJson(high_freq_root, HighFrequencyDataBuffer, measureJson(high_freq_root) + 1);

            // Send data to Medium Frequency topic

            char topic[31];
            sprintf(topic, "%s/%s", g_states.MQTTclientID, g_states.MQTTHighFrequencyTopic);

            int status = mqtt.publish(topic, HighFrequencyDataBuffer);

            if ( !status )  return false;
            else return true;

        }

};

highFrequencyMQTT highFrequency;