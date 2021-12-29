obj-m :=led_misc.o
KDIR := /home/liuli/linux
#CC = $(CROSS_COMPILE)gcc
PWD := $(shell pwd)
all:
        make -C $(KDIR) M=$(PWD) modules
clean:
        make -C $(KDIR) M=$(PWD) clean


#ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-


