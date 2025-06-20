/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesd-circular-buffer.h"
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Srimat Tirumala Swathi"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

static struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev; /* device information */
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev; /* for other methods */
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    size_t entry_offset;
    size_t bytes_to_read;
    struct aesd_buffer_entry *entry;
    struct aesd_dev *dev = filp->private_data;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    mutex_lock(&dev->lock);

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.buffer, *f_pos, &entry_offset);
    if (!entry) {
        mutex_unlock(&dev->lock);
        return 0; // EOF or no valid entry
    }

    bytes_to_read = entry->size - entry_offset;
    if (bytes_to_read > count)
        bytes_to_read = count;

    if (copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_read))
    {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    *f_pos += bytes_to_read;
    retval = bytes_to_read;

    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    char *kbuf;
    char *new_buf;
    size_t new_size;
    struct aesd_buffer_entry entry;
    struct aesd_dev *dev = filp->private_data;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    printk(KERN_ALERT "%s: %s\n", __func__, kbuf);
    entry.buffptr = kbuf;
    entry.size = count;

    mutex_lock(&dev->lock);

    if (dev->part_buf) {
        new_size = dev->part_size + count;
        new_buf = krealloc(dev->part_buf, new_size, GFP_KERNEL);
        if (!new_buf) {
            kfree(kbuf);
            mutex_unlock(&dev->lock);
            return -ENOMEM;
        }
        memcpy(new_buf + dev->part_size, kbuf, count);
        kfree(kbuf);
        dev->part_buf = new_buf;
        dev->part_size = new_size;
    }
    else
    {
        dev->part_buf = kbuf;
        dev->part_size = count;
    }

    if (dev->part_buf[dev->part_size - 1] == '\n') {
        entry.buffptr = dev->part_buf;
        entry.size = dev->part_size;

        aesd_circular_buffer_add_entry(&dev->buffer, &entry);

        dev->part_buf = NULL;
        dev->part_size = 0;
    }

    mutex_unlock(&dev->lock);

    retval = count;
    return retval;
}

long int aesd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct aesd_seekto seek_params;
    struct aesd_dev *dev = file->private_data;
    int retval = 0;
    unsigned int i;
    loff_t new_pos;

    switch (cmd) {
        case AESDCHAR_IOCSEEKTO:
            // Copy the seek parameters from user space into the struct
            if (copy_from_user(&seek_params, (struct aesd_seekto *)arg, sizeof(seek_params))) {
                return -EFAULT;
            }

            // Validate the write_cmd index
            if (seek_params.write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED ||
                !dev->buffer.entry[seek_params.write_cmd].buffptr) {
                return -EINVAL;  // Invalid write_cmd index
            }

            // Validate the offset within the specific write command
            if (seek_params.write_cmd_offset >= dev->buffer.entry[seek_params.write_cmd].size) {
                return -EINVAL;  // Invalid offset within the write command
            }

            // Update the file pointer (f_pos) to the new position
	    new_pos = 0;

	    for (i = 0; i < seek_params.write_cmd; i++) {
		    new_pos += dev->buffer.entry[i].size;
	    }

	    new_pos += seek_params.write_cmd_offset;

	    file->f_pos = new_pos;

            printk(KERN_INFO "Seek to write command %d, offset %d\n",
                   seek_params.write_cmd, seek_params.write_cmd_offset);
            break;

        default:
            return -ENOTTY;  // Command not recognized
    }

    return retval;
}

size_t aesd_circular_buffer_total_size(struct aesd_circular_buffer *cb)
{
    size_t total_size = 0;
    unsigned int i;

    // Sum the size of each valid entry in the buffer
    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (cb->entry[i].buffptr != NULL) {
            total_size += cb->entry[i].size;
        }
    }

    return total_size;
}

loff_t aesd_llseek(struct file *file, loff_t offset, int whence)
{
    struct aesd_dev *dev = file->private_data;
    size_t total_size = aesd_circular_buffer_total_size(&dev->buffer); // Get total size of the buffer
    loff_t new_pos;

    switch (whence) {
        case SEEK_SET:
            // Move to the absolute offset
            new_pos = offset;
            break;
        case SEEK_CUR:
            // Move relative to the current file position
            new_pos = file->f_pos + offset;
            break;
        case SEEK_END:
            // Move relative to the end of the file
            new_pos = total_size + offset;
            break;
        default:
            return -EINVAL; // Invalid whence value
    }

    // Ensure that the new position is within valid bounds
    if (new_pos < 0) {
        return -EINVAL; // Invalid position
    }

    // Prevent going beyond the end of the buffer
    if (new_pos > total_size) {
        new_pos = total_size;
    }

    // Update the file position
    file->f_pos = new_pos;
    return new_pos;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .unlocked_ioctl = aesd_ioctl,
    .llseek =   aesd_llseek,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);
    printk(KERN_ALERT "major: %d, minor: %d\n", aesd_major, aesd_minor);
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    unsigned char i;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (aesd_device.buffer.entry[i].buffptr != NULL) {
            kfree(aesd_device.buffer.entry[i].buffptr);
        }
    }
    if (aesd_device.part_buf) {
        kfree(aesd_device.part_buf);
    }
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
