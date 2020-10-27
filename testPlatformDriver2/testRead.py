#!/usr/bin/python
# -*- coding: UTF-8 -*-
fh = open("/dev/mypiled", "r")
str1 = fh.read(10)
print("read /dev/mypiled: ", str1,"\n")
# 关闭打开的文件
fh.close()
