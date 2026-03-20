/*!
 * @file scanModbusID.ino
 * @brief 扫描modbus总线上，所有串口配置为9600波特率，8位数据位，无校验位，1位停止位的modbus从机的地址。
 * @n modbus从机设备地址范围为1~247(0x01~0xF7),0为广播地址，所有modbus从机接受到广播包
 * @n 都会处理，但不会响应。
 * @n 一个modbus主机可以连多个modbus从机，在运行此demo之前，必须知道modbus从机的波特率，数据位，校
 * @n 验位，停止位等串口配置。
 * @n connected table
 * ---------------------------------------------------------------------------------------------------------------
 * sensor pin |             MCU                | Leonardo/Mega2560/M0 |    UNO    | ESP8266 | ESP32 |  microbit  |
 *     VCC    |            3.3V/5V             |        VCC           |    VCC    |   VCC   |  VCC  |     X      |
 *     GND    |              GND               |        GND           |    GND    |   GND   |  GND  |     X      |
 *     RX     |              TX                |     Serial1 RX1      |     5     |5/D6(TX) |  D2   |     X      |
 *     TX     |              RX                |     Serial1 TX1      |     4     |4/D7(RX) |  D3   |     X      |
 * ---------------------------------------------------------------------------------------------------------------
 * @note: 不支持UNO，Microbit，ESP8266
 *
 * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @licence     The MIT License (MIT)
 * @author [Arya](xue.peng@dfrobot.com)
 * @version  V1.0
 * @date  2021-07-16
 * @https://github.com/DFRobot/DFRobot_RTU
 */
#include "DFRobot_RTU.h"
#if defined(ARDUINO_AVR_UNO)||defined(ESP8266)
#include <SoftwareSerial.h>
#endif

#define DEVICE_ID_REG   0x02

#if defined(ARDUINO_AVR_UNO)||defined(ESP8266)
  SoftwareSerial mySerial(/*rx =*/4, /*tx =*/5);
  DFRobot_RTU modbus(/*s =*/&mySerial);
#else
  DFRobot_RTU modbus(/*s =*/&Serial1);
#endif

void setup() {
  Serial.begin(115200);
  while(!Serial){                                                     //Waiting for USB Serial COM port to open.
  }

#if defined(ARDUINO_AVR_UNO)||defined(ESP8266)
    mySerial.begin(9600);
#elif defined(ESP32)
  Serial1.begin(9600, SERIAL_8N1, /*rx =*/D3, /*tx =*/D2);
#else
  Serial1.begin(9600);
#endif
  delay(1000);
}

void loop() {
  uint8_t modbusID;
  uint16_t ret = 0;
  int nDevices;
  Serial.println("Scanning...");
  nDevices = 0;
  for(modbusID = 1; modbusID < 248; modbusID++){
      ret = modbus.readHoldingRegister(modbusID, DEVICE_ID_REG);
      if(ret == modbusID){
          Serial.print("modbus device found at address 0x");
          if(modbusID < 16){ 
              Serial.print("0");
          }
          Serial.print(modbusID,HEX);
          Serial.println("  !");
          nDevices++;
      }
  }
  if(nDevices == 0)
    Serial.println("No modbus devices found\n");
  else
    Serial.println("done\n");
  delay(1000);           // wait 1 seconds for next scan
}


