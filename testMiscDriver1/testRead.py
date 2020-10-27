
# 打开文件进行读取
fh = open("/dev/mymiscled", "r")
# 根据大小读取文件内容
print('输出来自 read() 方法\n', fh.read(2048))
# 关闭文件
fh.close()

