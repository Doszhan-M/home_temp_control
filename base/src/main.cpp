#include <Arduino.h>

#include <ESP8266WiFi.h>

#include <ESP8266HTTPClient.h>

#include <DHTNew.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <FS.h>
#include "LittleFS.h"

#include <Wire.h>
#include <ErriezDS1307.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

// настройки таймера задержки
unsigned long lastTime = 0;
unsigned long timerDelay = 20000; // 10 seconds (10000)

// Конфигурация WIFI подключения
String wifi_ssid;
String wifi_password;

// настройки точки доступа
const char *ssid = "Actuator";
const char *password = "rmc6394B";

// url sonoff
const char *url = "http://192.168.4.2:8081/zeroconf/switch";

// ----------------------- ПИНЫ --------------------------------------------------------------------
#define DHT_VCC D6   // питание датчика DHT22
#define DHT_PIN D5   // дата пин датчика DHT22
#define CLOSE_PIN D3 // пин кнопки для закрытия клапана
#define OPEN_PIN D7  // пин кнопки для открытия клапана
 
// Классы ------------------------------------------------------------------------------------------
DHT dht(DHT_PIN, DHT_MODEL_DHT22); // объявить объект класса dht
WiFiClient client;                 // объявить объект класса wifi
AsyncWebServer server(80);         // объявить объект класса http сервера
HTTPClient restclient;             // объявить объект класса rest клиента
ErriezDS1307 rtc;                  // объявить объект класса rtc времени
WiFiUDP ntpUDP;                    // объявить объект класса ntp клиента
NTPClient timeClient(ntpUDP);

// Переменные --------------------------------------------------------------------------------------
String manual_control = "OFF";
String manual_valve_target = "neutral";
uint8_t wifiCount = 0;
bool valve_is_opened = true;
String valveState = "OPEN";                       // статус клапана для отображения в html
float max_temp;                                   // уставка для макс температуры
float min_temp;                                   // уставка для мин температуры
uint8_t night_temp_delta;                         // дельта для увеличения уставки в ночное время с 0 до 8
float night_max_temp;                             // чтобы ночью увеличит уставку на night_temp_delta градус
float night_min_temp;                             // чтобы ночью увеличит уставку на night_temp_delta градус
const char *max_temp_file = "/max.cfg";           // файл для хранения настроек
const char *min_temp_file = "/min.cfg";           // файл для хранения настроек
const char *night_temp_delta_file = "/delta.cfg"; // файл для хранения настроек
const char *input_max_temp = "max_temp";          // name в html форме
const char *input_min_temp = "min_temp";          // name в html форме
const char *input_delta = "delta";                // name в html форме

const char *input_ssid = "ssid";               // query параметр
const char *wifi_ssid_file = "/wifi_ssid.cfg"; // файл для хранения настроек
const char *input_password = "password";       // query параметр
const char *wifi_pass_file = "/wifi_pass.cfg"; // файл для хранения настроек

uint8_t hour;
uint8_t minut;
uint8_t sec;
uint8_t mday;
uint8_t mon;
uint16_t year;
uint8_t wday;
String month;
String minute;
String dateTime;

// Объявление функции ---------------------------------------------------------------------------------
void notFound(AsyncWebServerRequest *request);
String getTemperature();
String getHumidity();
String getValveState();
String manualControlOn();
String manualControlOff();
String processor(const String &var);
String get_night_temp_delta();
String get_max_temp();
String get_min_temp();
String get_wifi_ssid();
String get_wifi_pass();
String readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);
void close_valve_startup();
void open_valve();
void close_valve();
String manualOpenValve();
String manualCLoseValve();
String showTime();
void setNightTemperature();
// ------------------------------------------------------------------------------------------------------------------------

void setup()
{

  Serial.begin(115200);

  // Определить пины --------------------------------------------------
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(CLOSE_PIN, INPUT_PULLUP);
  pinMode(OPEN_PIN, INPUT_PULLUP);

  // Старт датчика DHT22 ----------------------------------------------
  pinMode(DHT_PIN, INPUT);     // D5 пин в режиме входа
  pinMode(DHT_VCC, OUTPUT);    // D6 пин в режиме выхода
  digitalWrite(DHT_VCC, HIGH); // подать напряжение 3,3V на D6 пин
  dht.begin();
  Serial.println(F("\nInitializing DHT22"));
  if (dht.getError() != DHT_ERROR_NONE)
  {
    Serial.print("Error: ");
    Serial.println(dht.getErrorString());
    return;
  }
  // -------------------------------------------------------------------

  // Работа файловой системы -----------------------------------------
  if (LittleFS.begin())
  {
    night_temp_delta = get_night_temp_delta().toInt();
    max_temp = get_max_temp().toFloat();
    min_temp = get_min_temp().toFloat();
    wifi_ssid = get_wifi_ssid();
    wifi_password = get_wifi_pass();
  }
  else
  {
    Serial.println("Error while mounting LittleFS");
    return;
  }
  // -------------------------------------------------------------------

  // Подключение к wifi сети --------------------------------------------
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.println(".");
  Serial.println("WIFI connecting");
  while (wifiCount < 40)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      wifiCount++;
    }
    else
    {
      wifiCount = 41;
    }
  }
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.print("WiFi Failed!");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  // -------------------------------------------------------------------

  // Настройка NTP клиента для синхронизации времени по интернету--------
  timeClient.begin();
  timeClient.setTimeOffset(21600); // GMT +6
  timeClient.update();
  // -------------------------------------------------------------------

  // Старт датчика DS1307; ----------------------------------------------
  Wire.begin();
  Wire.setClock(100000);
  Serial.println(F("\nInitializing DS1307"));

  while (!rtc.begin()) // Initialize RTC
  {
    Serial.println(F("RTC not found"));
  }
  if (timeClient.getEpochTime() > 500000) // если получено время из интернета
  {
    if (rtc.getEpoch() != timeClient.getEpochTime()) // если время на часах отличается от NTP
    {
      if (!rtc.setEpoch(timeClient.getEpochTime())) // установить время из NTP клиента
      {
        Serial.println(F("Error: RTC write failed"));
      };
    }
  }
  // -------------------------------------------------------------------------------------------

  // Точка доступа для sonoff ------------------------------------------------------------------
  WiFi.softAP(ssid, password);     // WiFi.softAP используется для запуска режима AP NodeMCU.
  Serial.print("Access Point:");   // Выводим информацию через последовательный монитор
  Serial.println(ssid);            // Сообщаем пользователю имя WiFi, установленное NodeMCU
  Serial.print("IP-адрес:");       // И IP-адрес NodeMCU
  Serial.println(WiFi.softAPIP()); // IP-адрес NodeMCU можно получить, вызвав WiFi.softAPIP ()
  // -------------------------------------------------------------------------------------------

  // Инициализация http клиента -----------------------------------------------------------------
  restclient.begin(client, url);
  Serial.println("restclient started");

  // --------------------------------------------------------------------------------------------

  // Эндпойнты web сервера ----------------------------------------------------------------------
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", String(), false, processor); });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/style.css", "text/css"); });

  server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "index.js", "text/js"); });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "favicon.ico", "image/ico"); });

  server.on("/get_temperature", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getTemperature().c_str()); });

  server.on("/get_humidity", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getHumidity().c_str()); });

  server.on("/get_night_delta", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", get_night_temp_delta().c_str()); });

  server.on("/get_valve_state", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getValveState().c_str()); });

  server.on("/get_datetime", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", showTime().c_str()); });

  server.on("/manual_control_state", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", manual_control.c_str()); });

  server.on("/manual_control_on", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", manualControlOn().c_str()); });

  server.on("/manual_control_off", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", manualControlOff().c_str()); });

  server.on("/open_valve", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", manualOpenValve().c_str()); });

  server.on("/close_valve", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", manualCLoseValve().c_str()); });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String inputMessage;

    if (request->hasParam(input_max_temp))
    {
      inputMessage = request->getParam(input_max_temp)->value();
      if (inputMessage.toFloat() <= min_temp) {
        return;
      };
      writeFile(LittleFS, max_temp_file, inputMessage.c_str());
      delay(15);
      max_temp = get_max_temp().toFloat();
      Serial.println(max_temp);
    };

    if (request->hasParam(input_min_temp))
    {
      inputMessage = request->getParam(input_min_temp)->value();
      if (inputMessage.toFloat() >= max_temp) {
        return;
      };
      writeFile(LittleFS, min_temp_file, inputMessage.c_str());
      delay(15);
      min_temp = get_min_temp().toFloat();
      Serial.println(min_temp);
    }; 
    if (request->hasParam(input_delta))
    {
      inputMessage = request->getParam(input_delta)->value();
      if (inputMessage.toInt() < 0) {
        return;
      };
      writeFile(LittleFS, night_temp_delta_file, inputMessage.c_str());
      delay(15);
      night_temp_delta = get_night_temp_delta().toInt();
      Serial.println(night_temp_delta);
    }; });

  // server.on("/wifi_settings", HTTP_GET, [](AsyncWebServerRequest *request)
  //           {
  //     String inputMessage;

  //     if (request->hasParam(input_ssid))
  //     {
  //       inputMessage = request->getParam(input_ssid)->value();
  //       writeFile(LittleFS, wifi_ssid_file, inputMessage.c_str());
  //       delay(15);
  //       wifi_ssid = get_wifi_ssid();
  //       Serial.println(wifi_ssid);
  //     };

  //     if (request->hasParam("password"))
  //     {
  //       inputMessage = request->getParam("password")->value();
  //       writeFile(LittleFS, wifi_pass_file, inputMessage.c_str());
  //       delay(15);
  //       wifi_password = get_wifi_pass();
  //       Serial.println(wifi_password);
  //     };
  //     request->send_P(200, "text/plain", "WiFi settings accepted!"); });

  server.onNotFound(notFound);
  server.begin();
  Serial.println("HTTP server started");

  // При запуске открыть привод клапана --------------------------------------------------------------
  // close_valve_startup();
}
// ---------------------------------------------------------------------------------------------------

void loop()
{
  if ((millis() - lastTime) > timerDelay) // вместо delay()
  {

    // управление физической кнопкой -----------------------------------------
    boolean btn_close = !digitalRead(CLOSE_PIN);
    boolean btn_open = !digitalRead(OPEN_PIN);

    if (btn_close)
    {
      manual_control = "ON";
      manual_valve_target = "CLOSE";
      Serial.print("btn_close: ");
      Serial.println(btn_close);
    }
    else if (btn_open)
    {
      manual_control = "ON";
      manual_valve_target = "OPEN";
      Serial.print("btn_open: ");
      Serial.println(btn_open);
    }
    else
    {
      manual_control = "OFF";
    };

    // ----------------------------------------------------------------------

    showTime();
    setNightTemperature();

    // Авто управление  -----------------------------------------------------
    if (manual_control == "OFF")
    {
      float temperature = dht.readTemperature(); // считать температуру
      Serial.print("Temperature: ");
      Serial.println(temperature);

      if (temperature > max_temp && valve_is_opened)
      {
        close_valve();
      };

      if (temperature < min_temp && !valve_is_opened)
      {
        open_valve();
      };
    }
    // ручное управление --------- ------------------------------------------
    else
    {
      Serial.println("manual_control on");

      if (manual_valve_target == "OPEN" && !valve_is_opened)
      {
        open_valve();
      }
      if (manual_valve_target == "CLOSE" && valve_is_opened)
      {
        close_valve();
      }
    };

    lastTime = millis();
  };
}

// ------------------------------------------------------------------------------------------------

// Функции управления реле

// Отправить запрос на включение реле
void close_valve()
{
  restclient.addHeader("Content-Type", "application/json");                            // header
  String RequestData = "{\"deviceid\":\"1000b91ec6\",\"data\":{\"switch\": \"off\"}}"; // payload
  int ResponseStatusCode = restclient.POST(RequestData);                               // post запрос
  if (ResponseStatusCode == 200)
  {
    Serial.print("Relay has been switched off! Status: ");
    Serial.println(ResponseStatusCode);
    Serial.println("Valve has been closed");
    /* запрос с проверкой не нужно делать, если пришел ответ 200, значит sonoff
     ответил и выполнил задание, поэтому сразу valve_is_opened можно менять */
    valve_is_opened = false;
    valveState = "CLOSED";
    digitalWrite(LED_BUILTIN, HIGH); // выключить светодиод
    String payload = restclient.getString();
    Serial.println(payload);
  }
  else
  {
    Serial.print("Error! StatusCode code: ");
    Serial.println(ResponseStatusCode);
  }
};

// Отправить запрос на выключение реле
void open_valve()
{
  restclient.addHeader("Content-Type", "application/json");                           // header
  String RequestData = "{\"deviceid\":\"1000b91ec6\",\"data\":{\"switch\": \"on\"}}"; // payload
  int ResponseStatusCode = restclient.POST(RequestData);                              // post запрос
  if (ResponseStatusCode == 200)
  {
    Serial.print("Relay has been switched on! Status: ");
    Serial.println(ResponseStatusCode);
    Serial.println("Valve has been opened");
    valve_is_opened = true;
    valveState = "OPEN";
    digitalWrite(LED_BUILTIN, LOW); // включить светодиод
  }
  else
  {
    Serial.print("Error! HTTP Response code: ");
    Serial.println(ResponseStatusCode);
  }
};

// Отправить запрос на выключение реле во время старта
void close_valve_startup()
{
  restclient.addHeader("Content-Type", "application/json");                            // header
  String RequestData = "{\"deviceid\":\"1000b91ec6\",\"data\":{\"switch\": \"off\"}}"; // payload
  int ResponseStatusCode = restclient.POST(RequestData);                               // post запрос
  if (ResponseStatusCode == 200)
  {
    Serial.print("Relay has been switched off! Status: ");
    Serial.println(ResponseStatusCode);
    Serial.println("Valve has been closed");
    valve_is_opened = false;
    valveState = "CLOSED";
    digitalWrite(LED_BUILTIN, HIGH); // выключить светодиод
  }
  else
  {
    Serial.print("Error! HTTP Response code: ");
    Serial.println(ResponseStatusCode);
    Serial.println("Recursion");
    return close_valve_startup();
  }
};

// Функции для http сервера -----------------------------------
void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

String get_max_temp()
{
  String value = readFile(LittleFS, max_temp_file);
  float max = value.toFloat();
  night_max_temp = max + night_temp_delta;
  return String(max);
};

String get_min_temp()
{
  String value = readFile(LittleFS, min_temp_file);
  float min = value.toFloat();
  night_min_temp = min + night_temp_delta;
  return String(min);
};

String get_night_temp_delta()
{
  String value = readFile(LittleFS, night_temp_delta_file);
  uint8_t min = value.toInt();
  return String(min);
};

String get_wifi_ssid()
{
  String value = readFile(LittleFS, wifi_ssid_file);
  return value;
};

String get_wifi_pass()
{
  String value = readFile(LittleFS, wifi_pass_file);
  return value;
};

String getTemperature()
{
  delay(dht.getMinimumSamplingPeriod());
  float temperature = dht.readTemperature();
  return String(temperature, 2);
};

String getHumidity()
{
  delay(dht.getMinimumSamplingPeriod());
  float humidity = dht.readHumidity();
  return String(humidity, 2);
};

String getValveState()
{
  return valveState;
};

String manualControlOn()
{
  manual_control = "ON";
  return manual_control;
};

String manualControlOff()
{
  manual_control = "OFF";
  manual_valve_target = "neutral";
  return manual_control;
};

String manualOpenValve()
{
  manual_valve_target = "OPEN";
  return manual_valve_target;
};

String manualCLoseValve()
{
  manual_valve_target = "CLOSE";
  return manual_valve_target;
};

// Подставить требуемые значения в html
String processor(const String &var)
{
  if (var == "STATE")
  {
    return getValveState();
  }
  else if (var == "TEMPERATURE")
  {
    return getTemperature();
  }
  else if (var == "HUMIDITY")
  {
    return getHumidity();
  }
  else if (var == "MAX_TEMP")
  {
    return get_max_temp();
  }
  else if (var == "MIN_TEMP")
  {
    return get_min_temp();
  }
  else if (var == "DELTA")
  {
    return get_night_temp_delta();
  }
  else if (var == "DATE_TIME")
  {
    return showTime();
  }
  return String();
}
// -----------------------------------------

// Записать настройки в пзу память ------------------------------------------
void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- write failed");
  }
};

// Считать настройки из пзу
String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory())
  {
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while (file.available())
  {
    fileContent += String((char)file.read());
  }
  return fileContent;
};

// Получить текущее время из часов ----------------------------------------------
String showTime()
{
  if (!rtc.getDateTime(&hour, &minut, &sec, &mday, &mon, &year, &wday))
  {
    dateTime = "Get time failed";
    Serial.println(dateTime);
    return dateTime;
  }
  else
  {
    if (mon < 10)
    {
      month = "0" + String(mon);
    }
    else
    {
      month = mon;
    };

    if (minut < 10)
    {
      minute = "0" + String(minut);
    }
    else
    {
      minute = String(minut);
    };
    dateTime = String(hour) + ":" + minute + "  " + String(mday) + "." + month + "." + String(year);
    return dateTime;
  };
};

// Ночью до 8 утра поднять уставку на 1 градус --------------------------------------
void setNightTemperature()
{
  if (0 <= hour && hour < 8)
  {
    if (max_temp < night_max_temp)
    {
      max_temp = night_max_temp;
      min_temp = night_min_temp;
    }
  }
  else
  {
    if (max_temp == night_max_temp)
    {
      max_temp = night_max_temp - night_temp_delta;
      min_temp = night_min_temp - night_temp_delta;
    }
  }
};
