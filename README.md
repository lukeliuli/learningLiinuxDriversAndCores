# learningLiinuxDriversAndCores
1. helloworld1：gcc生成可执行文件
2. testCharDevNull1：最简单的字符驱动。testCharLed1：最简单的字符驱动，加入GPIO电路LED灯，硬件配置只适合树莓派3B
3. testHello1：最简单的驱动
4. testPlatformDriver1：最简单的platform驱动
5. testMiscDriver1:最简单的混杂驱动
6. testI2C_simpleMPU6050:最简单的MPU6050驱动,包括python smbus直接读取I2C的程序。必须先insmod mpu6050drv.ko 然后insmod mpu6050dev.ko
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

101. 现有树莓派3B，通过升级已经支持U盘启动。4b 没有成功 https://blog.csdn.net/qq_38685043/article/details/119650171  
102. 注意驱动编译时，make文件要修改内核源代码
103. sudo adduser cekong01 ; sudo usermod -aG sudo cekong01 
104. git clone  --depth=1 -b 1.20211029 https://github.com/raspberrypi/linux.git #获得版本为5.10.63-v7+的树莓派内核源代码

105. sudo apt install raspberrypi-kernel-headers=1.20211029;sudo sync

# 使用QEMU模拟树莓派3B
0. 下载qemu的最新版本(现在使用6.2.0)  
1. window版本将qemu路径加入PATH，linux版本make install  
2. 下载最新版本的树莓派系统镜像(现在使用2021-10-31版本的精简无界面镜像(lite版本)，内核版本5.10.64-v7+)  
3. 用winrar或者7z，获得镜像中的bcm2710-rpi-3-b.dtb,以及kernel8.img  
   这条有错误。如果要进行驱动开发必须自己编译内核，采用kernel8
4. 采用qemuRaspi中的raspWinOK_64里面的命令行(linux也一样进行运行)
5. QEMU只能模拟64位的树莓派3B,也就是说 KERNELS=kernal8，以及只能交叉编译生成ko,然后上传到服务器中  

注意    
1. 可以采用-nographic进行运行，但是不建议使用    
2. 默认命令中有使用USB+ISO进行文件在主机给虚拟机之间进行文件传输  
3. 如何用monitor进行USB设备的动态加载，用界面方式最快。这里用ISO制作文件(用ultraISO文件进行编辑比较简单)，并采用界面monitor进行文件的动态加载 基本命令如下    
  drive_add 0 id=d1, if=none,format=raw,media=cdrom,file=./myData.iso    
  device_add usb-storage,id=d1,drive=d1  
  device_del d1  
4. SD卡扩容、格式化、并加入了文件系统如下  
4.1  qemu resize  
4.2  fdisk ,见https://wenku.baidu.com/view/e373afd0e45c3b3567ec8bb3.html  
4.3  sudo resize2fs /dev/mmcblk0p2  
5. 很多细节屏蔽，例如ARM平台下的网络设备现阶段只能用usb-net。例如树莓派3b的qemu的核是kernel8.img而不是kernel7.img(kernel7.img无法启动)
6. 整个系统已经在我给出的云主机实现，用户名和密码一样  



-------------------------------------------------------------------------
1. 服务器
服务器1的地址为lukeliuli.xicp.net，端口为22965，用户名pi，密码123456。采用ssh登录，命令行形式为 ssh -p 22965 pi@lukeliuli.xicp.net （不稳定）
服务器2的地址为101.35.92.4，端口为5522，用户名pi，密码123456。采用ssh登录，命令行形式为 ssh -p 5522 pi@101.35.92.4 (主力) 
2. ssh进入服务器后建立自己的用户名，并加入sudo用户。具体命令见github上面的代码库
3. 现在这个版本是5.10.63(不是最新版本。最新树莓派版本为5.10.88)。所以采用最新版本的交叉编译无法在这个树莓派上运行。
   如何进行5.10.63版本的树莓派3b的交叉编译，见见github上面的代码库
4. 这个是模拟树莓派不是真实的硬件。模拟方法基于qemu,完全可以在自己电脑上搭建。具体方法实验课上会演示和看github上面的代码库
5. 系统慢，但是省钱。