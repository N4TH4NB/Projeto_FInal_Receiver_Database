// Wifi e Esp-NOW
#include <WiFi.h>
#include <esp_now.h>

// Firebase
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

// Json
#include <ArduinoJson.h>

// Filesystem e credenciais de wifi e firebase
#include "LittleFS.h"
#include "cred.h"

unsigned long previousMillis = 0;

typedef struct struct_message
{
  float temp;
  float press;
  float alt;
  int lum;
  float tensao;
  int chuva;
  long lon;
  long lat;
  unsigned short ano;
  unsigned char mes;
  unsigned char dia;
  unsigned char hora;
  unsigned char minuto;
  unsigned char segundo;
} struct_message;
struct_message myData;

void authHandler();
// void asyncCB(AsyncResult &aResult);
void printResult(AsyncResult &aResult);
void printError(int code, const String &msg);

DefaultNetwork network;
NoAuth noAuth;
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client, getNetwork(network));
RealtimeDatabase Database;
// bool taskComplete = false;
AsyncResult aResult_no_callback;

// Callback de recebimento de dados ESP-NOW
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  // Copia os dados recebidos
  memcpy(&myData, incomingData, sizeof(myData));

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
}

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
  Serial.printf("Endereço IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("Canal Wi-Fi: %d\n", WiFi.channel());

  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
  Serial.println("Initializing app...");

  ssl_client.setInsecure();
  initializeApp(aClient, app, getAuth(noAuth), aResult_no_callback);
  authHandler();

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
  if (millis() - previousMillis >= 5000)
  {
    previousMillis = millis();
    JsonDocument jsonDoc;
    jsonDoc["temp"] = myData.temp;
    jsonDoc["press"] = myData.press;
    jsonDoc["lum"] = myData.lum;
    jsonDoc["tensao"] = myData.tensao;
    jsonDoc["chuva"] = myData.chuva;
    jsonDoc["lon"] = myData.lon;
    jsonDoc["lat"] = myData.lat;

    char datetime[20];
    snprintf(datetime, sizeof(datetime), "%02d-%02d-%04dT%02d:%02d:%02d",
             myData.dia, myData.mes, myData.ano,
             myData.hora, myData.minuto, myData.segundo);
    jsonDoc["hora"] = datetime;
    String jsonData;
    jsonDoc.shrinkToFit();
    serializeJson(jsonDoc, jsonData);

    Serial.print("Push Json... ");
    String name = Database.push<object_t>(aClient, "", object_t(jsonData));
    if (aClient.lastError().code() == 0)
      Firebase.printf("ok, name: %s\n", name.c_str());
    else
      printError(aClient.lastError().code(), aClient.lastError().message());
  }
}

void authHandler()
{
  // Blocking authentication handler with timeout
  unsigned long ms = millis();
  while (app.isInitialized() && !app.ready() && millis() - ms < 120 * 1000)
  {
    // The JWT token processor required for ServiceAuth and CustomAuth authentications.
    // JWT is a static object of JWTClass and it's not thread safe.
    // In multi-threaded operations (multi-FirebaseApp), you have to define JWTClass for each FirebaseApp,
    // and set it to the FirebaseApp via FirebaseApp::setJWTProcessor(<JWTClass>), before calling initializeApp.
    JWT.loop(app.getAuth());
    printResult(aResult_no_callback);
  }
}

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