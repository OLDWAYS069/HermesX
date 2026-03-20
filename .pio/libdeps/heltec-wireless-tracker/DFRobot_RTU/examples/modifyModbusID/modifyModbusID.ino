/*!
 * @file modifyModbusID.ino
 * @brief 修改modbus从机的设备ID。每个modbus从机都有唯一识别的设备ID号，范围0x0000~0x00F7(0~247),修改设备ID有2种方式：
 * @n 1: 不知道设备的ID地址，可以通过广播地址0x00修改从机的ID地址，此命令会将总线上所有的从机的地址都修改为设置的ID（用0x00修改地址时，总线上最好只接一个设备）
 * @n 2: 知道设备的ID地址，直接用ID修改
 * @n note：运行此demo必须知道设备的串口配置(波特率，数据位，校验位，停止位)
 * @n connected table
 * ---------------------------------------------------------------------------------------------------------------
 * sensor pin |             MCU                | Leonardo/Mega2560/M0 |    UNO    | ESP8266 | ESP32 |  microbit  |
 *     VCC    |            3.3V/5V             |        VCC           |    VCC    |   VCC   |  VCC  |     X      |
 *     GND    |              GND               |        GND           |    GND    |   GND   |  GND  |     X      |
 *     RX     |              TX                |     Serial1 RX1      |     5     |5/D6(TX) |  D2   |     X      |
 *     TX     |              RX                |     Serial1 TX1      |     4     |4/D7(RX) |  D3   |     X      |
 * ---------------------------------------------------------------------------------------------------------------
 *
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
  
  //方法1：通过广播地址0x00将modbus从机的地址设置为0x10
  modbus.writeHoldingRegister(/*id =*/0x00, /*reg =*/DEVICE_ID_REG, /*val =*/0x10);
  delay(1000);
  uint16_t ret = modbus.readHoldingRegister(0x10, DEVICE_ID_REG);
  if(ret == 0x10){
      Serial.print("new ID1 is 0x");
      Serial.println(ret, HEX);
      //方法2：已知从机的地址为0x10，将其修改为0x20
      modbus.writeHoldingRegister(/*id =*/0x00, /*reg =*/DEVICE_ID_REG, /*val =*/0x20);
      delay(1000);
      ret = modbus.readHoldingRegister(0x20, DEVICE_ID_REG);
      Serial.print("new ID2 is 0x");
      Serial.println(ret, HEX);
  }

}

void loop() {
  
}


