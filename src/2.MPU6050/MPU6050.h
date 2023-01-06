#include <Arduino.h>

#include <I2Cdev.h>
#include <MPU6050_6Axis_MotionApps20.h>

#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

#ifndef CONFIG_DATA
    #include "../config.h"
#endif

#define MPU_INTERRUPT_PIN GPIO_NUM_39

MPU6050 mpu;

class MPU {
    private:

        // MPU control/status vars
        bool dmpReady = false;  // set true if DMP init was successful
        uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
        uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
        uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
        uint16_t fifoCount;     // count of all bytes currently in FIFO
        uint8_t fifoBuffer[64]; // FIFO storage buffer

    public:
        struct MPUDataStruct {
            // orientation/motion vars
            Quaternion q;           // [w, x, y, z]         quaternion container
            VectorInt16 aa;         // [x, y, z]            accel sensor measurements
            VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
            VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
            VectorFloat gravity;    // [x, y, z]            gravity vector
            float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
        };
        // Instantiate the MPU structure
        struct MPUDataStruct MPUData;

        void setup() {

            pinMode( MPU_INTERRUPT_PIN, INPUT );

            Wire.begin();
            Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties

            // initialize device
            Serial.println(F("Initializing I2C devices..."));
            mpu.initialize();

            // verify connection
            if ( mpu.testConnection() ) Serial.println( F("MPU6050 connection successful") );
            else {
                Serial.println( F("[ERROR]    Handdle MPU6050 connection failed"));
                utilities.ESPReset();
                
                while(true);
            }

            // load and configure the DMP
            devStatus = mpu.dmpInitialize();

            // supply your own gyro offsets here, scaled for min sensitivity
            mpu.setXGyroOffset(220);
            mpu.setYGyroOffset(76);
            mpu.setZGyroOffset(-85);
            mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

            // make sure it worked (returns 0 if so)
            if (devStatus == 0) {

                // Calibration Time: generate offsets and calibrate our MPU6050
                mpu.CalibrateAccel(6);
                mpu.CalibrateGyro(6);
                mpu.PrintActiveOffsets();

                // turn on the DMP, now that it's ready
                mpu.setDMPEnabled(true);

                // enable Arduino interrupt detection
                mpuIntStatus = mpu.getIntStatus();

                // set our DMP Ready flag so all the functions knows it's okay to use it
                this->dmpReady = true;

                // get expected DMP packet size for later comparison
                this->packetSize = mpu.dmpGetFIFOPacketSize();

            } else {
                // ERROR!
                // 1 = initial memory load failed
                // 2 = DMP configuration updates failed
                Serial.print(F("DMP Initialization failed (code "));
                Serial.print(devStatus);
                Serial.println(F(")"));
            }

        }

        bool UpdateSamples(){
            const TickType_t xTickLimit = xTaskGetTickCount() + (g_states.MQTTHighPeriod / portTICK_PERIOD_MS);

            // Tries <tryout> times to read interrupt pin
            while ( xTaskGetTickCount() < xTickLimit ) {

                // If MPU data is ready, check for INT pin HIGH
                if ( digitalRead(MPU_INTERRUPT_PIN) ){

                    if (!this->dmpReady) continue;
                    if ( !digitalRead(MPU_INTERRUPT_PIN) && this->fifoCount < this->packetSize)  continue;

                    this->mpuIntStatus = mpu.getIntStatus();
                    this->fifoCount = mpu.getFIFOCount();

                    // check for overflow (this should never happen unless our code is too inefficient)
                    if ( (this->mpuIntStatus & 0x10) || this->fifoCount == 1024 ) {
                        // reset so we can continue cleanly
                        mpu.resetFIFO();
                        continue;

                        // otherwise, check for DMP data ready interrupt (this should happen frequently)
                    }
                    
                    if ( this->mpuIntStatus & 0x02 ) {

                        // wait for correct available data length, should be a VERY short wait
                        while ( this->fifoCount < this->packetSize ) this->fifoCount = mpu.getFIFOCount();

                        // read a packet from FIFO
                        mpu.getFIFOBytes( this->fifoBuffer, this->packetSize );

                        // track FIFO count here in case there is > 1 packet available
                        // (this lets us immediately read more without waiting for an interrupt)
                        this->fifoCount -= this->packetSize;

                        // display Euler angles in degrees
                        mpu.dmpGetQuaternion( &this->MPUData.q, this->fifoBuffer );
                        mpu.dmpGetAccel(&this->MPUData.aa, fifoBuffer);
                        mpu.dmpGetGravity( &this->MPUData.gravity, &this->MPUData.q );
                        mpu.dmpGetYawPitchRoll( this->MPUData.ypr, &this->MPUData.q, &this->MPUData.gravity );
                        mpu.dmpGetLinearAccel(&this->MPUData.aaReal, &this->MPUData.aa, &this->MPUData.gravity);
                        mpu.dmpGetLinearAccelInWorld(&this->MPUData.aaWorld, &this->MPUData.aaReal, &this->MPUData.q);

                        this->MPUData.ypr[0] *= (180/M_PI);
                        this->MPUData.ypr[1] *= (180/M_PI);
                        this->MPUData.ypr[2] *= (180/M_PI);
                            
                        // display real acceleration, adjusted to remove gravity
                        mpu.dmpGetAccel( &this->MPUData.aa, this->fifoBuffer );
                        mpu.dmpGetLinearAccel( &this->MPUData.aaReal, &this->MPUData.aa, &this->MPUData.gravity );
                        
                        return true;

                    }
                }
            }

            // If tryout is reached, return false
            return false;
        }

};

MPU MPU_DATA;