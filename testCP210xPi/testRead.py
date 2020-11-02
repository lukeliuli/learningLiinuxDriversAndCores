#!/usr/bin/python
# -*- coding: UTF-8 -*-
fh = open("/dev/ttyUSB0", "r")
str1 = fh.read(10)
print("读取的字符串是 : ", str1,"\n")
# 关闭打开的文件
fh.close()
