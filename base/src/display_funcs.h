extern SSD1306Wire display;

// Все функции рисуют в буфер целиком и выводят его одним display.display(),
// поэтому экран не мерцает. Вызываются только когда что-то изменилось.
//
// Дисплей двухцветный: строки 0..15 жёлтые, 16..63 синие.
// В жёлтой полосе — первая строка главного экрана или мелкий заголовок экрана (шрифт 10, y = 1).
// Крупный текст (шрифт 24) — только в синей зоне: y = 11 / 35 для двух строк, y = 23 для одной.
// Подсказка — шрифт 10 внизу синей зоны (y = 51).
const int16_t LINE1_Y = 11;   // первая строка шрифта 24
const int16_t LINE2_Y = 35;   // вторая строка шрифта 24
const int16_t VALUE_Y = 23;   // одна строка шрифта 24 посередине синей зоны

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

// Заголовок экрана мелким шрифтом в жёлтой полосе
void drawHeader(const char *text) {
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 1, text);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
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

// Хранитель экрана: крупная температура, под ней строка состояния, блок стоит в (x, y) синей зоны.
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

// Экран одной настройки: заголовок в жёлтой полосе, значение крупно, подсказка внизу
void valueDisplay(const char *header, const char *value, bool editing) {
  display.clear();
  drawHeader(header);
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, VALUE_Y, value);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  drawHint(editing ? "turn: change   hold: save" : "click: edit");
  display.display();
}

// Короткое сообщение крупно, например "Saved" после сохранения настройки
void messageDisplay(const char *text) {
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, VALUE_Y, text);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.display();
}

// Дата и время. field: -1 — просмотр, 0..4 — правка (часы, минуты, день, месяц, год)
void dateTimeDisplay(const char *header, bool valid, uint8_t hour, uint8_t minute, uint8_t day,
                     uint8_t month, uint16_t year, int8_t field, bool blinkOn) {
  display.clear();
  drawHeader(header);
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
  // при правке раскладка та же, выбранное поле мигает и подчёркнуто, его название — в заголовке
  drawParts(LINE1_Y, timeParts, 3, field, blinkOn);
  drawParts(LINE2_Y, dateParts, 5, field, blinkOn);
  display.display();
}

// Ночной период. field: -1 — просмотр, 0 — начало, 1 — конец
void nightDisplay(const char *header, uint8_t start, uint8_t end, int8_t field, bool blinkOn) {
  display.clear();
  drawHeader(header);
  display.setFont(ArialMT_Plain_24);
  // только часы: минуты не настраиваются, а "22:00-06:00" занимало всю ширину экрана
  char s[16], e[16];
  snprintf(s, sizeof(s), "%02u", start);
  snprintf(e, sizeof(e), "%02u", end);
  if (start == end && field < 0) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, VALUE_Y, "off");
    display.setTextAlignment(TEXT_ALIGN_LEFT);
  } else {
    const TextPart parts[] = {{s, 0}, {" - ", -1}, {e, 1}};
    drawParts(VALUE_Y, parts, 3, field, blinkOn);
  }
  if (field < 0) drawHint("click: edit");
  else drawHint(start == end ? "start = end: night off" : "click: next   hold: save");
  display.display();
}

// График температуры за 24 ч.
// temps — в десятых долях градуса, open — доля времени с открытым клапаном, % (оба от старых к новым).
// Внизу полоса: клапан был открыт больше половины интервала. Уставка — пунктиром.
template <uint16_t N>
void graphDisplay(const char *header, const thermo::History<N> &temps, const thermo::History<N> &open,
                  float setTemp) {
  display.clear();
  drawHeader(header);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  if (temps.size() < 2) {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 30, "collecting data...");
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.display();
    return;
  }
  int16_t lo = temps.minValue(), hi = temps.maxValue();
  if (hi - lo < 10) {               // минимум 1 °C по вертикали
    int16_t mid = (hi + lo) / 2;
    lo = mid - 5;
    hi = mid + 5;
  }
  const int16_t x0 = 32, yTop = 18, yBottom = 58;   // кривая только в синей зоне
  const int16_t yBar = 61;                          // полоса «клапан открыт»: строки 61..63
  const int16_t width = 128 - x0;
  auto yOf = [&](int16_t v) { return yBottom - (int32_t)(v - lo) * (yBottom - yTop) / (hi - lo); };

  char buf[8];
  snprintf(buf, sizeof(buf), "%.1f", hi / 10.0f);
  display.drawString(0, 16, buf);
  snprintf(buf, sizeof(buf), "%.1f", lo / 10.0f);
  display.drawString(0, 47, buf);
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
  uint16_t n = temps.size();
  int16_t start = width - n;
  if (start < 0) start = 0;
  for (uint16_t i = 1; i < n; i++) {
    display.drawLine(x0 + start + i - 1, yOf(temps.at(i - 1)), x0 + start + i, yOf(temps.at(i)));
  }
  for (uint16_t i = 0; i < open.size() && i < n; i++) {
    if (open.at(i) >= 50) display.drawVerticalLine(x0 + start + i, yBar, 3);
  }
  display.display();
}
