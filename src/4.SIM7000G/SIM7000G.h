//#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_MODEM_SIM7000
// #define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

#ifndef RECONNECT_ATTEMPT_LIMIT
    #define RECONNECT_ATTEMPT_LIMIT 3
#endif

// Caso eu queria debugar os comandos AT mandados para o SIM800L
// #define DUMP_AT_COMMANDS
// #define SerialMon Serial

#include <TinyGsmClient.h>
#include <ArduinoJson.h>

#ifndef CONFIG_DATA
    #include "../config.h"
#endif

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       4
#define MODEM_TX             27
#define MODEM_RX             26
// Set serial for debug console (to the Serial Monitor, default speed 115200)

#define SerialAT Serial1

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

TinyGsmClient GSMclient(modem);

class SIM7000G{

    public:
        void turnGPSOn(void){

            // Set SIM7000G GPIO4 LOW ,turn on GPS power
            // CMD:AT+SGPIO=0,4,1,1
            // Only in version 20200415 is there a function to control GPS power
            modem.sendAT("+SGPIO=0,4,1,1");
            if (modem.waitResponse(1500L) != 1) {
                DBG(" SGPIO=0,4,1,1 false ");
            }
            vTaskDelay( 1000 / portTICK_PERIOD_MS );
            modem.enableGPS();
        }

        void turnGPSOff(void){

            // Set SIM7000G GPIO4 LOW ,turn off GPS power
            // CMD:AT+SGPIO=0,4,1,0
            // Only in version 20200415 is there a function to control GPS power
            modem.sendAT("+SGPIO=0,4,1,0");
            if (modem.waitResponse(/*10000L*/) != 1) {
                DBG(" SGPIO=0,4,1,0 false ");
            }
            vTaskDelay( 1000 / portTICK_PERIOD_MS );
            modem.disableGPS();
        }

    public:
        // Creates a struct to store GPS medium frequency info
        struct gpsInfo {
            float latitude = 0.0;
            float longitude = 0.0;
            float speed = 0.0;
            float altitude = 0.0;
            int vSat = 0;
            int uSat = 0;
            float Accuracy = 0.0;
            
            int year = 0;
            int month = 0;
            int day = 0;

            int hour = 0;
            int minute = 0;
            int second = 0;
        };
        // Instantiate the GPS structure
        struct gpsInfo CurrentGPSData;

        // Creates a struct to store GPRS data
        struct gprsInfo {
            int signalQlty = 0;
            
            //  Used to locate GSM tower's location
            int cellID = 0;
            int MCC = 0;
            int MNC = 0;
            int LAC = 0;
        };
        // Instantiate the GPRS structure
        struct gprsInfo CurrentGPRSData;

        void setup(){

            SerialAT.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX, false);
            Serial.println("Setting GPRS...");

            // Set modem reset, enable, power pins
            // pinMode(MODEM_PWKEY, OUTPUT);
            // pinMode(MODEM_RST, OUTPUT);
            pinMode(MODEM_POWER_ON, OUTPUT);

            // digitalWrite(MODEM_PWKEY, LOW);
            // digitalWrite(MODEM_RST, HIGH);
            digitalWrite(MODEM_POWER_ON, HIGH);

            SerialAT.begin(9600);

            modem.restart();
            // modem.init();
            modem.setBaud(9600);

            Serial.print("\nConnecting to APN ");
            Serial.print(g_states.APN);

            if (modem.gprsConnect(g_states.APN, g_states.APNPassword, g_states.APNPassword))
                Serial.println("       [OK]");
            else {
                Serial.println("       [FAIL]");
                Serial.println("[ERROR]    Handdle APN connection fail");
                while(1);
            }
            
            Serial.print("Connecting to GPRS: ");
            if (modem.isGprsConnected()){
                Serial.println("       [OK]");

            } else {
                Serial.println("       [FAIL]");
                Serial.println("Handlle GPRS connection fail");
                while(1);
            }

        }

        // Function to reconect to GPRS if it is disconnected
        void maintainGPRSconnection(){
            // If there is no GPRS connection, then try to connect

            for ( int attempt = 0; attempt < RECONNECT_ATTEMPT_LIMIT; attempt++ ){
                Serial.print(attempt);
                Serial.print("    Reconnecting to GPRS");
                
                if ( modem.gprsConnect( g_states.APN, g_states.APNPassword, g_states.APNPassword ) ){
                    Serial.println("       [OK]");
                    return;
                } else Serial.println("       [FAIL]");
            }

            Serial.println("[ERROR]    Handdle APN connection fail");
            while(1);
        }

        bool update_GPS_data(){

            static TickType_t tickLimit = xTaskGetTickCount() + (g_states.GPSUpdatePeriod / portTICK_PERIOD_MS);

            while( xTaskGetTickCount() < tickLimit ) {
                // Update GPS data
                if ( modem.getGPS( &this->CurrentGPSData.latitude, &this->CurrentGPSData.longitude, &this->CurrentGPSData.speed, &this->CurrentGPSData.altitude,
                                    &this->CurrentGPSData.vSat, &this->CurrentGPSData.uSat, &this->CurrentGPSData.Accuracy,
                                    &this->CurrentGPSData.year, &this->CurrentGPSData.month, &this->CurrentGPSData.day,
                                    &this->CurrentGPSData.hour, &this->CurrentGPSData.minute, &this->CurrentGPSData.second ) ){

                                        Serial.println("GPS data");
                                        Serial.print("latitude: ");
                                        Serial.println(this->CurrentGPSData.latitude);
                                        Serial.print("longitude: ");
                                        Serial.println(this->CurrentGPSData.longitude);
                                        Serial.print("altitude: ");
                                        Serial.println(this->CurrentGPSData.altitude);
                                        Serial.print("speed: ");
                                        Serial.println(this->CurrentGPSData.speed);
                                        Serial.print("vSat: ");
                                        Serial.println(this->CurrentGPSData.vSat);
                                        Serial.print("uSat: ");
                                        Serial.println(this->CurrentGPSData.uSat);
                                        Serial.print("Accuracy: ");
                                        Serial.println(this->CurrentGPSData.Accuracy);

                                        //  If GPS data is collected
                                        return true;
                                    }
                vTaskDelay( 250 / portTICK_PERIOD_MS );
            }


            //  If GPS data is NOT collected
            return false;

        }

        // Warning, chaotic function ahead!
        // Update de GPRS data
        bool update_GPRS_data(){

            // Update the gprs signal quality
           this-> CurrentGPRSData.signalQlty = modem.getSignalQuality();

            // Now the chaotic part: Get the Cell tower info.
            // This is messy because TinyGSM lib still doesn't have specific function for this job, so we have to parse our way out of it

            modem.sendAT(GF("+CPSI?"));
            modem.waitResponse(1000L, GF("OK"));

            // Read the AT command's response
            char response[70];  // Buffer for the AT command's response
            int position = 0;   // position in array inside the loop 
            while ( SerialAT.available() ){
                response[position] = SerialAT.read();
                position++;
            }
            response[position] = '\0';

            // Split the response's parameters position in the char array
            position = 0;               // position in array inside the loop
            int commas_positions[9];    // array to store commas position inside the AT command's char array
            for ( int i = 0; i < sizeof(response); i++){
                
                // If finds comma, store it's position
                if ( response[i] == ',' ){
                    commas_positions[position] = i;
                    position++;
                }
                
            }
            
            /*   ____________________________________________________________________________________
                |                                                                                    |
                | As we only looking for the 3rd, 4th, 5th and 6th parameters, we only retrieve them |
                |____________________________________________________________________________________|
            */

            // First, the MCC
            char mcc[4];
            position = 0;
            for ( int i = commas_positions[1]; i < commas_positions[2]; i++  ){
                mcc[position] = response[i];
                position++;
            }
            // Converts the mcc char array to int and stores it
            if ( sscanf( mcc, "%d", &this->CurrentGPRSData.MCC ) == 0 ) Serial.println("Medium frequency data: Error on parsing MCC data");

            // Next, the MNC
            char mnc[4];
            position = 0;
            for ( int i = commas_positions[2]; i < commas_positions[3]; i++  ){
                mnc[position] = response[i];
                position++;
            }
            // Converts the mnc char array to int and stores it
            if ( sscanf( mnc, "%d", &this->CurrentGPRSData.MNC ) == 0 ) Serial.println("Medium frequency data: Error on parsing MNC data");

            // Next, the LAC
            char lac[4];
            position = 0;
            for ( int i = commas_positions[3]; i < commas_positions[4]; i++  ){
                lac[position] = response[i];
                position++;
            }
            // Converts the lac char array to int and stores it
            if ( sscanf( lac, "%d", &this->CurrentGPRSData.LAC) == 0 ) Serial.println("Medium frequency data: Error on parsing LAC data");

            // Finally, the CellID. In this case, it needs only the first 3 char from the AT command response's parametter
            char cell_id[4];
            position = 0;
            for ( int i = commas_positions[3]; i < commas_positions[3] + 3; i++  ){
                cell_id[position] = response[i];
                position++;
            }
            // Converts the lac char array to int and stores it
            if ( sscanf( cell_id, "%d", &this->CurrentGPRSData.cellID ) == 0 ) Serial.println("Medium frequency data: Error on parsing CellID data");

            return true;
        }

};

SIM7000G sim_7000g;