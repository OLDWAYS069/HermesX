#include <Wire.h>
#include <INA3221.h>

#define SERIAL_SPEED     9600  // serial baud rate
#define PRINT_DEC_POINTS 3       // decimal points to print

 int AddressArray[4] = {0,0,0,0};
  
  // These are the possible I2C addresses for the INA3221

  // INA3221_ADDR40_GND = 0b1000000,  // A0 pin -> GND
  // INA3221_ADDR41_VCC = 0b1000001,  // A0 pin -> VCC
  // INA3221_ADDR42_SDA = 0b1000010,  // A0 pin -> SDA - this is the default address for Meshtastic
  // INA3221_ADDR43_SCL = 0b1000011   // A0 pin -> SCL


INA3221 ina_0(INA3221_ADDR40_GND);
INA3221 ina_1(INA3221_ADDR41_VCC);
INA3221 ina_2(INA3221_ADDR42_SDA);
INA3221 ina_3(INA3221_ADDR43_SCL);

// Set I2C address for each of the possible inputs

void current_measure_init() {
    ina_0.begin(&Wire);
    ina_0.reset();
    ina_0.setShuntRes(100, 100, 100);

    ina_1.begin(&Wire);
    ina_1.reset();
    ina_1.setShuntRes(100, 100, 100);

    ina_2.begin(&Wire);
    ina_2.reset();
    ina_2.setShuntRes(100, 100, 100);

    ina_3.begin(&Wire);
    ina_3.reset();
    ina_3.setShuntRes(100, 100, 100);

    // Set shunt resistors to 100 mOhm for all channels
}

void setup() {


    Serial.begin(SERIAL_SPEED);
    current_measure_init();



    while (!Serial) {
        delay(1);
    }

// Run a check for the manufacturer ID of each address, and set that entry in the address array if it is correct.

  Serial.println("Checking INA3221_ADDR40_GND");
  Serial.print("ManufID is ");
  Serial.print(ina_0.getManufID());
if(ina_0.getManufID()==0x5449){
  Serial.println(" = Correct");
  AddressArray[0] = 1;
}else{
  Serial.println(" = error, should be 0x5449 / 21577");
}

  Serial.println("Checking NA3221_ADDR41_VCC");
  Serial.print("ManufID is ");
  Serial.print(ina_1.getManufID());
if(ina_1.getManufID()==0x5449){
  Serial.println(" = Correct");
    AddressArray[1] = 1;
}else{
  Serial.println("incorrect, should be 0x5449 / 21577");
}

  Serial.println("Checking INA3221_ADDR42_SDA");
  Serial.print("ManufID is ");
if(ina_2.getManufID()==0x5449){
  Serial.print(ina_2.getManufID());
  Serial.println(" = Correct");
  AddressArray[2] = 1;
}else{
  Serial.println("incorrect, should be 0x5449 / 21577");
}

  Serial.println("Checking INA3221_ADDR43_SCL");
  Serial.print("ManufID is ");
if(ina_3.getManufID()==0x5449){
  Serial.print(ina_3.getManufID());
  Serial.println(" = Correct");
    AddressArray[3] = 1;
}else{
  Serial.println("incorrect, should be 0x5449 / 21577");
}

}

void loop() {

// measure the current and voltage of each channel of each connected INA board and print it over serial

if(AddressArray[0] == 1){
  Serial.print(" Channel 1a: ");
  Serial.print(ina_0.getCurrent(INA3221_CH1), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_0.getVoltage(INA3221_CH1), PRINT_DEC_POINTS);
  Serial.print(" V ");

  Serial.print(" Channel 2a: ");
  Serial.print(ina_0.getCurrent(INA3221_CH2), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_0.getVoltage(INA3221_CH2), PRINT_DEC_POINTS);
  Serial.print(" V ");

  Serial.print(" Channel 3a: ");
  Serial.print(ina_0.getCurrent(INA3221_CH3), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_0.getVoltage(INA3221_CH3), PRINT_DEC_POINTS);
  Serial.print(" V ");

  Serial.println("");

}

if(AddressArray[1] == 1){
  Serial.print(" Channel 1b: ");
  Serial.print(ina_1.getCurrent(INA3221_CH1), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_1.getVoltage(INA3221_CH1), PRINT_DEC_POINTS);
  Serial.println(" V ");

  Serial.print(" Channel 2b: ");
  Serial.print(ina_1.getCurrent(INA3221_CH2), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_1.getVoltage(INA3221_CH2), PRINT_DEC_POINTS);
  Serial.println(" V ");

  Serial.print(" Channel 3b: ");
  Serial.print(ina_1.getCurrent(INA3221_CH3), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_1.getVoltage(INA3221_CH3), PRINT_DEC_POINTS);
  Serial.println(" V ");

  Serial.println("");

}

if(AddressArray[2] == 1){
  Serial.print(" Channel 1c: ");
  Serial.print(ina_2.getCurrent(INA3221_CH1), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_2.getVoltage(INA3221_CH1), PRINT_DEC_POINTS);
  Serial.print(" V ");

  Serial.print(" Channel 2c: ");
  Serial.print(ina_2.getCurrent(INA3221_CH2), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_2.getVoltage(INA3221_CH2), PRINT_DEC_POINTS);
  Serial.print(" V ");

  Serial.print(" Channel 3c: ");
  Serial.print(ina_2.getCurrent(INA3221_CH3), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_2.getVoltage(INA3221_CH3), PRINT_DEC_POINTS);
  Serial.print(" V ");

  Serial.println("");

}

if(AddressArray[3] == 1){
  Serial.print(" Channel 1d: ");
  Serial.print(ina_3.getCurrent(INA3221_CH1), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_3.getVoltage(INA3221_CH1), PRINT_DEC_POINTS);
  Serial.println(" V ");

  Serial.print(" Channel 2d: ");
  Serial.print(ina_3.getCurrent(INA3221_CH2), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_3.getVoltage(INA3221_CH2), PRINT_DEC_POINTS);
  Serial.println(" V ");

  Serial.print(" Channel 3d: ");
  Serial.print(ina_3.getCurrent(INA3221_CH3), PRINT_DEC_POINTS);
  Serial.print(" mA, ");
  Serial.print(ina_3.getVoltage(INA3221_CH3), PRINT_DEC_POINTS);
  Serial.println(" V ");

  Serial.println("");

}



  delay(500);

}