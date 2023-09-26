#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// настройки таймера задержки
unsigned long lastTime = 0;
unsigned long timerDelay = 10000; // 10 seconds (10000)

// Конфигурация WIFI подключения
// const char *wifi_ssid = "ASUS_ROUTER";
// const char *wifi_password = "aSus2020";
const char *wifi_ssid = "Actuator";
const char *wifi_password = "rmc6394B";

// ----------------------- ПИНЫ --------------------------------------------------------------------
#define RELAY_PIN D3                  // управление реле

// Переменные --------------------------------------------------------------------------------------
uint8_t wifiCount = 0;

// Объявление функции ---------------------------------------------------------------------------------
void WifiConnect();
void checkAndWifiConnect();
String openValve();
String closeValve();

// Классы ------------------------------------------------------------------------------------------
AsyncWebServer server(80);         // объявить объект класса http сервера


  // -------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Определить пины --------------------------------------------------
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);    // выключить светодиод

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);       // закрыть реле
  
  WifiConnect();

  server.on("/open_valve", HTTP_GET, [](AsyncWebServerRequest *request)
          { request->send_P(200, "text/plain", openValve().c_str()); });
  server.on("/close_valve", HTTP_GET, [](AsyncWebServerRequest *request)
          { request->send_P(200, "text/plain", closeValve().c_str()); });

  server.begin();
  Serial.println("HTTP server started");
}
  // -------------------------------------------------------------------
void loop() {

  if ((millis() - lastTime) > timerDelay) // вместо delay()
  {
    checkAndWifiConnect();
    Serial.println("LOOP");

    lastTime = millis();
  };
}
  // -------------------------------------------------------------------


void WifiConnect()
{
  // Подключение к wifi сети --------------------------------------------
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.println(".");
  Serial.println("WIFI connecting");
  while (wifiCount < 100)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      digitalWrite(LED_BUILTIN, LOW);             // включить светодиод
      delay(150);
      Serial.print(".");
      wifiCount++;
      digitalWrite(LED_BUILTIN, HIGH);             // включить светодиод
      delay(150);
    }
    else
    {
      Serial.println("");
      Serial.print("Connected to WiFi network with IP Address: ");
      Serial.println(WiFi.localIP());
      wifiCount = 101;
    }
  }
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println(".");
    Serial.println("WiFi Failed!");
    wifiCount = 0;
  }
  // -------------------------------------------------------------------
};

void checkAndWifiConnect()
{
  if (WiFi.status() != WL_CONNECTED)
    {
      WifiConnect();
    }
};

String openValve()
{
  Serial.println("openValve!");
  digitalWrite(LED_BUILTIN, LOW);             // включить светодиод
  digitalWrite(RELAY_PIN, LOW);              // открыть реле
  return "Valve has been OPENED";
};

String closeValve()
{
  Serial.println("closeValve!");
  digitalWrite(LED_BUILTIN, HIGH);           // выключить светодиод
  digitalWrite(RELAY_PIN, HIGH);              // закрыть реле
  return "Valve has been CLOSED";
};
