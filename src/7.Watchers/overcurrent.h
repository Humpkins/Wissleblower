// #include "../../config.h"
// #include "../1.MCP2515/MCP2515.h"
// #include "../4.SIM7000G/SIM7000G.h"

class overcurrent {

    public:
        void watch() {

            File file = SPIFFS.open( g_states.ALARM_CURRENT_NAME, "w");
                    
            if ( !file ) {
                Serial.printf("[ERROR]  Error on open %s\n", g_states.ALARM_CURRENT_NAME);
                return;
            } else {
                // Check for overcurrent for each battery
                for ( int i = 0; i < sizeof(MCP_DATA.batteries)/sizeof(MCP_DATA.batteries[0]); i++  ) {
                    if ( MCP_DATA.batteries[i].current > g_states.MAX_CURRENT ) {

                        int currentWritten = file.print( MCP_DATA.batteries[i].current );
                        file.print(", ");
                        int latWritten = file.print( sim_7000g.CurrentGPSData.latitude );
                        file.print(", ");
                        int lngWritten = file.print( sim_7000g.CurrentGPSData.longitude );
                        file.print(", ");
                        int timestampWritten = file.print( sim_7000g.CurrentGPSData.datetime );
                        file.print(", ");
                        int btryNumWritten = file.printf("Battery %i", i);
                        file.print("\n");

                        
                    }
                }

                file.close();
                return;
            }
        }
};

overcurrent WatcherCurrent;