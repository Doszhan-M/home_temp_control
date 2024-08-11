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
