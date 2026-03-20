/*
  Using the MLX90632 FIR Sensor
  By: Nathan Seidle
  SparkFun Electronics
  Date: December 21st, 2017
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14569

  This example shows how to write to an EEPROM register. This is handy if you need
  to change the REG_I2C_ADDRESS register to make the sensor be a totally different I2C address.

  Hardware Connections:
  Attach the Qwiic Shield to your Arduino/Photon/ESP32 or other
  Plug the sensor onto the shield
  Serial.print it out at 9600 baud to serial monitor.
*/

#include <Wire.h>

#include "SparkFun_MLX90632_Arduino_Library.h"
MLX90632 myTempSensor;

void setup()
{
  Serial.begin(9600);
  Serial.println("MLX90632 Read Example");

  Wire.begin();

  myTempSensor.enableDebugging(Serial);
  myTempSensor.begin();

  //readRegister16 returns a status value not the value found at the memory location
  //We have to pass in a container for readRegister to store the data into
  uint16_t valueInMemory; //Create a container
  myTempSensor.readRegister16(EE_Hb, valueInMemory);
  Serial.print("Value stored in EE_Hb: 0x");
  Serial.println(valueInMemory, HEX);

  //Write a new dummy value to EE_Hb register.
  myTempSensor.writeEEPROM(EE_Hb, 0x02AB);

  myTempSensor.readRegister16(EE_Hb, valueInMemory);
  Serial.print("New value stored in EE_Hb (should be 0x02AB): 0x");
  Serial.println(valueInMemory, HEX);

  Serial.println("Done");
}

void loop()
{

}

