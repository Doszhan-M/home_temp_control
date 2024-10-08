# Home Temperature Control
<img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/qYME/RhuFbhk33" alt="Wiring diagram" width="400" height="350"/>

## Thermal management application on esp8266 NodeMCU V3 module

### On the ESP8266 module, raised the web server, which can process requests from the browser. Adaptation of the html page is made only for mobile devices. To access the page, you need to connect the ESP8266 to your home router and set a static ip address in the router settings. The module also raises its access point, you can go through it. The relay from Sonoff must connect to the access point from ESP8266. The main functionality of the device is to maintain the temperature in a given interval. You can also control the 3-position button on the device itself: open - auto - close. 


Used Components: 
1. ESP8266
2. DHT22
3. DS1307
4. Sonoff Basic R3 (DIY Mode) [manual](https://sonoff.tech/uncategorized/basiczbr3/) , [Sonoff_Devices_DIY_Tools](https://github.com/itead/Sonoff_Devices_DIY_Tools)
5. Button 3-position (hw-040)
6. Electric thermal actuators for heating radiators [bought here](https://aliexpress.ru/item/1005003695755495.html?spm=a2g2w.orderdetail.0.0.61994aa6l1Ep1u&sku_id=12000026837565021)


## Wiring diagram:
<img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/Sbi3/brSmG86gL" alt="Wiring diagram" width="350" height="330"/> <img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/uJVa/tJB8tkTuR" alt="Wiring diagram" width="350" height="330"/>

## Completed:
<img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/b8zU/J4Dqv3BDe" alt="Wiring diagram" height="150"/> <img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/fNHe/wgmHKzXUH" alt="Wiring diagram" height="150"/> <img src="https://thumb.cloud.mail.ru/weblink/thumb/xw1/7mxp/EgxGtMNyh" alt="Wiring diagram" height="150"/>
