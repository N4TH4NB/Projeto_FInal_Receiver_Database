#include <WiFi.h>
#include <esp_now.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
// Credenciais de wifi e Firebase
#include "cred.h"

// Estrutura de dados ESP-NOW
typedef struct SensorData
{
  float temperatura;
  float pressao;
  int luminosidade;
  float chuva;
  int bateria;
  long latitude;
  long longitude;
  unsigned short ano;
  unsigned char mes, dia, hora, minuto, segundo;
} SensorData;
SensorData sensorData;

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);
void sendData();
void printError(int code, const String &msg);

DefaultNetwork network;
NoAuth noAuth;
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client, getNetwork(network));
RealtimeDatabase Database;
AsyncResult aResult_no_callback;

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Conectando ao Wi-Fi...");
  }
  Serial.println("Wi-Fi conectado.");
  //Serial.printf("Endereço IP: %s\n", WiFi.localIP().toString().c_str());
  //Serial.printf("Canal Wi-Fi: %d\n", WiFi.channel());

  // Conecta ao Firebase
  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
  Serial.println("Inicializando database...");

  ssl_client.setInsecure();
  initializeApp(aClient, app, getAuth(noAuth), aResult_no_callback);

  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  aClient.setAsyncResult(aResult_no_callback);

  // Inicializa ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Erro ao inicializar ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
  app.loop();
  Database.loop();
}

// Callback de recebimento de dados ESP-NOW
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  // Copia os dados recebidos
  memcpy(&sensorData, incomingData, sizeof(sensorData));
  Serial.printf("Bytes recebidos: %d\n", len);
  Serial.print("MAC Address: ");
  for (int i = 0; i < 6; ++i)
  {
    Serial.printf("%02X", mac[i]);
    if (i < 5)
      Serial.print(":");
  }
  Serial.println();
  sendData();
}

// "Monta" o json e envia para o Firebase
void sendData()
{
  char datetime[21];
  snprintf(datetime, sizeof(datetime), "%04d-%02d-%02dT%02d:%02d:%02dZ",  // Formata ano, mes, dia, hora, minuto e segundo em uma string conforme a ISO8601
           sensorData.ano, sensorData.mes, sensorData.dia,
           sensorData.hora, sensorData.minuto, sensorData.segundo);
  Serial.print(datetime);

  JsonDocument jsonDoc;
  jsonDoc["Temp"] = sensorData.temperatura;
  jsonDoc["Press"] = sensorData.pressao;
  jsonDoc["Lum"] = sensorData.luminosidade;
  jsonDoc["Bat"] = sensorData.bateria;
  jsonDoc["Chuva"] = sensorData.chuva;
  jsonDoc["Lon"] = sensorData.longitude;
  jsonDoc["Lat"] = sensorData.latitude;

  String jsonData;
  jsonDoc.shrinkToFit();  // Libera memoria alocada a mais
  serializeJson(jsonDoc, jsonData); // Passa esse json para uma string
  serializeJson(jsonDoc, Serial); // Printa esse json na serial 

  String path = "hora/";
  path += datetime; // Cria um nó usando a data como nome
  Serial.print("Set Json... ");
  bool status = Database.set<object_t>(aClient, path, object_t(jsonData)); // Envia para o Firebase
  if (status)
    Serial.println("ok");
  else
    printError(aClient.lastError().code(), aClient.lastError().message());
};


// Printa possiveis erros
void printError(int code, const String &msg)
{
  Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}