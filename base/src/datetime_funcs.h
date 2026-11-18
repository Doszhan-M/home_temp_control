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
bool rtcFound = false;    // модуль часов ответил на шине I2C
bool timeValid = false;   // последнее чтение времени успешно

// Запустить часы. Без них термостат работает дальше, только без ночного режима.
bool initClock() {
  static bool reported = false;   // не повторять сообщение каждую секунду
  rtcFound = rtc.begin();
  if (!rtcFound && !reported) Serial.println(F("RTC not found"));
  reported = !rtcFound;
  return rtcFound;
}

// Считать время из часов. При ошибке timeValid = false.
bool readClock() {
  if (!rtcFound && !initClock()) {
    timeValid = false;
    return false;
  }
  ClockTime t;
  timeValid = rtc.getDateTime(&t.hour, &t.minute, &t.second, &t.day, &t.month, &t.year, &t.wday);
  if (timeValid) now_t = t;
  else rtcFound = false;  // при следующем чтении попробуем заново найти часы
  return timeValid;
}

// Записать время в часы. День недели вычисляется сам.
bool writeClock(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint16_t year) {
  bool ok = rtc.setDateTime(hour, minute, second, day, month, year, thermo::dayOfWeek(day, month, year));
  Serial.printf("Set RTC %02u:%02u:%02u %02u.%02u.%u -> %s\n", hour, minute, second, day, month, year, ok ? "ok" : "FAILED");
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
    writeClock(targetHour, targetMinute, targetSecond, targetDay, targetMonth, targetYear);
  }
}

// Установить время по моменту компиляции прошивки (__DATE__ / __TIME__ — локальное время ПК),
// если часы отстают от него. uploadDelaySec — поправка на время сборки и загрузки прошивки.
// Запасной вариант: основной способ — экран даты/времени на энкодере.
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
