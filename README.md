# Home Temperature Control
<img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/qYME/RhuFbhk33" alt="Wiring diagram" width="400" height="350"/>

## Thermal management application on esp8266 NodeMCU V3 module

The controller keeps the room temperature within a set interval by opening and closing the thermal actuators on the radiators. At night (00:00–08:00) the setpoint is raised by a configurable delta.

All control is local, on the device itself, with a rotary encoder and an OLED display: setpoint, mode (auto / manual open / manual close), night delta, current date and time. The controller runs its own Wi-Fi access point, used only to send open/close commands to the relay module.

Earlier versions also connected to the home Wi-Fi router and served a mobile web page. In practice this was unnecessary for this project, so both were removed: local control on the device is enough. Firmware updates are done over USB.


Used Components: 
1. ESP8266 NodeMCU V3 (controller, firmware in `base/`)
2. DHT22 (temperature sensor)
3. DS1307 RTC (Tiny RTC module; it has a charging circuit, so use an LIR2032 battery, not CR2032)
4. Relay module: Wemos D1 mini (ESP8266) + relay, custom firmware in `relay/`
5. Rotary encoder with button (HW-040)
6. SSD1306 OLED display (I2C)
7. Electric thermal actuators for heating radiators [bought here](https://aliexpress.ru/item/1005003695755495.html?spm=a2g2w.orderdetail.0.0.61994aa6l1Ep1u&sku_id=12000026837565021)


## Wiring diagram:
<img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/Sbi3/brSmG86gL" alt="Wiring diagram" width="350" height="330"/> <img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/uJVa/tJB8tkTuR" alt="Wiring diagram" width="350" height="330"/>

## Completed:
<img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/b8zU/J4Dqv3BDe" alt="Wiring diagram" height="150"/> <img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/fNHe/wgmHKzXUH" alt="Wiring diagram" height="150"/> <img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/7mxp/EgxGtMNyh" alt="Wiring diagram" height="150"/>
