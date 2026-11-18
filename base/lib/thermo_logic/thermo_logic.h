// Логика термостата без привязки к железу: её можно проверять тестами (test/test_logic).
#pragma once
#include <stdint.h>

namespace thermo {

enum class Valve : uint8_t { Unknown, Open, Closed };

// Ночной период [start, end) в часах, с переходом через полночь (например 22..6).
// start == end — ночной режим выключен.
inline bool isNightHour(uint8_t hour, uint8_t start, uint8_t end) {
  if (start == end) return false;
  if (start < end) return hour >= start && hour < end;
  return hour >= start || hour < end;
}

// Решение термостата: какое состояние клапана нужно.
// Внутри диапазона [target - hyst, target + hyst] состояние не меняется.
// Если текущее состояние неизвестно (после старта), выбирается по стороне от уставки.
// Если температура неизвестна (NaN), состояние не меняется.
inline Valve decideValve(float cur, float target, float hyst, Valve current) {
  if (cur != cur) return current;  // NaN — датчик не отвечает
  if (cur > target + hyst) return Valve::Closed;
  if (cur < target - hyst) return Valve::Open;
  if (current == Valve::Unknown) return cur >= target ? Valve::Closed : Valve::Open;
  return current;
}

// Прошло ли interval мс с момента last. Корректно при переполнении millis() (~49 дней).
inline bool intervalPassed(uint32_t now, uint32_t last, uint32_t interval) {
  return (uint32_t)(now - last) >= interval;
}

// Округление до десятых, чтобы шаги по 0.1 не накапливали ошибку float.
inline float roundTenth(float v) {
  return (float)(int32_t)(v * 10.0f + (v >= 0 ? 0.5f : -0.5f)) / 10.0f;
}

// Значение по кругу в пределах [lo, hi]: 23 + 1 -> 0 для часов и т. п.
inline int wrapValue(int v, int lo, int hi) {
  int span = hi - lo + 1;
  while (v < lo) v += span;
  while (v > hi) v -= span;
  return v;
}

inline bool isLeapYear(uint16_t y) {
  return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

inline uint8_t daysInMonth(uint8_t month, uint16_t year) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 31;
  if (month == 2 && isLeapYear(year)) return 29;
  return days[month - 1];
}

// День недели: 0 = воскресенье ... 6 = суббота (как ждёт ErriezDS1307).
inline uint8_t dayOfWeek(uint8_t day, uint8_t month, uint16_t year) {
  static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) year -= 1;
  return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

// Кольцевой буфер истории температуры (значения в десятых долях градуса).
template <uint16_t N>
class History {
 public:
  void push(int16_t v) {
    data_[head_] = v;
    head_ = (head_ + 1) % N;
    if (count_ < N) count_++;
  }
  uint16_t size() const { return count_; }
  static constexpr uint16_t capacity() { return N; }
  // i = 0 — самое старое значение, size() - 1 — самое новое
  int16_t at(uint16_t i) const { return data_[(head_ + N - count_ + i) % N]; }
  int16_t minValue() const {
    int16_t m = at(0);
    for (uint16_t i = 1; i < count_; i++) if (at(i) < m) m = at(i);
    return m;
  }
  int16_t maxValue() const {
    int16_t m = at(0);
    for (uint16_t i = 1; i < count_; i++) if (at(i) > m) m = at(i);
    return m;
  }

 private:
  int16_t data_[N] = {};
  uint16_t head_ = 0;
  uint16_t count_ = 0;
};

}  // namespace thermo
