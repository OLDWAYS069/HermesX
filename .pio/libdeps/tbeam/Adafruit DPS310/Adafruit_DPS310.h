/**************************************************************************/
/*!
    @file     Adafruit_DPS310.h
    @author   Limor Fried (Adafruit Industries)
    ----> https://www.adafruit.com/4494

    Adafruit invests time and resources providing this open source code,
    please support Adafruit and open-source hardware by purchasing
    products from Adafruit!

*/
/**************************************************************************/

#ifndef ADAFRUIT_DPS310_H
#define ADAFRUIT_DPS310_H

#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_SPIDevice.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

/*=========================================================================
I2C ADDRESS/BITS
-----------------------------------------------------------------------*/
#define DPS310_I2CADDR_DEFAULT (0x77) ///< Default breakout addres
/*=========================================================================*/

#define DPS310_PRSB2 0x00       ///< Highest byte of pressure data
#define DPS310_TMPB2 0x03       ///< Highest byte of temperature data
#define DPS310_PRSCFG 0x06      ///< Pressure configuration
#define DPS310_TMPCFG 0x07      ///< Temperature configuration
#define DPS310_MEASCFG 0x08     ///< Sensor configuration
#define DPS310_CFGREG 0x09      ///< Interrupt/FIFO configuration
#define DPS310_RESET 0x0C       ///< Soft reset
#define DPS310_PRODREVID 0x0D   ///< Register that contains the part ID
#define DPS310_TMPCOEFSRCE 0x28 ///< Temperature calibration src
/** The measurement rate ranges */
typedef enum {
  DPS310_1HZ,   ///< 1 Hz
  DPS310_2HZ,   ///< 2 Hz
  DPS310_4HZ,   ///< 4 Hz
  DPS310_8HZ,   ///< 8 Hz
  DPS310_16HZ,  ///< 16 Hz
  DPS310_32HZ,  ///< 32 Hz
  DPS310_64HZ,  ///< 64 Hz
  DPS310_128HZ, ///< 128 Hz
} dps310_rate_t;

/** The  oversample rate ranges */
typedef enum {
  DPS310_1SAMPLE,    ///< 1 Hz
  DPS310_2SAMPLES,   ///< 2 Hz
  DPS310_4SAMPLES,   ///< 4 Hz
  DPS310_8SAMPLES,   ///< 8 Hz
  DPS310_16SAMPLES,  ///< 16 Hz
  DPS310_32SAMPLES,  ///< 32 Hz
  DPS310_64SAMPLES,  ///< 64 Hz
  DPS310_128SAMPLES, ///< 128 Hz
} dps310_oversample_t;

/** The  oversample rate ranges */
typedef enum {
  DPS310_IDLE = 0b000,            ///< Stopped/idle
  DPS310_ONE_PRESSURE = 0b001,    ///< Take single pressure measurement
  DPS310_ONE_TEMPERATURE = 0b010, ///< Take single temperature measurement
  DPS310_CONT_PRESSURE = 0b101,   ///< Continuous pressure measurements
  DPS310_CONT_TEMP = 0b110,       ///< Continuous pressure measurements
  DPS310_CONT_PRESTEMP = 0b111,   ///< Continuous temp+pressure measurements
} dps310_mode_t;

class Adafruit_DPS310;

/** Adafruit Unified Sensor interface for temperature component of DPS310 */
class Adafruit_DPS310_Temp : public Adafruit_Sensor {
public:
  /** @brief Create an Adafruit_Sensor compatible object for the temp sensor
      @param parent A pointer to the DPS310 class */
  Adafruit_DPS310_Temp(Adafruit_DPS310 *parent) { _theDPS310 = parent; }
  bool getEvent(sensors_event_t *);
  void getSensor(sensor_t *);

private:
  int _sensorID = 310;
  Adafruit_DPS310 *_theDPS310 = NULL;
};

/** Adafruit Unified Sensor interface for pressure component of DPS310 */
class Adafruit_DPS310_Pressure : public Adafruit_Sensor {
public:
  /** @brief Create an Adafruit_Sensor compatible object for the pressure sensor
      @param parent A pointer to the DPS310 class */
  Adafruit_DPS310_Pressure(Adafruit_DPS310 *parent) { _theDPS310 = parent; }
  bool getEvent(sensors_event_t *);
  void getSensor(sensor_t *);

private:
  int _sensorID = 311;
  Adafruit_DPS310 *_theDPS310 = NULL;
};

/** Class for hardware interfacing with a DPS310 */
class Adafruit_DPS310 {
public:
  Adafruit_DPS310(void);
  ~Adafruit_DPS310(void);

  bool begin_I2C(uint8_t i2c_addr = DPS310_I2CADDR_DEFAULT,
                 TwoWire *wire = &Wire);
  bool begin_SPI(uint8_t cs_pin, SPIClass *theSPI = &SPI);
  bool begin_SPI(int8_t cs_pin, int8_t sck_pin, int8_t miso_pin,
                 int8_t mosi_pin);

  void reset(void);
  void setMode(dps310_mode_t mode);

  void configurePressure(dps310_rate_t rate, dps310_oversample_t os);
  void configureTemperature(dps310_rate_t rate, dps310_oversample_t os);

  bool pressureAvailable(void);
  bool temperatureAvailable(void);

  float readAltitude(float seaLevelhPa = 1013.25);

  Adafruit_Sensor *getTemperatureSensor(void);
  Adafruit_Sensor *getPressureSensor(void);

  bool getEvents(sensors_event_t *temp_event, sensors_event_t *pressure_event);

private:
  bool _init(void);
  void _readCalibration(void);
  void _read();

  int16_t _c0, _c1, _c01, _c11, _c20, _c21, _c30;
  int32_t _c00, _c10;

  int32_t raw_pressure, raw_temperature;
  float _temperature, _scaled_rawtemp, _pressure;
  int32_t temp_scale, pressure_scale;

  Adafruit_I2CDevice *i2c_dev = NULL;
  Adafruit_SPIDevice *spi_dev = NULL;

  Adafruit_DPS310_Temp *temp_sensor = NULL;
  Adafruit_DPS310_Pressure *pressure_sensor = NULL;

  int32_t _sensorID;
};

#endif
