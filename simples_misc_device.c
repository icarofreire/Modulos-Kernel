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
#include <linux/kthread.h>
#include <linux/delay.h>

#include <linux/miscdevice.h>

/*
	Um simples exemplo de um misc device;
	criando um dispositivo de character(Char Drive) com todas as regras padrões para se criar um device simples;
*/


#define TAM_MSG 100//1024
static char msg[TAM_MSG];


#define ERRO(P)\
if(IS_ERR_OR_NULL(P)){\
	printk(KERN_ERR "Erro ou Nulo: %d\n", __LINE__);\
	return PTR_ERR(P);\
}

/* protótipos */
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


static struct miscdevice sample_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "simple_misc",
    .fops = &modulo_fops,
};

/* driver initialization */
static int __init modulo_init(void)
{
    int ret;

    /*
		 * Criar o "Olá" device em /sys/class/mis diretório.
     * Udev criará automaticamente o /dev/hello dispositivo usando as regras padrão.
     */
    ret = misc_register(&sample_device);
    if (ret){
    		printk(KERN_ERR "Não é possível registrar o device.\n");
    }

    printk(KERN_INFO "Modulo inicializado.\n");

    return ret;
}

/* driver exit */
static void __exit modulo_exit(void)
{
		misc_deregister(&sample_device);

    printk(KERN_INFO "Modulo finalizado.\n");
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
