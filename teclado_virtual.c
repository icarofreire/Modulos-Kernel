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


static char* msg[100]={0};
static short readPos=0;

const char frase[] = { KEY_P, KEY_A, KEY_R, KEY_K, KEY_O, KEY_U, KEY_R, KEY_SPACE,  /* PARKOUR */
					   KEY_A, KEY_R, KEY_A, KEY_C, KEY_A, KEY_J, KEY_U, KEY_SPACE,  /* ARACAJU */
					   KEY_K, KEY_E, KEY_R, KEY_N, KEY_E, KEY_L, 					/* KERNEL */
					   KEY_ENTER };

/* converter um inteiro representando segundos para milisegundos. */
unsigned int esperar(unsigned int segundos)
{
	return (segundos * 1000); // p/ milisegundos.
}

/* Nome de meu device driver */
#define DEVICENAME "driver_icaro"

/* Nome do dispositivo que será registrado no device model, e criado automaticamente no diretorio /dev; */
#define NOME_ARQUIVO_DEV "hello"

/* prototipo para as funções da estrutura file_operations */
static int dev_open(struct inode *, struct file * ); 
static int dev_release( struct inode *, struct file * ); 
static ssize_t dev_read(struct file *, char *, size_t, loff_t * ); 
static ssize_t dev_write(struct file *, const char *, size_t, loff_t * );

static int iniciar_tarefa(void*);

/* definida a estrutura de operações em arquivo no device driver */
static struct file_operations leds_fops = {
    .owner      = THIS_MODULE,
    .open       = dev_open,
    .release    = dev_release,
    .read       = dev_read,
    .write      = dev_write,
};

/* estrutura para criar uma thread do kernel */
static struct task_struct *tarefa;

/* estrutura para entrada do teclado */
static struct input_dev *entrada;

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

static int dev_release( struct inode *inode, struct file *fp )
{
	/* return OK */
	return 0;
}

// ---

static int iniciar_tarefa(void* u)
{
	int i;
	while(!kthread_should_stop())
	{
		for(i = 0; i < sizeof(frase); i++)
		{
			input_report_key(entrada, frase[i], 1);
			input_report_key(entrada, frase[i], 0);
			input_sync(entrada);
		}
		printk(KERN_ALERT "Frase escrita.\n");		
		msleep(esperar(15));
	}

	return (0);
}


static int hello_init(void)
{
    printk(KERN_ALERT "Hello, world icaro\n");

	/* registrando um device number dinamicamente. */
	if( alloc_chrdev_region(&mydriver_dev, 0, 1, DEVICENAME) )
	{
		pr_err("Erro ao registrar device number dinamicamente.\n");	
	}else{
		/* inicializa a estrutura que representa um char drive */
		cdev_init(&mydriver_cdev, &leds_fops);
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
		
		
		entrada = input_allocate_device();
		if(!entrada){
			printk("Erro ao alocar memoria para o device de entrada.");
			return -ENOMEM;
		}
		
		static const char *name = "Teclado Virtual";
		entrada->name = name;
		set_bit(EV_KEY, entrada->evbit);
		
		int i;
		for (i = 0; i < 256; i++)
        {
        	set_bit(i, entrada->keybit);
        }
        
        input_register_device(entrada); /* <= registra a estrutura do dispositivo de entrada */
		tarefa = kthread_run(iniciar_tarefa, NULL, "%s", "iniciar_tarefa");
		if( IS_ERR_OR_NULL(tarefa) )
		{
			pr_err("Erro ao criar thread.");
			return PTR_ERR(tarefa);
		}
		
	}

    return 0;
}
 
static void hello_exit(void)
{

	kthread_stop(tarefa);
	input_unregister_device(entrada);
	
	cdev_del(&mydriver_cdev); /* desregistra o char drive */
	unregister_chrdev_region(mydriver_dev, 1); /* libera o major/minor number alocado. */

	/* \/ ker­nel envie uma men­sagem para o mdev remover os arquivos de dis­pos­i­tivo pre­vi­a­mente criados. */
	device_destroy(eeprom_class, mydriver_dev); // remove o dispositivo criado dinamicamente.
	class_destroy(eeprom_class); // destroi a classe do dispositivo.

    printk(KERN_ALERT "Goodbye, cruel world\n");
}

 
module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Icaro freire");
MODULE_DESCRIPTION("treinamento de device driver; Teclado Virtual.");


