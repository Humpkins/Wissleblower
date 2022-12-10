#include <Arduino.h>

class UTILS {
    
    public:
        bool ParseCharArray( char * desiredChar, char * response,
                                int position, char splitter, bool isLast = false ){

            // Split the response's parameters position in the char array
            int splitter_count = 0;
            for ( int i = 0; i < sizeof(response); i++){
                
                // If finds aplitter, check if is the desired position. If not, increase splitter_count
                if ( response[i] == splitter && splitter_count != position ) splitter_count++;

                // If splitter_count matches the desired position, start store the incomming char
                if ( splitter_count == position ) {
                    int ArrPosition = 0;               // position in array inside the loop
                    // Reads incomming char until finds another splitter or array end
                    for ( int j = i + 1; (!isLast)?response[j] != splitter:response[j] != '\0'; j++ ) {
                        desiredChar[ ArrPosition ] = response[j];
                        ArrPosition++;
                    }
                    desiredChar[ ArrPosition ] = '\0';
                    return true;
                }
            }

            return false;
        }

};

UTILS utils;