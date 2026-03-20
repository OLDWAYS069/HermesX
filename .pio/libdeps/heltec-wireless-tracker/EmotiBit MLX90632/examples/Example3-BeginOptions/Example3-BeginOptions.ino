/*
  Using the MLX90632 FIR Sensor
  By: Nathan Seidle
  SparkFun Electronics
  Date: December 21st, 2017
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14569

  This example shows how to init the sensor with a different I2C address,
  on a different I2C port than Wire, and expose the errorFlag.
  Many platforms have multiple I2C buses. This library can use them.

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

  //Arduino Uno does not have Wire1, this only works on other platforms like SparkFun SAMD21
  Wire1.begin();

  byte sensorAddress = 0x3B; //The default I2C address for the SparkX breakout board is 0x3B.
  //But if you close the I2C ADR jumper it changes the device address to 0x3A.
  //This allows you to have up to two sensors on one I2C bus.
  
  MLX90632::status errorFlag; //Declare a variable called errorFlag that is of type 'status'
  
  //Now begin communication with all these settings
  myTempSensor.begin(sensorAddress, Wire1, errorFlag); //Useful on SAMD21 and other platforms

  //The errorFlag is set to one of a handful of different errors
  if(errorFlag == MLX90632::SENSOR_SUCCESS)
  {
    Serial.println("MLX90632 online!");
  }
  else
  {
    //Something went wrong
    if(errorFlag == MLX90632::SENSOR_ID_ERROR) Serial.println("Sensor ID did not match the sensor address. Probably a wiring error.");
    else if(errorFlag == MLX90632::SENSOR_I2C_ERROR) Serial.println("Sensor did not respond to I2C properly. Check wiring.");
    else if(errorFlag == MLX90632::SENSOR_TIMEOUT_ERROR) Serial.println("Sensor failed to respond.");
    else Serial.println("Other Error");
  }
}

void loop()
{
  float objectTemp = myTempSensor.getObjectTempF(); //Get the temperature of the object we're looking at
  Serial.print("Object temperature: ");
  Serial.print(objectTemp, 2);
  Serial.print(" F");

  Serial.println();
}

