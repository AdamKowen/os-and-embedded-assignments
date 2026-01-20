
// encdev.c - A simple character device driver that encrypts data
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/slab.h>		/* for kmalloc and kfree */
MODULE_LICENSE("GPL");

#include "encdev.h"

#define NAME "encdev"

static int size  = 1024;
static int count = 4;
module_param(size,  int,  S_IRUSR);
module_param(count, int,  S_IRUSR);

static unsigned char **buffers;

static inline bool valid_minor(unsigned int minor)
{
        return minor < count;
}

struct file_val {
        unsigned char  key;      // encryption key (0-255)
        unsigned char *buf;      // pointer to the device buffer
};

static int device_open(struct inode *inode, struct file *filp)
{
	unsigned int minor = MINOR(inode->i_rdev); //macro to get minor number - given in assigment
	struct file_val *val;

    if (!valid_minor(minor)) {
        printk(KERN_ALERT "%s: invalid minor number %d\n", NAME, minor);
        return -ENODEV;
    }

	val = kmalloc(sizeof(*val), GFP_KERNEL);

	if (!val)
		return -1;

	val->key = 0;                       // default key = 0
    val->buf = buffers[minor];          // buffer for this device
    filp->private_data = val;

	printk(KERN_INFO NAME ": open minor=%u key=%u\n", minor, val->key);
	return 0;
}



int device_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s: device_release()\n", NAME);
	if (filp->private_data) {
		kfree(filp->private_data);
	}
	return 0;
}

static long device_ioctl(
	struct   file* file,
	unsigned int   cmd_id,
	unsigned long  arg)
{
	if (cmd_id == IOCTL_SET_VAL) {

		if ((arg < 0) || (arg > 255))
			return -EINVAL;

        ((struct file_val*)file->private_data)->key = (unsigned char)arg;
		return 0;
	}
	return -EINVAL;
}

static ssize_t device_read(
	struct file* file,
	char __user* buffer,
	size_t       length,
	loff_t*      offset)
{
	int i;
	struct file_val *val = file->private_data;
	printk("Invoking device read(%p,%ld)\n", file, length);

if (*offset >= size) return 0;
if (*offset + length > size) length = size - *offset;

	for (i = 0; i < length; ++i) {
		put_user(val->buf[*offset + i] - val->key, buffer + i);
	}

	*offset += i;
	return i;
}


static ssize_t device_write(
struct file* file,
	const char __user* buffer,
	size_t       length,
	loff_t*      offset)
{

	int i;
    struct file_val *val = file->private_data;
printk("Invoking device write(%p,%ld)\n", file, length);

    if (*offset >= size) return 0;
if (*offset + length > size) length = size - *offset;

    for (i = 0; i < length; ++i) {
        unsigned char ch;
        if (get_user(ch, buffer + i)) return -EFAULT;
        val->buf[*offset + i] = ch + val->key;
    }
    *offset += i;
	return i;
}

static loff_t device_llseek(struct file *file, loff_t offset, int whence)
{
    loff_t newpos = (whence == SEEK_SET) ? offset :
                    (whence == SEEK_CUR) ? file->f_pos + offset :
                    (whence == SEEK_END) ? size + offset : -1;

    if (newpos < 0 || newpos > size) return -EINVAL;
    file->f_pos = newpos;
    return newpos;
}



struct file_operations fops = {
	.owner		= THIS_MODULE,
	.read		= device_read,
	.open		= device_open,
	.release	= device_release,
	.write          = device_write,
    .llseek         = device_llseek,
	.unlocked_ioctl = device_ioctl,
};

	int init_module(void)
	{
		int i, j, ret;
		buffers = kmalloc_array(count, sizeof(unsigned char *), GFP_KERNEL);

		for (i = 0; i < count; ++i) {
    		buffers[i] = kmalloc(size, GFP_KERNEL);
    		if (!buffers[i]) return -ENOMEM;


    		for ( j = 0; j < size; ++j)
        		buffers[i][j] = 0;
		}

		ret = register_chrdev(MAJOR_NUM, NAME, &fops);
		if (ret < 0) {
			printk(KERN_ALERT "%s registartion failed: %d\n", NAME, ret);
			return ret;
		}

		printk(KERN_NOTICE "Module %s registered, major number is: %d\n", NAME, MAJOR_NUM);
		printk(KERN_INFO "Don't forget to rm the device file and rmmod the module!\n");
		return 0;
}

void cleanup_module(void)
{
	int i;
	unregister_chrdev(MAJOR_NUM, NAME);
	for (i = 0; i < count; ++i)
    	kfree(buffers[i]);
	kfree(buffers);
	printk(KERN_INFO "Module %s unregistered\n", NAME);
}