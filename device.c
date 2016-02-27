#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/err.h>


#define DEVICE_NAME_1     "leds1"
#define DEVICE_NAME_2     "leds2"

#define ERRO(P)\
if(IS_ERR_OR_NULL(P)){\
	printk(KERN_ERR "Erro ou Nulo");\
	return PTR_ERR(P);\
}

/* prototypes */
static int leds_open(struct inode *inode, struct file *file);
static ssize_t leds_read(struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t leds_write(struct file *file, const char *buf, size_t count, loff_t *ppos);
static int leds_release(struct inode *inode, struct file *file);


/* file operations structure */
static struct file_operations leds_fops = {
    .owner      = THIS_MODULE,
    .open       = leds_open,
    .release    = leds_release,
    .read       = leds_read,
    .write      = leds_write,
};


struct cdev c_dev;

/* leds driver major number */
static dev_t leds_dev_number;

/* class for the sysfs entry */
struct class *leds_class;



/* driver initialization */
static int __init leds_init(void)
{
    int ret;
    struct device *dev_ret;
        
    /* request device major number */
    if ((ret = alloc_chrdev_region(&leds_dev_number, 0, 1, DEVICE_NAME_1) < 0)) {
        printk(KERN_DEBUG "Erro ao registrar device!\n");
        return ret;
    }
    
    /* create /sys entry */
    leds_class = class_create(THIS_MODULE, DEVICE_NAME_2);
    if( IS_ERR(leds_class) )
    {
		printk(KERN_ERR "Erro em /sys entry.");
		unregister_chrdev_region(leds_dev_number, 1);
		return PTR_ERR(leds_class);
	}
	
	
	/* connect file operations to this device */
    cdev_init(&c_dev, &leds_fops);
    c_dev.owner = THIS_MODULE;
    
    /* connect major/minor numbers */
    if ( (ret = cdev_add(&c_dev, leds_dev_number, 1)) < 0) 
    {
		printk(KERN_DEBUG "Erro ao adicionar o dispositivo!\n");
		device_destroy(leds_class, leds_dev_number);
		class_destroy(leds_class);
		unregister_chrdev_region(leds_dev_number, 1);
		
		return ret;
    }
    
    
    // dev_t corresponding <major, minor>  
    /* send uevent to udev to create /dev node */
    dev_ret = device_create(leds_class, NULL, leds_dev_number, NULL, "leds_dev");
    if( IS_ERR(dev_ret) )
    {
		printk(KERN_ERR "Erro ao criar o device em /dev.");
		class_destroy(leds_class);
		unregister_chrdev_region(leds_dev_number, 1);
		return PTR_ERR(dev_ret);
	}
    
    printk(KERN_INFO "Leds driver initialized.\n");
    
    // ...
        
        
        
    
    
    // ...
    
    return 0;        
}

/* driver exit */
static void __exit leds_exit(void)
{    
    cdev_del(&c_dev);
    device_destroy(leds_class, leds_dev_number );
    
    /* destroy class */
    class_destroy(leds_class);   

	/* release major number */
    unregister_chrdev_region(leds_dev_number, 1);

    printk(KERN_INFO "Exiting leds driver.\n");
}

/* open led file */
static int leds_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "Driver: open()\n");
    
    /* return OK */
    return 0;
}

/* close led file */
static int leds_release(struct inode *inode, struct file *file)
{
    /* return OK */
    return 0;
}

/* read led status */
static ssize_t leds_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    printk(KERN_INFO "Driver: read()\n");
    
    return 0;
}

static ssize_t leds_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    printk(KERN_INFO "Driver: write()\n");
        
    return count;
}

module_init(leds_init);
module_exit(leds_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Icaro Freire Martins");
MODULE_DESCRIPTION("Treinamento de Character Driver");
