# -*- coding: utf-8 -*
'''!
  @file  read_data.py
  @brief Get the raw data, which is the tipping bucket count of rainfall, in units of counts.
  @copyright   Copyright (c) 2021 DFRobot Co.Ltd (http://www.dfrobot.com)
  @license     The MIT License (MIT)
  @author      [fary](feng.yang@dfrobot.com)
  @version     V1.0
  @date        2023-01-28
  @url         https://github.com/DFRobor/DFRobot_RainfallSensor
'''
from __future__ import print_function
import sys
sys.path.append("../")
import time

from DFRobot_RainfallSensor import *

#sensor=DFRobot_RainfallSensor_UART()
sensor=DFRobot_RainfallSensor_I2C()

def setup():
  while (sensor.begin() == False):
    print("Sensor initialize failed!!")
    time.sleep(1)
  print("Sensor  initialize success!!")

  print("Version: "+ sensor.get_firmware_version())
  print("vid: %#x"%(sensor.vid))
  print("pid: %#x"%(sensor.pid))

  #Get the raw data, which is the tipping bucket count of rainfall, in units of counts.
  #sensor.set_rain_accumulated_value(0.2794)
def loop():
  #Get the sensor operating time in units of hours.
  workingtime = sensor.get_sensor_working_time()
  #Get the sensor operating time in units of hours.
  rainfall=sensor.get_rainfall()
  #Here is an example function that calculates the cumulative rainfall in a specified hour of the system. The function takes an optional argument, which can be any value between 1 and 24.
  one_hour_rainfall=sensor.get_rainfall_time(1)

  #Get the raw data, which is the tipping bucket count of rainfall, in units of counts.
  rainfall_raw=sensor.get_raw_data()

  print("workingtime : %f H"%(workingtime))
  print("rainfall : %f mm"%(rainfall))
  print("The amount of rain in one hour: %f mm"%(one_hour_rainfall))
  print("Original value collected by rain gauge %d"%(rainfall_raw))
  print("--------------------------------------------------------------------")
  time.sleep(1)

if __name__ == "__main__":
  setup()
  while True:
    loop()
