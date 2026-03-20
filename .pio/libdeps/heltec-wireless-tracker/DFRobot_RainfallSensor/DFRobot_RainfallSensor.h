/*!
 * @file  DFRobot_RainfallSensor.h
 * @brief  Define infrastructure of DFRobot_RainfallSensor class
 * @details  This library implements all the functions of communicating with the SEN0575 device, including configuring device parameters and reading device data.
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license  The MIT License (MIT)
 * @author  [fary](feng.yang@dfrobot.com)
 * @version  V1.0
 * @date  2023-01-28
 * @url  https://github.com/DFRobot/DFRobot_RainfallSensor
 */
#ifndef __DFROBOT_RAINFALLSENSOR_H__
#define __DFROBOT_RAINFALLSENSOR_H__
#include <Arduino.h>
#include "DFRobot_RTU.h"
#include "Wire.h"
class DFRobot_RainfallSensor{
  public: 
    #define I2C_REG_PID                            0x00
    #define I2C_REG_VID                            0x02
    #define I2C_REG_VERSION                        0x0A
    #define I2C_REG_TIME_RAINFALL                  0x0C
    #define I2C_REG_CUMULATIVE_RAINFALL            0x10
    #define I2C_REG_RAW_DATA                       0x14
    #define I2C_REG_SYS_TIME                       0x18
    #define I2C_REG_RAIN_HOUR                      0x26
    #define I2C_REG_BASE_RAINFALL                  0x28
    #define IIC_MODE  0
    #define UART_MODE 1
    /**
     * @enum  eSEN0575InputReg_t
     * @brief  Input Register Address
     */
    typedef enum{
      eInputRegPidSEN0575=0x0000,                 /**< SEN0575 stores the address of the input register for PID in memory. */
      eInputRegVidSEN0575,                        /**< The address of the input register for VID in memory. */
      eInputRegRegAddrSEN0575,                    /**< The address of the input register for device address in memory. */
      eInputRegBaudSEN0575,                       /**< The address of the input register for device baudrate in memory. */
      eInputRegVerifyAndStopSEN0575,              /**< The address of the input register for RS485 parity bit and stop bit in memory. */
      eInputRegVersionSEN0575,                    /**< The address of the input register for firmware version in memory. */
      eInputRegTimeRainFallLSEN0575,              /**< The address of the input register for low 16-bit cumulative rainfall in set time. */
      eInputRegTimeRainFallHSEN0575,              /**< The address of the input register for high 16-bit cumulative rainfall in set time. */
      eInputRegCumulativeRainFallLSEN0575,        /**< The address of the input register for low 16-bit cumulative rainfall since working started. */
      eInputRegCumulativeRainFallHSEN0575,        /**< The address of the input register for high 16-bit cumulative rainfall since working started. */
      eInputRegRawDataLSEN0575,                   /**< The address of the input register for raw data (low 16-bit) in memory. */
      eInputRegRawDataHSEN0575,                   /**< he address of the input register for raw data (high 16-bit) in memory. */
      eInputRegSysWorkingTimeSEN0575,             /**< The address of the input register for system working time in memory. */
    }eSEN0575InputReg_t;
  
    /**
     * @enum  eSEN0575HoldingReg_t
     * @brief  Holding Register Address of the Device
     */
    typedef enum{
      eHoldingRegReserved0SEN0575=0x0000,         /**< SEN0575 this register is reserved. */
      eHoldingRegReserved1SEN0575,                /**< SEN0575 this register is reserved. */
      eHoldingRegReserved2SEN0575,                /**< SEN0575 this register is reserved. */
      eHoldingRegReserved3SEN0575,                /**< SEN0575 this register is reserved. */
      eHoldingRegReserved4SEN0575,                /**< SEN0575 this register is reserved. */
      eHoldingRegReserved5SEN0575,                /**< SEN0575 this register is reserved. */
      eHoldingRegRainHourSEN0575,                 /**< Set the time to calculate cumulative rainfall */
      eHoldingRegBaseRainFallSEN0575,             /**< Set the base rainfall value. */
    }eSEN0575HoldingReg_t;
  
    /**
     * @fn DFRobot_RainfallSensor
     * @brief Construct a new dfrobot windspeedwinddirectionrainsensor object
     * @param mode Working mode,IIC_MODE:0 ,UART_MODE:1
     */
    DFRobot_RainfallSensor(uint8_t mode);
    ~DFRobot_RainfallSensor(){};
  
    /**
     * @fn begin
     * @brief This function will attempt to communicate with a slave device and determine if the communication is successful based on the return value.
     * @return Communication result
     * @retval true  Succeed
     * @retval false Failed
     */
    bool begin(void);
  
    /**
     * @fn getFirmwareVersion
     * @brief  get firmware version
     * @return  Return  firmware version
     */
    String getFirmwareVersion(void);
  
    /**
     * @fn getRainfall
     * @brief Get cumulative rainfall
     * @return Cumulative rainfall value
     */
    float getRainfall(void);
  
    /**
     * @fn getRainfall
     * @brief Get the cumulative rainfall within the specified time
     * @param hour Specified time (valid range is 1-24 hours)
     * @return Cumulative rainfall
     */
    float getRainfall(uint8_t hour);
  
    /**
     * @fn getRawData
     * @brief Get the Rawdata object
     * @return Number of tipping bucket counts, unit: count
     */
    uint32_t getRawData();
  
    /**
     * @fn getSensorWorkingTime
     * @brief Obtain the sensor working time
     * @return Working time of the sensor, in hours
     */
    float getSensorWorkingTime();
  
    /**
     * @fn setRainAccumulatedValue
     * @brief Set the Rain Accumulated Value object
     * @param accumulatedValue Rainfall accumulation value, in millimeters
     * @return Set result
     * @retval 0  set successfully
     * @retval Other values set failed
     */
    uint8_t setRainAccumulatedValue(float accumulatedValue = 0.2794);
    uint32_t vid;
    uint32_t pid;
  private:
    /**
     * @fn getPidVid
     * @brief  Get VID and PID
     * @return  Result of getting the VID and PID
     * @retval true:succeeded in getting the VID and PID
     * @retval false:failed to get the data or the data obtained is incorrect
     */
    bool getPidVid(void);
    virtual uint8_t readRegister(uint8_t reg,void* pBuf, size_t size){return 0;};
    virtual uint16_t readRegister(uint16_t reg){return 0;};
    virtual uint8_t writeRegister(uint8_t reg,void* pBuf,size_t size){return 0;};
    virtual uint16_t writeRegister(uint16_t reg,uint16_t data){return 0;};
  private:
    int    _dePin;
    uint8_t   _mode;
};
class DFRobot_RainfallSensor_UART:public DFRobot_RainfallSensor,public DFRobot_RTU{
  public: 
    /**
     * @fn DFRobot_RainfallSensor_UART
     * @brief Construct a new dfrobot windspeedwinddirectionrainsensor uart object
     * @param s The UART device to be used
     */
    DFRobot_RainfallSensor_UART(Stream *s);
    ~DFRobot_RainfallSensor_UART(){};
    /**
     * @fn begin
     * @brief This function will attempt to communicate with the slave device, and check the communication status based on the return value.
     * @return Communication result
     * @retval true  Succeed
     * @retval false Failed
     */
    bool begin(void);
  private:
    /**
     * @fn readRegister
     * @brief Read input register
     * @param reg Input register address
     * @return uint16_t Data read
     */
    uint16_t readRegister(uint16_t reg);
  
    /**
     * 
     * @fn writeRegister
     * @brief Write to the holding register. After writing, an internal delay of 12ms is applied. The sensor needs to spend 12ms to save after writing, during which communication cannot be performed.
     * @param reg The address of the holding register.
     * @param data The data to be written.
     * @return uint16_t The result of the write operation.
     * @retval 0 Indicates success.
     * @retval Other values indicate failure to set.
     */
    uint16_t writeRegister(uint16_t reg,uint16_t data);
  private:
    uint8_t _deviceAddr;
};

class DFRobot_RainfallSensor_I2C:public DFRobot_RainfallSensor{
public: 
  /**
   * @fn DFRobot_RainfallSensor_I2C
   * @brief Construct a new dfrobot windspeedwinddirectionrainsensor iic object
   * @param pWire The I2C device to be used
   */
  DFRobot_RainfallSensor_I2C(TwoWire *pWire);
  ~DFRobot_RainfallSensor_I2C(){};
  /**
   * @fn begin
   * @brief This function will attempt to communicate with the slave device, and returns the communication result
   * @return Communication result
   * @retval true  Succeed
   * @retval false Failed
   */
  bool begin(void);
private:
  /**
   * @fn readRegister
   * @brief Read from an I2C register
   * @param reg I2C register address
   * @param pBuf Buffer space
   * @param size Read length
   * @return Length of data read
   */
  uint8_t readRegister(uint8_t reg,void* pBuf, size_t size);

  /**
   * @fn writeRegister
   * @brief Write I2C register, after writing, there is an internal delay of 12MS. The sensor needs to spend 12Ms to save after writing, and communication cannot be performed at this time.
   * @param reg I2C register address
   * @param pBuf Data storage space
   * @param size Read length
   * @return Write result
   * @retval 0 indicates success
   * @retval other values indicate setting failure
   */
  uint8_t writeRegister(uint8_t reg,void* pBuf,size_t size);
private:
  TwoWire* _pWire;
  uint8_t _deviceAddr;
};
#endif
