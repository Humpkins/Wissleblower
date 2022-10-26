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
#include <soc/rtc_wdt.h>

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

    private:

        void turnGPSOn(void){

            modem.sendAT("+SGPIO=0,4,1,1");
            vTaskDelay( 500 / portTICK_PERIOD_MS );
            modem.sendAT("+CGNSPWR=1");
        }

        void turnGPSOff(void){

            modem.sendAT("+SGPIO=0,4,1,0");
            vTaskDelay( 500 / portTICK_PERIOD_MS );
            modem.sendAT("+CGNSPWR=0");

        }

    public:
        // Creates a struct to store GPS medium frequency info
        struct gpsInfo {
            float latitude = 0.0;
            float longitude = 0.0;
            float speed = 0.0;
            float altitude = 0.0;
            float orientation = 0.0;
            int vSatGNSS = 0;
            int uSatGPS = 0;
            int uSatGLONASS = 0;
        };
        // Instantiate the GPS structure
        struct gpsInfo CurrentGPSData;

        // Creates a struct to store GPRS data
        struct gprsInfo {
            int signalQlty = 0;
            char operationalMode[11] = "";
            
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
            
            if ( !modem.isGprsConnected() ){
                Serial.print("Connecting to GPRS: ");
                if (modem.isGprsConnected()){
                    Serial.println("       [OK]");

                } else {
                    Serial.println("       [FAIL]");
                    Serial.println("Handlle GPRS connection fail");
                    while(1);
                }
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

        bool updateGPSData(){
            this -> turnGPSOn();

            int tryOut = 5;
            for ( int i = 0; i < tryOut; i++ ) {
                
                SerialAT.print("AT+CGNSINF");
                vTaskDelay( 1000 / portTICK_PERIOD_MS );

                char response[95];
                char midBuff = '\n';
                int position = 0;
                while ( SerialAT.available() ){
                    midBuff = SerialAT.read();

                    if ( midBuff == '\n' ) break;
                    response[ position ] = midBuff;
                    position++;
                }
                response[position] = '\0';

                if ( strstr( response, "+CGNSINF: 1, 1" ) == NULL ) {
                    vTaskDelay( 150 / portTICK_PERIOD_MS );
                    continue;
                } else {
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


                    /*   ___________________________________________________________________________________________
                        |                                                                                           |
                        | As we only looking for the 4th to 8th and 15th to 17th parameters, we only retrieve them  |
                        |___________________________________________________________________________________________|
                    */
                    // First, the Latitude
                    char Latitude[11];
                    position = 0;
                    for ( int i = commas_positions[2]; i < commas_positions[3]; i++  ){
                        Latitude[position] = response[i];
                        position++;
                    }
                    Latitude[position] = '\0';
                    if ( sscanf( Latitude, "%.6f", this->CurrentGPSData.latitude ) == 0 ) Serial.println("Error on parsing the Latitude info");

                    // First, the Longitude
                    char Longitude[12];
                    position = 0;
                    for ( int i = commas_positions[3]; i < commas_positions[4]; i++  ){
                        Longitude[position] = response[i];
                        position++;
                    }
                    Longitude[position] = '\0';
                    if ( sscanf( Longitude, "%.6f", this->CurrentGPSData.longitude ) == 0 ) Serial.println("Error on parsing the Longitude info");

                    // First, the Altitude
                    char Altitude[9];
                    position = 0;
                    for ( int i = commas_positions[4]; i < commas_positions[5]; i++  ){
                        Altitude[position] = response[i];
                        position++;
                    }
                    Altitude[position] = '\0';
                    if ( sscanf( Altitude, "%.1f", this->CurrentGPSData.altitude ) == 0 ) Serial.println("Error on parsing the Altitude info");

                    // First, the speed
                    char speed[7];
                    position = 0;
                    for ( int i = commas_positions[5]; i < commas_positions[6]; i++  ){
                        speed[position] = response[i];
                        position++;
                    }
                    speed[position] = '\0';
                    if ( sscanf( speed, "%.2f", this->CurrentGPSData.speed ) == 0 ) Serial.println("Error on parsing the Speed info");

                    // First, the orientation
                    char orientation[7];
                    position = 0;
                    for ( int i = commas_positions[6]; i < commas_positions[7]; i++  ){
                        orientation[position] = response[i];
                        position++;
                    }
                    orientation[position] = '\0';
                    if ( sscanf( orientation, "%.2f", this->CurrentGPSData.orientation ) == 0 ) Serial.println("Error on parsing the Orientation info");

                    // First, the GNSS_Satelites_in_view
                    char GNSS_Satelites_in_view[3];
                    position = 0;
                    for ( int i = commas_positions[13]; i < commas_positions[14]; i++  ){
                        GNSS_Satelites_in_view[position] = response[i];
                        position++;
                    }
                    GNSS_Satelites_in_view[position] = '\0';
                    if ( sscanf( GNSS_Satelites_in_view, "%.2f", this->CurrentGPSData.vSatGNSS ) == 0 ) Serial.println("Error on parsing the Viewd Satelites info");

                    // First, the GPS_Satelites_in_use
                    char GPS_Satelites_in_use[3];
                    position = 0;
                    for ( int i = commas_positions[14]; i < commas_positions[15]; i++  ){
                        GPS_Satelites_in_use[position] = response[i];
                        position++;
                    }
                    GPS_Satelites_in_use[position] = '\0';
                    if ( sscanf( GPS_Satelites_in_use, "%.2f", this->CurrentGPSData.uSatGPS ) == 0 ) Serial.println("Error on parsing the Used GPS Satelites info");

                    // First, the GLONASS_Satelites_in_use
                    char GLONASS_Satelites_in_use[3];
                    position = 0;
                    for ( int i = commas_positions[15]; i < commas_positions[16]; i++  ){
                        GLONASS_Satelites_in_use[position] = response[i];
                        position++;
                    }
                    GLONASS_Satelites_in_use[position] = '\0';
                    if ( sscanf( GLONASS_Satelites_in_use, "%.2f", this->CurrentGPSData.uSatGLONASS ) == 0 ) Serial.println("Error on parsing the Used GLONASS Satelites info");

                    this->turnGPSOff();

                    // If read, return true
                    return true;
                }
            }

            this->turnGPSOff();

            //  If could not lock GPS data in <tryOut> times, return false
            return false;

        }

        // Warning, chaotic function ahead!
        // Update de GPRS data
        bool update_GPRS_data(){

            // Update the gprs signal quality
           this-> CurrentGPRSData.signalQlty = modem.getSignalQuality();

            // Now the chaotic part: Get the Cell tower info.
            // This is messy because TinyGSM lib still doesn't have specific function for this job, so we have to parse our way out of it
            int tryOut = 5;
            for ( int i = 0; i < tryOut; i++ ){

                SerialAT.print("AT+CPSI?\r\n");
                vTaskDelay( 500 / portTICK_PERIOD_MS );

                // Read the AT command's response
                char response[70];  // Buffer for the AT command's response
                char midBuff = '\n';
                int position = 0;   // position in array inside the loop 
                while ( SerialAT.available() ){
                    midBuff = SerialAT.read();

                    if ( midBuff == '\n' ) break;

                    response[position] = midBuff;
                    position++;
                }
                response[position] = '\0';

                if ( strstr( response, "+CPSI: GSM,Online," ) == NULL ) {
                    vTaskDelay( 150 / portTICK_PERIOD_MS );
                    continue;
                }

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
                    | As we only looking for the 1st, 3rd, 4th, 5th and 6th parameters, we only retrieve them |
                    |____________________________________________________________________________________|
                */
                // First, the Operational mode
                char opMode[11];
                position = 0;
                for ( int i = 0; i < commas_positions[0]; i++  ){
                    opMode[position] = response[i];
                    position++;
                }
                opMode[position] = '\0';

                // Next, the MCC
                char mcc[5];
                position = 0;
                for ( int i = commas_positions[1] + 1; response[i] != '-'; i++  ){
                    mcc[position] = response[i];
                    position++;
                }
                mcc[position] = '\0';
                // Converts the mcc char array to int and stores it
                if ( sscanf( mcc, "%d", &this->CurrentGPRSData.MCC ) == 0 ) Serial.println("Medium frequency data: Error on parsing MCC data");

                // Next, the MNC
                char mnc[3];
                position = 0;
                for ( int i = commas_positions[1] + 5; i < commas_positions[2]; i++  ){
                    mnc[position] = response[i];
                    position++;
                }
                mnc[position] = '\0';
                // Converts the mnc char array to int and stores it
                if ( sscanf( mnc, "%d", &this->CurrentGPRSData.MNC ) == 0 ) Serial.println("Medium frequency data: Error on parsing MNC data");

                // Next, the LAC
                char lac[4];
                position = 0;
                for ( int i = commas_positions[2] + 1; i < commas_positions[3]; i++  ){
                    lac[position] = response[i];
                    position++;
                }
                lac[position] = '\0';
                // Converts the lac char array to int and stores it
                if ( sscanf( lac, "%d", &this->CurrentGPRSData.LAC) == 0 ) Serial.println("Medium frequency data: Error on parsing LAC data");

                // Finally, the CellID. In this case, it needs only the first 3 char from the AT command response's parametter
                char cell_id[4];
                position = 0;
                for ( int i = commas_positions[3] + 1; i < commas_positions[4]; i++  ){
                    cell_id[position] = response[i];
                    position++;
                }
                cell_id[position] = '\0';
                // Converts the lac char array to int and stores it
                if ( sscanf( cell_id, "%d", &this->CurrentGPRSData.cellID ) == 0 ) Serial.println("Medium frequency data: Error on parsing CellID data");

                return true;
            }

            return false;
        }

};

SIM7000G sim_7000g;