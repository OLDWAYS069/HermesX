/*!
 * @file getData.ino
 * @brief This is a routine to get skylark data
 * ---------------------------------------------------------------------------------------------------------------
 *    board   |             MCU                | Leonardo/Mega2560/M0 |    UNO    | ESP8266 | ESP32 |  microbit  |
 *     VCC    |            3.3V/5V             |        VCC           |    VCC    |   VCC   |  VCC  |     X      |
 *     GND    |              GND               |        GND           |    GND    |   GND   |  GND  |     X      |
 *     RX     |              TX                |     Serial1 TX1      |     5     |   5/D6  |  D2   |     X      |
 *     TX     |              RX                |     Serial1 RX1      |     4     |   4/D7  |  D3   |     X      |
 * ---------------------------------------------------------------------------------------------------------------
 * 
 * @copyright    Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license      The MIT License (MIT)
 * @author       [TangJie](jie.tang@dfrobot.com)
 * @version      V1.0.0
 * @date         2023-06-8
 * @url         https://github.com/DFRobot/DFRobot_LarkWeatherStation
 */
#include "DFRobot_LarkWeatherStation.h"
#if defined(ARDUINO_AVR_UNO)||defined(ESP8266)
#include <SoftwareSerial.h>
#endif
#define DEVICE_ADDR                  0x42

#define MODESWITCH        /*UART:*/1 /*I2C: 0*/

#if MODESWITCH
#if defined(ARDUINO_AVR_UNO)||defined(ESP8266)
  SoftwareSerial mySerial(/*rx =*/4, /*tx =*/5);
  DFRobot_LarkWeatherStation_UART atm(&mySerial);
#else
  DFRobot_LarkWeatherStation_UART atm(&Serial1);
#endif
#else
DFRobot_LarkWeatherStation_I2C atm(DEVICE_ADDR,&Wire);
#endif

void setup(void){
  #if MODESWITCH
  //Init MCU communication serial port
  #if defined(ARDUINO_AVR_UNO)||defined(ESP8266)
    mySerial.begin(115200);
  #elif defined(ESP32)
    Serial1.begin(115200, SERIAL_8N1, /*rx =*/D3, /*tx =*/D2);
  #else
    Serial1.begin(115200);
  #endif
  #endif
  Serial.begin(115200);
  while(atm.begin()!= 0){
    Serial.println("init error");
    delay(1000);
  }
  Serial.println("init success");
  atm.setTime(2023,1,11,23,59,0);
}

void loop(void){
    Serial.println("----------------------------");
    Serial.print(atm.getValue("Temp"));
    Serial.println(atm.getUnit("Temp"));
    Serial.print(atm.getValue("Humi"));
    Serial.println(atm.getUnit("Humi"));
    Serial.print(atm.getValue("Speed"));
    Serial.println(atm.getUnit("Speed"));
    Serial.println(atm.getValue("Dir"));
    Serial.print(atm.getValue("Altitude"));
    Serial.println(atm.getUnit("Altitude"));
    Serial.print(atm.getValue("Pressure"));
    Serial.println(atm.getUnit("Pressure"));
    Serial.println("----------------------------");
    Serial.println(atm.getInformation(true));
    delay(100);
}