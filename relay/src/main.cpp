#include <Arduino.h>
#include <RCSwitch.h>


// ----------------------- ПИНЫ --------------------------------------------------------------------
#define RADIO_VCC 9                                         // питание радио модуля
#define RADIO_GND 12                                        // GND радио модуля
#define RADIO_DATA 10                                       // data пин радио модуля


// Классы ------------------------------------------------------------------------------------------
RCSwitch radioModule = RCSwitch();


void setup() {
    Serial.begin(9600);

    pinMode(RADIO_VCC, OUTPUT);           
    digitalWrite(RADIO_VCC, HIGH);       
    pinMode(RADIO_GND, OUTPUT);         
    digitalWrite(RADIO_GND, LOW);

    radioModule.enableReceive(RADIO_DATA);
}


void loop() {
    Serial.println("Check Arduino Receive");
    Serial.println(radioModule.available());

    if( radioModule.available() ){
        int value = radioModule.getReceivedValue();
 
        if( value == 1 )
            Serial.println("Receive 1");
        else if( value == 2 )
            Serial.println("Receive 2");

        radioModule.resetAvailable();
    }
    delay(1000); 
}
