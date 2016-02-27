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
#include <linux/pci.h>


#define DEVICE_NAME_1     "mod_1"
#define DEVICE_NAME_2     "mod_2"
#define NUM_DISPOSITIVOS	1 /* números de arquivos do dispositivo */

#define TAM_MSG 100//1024
static char msg[TAM_MSG];

#define ERRO(P)\
if(IS_ERR_OR_NULL(P)){\
	printk(KERN_ERR "Erro ou Nulo na linha: [%d].\n", __LINE__);\
	return PTR_ERR(P);\
}

static void __iomem *vram;

/* Endereço virtual;
	obtido por "cat /proc/iomem", para listar o mapa de memória do sistema:
	d0700000-d070ffff : 0000:00:14.0

	0000:00:14.0 <= é o PCI(Barramento) do USB do mouse;
*/
#define MOUSE_BASE	0xd0700000

#define VRAM_BASE 0xc0000000//0x000A0000


/*  Vendor do PCI;
		obtido pelo comando "sudo su;lspci -x;"
		Detectar qual o pci que deseja trabalhar, e na primeira linha descrita por "00:",
		esta descrita o vendor e o id do produto pci, nos 4 primeiros numeros hexa.
		Os 2 primeiros numeros hexa em ordem inversa, são o vendor;
		Os outros 2 numeros hexa(3º e 4º) são o id do pci(em ordem inversa);
		Ex:
		"00: 86 80 35 0f"
		vendor = 0x8086
		id_produto = 0x0f35
*/
#define PCI_VENDOR_ID	0x8086

// numero do produto do PCI;
#define PCI_PRODUTO_ID	0x0f31 //0x0f35


/* prototypes */
static int modulo_open(struct inode *inode, struct file *file);
static ssize_t modulo_read(struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t modulo_write(struct file *file, const char *buf, size_t count, loff_t *ppos);
static int modulo_release(struct inode *inode, struct file *file);


/* file operations structure */
static struct file_operations modulo_fops = {
    .owner      = THIS_MODULE,
    .open       = modulo_open,
    .release    = modulo_release,
    .read       = modulo_read,
    .write      = modulo_write,
};


struct cdev c_dev;

/* leds driver major number */
/* dev_t estrutura para criar o major e minor number */
static dev_t modulo_dev_number;

/* class for the sysfs entry */
struct class *modulo_class;



/* driver initialization */
static int __init modulo_init(void)
{
    int ret, i;
    struct device *dev_ret;

    /* request device major number */
    if ((ret = alloc_chrdev_region(&modulo_dev_number, 0, NUM_DISPOSITIVOS, DEVICE_NAME_1) < 0)) {
        printk(KERN_DEBUG "Erro ao registrar device!\n");
        return ret;
    }

    /* create /sys entry */
    modulo_class = class_create(THIS_MODULE, DEVICE_NAME_2);
    if( IS_ERR(modulo_class) )
    {
		printk(KERN_ERR "Erro em /sys entry.");
		unregister_chrdev_region(modulo_dev_number, 1);
		return PTR_ERR(modulo_class);
	}

	/* connect file operations to this device */
    cdev_init(&c_dev, &modulo_fops);
    c_dev.owner = THIS_MODULE;

    /* connect major/minor numbers */
    if ( (ret = cdev_add(&c_dev, modulo_dev_number, 1)) < 0)
    {
		printk(KERN_DEBUG "Erro ao adicionar o dispositivo!\n");
		device_destroy(modulo_class, modulo_dev_number);
		class_destroy(modulo_class);
		unregister_chrdev_region(modulo_dev_number, 1);

		return ret;
    }

    // dev_t corresponding <major, minor>
    /* send uevent to udev to create /dev node */
    dev_ret = device_create(modulo_class, NULL, modulo_dev_number, NULL, "modulo_dev");
    if( IS_ERR(dev_ret) )
    {
		printk(KERN_ERR "Erro ao criar o device em /dev.");
		class_destroy(modulo_class);
		unregister_chrdev_region(modulo_dev_number, 1);
		return PTR_ERR(dev_ret);
	}

    printk(KERN_INFO "Driver inicializado.\n");

    /* ... */

		struct pci_dev *dev;
		dev = pci_get_device(PCI_VENDOR_ID, PCI_PRODUTO_ID, NULL);
		if (dev)
		{
		    /* Use the PCI device */
				printk(KERN_INFO "PCI OK.\n");

				for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
					unsigned long len = pci_resource_len(dev, i);

					if(!IS_ERR_OR_NULL(len)){
							printk(KERN_INFO "tamanho PCI OK.\n");
							vram = ioremap(VRAM_BASE, len);
							ERRO(vram)
							/*if( vram == NULL ){
									printk(KERN_ERR "Erro ao mapear PCI.\n");
									return -ENOMEM;
							}*/
							/* ... */
							
							printk(KERN_ALERT "Device Vendor ID: 0x%X\n", dev->vendor);
        						printk(KERN_ALERT "Device Product ID: 0x%X\n", dev->device);

					}

				}

				/*...*/
		    pci_dev_put(dev);
		}else{
			printk(KERN_ALERT "PCI nao encontrado.\n");
		}


    return 0;
}

/* driver exit */
static void __exit modulo_exit(void)
{
    cdev_del(&c_dev);
    device_destroy(modulo_class, modulo_dev_number );

    /* destroy class */
    class_destroy(modulo_class);

	/* release major number */
    unregister_chrdev_region(modulo_dev_number, NUM_DISPOSITIVOS);

		iounmap(vram);

    printk(KERN_INFO "Driver removido.\n");
}

/* open file */
static int modulo_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "Driver: open()\n");

    /* return OK */
    return 0;
}

/* close file */
static int modulo_release(struct inode *inode, struct file *file)
{
    /* return OK */
    return 0;
}

/* read status */
static ssize_t modulo_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    printk(KERN_INFO "Driver: read()\n");
	printk(KERN_INFO "In chardrv read routine.\n");
	printk(KERN_INFO "O tamanho é: %lu.\n", count);
	printk(KERN_INFO "Offset é: %lu.\n", *ppos);

	 int maxbytes; // máximo de bytes que podem ser lidos a partir de *ppos para TAM_MSG;
     int bytes_p_ler; // indica o número de bytes a ser lido;
     int bytes_lidos; // número de bytes realmente lidos;

     /*
     maxbytes = TAM_MSG - *ppos;
     if( maxbytes > count ){
		bytes_p_ler = count; // se o resto dos bytes para ler do buffer, for maior que count, então não há mais bytes para ler; count será igual a zero;
	 }else{
        bytes_p_ler = maxbytes; // mais bytes para ler, existe mais bytes no buffer;
     }
     */

     maxbytes = TAM_MSG - *ppos;
     bytes_p_ler = maxbytes;
     if( maxbytes == 0 ){
		return bytes_lidos;
	 }

     if( bytes_p_ler == 0 ){ // Atingido o final do dispositivo.
		 printk(KERN_INFO "Atingido o final do dispositivo\n");
	 }

	 /* copy_to_user:
		Retorna o número de bytes que não puderam ser copiados.
		Em caso de sucesso, esta será zero.
      * */

	 bytes_lidos = bytes_p_ler - copy_to_user(buf, msg, bytes_p_ler);
	 printk(KERN_INFO "dispositivo foi lido: %d\n",bytes_lidos);
    *ppos += bytes_lidos;
     printk(KERN_INFO "dispositivo foi lido.\n");

     return bytes_lidos;
}

/* write status */
static ssize_t modulo_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    printk(KERN_INFO "Driver: write()\n");
    printk(KERN_INFO "MODULO WRITE: Offset é: %lu.\n", *ppos);

     memset(msg,0,TAM_MSG);
     int length = count;

     if (length > TAM_MSG - 1){
		length = TAM_MSG - 1;
	 }

     if (length < 0){
		return -EINVAL; /* Invalid argument */
	 }

     /* copy_from_user:
		Retorna o número de bytes que não puderam ser copiados.
		Em caso de sucesso, esta será zero.
		Se alguns dados não puderam ser copiados, esta função irá preencher os copiados
		dados para o tamanho solicitado usando zero bytes.
	  * */

     if (copy_from_user(msg, buf, length)){
		return -EFAULT; /* Bad address */
	 }

     return count;
}

module_init(modulo_init);
module_exit(modulo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Icaro Freire Martins");
MODULE_DESCRIPTION("Treinamento de Character Driver");
