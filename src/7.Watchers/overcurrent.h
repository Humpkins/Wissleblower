class overcurrent {

    public:
        File watcherFile;
        int16_t isSetedUp = 0;

        void setup(){
            if ( SD.cardType() == CARD_NONE ) {
                Serial.println("Mounting SD");
                if ( SD.begin() ) Serial.println("Compleated SD mounting");
            } else {
                if ( !SD.exists(g_states.ALARM_CURRENT_NAME) ) {
                    this->watcherFile = SD.open(g_states.ALARM_CURRENT_NAME, FILE_WRITE);
                    this->watcherFile.println("current,lat,lon,timestamp,BatteryID");
                    this->watcherFile.close();
                }
                //  Flags the initial setup complete
                this-> isSetedUp = 1;
            }
        }

        void current() {
            //  If SD is not mounted, ignore this step
            if ( SD.cardType() == CARD_NONE ) SD.begin();

            // Check for overcurrent for each battery. If encounters overcurrented battery, logs it to a file
            for ( int i = 0; i < sizeof(TJA_DATA.batteries)/sizeof(TJA_DATA.batteries[0]); i++ ) {
                if ( TJA_DATA.batteries[i].current > g_states.MAX_CURRENT ) {


                    if ( logger.logFile ) logger.logFile.close();
                    if ( !this->watcherFile ) this->watcherFile = SD.open(g_states.ALARM_CURRENT_NAME, FILE_WRITE);

                    this->watcherFile.print( TJA_DATA.batteries[i].current );
                    this->watcherFile.print(",");
                    this->watcherFile.print( sim_7000g.CurrentGPSData.latitude );
                    this->watcherFile.print(",");
                    this->watcherFile.print( sim_7000g.CurrentGPSData.longitude );
                    this->watcherFile.print(",");
                    this->watcherFile.print( sim_7000g.CurrentGPSData.datetime );
                    this->watcherFile.print(",");
                    this->watcherFile.print(i);

                    this->watcherFile.println();

                    this->watcherFile.close();
                }
            }
            return;
        }
};

overcurrent WatcherCurrent;