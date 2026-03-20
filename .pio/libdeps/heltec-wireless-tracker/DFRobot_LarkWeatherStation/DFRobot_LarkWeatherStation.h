/*!
 * @file  DFRobot_LarkWeatherStation.h
 * @brief DFRobot_LarkWeatherStation Class infrastructure
 * @copyright	Copyright (c) 2021 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license   The MIT License (MIT)
 * @author    [TangJie](jie.tang@dfrobot.com)
 * @version   V1.0
 * @date      2023-07-03
 * @url       https://github.com/DFRobot/DFRobot_LarkWeatherStation
 */
#ifndef _DFROBOT_LARKWEATHERSTATION_H_
#define _DFROBOT_LARKWEATHERSTATION_H_

#include "Arduino.h"
#include "Wire.h"
#include "DFRobot_RTU.h"

#if (defined ARDUINO_AVR_UNO) && (defined ESP8266)
#include "SoftwareSerial.h"
#else
#include "HardwareSerial.h"
#endif

//#define ENABLE_DBG ///< Enable this macro to see the detailed execution process of the program
#ifdef ENABLE_DBG
#define DBG(...) {Serial.print("[");Serial.print(__FUNCTION__); Serial.print("(): "); Serial.print(__LINE__); Serial.print(" ] "); Serial.println(__VA_ARGS__);}
#else
#define DBG(...)
#endif

typedef struct{
    uint16_t year;
    uint16_t  month;
    uint16_t  day;
    uint16_t  hour;
    uint16_t  minute;
    uint16_t  second;
    uint16_t  week;
  }sTime_t;


class DFRobot_LarkWeatherStation{
public:

  /**
   * @fn DFRobot_LarkWeatherStation
   * @brief DFRobot_RP2040_SCI Class Constructor.
   */
  DFRobot_LarkWeatherStation();

  /**
   * @fn  ~DFRobot_LarkWeatherStation
   * @brief DFRobot_RP2040_SCI Class Destructor. 
   */
  ~DFRobot_LarkWeatherStation();

  /**
   * @fn begin
   * @brief Initalize the SCI Acquisition Module, mainly for initializing communication interface
   * 
   * @param freq Set communication frequency, no more than 100kHz
   * @return int Init status
   * @n       0  Init successful
   * @n      -1  The communication interface class & object are not passed in
   * @n      -2  Check if the hardware connection is correct
   */
  int begin(uint32_t freq = 100000);
  /**
   * @brief Get sensor data
   *
   * @param keys Data to be obtained
   * @return String Returns the acquired data
   */
  String getValue(const char *keys);
  /**
   * @brief Get data unit
   *
   * @param keys Data for which units need to be obtained
   * @return String Returns the obtained units
   */
  String getUnit(const char *keys);
 /**
  * @brief Get all data
  *
  * @param state true: include timestamp, false: do not include timestamp
  * @return String Returns all the acquired data
  */
  String getInformation(bool state);
  /**
   * @brief Set RTC time
   *
   * @param year Year
   * @param month Month
   * @param day Day
   * @param hour Hour
   * @param minute Minute
   * @param second Second
   * @return Returns the set status
   */
  uint8_t setTime(uint16_t year,uint8_t month,uint8_t day,uint8_t hour,uint8_t minute,uint8_t second);
 /**
  * @brief Get RTC time
  *
  * @return Returns the acquired RTC time
  */
  String getTimeStamp(void);

protected:
  /**
   * @fn recvPacket
   * @brief Receive and parse the response data packet
   * 
   * @param cmd       Command to receive packet
   * @param errorCode Receive error code
   * @return Pointer array
   * @n      NULL    indicates receiving packet failed
   * @n      Non-NULL  response packet pointer
   */
  void *recvPacket(uint8_t cmd, uint8_t *errorCode);
  /**
   * @fn init
   * @brief Pure virtual function, interface init
   * 
   * @param freq     Communication frequency
   * @return Init status
   * @n       0    Init succeeded
   * @n      -1    Interface object is null pointer
   * @n      -2    Device does not exist
   */
  virtual int init(uint32_t freq) = 0;
  /**
   * @fn sendPacket
   * @brief I2C interface init
   * 
   * @param pkt    Set I2C communication frequency
   * @param length Set I2C communication frequency
   * @param stop   
   * @n     true   Stop
   * @n     false  Not stop
   */
  virtual void sendPacket(void *pkt, int length, bool stop) = 0;
  /**
   * @fn recvData
   * @brief I2C interface init
   * 
   * @param data    Store the received data cache
   * @param len     Byte number to be read
   * @return Actually read byte number    
   */
  virtual int recvData(void *data, int len) = 0;
  /**
   * @fn recvFlush
   * @brief Clear receive cache
   */
  virtual void recvFlush() = 0;
  /**
   * @fn sendFlush
   * @brief Clear send cache
   */
  virtual void sendFlush() = 0;

private:
  uint32_t _timeout; ///< Time of receive timeout
};

class DFRobot_LarkWeatherStation_I2C:public DFRobot_LarkWeatherStation {


public:
/**
 * @brief Example Initialize an i2c object
 * 
 * @param addr device address
 * @param pWire I2C object
 */
DFRobot_LarkWeatherStation_I2C(uint8_t addr = 0x42, TwoWire *pWire = &Wire);
~DFRobot_LarkWeatherStation_I2C();
  protected:
  /**
   * @fn init
   * @brief Initalize I2C interface
   * 
   * @param freq Set I2C communication frequency
   * @return int Init status
   * @n       0  Init successful
   * @n      -1  The communication interface class & object are not passed in
   * @n      -2  Check if the hardware connection is correct
   */
  int init(uint32_t freq);
 /**
   * @fn sendPacket
   * @brief Send data
   * 
   * @param pkt    Data pointer
   * @param length Length of the data to be sent
   * @param stop   Whether to send stop condition
   * @n     true   Stop
   * @n     false  Not stop
   */
  void sendPacket(void *pkt, int length, bool stop = true);
  /**
   * @fn recvData
   * @brief I2C read byte
   * 
   * @param data    Received and stored data cache
   * @param len     Byte number to be read
   * @return Actually read byte number   
   */
  int recvData(void *data, int len);
  /**
   * @fn recvFlush
   * @brief Clear receive cache
   */
  void recvFlush();
  /**
   * @fn sendFlush
   * @brief Clear send cache
   */
  void sendFlush();
private:
  TwoWire *_pWire;
  uint8_t _addr;
};

class DFRobot_LarkWeatherStation_UART:public DFRobot_LarkWeatherStation {

public:
/**
 * @brief Initializes a UART object
 * 
 * @param s Serial port parameter object
 */
DFRobot_LarkWeatherStation_UART(Stream *s);
~DFRobot_LarkWeatherStation_UART();
protected:
   /**
   * @fn init
   * @brief Initalize uatr interface
   * 
   * @param freq Insubstantiality
   * @return int Init status
   * @n       0  Init successful
   * @n      -1  The communication interface class & object are not passed in
   * @n      -2  Check if the hardware connection is correct
   */
  int init(uint32_t freq);
 /**
   * @fn sendPacket
   * @brief Send data
   * 
   * @param pkt    Data pointer
   * @param length Length of the data to be sent
   * @param stop   Whether to send stop condition
   * @n     true   Stop
   * @n     false  Not stop
   */
  void sendPacket(void *pkt, int length, bool stop = true);
  /**
   * @fn recvData
   * @brief I2C read byte
   * 
   * @param data    Received and stored data cache
   * @param len     Byte number to be read
   * @return Actually read byte number   
   */
  int recvData(void *data, int len);
  /**
   * @fn recvFlush
   * @brief Clear receive cache
   */
  void recvFlush();
  /**
   * @fn sendFlush
   * @brief Clear send cache
   */
  void sendFlush();
private:
  uint8_t _state = 0;
  Stream *_s;
};

#endif
