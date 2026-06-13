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


static bool has_newline(const char *buffer, size_t length) {
    const char *ptr = buffer;
    for(int i = 0; i < length; i++) {
        PDEBUG("current char : %c", buffer[i]);
        if(buffer[i] == '\n') 
        {
            PDEBUG("found new line");
            return true;
        }
    }
    return false;
}

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev* dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
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
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    char* to;
    struct aesd_buffer_entry* read_entry;
    size_t entry_byte_off = 0;
    size_t count_read_bytes = 0;
    int iter;
    PDEBUG("h1");
    struct aesd_dev* dev = (struct aesd_dev*) filp->private_data;
    struct aesd_circular_buffer* circular_buffer = dev->buffer;
    PDEBUG("h2");
    if (mutex_lock_interruptible(&dev->lock))
    {
        retval = -ERESTARTSYS;
        goto out;
    }
    to = kmalloc(count ,GFP_KERNEL);
    if (!to) {
        goto out;
    }
    PDEBUG("h3");
    while(count > 0) {
        PDEBUG("count : %d", count);
        read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(
                                                                    circular_buffer,
                                                                    *f_pos + count_read_bytes,
                                                                    &entry_byte_off
                                                                    );
        if (!read_entry)
        {
            break;
        }
        for(iter = 0; iter < read_entry->size; iter++)
        {
            to[count_read_bytes + iter] = read_entry->buffptr[iter];
        }
        count_read_bytes = count_read_bytes + read_entry->size;
        count = count - read_entry->size;
        PDEBUG("count_read_bytes : %d", count_read_bytes);
        PDEBUG("new count : %d", count);

    }
    if(copy_to_user(buf, to, count_read_bytes))
    {
        retval = -EFAULT;
        goto out;
    }
    PDEBUG("h8");
    retval = count_read_bytes;
    *f_pos = *f_pos + count_read_bytes;
    out:
        mutex_unlock(&dev->lock);
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
    char* kernel_buff;
    const char* popped_entry;
    struct aesd_buffer_entry* new_entry;
    static bool ongoing_reception = false;
    struct aesd_dev* dev = (struct aesd_dev*) filp->private_data;
    struct aesd_circular_buffer* circular_buffer = dev->buffer;
    bool new_line_exist;
    if (mutex_lock_interruptible(&dev->lock))
    {
        retval = -ERESTARTSYS;
        goto out;
    }
    kernel_buff = kmalloc(count ,GFP_KERNEL);
    if (!kernel_buff) {
        goto out;
    }
    PDEBUG("h2");
    if(copy_from_user(kernel_buff, buf, count))
    {
        PDEBUG("h3");
        retval = -EFAULT;
        goto free_kernel_buff;
    }
    new_line_exist = has_newline(kernel_buff, count);
    PDEBUG("h4");
    // no current reception ongoing and new line in received data add in buffer
    if (!ongoing_reception && new_line_exist)
    {
        PDEBUG("!ongoing_reception && new_line_exist");
        new_entry = kmalloc(sizeof(struct aesd_buffer_entry) ,GFP_KERNEL);
        if (!new_entry) {
            goto free_kernel_buff;
        }
        new_entry->buffptr = kernel_buff;
        new_entry->size = count;
        popped_entry = aesd_circular_buffer_add_entry(circular_buffer, new_entry);
    } // no current reception ongoing and no new line in received data start a new reception cycle
    else if(!ongoing_reception && !new_line_exist)
    {
        PDEBUG("!ongoing_reception && !new_line_exist");
        dev->new_entry = kmalloc(sizeof(struct aesd_buffer_entry) ,GFP_KERNEL);
        if (!dev->new_entry) {
            goto free_kernel_buff;
        }
        dev->new_entry->buffptr = kernel_buff;
        dev->new_entry->size = count;
        ongoing_reception = true;
    } // current reception ongoing and no new line in received data continue reception
    else if (ongoing_reception && !new_line_exist)
    {
        PDEBUG("ongoing_reception && !new_line_exist");
        PDEBUG("temp_entry->buffptr : %x", dev->new_entry->buffptr);
        krealloc(dev->new_entry->buffptr, dev->new_entry->size + count, GFP_KERNEL);
        if (!dev->new_entry->buffptr) {
            goto free_kernel_buff;
        }
        PDEBUG("start copying");
        for(int i = 0; i < count; i++)
        {
            PDEBUG("copy %c from kernel_buff[%d] to temp_entry->buffptr[%d + %d]", kernel_buff[i], i, dev->new_entry->size, i);
            dev->new_entry->buffptr[dev->new_entry->size+i] = kernel_buff[i];
        }
        PDEBUG("finished copy");
        dev->new_entry->size += count;
    } // current reception ongoing and new line in received data finish reception cycle
    else
    {
        PDEBUG("ongoing_reception && new_line_exist");
        krealloc(dev->new_entry->buffptr, dev->new_entry->size + count  ,GFP_KERNEL);
        if (!dev->new_entry->buffptr) {
            goto free_kernel_buff;
        }
        for(int i = 0; i < count; i++)
        {
            dev->new_entry->buffptr[dev->new_entry->size+i] = kernel_buff[i];
        }
        
        dev->new_entry->size += count;
        popped_entry = aesd_circular_buffer_add_entry(circular_buffer, dev->new_entry);
        ongoing_reception = false;
    }
    PDEBUG("h5");
    if(popped_entry) {
        kfree(popped_entry);
    }
    PDEBUG("h6");
    retval = *f_pos + count;
    goto out;
    free_kernel_buff:
        kfree(kernel_buff);
    out:
        mutex_unlock(&dev->lock);
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
    
    mutex_init(&aesd_device.lock);
    aesd_device.buffer = kmalloc(sizeof(struct aesd_circular_buffer) ,GFP_KERNEL);

    result = aesd_setup_cdev(&aesd_device);
    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    
    struct aesd_circular_buffer* circular_buffer = aesd_device.buffer;
    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    for (int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
    {
        kfree(circular_buffer->entry[i].buffptr);
    }
    kfree(circular_buffer);
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
