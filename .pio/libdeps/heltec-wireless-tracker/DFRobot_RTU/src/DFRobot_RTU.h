/*!
 * @file DFRobot_RTU.h
 * @brief Modbus RTU libary for Arduino. 
 *
 * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @licence     The MIT License (MIT)
 * @author [Arya](xue.peng@dfrobot.com)
 * @version  V1.0
 * @date  2021-07-16
 * @https://github.com/DFRobot/DFRobot_RTU
 */
#ifndef __DFRobot_RTU_H
#define __DFRobot_RTU_H

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include<Stream.h>

//Define RTU_DBG, change 0 to 1 open the RTU_DBG, 1 to 0 to close.  
#if 0
#define RTU_DBG(...) {Serial.print("["); Serial.print(__FUNCTION__); Serial.print("(): "); Serial.print(__LINE__); Serial.print(" ] "); Serial.println(__VA_ARGS__);}
#else
#define RTU_DBG(...)
#endif


#ifndef RTU_BROADCAST_ADDRESS
#define RTU_BROADCAST_ADDRESS                      0x00 /**<modbus RTU协议的广播地址为0x00*/
#endif

class DFRobot_RTU{
protected:
typedef struct{
  uint16_t len;
  uint8_t id;
  uint8_t cmd;
  uint8_t payload[0];
  uint16_t cs;
}__attribute__ ((packed)) sRtuPacketHeader_t, *pRtuPacketHeader_t;

typedef enum{
  eRTU_EXCEPTION_ILLEGAL_FUNCTION = 0x01,
  eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS,
  eRTU_EXCEPTION_ILLEGAL_DATA_VALUE,
  eRTU_EXCEPTION_SLAVE_FAILURE,
  eRTU_EXCEPTION_CRC_ERROR = 0x08,
  eRTU_RECV_ERROR,
  eRTU_MEMORY_ERROR,
  eRTU_ID_ERROR
}eRtuStatusExceptionCode_t;

typedef enum{
  eCMD_READ_COILS = 0x01,
  eCMD_READ_DISCRETE = 0x02,
  eCMD_READ_HOLDING  = 0x03,
  eCMD_READ_INPUT  = 0x04,
  eCMD_WRITE_COILS = 0x05,
  eCMD_WRITE_HOLDING  = 0x06,
  eCMD_WRITE_MULTI_COILS = 0x0F,
  eCMD_WRITE_MULTI_HOLDING  = 0x10
}eFunctionCommand_t;

  void clearRecvBuffer();
  uint16_t calculateCRC(uint8_t *data, uint8_t len);
  pRtuPacketHeader_t packed(uint8_t id, eFunctionCommand_t cmd, void *data, uint16_t size);
  pRtuPacketHeader_t packed(uint8_t id, uint8_t cmd, void *data, uint16_t size);
  void sendPackage(pRtuPacketHeader_t header);
  pRtuPacketHeader_t recvAndParsePackage(uint8_t id, uint8_t cmd, uint16_t data, uint8_t *error);
public:
/**
 * @brief DFRobot_RTU abstract class constructor. Construct serial port.
 * @param s:  The class pointer object of Abstract class， here you can fill in the pointer to the serial port object.
 * @param dePin: RS485 flow control, pull low to receive, pull high to send.
 */
  DFRobot_RTU(Stream *s,int dePin);
  DFRobot_RTU(Stream *s);
  DFRobot_RTU();
  ~DFRobot_RTU(){}

/**
 * @brief Set receive timeout time, unit ms.
 * @param timeout:  receive timeout time, unit ms, default 100mss.
 */
  void setTimeoutTimeMs(uint32_t timeout = 100);

/**
 * @brief Read a coils Register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Coils register address.
 * @return Return the value of the coils register value.
 * @n      true: The value of the coils register value is 1.
 * @n      false: The value of the coils register value is 0.
 */
  bool readCoilsRegister(uint8_t id, uint16_t reg);
/**
 * @brief Read a discrete input register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @return Return the value of the coils register value.
 * @n      true: The value of the coils register value is 1.
 * @n      false: The value of the coils register value is 0.
 */
  bool readDiscreteInputsRegister(uint8_t id, uint16_t reg);
/**
 * @brief Read a holding Register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Holding register address.
 * @return Return the value of the holding register value.
 */
  uint16_t readHoldingRegister(uint8_t id, uint16_t reg);

/**
 * @brief Read a input Register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: input register address.
 * @return Return the value of the input register value.
 */
  uint16_t readInputRegister(uint8_t id, uint16_t reg);
/**
 * @brief Write a coils Register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Coils register address.
 * @param flag: The value of the register value which will be write, 0 ro 1.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
 * @n      9 or eRTU_RECV_ERROR:  Receive packet error.
 * @n      10 or eRTU_MEMORY_ERROR: Memory error.
 * @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
 */
  uint8_t writeCoilsRegister(uint8_t id, uint16_t reg, bool flag);
/**
 * @brief Write a holding register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Holding register address.
 * @param val: The value of the register value which will be write.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
 * @n      9 or eRTU_RECV_ERROR:  Receive packet error.
 * @n      10 or eRTU_MEMORY_ERROR: Memory error.
 * @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
 */
  uint8_t writeHoldingRegister(uint8_t id, uint16_t reg, uint16_t val);

/**
 * @brief Read multiple coils Register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Coils register address.
 * @param regNum: Number of coils Register.
 * @param data: Storage register worth pointer.
 * @param size: Cache size of data.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
 * @n      9 or eRTU_RECV_ERROR:  Receive packet error.
 * @n      10 or eRTU_MEMORY_ERROR: Memory error.
 * @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
 */
  uint8_t readCoilsRegister(uint8_t id, uint16_t reg, uint16_t regNum, uint8_t *data, uint16_t size);
/**
 * @brief Read multiple discrete inputs register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Discrete inputs register. address.
 * @param regNum: Number of coils Register.
 * @param data: Storage register worth pointer.
 * @param size: Cache size of data.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
 * @n      9 or eRTU_RECV_ERROR:  Receive packet error.
 * @n      10 or eRTU_MEMORY_ERROR: Memory error.
 * @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
 */
  uint8_t readDiscreteInputsRegister(uint8_t id, uint16_t reg, uint16_t regNum, uint8_t *data, uint16_t size);
/**
 * @brief Read multiple Holding register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Holding register.
 * @param data: Storage register worth pointer.
 * @param size: Cache size.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
 * @n      9 or eRTU_RECV_ERROR:  Receive packet error.
 * @n      10 or eRTU_MEMORY_ERROR: Memory error.
 * @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
 */
  uint8_t readHoldingRegister(uint8_t id, uint16_t reg, void *data, uint16_t size);

/**
 * @brief Read multiple Input register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Input register.
 * @param data: Storage register worth pointer.
 * @param regNum: register numbers.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
 * @n      9 or eRTU_RECV_ERROR:  Receive packet error.
 * @n      10 or eRTU_MEMORY_ERROR: Memory error.
 * @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
 */
  uint8_t readInputRegister(uint8_t id, uint16_t reg, void *data, uint16_t size);
/**
 * @brief Read multiple Holding register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Holding register.
 * @param data: Storage register worth pointer.
 * @param regNum: register numbers.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
 * @n      9 or eRTU_RECV_ERROR:  Receive packet error.
 * @n      10 or eRTU_MEMORY_ERROR: Memory error.
 * @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
 */
  uint8_t readHoldingRegister(uint8_t id, uint16_t reg, uint16_t *data, uint16_t regNum);

/**
 * @brief Read multiple Input register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Input register.
 * @param data: Storage register worth pointer.
 * @param regNum: register numbers.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
 * @n      9 or eRTU_RECV_ERROR:  Receive packet error.
 * @n      10 or eRTU_MEMORY_ERROR: Memory error.
 * @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
 */
  uint8_t readInputRegister(uint8_t id, uint16_t reg, uint16_t *data, uint16_t regNum);
  
/**
 * @brief Write multiple coils Register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Coils register address.
 * @param regNum: Numbers of Coils register.
 * @param data: Storage register worth pointer.
 * @param size: Cache size.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
 * @n      9 or eRTU_RECV_ERROR:  Receive packet error.
 * @n      10 or eRTU_MEMORY_ERROR: Memory error.
 * @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
 */
  uint8_t writeCoilsRegister(uint8_t id, uint16_t reg, uint16_t regNum, uint8_t *data, uint16_t size);
/**
 * @brief Write multiple Holding Register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Holding register address.
 * @param data: Storage register worth pointer.
 * @param size: Cache size.
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
 * @n      9 or eRTU_RECV_ERROR:  Receive packet error.
 * @n      10 or eRTU_MEMORY_ERROR: Memory error.
 * @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
 */
  uint8_t writeHoldingRegister(uint8_t id, uint16_t reg, void *data, uint16_t size);
/**
 * @brief Write multiple Holding Register.
 * @param id:  modbus device ID. Range: 0x00 ~ 0xF7(0~247), 0x00 is broadcasr address, which all slaves will process broadcast packets, 
 * @n          but will not answer.
 * @param reg: Holding register address.
 * @param data: Storage register worth pointer.
 * @param regNum: Number of coils Register..
 * @return Exception code:
 * @n      0 : sucess.
 * @n      1 or eRTU_EXCEPTION_ILLEGAL_FUNCTION : Illegal function.
 * @n      2 or eRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS: Illegal data address.
 * @n      3 or eRTU_EXCEPTION_ILLEGAL_DATA_VALUE:  Illegal data value.
 * @n      4 or eRTU_EXCEPTION_SLAVE_FAILURE:  Slave failure.
 * @n      8 or eRTU_EXCEPTION_CRC_ERROR:  CRC check error.
 * @n      9 or eRTU_RECV_ERROR:  Receive packet error.
 * @n      10 or eRTU_MEMORY_ERROR: Memory error.
 * @n      11 or eRTU_ID_ERROR: Broadcasr address or error ID
 */
  uint8_t writeHoldingRegister(uint8_t id, uint16_t reg, uint16_t *data, uint16_t regNum);

  
private:
  uint32_t _timeout;
  Stream *_s;
  int _dePin;
};
#endif
