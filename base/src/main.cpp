#include <Arduino.h>
#include <RCSwitch.h>


// ----------------------- ПИНЫ --------------------------------------------------------------------
#define RADIO_VCC 10                                        // питание радио модуля
#define RADIO_GND 11                                        // GND радио модуля
#define RADIO_DATA 12                                       // data пин радио модуля


// Классы ------------------------------------------------------------------------------------------
RCSwitch radioModule = RCSwitch();


void setup() {
    Serial.begin(9600);

    pinMode(RADIO_VCC, OUTPUT);           
    digitalWrite(RADIO_VCC, HIGH);       
    pinMode(RADIO_GND, OUTPUT);         
    digitalWrite(RADIO_GND, LOW);
    radioModule.enableTransmit(RADIO_DATA);
}


void loop() {
    Serial.println("Check Arduino");
    radioModule.send(1, 4);
    delay(3000); 
    radioModule.send(2, 4);
    delay(3000); 
}
