obj-m :=hello.o
#For pi ARM
KDIR := /home/pi/linux
#For linux X86
#KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	rm -rf *.o *~ core .depend *.cmd *.ko *.mod.c *.mod .tmp_versions *.markers *.order *.symvers



