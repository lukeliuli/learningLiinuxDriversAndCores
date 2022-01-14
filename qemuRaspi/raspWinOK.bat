chcp 65001
qemu-system-aarch64 ^
-M raspi3b ^
-kernel kernel8.img ^
-sd lite.img ^
-append "rw earlyprintk loglevel=8 console=ttyAMA0,115200 dwc_otg.lpm_enable=0 root=/dev/mmcblk0p2 rootwait" ^
-dtb bcm2710-rpi-3-b.dtb  ^
-m 1024M ^
-netdev user,id=net0,hostfwd=tcp::5522-:22 ^
-device usb-net,netdev=net0 

pause
