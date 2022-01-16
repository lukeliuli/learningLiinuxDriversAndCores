chcp 65001
qemu-system-aarch64 ^
-M raspi3b ^
-kernel .\pi3B-5.10.63-V8+\Kernel8.Image ^
-sd lite.img ^
-append "rw earlyprintk loglevel=8 console=ttyAMA0,115200 dwc_otg.lpm_enable=0 root=/dev/mmcblk0p2 rootwait" ^
-dtb .\pi3B-5.10.63-V8+\bcm2710-rpi-3-b.dtb  ^
-m 1024M ^
-nographic ^
-netdev user,id=net0,hostfwd=tcp::5522-:22 ^
-device usb-net,netdev=net0
pause
