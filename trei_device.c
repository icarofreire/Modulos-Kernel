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

/*
	Modo para carregar o modulo com os parametros:
		insmod <trei_device.ko> <teste_parametro>=75
	Com array:
		insmod <trei_device.ko> <array_parametro>=75,98,46
*/

#define DEVICE_NAME_1     "mod_1"
#define DEVICE_NAME_2     "mod_2" /* criar /sys/class/DEVICE_NAME_2/ */
#define DEV_NAME			"modulo_dev"
#define NUM_DISPOSITIVOS	1 /* números de arquivos do dispositivo */

#define TAM_MSG 100//1024
static char msg[TAM_MSG];


#define ERRO(P)\
if(IS_ERR_OR_NULL(P)){\
	printk(KERN_ERR "Erro ou Nulo");\
	return PTR_ERR(P);\
}

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

static int teste_parametro = -1;
static int array_parametro[3] = {-1, -1, -1};
static int contador_array_parametro = 0; /* O endereço desta variável será passada para contar o numero de parametros passados como array. */

module_param(teste_parametro, int, 0764);
module_param_array(array_parametro, int, &contador_array_parametro, 0764);

/* O terceito parametro é a permissão no linux para esta variável;
	neste caso, 764:

|	Permissão |	Binário |	Decimal |
|		--- 		|		000		|		0			|
|		--x 		|		001		|		1			|
|		-w- 		|		010		|		2			|
|		-wx 		|		011		|		3			|
|		r-- 		|		100		|		4			|
|		r-x 		|		101		|		5			|
|		rw- 		|		110		|		6			|
|		rwx 		|		111		|		7			|

	7: A primeira parte significa permissões do proprietário.
	6: A segunda parte significa permissões do grupo ao qual o usuário pertence.
	4: A terceira parte significa permissões para os demais usuários.
*/


/* driver initialization */
static int __init modulo_init(void)
{
    int ret;
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
    dev_ret = device_create(modulo_class, NULL, modulo_dev_number, NULL, DEV_NAME);
    if( IS_ERR(dev_ret) )
    {
		printk(KERN_ERR "Erro ao criar o device em /dev.");
		class_destroy(modulo_class);
		unregister_chrdev_region(modulo_dev_number, 1);
		return PTR_ERR(dev_ret);
	}

    printk(KERN_INFO "Driver inicializado.\n");
    /* ... */

		if( teste_parametro != -1 ){
			printk(KERN_INFO "Parametro int passado: %d\n", teste_parametro);
		}else{
			printk(KERN_INFO "Nenhum Parametro 'teste_parametro' passado: %d\n", teste_parametro);
		}

		if( contador_array_parametro > 0 ){
			printk(KERN_INFO "Tamanho array passado por parametro: %d\n", contador_array_parametro);
			int p;
			for(p = 0; p<contador_array_parametro; p++){
				printk(KERN_INFO "Array[%d] = %d\n", p, array_parametro[p]);
			}
		}else{
			printk(KERN_INFO "Nenhum array passado por parametro.\n");
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

    printk(KERN_INFO "Exiting leds driver.\n");
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
