#include linux/module.h
#include linux/kernel.h
#include linux/proc_fs.h
#include linux/sched.h
#include asm/uaccess.h
 
#define STRINGLEN 1024
 
char global_buffer[STRINGLEN];
 
struct proc_dir_entry *example_dir, *hello_file;
 
int proc_read_hello(char *page, char **start, off_t off, int count, int *eof,
                void *data) {
        int len;
        len = sprintf(page, global_buffer); //把global_buffer的内容显示给访问者
        return len;
}
 
int proc_write_hello(struct file *file, const char *buffer, unsigned long count,
                void *data) {
        int len;
 
        if (count = STRINGLEN)
                len = STRINGLEN – 1;
        else
                len = count;
 
        /*
         * copy_from_user函数将数据从用户空间拷贝到内核空间
         * 此处将用户写入的数据存入global_buffer
         */
        copy_from_user(global_buffer, buffer, len);
        global_buffer[len] = \0′;
        return len;
}
 
static int __init proc_test_init(void) {
        example_dir = proc_mkdir("proc_test", NULL);
        hello_file = create_proc_entry("hello", S_IRUGO, example_dir);
        strcpy(global_buffer, "hello");
        hello_file->read_proc = proc_read_hello;
        hello_file->write_proc = proc_write_hello;
        return 0;
}
 
static void __exit proc_test_exit(void) {
        remove_proc_entry("hello", example_dir);
        remove_proc_entry(“proc_test”, NULL);
}
 
module_init(proc_test_init);
module_exit(proc_test_exit)
    /*
        写入proc文件

echo "Hello from kernel" /proc/proc_test/hello'
读取proc文件内容，将看到屏幕上显示了我们写入的字符串:Hello from kernel

cat /proc/proc_test/hello

*/