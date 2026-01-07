# -*- coding:utf-8 -*-

'''
  # scan_modbus_id.py
  #
  # @brief 扫描modbus总线上，所有串口配置为9600波特率，8位数据位，无校验位，1位停止位的modbus从机的地址。
  # @n modbus从机设备地址范围为1~247(0x01~0xF7),0为广播地址，所有modbus从机接受到广播包都会处理，但不会响应。
  # @n 一个modbus主机可以连多个modbus从机，在运行此demo之前，必须知道modbus从机的波特率，数据位，校验位，停止位等串口配置。
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
  while True:
    modbus_id = 1
    n_devices = 0
    print("Scanning...")
    while modbus_id < 248:
      ret = modbus.read_holding_register(modbus_id, DEVICE_ID_REG)
      if ret == modbus_id:
        print("modbus device found at address 0x%02X !"%modbus_id)
        n_devices += 1
      modbus_id += 1
    if n_devices == 0:
      print("No modbus devices found\n")
    else:
      print("done\n")
    time.sleep(1)
