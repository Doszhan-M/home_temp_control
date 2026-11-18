#include <thermo_logic.h>

extern ErriezDS1307 rtc;

// Текущее время из часов (обновляется readClock() раз в секунду)
struct ClockTime {
  uint8_t hour = 0, minute = 0, second = 0;
  uint8_t day = 1, month = 1;
  uint16_t year = 2024;
  uint8_t wday = 0;
};

ClockTime now_t;
bool rtcFound = false;       // модуль часов ответил на шине I2C
bool timeValid = false;      // последнее чтение времени успешно
bool timeConfirmed = false;  // время выставлено с энкодера и с тех пор не пропадало

// Метка в памяти DS1307 (её питает та же батарейка, что и часы). Пишется, когда время
// выставляют с энкодера. Нет метки — батарейка не удержала память, а значит и время.
const uint8_t RTC_MARKER_REG = 0x08;   // начало 56 байт RAM DS1307
const uint8_t RTC_MARKER[2] = {0xA5, 0x5A};

bool hasMarker() {
  uint8_t buf[2] = {0, 0};
  return rtc.readBuffer(RTC_MARKER_REG, buf, 2) && buf[0] == RTC_MARKER[0] && buf[1] == RTC_MARKER[1];
}

bool writeMarker() {
  uint8_t buf[2] = {RTC_MARKER[0], RTC_MARKER[1]};
  return rtc.writeBuffer(RTC_MARKER_REG, buf, 2);
}

// Год сборки прошивки: часы не могут показывать время раньше него
uint16_t buildYear() {
  return atoi(__DATE__ + 7);   // "Nov 18 2026"
}

// Запустить часы. Без них термостат работает дальше, только без ночного режима.
bool initClock() {
  static bool reported = false;   // не повторять сообщение каждую секунду
  bool wasFound = rtcFound;
  rtcFound = rtc.begin();
  if (!rtcFound && !reported) Serial.println(F("RTC not found"));
  reported = !rtcFound;
  // часы снова нашлись после пропажи — проверить, не пропало ли время
  if (rtcFound && !wasFound && timeConfirmed && !hasMarker()) timeConfirmed = false;
  return rtcFound;
}

void lostTime(const char *reason) {
  if (timeConfirmed) Serial.printf("RTC: time lost (%s)\n", reason);
  timeConfirmed = false;
}

// Считать время из часов. При ошибке timeValid = false.
bool readClock() {
  if (!rtcFound && !initClock()) {
    timeValid = false;
    return false;
  }
  ClockTime t;
  timeValid = rtc.getDateTime(&t.hour, &t.minute, &t.second, &t.day, &t.month, &t.year, &t.wday);
  if (!timeValid) {
    rtcFound = false;  // при следующем чтении попробуем заново найти часы
    return false;
  }
  now_t = t;
  if (!rtc.isRunning()) lostTime("clock halted");
  if (t.year < buildYear()) lostTime("date before firmware build");
  return true;
}

// Записать время в часы. День недели вычисляется сам.
// confirm = true — время выставил человек: ставим метку и снимаем предупреждение.
bool writeClock(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month,
                uint16_t year, bool confirm) {
  bool ok = rtc.setDateTime(hour, minute, second, day, month, year, thermo::dayOfWeek(day, month, year));
  Serial.printf("Set RTC %02u:%02u:%02u %02u.%02u.%u -> %s\n", hour, minute, second, day, month, year, ok ? "ok" : "FAILED");
  if (ok && confirm) {
    timeConfirmed = writeMarker();
    if (!timeConfirmed) Serial.println(F("RTC: marker write failed"));
  }
  readClock();
  return ok;
}

// Установить время, если текущее время в часах отстает от заданного
void setTimeIfOutdated(uint8_t targetHour, uint8_t targetMinute, uint8_t targetSecond,
                       uint8_t targetDay, uint8_t targetMonth, uint16_t targetYear) {
  if (!readClock()) return;
  const ClockTime &c = now_t;
  if (c.year < targetYear ||
      (c.year == targetYear && c.month < targetMonth) ||
      (c.year == targetYear && c.month == targetMonth && c.day < targetDay) ||
      (c.year == targetYear && c.month == targetMonth && c.day == targetDay && c.hour < targetHour) ||
      (c.year == targetYear && c.month == targetMonth && c.day == targetDay && c.hour == targetHour && c.minute < targetMinute))
  {
    writeClock(targetHour, targetMinute, targetSecond, targetDay, targetMonth, targetYear, false);
  }
}

// Установить время по моменту компиляции прошивки (__DATE__ / __TIME__ — локальное время ПК),
// если часы отстают от него. uploadDelaySec — поправка на время сборки и загрузки прошивки.
// Такое время не считается подтверждённым: оно нужно только чтобы часы шли.
void setTimeFromBuild(uint16_t uploadDelaySec) {
  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char monStr[4];
  int d, y, h, m, s;
  sscanf(__DATE__, "%3s %d %d", monStr, &d, &y);   // "Nov 18 2026"
  sscanf(__TIME__, "%d:%d:%d", &h, &m, &s);         // "15:32:07"

  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon  = (strstr(months, monStr) - months) / 3;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min  = m;
  t.tm_sec  = s;

  // mktime/gmtime_r нормализуют дату после прибавления поправки
  time_t ts = mktime(&t) + uploadDelaySec;
  gmtime_r(&ts, &t);

  setTimeIfOutdated(t.tm_hour, t.tm_min, t.tm_sec, t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
}

// При запуске: проверить, не пропадало ли время, и запустить часы.
// Признаки сбоя: часы остановлены, нет метки в памяти, дата раньше сборки прошивки.
void initTimeAtBoot() {
  if (!initClock()) return;
  bool running = rtc.isRunning();
  bool marker = hasMarker();
  readClock();
  bool plausible = timeValid && now_t.year >= buildYear();
  timeConfirmed = running && marker && plausible;
  Serial.printf("RTC: running=%d marker=%d plausible=%d -> %s\n", running, marker, plausible,
                timeConfirmed ? "time OK" : "SET TIME");
  // время из прошивки — только при сбое, чтобы часы шли с правдоподобного времени.
  // Время, выставленное с энкодера, прошивка не перезаписывает.
  if (!timeConfirmed) setTimeFromBuild(20);
  if (!rtc.isRunning()) rtc.clockEnable(true);
  readClock();
}
