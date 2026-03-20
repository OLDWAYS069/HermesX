DFRobot_RainfallSensor
===========================

- [中文版](./README_CN.md)

This library provides Arduino IDE and Raspberry Pi software drivers, as well as example code, for the SEN0575 rainfall sensor kit. With this library, users can obtain 24-hour rainfall information, the sensor's operating time, and the accumulated rainfall information during the sensor's operating time through software operations.

![产品效果图](../resources/images/SEN0575.png)

## Product Link ([Tipping Bucket Rainfall Measurement Gauge Detector for Weather Station - DFRobot](https://www.dfrobot.com/product-2689.html))

    SKU：SEN0575

## Table of Contents

  * [summary](#summary)
  * [installation](#installation)
  * [methods](#methods)
  * [compatibility](#compatibility)
  * [history](#history)
  * [credits](#credits)

## Summary
* Read rainfall sensor data

## Installation

To use the library, first download the library file, paste it into the designated directory, then open the examples folder and run the demo in that folder. This library is associated with the modbus_tk library, please make sure that the modbus_tk has been downloaded to the Raspberry Pi before using it.
```shell
python2 : 
pip2 install modbus_tk smbus
python3:
pip3 install modbus_tk smbus
```
To use the serial port function of this library, please make sure that the serial port of the Raspberry Pi (/dev/ttyAMA0) is configured correctly.
## Methods

```python
  def begin(self)
    '''!
      @brief This function will attempt to communicate with a slave device and determine if the communication is successful based on the return value.
      @return Communication result
      @retval true  Succeed
      @retval false Failed
    '''

  def get_firmware_version(self)
    '''!
      @brief  get firmware version
      @return  Return  firmware version
    '''

  def get_pid_vid(self)
    '''!
      @brief  Get the PID and VID of the device.
      @return Return true if the data is obtained correctly, false if failed or incorrect data.
    '''


  def get_rainfall(self)
    '''!
      @brief Get cumulative rainfall
      @return Cumulative rainfall value
    '''

  def get_rainfall_time(self,hour)
    '''!
      @brief   Get the cumulative rainfall within the specified time
      @param hour Specified time (valid range is 1-24 hours)
      @return Cumulative rainfall
    '''

  def get_raw_data(self)
    '''!
      @brief Get raw data
      @return Number of tipping bucket counts, unit: count
    '''

  def set_rain_accumulated_value(self,value)
    '''!
      @brief Set the rainfall accumulation value
      @param value Rainfall accumulation value, in millimeters
      @return Returns 0 if setting succeeded, other values if setting failed
    '''

  def get_sensor_working_time(self)
    '''!
      @brief Obtain the sensor working time
      @return Working time of the sensor, in hours
    '''
```

## Compatibility

* RaspberryPi Version

| Board        | Work Well | Work Wrong | Untested | Remarks |
| ------------ | :-------: | :--------: | :------: | ------- |
| Raspberry Pi2 |           |            |    √     |         |
| Raspberry Pi3 |           |            |    √     |         |
| Raspberry Pi4 |       √   |            |          |         |

* Python Version

| Python  | Work Well | Work Wrong | Untested | Remarks |
| ------- | :-------: | :--------: | :------: | ------- |
| Python2 |     √     |            |          |         |
| Python3 |     √     |            |          |         |

## History

- 2023/02/28 - Version 1.0.0 released.

## Credits

Written by fary(feng.yang@dfrobot.com), 2023. (Welcome to our [website](https://www.dfrobot.com/))
