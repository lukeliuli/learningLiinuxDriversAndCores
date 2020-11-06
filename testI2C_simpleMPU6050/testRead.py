#!/usr/bin/python
# -*- coding: UTF-8 -*-

import struct
fh = open("/dev/mpu6050", "r")
data = fh.read(24)
data2 = struct.unpack("iiiiii", data)
print(data2)
# 关闭打开的文件
fh.close()
