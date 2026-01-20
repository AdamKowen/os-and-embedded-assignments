#ifndef _ENCDEV_H
#define _ENCDEV_H

#include <linux/ioctl.h>

// The major device number.
// We don't rely on dynamic registration any more.
// We want ioctls to know this number at compile time.
#define MAJOR_NUM 244

// Set the message number of the device driver
// _IO	 - no params
// _IOW	 - write params (user transfers value to us, copy_from_user)
// _IOR  - read params  (we transfer value to user, copy_to_user)
// _IOWR - both write and read params
#define IOCTL_SET_VAL _IOW(MAJOR_NUM, 0, unsigned long)

#endif
