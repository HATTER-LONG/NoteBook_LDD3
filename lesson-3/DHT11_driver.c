#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/gpio.h>

/*三星平台的GPIO配置函数头文件*/
/*三星平台EXYNOS系列平台，GPIO配置参数宏定义头文件*/
#include <plat/gpio-cfg.h>
#include <mach/gpio.h>

/*三星平台4412平台，GPIO宏定义头文件*/
#include <mach/gpio-exynos4.h>

#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/time.h>
#include <linux/irq.h>
#include <asm-generic/uaccess.h>

#define DEVICE_NAME "DHT11_DEVICE"
#define DHT11_PIN EXYNOS4_GPX0(1)
#define DEVICE_MAJOR 0
#define DEVICE_NUM 1

struct dht11_sensor_dev{
	unsigned long pin;
	unsigned char value[5];
	unsigned int irq;	
	int bitcount;
	int bytecount;
	int started;
	int signal;
	int time;
	int count;
	dev_t devno;
	struct class *dht11_class;
	struct cdev cdev;
	struct mutex read_data;
	struct timeval lasttv;
	struct timeval tv;
	struct work_struct dht11_work;
};

static struct dht11_sensor_dev *dht11_dev;


#define Set_DHT11_IOout(PIN,data)		s3c_gpio_cfgpin(PIN, S3C_GPIO_OUTPUT);\
										gpio_set_value(PIN, data)
									
#define Set_DHT11_IOin(PIN)		s3c_gpio_cfgpin(PIN, S3C_GPIO_INPUT)

#if 0
static void dht11_data_factory(struct work_struct *work)
{
		long deltv = 0;
		mutex_lock(&dht11_dev->read_data);
		dht11_dev->count++;
		deltv = dht11_dev->tv.tv_sec - dht11_dev->lasttv.tv_sec;
		dht11_dev->time = (int)(deltv * 1000000 + (dht11_dev->tv.tv_usec - dht11_dev->lasttv.tv_usec)); 
		dht11_dev->lasttv = dht11_dev->tv;
		if((dht11_dev->signal == 1) && (dht11_dev->time>40))
		{
			dht11_dev->started = 1;		
		}
		else
		{
		   if((dht11_dev->signal == 0) && (dht11_dev->started == 1))
		  {		
				dht11_dev->started = 0;
				if((dht11_dev->time>20)&&dht11_dev->time <38)
				{
					dht11_dev->bitcount++;
					if(dht11_dev->bitcount == 8)
					{
						dht11_dev->bitcount = 0;
						dht11_dev->bytecount++;
					}
				}
				if ((dht11_dev->time>55)&& (dht11_dev->time<80))
				{
					dht11_dev->value[dht11_dev->bytecount] = dht11_dev->value[dht11_dev->bytecount] | (0x80 >> dht11_dev->bitcount);			
					dht11_dev->bitcount++;
					if(dht11_dev->bitcount == 8)
					{
					dht11_dev->bitcount = 0;
					dht11_dev->bytecount++;
					}
				}
		 	}
		
		}
		mutex_unlock(&dht11_dev->read_data);
}
#endif

static irqreturn_t dht11_interrupt_hander(int irq, void *dev_id)
{
	/*revise by Hatter*/
	struct dht11_sensor_dev *dev = (struct dht11_sensor_dev *)dev_id;
	dev->signal = gpio_get_value(dev->pin);
	do_gettimeofday(&dev->tv);	
	//schedule_work(&dht11_dev->dht11_work);
	long deltv = 0;
		//mutex_lock(&dht11_dev->read_data);
		dht11_dev->count++;
		deltv = dht11_dev->tv.tv_sec - dht11_dev->lasttv.tv_sec;
		dht11_dev->time = (int)(deltv * 1000000 + (dht11_dev->tv.tv_usec - dht11_dev->lasttv.tv_usec)); 
		dht11_dev->lasttv = dht11_dev->tv;
		if((dht11_dev->signal == 1) && (dht11_dev->time>40))
		{
			dht11_dev->started = 1;		
			return IRQ_HANDLED;
		}
		else
		{
		   if((dht11_dev->signal == 0) && (dht11_dev->started == 1))
		  {		
				dht11_dev->started = 0;
				if((dht11_dev->time>20)&&dht11_dev->time <38)
				{
					dht11_dev->bitcount++;
					if(dht11_dev->bitcount == 8)
					{
						dht11_dev->bitcount = 0;
						dht11_dev->bytecount++;
					}
					return IRQ_HANDLED;
				}
				if ((dht11_dev->time>55)&& (dht11_dev->time<80))
				{
					dht11_dev->value[dht11_dev->bytecount] = dht11_dev->value[dht11_dev->bytecount] | (0x80 >> dht11_dev->bitcount);			
					dht11_dev->bitcount++;
					if(dht11_dev->bitcount == 8)
					{
					dht11_dev->bitcount = 0;
					dht11_dev->bytecount++;
					}
					
				}
		 	}
		
		}
		//mutex_unlock(&dht11_dev->read_data);

	return IRQ_HANDLED;
}

static int dht11_start(void)
{
    if(gpio_request(dht11_dev->pin, DEVICE_NAME))
	{  
        printk(KERN_INFO "[%s] gpio_request \n", __func__);  
        return -1;  
    }
	/* revise by Hatter */
	//gpio_direction_output(dht11_dev->pin, 0);	
	Set_DHT11_IOout(dht11_dev->pin, 0);
	/*revise end*/
	
	msleep(30);
	gpio_set_value(dht11_dev->pin,1);
	udelay(30);
	
	/*revise by Hatter*/
	//gpio_direction_input(dht11_dev->pin);
	Set_DHT11_IOin(dht11_dev->pin);
	/*revise end*/
	
	gpio_free(dht11_dev->pin);
	do_gettimeofday(&dht11_dev->lasttv);
	dht11_dev->count=0;
	return 0;
}


static int dht11_setup_interrupts(void)  
{  
    int result;    
    dht11_dev->irq = gpio_to_irq(dht11_dev->pin);  
    result = request_irq(dht11_dev->irq, dht11_interrupt_hander,   
            IRQ_TYPE_EDGE_BOTH, DEVICE_NAME, (void *)dht11_dev);  
  
    switch (result) 
	{  
        case -EBUSY:  
            printk(KERN_ERR "*%s(): IRQ %d is busy\n", __func__, dht11_dev->irq);  
            return -EBUSY;  
        case -EINVAL:  
            printk(KERN_ERR "*%s(): Bad irq number or handler\n", __func__);  
            return -EINVAL;  
        default:  
		//	printk("request irq for dht11 ok\n");
            return result;   
    }  
    
}  


static int dht11_clear_interrupts(void)  
{  
   free_irq(dht11_dev->irq, (void *)dht11_dev);
   msleep(20);
   if(gpio_request(dht11_dev->pin, DEVICE_NAME)){  
        printk(KERN_INFO "[%s] gpio_request \n", __func__);  
        return -1;  
    }
   /*revise by Hatter*/
   //gpio_direction_output(dht11_dev->pin, 1);
	Set_DHT11_IOout(dht11_dev->pin, 1);
   /*revise end*/
   gpio_free(dht11_dev->pin);
   return 1;
}  

static int dht11_checksum(struct dht11_sensor_dev *dev)
{
	int tmp = 0;
	tmp = dev->value[0] + dev->value[1] + dev->value[2] + dev->value[3];
	
	if(tmp != dev->value[4]){
		printk(KERN_INFO "[%s] %d %d\n", __func__, dev->value[4], tmp);
		return -1;
	}
	return 1;
}

static int dht11_sensor_open(struct inode *inode, struct file *filp)
{
	printk("dht11_sensor_open\n");	
	try_module_get(THIS_MODULE);
	return 0;
}

static ssize_t dht11_sensor_read(struct file *filp,char __user *buf,size_t size,loff_t *f_pos)
{
	int result = 0;
	dht11_dev->started = 0;
	dht11_dev->bitcount = 0;
	dht11_dev->bytecount = 0;
	dht11_dev->value[0] = 0;
	dht11_dev->value[1] = 0;
	dht11_dev->value[2] = 0;
	dht11_dev->value[3] = 0;
	dht11_dev->value[4] = 0;
 
	dht11_start();
	dht11_setup_interrupts();
	msleep(10);
	dht11_clear_interrupts(); 
	result=dht11_checksum(dht11_dev);
	if(result<0)
			return -EAGAIN; 

	printk("Humidity=%d.%d%%---Temperature=%d.%dC\n",\
			dht11_dev->value[0], dht11_dev->value[1],\
			dht11_dev->value[2], dht11_dev->value[3]);
	printk("count =%d\n",dht11_dev->count);
    result=copy_to_user(buf,&dht11_dev->value,4);
    if(result<0)
	{
         printk("copy to user err\n");
         return -EAGAIN;
    } 
    return  result;           
}

static int dht11_sensor_release(struct inode *inode,struct file *filp)
{
	module_put(THIS_MODULE);
	gpio_free(dht11_dev->pin);
	dht11_clear_interrupts();
	printk("dht11_sensor_release\n");
	return 0;
}

static struct file_operations dht11_sensor_fops={
	.owner   = THIS_MODULE,
	.open    = dht11_sensor_open,
	.read    = dht11_sensor_read,
	.release = dht11_sensor_release,
};

static int dht11_gpio_init(void)
{
	int result;
	dht11_dev->pin = DHT11_PIN;
    result=gpio_request(dht11_dev->pin, DEVICE_NAME);
	if(result)
	{
		printk(KERN_INFO "[%s] gpio_request \n", __func__);
		return -1;
	}
	
	/*revise by Hatter*/
	//gpio_direction_output(dht11_dev->pin,1);
	Set_DHT11_IOout(dht11_dev->pin,1);
	/*revise end*/
	
	gpio_free(dht11_dev->pin);
	return 0;
}

static int dht11_sensor_setup_cdev(void)
{
	int ret;
	cdev_init(&(dht11_dev->cdev), &dht11_sensor_fops);
	dht11_dev->cdev.owner = THIS_MODULE;
	ret=cdev_add(&(dht11_dev->cdev),dht11_dev->devno, 1);
	if(ret)
	{
		printk(KERN_NOTICE"erro %d adding %s\n",ret,DEVICE_NAME);
	}
	return ret;
}

int __init dht11_sensor_init(void)
{
	int result;
	dht11_dev=kmalloc(sizeof(struct dht11_sensor_dev),GFP_KERNEL);
	if(!dht11_dev)
	{
		result=-ENOMEM;
		goto allocate_memory_fail;
	}
	dht11_dev->devno = DEVICE_MAJOR;
    if(DEVICE_MAJOR)
	{
		result = register_chrdev_region(dht11_dev->devno, DEVICE_NUM, DEVICE_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&dht11_dev->devno, DEVICE_MAJOR, DEVICE_NUM, DEVICE_NAME);
	}
	if(result < 0)
	{
	    printk("register_chrdev_region err!\n");
		goto chardev_region_fail;
	}
	dht11_dev->dht11_class = class_create(THIS_MODULE, DEVICE_NAME);
	if(IS_ERR(dht11_dev->dht11_class))
    {
             printk("Err: failed in creating class.\n");
             goto class_create_fail;
    } 
	/*creates a device and registers it with sysfs*/
	device_create(dht11_dev->dht11_class, NULL,dht11_dev->devno, NULL, DEVICE_NAME);
	result=dht11_sensor_setup_cdev();
	if(result<0)
		goto cdev_fail;
	result=dht11_gpio_init();
	if(result<0)
		goto gpio_request_fail;
	
	/*revise by Hatter*/
	//INIT_WORK(&dht11_dev->dht11_work,dht11_data_factory); 
	/*revise end*/
	
	mutex_init(&dht11_dev->read_data);
	printk("dht11 init ok!\n");
	return 0;
	
gpio_request_fail:
	cdev_del(&dht11_dev->cdev);
cdev_fail:
	device_destroy(dht11_dev->dht11_class,dht11_dev->devno);
	class_destroy(dht11_dev->dht11_class);
class_create_fail:
	unregister_chrdev_region(dht11_dev->devno,1);
chardev_region_fail:
allocate_memory_fail:
	kfree(dht11_dev);
	return result;
}

void  __exit dht11_sensor_exit(void)
{
	cdev_del(&dht11_dev->cdev);
	device_destroy(dht11_dev->dht11_class,dht11_dev->devno);
	class_destroy(dht11_dev->dht11_class);	
	unregister_chrdev_region(dht11_dev->devno, 1);
	kfree(dht11_dev);
}

module_init(dht11_sensor_init);
module_exit(dht11_sensor_exit);

MODULE_AUTHOR("jvaemape");
MODULE_DESCRIPTION("DHT11 Driver");
MODULE_LICENSE("Dual BSD/GPL");