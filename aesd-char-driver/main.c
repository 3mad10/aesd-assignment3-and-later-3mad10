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
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("3mad10"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev* dev;
    struct aesd_circular_buffer buffer;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    dev->buffer = kmalloc(sizeof(struct aesd_circular_buffer) ,GFP_KERNEL);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    struct aesd_dev* dev = (struct aesd_dev*)filp->private_data;
    for (int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
    {
        kfree(&(dev->buffer->entry[i].buffptr));
        kfree(&(dev->buffer->entry[i]));
    }
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    char* to;
    struct aesd_buffer_entry* read_entry;
    size_t entry_byte_off = 0;
    size_t count_read_bytes = 0;
    size_t current_off = 0;
    int iter;
    struct aesd_circular_buffer* circular_buffer = (struct aesd_circular_buffer*) filp->private_data;

    to = kmalloc(count ,GFP_KERNEL);
    if (!to) {
        goto out;
    }
    
    while(count > 0) {
        read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(
                                                                    circular_buffer,
                                                                    *f_pos + current_off,
                                                                    &entry_byte_off
                                                                    );
        for(iter = 0; iter < read_entry->size; iter++)
        {
            to[current_off + iter] = read_entry->buffptr[iter];
        }
        current_off = current_off + read_entry->size;
        count = count - read_entry->size;

    }
    retval = 0;
    out:
        return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    char* to;
    struct aesd_buffer_entry* popped_entry;
    struct aesd_buffer_entry* new_entry;
    struct aesd_circular_buffer* circular_buffer = (struct aesd_circular_buffer*) filp->private_data;
    to = kmalloc(count ,GFP_KERNEL);
    if (!to) {
        goto out1;
    }
    new_entry = kmalloc(sizeof(struct aesd_buffer_entry) ,GFP_KERNEL);
    if (!new_entry) {
        goto out2;
    }
    if(copy_from_user(to, buf, count))
    {
        retval = -EFAULT;
        goto out2;
    }
    new_entry->buffptr = to;
    new_entry->size = count;
    popped_entry = aesd_circular_buffer_add_entry(circular_buffer, new_entry);
    if(popped_entry) {
        kfree(popped_entry->buffptr);
        kfree(popped_entry);
    }
    retval = 0;
    goto out;
    out2:
        kfree(new_entry);
    out1:
        kfree(to);
    out:
        return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
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
    
    // mutex_init(&aesd_device.lock);
    result = aesd_setup_cdev(&aesd_device);
    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
