# learningLiinuxDriversAndCores
1. helloworld1：gcc生成可执行文件
2. testCharLed1：最简单的字符驱动 
3. testHello1：最简单的驱动
4. testPlatformDriver1：最简单的platform驱动
5. testMiscDriver1:最简单的混杂驱动
6. testI2C_simple8653:最简单的I2C驱动
7. testUSBdrivers1:最简单的USB驱动，skel驱动，usbmouse驱动
8. testMyCh341: 失败因为USB CH340无法改USB的ID 不知道如何用CH340CFG.可以尝试直接删除CH341的驱动（最后实验再说）   
9. testCP210x: 简单的usbserial驱动。通过修改usb的ID。从cp210x.c修改（几乎没有改）
注意 
1. testRead.py是基于python的测试驱动read是否可以 
2. 程序都是在树莓派3b+运行的。如果在虚拟机linux运行,参考testHello1里面的linuxMakefile 
