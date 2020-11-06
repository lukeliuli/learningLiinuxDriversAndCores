#just for TOF10120 and failed

import smbus
import time
 

bus = smbus.SMBus(1)

#EVICE_ADDRESS = 0x03;  #direct copy from i2cdetect   #7 bit address (will be left shifted to add the read write bit)
#set TOF101020 address 
# SET_ADDR_REG = 0x0f;
# ADDR_VAL = 0x0e<<1;#0x06 #7 bit address (will be left shifted to add the read write bit
# bus.write_byte_data(DEVICE_ADDRESS, SET_ADDR_REG, ADDR_VAL)
# time.sleep(0.1)

DEVICE_ADDRESS = 0x0e;
#set TOF101020 I2C or UART mode address 
# DEVICE_ADDRESS2 = DEVICE_ADDRESS;
# I2CUART_REG = 0x09;
# I2C_MODE= 0x01 
# UART_MODE = 0x00;
# bus.write_byte_data(DEVICE_ADDRESS2,I2CUART_REG,UART_MODE)
# time.sleep(0.1)

# MODE = bus.read_byte_data(DEVICE_ADDRESS2,I2CUART_REG)
# print(MODE)
# time.sleep(0.1)

#get TOF101020 address 
# DEVICE_ADDRESS = 0x0e;
# GET_ADDR_REG = 0x0f;
# addrVal = bus.read_byte_data(DEVICE_ADDRESS, GET_ADDR_REG)
# print(addrVal)
# time.sleep(0.1)


GET_DIST_REG1 = 0x00;
distVal1 = bus.read_byte_data(DEVICE_ADDRESS, GET_DIST_REG1)
print(distVal1)
time.sleep(0.1)

#read data
# GET_DISTMODE_REG = 0x08;
# distMode = bus.read_byte_data(DEVICE_ADDRESS, GET_DISTMODE_REG)
# time.sleep(0.1)
# print(distMode)




