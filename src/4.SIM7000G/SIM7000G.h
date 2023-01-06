//#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_MODEM_SIM7000
// #define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

#ifndef RECONNECT_ATTEMPT_LIMIT
    #define RECONNECT_ATTEMPT_LIMIT 2
#endif

// Caso eu queria debugar os comandos AT mandados para o SIM800L
// #define DUMP_AT_COMMANDS
// #define SerialMon Serial

#include <TinyGsmClient.h>
#include <time.h>
// #include <ArduinoJson.h>
// #include <soc/rtc_wdt.h>

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

TinyGsmClient GSMclient( modem, 0 );

class SIM7000G {

    public:

        void turnGPSOn(void){

            SerialAT.print("AT+SGPIO=0,4,1,1\r\n");
            vTaskDelay( 500 / portTICK_PERIOD_MS );
            SerialAT.print("AT+CGNSPWR=1\r\n");
            vTaskDelay( 500 / portTICK_PERIOD_MS );
            SerialAT.print("AT+CGNSHOT");
            vTaskDelay( 500 / portTICK_PERIOD_MS );
        }

        void turnGPSOff(void){

            SerialAT.print("AT+SGPIO=0,4,1,0\r\n");
            vTaskDelay( 500 / portTICK_PERIOD_MS );
            SerialAT.print("AT+CGNSPWR=0\r\n");
            vTaskDelay( 500 / portTICK_PERIOD_MS );

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
            char datetime[64] = "00_00_0000 00-00-00";
            int timezone = 0;
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
            char LAC[7] = "";
            char ICCID[21] = "";
        };
        // Instantiate the GPRS structure
        struct gprsInfo CurrentGPRSData;

        void setup(){

            SerialAT.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX, false);

            // Set modem reset, enable, power pins
            pinMode(MODEM_POWER_ON, OUTPUT);
            digitalWrite(MODEM_POWER_ON, HIGH);

            SerialAT.begin(9600);

            modem.restart();
            // modem.init();
            modem.setBaud(9600);

            Serial.print("\nConnecting to APN ");
            Serial.print(g_states.APN_GPRS);

            if (modem.gprsConnect(g_states.APN_GPRS, g_states.APNUser, g_states.APNPassword))
                Serial.println("       [OK]");
            else {
                Serial.println("       [RECONNECTION FAIL]");
                Serial.println("[ERROR]    Handdle APN connection RECONNECTION fail");
                utilities.ESPReset();
            }
            
            if ( !modem.isGprsConnected() ){
                Serial.print("Connecting to GPRS: ");
                if (modem.isGprsConnected()){
                    Serial.println("       [OK]");

                } else {
                    Serial.println("       [RECONNECTION FAIL]");
                    Serial.println("Handlle GPRS connection RECONNECTION fail");
                    utilities.ESPReset();
                }
            }

            Serial.print("Seting up the RTC");
            if ( this->setupDateTime() ) {
                Serial.println("        [OK]");
                Serial.printf( "Current datetime: %s\n", this->CurrentGPSData.datetime );
            } else {
                Serial.println("        [ERROR]");
                Serial.println("Handle RTC setup error");
                utilities.ESPReset();
            }

        }

        bool checkConnection(){

            int tryout = 5;
            for ( int i = 0; i < tryout; i++ ) {

                SerialAT.print("AT+CGATT?\r\n");
                vTaskDelay( 500 / portTICK_PERIOD_MS );

                // Read the AT command's response
                char response[10];  // Buffer for the AT command's response
                char midBuff = '\n';
                int position = 0;   // position in array inside the loop 
                while ( SerialAT.available() ){
                    midBuff = SerialAT.read();

                    //  read until the linebreak
                    if ( midBuff == '\n' ) break;

                    response[position] = midBuff;
                    position++;
                    if ( position > 9 ) break;
                }
                response[position] = '\0';
                //  Clear Serial buffer
                Serial.flush();

                //  If the response is like expected, return true
                if ( strstr( response, "+CGATT: 1" ) == NULL ) return true;
            }
            //  If tryout limit is reached, return false
            return false;
        }

        // Function to reconect to GPRS if it is disconnected
        void maintainGPRSconnection(){
            // If there is no GPRS connection, then try to connect

            for ( int attempt = 0; attempt < RECONNECT_ATTEMPT_LIMIT; attempt++ ){

                Serial.print(attempt);
                Serial.print("    Reconnecting to GPRS");

                //  Restart the modem before reconect on the last attempt
                if ( attempt == RECONNECT_ATTEMPT_LIMIT - 1 ) {
                    modem.restart();
                    vTaskDelay( 1000 / portTICK_PERIOD_MS );
                }
                
                if ( modem.gprsConnect( g_states.APN_GPRS, g_states.APNUser, g_states.APNPassword ) ){
                    Serial.println("       [OK]");
                    this->turnGPSOn();
                    return;
                } else Serial.println("       [FAIL]");
            }

            Serial.println("[ERROR]    Handdle APN connection fail");
            utilities.ESPReset();
            while(1);
        }

        bool updateGPSData(){

            char response[105]; //  <- If Stack smash ocours, increase this value
            char midBuff = '\n';
            int position = 0;
            bool startRead = false;
            int commas_positions[22];    // array to store commas position inside the AT command's char array

            bool retorna_true = false;

            int tryOut = 5;
            for ( int tries = 0; tries < tryOut; tries++ ) {

                SerialAT.print("AT+CGNSINF\r\n");
                vTaskDelay( 500 / portTICK_PERIOD_MS );

                midBuff = '\n';
                position = 0;
                startRead = false;
                while ( SerialAT.available() ){
                    midBuff = SerialAT.read();
                    
                    //  Start store the read serial data
                    if ( midBuff == '+' ) startRead = true;

                    // Top read on the first lineBreak
                    if ( startRead && midBuff == '\n' ) break;

                    //  Read data
                    if ( startRead ){
                        response[ position ] = midBuff;
                        position++;
                    }
                }
                response[position] = '\0';
                //  Clear Serial buffer
                Serial.flush();

                if ( strstr( response, "+CGNSINF: 1,1" ) == NULL ) continue;
                else {

                    // Split the response's parameters position in the char array
                    position = 0;               // position in array inside the loop
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
                    for ( int i = commas_positions[2] + 1; i < commas_positions[3]; i++  ){
                        Latitude[position] = response[i];
                        position++;
                    }
                    Latitude[position] = '\0';
                    if ( Latitude[0] != '\0' ) this->CurrentGPSData.latitude = std::stof(Latitude);

                    // First, the Longitude
                    char Longitude[12];
                    position = 0;
                    for ( int i = commas_positions[3] + 1; i < commas_positions[4]; i++  ){
                        Longitude[position] = response[i];
                        position++;
                    }
                    Longitude[position] = '\0';
                    if ( Longitude[0] != '\0' ) this->CurrentGPSData.longitude = std::stof(Longitude);

                    // First, the Altitude
                    char Altitude[9];
                    position = 0;
                    for ( int i = commas_positions[4] + 1; i < commas_positions[5]; i++  ){
                        Altitude[position] = response[i];
                        position++;
                    }
                    Altitude[position] = '\0';
                    if ( Altitude[0] != '\0' ) this->CurrentGPSData.altitude = std::stof(Altitude);

                    // First, the speed
                    char speed[7];
                    position = 0;
                    for ( int i = commas_positions[5] + 1; i < commas_positions[6]; i++  ){
                        speed[position] = response[i];
                        position++;
                    }
                    speed[position] = '\0';
                    if ( speed[0] != '\0' ) this->CurrentGPSData.speed = std::stof(speed);

                    // First, the orientation
                    char orientation[7];
                    position = 0;
                    for ( int i = commas_positions[6] + 1; i < commas_positions[7]; i++  ){
                        orientation[position] = response[i];
                        position++;
                    }
                    orientation[position] = '\0';
                    if ( orientation[0] != '\0' ) this->CurrentGPSData.orientation = std::stof(orientation);

                    // First, the GNSS_Satelites_in_view
                    char GNSS_Satelites_in_view[3];
                    position = 0;
                    for ( int i = commas_positions[13] + 1; i < commas_positions[14]; i++  ){
                        GNSS_Satelites_in_view[position] = response[i];
                        position++;
                    }
                    GNSS_Satelites_in_view[position] = '\0';
                    if ( GNSS_Satelites_in_view[0] != '\0' ) this->CurrentGPSData.vSatGNSS = std::stoi(GNSS_Satelites_in_view);

                    // First, the GPS_Satelites_in_use
                    char GPS_Satelites_in_use[3];
                    position = 0;
                    for ( int i = commas_positions[14] + 1; i < commas_positions[15]; i++  ){
                        GPS_Satelites_in_use[position] = response[i];
                        position++;
                    }
                    GPS_Satelites_in_use[position] = '\0';
                    if ( GPS_Satelites_in_use[0] != '\0' ) this->CurrentGPSData.uSatGPS = std::stoi(GPS_Satelites_in_use);

                    // First, the GLONASS_Satelites_in_use
                    char GLONASS_Satelites_in_use[3];
                    position = 0;
                    for ( int i = commas_positions[15] + 1; i < commas_positions[16]; i++  ){
                        GLONASS_Satelites_in_use[position] = response[i];
                        position++;
                    }
                    GLONASS_Satelites_in_use[position] = '\0';
                    if ( GLONASS_Satelites_in_use[0] != '\0' ) this->CurrentGPSData.uSatGLONASS = std::stoi(GLONASS_Satelites_in_use);

                    // If read, return true
                    // return true;
                    retorna_true = true;
                    break;
                }

            }

            //  If could not lock GPS data in <tryOut> times, return false
            return retorna_true;

        }

        // Warning, chaotic function ahead!
        // Update de GPRS data
        bool update_GPRS_data(){

            // Update the gprs signal quality
           this-> CurrentGPRSData.signalQlty = modem.getSignalQuality();
           strcpy( this-> CurrentGPRSData.ICCID, modem.getSimCCID().c_str() );

            // Now the chaotic part: Get the Cell tower info.
            // This is messy because TinyGSM lib still doesn't have specific function for this job, so we have to parse our way out of it
            int tryOut = 5;
            for ( int tries = 0; tries < tryOut; tries++ ){

                SerialAT.print("AT+CPSI?\r\n");
                vTaskDelay( 250 / portTICK_PERIOD_MS );

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
                //  Clear Serial buffer
                Serial.flush();

                if ( strstr( response, "+CPSI: GSM,Online," ) == NULL ) continue;
                else {
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
                    position = 0;
                    for ( int i = 7; i < commas_positions[0]; i++  ){
                        this->CurrentGPRSData.operationalMode[position] = response[i];
                        position++;
                    }
                    this->CurrentGPRSData.operationalMode[position] = '\0';

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
                    position = 0;
                    for ( int i = commas_positions[2] + 1; i < commas_positions[3]; i++  ){
                        this->CurrentGPRSData.LAC[position] = response[i];
                        position++;
                    }
                    this->CurrentGPRSData.LAC[position] = '\0';

                    // Finally, the CellID. In this case, it needs only the first 3 char from the AT command response's parametter
                    char cell_id[4];
                    position = 0;
                    for ( int i = commas_positions[3] + 1; i < commas_positions[4]; i++  ){
                        cell_id[position] = response[i];
                        position++;
                    }
                    cell_id[position] = '\0';
                    // Converts the cell_id char array to int and stores it
                    if ( sscanf( cell_id, "%d", &this->CurrentGPRSData.cellID ) == 0 ) Serial.println("Medium frequency data: Error on parsing CellID data");

                    return true;                 
                }

                vTaskDelay( 250/ portTICK_PERIOD_MS );

            }

            return false;
        }

        //  Sets up the microcontroller's RTC
        bool setupDateTime(){
            // modem.getGSMDateTime(DATE_FULL).toCharArray( this->CurrentGPSData.datetime, modem.getGSMDateTime(DATE_FULL).length() + 1 );

            struct tm Timestamp;
            int hh, mm, ss, yy, mon, day = 0;

            int tryout = 5;
            Serial.println("Antes do for");
            for ( int tries = 0; tries < tryout; tries++ ) {

                SerialAT.write("AT+CCLK?\r\n");
                vTaskDelay( 200 / portTICK_PERIOD_MS );

                char response[64];
                int position = 0;
                Serial.println("Antes do while");
                while ( SerialAT.available() ) {
                    response[position] = SerialAT.read();
                    position++;
                }
                Serial.printf("Antes do while %i\n", position);
                response[position] = '\0';

                if ( strstr( response, "+CCLK: " ) == NULL ) continue;
                else {
                    struct tm Timestamp;
                    Serial.println("Antes do sscanf");
                    sscanf( response, "\n+CCLK: \"%d/%d/%d,%d:%d:%d%d\"\nOK\n", &yy, &mon, &day, &hh, &mm, &ss, &this->CurrentGPSData.timezone );

                    Timestamp.tm_hour = hh;
                    Timestamp.tm_min = mm;
                    Timestamp.tm_sec = ss;
                    Timestamp.tm_year = 2000 + yy - 1900;

                    Timestamp.tm_mon = mon - 1;
                    Timestamp.tm_mday = day;
                    Timestamp.tm_isdst = -1;

                    time_t epoch = mktime( &Timestamp );

                    struct timeval tv;
                    tv.tv_sec = epoch + 1;
                    tv.tv_usec = 0;
                    settimeofday( &tv, NULL );

                    if ( epoch > 0 ) {
                        Serial.println("Antes do update");
                        this->updateDateTime();
                        return true;
                    }
                }
            }

            return false;
        }

        void updateDateTime(){

            time_t tt = time(NULL);
            time_t tt_with_offset = tt + ( 3600 * (this->CurrentGPSData.timezone/2) );

            struct tm Timestamp;

            Timestamp = *gmtime(&tt_with_offset);

            strftime(this->CurrentGPSData.datetime, 64, "%d/%m/%Y %H:%M:%S", &Timestamp);
        }
};

SIM7000G sim_7000g;