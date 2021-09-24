 #include<stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <sys/ioctl.h>
 #include <sys/time.h>
 #include <fcntl.h>
 int main(int argc, char **argv)
 {
    int on;
    int led_no =0;
    int fd;
    int i;
 
    fd = open("/dev/pi_Led", 0);
    if (fd < 0) {
       fd = open("/dev/pi_Led", 0);
    }
    if (fd < 0) {
      printf("pen device led failed\n");
      perror("open device led");
      exit(1);
    }
    for(i=0;i<=20;i++){
      on = i%2;
      ioctl(fd, on, led_no);
      printf("led blinking \n");
      sleep(1);
    }
    close(fd);
    return 0;
 }
