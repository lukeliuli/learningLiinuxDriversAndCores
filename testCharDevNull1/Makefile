

	
obj-m :=led.o
KDIR := /home/pi/codes/kernel/linux

#For pi ARM
#KDIR := /home/pi/codes/kernel/linux
#For linux X86
KDIR := /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean


