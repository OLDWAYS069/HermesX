# DFRobot_RTU

* [English Version](./README.md)

这是一个基于Modbus RTU协议的Arduino modbus库，它支持以下几种modbus协议命令：<br>
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
这是DFRobot基于modbus RTU协议为Arduino平台移植的modbus库。<br>

## Connected
Hardware conneted table 

 Sensor      |               MCU                 |
------------ | :-------------------------------: |
VCC          |                5V                 |
GND          |                GND                |
RX           |connected to the UART TX pin of MCU|
TX           |connected to the UART RX pin of MCU|

## Installation

To use this library, first download the library file, paste it into the \Arduino\libraries directory, then open the examples folder and run the demo in the folder.

## Methods

```C++
/**
 * @brief DFRobot_RTU构造函数. 传递串口指针.
 * @param s:  串口抽象类指针。
 * @param dePin RS485流控引脚,拉低接收，拉高发送
 */
DFRobot_RTU(Stream *s,int dePin);
DFRobot_RTU(Stream *s);
~DFRobot_RTU()

/**
 * @brief 设置串口接收超时时间，单位ms.
 * @param timeout:  串口接收超时时间参数，单位ms，默认100ms
 */
void setTimeoutTimeMs(uint32_t timeout = 100);

/**
 * @brief 读取线圈寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 线圈寄存器地址.
 * @return 返回线圈寄存器的值。
 * @n      true: 线圈寄存器的值为1
 * @n      false: 线圈寄存器的值为0
 */
  bool readCoilsRegister(uint8_t id, uint16_t reg);

/**
 * @brief 读取离散输入寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 离散输入寄存器地址.
 * @return 返回离散输入寄存器的值。
 */
uint16_t readDiscreteInputsRegister(uint8_t id, uint16_t reg);

/**
 * @brief 读取保持寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 保持寄存器地址.
 * @return 返回保持寄存器的值。
 */
uint16_t readHoldingRegister(uint8_t id, uint16_t reg);

/**
 * @brief 读取输入寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 输入寄存器地址.
 * @return 返回输入寄存器的值。
 */
  uint16_t readInputRegister(uint8_t id, uint16_t reg);

/**
 * @brief 写单个线圈寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 线圈寄存器地址.
 * @param flag: 要写入的线圈寄存器的值,0 或 1。
 * @return 返回写线圈寄存起的值, 0 或 1。
 */
  uint8_t writeCoilsRegister(uint8_t id, uint16_t reg, bool flag);
  
/**
 * @brief 写单个保持寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 保持寄存器地址.
 * @param val: 要写入的保持寄存器的值。
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
 * @n      9 or eRTU_RECV_ERROR:  接收包错误.
 * @n      10 or eRTU_MEMORY_ERROR: 内存错误.
 * @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
 */
uint8_t writeHoldingRegister(uint8_t id, uint16_t reg, uint16_t val);

/**
 * @brief 读多个线圈寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 读线圈寄存器的起始地址.
 * @param regNum: 线圈寄存器的个数
 * @param data: 存储要读取的数据的指针.
 * @param size: 要读取的字节数.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
 * @n      9 or eRTU_RECV_ERROR:  接收包错误.
 * @n      10 or eRTU_MEMORY_ERROR: 内存错误.
 * @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
 */
uint8_t readCoilsRegister(uint8_t id, uint16_t reg, uint16_t regNum, uint8_t *data, uint16_t size);

/**
 * @brief 读多个离散输入寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 读离散输入寄存器的起始地址.
 * @param regNum: 离散寄存器的个数
 * @param data: 存储要读取的数据的指针.
 * @param size: 要读取的字节数.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
 * @n      9 or eRTU_RECV_ERROR:  接收包错误.
 * @n      10 or eRTU_MEMORY_ERROR: 内存错误.
 * @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
 */
uint8_t readDiscreteInputsRegister(uint8_t id, uint16_t reg, uint16_t regNum, uint8_t *data, uint16_t size);

/**
 * @brief 读多个保持寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 读保持寄存器的起始地址.
 * @param data: 存储要读取的数据的指针.
 * @param size: 要读取的字节数.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
 * @n      9 or eRTU_RECV_ERROR:  接收包错误.
 * @n      10 or eRTU_MEMORY_ERROR: 内存错误.
 * @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
 */
uint8_t readHoldingRegister(uint8_t id, uint16_t reg, void *data, uint16_t size);


/**
 * @brief 读多个输入寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 读输入寄存器的起始地址.
 * @param data: 存储要读取的数据的指针.
 * @param size: 要读取的字节数.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
 * @n      9 or eRTU_RECV_ERROR:  接收包错误.
 * @n      10 or eRTU_MEMORY_ERROR: 内存错误.
 * @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
 */
  uint8_t readInputRegister(uint8_t id, uint16_t reg, void *data, uint16_t size);

/**
 * @brief 读多个保持寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 读保持寄存器的起始地址.
 * @param data: 存储要读取的数据的指针.
 * @param regNum: 表示要读取的寄存器的个数.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
 * @n      9 or eRTU_RECV_ERROR:  接收包错误.
 * @n      10 or eRTU_MEMORY_ERROR: 内存错误.
 * @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
 */
  uint8_t readHoldingRegister(uint8_t id, uint16_t reg, uint16_t *data, uint16_t regNum);

  /**
 * @brief 读多个输入寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 读输入寄存器的起始地址.
 * @param data: 存储要读取的数据的指针.
 * @param size: 要读取的字节数.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
 * @n      9 or eRTU_RECV_ERROR:  接收包错误.
 * @n      10 or eRTU_MEMORY_ERROR: 内存错误.
 * @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
 */
  uint8_t readInputRegister(uint8_t id, uint16_t reg, uint16_t *data, uint16_t size);

/**
 * @brief 写多个线圈寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 写线圈寄存器的起始地址.
 * @param regNum: 线圈寄存器的个数.
 * @param data: 存储要写入的数据的指针.
 * @param size: 要写入的字节数.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
 * @n      9 or eRTU_RECV_ERROR:  接收包错误.
 * @n      10 or eRTU_MEMORY_ERROR: 内存错误.
 * @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
 */
  uint8_t writeCoilsRegister(uint8_t id, uint16_t reg, uint16_t regNum, void *data, uint16_t size);

/**
 * @brief 写多个保持寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 写保持寄存器的起始地址.
 * @param data: 存储要写入的数据的指针.
 * @param size: 要写入的字节数.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
 * @n      9 or eRTU_RECV_ERROR:  接收包错误.
 * @n      10 or eRTU_MEMORY_ERROR: 内存错误.
 * @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
 */
uint8_t writeHoldingRegister(uint8_t id, uint16_t reg, void *data, uint16_t size);

/**
 * @brief 写多个保持寄存器的值。
 * @param id:  modbus 设备ID，范围0~0xF7(0~247)，其中0x00为广播地址，所有modbus从机都会处理广播包，但不会应答。
 * @param reg: 写保持寄存器的起始地址.
 * @param data: 存储要写入的数据的指针.
 * @param regNum: 表示要读取的寄存器的个数.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : 非法功能.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: 非法数据地址.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  非法数据值.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  从机故障.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC校验错误.
 * @n      9 or eRTU_RECV_ERROR:  接收包错误.
 * @n      10 or eRTU_MEMORY_ERROR: 内存错误.
 * @n      11 or eRTU_ID_ERROR:广播地址或错误ID(因为主机无法收到从机广播包的应答)
 */
  uint8_t writeHoldingRegister(uint8_t id, uint16_t reg, uint16_t *data, uint16_t regNum);
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
