// Тесты логики термостата (lib/thermo_logic). Запускаются на самом контроллере:
//   pio test -d base
// После тестов основную прошивку нужно залить заново: pio run -d base -t upload
#include <Arduino.h>
#include <unity.h>
#include <thermo_logic.h>

using namespace thermo;

void test_night_hours() {
  // 0..8, как было зашито раньше
  TEST_ASSERT_TRUE(isNightHour(0, 0, 8));
  TEST_ASSERT_TRUE(isNightHour(7, 0, 8));
  TEST_ASSERT_FALSE(isNightHour(8, 0, 8));
  TEST_ASSERT_FALSE(isNightHour(23, 0, 8));
  // через полночь
  TEST_ASSERT_TRUE(isNightHour(22, 22, 6));
  TEST_ASSERT_TRUE(isNightHour(3, 22, 6));
  TEST_ASSERT_FALSE(isNightHour(6, 22, 6));
  TEST_ASSERT_FALSE(isNightHour(12, 22, 6));
  // начало = конец — ночного режима нет
  TEST_ASSERT_FALSE(isNightHour(5, 5, 5));
}

void test_decide_valve() {
  // выше диапазона — закрыть, ниже — открыть
  TEST_ASSERT_EQUAL(Valve::Closed, decideValve(22.3, 22.0, 0.2, Valve::Open));
  TEST_ASSERT_EQUAL(Valve::Open, decideValve(21.7, 22.0, 0.2, Valve::Closed));
  // внутри диапазона — без изменений
  TEST_ASSERT_EQUAL(Valve::Open, decideValve(22.15, 22.0, 0.2, Valve::Open));
  TEST_ASSERT_EQUAL(Valve::Closed, decideValve(21.85, 22.0, 0.2, Valve::Closed));
  // после старта внутри диапазона — по стороне от уставки
  TEST_ASSERT_EQUAL(Valve::Closed, decideValve(22.1, 22.0, 0.2, Valve::Unknown));
  TEST_ASSERT_EQUAL(Valve::Open, decideValve(21.9, 22.0, 0.2, Valve::Unknown));
  // датчик не отвечает — клапан не трогаем
  TEST_ASSERT_EQUAL(Valve::Open, decideValve(NAN, 22.0, 0.2, Valve::Open));
  TEST_ASSERT_EQUAL(Valve::Unknown, decideValve(NAN, 22.0, 0.2, Valve::Unknown));
}

void test_interval_overflow() {
  TEST_ASSERT_TRUE(intervalPassed(1000, 0, 1000));
  TEST_ASSERT_FALSE(intervalPassed(999, 0, 1000));
  // переполнение millis(): 0xFFFFFF00 -> 0x00000100 = 512 мс
  TEST_ASSERT_TRUE(intervalPassed(0x00000100u, 0xFFFFFF00u, 512));
  TEST_ASSERT_FALSE(intervalPassed(0x00000100u, 0xFFFFFF00u, 513));
}

void test_round_and_wrap() {
  float v = 22.0;
  for (int i = 0; i < 7; i++) v = roundTenth(v + 0.1f);
  TEST_ASSERT_EQUAL_FLOAT(22.7f, v);
  TEST_ASSERT_EQUAL_FLOAT(-0.3f, roundTenth(-0.29f));
  TEST_ASSERT_EQUAL(0, wrapValue(24, 0, 23));
  TEST_ASSERT_EQUAL(23, wrapValue(-1, 0, 23));
  TEST_ASSERT_EQUAL(12, wrapValue(0, 1, 12));
  TEST_ASSERT_EQUAL(1, wrapValue(13, 1, 12));
}

void test_calendar() {
  TEST_ASSERT_TRUE(isLeapYear(2024));
  TEST_ASSERT_FALSE(isLeapYear(2026));
  TEST_ASSERT_FALSE(isLeapYear(2100));
  TEST_ASSERT_TRUE(isLeapYear(2000));
  TEST_ASSERT_EQUAL(29, daysInMonth(2, 2024));
  TEST_ASSERT_EQUAL(28, daysInMonth(2, 2026));
  TEST_ASSERT_EQUAL(30, daysInMonth(11, 2026));
  TEST_ASSERT_EQUAL(31, daysInMonth(12, 2026));
  // 0 = воскресенье
  TEST_ASSERT_EQUAL(0, dayOfWeek(11, 8, 2024));   // 11.08.2024 — воскресенье
  TEST_ASSERT_EQUAL(3, dayOfWeek(18, 11, 2026));  // 18.11.2026 — среда
  TEST_ASSERT_EQUAL(4, dayOfWeek(1, 1, 2026));    // 01.01.2026 — четверг
  TEST_ASSERT_EQUAL(4, dayOfWeek(29, 2, 2024));   // 29.02.2024 — четверг
}

void test_history() {
  History<4> h;
  TEST_ASSERT_EQUAL(0, h.size());
  h.push(10); h.push(20); h.push(15);
  TEST_ASSERT_EQUAL(3, h.size());
  TEST_ASSERT_EQUAL(10, h.at(0));
  TEST_ASSERT_EQUAL(15, h.at(2));
  h.push(30); h.push(5);              // вытесняет самое старое (10)
  TEST_ASSERT_EQUAL(4, h.size());
  TEST_ASSERT_EQUAL(20, h.at(0));
  TEST_ASSERT_EQUAL(5, h.at(3));
  TEST_ASSERT_EQUAL(5, h.minValue());
  TEST_ASSERT_EQUAL(30, h.maxValue());
}

void test_median() {
  TEST_ASSERT_EQUAL_FLOAT(2, median3(1, 2, 3));
  TEST_ASSERT_EQUAL_FLOAT(2, median3(3, 2, 1));
  TEST_ASSERT_EQUAL_FLOAT(2, median3(2, 3, 1));
  TEST_ASSERT_EQUAL_FLOAT(2, median3(1, 3, 2));
  TEST_ASSERT_EQUAL_FLOAT(5, median3(5, 5, 9));

  Median3 m;
  TEST_ASSERT_EQUAL_FLOAT(22.0, m.push(22.0));    // меньше трёх — последнее
  TEST_ASSERT_EQUAL_FLOAT(22.1, m.push(22.1));
  TEST_ASSERT_EQUAL_FLOAT(22.1, m.push(22.2));
  TEST_ASSERT_EQUAL_FLOAT(22.2, m.push(35.0));    // выброс 35 отброшен
  TEST_ASSERT_EQUAL_FLOAT(22.3, m.push(22.3));    // окно 35, 22.3, 22.2
  TEST_ASSERT_EQUAL_FLOAT(22.4, m.push(22.4));    // окно 35, 22.3, 22.4
  TEST_ASSERT_EQUAL_FLOAT(22.4, m.push(22.5));    // выброс ушёл из окна: 22.5, 22.3, 22.4
}

void test_frost() {
  TEST_ASSERT_TRUE(frostProtection(false, 9.9, 10, 11));
  TEST_ASSERT_FALSE(frostProtection(false, 10.5, 10, 11));  // ещё не включилась
  TEST_ASSERT_TRUE(frostProtection(true, 10.5, 10, 11));    // гистерезис: держится
  TEST_ASSERT_FALSE(frostProtection(true, 11.1, 10, 11));
  TEST_ASSERT_TRUE(frostProtection(true, NAN, 10, 11));     // датчик молчит — без изменений
}

void setup() {
  delay(2000);   // дать монитору порта подключиться
  UNITY_BEGIN();
  RUN_TEST(test_median);
  RUN_TEST(test_frost);
  RUN_TEST(test_night_hours);
  RUN_TEST(test_decide_valve);
  RUN_TEST(test_interval_overflow);
  RUN_TEST(test_round_and_wrap);
  RUN_TEST(test_calendar);
  RUN_TEST(test_history);
  UNITY_END();
}

void loop() {}
