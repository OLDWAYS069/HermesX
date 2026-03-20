/*
  This is a library written for the MLX90632 Non-contact thermal sensor
  SparkFun sells these at its website: www.sparkfun.com
  Do you like this library? Help support SparkFun. Buy a board!
  https://www.sparkfun.com/products/14569

  Written by Nathan Seidle @ SparkFun Electronics, December 28th, 2017

  The MLX90632 can remotely measure object temperatures within 1 degree C.

  This library handles the initialization of the MLX90632 and the calculations
  to get the temperatures.

  https://github.com/sparkfun/SparkFun_MLX90632_Arduino_Library

  Development environment specifics:
  Arduino IDE 1.8.3

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
  TODO:
  check EEPROM write
  check timing - how fast to take reading? Setting the SOC twice may be doubling time
  set emissivity
*/

//Declare global variables for the calibration values
double P_R;
double P_G;
double P_T;
double P_O;
double Ea;
double Eb;
double Fa;
double Fb;
double Ga;
double Gb;
double Ka;
double Ha;
double Hb;

double TOdut = 25.0; //Assume 25C for first iteration
double TO0 = 25.0; //object temp from previous calculation
double TA0 = 25.0; //ambient temp from previous calculation
double sensorTemp; //Internal temp of the MLX sensor

#include "SparkFun_MLX90632_Arduino_Library.h"

//Attempt communication with the device
//Return true if we got a 'Polo' back from Marco
boolean MLX90632::begin()
{
  uint8_t deviceAddress = MLX90632_DEFAULT_ADDRESS;
  TwoWire &wirePort = Wire;
  _lastObjectTemp = 25.0; 
  MLX90632::status returnError;
  if (begin(deviceAddress, wirePort, returnError) == true)
    return (true);
  return (false);
}

//This begins the communication with the device
//Returns a status error if anything went wrong
boolean MLX90632::begin(uint8_t deviceAddress, TwoWire &wirePort, status &returnError)
{
  returnError = SENSOR_SUCCESS;
  _deviceAddress = deviceAddress; //Get the I2C address from user
  _i2cPort = &wirePort; //Grab which port the user wants us to use

  //We require caller to begin their I2C port, with the speed of their choice
  //external to the library
  //_i2cPort->begin();
  //We could to a check here to see if user has init the Wire or not. Would
  //need to be for different platforms

  //Check communication with IC
  uint16_t thisAddress;
  returnError = readRegister16(EE_I2C_ADDRESS, thisAddress);
  if (thisAddress != _deviceAddress >> 1)
  {
    if (_printDebug)
    {
      _debugPort->print(F("Error: Communication failure. Check wiring. Expected device address: 0x"));
      _debugPort->print(_deviceAddress, HEX);
      _debugPort->print(F(", instead read: 0x"));
      _debugPort->print(thisAddress << 1, HEX);
      _debugPort->println();
    }
    returnError = SENSOR_ID_ERROR;
    return (false); //Error
  }

  if (_printDebug)
  {
    uint16_t thisVersion;
    returnError = readRegister16(EE_VERSION, thisVersion);
    _debugPort->print(F("Sensor EEPROM version (usually 0x205): 0x"));
    _debugPort->println(thisVersion, HEX);
  }

  //Wait for eeprom_busy to clear
  uint16_t counter = 0;
  while (eepromBusy())
  {
    delay(1);
    counter++;
    if (counter == MAX_WAIT)
    {
      if (_printDebug) _debugPort->println(F("EEPROM busy timeout"));
      returnError = SENSOR_TIMEOUT_ERROR;
      return (false); //Error
    }
  }

  setMode(MODE_SLEEP); //Before reading EEPROM sensor needs to stop taking readings

  //Load all the static calibration factors
  int16_t tempValue16;
  int32_t tempValue32;
  readRegister32(EE_P_R, (uint32_t&)tempValue32);
  P_R = (double)tempValue32 * pow(2, -8);
  readRegister32(EE_P_G, (uint32_t&)tempValue32);
  P_G = (double)tempValue32 * pow(2, -20);
  readRegister32(EE_P_T, (uint32_t&)tempValue32);
  P_T = (double)tempValue32 * pow(2, -44);
  readRegister32(EE_P_O, (uint32_t&)tempValue32);
  P_O = (double)tempValue32 * pow(2, -8);
  readRegister32(EE_Ea, (uint32_t&)tempValue32);
  Ea = (double)tempValue32 * pow(2, -16);
  readRegister32(EE_Eb, (uint32_t&)tempValue32);
  Eb = (double)tempValue32 * pow(2, -8);
  readRegister32(EE_Fa, (uint32_t&)tempValue32);
  Fa = (double)tempValue32 * pow(2, -46);
  readRegister32(EE_Fb, (uint32_t&)tempValue32);
  Fb = (double)tempValue32 * pow(2, -36);
  readRegister32(EE_Ga, (uint32_t&)tempValue32);
  Ga = (double)tempValue32 * pow(2, -36);
  
  readRegister16(EE_Gb, (uint16_t&)tempValue16);
  Gb = (double)tempValue16 * pow(2, -10);
  readRegister16(EE_Ka, (uint16_t&)tempValue16);
  Ka = (double)tempValue16 * pow(2, -10);
  readRegister16(EE_Ha, (uint16_t&)tempValue16);
  Ha = (double)tempValue16 * pow(2, -14); //Ha!
  readRegister16(EE_Hb, (uint16_t&)tempValue16);
  Hb = (double)tempValue16 * pow(2, -14);

  if (_printDebug)
  {
    //Print the calibration values
    _debugPort->print(F("P_R: "));
    _debugPort->println(P_R);
    _debugPort->print(F("P_G: "));
    _debugPort->println(P_G, 5);
    _debugPort->print(F("P_T: "));
    _debugPort->println(P_T, 5);
    _debugPort->print(F("P_O: "));
    _debugPort->println(P_O, 5);
    _debugPort->print(F("Ea: "));
    _debugPort->println(Ea, 5);
    _debugPort->print(F("Eb: "));
    _debugPort->println(Eb, 5);
    _debugPort->print(F("Fa: "));
    _debugPort->println(Fa, 10);
    _debugPort->print(F("Fb: "));
    _debugPort->println(Fb, 10);
    _debugPort->print(F("Ga: "));
    _debugPort->println(Ga, 5);
    _debugPort->print(F("Gb: "));
    _debugPort->println(Gb, 5);
    _debugPort->print(F("Ka: "));
    _debugPort->println(Ka, 5);
    _debugPort->print(F("Ha: "));
    _debugPort->println(Ha, 5);
    _debugPort->print(F("Hb: "));
    _debugPort->println(Hb, 5);

    _debugPort->println(F("MLX90632 online"));
  }

  setMode(MODE_STEP);
  return (true);
}

//Read all calibration values and calculate the temperature of the thing we are looking at
//Depending on mode, initiates a measurement
//If in sleep or step mode, clears the new_data bit, sets the SOC bit

bool MLX90632::startRawSensorValues(status& returnError)
{
	returnError = SENSOR_SUCCESS;

	//If the sensor is not in continuous mode then the tell sensor to take reading
	if (getMode() != MODE_CONTINUOUS)
	{
		returnError = setSOC();
	}

	//Write new_data = 0
	clearNewData();
	return (returnError == SENSOR_SUCCESS);
}


void MLX90632::getRawSensorValues(status& returnError, float &AMB, float &Sto) {
	// Removed following because it doens't seem to do anything
  //gatherSensorTemp(returnError);


	uint16_t status = getStatus(returnError);
	bool newData = status & ((uint16_t)1 << BIT_NEW_DATA);

	if (!newData)
	{
		// Don't bother recalculating TO0 if there's no new data
		returnError = SENSOR_NO_NEW_DATA;
		// ToDo: think about effect of globalTO0
		if (_printDebug)
		{
			_debugPort->println("accessed too early:getRaw");
		}
		AMB = -1.0;
		Sto = -1.0;
		return;
	}

	//Read cycle_pos to get measurement pointer
	int cyclePosition = status >> BIT_CYCLE_POS; //Shave off last two bits

	int16_t lowerRAM = 0;
	int16_t upperRAM = 0;

	//Get RAM_6 and RAM_9
	int16_t sixRAM;
	readRegister16(RAM_6, (uint16_t&)sixRAM);
	int16_t nineRAM;
	readRegister16(RAM_9, (uint16_t&)nineRAM);

	//If cycle_pos = 1
	//Calculate TA and TO based on RAM_4, RAM_5, RAM_6, RAM_9
	if (cyclePosition == 1)
	{
		readRegister16(RAM_4, (uint16_t&)lowerRAM);
		readRegister16(RAM_5, (uint16_t&)upperRAM);
	}
	//If cycle_pos = 2
	//Calculate TA and TO based on RAM_7, RAM_8, RAM_6, RAM_9
	else if (cyclePosition == 2)
	{
		readRegister16(RAM_7, (uint16_t&)lowerRAM);
		readRegister16(RAM_8, (uint16_t&)upperRAM);
	}
	else
	{
		if (_printDebug) _debugPort->println(F("Found a cycle position that was not 1 or 2"));
		readRegister16(RAM_4, (uint16_t&)lowerRAM);
		readRegister16(RAM_5, (uint16_t&)upperRAM);
	}

	clearNewData();

	//bool testDummyData = false;
	//if (testDummyData)
	//{
	//	// Generate and send dummy data to validate pipes
	//	static float testData = 30.f;
	//	testData += 0.1f;
	//	if (testData > 40.f)
	//	{
	//		testData = 30.f;
	//	}
	//	//return testData;
	//	// Find better handling
	//	AMB = -2.0;
	//	Sto = -2.0;
	//	returnError = SENSOR_SUCCESS;
	//	return;
	//}

	double VRta;
	// double AMB;

	VRta = nineRAM + Gb * (sixRAM / 12.0);
	AMB = (sixRAM / 12.0) / VRta * pow(2, 19); // we need AMB for object temp calculation

	double VRto;
	// double Sto;

	float S = (float)(lowerRAM + upperRAM) / 2.0;
	VRto = nineRAM + Ka * (sixRAM / 12.0);
	Sto = (S / 12.0) / VRto * (double)pow(2, 19); // we need Sto for Object temp calculation

	if (_printDebug)
	{
		_debugPort->println();
		_debugPort->print(F("VRta: "));
		_debugPort->println(VRta);
		_debugPort->print(F("AMB: "));
		_debugPort->println(AMB, 10);
		_debugPort->print(F("VRto: "));
		_debugPort->println(VRto);
		_debugPort->print(F("Sto: "));
		_debugPort->println(Sto);
		_debugPort->print(F("S: "));
		_debugPort->println(S);
	}
	returnError = SENSOR_SUCCESS;
}


float MLX90632::getObjectTemp(float AMB, float Sto)
{	
	if (AMB == -1.0 && Sto == -1.0)
	{
		if (_printDebug)
		{
			_debugPort->println("accessed too early:processeed");
		}
		return _lastObjectTemp;
	}
	double TAdut;
	double ambientTempK;
	double bigFraction;
	double objectTemp;

	// *******************    WARNING THIS SHOULD ITERATE 3 TIMES   **************
	// THIS CODE NEEDS TO BE REFACTORED SO THAT READING AND CALCULATING CAN BE PERFORMED SEPERATELY
  //Object temp requires 3 iterations
  //Updated Loop iterations to 3 times
  for (uint8_t i = 0 ; i < 3 ; i++)
  {
		// ToDo: Optimize this section to reduce CPU load / delay

		// Removed following because it doens't seem to do anything
    //double sensorTemp = P_O + (AMB - P_R) / P_G + P_T * pow((AMB - P_R), 2);


    TAdut = (AMB - Eb) / Ea + 25.0;

    ambientTempK = TAdut + 273.15;

    bigFraction = Sto / (1 * Fa * Ha * (1 + Ga * (TOdut - _lastObjectTemp) + Fb * (TAdut - TA0)));

    objectTemp = bigFraction + pow(ambientTempK, 4);
    objectTemp = pow(objectTemp, 0.25); //Take 4th root
    objectTemp = objectTemp - 273.15 - Hb;

    TO0 = objectTemp;

    if (_printDebug)
    {
      _debugPort->print(F("sensorTemp (Ta): "));
      _debugPort->println(sensorTemp, 4);
      _debugPort->print(F("TAdut: "));
      _debugPort->println(TAdut, 10);
      _debugPort->print(F("ambientTempK: "));
      _debugPort->println(ambientTempK, 10);
      _debugPort->print(F("bigFraction: "));
      _debugPort->println(bigFraction, 4);

      _debugPort->print(F("Object temp "));
      _debugPort->print(i);
      _debugPort->print(F(": "));
      _debugPort->println(objectTemp, 7);
    }

  }
  if (!isnan(TO0))
  {
	  _lastObjectTemp = TO0;
  }
  return (TO0);
}


float MLX90632::getObjectTemp()
{
	status returnError;
	return getObjectTemp(returnError);
}

float MLX90632::getObjectTemp(MLX90632::status &returnError)
{
	float AMB, Sto;
	float objectTemp;

	bool statusStart = false;
	while (!statusStart)
	{
		statusStart = startRawSensorValues(returnError); //start measurement conversion
	}

	returnError = MLX90632::status::SENSOR_NO_NEW_DATA;

	while (returnError != MLX90632::status::SENSOR_SUCCESS)
	{
		getRawSensorValues(returnError, AMB, Sto);
	}
	objectTemp = getObjectTemp(AMB, Sto);
	return objectTemp;
}


//Convert temp to F
float MLX90632::getObjectTempF()
{
	status myStatus;
	if (startRawSensorValues(myStatus))
	{
		float tempC;
		float AMB;
		float Sto;

		tempC = getObjectTemp();

		float tempF = tempC * 9.0/5.0 + 32.0;
		return(tempF);
	}
	else
	{
		return 0.0f;
	}
}

//Returns the current temperature of the sensor
float MLX90632::getSensorTemp()
{
  MLX90632::status tempFlag;
  return (getSensorTemp(tempFlag));
}

float MLX90632::getSensorTemp(status &returnError)
{
  returnError = SENSOR_SUCCESS;

  //If the sensor is not in continuous mode then the tell sensor to take reading
  if(getMode() != MODE_CONTINUOUS) setSOC();

  //Write new_data = 0
  clearNewData();

  //Wait for new data
  uint16_t counter = 0;
  while (dataAvailable() == false)
  {
    delay(1);
    counter++;
    if (counter == MAX_WAIT)
    {
      returnError = SENSOR_TIMEOUT_ERROR;
      return (0.0); //Error
    }
  }

  return (gatherSensorTemp(returnError));
}

//This reads all the temperature calibration factors for the sensor itself
//This is needed so that it can be called from getObjectTemp *and* users can call getSensorTemp 
//without causing a double read
float MLX90632::gatherSensorTemp(status &returnError)
{
  returnError = SENSOR_SUCCESS;

  //Get RAM_6 and RAM_9
  int16_t sixRAM;
  readRegister16(RAM_6, (uint16_t&)sixRAM);
  int16_t nineRAM;
  readRegister16(RAM_9, (uint16_t&)nineRAM);

  double VRta = nineRAM + Gb * (sixRAM / 12.0);

  double AMB = (sixRAM / 12.0) / VRta * pow(2, 19);

  double sensorTemp = P_O + (AMB - P_R) / P_G + P_T * pow((AMB - P_R), 2);

  if (_printDebug)
  {
    _debugPort->print(F("sixRAM: "));
    _debugPort->println(sixRAM);
    _debugPort->print(F("nineRAM: "));
    _debugPort->println(nineRAM);
    _debugPort->print(F("VRta: "));
    _debugPort->println(VRta, 7);
    _debugPort->print(F("AMB: "));
    _debugPort->println(AMB, 10);
    _debugPort->print(F("sensorTemp (Ta): "));
    _debugPort->println(sensorTemp, 4);
  }

  return(sensorTemp);
}

//Returns true if device is busy doing measurement
//Always true in continuous mode
bool MLX90632::deviceBusy()
{
  if (getStatus() & ((uint16_t)1 << BIT_DEVICE_BUSY)) return (true);
  return (false);
}

//Returns true if eeprom is busy
//EEPROM is busy during boot up and during EEPROM write/erase
bool MLX90632::eepromBusy()
{
  if (getStatus() & ((uint16_t)1 << BIT_EEPROM_BUSY)) return (true);
  return (false);
}

//Returns the cycle_pos from status register. cycle_pos is 0 to 31
uint8_t MLX90632::getCyclePosition()
{
  uint16_t status = getStatus() >> BIT_CYCLE_POS; //Shave off last two bits
  status &= 0x1F; //Get lower 5 bits.
  return (status);
}

//Returns true if new data is available
bool MLX90632::dataAvailable()
{
  if (getStatus() & ((uint16_t)1 << BIT_NEW_DATA)) return (true);
  return (false);
}

//Sets the brown_out bit. Datasheet says 'Customer should set bit to 1'. Ok.
void MLX90632::setBrownOut()
{
  uint16_t reg = getStatus(); //Get current bits
  reg |= (1 << BIT_BROWN_OUT); //Set the bit
  writeRegister16(REG_STATUS, reg); //Set the mode bits
}

//Clear the new_data bit. This is done after a measurement is complete
void MLX90632::clearNewData()
{
	// ToDo: Add status check/return
  uint16_t reg = getStatus(); //Get current bits
  reg &= ~(1 << BIT_NEW_DATA); //Clear the bit
  writeRegister16(REG_STATUS, reg); //Set the mode bits
}

//Returns the REG_STATUS 16-bit register
uint16_t MLX90632::getStatus()
{
  MLX90632::status returnError;
  return (getStatus(returnError));
}
uint16_t MLX90632::getStatus(status& returnError)
{
  uint16_t deviceStatus;
  returnError = readRegister16(REG_STATUS, deviceStatus);
  return (deviceStatus);
}

//Changes the mode to sleep
void MLX90632::sleepMode()
{
  setMode(MODE_SLEEP);
}

//Changes the mode to step
void MLX90632::stepMode()
{
  setMode(MODE_STEP);
}

//Changes the mode to continuous read
void MLX90632::continuousMode()
{
  setMode(MODE_CONTINUOUS);
}

//Sets the Start of Conversion (SOC) bit
MLX90632::status MLX90632::setSOC()
{
  uint16_t reg;
  MLX90632::status returnError = readRegister16(REG_CONTROL, reg); //Get current bits
  reg |= (1 << 3); //Set the bit
  writeRegister16(REG_CONTROL, reg); //Set the bit
  return (returnError);
}

//Sets the sensing mode (3 modes availabe)
MLX90632::status MLX90632::setMode(uint8_t mode)
{
  uint16_t reg;
  MLX90632::status returnError = readRegister16(REG_CONTROL, reg); //Get current bits
  reg &= ~(0x0003 << 1); //Clear the mode bits
  reg |= (mode << 1); //Set the bits
  writeRegister16(REG_CONTROL, reg); //Set the mode bits
  return (returnError);
}

//Returns the mode of the sensor
uint8_t MLX90632::getMode()
{
  MLX90632::status returnError;
  return (getMode(returnError));
}
uint8_t MLX90632::getMode(status &returnError)
{
  uint16_t mode;
  returnError = readRegister16(REG_CONTROL, mode); //Get current register settings
  mode = (mode >> 1) & 0x0003; //Clear all other bits
  return (mode);
}

//Writes a new device I2C address into EEPROM
//This is probably a function to use carefully. If you set the address
//to something unknown you'll need to use the I2C scanner sketch to detect it again
void MLX90632::writeI2CAddress(uint8_t newAddress)
{

}

//Calling this function with nothing sets the debug port to Serial
//You can also call it with other streams like Serial1, SerialUSB, etc.
void MLX90632::enableDebugging(Stream &debugPort)
{
  _debugPort = &debugPort;
  _printDebug = true;
}

void MLX90632::disableDebugging(void)
{
  _printDebug = false;
}

//Reads two consecutive bytes from a given location
//Stores the result at the provided outputPointer
MLX90632::status MLX90632::readRegister16(uint16_t addr, uint16_t &outputPointer)
{
  MLX90632::status returnError = SENSOR_SUCCESS; //By default, return success

  _i2cPort->beginTransmission(_deviceAddress);
  _i2cPort->write(addr >> 8); //MSB
  _i2cPort->write(addr & 0xFF); //LSB
  //_i2cPort->endTransmission(false); //Send a restart command. Do not release bus.
  if (_i2cPort->endTransmission(false) != 0) //Send a restart command. Do not release bus.
  {
    //Sensor did not ACK
    if (_printDebug) _debugPort->println(F("I2C Error: End transmission"));
    returnError = SENSOR_I2C_ERROR;
  }

  _i2cPort->requestFrom((uint8_t)_deviceAddress, (uint8_t)2);
  if (_i2cPort->available())
  {
    uint8_t msb = _i2cPort->read();
    uint8_t lsb = _i2cPort->read();

    outputPointer = (uint16_t)msb << 8 | lsb;
  }
  else
  {
    //Sensor did not respond
    if (_printDebug) _debugPort->println(F("I2C Error: No read response"));
    returnError = SENSOR_I2C_ERROR;
  }

  return (returnError); //Report whether we were successful or not
}

//Reads two 16-bit registers and combines them into 32 bits
MLX90632::status MLX90632::readRegister32(uint16_t addr, uint32_t &outputPointer)
{
  MLX90632::status returnError = SENSOR_SUCCESS; //By default, return success
 
  _i2cPort->beginTransmission(_deviceAddress);
  _i2cPort->write(addr >> 8); //MSB
  _i2cPort->write(addr & 0xFF); //LSB
  if (_i2cPort->endTransmission(false) != 0) //Send a restart command. Do not release bus.
  {
    //Sensor did not ACK
    if (_printDebug) _debugPort->println(F("I2C Error: End transmission"));
    returnError = SENSOR_I2C_ERROR;
  }
 
  _i2cPort->requestFrom(_deviceAddress, (uint8_t)4);
  if (_i2cPort->available())
  {
    uint8_t msb0 = _i2cPort->read();
    uint8_t lsb0 = _i2cPort->read();
    uint8_t msb1 = _i2cPort->read();
    uint8_t lsb1 = _i2cPort->read();
 
    //For the MLX90632 the first 16-bit chunk is LSB, the 2nd is MSB
    uint16_t lower = (uint16_t)msb0 << 8 | lsb0;
    uint16_t upper = (uint16_t)msb1 << 8 | lsb1;
 
    outputPointer = (uint32_t)upper << 16 | lower;
  }
 
  return (returnError); //Report whether we were successful or not
}

//Write two bytes to a spot
MLX90632::status MLX90632::writeRegister16(uint16_t addr, uint16_t val)
{
  MLX90632::status returnError = SENSOR_SUCCESS; //By default, return success

  _i2cPort->beginTransmission(_deviceAddress);
  _i2cPort->write(addr >> 8); //MSB
  _i2cPort->write(addr & 0xFF); //LSB
  _i2cPort->write(val >> 8); //MSB
  _i2cPort->write(val & 0xFF); //LSB
  if (_i2cPort->endTransmission() != 0)
  {
    //Sensor did not ACK
    if (_printDebug) _debugPort->println(F("I2C Error: End transmission"));
    returnError = SENSOR_I2C_ERROR;
  }

  return (returnError); //Report whether we were successful or not
}

//Write a value to EEPROM
//Requires unlocking the EEPROM, writing 0x0000, unlocking again, then writing value
//The datasheet doesn't go a good job of explaining how writing to EEPROM works.
//This should work but doesn't. It seems the IC is very sensitive to I2C traffic while
//the sensor is recording the new EEPROM.

//UPDATE: The sensor should be in STEP_MODE before data can be written into the EEPROM
void MLX90632::writeEEPROM(uint16_t addr, uint16_t val)
{
  //Put device into halt mode (page 15)
  uint8_t originalMode = getMode();

  // SENSOR HAS TO BE IN step MODE TO WRITE INTO THE EEPROM
	// ToDo: Consider performing a check on setMode success
  setMode(MODE_STEP);

  //Wait for complete
  while (deviceBusy()) delay(1);

  //Magic unlock (page 17)
  writeRegister16(0x3005, 0x554C);

  //Wait for complete
  delay(100);

  //Now we can write to one EEPROM word
  //Write 0x0000 to user's location (page 16) to erase
  writeRegister16(addr, 0x0000);

  //Wait for complete
  delay(100);

  //Magic unlock again
  writeRegister16(0x3005, 0x554C);

  //Wait for complete
  delay(100);

  //Now we can write to one EEPROM word
  writeRegister16(addr, val);

  //Wait for complete
  delay(100);

  //Return to original mode
  setMode(originalMode);
}

void MLX90632::setMeasurementRate(uint8_t rate) {
	uint16_t read_meas1, read_meas2;
	MLX90632::status returnError = setMode(MODE_STEP);
	// ToDo: Consider performing a check on setMode success
	readRegister16(EE_MEAS1, read_meas1);
	readRegister16(EE_MEAS2, read_meas2);
	if (rate == 0) {// for 0.5Hz Sampling
		if (read_meas1 != 0x800D) {
			writeEEPROM(EE_MEAS1, 0x800D);
		}
		if (read_meas2 != 0x801D) {
			writeEEPROM(EE_MEAS2, 0x801D);
		}
	}
	else if (rate == 1) {
		if (read_meas1 != 0x810D) {
			writeEEPROM(EE_MEAS1, 0x810D);
		}
		if (read_meas2 != 0x811D) {
			writeEEPROM(EE_MEAS2, 0x811D);
		}
	}
	else if (rate == 2) {
		if (read_meas1 != 0x820D) {
			writeEEPROM(EE_MEAS1, 0x820D);
		}
		if (read_meas2 != 0x821D) {
			writeEEPROM(EE_MEAS2, 0x821D);
		}
	}
	else if (rate == 4) {
		if (read_meas1 != 0x830D) {
			writeEEPROM(EE_MEAS1, 0x830D);
		}
		if (read_meas2 != 0x831D) {
			writeEEPROM(EE_MEAS2, 0x831D);
		}
	}
	else if (rate == 8) {
		if (read_meas1 != 0x840D) {
			writeEEPROM(EE_MEAS1, 0x840D);
		}
		if (read_meas2 != 0x841D) {
			writeEEPROM(EE_MEAS2, 0x841D);
		}
	}
	else if (rate == 16) {
		if (read_meas1 != 0x850D) {
			writeEEPROM(EE_MEAS1, 0x850D);
		}
		if (read_meas2 != 0x851D) {
			writeEEPROM(EE_MEAS2, 0x851D);
		}
	}
	else if (rate == 32) {
		if (read_meas1 != 0x860D) {
			writeEEPROM(EE_MEAS1, 0x860D);
		}
		if (read_meas2 != 0x861D) {
			writeEEPROM(EE_MEAS2, 0x861D);
		}
	}
	else if (rate == 64) {
		if (read_meas1 != 0x870D) {
			writeEEPROM(EE_MEAS1, 0x870D);
		}
		if (read_meas2 != 0x871D) {
			writeEEPROM(EE_MEAS2, 0x871D);
		}
	}
	readRegister16(EE_MEAS1, read_meas1);
	readRegister16(EE_MEAS2, read_meas2);

	// ToDo: Perform check to see whether rate was changed
	_refreshRate = rate;

	Serial.println("Updated Register contents");
	Serial.print("EE_MEAS1: ");
	Serial.println(read_meas1, HEX);
	Serial.print("EE_MEAS2: ");
	Serial.println(read_meas2, HEX);
	Serial.print("Refresh Rate: ");
	Serial.println(_refreshRate);
}