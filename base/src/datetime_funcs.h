extern ErriezDS1307 rtc;

uint8_t hour = 21;
uint8_t minut = 15;
uint8_t sec;
uint8_t mday = 28;
uint8_t mon = 2;
uint16_t year = 2024;
uint8_t wday;
String month;
String minute;
String dateTime;
String cur_time;
String cur_date;

// Получить текущее время из часов ----------------------------------------------
String getTime()
{
  if (!rtc.getDateTime(&hour, &minut, &sec, &mday, &mon, &year, &wday))
  {
    dateTime = "Failed";
    Serial.println(dateTime);
    return dateTime;
  } 
  else
  {
    if (minut < 10)
    {
      minute = "0" + String(minut);
    }
    else
    {
      minute = String(minut);
    };
    cur_time = String(hour) + ":" + minute;
    return cur_time;
  };
};

String getDate()
{
  if (!rtc.getDateTime(&hour, &minut, &sec, &mday, &mon, &year, &wday))
  {
    dateTime = "Failed";
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
    cur_date = String(mday) + "." + month + "." + String(year);
    return cur_date;
  };
};

// Установить время, если текущее время отстает от заданного
void setTimeIfOutdated(uint8_t targetHour, uint8_t targetMinute, uint8_t targetSecond, 
                       uint8_t targetDay, uint8_t targetMonth, uint16_t targetYear, uint8_t targetWday) {
  rtc.getDateTime(&hour, &minut, &sec, &mday, &mon, &year, &wday);
  if (year < targetYear ||
      (year == targetYear && mon < targetMonth) ||
      (year == targetYear && mon == targetMonth && mday < targetDay) ||
      (year == targetYear && mon == targetMonth && mday == targetDay && hour < targetHour) ||
      (year == targetYear && mon == targetMonth && mday == targetDay && hour == targetHour && minut < targetMinute)) 
  {
    rtc.setDateTime(targetHour, targetMinute, targetSecond, targetDay, targetMonth, targetYear, targetWday);
  }
}

// Установить время по моменту компиляции прошивки (__DATE__ / __TIME__ — локальное время ПК),
// если часы отстают от него. uploadDelaySec — поправка на время сборки и загрузки прошивки.
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

  // mktime/gmtime_r нормализуют дату после прибавления поправки и вычисляют день недели
  time_t ts = mktime(&t) + uploadDelaySec;
  gmtime_r(&ts, &t);

  setTimeIfOutdated(t.tm_hour, t.tm_min, t.tm_sec, t.tm_mday, t.tm_mon + 1,
                    t.tm_year + 1900, t.tm_wday);   // tm_wday: 0 = воскресенье
}
