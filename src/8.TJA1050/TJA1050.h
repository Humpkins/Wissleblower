#include <Arduino.h>
#include <ESP32CAN.h>
#include <CAN_config.h>

CAN_device_t CAN_cfg;               // CAN Config
const int rx_queue_size = 10;       // Receive Queue size

class TJA1050 {
    private:
        int anyFalse ( bool * boolArray, int arrSize ) {
            int falseCount = 0;

            for ( int index = 0; index < arrSize; index++ ){
                if ( !boolArray[index] ) falseCount++;
            }

            return falseCount;
        }

    public:

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

        void setup() {

            CAN_cfg.speed = CAN_SPEED_250KBPS;
            CAN_cfg.tx_pin_id = GPIO_NUM_25;
            CAN_cfg.rx_pin_id = GPIO_NUM_32;
            CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
            utilities.TJAqueueHandler = &CAN_cfg.rx_queue;

            // Set CAN Filter
            CAN_filter_t p_filter;
            p_filter.FM = Dual_Mode;

            p_filter.ACR0 = 0x60;
            p_filter.ACR1 = 0x00;
            p_filter.ACR2 = 0x24;
            p_filter.ACR3 = 0x00;

            p_filter.AMR0 = 0x00;
            p_filter.AMR1 = 0x1F;
            p_filter.AMR2 = 0x00;
            p_filter.AMR3 = 0x3F;

            ESP32Can.CANConfigFilter(&p_filter);

            // Init CAN Module
            ESP32Can.CANInit();
        }

        //  Function that update the IDs data from the incoming CAN packets
        bool UpdateSamples() {

            // Loop control variables
            bool already_checked[4] = { false, false, false };  //  IDs already checked

            const TickType_t tickLimit = xTaskGetTickCount() + (g_states.MQTTHighPeriod / portTICK_PERIOD_MS) ; // Timeout limit

            while ( anyFalse( already_checked, 3 ) > 0 ) {

                CAN_frame_t rx_frame;

                //  Check for packet arrival and read the incomming CAN data
                if ( xQueueReceive( CAN_cfg.rx_queue, &rx_frame, 50 / portTICK_PERIOD_MS ) == pdTRUE ){

                    /*  Check if the incomming ID is one of the expected IDs and if this packet was not checked before,
                        then read it's content, save it to the global state and flag this packet as already checked */
                    if ( rx_frame.FIR.B.RTR != CAN_RTR && rx_frame.MsgID == g_states.BASE_BATTERY_ID && !already_checked[0] ){

                        // Read the BMS1 Data and save it to the gloabl state
                        this->batteries[0].current        = (rx_frame.data.u8[2] * (int)pow(16, 2) + rx_frame.data.u8[3]) * 0.1;
                        this->batteries[0].voltage        = (rx_frame.data.u8[0] * (int)pow(16, 2) + rx_frame.data.u8[1]) * 0.1; // deslocamento para a esqueda
                        this->batteries[0].SoC            = rx_frame.data.u8[6];
                        this->batteries[0].SoH            = rx_frame.data.u8[7];
                        this->batteries[0].temperature    = rx_frame.data.u8[4];
                        //batteries[0].capacity     = ???;

                        // Flag this packet as already checked
                        already_checked[0] = true;
                        

                    } else if ( rx_frame.FIR.B.RTR != CAN_RTR && rx_frame.MsgID == (g_states.BASE_BATTERY_ID + 1) && !already_checked[1] ) {

                        // Read the BMS2 Data and save it to the gloabl state
                        this->batteries[1]. current       = (rx_frame.data.u8[2] * (int)pow(16, 2) + rx_frame.data.u8[3]) * 0.1;
                        this->batteries[1].voltage        = (rx_frame.data.u8[0] * (int)pow(16, 2) + rx_frame.data.u8[1]) * 0.1; // deslocamento para a esqueda
                        this->batteries[1].SoC            = rx_frame.data.u8[6];
                        this->batteries[1].SoH            = rx_frame.data.u8[7];
                        this->batteries[1].temperature    = rx_frame.data.u8[4];
                        //batteries[1].capacity     = ???;

                        // Flag this packet as already checked
                        already_checked[1] = true;

                    } else if ( rx_frame.FIR.B.RTR != CAN_RTR && rx_frame.MsgID == g_states.CONTROLLER_ID && !already_checked[2] ) {

                        // Read the controller data and save it to the gloabl state
                        this->CurrentPowertrainData.motorSpeedRPM         = rx_frame.data.u8[0] * (int)pow(16, 2) + rx_frame.data.u8[1];
                        this->CurrentPowertrainData.motorTorque           = (rx_frame.data.u8[2] * (int)pow(16, 2) + rx_frame.data.u8[3]) * 0.1;
                        this->CurrentPowertrainData.motorTemperature      = rx_frame.data.u8[7];
                        this->CurrentPowertrainData.controllerTemperature = rx_frame.data.u8[6];

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

TJA1050 TJA_DATA;