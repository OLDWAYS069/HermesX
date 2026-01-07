# DFRobot_Atmospherlum
* [English Version](./README.md)

This is a Skylark driver library that can be used to get data from the Skylark sensor.


![Product Image](./resources/images/EDU0157.png)


## Product Link (https://www.dfrobot.com)

    SKU：EDU0157

## Table of Contents

  * [summary](#summary)
  * [installation](#installation)
  * [methods](#methods)
  * [compatibility](#compatibility)
  * [history](#history)
  * [credits](#credits)

## Summary

  * Obtain data collected by larks

## Installation

Before using the library, first download the library file, paste it into the \Arduino\libraries directory, then open the examples folder and run the demo in that folder.

## Methods

```python
def begin(self):
    '''!
      @brief Initialize the SCI acquisition module, mainly used for initializing the communication interface
      @return int Initialization status
      @n       0      Initialization successful
      @n      others  Initialization failed
    '''

  def get_value(self, keys):
    '''!
      @brief Get sensor data
      @param keys  Data to be obtained
      @return Returns the acquired data
    '''

  def get_unit(self, keys):
    '''!
      @brief Get data units
      @param keys  Data for which units need to be obtained
      @return Returns the obtained units
    '''

  def get_information(self, state):
    '''!
      @brief Get all data
      @param state true: include timestamp, false: do not include timestamp
      @return String Returns all the acquired data
    '''

  def set_time(self, year, month, day,hour, minute, second):
    '''!
      @brief Set the RTC time
      @param year Year
      @param month Month
      @param day Day
      @param hour Hour
      @param minute Minute
      @param second Second
    '''

  def get_time_stamp(self):
    '''!
      @brief Get the RTC time
    '''

  
```

## Compatibility

* RaspberryPi Version

| Board        | Work Well | Work Wrong | Untested | Remarks |
| ------------ | :-------: | :--------: | :------: | ------- |
| RaspberryPi2 |           |            |    √     |         |
| RaspberryPi3 |     √     |            |          |         |
| RaspberryPi4 |           |            |     √    |         |

* Python Version

| Python  | Work Well | Work Wrong | Untested | Remarks |
| ------- | :-------: | :--------: | :------: | ------- |
| Python2 |     √     |            |          |         |
| Python3 |     √     |            |          |         |

## History

- 2023-06-8 - Version 1.0.0 released.

## Credits

Written by TangJie(jie.tang@dfrobot.com), 2021. (Welcome to our [website](https://www.dfrobot.com/))





