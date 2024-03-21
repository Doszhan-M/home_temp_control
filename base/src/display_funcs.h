extern SSD1306Wire display;


// Functions ------------------------------------------------------------------------------------------
void clearDisplay() {
  display.clear();
  display.display();
}

void startMessage() {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(40, 20, "Hello!");
  display.display();
  delay(1000);
  clearDisplay();
}

void mainDisplay(float tempCur, float setTemp, String mode, String valveState, bool clearDisp) {
  if (clearDisp == true) {
    clearDisplay();
  }  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Temp: " + String(tempCur) + "°C");
  display.drawString(0, 16, "Valve: " + valveState);
  display.drawString(0, 32, "Tset: " + String(setTemp) + "°C");
  display.drawString(0, 48, "Mode: " + mode);
  display.display();
}

void TsetDisplay(float tempSet, bool clearDisp) {
  if (clearDisp == true) {
    clearDisplay();
  }  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(30, 11, "Tset:");
  display.drawString(30, 35, String(tempSet));
  display.display();
}

String changeTsetDisplay(float temp) {
  clearDisplay();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(30, 20, String(temp));
  display.display();
  return "changeTset";
}

void modeDisplay(String mode, bool clearDisp) {
  if (clearDisp == true) {
    clearDisplay();
  }  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(25, 11, "Mode:");
  display.drawString(25, 35, mode);
  display.display();
}

String changeModeDisplay(String mode) {
  clearDisplay();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(30, 20, mode);
  display.display();
  return "changeMode";
}

void deltaSetDisplay(float nightTempDelta, bool clearDisp) {
  if (clearDisp == true) {
    clearDisplay();
  }  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(8, 11, "Night delta:");
  display.drawString(40, 35, String(nightTempDelta));
  display.display();
}

String deltaChangeDisplay(float temp) {
  clearDisplay();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(40, 20, String(temp));
  display.display();
  return "changeDelta";
}

void dateTimetDisplay(String cur_time, String cur_date,  bool clearDisp) {
  if (clearDisp == true) {
    clearDisplay();
  }  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(40, 11, cur_time);
  display.drawString(9, 35, cur_date);
  display.display();
}


void drawFontFaceDemo() {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Hello world");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 10, "Hello world");
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 26, "Hello world");
  display.display();
}
