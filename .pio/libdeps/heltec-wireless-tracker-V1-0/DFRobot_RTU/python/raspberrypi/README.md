# DFRobot_RTU

* [中文版](./README_CN.md)

Modbus RTU libary for raspberrypi. The supported modbus commands are as follows：<br>
* 0x01: Read one or multiple coils register;
* 0x02: Read one or multiple discrete inputs register;
* 0x03: Read one or multiple holding register;
* 0x04: Read one or multiple input register;
* 0x05: Write a coils register;
* 0x06: write a holding register;
* 0x0F: Write multiple coils register;
* 0x10: Write multiple holding register;
* support python2 and python3

![正反面svg效果图](https://github.com/Arya11111/DFRobot_MCP23017/blob/master/resources/images/SEN0245svg1.png)


## Product Link（链接到英文商城）
    
   
## Table of Contents

* [Summary](#summary)
* [Connected](#connected)
* [Installation](#installation)
* [Calibration](#calibration)
* [Methods](#methods)
* [Compatibility](#compatibility)
* [History](#history)
* [Credits](#credits)

## Summary
This is a modbus RTU libary  for Arduino by DFRobot.<br>

## Connected
Hardware conneted table 

Sensor      |               raspberrypi         
------------ | :-------------------------------: 
VCC          |                5V                 
GND          |                GND                
RX           |connected to the UART TX pin of MCU
TX           |connected to the UART RX pin of MCU

## Installation
To use this library, first download the library file, then open the examples folder and run the demo in the folder Proceed as follows:
* sudo git clone https://github.com/DFRobot/DFRobot_RTU
* cd python
* cd raspberrypi
* cd examples
* python demo_*
* python3 demo_*


## Methods

```C++
'''
  @brief Serial initialization.
  @param baud:  The UART baudrate of raspberry pi
  @param bits:  The UART data bits of raspberry pi
  @param parity:  The UART parity bits of raspberry pi
  @param stopbit:  The UART stopbit bits of raspberry pi.
'''
def __init__(self, baud, bits, parity, stopbit):

'''
  @brief Set receive timeout time, unit s.
  @param timeout:  receive timeout time, unit s, default 0.1s.
'''
def set_timout_time_s(self, timeout):

'''
  @brief Read a coils Register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Coils register address.
  @return Return the value of the coils register value.
  @n      True: The value of the coils register value is 1.
  @n      False: The value of the coils register value is 0.
'''
def read_coils_register(self, id, reg):
    
'''
  @brief Read a discrete input register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Discrete input register address.
  @return Return the value of the discrete input register value.
  @n      True: The value of the discrete input register value is 1.
  @n      False: The value of the discrete input register value is 0.
'''
def read_discrete_inputs_register(self, id, reg):

'''
  @brief Read a holding Register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Holding register address.
  @return Return the value of the holding register value.
'''
def read_holding_register(self, id, reg):

'''
  @brief Read a input Register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Input register address.
  @return Return the value of the holding register value.
'''
def read_input_register(self, id, reg):

'''
  @brief Write a coils Register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Coils register address.
  @param flag: The value of the register value which will be write, True ro False.
  @return Exception code:
  @n      0 : sucess.
  @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
  @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
  @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
  @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
  @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
  @n      9 or eRTU_RECV_ERROR:  Receive packet error.
  @n      10 or eRTU_MEMORY_ERROR: Memory error.
  @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
'''
def write_coils_register(self, id, reg, flag):

'''
  @brief Write a holding register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Holding register address.
  @param val: The value of the register value which will be write.
  @return Exception code:
  @n      0 : sucess.
  @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
  @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
  @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
  @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
  @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
  @n      9 or eRTU_RECV_ERROR:  Receive packet error.
  @n      10 or eRTU_MEMORY_ERROR: Memory error.
  @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
'''
def write_holding_register(self, id, reg, val):

'''
  @brief Read multiple coils Register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Read the start address of the coil register.
  @param reg_num: Number of coils Register.
  @return list: format as follow:
  @n      list[0]: Exception code:
  @n               0 : sucess.
  @n               1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
  @n               2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
  @n               3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
  @n               4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
  @n               8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
  @n               9 or eRTU_RECV_ERROR:  Receive packet error.
  @n               10 or eRTU_MEMORY_ERROR: Memory error.
  @n               11 or eRTU_ID_ERROR: Broadcasr address or error ID
  @n      list[1:]: The value of the coil register list.
'''
def read_coils_registers(self, id, reg, reg_num):

'''
  @brief Read multiple discrete inputs register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Read the start address of the discrete inputs register.
  @param reg_num: Number of coils Register.
  @return list: format as follow:
  @n      list[0]: Exception code:
  @n               0 : sucess.
  @n               1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
  @n               2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
  @n               3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
  @n               4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
  @n               8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
  @n               9 or eRTU_RECV_ERROR:  Receive packet error.
  @n               10 or eRTU_MEMORY_ERROR: Memory error.
  @n               11 or eRTU_ID_ERROR: Broadcasr address or error ID
  @n      list[1:]: The value list of the discrete inputs register.
'''
def read_discrete_inputs_registers(self, id, reg, reg_num):

'''
  @brief Read multiple holding register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Read the start address of the holding register.
  @param len: Number of read holding register.
  @return list: format as follow:
  @n      list[0]: Exception code:
  @n               0 : sucess.
  @n               1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
  @n               2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
  @n               3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
  @n               4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
  @n               8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
  @n               9 or eRTU_RECV_ERROR:  Receive packet error.
  @n               10 or eRTU_MEMORY_ERROR: Memory error.
  @n               11 or eRTU_ID_ERROR: Broadcasr address or error ID
  @n      list[1:]: The value list of the holding register.
'''
def read_holding_registers(self, id, reg, size):

'''
  @brief Read multiple input register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Read the start address of the input register.
  @param size: Number of read input register.
  @return list: format as follow:
  @n      list[0]: Exception code:
  @n               0 : sucess.
  @n               1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
  @n               2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
  @n               3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
  @n               4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
  @n               8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
  @n               9 or eRTU_RECV_ERROR:  Receive packet error.
  @n               10 or eRTU_MEMORY_ERROR: Memory error.
  @n               11 or eRTU_ID_ERROR: Broadcasr address or error ID
  @n      list[1:]: The value list of the input register.
'''
def read_input_registers(self, id, reg, size):
    
'''
  @brief Write multiple coils Register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Write the start address of the coils register.
  @param reg_num: Number of coils Register.
  @param data: The list of storage coils Registers' value which will be write.
  @return Exception code:
  @n      0 : sucess.
  @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
  @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
  @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
  @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
  @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
  @n      9 or eRTU_RECV_ERROR:  Receive packet error.
  @n      10 or eRTU_MEMORY_ERROR: Memory error.
  @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
'''
def write_coils_registers(self, id, reg, reg_num, data):

'''
  @brief Write multiple holding Register.
  @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
  @n          but will not answer.
  @param reg: Write the start address of the holding register.
  @param data: The list of storage holding Registers' value which will be write.
  @return Exception code:
  @n      0 : sucess.
  @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
  @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
  @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
  @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
  @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
  @n      9 or eRTU_RECV_ERROR:  Receive packet error.
  @n      10 or eRTU_MEMORY_ERROR: Memory error.
  @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
'''
def write_holding_registers(self, id, reg, data):

```

## Compatibility

MCU                | SoftwareSerial | HardwareSerial |  IO   
------------------ | :----------: | :----------: | :---------: 
Arduino Uno        |      √       |      X       |      √       
Mega2560           |      √       |      √       |      √       
Leonardo           |      √       |      √       |      √       
ESP32              |      X       |      √       |      √       
ESP8266            |      √       |      X       |      √       
micro:bit          |      X       |      X       |      √       
FireBeetle M0      |      X       |      √       |      √       
raspberry          |      X       |      √       |      √       

## History

- Data 2021-07-18
- Version V1.0

## Credits

Written by(xue.peng@dfrobot.com), 2021. (Welcome to our [website](https://www.dfrobot.com/))





