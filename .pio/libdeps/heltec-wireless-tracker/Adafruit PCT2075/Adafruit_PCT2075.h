/*!
 *  @file Adafruit_PCT2075.h
 *  @author   Bryan Siepert (Adafruit Industries)
 *
 * 	This is a library for the Adafruit PCT2075 breakout:
 * 	https://www.adafruit.com/product/4369
 *
 * 	Adafruit invests time and resources providing this open source code,
 *  please support Adafruit and open-source hardware by purchasing products from
 * 	Adafruit!
 *
 *
 *	BSD license (see license.txt)
 */

#ifndef _ADAFRUIT_PCT2075_H
#define _ADAFRUIT_PCT2075_H

#include "Arduino.h"
#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CDevice.h>
#include <Wire.h>

//#define PCT2075_I2CADDR_DEFAULT 0x37  ///<  Address is configured with pins
// A0-A2
#define PCT2075_I2CADDR_DEFAULT 0x37 ///<  Address is configured with pins A0-A2
#define PCT2075_REGISTER_TEMP 0x00   ///<  Temperature register (read-only)
#define PCT2075_REGISTER_CONFIG 0x01 ///<  Configuration register
#define PCT2075_REGISTER_THYST 0x02  ///<  Hysterisis register
#define PCT2075_REGISTER_TOS 0x03    ///<  OS register
#define PCT2075_REGISTER_TIDLE                                                 \
  0x04 ///< Measurement idle time registerconfiguration register

/**
 * @brief Mode options
 *
 * Allowed values for `setMode`.
 */

typedef enum {
  PCT2075_MODE_COMPARITOR, ///< In comparitor mode, the sensor acts like a
                           ///< thermostat and will activate the INT pin when an
                           ///< alert is triggered. The INT pin will be
                           ///< deactiveated when the temperature falls below
                           ///< `temperature_hysteresis`.
  PCT2075_MODE_INTERRUPT   ///<  In interrupt mode the INT pin is activated once
                           ///<  when a temperature fault is detected, and once
                           ///<  more when the temperature falls below
  ///< `temperature_hysteresis`. The alert is cleared by
  ///< reading a value
} pct2075_mode_t;
/** @brief Faut count options
 *
 * Allowed values for `setFaultCount`.
 */

typedef enum {
  PCT2075_FAULT_COUNT_1, ///< Raise an alert after 1 fault
  PCT2075_FAULT_COUNT_2, ///< Raise an alert after 2 faults
  PCT2075_FAULT_COUNT_4, ///< Raise an alert after 4 faults
  PCT2075_FAULT_COUNT_6, ///< Raise an alert after 6 faults
} pct2075_fault_count_t;

/*!
 *    @brief  Class that stores state and functions for interacting with
 *            the PCT2075 I2C Digital Potentiometer
 */
class Adafruit_PCT2075 {
public:
  Adafruit_PCT2075();
  boolean begin(uint8_t i2c_addr = PCT2075_I2CADDR_DEFAULT,
                TwoWire *wire = &Wire);

  float getTemperature(void);

  float getIdleTime(void);
  void setIdleTime(float new_idle_time);

  void setActiveHigh(bool active_high);

  float getHighTemperatureThreshold(void);
  void setHighTemperatureThreshold(float new_temp_threshold);

  float getTemperatureHysteresis(void);
  void setTemperatureHysteresis(float temp_hysteresis);

  pct2075_mode_t getMode(void);
  void setMode(pct2075_mode_t mode);

  pct2075_fault_count_t getFaultCount(void);
  void setFaultCount(pct2075_fault_count_t);

  Adafruit_BusIO_Register *CONFIG; ///< BusIO Register for CONFIG

private:
  bool _init(void);

  Adafruit_I2CDevice *i2c_dev;
};

#endif
