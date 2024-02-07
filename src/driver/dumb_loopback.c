#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device/class.h>
#include <linux/ioctl.h>
#include <linux/spi/spi.h>

#define DEVICE_NAME "dumb-loopback-0"
#define CLASS_NAME "dumb-loopback"
#define BUFFER_SIZE 1024

static int major_number = -1;
static struct class* loopback_class = NULL;
static struct device* loopback_device = NULL;

struct loopback_driver_data {
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

static struct loopback_driver_data l_dat;

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
      printk(KERN_ALERT "dumb_loopback: Failed to read %d characters\n", error_count);
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

#define IOCTL_MAGIC 0xA8
// Network characteristics
#define IOCTL_SET_FREQUENCY        _IOW(IOCTL_MAGIC,  1, int32_t*)
#define IOCTL_GET_FREQUENCY        _IOR(IOCTL_MAGIC,  2, int32_t*)
#define IOCTL_SET_SENDER_ADDRESS   _IOW(IOCTL_MAGIC,  3, char*)
#define IOCTL_GET_SENDER_ADDRESS   _IOR(IOCTL_MAGIC,  4, char*)
#define IOCTL_SET_RECEIVER_ADDRESS _IOW(IOCTL_MAGIC,  5, char*)
#define IOCTL_GET_RECEIVER_ADDRESS _IOR(IOCTL_MAGIC,  6, char*)
// Special commands
#define IOCTL_SEND_GPS_LOCATION    _IO(IOCTL_MAGIC,   8)
// Driver internal stuff
// Transmission characteristics
#define IOCTL_SET_BANDWIDTH        _IOW(IOCTL_MAGIC, 10, size_t*)
#define IOCTL_GET_BANDWIDTH        _IOR(IOCTL_MAGIC, 11, size_t*)
#define IOCTL_SET_CODING_RATE      _IOW(IOCTL_MAGIC, 12, size_t*)
#define IOCTL_GET_CODING_RATE      _IOR(IOCTL_MAGIC, 13, size_t*)
#define IOCTL_SET_SPREADING_FACTOR _IOW(IOCTL_MAGIC, 14, size_t*)
#define IOCTL_GET_SPREADING_FACTOR _IOR(IOCTL_MAGIC, 15, size_t*)

#define IOCTL_SEND_GPS _IO
static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
  // TODO check capabilities? e.g.
  // if (! capable (CAP_SYS_ADMIN))
  //   return -EPERM;
  switch (cmd) {
    case IOCTL_SET_FREQUENCY:
      if (copy_from_user(&l_dat.frequency, (int32_t*) arg, sizeof(l_dat.frequency))) {
        printk(KERN_ERR "dumb_loopback: error while setting frequency\n");
      }
      printk(KERN_INFO "dumb_loopback: frequency is now %d\n", l_dat.frequency);
      break;
    case IOCTL_GET_FREQUENCY:
      if (copy_to_user((int32_t*) arg, &l_dat.frequency, sizeof(l_dat.frequency))) {
        printk(KERN_ERR "dump_loopback: error while getting frequency\n");
      }
      break;
    case IOCTL_SET_SENDER_ADDRESS:
      if (copy_from_user(&l_dat.sender_address, (char*) arg, sizeof(l_dat.sender_address))) {
        printk(KERN_ERR "dumb_loopback: error while setting sender address\n");
      }
      printk(KERN_INFO "dumb_loopback: sender address is now %d\n", l_dat.sender_address);
      break;
    case IOCTL_GET_SENDER_ADDRESS:
      if (copy_to_user((char*) arg, &l_dat.sender_address, sizeof(l_dat.sender_address))) {
        printk(KERN_ERR "dump_loopback: error while getting sender address\n");
      }
      break;
    case IOCTL_SET_RECEIVER_ADDRESS:
      if (copy_from_user(&l_dat.receiver_address, (char*) arg, sizeof(l_dat.receiver_address))) {
        printk(KERN_ERR "dumb_loopback: error while setting receiver address\n");
      }
      printk(KERN_INFO "dumb_loopback: receiver address is now %d\n", l_dat.receiver_address);
      break;
    case IOCTL_GET_RECEIVER_ADDRESS:
      if (copy_to_user((char*) arg, &l_dat.receiver_address, sizeof(l_dat.receiver_address))) {
        printk(KERN_ERR "dump_loopback: error while getting receiver address\n");
      }
      break;
    case IOCTL_SEND_GPS_LOCATION:
      break;
    case IOCTL_SET_BANDWIDTH:
      if (copy_from_user(&l_dat.bandwidth, (char*) arg, sizeof(l_dat.bandwidth))) {
        printk(KERN_ERR "dumb_loopback: error while setting bandwidth\n");
      }
      printk(KERN_INFO "dumb_loopback: bandwidth is now %d\n", l_dat.bandwidth);
      break;
    case IOCTL_GET_BANDWIDTH:
      if (copy_to_user((char*) arg, &l_dat.bandwidth, sizeof(l_dat.bandwidth))) {
        printk(KERN_ERR "dump_loopback: error while getting bandwidth\n");
      }
      break;
    case IOCTL_SET_CODING_RATE:
      if (copy_from_user(&l_dat.coding_rate, (char*) arg, sizeof(l_dat.coding_rate))) {
        printk(KERN_ERR "dumb_loopback: error while setting coding_rate\n");
      }
      printk(KERN_INFO "dumb_loopback: coding_rate is now %d\n", l_dat.coding_rate);
      break;
    case IOCTL_GET_CODING_RATE:
      if (copy_to_user((char*) arg, &l_dat.spreading_factor, sizeof(l_dat.spreading_factor))) {
        printk(KERN_ERR "dump_loopback: error while getting spreading_factor\n");
      }
      break;
    case IOCTL_SET_SPREADING_FACTOR:
      if (copy_from_user(&l_dat.spreading_factor, (char*) arg, sizeof(l_dat.spreading_factor))) {
        printk(KERN_ERR "dumb_loopback: error while setting spreading_factor\n");
      }
      printk(KERN_INFO "dumb_loopback: spreading_factor is now %d\n", l_dat.spreading_factor);
      break;
    case IOCTL_GET_SPREADING_FACTOR:
      if (copy_to_user((char*) arg, &l_dat.spreading_factor, sizeof(l_dat.spreading_factor))) {
        printk(KERN_ERR "dump_loopback: error while getting spreading_factor\n");
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
  memset(l_dat.data_buffer, 0, BUFFER_SIZE); // Clear the buffer
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
