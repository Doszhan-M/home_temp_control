#include <Arduino.h>
#include <RCSwitch.h>


// ----------------------- ПИНЫ --------------------------------------------------------------------
#define RADIO_VCC 12                                         // питание радио модуля
#define RADIO_GND 9                                        // GND радио модуля
#define RADIO_DATA 11                                       // data пин радио модуля


// Классы ------------------------------------------------------------------------------------------
RCSwitch radioModule = RCSwitch();


void setup() {
    Serial.begin(9600);

    pinMode(RADIO_VCC, OUTPUT);           
    digitalWrite(RADIO_VCC, HIGH);       
    pinMode(RADIO_GND, OUTPUT);         
    digitalWrite(RADIO_GND, LOW);

    radioModule.enableReceive(11);
}


void loop() {
    Serial.println("Check Arduino Receive");
    Serial.println(radioModule.available());
    Serial.println(radioModule.getReceivedValue());

    if( radioModule.available() ){
        int value = radioModule.getReceivedValue();
 
        if( value == B1000 )
            Serial.println("Receive 1");
        else if( value == B0100 )
            Serial.println("Receive 2");

        radioModule.resetAvailable();
    }
    // delay(1000); 
}
