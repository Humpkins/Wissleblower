#include <Arduino.h>
#include <mcp_can.h>
#include <SPI.h>

#ifndef CONFIG_DATA
    #include "../config.h"
#endif

#define CS_PIN 5        // Chip Select Pin
#define MPC_INTERRUPT_PIN 13

MCP_CAN CAN0(CS_PIN);

class MCP2515 {
    private:
        int anyFalse ( bool * boolArray, int arrSize ) {
            int falseCount = 0;

            for ( int index = 0; index < arrSize; index++ ){
                if ( !boolArray[index] ) falseCount++;
            }

            return falseCount;
        }

    public:
        struct CAN_DATA {
            long unsigned int ID;
            unsigned char len = 0;
            unsigned char Content_Arr[8];
        };
        struct CAN_DATA IDs_Data;

        // Creates a struct to store bateries medium frequency info
        struct batteryInfo {
            int current = 0;
            int voltage = 0;
            int SoC = 0;
            int SoH = 0;
            int temperature = 0;
            int capacity = 0;
        };
        // Instantiate an array with all baterries configured
        struct batteryInfo batteries[2];

        // Creates a struct to store powertrain's data
        struct powertrainInfo {
            int motorSpeedRPM = 0;
            int motorTorque = 0;
            int motorTemperature = 0;
            int controllerTemperature = 0;
        };
        // Instantiate the powertrain structure
        struct powertrainInfo CurrentPowertrainData;

        // Function to set up the MCP2515 module
        void setup(){

            // Initialize MCP2515 running at 16MHz with a baudrate of 250kb/s and the masks and filters enable.
            if( !CAN0.begin(MCP_STDEXT, CAN_250KBPS, MCP_16MHZ) == CAN_OK ) {
                Serial.println("Handle MCP2515 setup error...");
                while(1);
            }
            
            CAN0.setMode(MCP_NORMAL);                      // Set operation mode to normal so the MCP2515 sends acks to received data.
            
            CAN0.init_Mask(0,0,0x0FFE0000);                // Init first mask...
            CAN0.init_Filt(0,0,0x01200000);                // Init first filter...
            CAN0.init_Filt(1,0,0x01210000);                // Init second filter...
            
            CAN0.init_Mask(1,0,0x0FFF0000);                // Init second mask...
            CAN0.init_Filt(2,0,0x03000000);                // Init third filter...

        }

        //  Function that update the IDs data from the incoming CAN packets
        bool UpdateSamples() {

            // Loop control variables
            bool already_checked[4] = { false, false, false };  //  IDs already checked

            const TickType_t tickLimit = xTaskGetTickCount() + (g_states.MQTTHighPeriod / portTICK_PERIOD_MS) ; // Timeout limit

            while ( anyFalse( already_checked, 3 ) > 0 ) {

                //  Check for packet arrival
                if ( !digitalRead(MPC_INTERRUPT_PIN) ){

                    // Read the incomming CAN data
                    CAN0.readMsgBuf( &this->IDs_Data.ID, &this->IDs_Data.len, this->IDs_Data.Content_Arr );

                    /*  Check if the incomming ID is one of the expected IDs and if this packet was not checked before,
                        then read it's content, save it to the global state and flag this packet as already checked */
                    if ( this->IDs_Data.ID == g_states.BASE_BATTERY_ID && !already_checked[0] ){

                        // Read the BMS1 Data and save it to the gloabl state
                        this->batteries[0].current        = (this->IDs_Data.Content_Arr[2] * (int)pow(16, 2) + this->IDs_Data.Content_Arr[3]) * 0.1;
                        this->batteries[0].voltage        = (this->IDs_Data.Content_Arr[0] * (int)pow(16, 2) + this->IDs_Data.Content_Arr[1]) * 0.1; // deslocamento para a esqueda
                        this->batteries[0].SoC            = this->IDs_Data.Content_Arr[6];
                        this->batteries[0].SoH            = this->IDs_Data.Content_Arr[7];
                        this->batteries[0].temperature    = this->IDs_Data.Content_Arr[4];
                        //batteries[0].capacity     = ???;

                        // Flag this packet as already checked
                        already_checked[0] = true;
                        

                    } else if ( this->IDs_Data.ID == (g_states.BASE_BATTERY_ID + 1) && !already_checked[1] ) {

                        // Read the BMS2 Data and save it to the gloabl state
                        this->batteries[1]. current       = (this->IDs_Data.Content_Arr[2] * (int)pow(16, 2) + this->IDs_Data.Content_Arr[3]) * 0.1;
                        this->batteries[1].voltage        = (this->IDs_Data.Content_Arr[0] * (int)pow(16, 2) + this->IDs_Data.Content_Arr[1]) * 0.1; // deslocamento para a esqueda
                        this->batteries[1].SoC            = this->IDs_Data.Content_Arr[6];
                        this->batteries[1].SoH            = this->IDs_Data.Content_Arr[7];
                        this->batteries[1].temperature    = this->IDs_Data.Content_Arr[4];
                        //batteries[1].capacity     = ???;

                        // Flag this packet as already checked
                        already_checked[1] = true;

                    } else if ( this->IDs_Data.ID == g_states.CONTROLLER_ID && !already_checked[2] ) {

                        // Read the controller data and save it to the gloabl state
                        this->CurrentPowertrainData.motorSpeedRPM         = this->IDs_Data.Content_Arr[0] * (int)pow(16, 2) + this->IDs_Data.Content_Arr[1];
                        this->CurrentPowertrainData.motorTorque           = (this->IDs_Data.Content_Arr[2] * (int)pow(16, 2) + this->IDs_Data.Content_Arr[3]) * 0.1;
                        this->CurrentPowertrainData.motorTemperature      = this->IDs_Data.Content_Arr[7];
                        this->CurrentPowertrainData.controllerTemperature = this->IDs_Data.Content_Arr[6];

                        // Flag this packet as already checked
                        already_checked[2] = true;
                    }
                }

                // Timeout limit
                if ( xTaskGetTickCount() >= tickLimit ) return false;
            }

            // If all the packets were received, return true
            return true;
        }

};

MCP2515 MCP_DATA;