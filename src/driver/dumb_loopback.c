#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device/class.h>

#define DEVICE_NAME "dumb-loopback-0"
#define CLASS_NAME "dumb-loopback"
#define BUFFER_SIZE 1024

static int major_number = -1;
static char data_buffer[BUFFER_SIZE];
static int next_write_idx = 0;
static int next_read_pos = 0;
static struct class* loopback_class = NULL;
static struct device* loopback_device = NULL;

static int dev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int error_count = 0;
    int bytes_read = 0;

    while (len && (next_read_pos != next_write_idx)) {
        error_count = copy_to_user(buffer++, data_buffer + next_read_pos, 1);
        if (error_count == 0) {
            len--;
            bytes_read++;
            next_read_pos = (next_read_pos + 1) % BUFFER_SIZE;
        } else {
            printk(KERN_ALERT "dumb_loopback: Failed to read %d characters\n", error_count);
            return -EFAULT; // Failed -- return a bad address message
        }
    }

    return bytes_read;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    int bytes_written = 0;
    for (bytes_written = 0; bytes_written < len; bytes_written++) {
        if (next_write_idx != ((next_read_pos - 1 + BUFFER_SIZE) % BUFFER_SIZE)) {
            copy_from_user(data_buffer + next_write_idx, buffer + bytes_written, 1);
            next_write_idx = (next_write_idx + 1) % BUFFER_SIZE;
        } else {
            break;
        }
    }

    return bytes_written;
}

static struct file_operations fops =
{
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static int __init loopback_init(void) {
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "dumb_loopback: Failed to register a major number\n");
        return major_number;
    }
    loopback_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(loopback_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "dumb_loopback: Failed to register device class\n");
        return PTR_ERR(loopback_class);
    }
    loopback_device = device_create(loopback_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(loopback_device)) {
        class_destroy(loopback_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "dumb_loopback: Failed to create the device\n");
        return PTR_ERR(loopback_device);
    }
    memset(data_buffer, 0, BUFFER_SIZE); // Clear the buffer
    printk(KERN_INFO "dumb_loopback: Registered successfully\n");
    return 0;
}

static void __exit loopback_exit(void) {
    device_destroy(loopback_class, MKDEV(major_number, 0));
    class_unregister(loopback_class);
    class_destroy(loopback_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "dumb_loopback: Unregistered successfully\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marcus Plutowski <marcusplutowski@gmail.com>");

module_init(loopback_init);
module_exit(loopback_exit);
