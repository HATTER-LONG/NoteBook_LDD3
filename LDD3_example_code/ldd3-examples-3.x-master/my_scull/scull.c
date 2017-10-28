#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#include "scull.h"

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_qset =    SCULLC_QSET;
int scull_quantum = SCULLC_QUANTUM;


module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);

struct scull_dev *scull_device;

MODULE_LICENSE("Dual BSD/GPL");

//追踪链表到目标位置
struct scull_qset *scull_follow(struct scull_dev *dev, int n)
{
	struct scull_qset *qs = dev->data;

        /* Allocate first qset explicitly if need be */
	if (! qs) {
		qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
		if (qs == NULL)
			return NULL;  /* Never mind */
		memset(qs, 0, sizeof(struct scull_qset));
	}

	/* Then follow the list */
	while (n--) {
		if (!qs->next) {
			qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
			if (qs->next == NULL)
				return NULL;  /* Never mind */
			memset(qs->next, 0, sizeof(struct scull_qset));
		}
		qs = qs->next;
		continue;
	}
	return qs;
}

int scull_trim(struct scull_dev *dev)
{
        struct scull_qset *next, *dptr;
        int qset = dev->qset; /* "dev" is not-null */
        int i;
        for (dptr = dev->data; dptr; dptr = next)
        { /* all the list items */
                if (dptr->data) {
                        for (i = 0; i < qset; i++)
                                kfree(dptr->data[i]);
                        kfree(dptr->data);
                        dptr->data = NULL;
                }

                next = dptr->next;
                kfree(dptr);
        }
        dev->size = 0;
        dev->quantum = scull_quantum;
        dev->qset = scull_qset;
        dev->data = NULL;
        return 0;
}

int scull_open(struct inode *inode, struct file *filp)
{
    struct scull_dev *dev;

    dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = dev;

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
    {
        if (mutex_lock_interruptible(&dev->mutex))
        {
            return -ERESTARTSYS;
        }
        scull_trim(dev);
        mutex_unlock(&dev->mutex);
    }

    return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
    return 0;
}

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr; /* the first listitem 第一个链表*/
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize = quantum * qset; /* how many bytes in the listitem  该链表项中有多少字节*/
	int item, s_pos, q_pos, rest;
	ssize_t retval = 0;

	if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;
	if (*f_pos >= dev->size)
			goto out;
	if (*f_pos + count > dev->size)
			count = dev->size - *f_pos;

	/* find listitem, qset index, and offset in the quantum 在量子集中寻找链表项、qest索引以及偏移量*/
	item = (long)*f_pos / itemsize;
	rest = (long)*f_pos % itemsize;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	/* follow the list up to the right position (defined elsewhere) 沿该链表前行、直到正确的位置（在其他地方定义）*/
	dptr = scull_follow(dev, item);
	if (dptr == NULL || !dptr->data || ! dptr->data[s_pos])
			goto out; /* don't fill holes */

	/* read only up to the end of this quantum 读取该量子的数据直到末尾*/
	if (count > quantum - q_pos)
			count = quantum - q_pos;

	if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count))
	{
			retval = -EFAULT;
			goto out;

	}
	*f_pos += count;
	retval = count;

out:
	mutex_unlock(&dev->mutex);
	return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	ssize_t retval = -ENOMEM; /* "goto out" 所使用的特殊值 */
	if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;

	/* 在量子集中寻找链表，qset索引以及偏移量 */
	item = (long)*f_pos / itemsize;
	rest = (long)*f_pos % itemsize;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	/* 沿着该链表前行，知道正确的位置（在其他地方定义的） */
	dptr = scull_follow(dev, item);
	if (dptr == NULL)
			goto out;
	if (!dptr->data)
	{
			dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
			if (!dptr->data)
					goto out;
			memset(dptr->data, 0, qset * sizeof(char *));
	}
	if (!dptr->data[s_pos])
	{
			dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
			if (!dptr->data[s_pos])

					goto out;
	}

	/* 将数据写入该量子，直到结尾 */
	if (count > quantum - q_pos)

			count = quantum - q_pos;
	if (copy_from_user(dptr->data[s_pos]+q_pos, buf, count))
	{
			retval = -EFAULT;
			goto out;

	}
	*f_pos += count;
	retval = count;

	/* 更新文本大小 */
	if (dev->size < *f_pos)
			dev->size = *f_pos;

out:
	mutex_unlock(&dev->mutex);
	return retval;

}
loff_t scull_llseek(struct file *filp, loff_t off, int whence)
{
    struct scull_dev *dev = filp->private_data;
    loff_t newpos;

    switch(whence)
    {
        case 0:
            newpos = off;
            break;
        case 1:
            newpos = filp->f_pos + off;
            break;
        case 2:
            newpos = dev->size + off;
            break;
        default:
            return -EINVAL;
    }
    if (newpos < 0)
    {
        return -EINVAL;
    }
    filp->f_pos = newpos;
    return newpos;
}

struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    .llseek = scull_llseek,
    .read = scull_read,
    .write = scull_write,
    .open = scull_open,
    .release = scull_release,    
};

void  scull_cleanup_module(void)
{
    dev_t devno = MKDEV(scull_major, scull_minor);

    if (scull_device)
    {
        scull_trim(scull_device);
        cdev_del(&scull_device->cdev);
        kfree(scull_device);    
    }
    unregister_chrdev_region(devno, 1);
}

static void scull_setup_cdev(struct scull_dev *dev)
{
    int err, devno = MKDEV(scull_major, scull_minor);

    cdev_init(&dev->cdev, &scull_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &scull_fops;
    err = cdev_add(&dev->cdev, devno, 1);

    if (err)
    {
        printk(KERN_NOTICE "Error %d adding scull", err);
    }
}

static int __init scull_init_module(void)
{
    int result;
    dev_t dev = 0;

    if (scull_major)    
    {
        dev = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(dev, 1, "scull");
    }
    else
    {
        result = alloc_chrdev_region(&dev, scull_minor, 1, "scull");
        scull_major = MAJOR(dev);
    }
    if (result < 0)
    {
        printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
        return result;
    }

    scull_device = kmalloc(sizeof(struct scull_dev), GFP_KERNEL);        
    if (!scull_device)
    {
        result = -ENOMEM;
        goto fail;
    }
    memset(scull_device, 0, sizeof(struct scull_dev));

//    sema_init(&scull_device->sem，1); 

    scull_setup_cdev(scull_device);

    return 0;

    fail:
        scull_cleanup_module();
        return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);

