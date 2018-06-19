#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/usb.h>

#define ERRO(x)\
if( IS_ERR_OR_NULL(x) ){\
    pr_err("%s: Erro ou Nullo\n", __func__ );\
    return PTR_ERR(x);\
}

/* 
	* Desrregistrar o modulo usb_storage (rmmod usb_storage), que esta associado a um drive de USB para armazenamento de costume.
	* a fim de obter o nosso motorista associado a essa interface, é preciso descarregar o driver usb-storage (ie, rmmod usb-storage ) e reconecte 		* o pen drive.
*/

static char* msg[100]={0};
static short readPos=0;

/* Par de identificação do produto criado pelo fornecedor. */
#define VENDOR_ID	0x0781
#define PRODUCT_ID	0x5567

/* Nome de meu device driver */
#define DEVICENAME "driver_icaro"

/* Nome do dispositivo que será registrado no device model, e criado automaticamente no diretorio /dev; */
#define NOME_ARQUIVO_DEV "hello"

/* prototipo para as funções da estrutura file_operations */
static int dev_open(struct inode *, struct file * ); 
static int dev_release( struct inode *, struct file * ); 
static ssize_t dev_read(struct file *, char *, size_t, loff_t * ); 
static ssize_t dev_write(struct file *, const char *, size_t, loff_t * );

static int conectar_usb(struct usb_interface *, const struct usb_device_id *);
static void desconectar_usb(struct usb_interface *);


static struct usb_device_id usb_table[] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{}
};
MODULE_DEVICE_TABLE(usb, usb_table);

/* definida a estrutura de operações em arquivo no device driver */
static struct file_operations _fops = {
    .owner      = THIS_MODULE,
    .open       = dev_open,
    .release    = dev_release,
    .read       = dev_read,
    .write      = dev_write,
};

/* estrutura de definição do device driver de usb para o subsistema linux. */
static struct usb_driver _usb = {
	 .name        = "ola_usb",
     .probe       = conectar_usb,
     .disconnect  = desconectar_usb,
     .id_table    = usb_table,
};


/* estrutura criada para criar um dispositivo dinamicamente no diretorio /dev;
   esta estrutura, registra o drive no device model do kernel( conjunto de objetos do kernel, chamados internamente de kobjects );
 */
static struct class *eeprom_class;

/* estrutura para representar um dispositivo de caractere; registro de char drive. */
struct cdev mydriver_cdev;

/* O kernel armazena as informações de major e minor number no tipo de dados dev_t */
static dev_t mydriver_dev;


static int dev_open(struct inode *inode, struct file *fp )
{
	printk(KERN_INFO "Device aberto com sucesso.\n");
	return 0;
}

static int dev_release( struct inode *inode, struct file *fp )
{
	/* return OK */
	return 0;
}

static int conectar_usb(struct usb_interface *interface, const struct usb_device_id *id)
{
	printk(KERN_ALERT "USB CONECTADO.\n");
	return 0;
}

static void desconectar_usb(struct usb_interface *interface)
{
	printk(KERN_ALERT "USB Desconectado.\n");
}

static int hello_init(void)
{
	int re;
    printk(KERN_ALERT "Hello, world icaro\n");

	/* registrando um device number dinamicamente. */
	if( alloc_chrdev_region(&mydriver_dev, 0, 1, DEVICENAME) )
	{
		pr_err("Erro ao registrar device number dinamicamente.\n");	
	}else{
		/* inicializa a estrutura que representa um char drive */
		cdev_init(&mydriver_cdev, &_fops);
		if(cdev_add(&mydriver_cdev, mydriver_dev, 1)){ /* adiciona o dispositivo(char drive) ao sistema */
			pr_err("Erro ao registrar char drive.\n");
		}
		/* /\ depois desta chamada, o kernel associará o major/minor number registrado com as operações
		   em arquivo definidas. seu dispositivo esta pronto a ser usado! */
		
		// registra o dispositivo no device model;
		eeprom_class = class_create(THIS_MODULE, NOME_ARQUIVO_DEV);

		// gera o evento uevent, criando o dispositivo automaticamente;
		device_create(
					eeprom_class, NULL, 
					mydriver_dev,
					NULL, NOME_ARQUIVO_DEV
				   );

	// ... 
	
		re = usb_register(&_usb);
		if( re < 0 ){
			pr_err("Erro ao registrar a estrutura de USB.\n");
			return -1;
		}
		
	}

    return 0;
}
 
static void hello_exit(void)
{
	/* desregista a estrutura de usb. */
	usb_deregister(&_usb);
	
	cdev_del(&mydriver_cdev); /* desregistra o char drive */
	unregister_chrdev_region(mydriver_dev, 1); /* libera o major/minor number alocado. */

	/* \/ ker­nel envie uma men­sagem para o mdev remover os arquivos de dis­pos­i­tivo pre­vi­a­mente criados. */
	device_destroy(eeprom_class, mydriver_dev); // remove o dispositivo criado dinamicamente.
	class_destroy(eeprom_class); // destroi a classe do dispositivo.

    printk(KERN_ALERT "Goodbye, cruel world\n");
}


static ssize_t dev_read(struct file *fp, char *buff, size_t length, loff_t *ppos)
{
	short count = 0;
	while( (length) && (msg[readPos] != 0) )
	{
		//put_user(msg[readPos], buff++);
		if( !copy_to_user(buff++, &msg[readPos], 1) )
		{		
			count++;
			length--;
			readPos++;
		}else{			
			return -EFAULT;		
		}	
	}

	return count;
	
}

static ssize_t dev_write(struct file *fp, const char *buff, size_t length, loff_t *ppos )
{
	short ind = length-1;
	short count = 0;
	
	//msg = kmalloc(100, GFP_KERNEL);
	unsigned long ret = copy_from_user(msg, buff, length);
	if(ret){
		return -EFAULT;
		//printk(KERN_ALERT "Erro ao alocar memoria\n");
	}else{

		//memset(msg,0,100);
		readPos = 0;
	
		while( (length > 0) )
		{
			msg[count++] = buff[ind--];
			length--;	
		}
	}
	return count;
}

 
module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Icaro freire");
MODULE_DESCRIPTION("treinamento de device driver; Conexão USB.");


