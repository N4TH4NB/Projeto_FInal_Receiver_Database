// Wifi e Esp-NOW
#include <WiFi.h>
#include <esp_now.h>
// Firebase
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
// Json
#include <ArduinoJson.h>
// Credenciais de wifi e firebase
#include "cred.h"

unsigned long previousMillis = 0;

// Estrutura de dados enviada
typedef struct SensorData
{
  float temperatura;
  float pressao;
  int luminosidade;
  int chuva;
  int bateria;
  long latitude;
  long longitude;
  unsigned short ano;
  unsigned char mes, dia, hora, minuto, segundo;
} SensorData;
SensorData sensorData;

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);
void sendData();
void printResult(AsyncResult &aResult);
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

  // Exibe os dados no monitor serial
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

void sendData()
{
  char datetime[21];
  snprintf(datetime, sizeof(datetime), "%04d-%02d-%02dT%02d:%02d:%02dZ",
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
  jsonDoc.shrinkToFit();
  serializeJson(jsonDoc, jsonData);
  serializeJson(jsonDoc, Serial);

  String path = "hora/";
  path += datetime;
  Serial.print("Set Json... ");
  bool status = Database.set<object_t>(aClient, path, object_t(jsonData));
  if (status)
    Serial.println("ok");
  else
    printError(aClient.lastError().code(), aClient.lastError().message());
};

void printResult(AsyncResult &aResult)
{
  if (aResult.isEvent())
  {
    Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
  }
  if (aResult.isDebug())
  {
    Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
  }
  if (aResult.isError())
  {
    Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
  }
}

void printError(int code, const String &msg)
{
  Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}