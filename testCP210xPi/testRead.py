#!/usr/bin/python
# -*- coding: UTF-8 -*-

import serial
from time import sleep
 
ser = serial.Serial('/dev/ttyUSB0', 9600, timeout=0.5) 
 
print(ser.port)
print(ser.baudrate)
 
def recv(serial):  
  while True:  
    data =serial.read(42)  
    if data == '':  
      continue
    else:
      break
    sleep(0.02) 
  return data 
 
while True:  
  data =recv(ser) 
  if data != '': 
    print(data)
    
