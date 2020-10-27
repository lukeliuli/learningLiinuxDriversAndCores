#!/usr/bin/python
# -*- coding: UTF-8 -*-
fh = open("/dev/pi_Led", "r")
str1 = fh.read(10)
print("read /dev/pi_Led: ", str1,"\n")
# 关闭打开的文件
fh.close()
