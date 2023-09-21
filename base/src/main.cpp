#include <Arduino.h>
#include <RCSwitch.h>


// ----------------------- ПИНЫ --------------------------------------------------------------------
#define RADIO_VCC 2                                        // питание радио модуля
// #define RADIO_GND 11                                        // GND радио модуля
#define RADIO_DATA 3                                       // data пин радио модуля


// Классы ------------------------------------------------------------------------------------------
RCSwitch radioModule = RCSwitch();


void setup() {
    Serial.begin(9600);

    pinMode(RADIO_VCC, OUTPUT);           
    digitalWrite(RADIO_VCC, HIGH);       
    // pinMode(RADIO_GND, OUTPUT);         
    // digitalWrite(RADIO_GND, LOW);
    radioModule.enableTransmit(RADIO_DATA);
}


void loop() {
    Serial.println("Check Base");
    radioModule.send(B0100, 4);
    delay(2000); 
    radioModule.send(B1000, 4);
    delay(2000); 
    
  radioModule.switchOn("11111", "00010");
  delay(1000);
  radioModule.switchOff("11111", "00010");
  delay(1000);

  /* Same switch as above, but using decimal code */
  radioModule.send(5393, 24);
  delay(1000);  
  radioModule.send(5396, 24);
  delay(1000);  

  /* Same switch as above, but using binary code */
  radioModule.send("000000000001010100010001");
  delay(1000);  
  radioModule.send("000000000001010100010100");
  delay(1000);

  /* Same switch as above, but tri-state code */ 
  radioModule.sendTriState("00000FFF0F0F");
  delay(1000);  
  radioModule.sendTriState("00000FFF0FF0");
  delay(1000);

  delay(20000);
}
