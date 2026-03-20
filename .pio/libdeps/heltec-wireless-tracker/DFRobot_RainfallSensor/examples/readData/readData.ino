/*!
 * @file  readData.ino
 * @brief  This example describes the method of using this module to test the collected rainfall within one hour.
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license    The MIT License (MIT)
 * @author     [fary](feng.yang@dfrobot.com)
 * @version    V1.0
 * @date       2023-02-28
 * @url        https://github.com/DFRobot/DFRobot_RainfallSensor
 */
#include "DFRobot_RainfallSensor.h"

//#define MODE_UART
#ifdef MODE_UART //Serial communication
 #if defined(ARDUINO_AVR_UNO)||defined(ESP8266)
   #include "SoftwareSerial.h"
   SoftwareSerial mySerial(/*rx =*/4, /*tx =*/5);
   DFRobot_RainfallSensor_UART Sensor(/*Stream *=*/&mySerial);
 #else
   DFRobot_RainfallSensor_UART Sensor(/*Stream *=*/&Serial1);
 #endif
#else //I2C communication
  DFRobot_RainfallSensor_I2C Sensor(&Wire);
#endif

void setup(void)
{
#ifdef MODE_UART
  #if defined(ARDUINO_AVR_UNO)||defined(ESP8266)
    mySerial.begin(9600);
  #elif defined(ESP32)
    Serial1.begin(9600, SERIAL_8N1, /*rx =*/D3, /*tx =*/D2);
  #else
    Serial1.begin(9600);
  #endif
#endif 
  Serial.begin(9600);
  
  delay(1000);
  while(!Sensor.begin()){
    Serial.println("Sensor init err!!!");
    delay(1000);
  }
  Serial.print("vid:\t");
  Serial.println(Sensor.vid,HEX);
  Serial.print("pid:\t");
  Serial.println(Sensor.pid,HEX);
  Serial.print("Version:\t");
  Serial.println(Sensor.getFirmwareVersion());
  //Set the cumulative rainfall value in units of mm.
  //Sensor.setRainAccumulatedValue(0.2794);
}

void loop()
{
  //Get the sensor operating time in units of hours.
  Serial.print("Sensor WorkingTime:\t");
  Serial.print(Sensor.getSensorWorkingTime());
  Serial.println(" H");
  //Get the cumulative rainfall during the sensor operating time.
  Serial.print("Rainfall:\t");
  Serial.println(Sensor.getRainfall());
  //Here is an example function that calculates the cumulative rainfall in a specified hour of the system. The function takes an optional argument, which can be any value between 1 and 24.
  Serial.print("1 Hour Rainfall:\t");
  Serial.print(Sensor.getRainfall(1));
  Serial.println(" mm");
  //Get the raw data, which is the tipping bucket count of rainfall, in units of counts.
  Serial.print("rainfall raw:\t");
  Serial.println(Sensor.getRawData());
  delay(1000);
}
