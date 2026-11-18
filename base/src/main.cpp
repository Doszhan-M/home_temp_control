#include <Arduino.h>
#include <DHTNew.h>
#include <Wire.h>
#include "LittleFS.h"
#include "SSD1306Wire.h"
#include <EncButton.h>
#include <ErriezDS1307.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <thermo_logic.h>

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

using thermo::intervalPassed;
using thermo::roundTenth;
using thermo::wrapValue;

// Константы --------------------------------------------------------------------------------------
const char *ssid = "Actuator";
const char *password = "rmc6394B";

const float    HYSTERESIS        = 0.2;                // клапан переключается при отклонении на ±0.2 °C
const uint32_t MIN_SWITCH_MS     = 10UL * 1000;        // в авто клапан переключается не чаще раза в 10 с
const uint32_t SENSOR_PERIOD_MS  = 2500;               // DHT22 не чаще раза в 2 с
const uint32_t SENSOR_FAIL_MS    = 60000;              // нет данных минуту — ошибка датчика
const uint32_t CLOCK_PERIOD_MS   = 1000;
const uint32_t EDIT_TIMEOUT_MS   = 30000;              // правка без нажатий 30 с — отмена
const uint32_t BLINK_MS          = 400;
const uint32_t HISTORY_PERIOD_MS = 15UL * 60 * 1000;   // точка графика раз в 15 мин: 96 точек = 24 ч

const float    FROST_ON_TEMP     = 10.0;               // ниже — клапан открывается при любом режиме
const float    FROST_OFF_TEMP    = 11.0;               // выше — защита от замерзания снимается

const uint32_t RETURN_MAIN_MS    = 60000;              // минута без нажатий — на главный экран
const uint32_t SAVER_MS          = 60000;              // минута без нажатий — хранитель экрана
const uint32_t SAVER_MOVE_MS     = 60000;              // блок хранителя сдвигается раз в минуту
const uint32_t WARNING_PHASE_MS  = 2000;               // предупреждение чередуется с режимом
const uint32_t RELAY_GRACE_MS    = 35000;              // после запуска не пугать NO LINK: реле нужно до 30 с,
                                                       // чтобы переподключиться, повтор команды — на ~32 с
const uint32_t SAVED_MSG_MS      = 1000;               // сколько показывать "Saved" после сохранения
const uint8_t  FAST_TURN_MS      = 50;                 // щелчки чаще — быстрое вращение, шаг больше
const uint8_t  SAVER_CONTRAST    = 10;                 // яркость хранителя экрана (0..255)
const uint8_t  NORMAL_CONTRAST   = 0xCF;               // яркость по умолчанию драйвера SSD1306

const float DEFAULT_SET_TEMP = 22.0;
const float DEFAULT_DELTA    = 0.5;

const char *setTempFile = "/setTemp.cfg";
const char *deltaFile   = "/delta.cfg";
const char *nightFile   = "/night.cfg";   // "0,8" — начало и конец ночи в часах
const char *modeFile    = "/mode.cfg";    // auto / open / close

// Состояние --------------------------------------------------------------------------------------
enum class Mode : uint8_t { Auto, Open, Close };
// Порядок экранов в меню: график после режима, ночная дельта и ночной период стоят рядом
enum Screen : uint8_t { SCR_MAIN, SCR_TSET, SCR_MODE, SCR_GRAPH, SCR_DELTA, SCR_NIGHT, SCR_DATETIME, SCREEN_COUNT };
enum class Edit : uint8_t { None, Tset, Mode, Delta, DateTime, Night };

// настройки
float setTemp = DEFAULT_SET_TEMP;
float nightTempDelta = DEFAULT_DELTA;   // дельта для увеличения уставки ночью
uint8_t nightStart = 0, nightEnd = 8;
Mode mode = Mode::Auto;

// измерения
thermo::Median3 tempFilter;   // медиана трёх последних измерений
float curTemp = NAN;          // температура после фильтра: по ней работают экран и термостат
bool sensorOk = false;
uint32_t lastSensorMs = 0, lastValidTempMs = 0;
float curHum = NAN;           // влажность, тоже медиана трёх
thermo::Median3 humFilter;
thermo::History<96> history;      // температура, десятые доли градуса
thermo::History<96> openHistory;  // доля времени с открытым клапаном за точку, %
float historySum = 0;
uint16_t historyCount = 0;
uint16_t historyOpenCount = 0;    // сколько измерений за точку клапан был открыт
uint32_t lastHistoryMs = 0;
bool historyStarted = false;

// управление
Valve desiredValve = Valve::Unknown;
uint32_t lastSwitchMs = 0;
bool forceDecision = true;   // следующее решение термостата без ожидания MIN_SWITCH_MS
bool isNight = false;
bool frostActive = false;    // защита от замерзания: клапан открыт при любом режиме
bool shownConfirmed = true;  // какое состояние «время подтверждено» сейчас на экране

// интерфейс
Screen screen = SCR_MAIN;
Edit edit = Edit::None;
bool redraw = true;
uint32_t lastInputMs = 0, lastBlinkMs = 0, lastClockMs = 0;
bool blinkOn = true;
int8_t editField = 0;
float editTemp, editDelta;
Mode editMode;
uint8_t editHour, editMinute, editDay, editMonth, editNightStart, editNightEnd;
uint16_t editYear;
uint8_t shownMinute = 255;
bool saver = false;          // хранитель экрана включён
int16_t saverX = 0, saverY = 11;
uint32_t lastSaverMoveMs = 0, lastWarningMs = 0;
bool warningPhase = false;   // показывать ли сейчас предупреждение вместо режима
bool savedShown = false;     // сейчас на экране "Saved"
uint32_t savedAtMs = 0;
bool relayGraceOver = false; // первая минута после запуска прошла

// Functions ---------------------------------------------------------------------------------
void loadSettings();
void saveFloat(const char *path, float value);
void saveNight();
void saveMode();
const char *modeName(Mode m);
void readSensor(uint32_t nowMs);
void updateClock(uint32_t nowMs);
void updateControl(uint32_t nowMs);
void handleEncoder(uint32_t nowMs);
void updateScreenTimers(uint32_t nowMs);
const char *currentWarning();
void formatDuration(char *buf, size_t size, uint32_t ms);
void render();


// Initializing -------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Определение пинов  -----------------------------------------
  pinMode(ENC_VCC, OUTPUT);
  digitalWrite(ENC_VCC, HIGH);

  // Точка доступа для relay ------------------------------------------------------------------
  Serial.print("Access Point:");
  Serial.println(ssid);
  WiFi.softAP(ssid, password);
  Serial.print("IP:");
  Serial.println(WiFi.softAPIP());

  // Дисплей -----------------------------------------------------
  display.init();
  display.flipScreenVertically();
  startMessage();

  // Датчик DHT22: при ошибке продолжаем, на экране будет SENSOR ERR
  pinMode(DHT_PIN, INPUT);
  dht.begin();
  Serial.println(F("Initializing DHT22"));

  // Энкодер
  eb.setEncType(EB_STEP2);
  eb.setFastTimeout(FAST_TURN_MS);

  // Часы DS1307: без них работаем дальше, только без ночного режима
  Wire.begin();
  Wire.setClock(100000);
  Serial.println(F("Initializing DS1307"));
  initTimeAtBoot();   // проверка сбоя времени; при сбое — время сборки + 20 с, чтобы часы шли
  shownConfirmed = timeConfirmed;

  // Настройки: без файловой системы работаем со значениями по умолчанию
  if (LittleFS.begin()) {
    loadSettings();
  } else {
    Serial.println("Error while mounting LittleFS, using defaults");
  }

  uint32_t nowMs = millis();
  lastSensorMs = nowMs - SENSOR_PERIOD_MS;   // первое чтение датчика сразу
  lastValidTempMs = nowMs;
  lastInputMs = nowMs;
}

// Main Loop ------------------------------------------------------------------------------------------
void loop() {
  uint32_t nowMs = millis();
  eb.tick();

  readSensor(nowMs);
  updateClock(nowMs);
  handleEncoder(nowMs);
  updateScreenTimers(nowMs);
  updateControl(nowMs);

  if (redraw) {
    redraw = false;
    render();
  }
}


// Датчик и история -----------------------------------------------------------------------------------
void readSensor(uint32_t nowMs) {
  if (!intervalPassed(nowMs, lastSensorMs, SENSOR_PERIOD_MS)) return;
  lastSensorMs = nowMs;

  float raw = dht.readTemperature();
  if (raw == raw) {
    float t = tempFilter.push(raw);   // одиночный выброс датчика отбрасывается
    if (!sensorOk || roundTenth(t) != roundTenth(curTemp)) redraw = true;
    curTemp = t;
    sensorOk = true;
    lastValidTempMs = nowMs;
    float rh = dht.getHumidity();      // из того же опроса датчика
    if (rh == rh) curHum = humFilter.push(rh);
    static uint32_t lastSensorLogMs = 0;
    if (intervalPassed(nowMs, lastSensorLogMs, 60000)) {   // журнал раз в минуту для диагностики
      lastSensorLogMs = nowMs;
      Serial.printf("Sensor: raw %.1f, median %.1f C, RH %.0f%%\n", raw, t, curHum);
    }
    bool open = desiredValve == Valve::Open;
    historySum += t;
    historyCount++;
    if (open) historyOpenCount++;
    if (!historyStarted) {             // первая точка графика — сразу
      history.push((int16_t)roundf(t * 10));
      openHistory.push(open ? 100 : 0);
      historySum = 0;
      historyCount = 0;
      historyOpenCount = 0;
      lastHistoryMs = nowMs;
      historyStarted = true;
    }
  } else if (sensorOk && intervalPassed(nowMs, lastValidTempMs, SENSOR_FAIL_MS)) {
    Serial.println(F("DHT22: no data, sensor error"));
    sensorOk = false;
    curTemp = NAN;                     // термостат не трогает клапан
    curHum = NAN;
    tempFilter.reset();                // после ошибки не смешивать старые и новые измерения
    humFilter.reset();
    redraw = true;
  }

  if (historyStarted && intervalPassed(nowMs, lastHistoryMs, HISTORY_PERIOD_MS)) {
    lastHistoryMs = nowMs;
    if (historyCount > 0) {            // среднее за 15 минут
      history.push((int16_t)roundf(historySum / historyCount * 10));
      openHistory.push((int16_t)(historyOpenCount * 100 / historyCount));
      historySum = 0;
      historyCount = 0;
      historyOpenCount = 0;
      if (screen == SCR_GRAPH) redraw = true;
    }
  }
}

// Часы ----------------------------------------------------------------------------------------------
void updateClock(uint32_t nowMs) {
  if (!intervalPassed(nowMs, lastClockMs, CLOCK_PERIOD_MS)) return;
  lastClockMs = nowMs;

  bool wasValid = timeValid;
  readClock();
  // ночной режим только по подтверждённому времени: сбитые часы не должны поднимать уставку днём
  bool night = timeValid && timeConfirmed && thermo::isNightHour(now_t.hour, nightStart, nightEnd);
  if (shownConfirmed != timeConfirmed) {
    shownConfirmed = timeConfirmed;
    redraw = true;
  }
  if (night != isNight) {
    isNight = night;
    forceDecision = true;              // смена дня/ночи — уставка изменилась
    redraw = true;
  }
  if (screen == SCR_DATETIME && edit == Edit::None &&
      (wasValid != timeValid || shownMinute != now_t.minute)) {
    redraw = true;
  }
}

// Термостат -------------------------------------------------------------------------------------------
void updateControl(uint32_t nowMs) {
  bool frost = thermo::frostProtection(frostActive, curTemp, FROST_ON_TEMP, FROST_OFF_TEMP);
  if (frost != frostActive) {
    Serial.printf("Frost protection %s (temp %.1f)\n", frost ? "ON" : "OFF", curTemp);
    frostActive = frost;
    forceDecision = true;
    redraw = true;
  }

  Valve want = desiredValve;
  if (frostActive) {
    want = Valve::Open;               // при любом режиме, даже ручном «закрыт»
  } else if (mode == Mode::Open) {
    want = Valve::Open;
  } else if (mode == Mode::Close) {
    want = Valve::Closed;
  } else {
    float target = setTemp + (isNight ? nightTempDelta : 0);
    Valve decision = thermo::decideValve(curTemp, target, HYSTERESIS, desiredValve);
    if (decision != desiredValve &&
        (forceDecision || desiredValve == Valve::Unknown || intervalPassed(nowMs, lastSwitchMs, MIN_SWITCH_MS))) {
      want = decision;
    }
  }
  forceDecision = false;

  if (want != desiredValve) {
    Serial.printf("Valve: %s -> %s (temp %.1f)\n", valveName(desiredValve), valveName(want), curTemp);
    desiredValve = want;
    lastSwitchMs = nowMs;
  }
  if (syncRelay(desiredValve, nowMs) && screen == SCR_MAIN) redraw = true;
}

// Энкодер ---------------------------------------------------------------------------------------------
void startEdit() {
  editField = 0;
  switch (screen) {
    case SCR_TSET:  editTemp = setTemp;         edit = Edit::Tset;  break;
    case SCR_MODE:  editMode = mode;            edit = Edit::Mode;  break;
    case SCR_DELTA: editDelta = nightTempDelta; edit = Edit::Delta; break;
    case SCR_DATETIME:
      editHour = now_t.hour;  editMinute = now_t.minute;
      editDay = now_t.day;    editMonth = now_t.month;
      editYear = now_t.year < 2024 ? 2024 : now_t.year;
      edit = Edit::DateTime;
      break;
    case SCR_NIGHT:
      editNightStart = nightStart;
      editNightEnd = nightEnd;
      edit = Edit::Night;
      break;
    default:
      break;   // на главном экране и графике правки нет
  }
}

void finishEdit() {
  edit = Edit::None;
  eb.counter = screen;   // повороты во время правки не должны листать меню
  shownMinute = 255;
  redraw = true;
}

void saveEdit() {
  switch (edit) {
    case Edit::Tset:
      setTemp = editTemp;
      saveFloat(setTempFile, setTemp);
      forceDecision = true;
      break;
    case Edit::Mode:
      mode = editMode;
      saveMode();
      forceDecision = true;
      break;
    case Edit::Delta:
      nightTempDelta = editDelta;
      saveFloat(deltaFile, nightTempDelta);
      forceDecision = true;
      break;
    case Edit::DateTime:
      writeClock(editHour, editMinute, 0, editDay, editMonth, editYear, true);   // снимает SET TIME!
      break;
    case Edit::Night:
      nightStart = editNightStart;
      nightEnd = editNightEnd;
      saveNight();
      lastClockMs = millis() - CLOCK_PERIOD_MS;   // сразу пересчитать день/ночь
      break;
    default:
      break;
  }
}

// fast — быстрое вращение: уставка и дельта по 0.5 °C, минуты по 5
void changeValue(int8_t dir, bool fast) {
  float tempStep = fast ? 0.5f : 0.1f;
  switch (edit) {
    case Edit::Tset:
      editTemp = constrain(roundTenth(editTemp + tempStep * dir), 5.0f, 35.0f);
      break;
    case Edit::Mode:
      // по кругу в обе стороны: auto -> open -> close -> auto
      editMode = (Mode)wrapValue((int)editMode + dir, (int)Mode::Auto, (int)Mode::Close);
      break;
    case Edit::Delta:
      editDelta = constrain(roundTenth(editDelta + tempStep * dir), -5.0f, 5.0f);
      break;
    case Edit::DateTime:
      switch (editField) {
        case 0: editHour = wrapValue(editHour + dir, 0, 23); break;
        case 1: editMinute = wrapValue(editMinute + dir * (fast ? 5 : 1), 0, 59); break;
        case 2: editDay = wrapValue(editDay + dir, 1, thermo::daysInMonth(editMonth, editYear)); break;
        case 3: editMonth = wrapValue(editMonth + dir, 1, 12); break;
        case 4: editYear = constrain(editYear + dir, 2024, 2099); break;
      }
      // 31 января -> февраль: день не может быть больше дней в месяце
      if (editDay > thermo::daysInMonth(editMonth, editYear)) editDay = thermo::daysInMonth(editMonth, editYear);
      break;
    case Edit::Night:
      if (editField == 0) editNightStart = wrapValue(editNightStart + dir, 0, 23);
      else editNightEnd = wrapValue(editNightEnd + dir, 0, 23);
      break;
    default:
      break;
  }
}

void handleEncoder(uint32_t nowMs) {
  bool input = eb.turn() || eb.click() || eb.hold();
  if (input) {
    lastInputMs = nowMs;
    redraw = true;
    savedShown = false;
  }

  // хранитель экрана: первое касание только будит экран и ничего не меняет
  if (saver) {
    if (input) {
      saver = false;
      display.setContrast(NORMAL_CONTRAST);
      eb.counter = screen;
    }
    return;
  }

  if (edit == Edit::None) {
    if (eb.turn()) {
      if (eb.counter >= SCREEN_COUNT || eb.counter <= -SCREEN_COUNT) eb.counter = 0;
      screen = (Screen)abs(eb.counter);
      shownMinute = 255;
    }
    if (eb.click()) startEdit();
    return;
  }

  // режим правки
  if (eb.turn()) {
    changeValue(eb.left() ? 1 : -1, eb.fast());   // как раньше: влево — больше
    blinkOn = true;
    lastBlinkMs = nowMs;
  }
  if (eb.click()) {
    if (edit == Edit::DateTime) editField = (editField + 1) % 5;
    if (edit == Edit::Night) editField = (editField + 1) % 2;
    blinkOn = true;
    lastBlinkMs = nowMs;
  }
  if (eb.hold()) {
    saveEdit();
    finishEdit();
    savedShown = true;   // подтверждение: при отмене по таймауту его нет
    savedAtMs = nowMs;
    return;
  }
  if (intervalPassed(nowMs, lastInputMs, EDIT_TIMEOUT_MS)) {
    Serial.println(F("Edit timeout, changes discarded"));
    finishEdit();
    return;
  }
  if ((edit == Edit::DateTime || edit == Edit::Night) && intervalPassed(nowMs, lastBlinkMs, BLINK_MS)) {
    blinkOn = !blinkOn;
    lastBlinkMs = nowMs;
    redraw = true;
  }
}

// Возврат на главный экран, хранитель экрана, мигающая точка ----------------------------------------
void moveSaver() {
  // ширина по самой длинной надписи под температурой, чтобы смена статуса не вылезла за край
  int16_t maxX = 128 - saverBlockWidth(curTemp, "CLOSED 99m  100%");
  if (maxX < 0) maxX = 0;
  // по кругу через всю синюю зону, чтобы пиксели изнашивались равномерно
  saverX = (saverX + 37) % (maxX + 1);
  saverY = 11 + (saverY - 11 + 5) % 15;   // от 11 до 25: блок целиком в синей зоне
}

void updateScreenTimers(uint32_t nowMs) {
  if (savedShown && intervalPassed(nowMs, savedAtMs, SAVED_MSG_MS)) {
    savedShown = false;
    redraw = true;
  }
  if (!relayGraceOver && nowMs >= RELAY_GRACE_MS) {
    relayGraceOver = true;
    if (!relayLink) redraw = true;   // реле так и не ответило — пора показать NO LINK
  }
  if (edit == Edit::None && !saver) {
    if (screen != SCR_MAIN && intervalPassed(nowMs, lastInputMs, RETURN_MAIN_MS)) {
      screen = SCR_MAIN;
      eb.counter = 0;
      redraw = true;
    }
    if (intervalPassed(nowMs, lastInputMs, SAVER_MS)) {
      saver = true;
      screen = SCR_MAIN;
      eb.counter = 0;
      display.setContrast(SAVER_CONTRAST);
      moveSaver();
      lastSaverMoveMs = nowMs;
      redraw = true;
    }
  }
  if (saver && intervalPassed(nowMs, lastSaverMoveMs, SAVER_MOVE_MS)) {
    moveSaver();
    lastSaverMoveMs = nowMs;
    redraw = true;
  }
  // предупреждение чередуется с обычной строкой; без предупреждения экран не перерисовывается зря
  if (currentWarning() && (saver || (screen == SCR_MAIN && edit == Edit::None)) &&
      intervalPassed(nowMs, lastWarningMs, WARNING_PHASE_MS)) {
    warningPhase = !warningPhase;
    lastWarningMs = nowMs;
    redraw = true;
  }
}

// Длительность для хранителя экрана: "45m", после 100 минут — "5h"
void formatDuration(char *buf, size_t size, uint32_t ms) {
  uint32_t minutes = ms / 60000;
  if (minutes < 100) snprintf(buf, size, "%lum", (unsigned long)minutes);
  else snprintf(buf, size, "%luh", (unsigned long)(minutes / 60));
}

// Предупреждение для главного экрана и хранителя, или nullptr
const char *currentWarning() {
  if (!timeValid) return "RTC ERROR!";
  if (!timeConfirmed) return "SET TIME!";
  return nullptr;
}

// Экран -----------------------------------------------------------------------------------------------
void render() {
  char buf[24];
  bool editing = edit != Edit::None;
  const char *warning = currentWarning();

  // NO LINK — только если реле не ответило и после первой минуты; до этого "---"
  bool linkOk = relayLink || !relayGraceOver;

  if (saver) {
    // строка под температурой: клапан и сколько он в этом положении, влажность
    char status[32];
    char hum[8] = "";
    if (curHum == curHum) snprintf(hum, sizeof(hum), "  %.0f%%", curHum);
    if (warning && warningPhase) snprintf(status, sizeof(status), "%s", warning);
    else if (frostActive) snprintf(status, sizeof(status), "FROST%s", hum);
    else if (!linkOk) snprintf(status, sizeof(status), "NO LINK%s", hum);
    else {
      char dur[8];
      formatDuration(dur, sizeof(dur), millis() - lastSwitchMs);
      snprintf(status, sizeof(status), "%s %s%s", valveName(valveState), dur, hum);
    }
    saverDisplay(saverX, saverY, curTemp, status);
    return;
  }

  if (savedShown) {
    messageDisplay("Saved");
    return;
  }

  switch (screen) {
    case SCR_MAIN: {
      // ночью показывается фактическая уставка: дневная + ночная дельта
      bool nightActive = isNight && mode == Mode::Auto && !frostActive;
      mainDisplay(curTemp, setTemp + (nightActive ? nightTempDelta : 0),
                  frostActive ? "FROST" : modeName(mode), nightActive,
                  valveName(valveState), linkOk, warning, warningPhase);
      break;
    }
    case SCR_TSET:
      snprintf(buf, sizeof(buf), "%.1f°C", editing ? editTemp : setTemp);
      valueDisplay(editing ? "EDIT: SETPOINT" : "SETPOINT", buf, editing);
      break;
    case SCR_MODE:
      valueDisplay(editing ? "EDIT: MODE" : "MODE", modeName(editing ? editMode : mode), editing);
      break;
    case SCR_DELTA:
      snprintf(buf, sizeof(buf), "%.1f°C", editing ? editDelta : nightTempDelta);
      valueDisplay(editing ? "EDIT: NIGHT DELTA" : "NIGHT DELTA", buf, editing);
      break;
    case SCR_DATETIME:
      if (editing) {
        static const char *fieldNames[] = {"EDIT: HOUR", "EDIT: MINUTE", "EDIT: DAY", "EDIT: MONTH", "EDIT: YEAR"};
        dateTimeDisplay(fieldNames[editField], true, editHour, editMinute, editDay, editMonth, editYear, editField, blinkOn);
      } else {
        static const char *days[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
        const char *header = timeValid ? days[thermo::dayOfWeek(now_t.day, now_t.month, now_t.year)] : "DATE & TIME";
        dateTimeDisplay(header, timeValid, now_t.hour, now_t.minute, now_t.day, now_t.month, now_t.year, -1, true);
        shownMinute = now_t.minute;
      }
      break;
    case SCR_NIGHT:
      if (editing) nightDisplay(editField == 0 ? "EDIT: NIGHT START" : "EDIT: NIGHT END",
                                editNightStart, editNightEnd, editField, blinkOn);
      else nightDisplay("NIGHT PERIOD", nightStart, nightEnd, -1, true);
      break;
    case SCR_GRAPH: {
      // заголовок: сколько часов клапан был открыт за время графика, текущая влажность
      uint32_t openSum = 0;
      for (uint16_t i = 0; i < openHistory.size(); i++) openSum += openHistory.at(i);
      float heatHours = openSum / 100.0f * HISTORY_PERIOD_MS / 3600000.0f;
      char header[40];
      int len = snprintf(header, sizeof(header), "HEAT %.1fh", heatHours);   // "24H" не влезал
      if (curHum == curHum) snprintf(header + len, sizeof(header) - len, "   RH %.0f%%", curHum);
      graphDisplay(header, history, openHistory, setTemp);
      break;
    }
    default:
      break;
  }
}

// Настройки ---------------------------------------------------------------------------------------
const char *modeName(Mode m) {
  switch (m) {
    case Mode::Open:  return "open";
    case Mode::Close: return "close";
    default:          return "auto";
  }
}

void loadSettings() {
  setTemp = readFloatSetting(LittleFS, setTempFile, DEFAULT_SET_TEMP, 5, 35);
  nightTempDelta = readFloatSetting(LittleFS, deltaFile, DEFAULT_DELTA, -5, 5);

  String night = readFile(LittleFS, nightFile);
  int s, e;
  if (sscanf(night.c_str(), "%d,%d", &s, &e) == 2 && s >= 0 && s <= 23 && e >= 0 && e <= 23) {
    nightStart = s;
    nightEnd = e;
  }

  String m = readFile(LittleFS, modeFile);
  m.trim();
  if (m == "open") mode = Mode::Open;
  else if (m == "close") mode = Mode::Close;
  else mode = Mode::Auto;

  Serial.printf("Settings: Tset %.1f, delta %.1f, night %u-%u, mode %s\n",
                setTemp, nightTempDelta, nightStart, nightEnd, modeName(mode));
}

void saveFloat(const char *path, float value) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", value);
  writeFile(LittleFS, path, buf);
}

void saveNight() {
  char buf[8];
  snprintf(buf, sizeof(buf), "%u,%u", nightStart, nightEnd);
  writeFile(LittleFS, nightFile, buf);
}

void saveMode() {
  writeFile(LittleFS, modeFile, modeName(mode));
}
