#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/unistd.h>


extern asmlinkage long sys_close(unsigned int);

MODULE_LICENSE("GPL");

static int  sysnr = -1;
static char *msg  = "defaultmessage";

module_param(sysnr, int, 0);
MODULE_PARM_DESC(sysnr, "syscall number to hook");

module_param(msg, charp, 0);
MODULE_PARM_DESC(msg, "massage to print before syscall");

unsigned long **sys_call_table;
unsigned long original_cr0;

asmlinkage long (*ref_mkdir)(const char __user *, umode_t);
asmlinkage long (*ref_chdir)(const char __user *);
asmlinkage long (*ref_close)(unsigned int);
asmlinkage long (*ref_dup)(unsigned int);

asmlinkage long new_mkdir(const char __user *pathname, umode_t mode)
{
    int i;
    long ret;
    char kpath_name[11];

    ret = ref_mkdir(pathname, mode);

    for (i = 0; i < 10; i++)
        get_user(kpath_name[i], pathname + i);

    kpath_name[10] = '\0';
    printk(KERN_INFO "%s %s\n", msg, kpath_name);

    return ret;
}

asmlinkage long new_chdir(const char __user *pathname)
{
    int i;
    long ret;
    char kpath_name[11];

    ret = ref_chdir(pathname);

    for (i = 0; i < 10; i++)
        get_user(kpath_name[i], pathname + i);

    kpath_name[10] = '\0';
    printk(KERN_INFO "%s %s\n", msg, kpath_name);

    return ret;
}

asmlinkage long new_close(unsigned int fd)
{
    printk(KERN_INFO "%s %u\n", msg, fd);
    return ref_close(fd);
}

asmlinkage long new_dup(unsigned int fd)
{
    printk(KERN_INFO "%s %u\n", msg, fd);
    return ref_dup(fd);
}

static unsigned long **acquire_sys_call_table(void)
{
    unsigned long int offset = PAGE_OFFSET;
    unsigned long **sct;

    while (offset < ULLONG_MAX) {
        sct = (unsigned long **)offset;

        if (sct[__NR_close] == (unsigned long *) sys_close)
            return sct;

        offset += sizeof(void *);
    }

    return NULL;
}

int init_module(void)
{
    if (!(sys_call_table = acquire_sys_call_table()))
        return -1;

    printk(KERN_INFO "intercepting syscall %d\n", sysnr);

    switch (sysnr) {
    case __NR_mkdir:
        ref_mkdir = (void *)sys_call_table[__NR_mkdir];
        original_cr0 = read_cr0();
        write_cr0(original_cr0 & ~0x00010000);
        sys_call_table[__NR_mkdir] = (unsigned long *)new_mkdir;
        write_cr0(original_cr0);
        break;

    case __NR_chdir:
        ref_chdir = (void *)sys_call_table[__NR_chdir];
        original_cr0 = read_cr0();
        write_cr0(original_cr0 & ~0x00010000);
        sys_call_table[__NR_chdir] = (unsigned long *)new_chdir;
        write_cr0(original_cr0);
        break;

    case __NR_close:
        ref_close = (void *)sys_call_table[__NR_close];
        original_cr0 = read_cr0();
        write_cr0(original_cr0 & ~0x00010000);
        sys_call_table[__NR_close] = (unsigned long *)new_close;
        write_cr0(original_cr0);
        break;

    case __NR_dup:
        ref_dup = (void *)sys_call_table[__NR_dup];
        original_cr0 = read_cr0();
        write_cr0(original_cr0 & ~0x00010000);
        sys_call_table[__NR_dup] = (unsigned long *)new_dup;
        write_cr0(original_cr0);
        break;

    default:
        return -1;
    }

    return 0;
}

void cleanup_module(void)
{
    if (!sys_call_table)
        return;

    printk(KERN_INFO "restoring syscall %d\n", sysnr);

    original_cr0 = read_cr0();
    write_cr0(original_cr0 & ~0x00010000);

    switch (sysnr) {
    case __NR_mkdir:
        sys_call_table[__NR_mkdir] = (unsigned long *)ref_mkdir;
        break;
    case __NR_chdir:
        sys_call_table[__NR_chdir] = (unsigned long *)ref_chdir;
        break;
    case __NR_close:
        sys_call_table[__NR_close] = (unsigned long *)ref_close;
        break;
    case __NR_dup:
        sys_call_table[__NR_dup] = (unsigned long *)ref_dup;
        break;
    }

    write_cr0(original_cr0);

    msleep(3000);
}
