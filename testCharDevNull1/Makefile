

	
obj-m :=led.o
KDIR := /home/pi/codes/kernel/linux


#For pi ARM
#KDIR := /home/pi/linux
#For linux X86, 一定可能性，这个在树莓派平台也能用，不需要编译内核
#KDIR := /lib/modules/$(shell uname -r)/build
#For cross 
KDIR :=/home/pi/linux
ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)gcc
#命令行中运行 make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean





