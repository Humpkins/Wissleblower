#include <Arduino.h>

#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <stdio.h>
#include <stdint.h>

StaticJsonDocument<800> puff;

#define CONFIG_DATA

class states{
  public:
    const char APN[21] = "claro.com.br";
    const char APNUser[21] = "claro";
    const char APNPassword[21] = "claro";

    const char MQTTHost[31] = "test.mosquitto.org";
    const int  MQTTPort = 1883;
    const char MQTTclientID[21] = "MotoTesteTCC";
    const char MQTTUsername[5] = "";
    const char MQTTPassword[5] = "";
    const char MQTTInfoTopic[5] = "Info";
    const char MQTTDataTopic[14] = "potenciometro";
    const char MQTTTopic[31] = "Teste_moto_cliente_tcc";

    const char MQTTLowFrequencyTopic[31] = "LowFrequency";
    const int16_t MQTTLowPeriod = 10000;

    const char MQTTMediumFrequencyTopic[31] = "MediumFrequency";
    const int MQTTMediumPeriod = 1000;

    const char MQTTHighFrequencyTopic[31] = "HighFrequency";
    const int MQTTHighPeriod = 500;

    const int GPSUpdatePeriod = 3000;

    const char HOST[21] = "whistleblower";

    const char AP_SSID_local[21] = "TCU_TCC_Mateus";
    const char AP_PSW[21] = "tcc_mateus";

    const char STA_SSID[21] = "Unicon 2.4_Juliana";
    const char STA_PSW[21] = "99061012";

    const int N_BATERRIES = 0;

    const float GEAR_RATIO = 1;
    const float DRIVEN_WHEEL_MAX_DIAMETER_M = 0.6278;

    const int BASE_BATTERY_ID = 0x120;
    const int CONTROLLER_ID = 0x300;

    // Metodos para atualizar os dados
    void atualizaAPN(){};

    void showConfigJson(){

      if ( !SPIFFS.begin() ) {
        Serial.println("[ERROR]    Handdle SPIFFS mount error");
        while(true);
      }

      File config = SPIFFS.open(F("/config.json"));

      if ( config && config.size() ){

        DeserializationError err = deserializeJson( puff, config );
        if ( err ) {
          Serial.println("Deserialization error");
          Serial.println(err.f_str());
          while(true);
        }

        // int dados;
        // dados = puff["MQTTPort"];

        const char* dados = puff["APN"];
        // strlcpy(dados, puff["APN"], sizeof(dados));
        Serial.println(dados);

        while(true);

      } else {
        Serial.println("Erro ao ler os dados");
      }

    }
};

states g_states;
