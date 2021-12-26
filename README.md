# learningLiinuxDriversAndCores
1. helloworld1：gcc生成可执行文件
2. testCharLed1：最简单的字符驱动 
3. testHello1：最简单的驱动
4. testPlatformDriver1：最简单的platform驱动
5. testMiscDriver1:最简单的混杂驱动
6. testI2C_simpleMPU6050:最简单的MPU6050驱动,包括python smbus直接读取I2C的程序
7. testCP210x: 简单的usbserial驱动。通过修改usb的ID。从cp210x.c修改（几乎没有改）
－－

8. testI2C_simple8653:最简单的I2C驱动
9. testUSBdrivers1:最简单的USB驱动，skel驱动，usbmouse驱动
10. testMyCh341: 失败因为USB CH340无法改USB的ID 不知道如何用CH340CFG.可以尝试直接删除CH341的驱动（最后实验再说）   
9. testCP210xPi: 简单的usbserial驱动。通过修改usb的ID。从cp210x.c修改（几乎没有改）.不用。
注意 
1. testRead.py是基于python的测试驱动read是否可以 
2. 程序都是在树莓派3b+运行的。如果在虚拟机linux运行,参考testHello1里面的linuxMakefile 



# 编译内核
0. 向一个SD卡中烧写系统并准备好所有键盘鼠标和显示屏，并运行成功.  参考：https://www.raspberrypi.com/software/. 这一步骤老师已经准备好了。
1. 根据实验报告和实验提示编译内核。写嵌入式驱动程需要对应嵌入式平台的内核源代码，自己编译内核源代码过程中，必然下载源代码。如果实在不想编译内核，根据uname -r 获得对于版本的内核源代码也可以进行驱动开发
