# DFRobot_RTU

* [English Version](./README.md)

这是一个基于Modbus RTU协议的raspberry pi python modbus库，它支持以下几种modbus协议命令：<br>
* 0x01: 读一个或多个线圈寄存器；
* 0x02: 读一个或多个离散输入寄存器；
* 0x03: 读一个或多个保持寄存器；
* 0x04: 读一个或多个输入寄存器；
* 0x05: 写单个线圈寄存器；
* 0x06: 写单个保持寄存器；
* 0x0F: 写多个线圈寄存器；
* 0x10: 写多个保持寄存器；

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
这是DFRobot基于modbus RTU协议为raspberry pi 平台移植的python modbus库。<br>


## Connected
Hardware conneted table 

Sensor      |               raspberry pi         |
------------ | :-------------------------------: |
VCC          |                5V                 |
GND          |                GND                |
RX           |connected to the UART TX pin of MCU|
TX           |connected to the UART RX pin of MCU|

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
  @brief 树莓派串口通信配置.
  @param baud:  树莓派串口通信波特率参数
  @param bits:  树莓派串口通信数据位参数
  @param parity:  树莓派串口通信校验位参数
  @param stopbit:  树莓派串口通信停止位参数
'''
def __init__(self, baud, bits, parity, stopbit):

'''
  @brief 设置接收超时时间，单位s.
  @param timeout:  接收超时形参，单位秒，默认为0.1s
'''
def set_timout_time_s(self, timeout = 0.1):
    
'''
  @brief 读取线圈寄存器的值。
  @param id:   modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 线圈寄存器地址。
  @return 返回线圈寄存器的值。
  @n      True: 线圈寄存器的值为1
  @n      False: 线圈寄存器的值为0
'''
def read_coils_register(self, id, reg):
    
'''
  @brief 读取离散输入寄存器的值。
  @param id:   modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 离散输入寄存器地址。
  @return 返回离散输入寄存器的值。
  @n      True: 离散输入寄存器的值为1
  @n      False: 离散输入寄存器的值为0
'''
def read_discrete_inputs_register(self, id, reg):

'''
  @brief 读取保持寄存器的值。
  @param id:   modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 保持寄存器地址。
  @return 返回保持寄存器的值。
'''
def read_holding_register(self, id, reg):

'''
  @brief 读取输入寄存器的值。
  @param id:   modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 输入寄存器地址。
  @return 返回输入寄存器的值。
'''
def read_input_register(self, id, reg):

'''
  @brief 写单个线圈寄存器的值。
  @param id:   modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 线圈寄存器地址。
  @param flag: 将要被写入的线圈寄存器的值，True or False
  @return 返回写入的线圈寄存器的值
'''
def write_coils_register(self, id, reg, flag):

'''
  @brief 写单个保持寄存器的值。
  @param id:   modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 保持寄存器地址。
  @param val: 将要被写入的保持寄存器的值
  @return 返回写入的保持寄存器的值
'''
def write_holding_register(self, id, reg, val):

'''
  @brief 读取多个线圈寄存器的值。
  @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 读取线圈寄存器的起始地址。
  @param reg_num: 离散输入寄存器的个数
  @return 列表: 格式如下:
  @n      list[0]: Exception code:
  @n               0 : sucess.
  @n               1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
  @n               2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
  @n               3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
  @n               4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
  @n               8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
  @n               9 or eRTU_RECV_ERROR:  接收包错误.
  @n               10 or eRTU_MEMORY_ERROR: 内存错误.
  @n               11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
  @n      list[1:]: 读取的线圈寄存器的值.
'''
def read_coils_registers(self, id, reg, reg_num):

'''
  @brief 读取多个离散输入寄存器的值。
  @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 读取离散输入寄存器的起始地址。
  @param reg_num: 离散输入寄存器的个数
  @return 列表: 格式如下:
  @n      list[0]: Exception code:
  @n               0 : sucess.
  @n               1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
  @n               2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
  @n               3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
  @n               4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
  @n               8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
  @n               9 or eRTU_RECV_ERROR:  接收包错误.
  @n               10 or eRTU_MEMORY_ERROR: 内存错误.
  @n               11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
  @n      list[1:]: 读取的离散输入寄存器的值.
'''
def read_discrete_inputs_registers(self, id, reg, reg_num):

'''
  @brief 读取多个保持寄存器的值。
  @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 读取保持寄存器的起始地址。
  @param len: 读取保持寄存器的个数
  @return 列表: 格式如下:
  @n      list[0]: Exception code:
  @n               0 : sucess.
  @n               1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
  @n               2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
  @n               3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
  @n               4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
  @n               8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
  @n               9 or eRTU_RECV_ERROR:  接收包错误.
  @n               10 or eRTU_MEMORY_ERROR: 内存错误.
  @n               11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
  @n      list[1:]: 读取的保持寄存器的值.
'''
def read_holding_registers(self, id, reg, size):

'''
  @brief 读取多个输入寄存器的值。
  @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 读取输入寄存器的起始地址。
  @param len: 读取输入寄存器的个数
  @return 列表: 格式如下:
  @n      list[0]: Exception code:
  @n               0 : sucess.
  @n               1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
  @n               2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
  @n               3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
  @n               4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
  @n               8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
  @n               9 or eRTU_RECV_ERROR:  接收包错误.
  @n               10 or eRTU_MEMORY_ERROR: 内存错误.
  @n               11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
  @n      list[1:]: 读取的输入寄存器的值.
'''
def read_input_registers(self, id, reg, size):

'''
  @brief 写多个线圈寄存器的值。
  @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 写线圈寄存器的起始地址。
  @param reg_num: 线圈寄存器的个数.
  @param data: 将要被写入的线圈寄存器的列表
  @return Exception code:
  @n      0 : sucess.
  @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
  @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
  @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
  @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
  @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
  @n      9 or eRTU_RECV_ERROR:  接收包错误.
  @n      10 or eRTU_MEMORY_ERROR: 内存错误.
  @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
'''
def write_coils_registers(self, id, reg, reg_num, data):

'''
  @brief 写多个保持寄存器的值。
  @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
  @param reg: 写保持寄存器的起始地址。
  @param data: 将要被写入的保持寄存器的列表
  @return Exception code:
  @n      0 : sucess.
  @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
  @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
  @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
  @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
  @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
  @n      9 or eRTU_RECV_ERROR:  接收包错误.
  @n      10 or eRTU_MEMORY_ERROR: 内存错误.
  @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
'''
def write_holding_registers(self, id, reg, data):
```

## Compatibility

MCU                | SoftwareSerial | HardwareSerial |
------------------ | :----------: | :----------: |
Arduino Uno        |      √       |      X       |
Mega2560           |      √       |      √       |
Leonardo           |      √       |      √       |
ESP32              |      X       |      √       |
ESP8266            |      √       |      X       |
micro:bit          |      X       |      X       |
FireBeetle M0      |      X       |      √       |
raspberry          |      X       |      √       |

## History

- Data 2021-07-17
- Version V1.0

## Credits

Written by(xue.peng@dfrobot.com), 2021. (Welcome to our [website](https://www.dfrobot.com/))
