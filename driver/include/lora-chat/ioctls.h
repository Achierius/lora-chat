#include <linux/ioctl.h>

// Magic number for lora-chat driver
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
// Transmission characteristics
#define IOCTL_SET_BANDWIDTH        _IOW(IOCTL_MAGIC, 10, size_t*)
#define IOCTL_GET_BANDWIDTH        _IOR(IOCTL_MAGIC, 11, size_t*)
#define IOCTL_SET_CODING_RATE      _IOW(IOCTL_MAGIC, 12, size_t*)
#define IOCTL_GET_CODING_RATE      _IOR(IOCTL_MAGIC, 13, size_t*)
#define IOCTL_SET_SPREADING_FACTOR _IOW(IOCTL_MAGIC, 14, size_t*)
#define IOCTL_GET_SPREADING_FACTOR _IOR(IOCTL_MAGIC, 15, size_t*)
