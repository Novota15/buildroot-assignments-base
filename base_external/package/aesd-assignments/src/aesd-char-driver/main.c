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
#include <linux/uaccess.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Grant Novota"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t read_bytes = 0;
    ssize_t ret_entry_offset = 0;
    struct aesd_buffer_entry *buffer_entry = NULL;
    struct aesd_dev *aesd_dev = NULL;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    if((filp == NULL) || (buf == NULL))
    {
        return -EINVAL;
    }
    aesd_dev = filp->private_data;

    // Lock mutex before acessing the global data
    if (mutex_lock_interruptible(&aesd_dev->aesd_mutex))
    {
        PDEBUG("mutex_lock_interruptible failed");
        return -ERESTARTSYS;
    }

    // Find entry for the position specified
    buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_dev->buffer, *f_pos, &ret_entry_offset);
    if(buffer_entry == NULL)
    {
        PDEBUG("aesd_circular_buffer_find_entry_offset_for_fpos returned nothing");
        mutex_unlock(&aesd_dev->aesd_mutex);
        return read_bytes;
    }

    // calculate the number of bytes that can be read
    if((buffer_entry->size - ret_entry_offset) > count)
    {
        read_bytes = count;
    }
    else
    {
        read_bytes = buffer_entry->size - ret_entry_offset;
    }

    // copy the read_bytes number of bytes from kernel buffer to user buffer
    if (copy_to_user(buf, buffer_entry->buffptr+ret_entry_offset, read_bytes))
    {
        PDEBUG("copy_to_user failed");
        mutex_unlock(&aesd_dev->aesd_mutex);
        return -EFAULT;
    }

    // advance the pointer by the number of bytes read
    *f_pos += read_bytes;

    // unlock mutex once done accessing global data
    mutex_unlock(&aesd_dev->aesd_mutex);

    return read_bytes;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t write_bytes = 0;
    char *write_buf = NULL;
    struct aesd_dev *aesd_dev = NULL;
    char *last_byte = NULL;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    if((filp == NULL) || (buf == NULL))
    {
        return -EINVAL;
    }

    // allocate count number of bytes for kernel buffer
    write_buf = kmalloc(count, GFP_KERNEL);
    if(write_buf == NULL)
    {
        PDEBUG("kmalloc failed");
        return -ENOMEM;
    }

    // Copy buffer from user space into kernel space
    if (copy_from_user(write_buf, buf, count))
    {
        PDEBUG("copy_from_user failed");
        kfree(write_buf);
        return -EFAULT;
    }

    // Check for the position of new line character
    last_byte = memchr(write_buf, '\n', count);
    if(last_byte != NULL)
    {
        // If new line, calculate the number of bytes to write
        write_bytes = (last_byte - write_buf) + 1;
    }
    else
    {
        // If no new line, write upto the capacity
        write_bytes = count;
    }

    aesd_dev = filp->private_data;

    // Lock mutex before acessing the global data
    if (mutex_lock_interruptible(&aesd_dev->aesd_mutex))
    {
        PDEBUG("mutex_lock_interruptible failed");
        kfree(write_buf);
        return -ERESTARTSYS;
    }

    // Reallocate the memory based on write bytes
    aesd_dev->buffer_entry.buffptr = krealloc(aesd_dev->buffer_entry.buffptr, aesd_dev->buffer_entry.size + write_bytes, GFP_KERNEL);
    if(aesd_dev->buffer_entry.buffptr == NULL)
    {
        PDEBUG("krealloc failed");
        mutex_unlock(&aesd_dev->aesd_mutex);
        kfree(write_buf);
        return -ENOMEM;
    }

    // Copy the kernel space buffer content to the circular buffer entry
    memcpy((void *)aesd_dev->buffer_entry.buffptr+aesd_dev->buffer_entry.size, write_buf, write_bytes);
    aesd_dev->buffer_entry.size += write_bytes;

    if(last_byte)
    {
        const char *ret_ptr = NULL;
        // If new line was detected, write to the circular buffer
        ret_ptr = aesd_circular_buffer_add_entry(&aesd_dev->buffer, &aesd_dev->buffer_entry);
        if(ret_ptr)
        {
            // Free the pointer to the oldest data
            kfree(ret_ptr);
        }
        // Reset the pointer and size for the new request
        aesd_dev->buffer_entry.size = 0;
        aesd_dev->buffer_entry.buffptr = NULL;
    }

    // unlock mutex once done accessing global data
    mutex_unlock(&aesd_dev->aesd_mutex);
    kfree(write_buf);

    return count;
}

/**
 * Compute the total size, in bytes, of all valid entries currently stored in the
 * circular buffer.  Caller must hold the device mutex.
 */
static loff_t aesd_total_size(struct aesd_dev *dev)
{
    loff_t total = 0;
    uint8_t index = 0;
    struct aesd_buffer_entry *entry = NULL;

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &dev->buffer, index)
    {
        if (entry->buffptr != NULL)
        {
            total += entry->size;
        }
    }
    return total;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    struct aesd_dev *dev = NULL;
    loff_t total_size = 0;
    loff_t new_pos = 0;

    if (filp == NULL)
    {
        return -EINVAL;
    }
    dev = filp->private_data;

    if (mutex_lock_interruptible(&dev->aesd_mutex))
    {
        return -ERESTARTSYS;
    }

    total_size = aesd_total_size(dev);
    new_pos = fixed_size_llseek(filp, offset, whence, total_size);

    mutex_unlock(&dev->aesd_mutex);

    PDEBUG("llseek offset=%lld whence=%d -> new_pos=%lld (total=%lld)",
           offset, whence, new_pos, total_size);

    return new_pos;
}

/**
 * Adjust @filp->f_pos based on the requested write_cmd index and offset.
 * Returns 0 on success, -EINVAL when the requested command or offset is
 * outside the bounds of the data currently stored in the circular buffer.
 * Caller must NOT hold the device mutex; this function acquires it.
 */
static long aesd_adjust_file_offset(struct file *filp, uint32_t write_cmd, uint32_t write_cmd_offset)
{
    struct aesd_dev *dev = NULL;
    struct aesd_buffer_entry *entry = NULL;
    uint8_t index = 0;
    uint8_t count = 0;
    uint8_t cmd_index = 0;
    loff_t new_pos = 0;
    long retval = 0;

    if (filp == NULL)
    {
        return -EINVAL;
    }
    dev = filp->private_data;

    if (write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
    {
        return -EINVAL;
    }

    if (mutex_lock_interruptible(&dev->aesd_mutex))
    {
        return -ERESTARTSYS;
    }

    // Determine the number of currently valid entries in the circular buffer.
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &dev->buffer, index)
    {
        if (entry->buffptr != NULL)
        {
            count++;
        }
    }

    if (write_cmd >= count)
    {
        retval = -EINVAL;
        goto out;
    }

    // Sum up entries before the requested write_cmd, walking from out_offs.
    for (cmd_index = 0; cmd_index < write_cmd; cmd_index++)
    {
        uint8_t pos = (dev->buffer.out_offs + cmd_index) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        new_pos += dev->buffer.entry[pos].size;
    }

    // Validate offset against the size of the requested command.
    {
        uint8_t pos = (dev->buffer.out_offs + write_cmd) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        if (write_cmd_offset >= dev->buffer.entry[pos].size)
        {
            retval = -EINVAL;
            goto out;
        }
    }

    new_pos += write_cmd_offset;
    filp->f_pos = new_pos;

out:
    mutex_unlock(&dev->aesd_mutex);
    return retval;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long retval = 0;

    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC)
    {
        return -ENOTTY;
    }
    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR)
    {
        return -ENOTTY;
    }

    switch (cmd)
    {
    case AESDCHAR_IOCSEEKTO:
    {
        struct aesd_seekto seekto;
        if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0)
        {
            retval = -EFAULT;
        }
        else
        {
            retval = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
        }
        break;
    }
    default:
        retval = -ENOTTY;
        break;
    }

    return retval;
}

struct file_operations aesd_fops = {
    .owner =          THIS_MODULE,
    .read =           aesd_read,
    .write =          aesd_write,
    .open =           aesd_open,
    .release =        aesd_release,
    .llseek =         aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl =   compat_ptr_ioctl,
#endif
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

    mutex_init(&aesd_device.aesd_mutex);

    aesd_circular_buffer_init(&aesd_device.buffer);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    uint8_t index = 0;
    struct aesd_buffer_entry *entry;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    // Referenced from aesd-circular-buffer.h

    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,index) {
        kfree(entry->buffptr);
    }
    if (aesd_device.buffer_entry.buffptr) {
        kfree(aesd_device.buffer_entry.buffptr);
    }
    mutex_destroy(&aesd_device.aesd_mutex);

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
