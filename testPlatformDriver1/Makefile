obj-m :=led_dev.o led_drv.o
KDIR := /home/pi/codes/kernel/linux
PWD := $(shell pwd)
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean


