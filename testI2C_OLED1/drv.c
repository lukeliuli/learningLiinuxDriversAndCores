1 #include <linux/kernel.h>
  2 #include <linux/module.h>
  3 #include <linux/i2c.h>
  4 #include <linux/init.h>
  5 #include <linux/cdev.h>
  6 #include <linux/fs.h>
  7 #include <linux/device.h>
  8 #include <linux/slab.h>
  9 #include <linux/uaccess.h>
 10 
 11 #include "oledfont.h"
 12 
 13 //字符驱动
 14 struct _oled_cdev
 15 {
 16     struct cdev            *cdev;
 17     dev_t                  dev_num;
 18     struct class           *cls; 
 19     struct i2c_client      *client;
 20 
 21     unsigned char        size;
 22     unsigned char        buf[128*8];
 23 };
 24 
 25 static struct _oled_cdev *oled_cdev;
 26 
 27 static int oled_send_byte(unsigned char data, unsigned char cmd)
 28 {
 29     int ret;
 30 
 31     if(cmd)    //我们使用内核文档推荐的SMBus API
 32     {
 33         ret = i2c_smbus_write_byte_data(oled_cdev->client, 0x00, data);
 34     }
 35     else
 36     {
 37         ret = i2c_smbus_write_byte_data(oled_cdev->client, 0x40, data);
 38     }
 39 
 40     if(ret)
 41         return -EIO;
 42 
 43     return 0;
 44 }
 45 
 46 /* 关于OLED这部分代码，是从厂商提供的代码处移植过来，当然还没完全移植过来，有些目前没用到的函数没有移过来 */
 47 
 48 //OLED初始化
 49 static void oled_init(void)
 50 {
 51     oled_send_byte(0xAE, 1);    //display off
 52     oled_send_byte(0x00, 1);    //set low column address
 53     oled_send_byte(0x10, 1);    //set high column address
 54     oled_send_byte(0x40, 1);    //set start line address    
 55     oled_send_byte(0xB0, 1);    //set page address
 56     oled_send_byte(0x81, 1);    //contract control
 57     oled_send_byte(0xFF, 1);    //128   
 58     oled_send_byte(0xA1, 1);    //set segment remap 
 59     oled_send_byte(0xA6, 1);    //normal / reverse
 60     oled_send_byte(0xA8, 1);    //set multiplex ratio(1 to 64)
 61     oled_send_byte(0x3F, 1);    //1/32 duty
 62     oled_send_byte(0xC8, 1);    //Com scan direction
 63     oled_send_byte(0xD3, 1);    //set display offset
 64     oled_send_byte(0x00, 1);
 65     
 66     oled_send_byte(0xD5, 1);    //set osc division
 67     oled_send_byte(0x80, 1);
 68     
 69     oled_send_byte(0xD8, 1);    //set area color mode off
 70     oled_send_byte(0x05, 1);
 71     
 72     oled_send_byte(0xD9, 1);    //Set Pre-Charge Period
 73     oled_send_byte(0xF1, 1);
 74     
 75     oled_send_byte(0xDA, 1);    //set com pin configuartion
 76     oled_send_byte(0x12, 1);
 77     
 78     oled_send_byte(0xDB, 1);    //set Vcomh
 79     oled_send_byte(0x30, 1);
 80     
 81     oled_send_byte(0x8D, 1);    //set charge pump enable
 82     oled_send_byte(0x14, 1);
 83     
 84     oled_send_byte(0xAF, 1);    //turn on oled panel
 85 }
 86 
 87 //OLED清屏
 88 static void oled_clear(void)
 89 {
 90     unsigned char i, n;         
 91     for(i=0; i<8; i++)  
 92     {  
 93         oled_send_byte(0xb0 + i, 1);     //设置页地址（0~7）
 94         oled_send_byte(0x00, 1);         //设置显示位置—列低地址
 95         oled_send_byte(0x10, 1);         //设置显示位置—列高地址     
 96 
 97         for(n=0; n<128; n++)
 98             oled_send_byte(0, 0); 
 99     }
100 }
101 
102 //OLED设置坐标
103 static void oled_set_pos(unsigned int x, unsigned int y)
104 {
105     unsigned char temp = 0;
106 
107     temp = 0xb0 + y;
108     oled_send_byte(temp, 1);
109 
110     temp = ((x & 0xf0) >> 4) |0x10;
111     oled_send_byte(temp, 1);
112 
113     temp = x & 0x0f;
114     oled_send_byte(temp, 1); 
115 }
116 
117 /*
118   OLED显示一个字符
119   x - 横坐标，范围在0-127
120   y - 纵坐标，范围在0-7
121 */
122 static void oled_show_char(unsigned char x, unsigned char y, unsigned char data)
123 {
124     unsigned char c=0,i;    
125     c = data - ' ';    //得到偏移后的值            
126 
127     if(x > 127)
128     {    
129         x = 0;
130         y = y + 2;
131     }
132     
133     if(16 == oled_cdev->size)
134     {
135         oled_set_pos(x, y);    
136         for(i=0; i<8; i++)
137             oled_send_byte(F8X16[c*16 + i], 0);
138         
139         oled_set_pos(x, y+1);
140         for(i=0; i<8; i++)
141             oled_send_byte(F8X16[c*16 + i + 8], 0);
142     }
143     else 
144     {    
145         oled_set_pos(x, y);
146         for(i=0; i<6; i++)
147             oled_send_byte(F6x8[c][i], 0);
148     }
149 }
150 
151 static void oled_show_string(unsigned char x, unsigned char y, unsigned char *buf)
152 {
153     unsigned char i = 0;
154     while (buf[i] != '\0')
155     {        
156         oled_show_char(x, y, buf[i]);
157         x += 8;
158         if(x > 120)
159         {    
160             x = 0;
161             y += 2;
162         }
163         i++;
164     }
165 }
166 
167 /* 这部分是字符驱动代码 */
168 
169 int cdev_open(struct inode *inode, struct file *file)
170 {
171     oled_init();
172     oled_clear();
173     
174     return 0;
175 }
176 
177 int cdev_release(struct inode *inode, struct file *file)
178 {
179     oled_clear();
180     return 0;
181 }
182 
183 ssize_t cdev_read(struct file *file, char __user *buf, size_t count, loff_t *position)
184 {
185     return 0;
186 }
187 
188 ssize_t cdev_write(struct file *file, const char __user *buf, size_t count, loff_t *position)
189 {
190     oled_clear();
191 
192     copy_from_user(oled_cdev->buf, buf, count);
193 
194     //待完善，这里只用于测试
195     oled_show_string(0, 0, oled_cdev->buf);
196     //oled_show_string(6, 3, "0.96' OLED TEST");
197 
198     return 0;
199 }
200 
201 //待完善，可用来控制OLED相关的显示参数，如字符大小
202 long cdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
203 {
204     return 0;
205 }
206 
207 struct file_operations cdev_fops = 
208 {
209     .owner                 = THIS_MODULE,
210     .open                  = cdev_open,
211     .release               = cdev_release,
212     .read                  = cdev_read,
213     .write                 = cdev_write,
214     .unlocked_ioctl        = cdev_ioctl,
215 };
216 
217 static int oled_probe(struct i2c_client *client, const struct i2c_device_id *id)
218 {
219     oled_cdev = kzalloc(sizeof(struct _oled_cdev), GFP_KERNEL);
220 
221     oled_cdev->client = client;
222     oled_cdev->size = 16;
223     
224     oled_cdev->cdev = cdev_alloc();
225     cdev_init(oled_cdev->cdev, &cdev_fops);
226     oled_cdev->cdev->owner = THIS_MODULE;
227 
228     alloc_chrdev_region(&oled_cdev->dev_num, 0, 1, "oled");
229     cdev_add(oled_cdev->cdev, oled_cdev->dev_num, 1);
230 
231     //创建设备节点 /dev/oled
232     oled_cdev->cls = class_create(THIS_MODULE, "oled_cls");
233     device_create(oled_cdev->cls, NULL, oled_cdev->dev_num, NULL, "oled");
234     
235     return 0;
236 }
237 
238 static int oled_remove(struct i2c_client *client)
239 {
240     device_destroy(oled_cdev->cls, oled_cdev->dev_num);
241     class_destroy(oled_cdev->cls);
242     cdev_del(oled_cdev->cdev);
243     unregister_chrdev_region(oled_cdev->dev_num, 1);
244     kfree(oled_cdev);
245     return 0;
246 }
247 
248 static const struct i2c_device_id oled_id_table[] = {
249     { "0.96-oled", 0 },
250     {}
251 };
252 
253 static struct i2c_driver oled_driver = {
254     .driver = {
255         .name      = "oled",
256         .owner     = THIS_MODULE,
257     },
258     .probe         = oled_probe,
259     .remove        = oled_remove,
260     .id_table      = oled_id_table,
261 };
262 
263 static int oled_drv_init(void)
264 {
265     i2c_add_driver(&oled_driver);
266     return 0;
267 }
268 
269 static void oled_drv_exit(void)
270 {
271     i2c_del_driver(&oled_driver);
272 }
273 
274 module_init(oled_drv_init);
275 module_exit(oled_drv_exit);
276
    //https://www.cnblogs.com/Recca/p/12760575.html