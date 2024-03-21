#include <Arduino.h>
#include <DHTNew.h>
#include <Wire.h>
#include "LittleFS.h"
#include "SSD1306Wire.h"
#include <EncButton.h>
#include <ErriezDS1307.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include "display_funcs.h"
#include "file_system_funcs.h"
#include "datetime_funcs.h"
#include "valve_funcs.h"

// ПИНЫ
#define ENC_VCC D5   // питание энкодера
#define DHT_PIN D4   // дата пин датчика DHT22

// Classes ------------------------------------------------------------------------------------------
DHT dht(DHT_PIN, DHT_MODEL_DHT22);        // объект класса dht
SSD1306Wire display(0x3c, SDA, SCL);      // объект класса дисплея
EncButton eb(D8, D7, D6);                 // объект класса энкодера
ErriezDS1307 rtc;                         // объект класса часов
HTTPClient restClient;                    // объект класса rest клиента
WiFiClient client;                        // объект класса wifi


// Константы --------------------------------------------------------------------------------------
const char *ssid = "Actuator";
const char *password = "rmc6394B";

// Переменные --------------------------------------------------------------------------------------
float curTemp; 
float setTemp; 
const char *setTempFile = "/setTemp.cfg";
float maxTemp;
float minTemp;
float nightTempDelta = 0.5;                      // дельта для увеличения уставки в ночное время с 0 до 8
const char *deltaFile = "/delta.cfg";
bool clearDisp = false;
String currentDisplay = "";
String mode = "auto";
bool manualClose = false;
bool localManualClose = false;


// Functions ---------------------------------------------------------------------------------
void getSetTemp();
void getNightTempDelta();
void saveSetTemp(float temp);
void saveDeltaTemp(float temp);


// Initializing -------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Определение пинов  -----------------------------------------
  pinMode(ENC_VCC, OUTPUT);    
  digitalWrite(ENC_VCC, HIGH);

  // Точка доступа для relay ------------------------------------------------------------------
  Serial.print("Access Point:");   // Выводим информацию через последовательный монитор
  Serial.println(ssid);            // Сообщаем пользователю имя WiFi, установленное NodeMCU
  WiFi.softAP(ssid, password);     // WiFi.softAP используется для запуска режима AP NodeMCU.
  Serial.print("IP-адрес:");       // И IP-адрес NodeMCU
  Serial.println(WiFi.softAPIP()); // IP-адрес NodeMCU можно получить, вызвав WiFi.softAPIP ()
  // -------------------------------------------------------------------------------------------

  // Initialising the display  -----------------------------------------
  display.init();
  display.flipScreenVertically();
  clearDisplay();
  startMessage();

  // Старт датчика DHT22 ----------------------------------------------
  pinMode(DHT_PIN, INPUT);     // D5 пин в режиме входа
  dht.begin();
  Serial.println(F("\nInitializing DHT22"));
  if (dht.getError() != DHT_ERROR_NONE)
  {
    Serial.print("Error: ");
    Serial.println(dht.getErrorString());
    return;
  }
  // -------------------------------------------------------------------

  // Initialising the Encoder-----------------------------------------
  eb.setEncType(EB_STEP2);

  // Старт датчика DS1307; ----------------------------------------------
  Wire.begin();
  Wire.setClock(100000);
  Serial.println(F("\nInitializing DS1307"));
  while (!rtc.begin()) {Serial.println(F("RTC not found"));}
  // Установка времени: 16:31:01, 17 марта 2024 года, воскресенье:
  rtc.setDateTime(15, 59, 1, 19, 3, 2024, 7);

  // Initialising file system -----------------------------------------
  if (LittleFS.begin())
  {
    getSetTemp();
    getNightTempDelta();
  }
  else
  {
    Serial.println("Error while mounting LittleFS");
    return;
  }  
}

// Main Loop ------------------------------------------------------------------------------------------
void loop() {
  eb.tick();
  curTemp = dht.readTemperature();
  // Serial.print("curTemp: ");
  // Serial.println(curTemp);

  // unsigned int connectedClients = WiFi.softAPgetStationNum();
  // Serial.print("Количество подключенных клиентов: ");
  // Serial.println(connectedClients);

  // Основные разделы
  if (eb.counter == 0 && currentDisplay == "") {
    mainDisplay(curTemp, setTemp, mode, valveState, clearDisp);
  }
  if ((eb.counter == 1 || eb.counter == -1) && currentDisplay == "") {
    TsetDisplay(setTemp, clearDisp);
    if (eb.click()) {
      currentDisplay = changeTsetDisplay(setTemp);
    }
  }
  if ((eb.counter == 2 || eb.counter == -2) && currentDisplay == "") {
    modeDisplay(mode, clearDisp);
    if (eb.click()) {
      currentDisplay = changeModeDisplay(mode);
    }    
  }    
  if ((eb.counter == 3 || eb.counter == -3) && currentDisplay == "") {
    deltaSetDisplay(nightTempDelta, clearDisp);
    if (eb.click()) {
      currentDisplay = deltaChangeDisplay(nightTempDelta);
    }    
  }    
  if ((eb.counter == 4 || eb.counter == -4) && currentDisplay == "") {
    cur_time = getTime();
    cur_date = getDate();
    dateTimetDisplay(cur_time, cur_date, clearDisp);
  }    
  if ((eb.counter > 4 || eb.counter < -4) && currentDisplay == "") {
    eb.counter = 0;
  } 
  clearDisp = false;
   
  // Раздел для изменения уставки
  if (currentDisplay == "changeTset") {
    if (eb.left()) {
      setTemp = setTemp + 0.1;
      changeTsetDisplay(setTemp);
    };
    if (eb.right()) {
      setTemp = setTemp - 0.1;
      changeTsetDisplay(setTemp);
    };
    if (eb.hold()) {
      saveSetTemp(setTemp);
      eb.counter = 1;
      currentDisplay = "";
      clearDisp = true;
    }    
  }
  if (currentDisplay == "changeMode") {
    if (eb.left()) {
      mode = "auto";
      changeModeDisplay(mode);
    };
    if (eb.right()) {
      mode = "neutral";
      localManualClose = !localManualClose;
      if (localManualClose == false) {
        changeModeDisplay("open");
      }
      if (localManualClose == true) {
        changeModeDisplay("close");
      }
    };
    if (eb.hold()) {
      eb.counter = 2;
      currentDisplay = "";
      clearDisp = true;
      if (mode == "neutral") {
        mode = "manual";
        manualClose = localManualClose;
      }
    }    
  }
  if (currentDisplay == "changeDelta") {
    if (eb.left()) {
      nightTempDelta = nightTempDelta + 0.1;
      deltaChangeDisplay(nightTempDelta);
    };
    if (eb.right()) {
      nightTempDelta = nightTempDelta - 0.1;
      deltaChangeDisplay(nightTempDelta);
    };
    if (eb.hold()) {
      saveDeltaTemp(nightTempDelta);
      eb.counter = 3;
      currentDisplay = "";
      clearDisp = true;
    }    
  }
  if (eb.turn()) {clearDisp = true;}


  // Раздел управления температурой
  if (mode == "manual") {
    if (manualClose == true && valveState == "OPEN")
      {
        Serial.print("Manual close");
        close_valve();
      }
    else if (manualClose == false && valveState == "CLOSED")
      {
        Serial.print("Manual open!");
        open_valve();
      }
  } else {
      getTime();
      if (0 <= hour && hour < 8) {
        if (curTemp > maxTemp + nightTempDelta && valveState == "OPEN")
        {
          close_valve();
        };
        if (curTemp < minTemp + nightTempDelta && valveState == "CLOSED")
        {
          open_valve();
        };
      } else {
        if (curTemp > maxTemp && valveState == "OPEN")
        {
          close_valve();
        };
        if (curTemp < minTemp && valveState == "CLOSED")
        {
          open_valve();
        };
      }
  }
}


void getSetTemp()
{
  String value = readFile(LittleFS, setTempFile);
  setTemp = value.toFloat();
  maxTemp = setTemp + 0.1; 
  minTemp = setTemp - 0.1;   
};

void saveSetTemp(float temp)
{
  String tempStr = String(temp);
  writeFile(LittleFS, setTempFile, tempStr.c_str());
  maxTemp = temp + 0.1; 
  minTemp = temp - 0.1;   
};

void getNightTempDelta()
{
  String value = readFile(LittleFS, deltaFile);
  nightTempDelta = value.toFloat();
};

void saveDeltaTemp(float nightTempDelta)
{
  String tempStr = String(nightTempDelta);
  writeFile(LittleFS, deltaFile, tempStr.c_str());
};
