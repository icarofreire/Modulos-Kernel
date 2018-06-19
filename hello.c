#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/slab.h>


static char* msg[100]={0};
static short readPos=0;

/* Nome de meu device driver */
#define DEVICENAME "driver_icaro"

/* Nome do dispositivo que será registrado no device model, e criado automaticamente no diretorio /dev; */
#define NOME_ARQUIVO_DEV "hello"

/* prototipo para as funções da estrutura file_operations */
static int dev_open(struct inode *, struct file * ); 
static int dev_release( struct inode *, struct file * ); 
static ssize_t dev_read(struct file *, char *, size_t, loff_t * ); 
static ssize_t dev_write(struct file *, const char *, size_t, loff_t * );


/* definida a estrutura de operações em arquivo no device driver */
static struct file_operations leds_fops = {
    .owner      = THIS_MODULE,
    .open       = dev_open,
    .release    = dev_release,
    .read       = dev_read,
    .write      = dev_write,
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
	if(/*!msg*/ret){
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

static int dev_release( struct inode *inode, struct file *fp )
{
	/* return OK */
	return 0;
}

// ---

static int hello_init(void)
{
    struct device *dev;
    printk(KERN_ALERT "Hello, world icaro\n");

	//register_chrdev(DRIVER_MAJOR, DEVICENAME, &mydriver_dev);

	/* registrando um device number dinamicamente. */
	if( alloc_chrdev_region(&mydriver_dev, 0, 1, DEVICENAME)
	  	//register_chrdev(DRIVER_MAJOR, DEVICENAME, &leds_fops) < 0
	  ){
		pr_err("Erro ao registrar device number dinamicamente.\n");	
	}else{
		/* inicializa a estrutura que representa um char drive */
		cdev_init(&mydriver_cdev, &leds_fops);
		if(cdev_add(&mydriver_cdev, mydriver_dev, 1)){ /* adiciona o dispositivo(char drive) ao sistema */
			pr_err("Erro ao registrar char drive.\n");
		}
		/* /\ depois desta chamada, o kernel associará o major/minor number registrado com as operações
		   em arquivo definidas. seu dispositivo esta pronto a ser usado! */
		
		printk(KERN_INFO "<Major, Minor>: <%d, %d>\n", MAJOR(mydriver_dev), MINOR(mydriver_dev));

		// registra o dispositivo no device model;
		eeprom_class = class_create(THIS_MODULE, NOME_ARQUIVO_DEV);

		// gera o evento uevent, criando o dispositivo automaticamente;
		dev = device_create(
					eeprom_class, NULL, 
					mydriver_dev,
					NULL, NOME_ARQUIVO_DEV
				   );
		if( IS_ERR(dev) ) // verifica se houve um erro ao criar o dispositivo;
		{
			printk(KERN_ALERT "Erro ao criar o arquivo /dev\n");
		}

	}

    return 0;
}
 
static void hello_exit(void)
{
	//kfree(msg);

	cdev_del(&mydriver_cdev); /* desregistra o char drive */
	unregister_chrdev_region(mydriver_dev, 1); /* libera o major/minor number alocado. */
	//unregister_chrdev (DRIVER_MAJOR, DEVICENAME);

	/* \/ ker­nel envie uma men­sagem para o mdev remover os arquivos de dis­pos­i­tivo pre­vi­a­mente criados. */
	device_destroy(eeprom_class, mydriver_dev); // remove o dispositivo criado dinamicamente.
	class_destroy(eeprom_class); // destroi a classe do dispositivo.

    printk(KERN_ALERT "Goodbye, cruel world\n");
}
 
module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Icaro freire");
MODULE_DESCRIPTION("treinamento de device driver");


