/**************************************************************************/
/**
 *  @file     Adafruit_DPS310.cpp
 *  @author   Limor Fried (Adafruit Industries)
 *  @mainpage Adafruit DSP310 Barometric Pressure Sensor Library
 *  @section intro_sec Introduction
 *
 * 	I2C Driver for the [Adafruit DPS310 barometric pressure breakout
 board](https://www.adafruit.com/product/4494)
 *
 * Adafruit invests time and resources providing this open source code,
 *  please support Adafruit and open-source hardware by purchasing products from
 * 	Adafruit!
 *  @see Adafruit_DPS310

 *  @section dependencies Dependencies
 *  This library depends on the [Adafruit BusIO
 library](https://github.com/adafruit/Adafruit_BusIO) and the [Adafruit Unified
 Sensor Library](https://github.com/adafruit/Adafruit_Sensor)

 * 	@section license License
 *
 * 	BSD (see license.txt)
*/
/**************************************************************************/

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <Adafruit_DPS310.h>
#include <Wire.h>

static int32_t oversample_scalefactor[] = {524288, 1572864, 3670016, 7864320,
                                           253952, 516096,  1040384, 2088960};

/**************************************************************************/
/*!
    @brief  Instantiates a new DPS310 class
*/
/**************************************************************************/
Adafruit_DPS310::Adafruit_DPS310(void) {
  temp_sensor = new Adafruit_DPS310_Temp(this);
  pressure_sensor = new Adafruit_DPS310_Pressure(this);
}

/**************************************************************************/
/*!
    @brief  Cleans up any allocations in our object
*/
/**************************************************************************/
Adafruit_DPS310::~Adafruit_DPS310(void) {
  delete temp_sensor;
  delete pressure_sensor;
}

/*!
 *    @brief  Sets up the hardware and initializes I2C
 *    @param  i2c_address
 *            The I2C address to be used.
 *    @param  wire
 *            The Wire object to be used for I2C connections.
 *    @return True if initialization was successful, otherwise false.
 */
bool Adafruit_DPS310::begin_I2C(uint8_t i2c_address, TwoWire *wire) {
  if (!i2c_dev) {
    i2c_dev = new Adafruit_I2CDevice(i2c_address, wire);
  }
  spi_dev = NULL;

  if (!i2c_dev->begin()) {
    return false;
  }

  return _init();
}

/*!
 *    @brief  Sets up the hardware and initializes hardware SPI
 *    @param  cs_pin The arduino pin # connected to chip select
 *    @param  theSPI The SPI object to be used for SPI connections.
 *    @return True if initialization was successful, otherwise false.
 */
boolean Adafruit_DPS310::begin_SPI(uint8_t cs_pin, SPIClass *theSPI) {
  i2c_dev = NULL;
  if (!spi_dev) {
    spi_dev = new Adafruit_SPIDevice(cs_pin,
                                     1000000,               // frequency
                                     SPI_BITORDER_MSBFIRST, // bit order
                                     SPI_MODE0,             // data mode
                                     theSPI);
  }
  if (!spi_dev->begin()) {
    return false;
  }
  return _init();
}

/*!
 *    @brief  Sets up the hardware and initializes software SPI
 *    @param  cs_pin The arduino pin # connected to chip select
 *    @param  sck_pin The arduino pin # connected to SPI clock
 *    @param  miso_pin The arduino pin # connected to SPI MISO
 *    @param  mosi_pin The arduino pin # connected to SPI MOSI
 *    @return True if initialization was successful, otherwise false.
 */
bool Adafruit_DPS310::begin_SPI(int8_t cs_pin, int8_t sck_pin, int8_t miso_pin,
                                int8_t mosi_pin) {
  i2c_dev = NULL;
  if (!spi_dev) {
    spi_dev = new Adafruit_SPIDevice(cs_pin, sck_pin, miso_pin, mosi_pin,
                                     1000000,               // frequency
                                     SPI_BITORDER_MSBFIRST, // bit order
                                     SPI_MODE0);            // data mode
  }
  if (!spi_dev->begin()) {
    return false;
  }
  return _init();
}

/*!
 *    @brief  Common initialization code for I2C & SPI
 *    @return True if initialization was successful, otherwise false.
 */
bool Adafruit_DPS310::_init(void) {
  // Check connection
  Adafruit_BusIO_Register chip_id = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_PRODREVID, 1);

  // make sure we're talking to the right chip
  if (chip_id.read() != 0x10) {
    // No DPS310 detected ... return false
    return false;
  }

  reset();
  _readCalibration();
  // default to high precision
  configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
  configureTemperature(DPS310_64HZ, DPS310_64SAMPLES);
  // continuous
  setMode(DPS310_CONT_PRESTEMP);
  // wait until we have at least one good measurement
  while (!temperatureAvailable() || !pressureAvailable()) {
    delay(10);
  }
  return true;
}

/**************************************************************************/
/*!
@brief  Performs a software reset
*/
/**************************************************************************/
void Adafruit_DPS310::reset(void) {
  Adafruit_BusIO_Register SOFTRESET = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_RESET, 1);
  SOFTRESET.write(0x89);
  // Wait for a bit till its out of hardware reset
  delay(10);

  Adafruit_BusIO_Register MEAS_CFG = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_MEASCFG, 1);
  Adafruit_BusIO_RegisterBits SENSOR_RDY =
      Adafruit_BusIO_RegisterBits(&MEAS_CFG, 1, 6);
  while (!SENSOR_RDY.read()) {
    delay(1);
  }
}

static int32_t twosComplement(int32_t val, uint8_t bits) {
  if (val & ((uint32_t)1 << (bits - 1))) {
    val -= (uint32_t)1 << bits;
  }
  return val;
}

void Adafruit_DPS310::_readCalibration(void) {
  // Wait till we're ready to read calibration
  Adafruit_BusIO_Register MEAS_CFG = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_MEASCFG, 1);
  Adafruit_BusIO_RegisterBits CALIB_RDY =
      Adafruit_BusIO_RegisterBits(&MEAS_CFG, 1, 7);
  while (!CALIB_RDY.read()) {
    delay(1);
  }

  uint8_t coeffs[18];
  for (uint8_t addr = 0; addr < 18; addr++) {
    Adafruit_BusIO_Register coeff = Adafruit_BusIO_Register(
        i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, 0x10 + addr, 1);
    coeffs[addr] = coeff.read();
  }
  _c0 = ((uint16_t)coeffs[0] << 4) | (((uint16_t)coeffs[1] >> 4) & 0x0F);
  _c0 = twosComplement(_c0, 12);

  _c1 = twosComplement((((uint16_t)coeffs[1] & 0x0F) << 8) | coeffs[2], 12);

  _c00 = ((uint32_t)coeffs[3] << 12) | ((uint32_t)coeffs[4] << 4) |
         (((uint32_t)coeffs[5] >> 4) & 0x0F);
  _c00 = twosComplement(_c00, 20);

  _c10 = (((uint32_t)coeffs[5] & 0x0F) << 16) | ((uint32_t)coeffs[6] << 8) |
         (uint32_t)coeffs[7];
  _c10 = twosComplement(_c10, 20);

  _c01 = twosComplement(((uint16_t)coeffs[8] << 8) | (uint16_t)coeffs[9], 16);
  _c11 = twosComplement(((uint16_t)coeffs[10] << 8) | (uint16_t)coeffs[11], 16);
  _c20 = twosComplement(((uint16_t)coeffs[12] << 8) | (uint16_t)coeffs[13], 16);
  _c21 = twosComplement(((uint16_t)coeffs[14] << 8) | (uint16_t)coeffs[15], 16);
  _c30 = twosComplement(((uint16_t)coeffs[16] << 8) | (uint16_t)coeffs[17], 16);
  /*
  Serial.print("c0 = "); Serial.println(_c0);
  Serial.print("c1 = "); Serial.println(_c1);
  Serial.print("c00 = "); Serial.println(_c00);
  Serial.print("c10 = "); Serial.println(_c10);
  Serial.print("c01 = "); Serial.println(_c01);
  Serial.print("c11 = "); Serial.println(_c11);
  Serial.print("c20 = "); Serial.println(_c20);
  Serial.print("c21 = "); Serial.println(_c21);
  Serial.print("c30 = "); Serial.println(_c30);
  */
}

/**************************************************************************/
/*!
    @brief  Whether new temperature data is available
    @returns True if new data available to read
*/
/**************************************************************************/
bool Adafruit_DPS310::temperatureAvailable(void) {
  Adafruit_BusIO_Register MEAS_CFG = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_MEASCFG, 1);
  Adafruit_BusIO_RegisterBits tempbit =
      Adafruit_BusIO_RegisterBits(&MEAS_CFG, 1, 5);
  return tempbit.read();
}

/**************************************************************************/
/*!
    @brief  Whether new pressure data is available
    @returns True if new data available to read
*/
/**************************************************************************/
bool Adafruit_DPS310::pressureAvailable(void) {
  Adafruit_BusIO_Register MEAS_CFG = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_MEASCFG, 1);
  Adafruit_BusIO_RegisterBits presbit =
      Adafruit_BusIO_RegisterBits(&MEAS_CFG, 1, 4);
  return presbit.read();
}

/**************************************************************************/
/*!
 * @brief Calculates the approximate altitude using barometric pressure and the
 * supplied sea level hPa as a reference.
 * @param seaLevelhPa
 *        The current hPa at sea level.
 * @return The approximate altitude above sea level in meters.
 */
/**************************************************************************/
float Adafruit_DPS310::readAltitude(float seaLevelhPa) {

  float altitude;

  _read();

  altitude = 44330 * (1.0 - pow((_pressure / 100) / seaLevelhPa, 0.1903));

  return altitude;
}

/**************************************************************************/
/*!
    @brief  Set the operational mode of the sensor (continuous or one-shot)
    @param mode can be DPS310_IDLE, one shot: DPS310_ONE_PRESSURE or
   DPS310_ONE_TEMPERATURE, continuous: DPS310_CONT_PRESSURE, DPS310_CONT_TEMP,
   DPS310_CONT_PRESTEMP
*/
/**************************************************************************/
void Adafruit_DPS310::setMode(dps310_mode_t mode) {
  Adafruit_BusIO_Register MEAS_CFG = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_MEASCFG, 1);
  Adafruit_BusIO_RegisterBits modebits =
      Adafruit_BusIO_RegisterBits(&MEAS_CFG, 3, 0);
  modebits.write(mode);
}

/**************************************************************************/
/*!
    @brief Set the sample rate and oversampling averaging for pressure
    @param rate How many samples per second to take
    @param os How many oversamples to average
*/
/**************************************************************************/
void Adafruit_DPS310::configurePressure(dps310_rate_t rate,
                                        dps310_oversample_t os) {
  Adafruit_BusIO_Register PRS_CFG = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_PRSCFG, 1);

  Adafruit_BusIO_RegisterBits ratebits =
      Adafruit_BusIO_RegisterBits(&PRS_CFG, 3, 4);
  Adafruit_BusIO_RegisterBits osbits =
      Adafruit_BusIO_RegisterBits(&PRS_CFG, 4, 0);
  ratebits.write(rate);
  osbits.write(os);

  Adafruit_BusIO_Register CFG_REG = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_CFGREG, 1);
  Adafruit_BusIO_RegisterBits shiftbit =
      Adafruit_BusIO_RegisterBits(&CFG_REG, 1, 2);

  if (os > DPS310_8SAMPLES) {
    shiftbit.write(1);
  } else {
    shiftbit.write(0);
  }

  pressure_scale = oversample_scalefactor[os];
}

/**************************************************************************/
/*!
    @brief Set the sample rate and oversampling averaging for temperature
    @param rate How many samples per second to take
    @param os How many oversamples to average
*/
/**************************************************************************/
void Adafruit_DPS310::configureTemperature(dps310_rate_t rate,
                                           dps310_oversample_t os) {
  Adafruit_BusIO_Register TMP_CFG = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_TMPCFG, 1);
  Adafruit_BusIO_RegisterBits ratebits =
      Adafruit_BusIO_RegisterBits(&TMP_CFG, 3, 4);
  Adafruit_BusIO_RegisterBits osbits =
      Adafruit_BusIO_RegisterBits(&TMP_CFG, 4, 0);

  ratebits.write(rate);
  osbits.write(os);
  temp_scale = oversample_scalefactor[os];

  // Set shift bit if necessary
  Adafruit_BusIO_Register CFG_REG = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_CFGREG, 1);
  Adafruit_BusIO_RegisterBits shiftbit =
      Adafruit_BusIO_RegisterBits(&CFG_REG, 1, 3);

  if (os > DPS310_8SAMPLES) {
    shiftbit.write(1);
  } else {
    shiftbit.write(0);
  }

  // Find out what our calibration source is
  Adafruit_BusIO_Register TMP_COEFF = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_TMPCOEFSRCE, 1);
  Adafruit_BusIO_RegisterBits srcbit =
      Adafruit_BusIO_RegisterBits(&TMP_COEFF, 1, 7);
  // Serial.print("temp coeff: 0x"); Serial.println(TMP_COEFF.read(), HEX);
  // Serial.print("src bit: 0x"); Serial.println(srcbit.read(), HEX);

  // the bit is in another register
  Adafruit_BusIO_RegisterBits extbit =
      Adafruit_BusIO_RegisterBits(&TMP_CFG, 1, 7);

  extbit.write(srcbit.read());
}

/**************************************************************************/
/*!
  @brief  Read the XYZ data from the sensor and store in the internal
  raw_pressure, raw_temperature, _pressure and _temperature variables.
*/
/**************************************************************************/

void Adafruit_DPS310::_read(void) {
  Adafruit_BusIO_Register PRS_B2 = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_PRSB2, 3, MSBFIRST);
  Adafruit_BusIO_Register TMP_B2 = Adafruit_BusIO_Register(
      i2c_dev, spi_dev, ADDRBIT8_HIGH_TOREAD, DPS310_TMPB2, 3, MSBFIRST);
  raw_temperature = twosComplement(TMP_B2.read(), 24);
  raw_pressure = twosComplement(PRS_B2.read(), 24);

  _scaled_rawtemp = (float)raw_temperature / temp_scale;
  _temperature = _scaled_rawtemp * _c1 + _c0 / 2.0;
  // Serial.print("Temp: "); Serial.println(_temperature);

  // Serial.print("Raw prs: " ); Serial.println(raw_pressure);
  _pressure = (float)raw_pressure / pressure_scale;
  // Serial.print("Scaled prs:" ); Serial.println(_pressure, 6);

  /*
  Serial.println(_c00);
  Serial.println(_pressure * ((int32_t)_c10 + _pressure * ((int32_t)_c20 +
  _pressure * (int32_t)_c30)), 6); Serial.println(_scaled_rawtemp *
  ((int32_t)_c01 + _pressure * ((int32_t)_c11 + _pressure * (int32_t)_c21)), 6);
  */

  _pressure =
      (int32_t)_c00 +
      _pressure * ((int32_t)_c10 +
                   _pressure * ((int32_t)_c20 + _pressure * (int32_t)_c30)) +
      _scaled_rawtemp *
          ((int32_t)_c01 +
           _pressure * ((int32_t)_c11 + _pressure * (int32_t)_c21));

  // Serial.print("Press: "); Serial.println(_pressure);
}

/**************************************************************************/
/*!
    @brief  Gets the most recent sensor event, Adafruit Unified Sensor format
    @param  temp_event Pointer to an Adafruit Unified sensor_event_t object that
   we'll fill in with temperature data
    @param  pressure_event Pointer to an Adafruit Unified sensor_event_t object
   that we'll fill in with pressure data
    @returns True on successful read
*/
/**************************************************************************/
bool Adafruit_DPS310::getEvents(sensors_event_t *temp_event,
                                sensors_event_t *pressure_event) {
  _read();

  if (temp_event != NULL) {
    /* Clear the event */
    memset(temp_event, 0, sizeof(sensors_event_t));
    // fill in deets
    temp_event->version = 1;
    temp_event->sensor_id = _sensorID;
    temp_event->type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
    temp_event->timestamp = millis();
    temp_event->temperature = _temperature;
  }

  if (pressure_event != NULL) {
    memset(pressure_event, 0, sizeof(sensors_event_t));
    pressure_event->version = 1;
    pressure_event->sensor_id = _sensorID;
    pressure_event->type = SENSOR_TYPE_PRESSURE;
    pressure_event->timestamp = millis();
    pressure_event->pressure = _pressure / 100;
  }

  return true;
}

/*!
    @brief  Gets an Adafruit Unified Sensor object for the temp sensor component
    @return Adafruit_Sensor pointer to temperature sensor
 */
Adafruit_Sensor *Adafruit_DPS310::getTemperatureSensor(void) {
  return temp_sensor;
}

/*!
    @brief  Gets an Adafruit Unified Sensor object for the pressure sensor
   component
    @return Adafruit_Sensor pointer to pressure sensor
 */
Adafruit_Sensor *Adafruit_DPS310::getPressureSensor(void) {
  return pressure_sensor;
}

/**************************************************************************/
/*!
    @brief  Gets the sensor_t data for the DPS310's temperature sensor
*/
/**************************************************************************/
void Adafruit_DPS310_Temp::getSensor(sensor_t *sensor) {
  /* Clear the sensor_t object */
  memset(sensor, 0, sizeof(sensor_t));

  /* Insert the sensor name in the fixed length char array */
  strncpy(sensor->name, "DPS310", sizeof(sensor->name) - 1);
  sensor->name[sizeof(sensor->name) - 1] = 0;
  sensor->version = 1;
  sensor->sensor_id = _sensorID;
  sensor->type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
  sensor->min_delay = 0;
  sensor->min_value = -40.0; /* Temperature range -40 ~ +85 C  */
  sensor->max_value = +85.0;
  sensor->resolution = 0.01; /*  0.01 C */
}

/**************************************************************************/
/*!
    @brief  Gets the temperature as a standard sensor event
    @param  event Sensor event object that will be populated
    @returns True
*/
/**************************************************************************/
bool Adafruit_DPS310_Temp::getEvent(sensors_event_t *event) {
  return _theDPS310->getEvents(event, NULL);
}

/**************************************************************************/
/*!
    @brief  Gets the sensor_t data for the DPS310's pressure sensor
*/
/**************************************************************************/
void Adafruit_DPS310_Pressure::getSensor(sensor_t *sensor) {
  /* Clear the sensor_t object */
  memset(sensor, 0, sizeof(sensor_t));

  /* Insert the sensor name in the fixed length char array */
  strncpy(sensor->name, "DPS310", sizeof(sensor->name) - 1);
  sensor->name[sizeof(sensor->name) - 1] = 0;
  sensor->version = 1;
  sensor->sensor_id = _sensorID;
  sensor->type = SENSOR_TYPE_PRESSURE;
  sensor->min_delay = 0;
  sensor->min_value = 300.0; /* 300 ~ 1200 hPa  */
  sensor->max_value = 1200.0;
  sensor->resolution = 0.002; /* 0.002 hPa relative */
}

/**************************************************************************/
/*!
    @brief  Gets the pressure as a standard sensor event
    @param  event Sensor event object that will be populated
    @returns True
*/
/**************************************************************************/
bool Adafruit_DPS310_Pressure::getEvent(sensors_event_t *event) {
  return _theDPS310->getEvents(NULL, event);
}
