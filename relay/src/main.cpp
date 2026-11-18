#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Конфигурация WIFI подключения (точка доступа контроллера base)
const char *wifi_ssid = "Actuator";
const char *wifi_password = "rmc6394B";

// Статический адрес: контроллер base шлёт команды на 192.168.4.2
const IPAddress relayIp(192, 168, 4, 2);
const IPAddress gatewayIp(192, 168, 4, 1);
const IPAddress subnetMask(255, 255, 255, 0);

// ----------------------- ПИНЫ --------------------------------------------------------------------
#define RELAY_PIN D3                  // управление реле

// Тайминги ---------------------------------------------------------------------------------------
const uint32_t WATCHDOG_MS       = 10UL * 60 * 1000;  // нет команд 10 мин — открыть клапан
const uint32_t WIFI_CHECK_MS     = 10000;             // проверка подключения к Wi-Fi
const uint32_t WIFI_RECONNECT_MS = 30000;             // повторное подключение не чаще раза в 30 с

// Переменные --------------------------------------------------------------------------------------
volatile uint32_t lastCommandMs = 0;  // время последней команды от base
bool watchdogTripped = false;
uint32_t lastWifiCheckMs = 0;
uint32_t lastWifiBeginMs = 0;

// Объявление функции ---------------------------------------------------------------------------------
void wifiBegin();
void checkWifi();
void checkWatchdog();
String openValve();
String closeValve();

// Классы ------------------------------------------------------------------------------------------
AsyncWebServer server(80);         // объявить объект класса http сервера


  // -------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Определить пины --------------------------------------------------
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  openValve();                        // при старте клапан открыт (безопасное состояние)

  // Не сохранять настройки Wi-Fi во флеш: иначе каждое переподключение (раз в 30 с, пока нет
  // связи с base) записывает флеш-память дважды, и за дни без связи она изнашивается.
  // Имя сети и пароль всё равно задаются в коде при каждом запуске.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.config(relayIp, gatewayIp, subnetMask);
  WiFi.setAutoReconnect(true);
  wifiBegin();

  // ждём подключения не дольше 15 с, дальше — переподключение в loop
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi not connected yet");
  }

  server.on("/open_valve", HTTP_GET, [](AsyncWebServerRequest *request) {
    lastCommandMs = millis();
    request->send(200, "text/plain", openValve());
  });
  server.on("/close_valve", HTTP_GET, [](AsyncWebServerRequest *request) {
    lastCommandMs = millis();
    request->send(200, "text/plain", closeValve());
  });

  server.begin();
  Serial.println("HTTP server started");
  lastCommandMs = millis();
}
  // -------------------------------------------------------------------
void loop() {
  checkWifi();
  checkWatchdog();
}
  // -------------------------------------------------------------------


void wifiBegin()
{
  Serial.println("WIFI connecting");
  WiFi.begin(wifi_ssid, wifi_password);
  lastWifiBeginMs = millis();
}

// Переподключение без блокировки: ESP сама пробует восстановить связь (autoReconnect),
// а если связи нет долго — начинаем подключение заново.
void checkWifi()
{
  if (millis() - lastWifiCheckMs < WIFI_CHECK_MS) return;
  lastWifiCheckMs = millis();
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiBeginMs >= WIFI_RECONNECT_MS) {
    WiFi.disconnect();   // при persistent(false) только в RAM, флеш не трогается
    wifiBegin();
  }
}

// Сторожевой таймер: если base молчит (завис, выключен, нет Wi-Fi), открыть клапан,
// чтобы в доме не замёрзло. base повторяет команду раз в минуту, поэтому 10 мин
// тишины — точно проблема.
void checkWatchdog()
{
  if (millis() - lastCommandMs < WATCHDOG_MS) {
    watchdogTripped = false;
    return;
  }
  if (!watchdogTripped) {
    Serial.println("Watchdog: no commands from base, opening valve");
    openValve();
    watchdogTripped = true;
  }
}

String openValve()
{
  Serial.println("openValve!");
  digitalWrite(LED_BUILTIN, LOW);             // включить светодиод
  digitalWrite(RELAY_PIN, LOW);               // открыть клапан
  return "Valve has been OPENED";
};

String closeValve()
{
  Serial.println("closeValve!");
  digitalWrite(LED_BUILTIN, HIGH);            // выключить светодиод
  digitalWrite(RELAY_PIN, HIGH);              // закрыть клапан
  return "Valve has been CLOSED";
};
