#include <Wire.h>
#include "wiring_private.h"
#include "SparkFun_MLX90632_Arduino_Library.h"
#define MOSFET 6
MLX90632 myTempSensor;

TwoWire myWire(&sercom1, 11, 13);
	
void setup(){
	Serial.begin(9600);
	Serial.println("MLX90632 Read Example");
	// Enable MOSFET
	pinMode(MOSFET, OUTPUT);
	digitalWrite(MOSFET, LOW);
	myWire.begin();
	pinPeripheral(11, PIO_SERCOM);
	pinPeripheral(13, PIO_SERCOM);
	myWire.setClock(100000);
	while (!Serial.available());

	MLX90632::status tempStatus;
	myTempSensor.begin(0x3A, myWire, tempStatus);
	myTempSensor.enableDebugging(Serial);

	uint16_t read_meas1, read_meas2;
	myTempSensor.readRegister16(EE_MEAS1, read_meas1);
	myTempSensor.readRegister16(EE_MEAS2, read_meas2);
	Serial.println("Before writing");
	Serial.print("EE_MEAS1:");
	Serial.println(read_meas1, HEX);
	Serial.print("EE_MEAS2:");
	Serial.println(read_meas2, HEX);
	myTempSensor.setMeasurementRate(8);
}

void loop(){

}
