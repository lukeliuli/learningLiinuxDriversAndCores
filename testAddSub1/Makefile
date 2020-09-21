
obj-m := helloAddSub.o addsub.o
KDIR := /home/pi/codes/kernel/linux
PWD := $(shell pwd)
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	rm -rf *.o *~ core .depend *.cmd *.ko *.mod.c *.mod .tmp_versions *.markers *.order *.symvers

