#define SCK GPIO_NUM_14
#define MISO GPIO_NUM_2
#define MOSI GPIO_NUM_15
#define SS GPIO_NUM_13

#include <SD.h>

class LOGGER {
    public:
        char basePath[15] = "logger_";
        char constPath[36];
        File logFile;

        int16_t mountFailCount = 0;
        int16_t mountFailMax = 5;

        bool recording = false;

        void setup(){
            SPI.begin(SCK, MISO, MOSI, SS);
        }

        void stopLog(){

            //  Stop logging if it is doing it            
            if ( this->recording ) {
                
                if ( this->logFile ) this->logFile.close();
                if ( SD.cardType() != CARD_NONE ) SD.end();

                //  Flags not-recording state
                this->recording = false;
                SD.end();

                vTaskSuspend( xLog );
            }

        }

        /*  0 - Fail to mount SD
            1 - Successefully started recording
            2 - Loggin service is already running */
        int startLog(){
            
            //  Starts logging if it is not already doing it
            if ( !this->recording ) {
                
                //  Reset the fail count
                this->mountFailCount  = 0;

                //  If the card is not mounted
                if( SD.cardType() == CARD_NONE ) {

                    for ( int i = 0; i < this->mountFailMax; i++ ) {

                        Serial.printf("Mounting SD %i", i);
                        //  Preventing from SD logging into serial
                        Serial.end();

                        //  Mount the SD card
                        if ( SD.begin() ) {
                            Serial.begin(115200);
                            while( !Serial );

                            Serial.println("      [OK]");
                            break;
                        } else {
                            Serial.begin(115200);
                            while( !Serial );

                            Serial.println("        [FAIL]");
                        }

                        //  Return error on SD mount attempt tryout
                        if ( i >= this->mountFailMax ) return 0;

                        vTaskDelay( 750 / portTICK_PERIOD_MS );
                    }
                }

                char * sanitized = utilities.replace_char( sim_7000g.CurrentGPSData.datetime, '/', '_' );
                sanitized = utilities.replace_char( sim_7000g.CurrentGPSData.datetime, ':', '-' );

                sprintf( this->constPath, "/%s%s.txt", this->basePath, sanitized );
                this->logFile = SD.open(this->constPath, FILE_WRITE);

                Serial.print("Creating file");
                Serial.end();
                if ( this->logFile ) {

                    // 34 fields
                    this->logFile.println("Timestamp,current1,current2,voltage1,voltage2,SoC1,SoC2,SoH1,SoH2,Temp1,Temp2,motorTemp,controllerTemp,speed,torque,gprsQlty,GPRSOpMode,inViewGNSS,usedGPS,usedGLONASS,Lat,Lon,gps_speed,gps_altitude,MCC,MNC,LAC,CellID,pitch,yall,roll,acc_x,acc_y,acc_z");

                    //  Flags recording state
                    this->recording = true;

                    //  Resume the SD writing task
                    vTaskResume( xLog );

                    //  Bring Serial COM back on
                    Serial.begin(115200);
                    while( !Serial );

                    Serial.println("        [OK]");
                    //  Return success on SD mount attempt
                    return 1;

                } else {
                    //  Bring Serial COM back on
                    Serial.begin(115200);
                    while( !Serial );

                    Serial.println("         [FAIL]");
                }

            } else return 2;

        }

        void updateFile(){

            //  Mount SD if it is not mounted
            if ( SD.cardType() == CARD_NONE ) {

                Serial.print("SD not mounted, trying to mount");
                Serial.end();

                if ( SD.begin() ) {

                    Serial.begin(115200);
                    while( !Serial );

                    Serial.println("          [OK]");
                } else {
                    Serial.begin(115200);
                    while( !Serial );

                    Serial.println("          [FAIL]");
                    this->mountFailCount++;
                }
            }

            //  Verify if the mount tryout is reached
            if ( this->mountFailCount > mountFailMax ) {

                this->stopLog();
                return;
            }

            //  Re-open the log file, if it is not mounted yet
            if ( !this->logFile ) this->logFile = SD.open( this->constPath, FILE_WRITE );
            else {
                //"Timestamp,current1,current2,voltage1,voltage2,
                this->logFile.printf("%s", sim_7000g.CurrentGPSData.datetime);
                this->logFile.printf(",%i", TJA_DATA.batteries[0].current);
                this->logFile.printf(",%i", TJA_DATA.batteries[1].current);
                this->logFile.printf(",%i", TJA_DATA.batteries[0].voltage);
                this->logFile.printf(",%i", TJA_DATA.batteries[1].voltage);
                //SoC1,SoC2,SoH1,SoH2,Temp1,Temp2,
                this->logFile.printf(",%i", TJA_DATA.batteries[0].SoC);
                this->logFile.printf(",%i", TJA_DATA.batteries[1].SoC);
                this->logFile.printf(",%i", TJA_DATA.batteries[0].SoH);
                this->logFile.printf(",%i", TJA_DATA.batteries[1].SoH);
                this->logFile.printf(",%i", TJA_DATA.batteries[0].temperature);
                this->logFile.printf(",%i", TJA_DATA.batteries[1].temperature);
                //motorTemp,controllerTemp,speed,torque,
                this->logFile.printf(",%i", TJA_DATA.CurrentPowertrainData.motorTemperature);
                this->logFile.printf(",%i", TJA_DATA.CurrentPowertrainData.controllerTemperature);
                this->logFile.printf(",%i", TJA_DATA.CurrentPowertrainData.motorSpeedRPM);
                this->logFile.printf(",%i", TJA_DATA.CurrentPowertrainData.motorTorque);
                //gprsQlty,GPRSOpMode,inViewGNSS,usedGPS,usedGLONASS,
                this->logFile.printf(",%i", sim_7000g.CurrentGPRSData.signalQlty );
                this->logFile.printf(",%s", sim_7000g.CurrentGPRSData.operationalMode );
                this->logFile.printf(",%i", sim_7000g.CurrentGPSData.vSatGNSS );
                this->logFile.printf(",%i", sim_7000g.CurrentGPSData.uSatGPS );
                this->logFile.printf(",%i", sim_7000g.CurrentGPSData.uSatGLONASS );
                //Lat,Lon,gps_speed,gps_altitude,
                this->logFile.printf(",%.8f", sim_7000g.CurrentGPSData.latitude );
                this->logFile.printf(",%.8f", sim_7000g.CurrentGPSData.longitude );
                this->logFile.printf(",%1.f", sim_7000g.CurrentGPSData.speed );
                this->logFile.printf(",%.1f", sim_7000g.CurrentGPSData.altitude );
                //MCC,MNC,LAC,CellID,
                this->logFile.printf(",%i", sim_7000g.CurrentGPRSData.MCC );
                this->logFile.printf(",%i", sim_7000g.CurrentGPRSData.MNC );
                this->logFile.printf(",%x", sim_7000g.CurrentGPRSData.LAC );
                this->logFile.printf(",%i", sim_7000g.CurrentGPRSData.cellID );
                //pitch,yall,roll,
                this->logFile.printf(",%.1f", MPU_DATA.MPUData.ypr[1] );
                this->logFile.printf(",%.1f", MPU_DATA.MPUData.ypr[0] );
                this->logFile.printf(",%.1f", MPU_DATA.MPUData.ypr[2] );
                //acc_x,acc_y,acc_z"
                this->logFile.printf(",%.1f", MPU_DATA.MPUData.aaWorld.x );
                this->logFile.printf(",%.1f", MPU_DATA.MPUData.aaWorld.y );
                this->logFile.printf(",%.1f", MPU_DATA.MPUData.aaWorld.z );

                this->logFile.println();

                return;
            }

        }
};
LOGGER logger;