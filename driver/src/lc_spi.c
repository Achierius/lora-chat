#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device/class.h>
#include <linux/ioctl.h>
#include <linux/spi/spi.h>

#include "lora-chat/ioctls.h"
#include "lora-chat/device_info.h"

#define BUFFER_SIZE 1024
#define DRIVER_NAME "lc_spi"

static int major_number = -1;
static struct class* lc_spi_class = NULL;
static struct device* lc_spi_device = NULL;

struct lc_spi_driver_data {
  size_t next_write_idx;
  size_t next_read_idx;

  int32_t frequency;
  char sender_address;
  char receiver_address;

  int32_t bandwidth;
  int32_t coding_rate;
  int32_t spreading_factor;

  char data_buffer[BUFFER_SIZE];
};

static struct lc_spi_driver_data l_dat;

static int dev_open(struct inode *inodep, struct file *filep) {
  return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
  return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
  int error_count = 0;
  int bytes_read = 0;

  while (len && (l_dat.next_read_idx != l_dat.next_write_idx)) {
    error_count = copy_to_user(buffer++, l_dat.data_buffer + l_dat.next_read_idx, 1);
    if (error_count == 0) {
      len--;
      bytes_read++;
      l_dat.next_read_idx = (l_dat.next_read_idx + 1) % BUFFER_SIZE;
    } else {
      printk(KERN_ALERT DRIVER_NAME ": failed to read %d characters\n", error_count);
      return -EFAULT; // Failed -- return a bad address message
    }
  }

  return bytes_read;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
  int bytes_written = 0;
  for (bytes_written = 0; bytes_written < len; bytes_written++) {
    if (l_dat.next_write_idx != ((l_dat.next_read_idx - 1 + BUFFER_SIZE) % BUFFER_SIZE)) {
      copy_from_user(l_dat.data_buffer + l_dat.next_write_idx, buffer + bytes_written, 1);
      l_dat.next_write_idx = (l_dat.next_write_idx + 1) % BUFFER_SIZE;
    } else {
      break;
    }
  }

  return bytes_written;
}

static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
  // TODO check capabilities? e.g.
  // if (! capable (CAP_SYS_ADMIN))
  //   return -EPERM;
  switch (cmd) {
    case IOCTL_SET_FREQUENCY:
      if (copy_from_user(&l_dat.frequency, (int32_t*) arg, sizeof(l_dat.frequency))) {
        printk(KERN_ERR DRIVER_NAME ": error while setting frequency\n");
      }
      printk(KERN_INFO DRIVER_NAME ": frequency is now %d\n", l_dat.frequency);
      break;
    case IOCTL_GET_FREQUENCY:
      if (copy_to_user((int32_t*) arg, &l_dat.frequency, sizeof(l_dat.frequency))) {
        printk(KERN_ERR DRIVER_NAME ": error while getting frequency\n");
      }
      break;
    case IOCTL_SET_SENDER_ADDRESS:
      if (copy_from_user(&l_dat.sender_address, (char*) arg, sizeof(l_dat.sender_address))) {
        printk(KERN_ERR DRIVER_NAME ": error while setting sender address\n");
      }
      printk(KERN_INFO DRIVER_NAME ": sender address is now %d\n", l_dat.sender_address);
      break;
    case IOCTL_GET_SENDER_ADDRESS:
      if (copy_to_user((char*) arg, &l_dat.sender_address, sizeof(l_dat.sender_address))) {
        printk(KERN_ERR DRIVER_NAME ": error while getting sender address\n");
      }
      break;
    case IOCTL_SET_RECEIVER_ADDRESS:
      if (copy_from_user(&l_dat.receiver_address, (char*) arg, sizeof(l_dat.receiver_address))) {
        printk(KERN_ERR DRIVER_NAME ": error while setting receiver address\n");
      }
      printk(KERN_INFO DRIVER_NAME ": receiver address is now %d\n", l_dat.receiver_address);
      break;
    case IOCTL_GET_RECEIVER_ADDRESS:
      if (copy_to_user((char*) arg, &l_dat.receiver_address, sizeof(l_dat.receiver_address))) {
        printk(KERN_ERR DRIVER_NAME ": error while getting receiver address\n");
      }
      break;
    case IOCTL_SEND_GPS_LOCATION:
      break;
    case IOCTL_SET_BANDWIDTH:
      if (copy_from_user(&l_dat.bandwidth, (char*) arg, sizeof(l_dat.bandwidth))) {
        printk(KERN_ERR DRIVER_NAME ": error while setting bandwidth\n");
      }
      printk(KERN_INFO DRIVER_NAME ": bandwidth is now %d\n", l_dat.bandwidth);
      break;
    case IOCTL_GET_BANDWIDTH:
      if (copy_to_user((char*) arg, &l_dat.bandwidth, sizeof(l_dat.bandwidth))) {
        printk(KERN_ERR DRIVER_NAME ": error while getting bandwidth\n");
      }
      break;
    case IOCTL_SET_CODING_RATE:
      if (copy_from_user(&l_dat.coding_rate, (char*) arg, sizeof(l_dat.coding_rate))) {
        printk(KERN_ERR DRIVER_NAME ": error while setting coding_rate\n");
      }
      printk(KERN_INFO DRIVER_NAME ": coding_rate is now %d\n", l_dat.coding_rate);
      break;
    case IOCTL_GET_CODING_RATE:
      if (copy_to_user((char*) arg, &l_dat.spreading_factor, sizeof(l_dat.spreading_factor))) {
        printk(KERN_ERR DRIVER_NAME ": error while getting spreading_factor\n");
      }
      break;
    case IOCTL_SET_SPREADING_FACTOR:
      if (copy_from_user(&l_dat.spreading_factor, (char*) arg, sizeof(l_dat.spreading_factor))) {
        printk(KERN_ERR DRIVER_NAME ": error while setting spreading_factor\n");
      }
      printk(KERN_INFO DRIVER_NAME ": spreading_factor is now %d\n", l_dat.spreading_factor);
      break;
    case IOCTL_GET_SPREADING_FACTOR:
      if (copy_to_user((char*) arg, &l_dat.spreading_factor, sizeof(l_dat.spreading_factor))) {
        printk(KERN_ERR DRIVER_NAME ": error while getting spreading_factor\n");
      }
      break;
    default:
      return -ENOTTY;
  }
  return 0;
}

const static struct file_operations fops =
{
  .open = dev_open,
  .read = dev_read,
  .write = dev_write,
  .release = dev_release,
  .unlocked_ioctl = dev_ioctl,
};

static int __init lc_spi_init(void) {
  major_number = register_chrdev(0, LORACHAT_DEVICE_NAME, &fops);
  if (major_number < 0) {
    printk(KERN_ALERT DRIVER_NAME ": failed to register a major number\n");
    return major_number;
  }
  lc_spi_class = class_create(THIS_MODULE, LORACHAT_CLASS_NAME);
  if (IS_ERR(lc_spi_class)) {
    unregister_chrdev(major_number, LORACHAT_DEVICE_NAME);
    printk(KERN_ALERT DRIVER_NAME ": failed to register device class\n");
    return PTR_ERR(lc_spi_class);
  }
  lc_spi_device = device_create(lc_spi_class, NULL, MKDEV(major_number, 0), NULL, LORACHAT_DEVICE_NAME);
  if (IS_ERR(lc_spi_device)) {
    class_destroy(lc_spi_class);
    unregister_chrdev(major_number, LORACHAT_DEVICE_NAME);
    printk(KERN_ALERT DRIVER_NAME ": failed to create the device\n");
    return PTR_ERR(lc_spi_device);
  }
  memset(l_dat.data_buffer, 0, BUFFER_SIZE); // Clear the buffer
  printk(KERN_INFO DRIVER_NAME ": registered successfully\n");
  return 0;
}

static void __exit lc_spi_exit(void) {
  device_destroy(lc_spi_class, MKDEV(major_number, 0));
  class_unregister(lc_spi_class);
  class_destroy(lc_spi_class);
  unregister_chrdev(major_number, LORACHAT_DEVICE_NAME);
  printk(KERN_INFO DRIVER_NAME ": unregistered successfully\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marcus Plutowski <marcusplutowski@gmail.com>");

module_init(lc_spi_init);
module_exit(lc_spi_exit);
