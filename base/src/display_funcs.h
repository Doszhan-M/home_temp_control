extern SSD1306Wire display;

// Все функции рисуют в буфер целиком и выводят его одним display.display(),
// поэтому экран не мерцает. Вызываются только когда что-то изменилось.
//
// Дисплей двухцветный: строки 0..15 жёлтые, 16..63 синие. Шрифт 24 рисуется с y = 11 и y = 35:
// так буквы целиком попадают в синюю зону (раскладка исходной прошивки). В жёлтой зоне —
// только первая строка главного экрана.
const int16_t LINE1_Y = 11;   // первая строка шрифта 24
const int16_t LINE2_Y = 35;   // вторая строка шрифта 24

// Части строки, одну из которых можно выделить миганием (правка даты/времени и ночи)
struct TextPart {
  const char *text;
  int8_t field;    // номер поля для выделения, -1 — разделитель
};

void drawParts(int16_t y, const TextPart *parts, uint8_t n, int8_t selected, bool blinkOn) {
  int16_t total = 0;
  for (uint8_t i = 0; i < n; i++) total += display.getStringWidth(String(parts[i].text));
  int16_t x = (128 - total) / 2;
  for (uint8_t i = 0; i < n; i++) {
    int16_t w = display.getStringWidth(String(parts[i].text));
    bool isSelected = parts[i].field >= 0 && parts[i].field == selected;
    if (!isSelected || blinkOn) display.drawString(x, y, parts[i].text);
    if (isSelected) display.drawHorizontalLine(x, y + 24, w);   // подчёркивание выбранного поля
    x += w;
  }
}

void startMessage() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(40, 20, "Hello!");
  display.display();
  delay(1000);
}

// Подсказка мелким шрифтом внизу синей зоны
void drawHint(const char *text) {
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 51, text);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}

// Главный экран. warning (например "SET TIME!") по очереди с режимом в нижней строке.
void mainDisplay(float tempCur, float setTemp, const char *mode, bool night,
                 const char *valve, bool relayLink, const char *warning, bool showWarning) {
  char buf[24];
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  if (tempCur == tempCur) snprintf(buf, sizeof(buf), "Temp: %.1f°C", tempCur);
  else snprintf(buf, sizeof(buf), "Temp: ERR");
  display.drawString(0, 0, buf);
  snprintf(buf, sizeof(buf), "Valve: %s", relayLink ? valve : "NO LINK");
  display.drawString(0, 16, buf);
  snprintf(buf, sizeof(buf), "Tset: %.1f°C", setTemp);
  display.drawString(0, 32, buf);
  if (warning && showWarning) snprintf(buf, sizeof(buf), "%s", warning);
  else snprintf(buf, sizeof(buf), "Mode: %s%s", mode, night ? " (N)" : "");
  display.drawString(0, 48, buf);
  display.display();
}

// Хранитель экрана: крупная температура и клапан, блок стоит в (x, y) синей зоны.
// Допустимо: y от 11 до 25, x от 0 до 128 - ширина блока (см. saverBlockWidth).
void saverDisplay(int16_t x, int16_t y, float tempCur, const char *status) {
  char buf[12];
  if (tempCur == tempCur) snprintf(buf, sizeof(buf), "%.1f°", tempCur);
  else snprintf(buf, sizeof(buf), "ERR");
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(x, y, buf);
  display.setFont(ArialMT_Plain_10);
  display.drawString(x, y + 26, status);
  display.display();
}

// Ширина блока хранителя экрана, чтобы выбрать, куда его можно сдвинуть
int16_t saverBlockWidth(float tempCur, const char *status) {
  char buf[12];
  if (tempCur == tempCur) snprintf(buf, sizeof(buf), "%.1f°", tempCur);
  else snprintf(buf, sizeof(buf), "ERR");
  display.setFont(ArialMT_Plain_24);
  int16_t w1 = display.getStringWidth(String(buf));
  display.setFont(ArialMT_Plain_10);
  int16_t w2 = display.getStringWidth(String(status));
  return w1 > w2 ? w1 : w2;
}

// Экран одной настройки: заголовок + значение. editing — режим правки.
void valueDisplay(const char *title, const char *value, bool editing) {
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  if (editing) {
    display.drawString(64, 20, value);   // как в исходной прошивке: при правке только значение
    drawHint("turn: change   hold: save");
  } else {
    display.drawString(64, LINE1_Y, title);
    display.drawString(64, LINE2_Y, value);
  }
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.display();
}

// Короткое сообщение крупно, например "Saved" после сохранения настройки
void messageDisplay(const char *text) {
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 23, text);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.display();
}

// Дата и время. field: -1 — просмотр, 0..4 — правка (часы, минуты, день, месяц, год)
void dateTimeDisplay(bool valid, uint8_t hour, uint8_t minute, uint8_t day, uint8_t month,
                     uint16_t year, int8_t field, bool blinkOn) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  if (!valid && field < 0) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, LINE1_Y, "RTC");
    display.drawString(64, LINE2_Y, "error");
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.display();
    return;
  }
  char h[12], m[12], d[12], mo[12], y[12];
  snprintf(h, sizeof(h), "%02u", hour);
  snprintf(m, sizeof(m), "%02u", minute);
  snprintf(d, sizeof(d), "%02u", day);
  snprintf(mo, sizeof(mo), "%02u", month);
  snprintf(y, sizeof(y), "%04u", year);
  const TextPart timeParts[] = {{h, 0}, {":", -1}, {m, 1}};
  const TextPart dateParts[] = {{d, 2}, {".", -1}, {mo, 3}, {".", -1}, {y, 4}};
  // при правке раскладка та же, выбранное поле мигает и подчёркнуто
  drawParts(LINE1_Y, timeParts, 3, field, blinkOn);
  drawParts(LINE2_Y, dateParts, 5, field, blinkOn);
  display.display();
}

// Ночной период. field: -1 — просмотр, 0 — начало, 1 — конец
void nightDisplay(uint8_t start, uint8_t end, int8_t field, bool blinkOn) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, LINE1_Y, "Night:");
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  // только часы: минуты не настраиваются, а "22:00-06:00" занимало всю ширину экрана
  char s[16], e[16];
  snprintf(s, sizeof(s), "%02u", start);
  snprintf(e, sizeof(e), "%02u", end);
  int16_t y = LINE2_Y;
  if (start == end && field < 0) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, y, "off");
    display.setTextAlignment(TEXT_ALIGN_LEFT);
  } else {
    const TextPart parts[] = {{s, 0}, {" - ", -1}, {e, 1}};
    drawParts(y, parts, 3, field, blinkOn);
  }
  display.display();
}

// График температуры: values — в десятых долях градуса, от старых к новым.
// setTemp рисуется пунктиром, если попадает в диапазон графика.
template <uint16_t N>
void graphDisplay(const thermo::History<N> &h, float setTemp, const char *period) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  if (h.size() < 2) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, LINE1_Y, "Graph");
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 42, "collecting data...");
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.display();
    return;
  }
  int16_t lo = h.minValue(), hi = h.maxValue();
  if (hi - lo < 10) {               // минимум 1 °C по вертикали
    int16_t mid = (hi + lo) / 2;
    lo = mid - 5;
    hi = mid + 5;
  }
  const int16_t x0 = 32, yTop = 18, yBottom = 62;   // график только в синей зоне
  const int16_t width = 128 - x0;
  auto yOf = [&](int16_t v) { return yBottom - (int32_t)(v - lo) * (yBottom - yTop) / (hi - lo); };

  char buf[8];
  snprintf(buf, sizeof(buf), "%.1f", hi / 10.0f);
  display.drawString(0, 16, buf);
  snprintf(buf, sizeof(buf), "%.1f", lo / 10.0f);
  display.drawString(0, 51, buf);
  display.drawString(0, 34, period);
  display.drawVerticalLine(x0 - 2, yTop, yBottom - yTop + 1);

  int16_t sp = (int16_t)(setTemp * 10.0f + 0.5f);
  if (sp >= lo && sp <= hi) {
    for (int16_t x = x0; x < 128; x += 3) display.setPixel(x, yOf(sp));
  }

  // засечки внизу: 6, 12 и 18 часов назад (точка раз в 15 мин -> 24 точки на 6 часов)
  for (int16_t k = 1; k <= 3; k++) {
    int16_t x = x0 + width - 1 - k * 24;
    display.drawVerticalLine(x, yBottom - 2, 3);
  }

  // точки распределяются по всей ширине, новые — справа
  uint16_t n = h.size();
  int16_t start = width - n;
  if (start < 0) start = 0;
  for (uint16_t i = 1; i < n; i++) {
    display.drawLine(x0 + start + i - 1, yOf(h.at(i - 1)), x0 + start + i, yOf(h.at(i)));
  }
  display.display();
}
