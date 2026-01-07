# -*- coding:utf-8 -*-

'''
  # test.py
  #
  # @brief 测试发送命令
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
    print("read_coils_register:")
    modbus.read_coils_register(id = 0x20, reg = 0x00)
    print("read_discrete_inputs_register:")
    modbus.read_discrete_inputs_register(id = 0x20, reg = 0x00)
    print("read_discrete_inputs_register:")
    modbus.read_discrete_inputs_register(id = 0x20, reg = 0x00)
    print("read_coils_registers:")
    modbus.read_coils_registers(id = 0x20, reg = 0x00, reg_num = 1)
    print("read_discrete_inputs_registers:")
    modbus.read_discrete_inputs_registers(id = 0x20, reg = 0x00, reg_num = 1)
    print("read_holding_registers:")
    modbus.read_holding_registers(id = 0x20, reg = 0x00, size = 1)
    print("write_coils_registers:")
    modbus.write_coils_registers(id = 0x20, reg = 0x02, reg_num = 1, data = [0xFF])
    print("write_holding_registers:")
    modbus.write_holding_registers(id = 0x20, reg = 0x02, data = [0x00,0x20])
    print("write_coils_register:")
    modbus.write_coils_register(id = 0x20, reg = 0x02, flag = True)
    print("write_holding_register:")
    modbus.write_holding_register(id = 0x20, reg = 0x02, val = 0x20)

    print("\n")
    time.sleep(2)
