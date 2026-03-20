# -*- coding:utf-8 -*-

'''
  # modify_modbus_id.py
  #
  # @brief 修改modbus从机的设备ID。每个modbus从机都有唯一识别的设备ID号，范围0x0000~0x00F7(0~247),修改设备ID有2种方式：
  # @n 1: 不知道设备的ID地址，可以通过广播地址0x00修改从机的ID地址，此命令会将总线上所有的从机的地址都修改为设置的ID（用0x00修改地址时，总线上最好只接一个设备）
  # @n 2: 知道设备的ID地址，直接用ID修改
  # @n note：运行此demo必须知道设备的串口配置(波特率，数据位，校验位，停止位)
  #
  # @n connected
  # -----------------------------------------------------------------------------
  # sensor pin |             MCU                |         raspberry pi          |
  #     VCC    |            3.3V/5V             |            5V/3V3             |
  #     GND    |              GND               |             GND               |
  #     RX     |              TX                |          (BCM)14 TX           |
  #     TX     |              RX                |          (BCM)15 RX           |
  # -----------------------------------------------------------------------------
  #
  # @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
  # @licence     The MIT License (MIT)
  # @author [Arya](xue.peng@dfrobot.com)
  # @version  V1.0
  # @date  2021-07-16
  # @https://github.com/DFRobot/DFRobot_RTU
'''

import sys
import os
import time

sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
from DFRobot_RTU import *

modbus = DFRobot_RTU(9600, 8, 'N', 1)

if __name__ == "__main__":
  DEVICE_ID_REG = 0x02
  '''方法1：通过广播地址0x00将modbus从机的地址设置为0x10'''
  modbus.write_holding_register(id = 0x00, reg = DEVICE_ID_REG, val = 0x10)
  time.sleep(1)
  ret = modbus.read_holding_register(id = 0x10, reg = DEVICE_ID_REG)
  if ret == 0x10:
    print("new ID1 is %02X"%ret)
    '''方法2：已知从机的地址为0x10，将其修改为0x20'''
    modbus.write_holding_register(id = 0x10, reg = DEVICE_ID_REG, val = 0x20)
    time.sleep(1)
    ret = modbus.read_holding_register(id = 0x20, reg = DEVICE_ID_REG)
    print("new ID2 is %02X"%ret)

