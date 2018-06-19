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

#define MIN(a,b)(((a) <= (b))?(a):(b))

/* 
	* Desrregistrar o modulo usb_storage (rmmod usb_storage), que esta associado a um drive de USB para armazenamento de costume.
	* a fim de obter o nosso motorista associado a essa interface, é preciso descarregar o driver usb-storage (ie, rmmod usb-storage ) e reconecte 		* o pen drive.
	
	* Visualizar todas as informações dos devices descritores com o comando "lsusb -v".
	idVendor, idProduct, bNumEndpoints, bEndpointAddress...
*/


struct usb_skel {
	 struct urb *in_urb;
	 struct usb_device *udev;
	 struct usb_interface *interface;
	 int in_endpoint;
	 int out_endpoint;
	 unsigned char *		bulk_in_buffer;		/* the buffer to receive data */
	 size_t			bulk_in_size;		/* the size of the receive buffer */
};


/* comprimento em bytes dos dados para enviar */
//#define TAM_BUF_MSG 500
//static char* msg[TAM_BUF_MSG]; //erro.
//static char msg[TAM_BUF_MSG];

/* Par de identificação do produto criado pelo fornecedor. */
#define VENDOR_ID	0x0781
#define PRODUCT_ID	0x5567

/* Descrições de Endpoint */
#define ENDPOINT_ADDRESS_IN		0x81
#define ENDPOINT_ADDRESS_OUT	0x02

/* Nome de meu device driver */
#define DEVICENAME "driver_icaro"

/* Nome do dispositivo que será registrado no device model, e criado automaticamente no diretorio /dev; */
#define NOME_ARQUIVO_DEV "hello"

// ---
static void skel_write_bulk_callback(struct urb *urb)
{
    /* sync/async unlink faults aren't errors */
    if (urb->status && 
        !(urb->status == -ENOENT || 
          urb->status == -ECONNRESET ||
          urb->status == -ESHUTDOWN)) {
        //dbg("%s - nonzero write bulk status received: %d", __FUNCTION__, urb->status);
        pr_err("Estatus de envio diferentes de zero. Erro: [%d]\n", urb->status);
    }
    
    /* Liberar nosso buffer alocado */
    /* A função usb_buffer_free() foi renomeada para usb_free_coherent() */
	usb_free_coherent(urb->dev, urb->transfer_buffer_length, urb->transfer_buffer, urb->transfer_dma);
}
// ---

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

/* Representação para o kernel do device de USB */
//static struct usb_device *_device_usb;

/* identifica um driver de USB que quer usar o número major USB.
Esta estrutura é usada para a usb_register_dev() e usb_unregister_dev() funções, 
para consolidar uma série de parâmetros utilizados por eles. */
static struct usb_class_driver classe_usb = {
	.name	= "_usb_ola",
	.fops	= &_fops,
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
	struct usb_skel *dev;
	struct usb_interface *interface;
	int subminor = iminor(inode);

	interface = usb_find_interface(&_usb, subminor);
	if (!interface) {
		return -ENODEV; /* No such device */
	}
	
	dev = usb_get_intfdata(interface);
	if (!dev) {
		return -ENODEV; /* No such device */
	}
	
	/* Salvar o nosso objeto na estrutura privada do arquivo */
	fp->private_data = dev;
	printk(KERN_INFO "Device aberto com sucesso.\n");
	
	return 0;
}

static int dev_release( struct inode *inode, struct file *fp )
{
	struct usb_skel *dev;
	dev = (struct usb_skel *)fp->private_data;
	if (dev == NULL){
		return -ENODEV;
	}

	/* return OK */
	return 0;
}

static int conectar_usb(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_skel *dev;
	struct usb_host_interface *usb_des;
	struct usb_endpoint_descriptor *endpoint;
	int retval = -ENOMEM; /* Out of memory */
	size_t buffer_size;
	
	/* allocate memory for our device state and initialize it */
	dev = kmalloc(sizeof(struct usb_skel), GFP_KERNEL);
	if (dev == NULL) {
		pr_err("Out of memory");
		return retval;
	}
	memset(dev, 0x00, sizeof (*dev));
	
	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;
	ERRO(dev->udev)
	ERRO(dev->udev)
	
	usb_des = interface->cur_altsetting;
	ERRO(usb_des)
	
	printk(KERN_ALERT "USB CONECTADO.\n");
	
	int re;
	int j;
	
	re = usb_register_dev(interface, &classe_usb); /* registra o device de USB. */
	if( re < 0 ){
		pr_err("Erro ao registrar o device.\n");
	}else{
		printk(KERN_INFO "Device USB registrado.\nMinor obtido: %d\n", interface->minor);
		
		usb_set_intfdata(interface, dev);
		
		for(j = 0; j < usb_des->desc.bNumEndpoints; j++)
		{
			endpoint = &usb_des->endpoint[j].desc;
			ERRO(endpoint)
			
			printk(KERN_INFO "ED[%d]->bEndpointAddress: 0x%02X\n", j, endpoint->bEndpointAddress);
		    printk(KERN_INFO "ED[%d]->bmAttributes: 0x%02X\n", j, endpoint->bmAttributes);
		    printk(KERN_INFO "ED[%d]->wMaxPacketSize: 0x%04X (%d)\n*****\n", j, endpoint->wMaxPacketSize, endpoint->wMaxPacketSize);
        	
        	/* we found a bulk in endpoint */
        	if( !dev->in_endpoint && usb_endpoint_is_bulk_in(endpoint) ){
        		dev->in_endpoint = endpoint->bEndpointAddress;
        		buffer_size = endpoint->wMaxPacketSize;
        		dev->bulk_in_size = buffer_size;
        	}
        	/* we found a bulk out endpoint */
        	if( !dev->out_endpoint && usb_endpoint_is_bulk_out(endpoint) ){
        		dev->out_endpoint = endpoint->bEndpointAddress;
        	}
        	
		}
		
		if(!dev->in_endpoint || !dev->out_endpoint){
			pr_err("Could not find both bulk-in and bulk-out endpoints\n");
        	return -ENODEV; /* No such device */
		}else{
			dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
			if (!dev->bulk_in_buffer) {
				pr_err("Could not allocate bulk_in_buffer\n");
				return -ENOMEM; /* Out of memory */
			}
		}
		
		/* save our data pointer in this interface device */
		usb_set_intfdata(interface, dev); 
		
		
	}// fim else/
	
	
	
	return 0;
}

static void desconectar_usb(struct usb_interface *interface)
{
	struct usb_skel *dev;
	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	usb_deregister_dev(interface, &classe_usb); /* desregistra o device de USB. */
	
	printk(KERN_ALERT "USB DESCONECTADO.\n");
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
		ERRO(eeprom_class)
		
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
	struct usb_skel *dev;
	int retval = 0;
    int actual_size;
	
	dev = (struct usb_skel *)fp->private_data;
	
	/* do a blocking bulk read to get data from the device */
	retval = usb_bulk_msg(dev->udev,
			      usb_rcvbulkpipe(dev->udev, ENDPOINT_ADDRESS_IN),
			      dev->bulk_in_buffer,
			      min(dev->bulk_in_size, length),
			      &actual_size,
                              HZ*10);

	/* if the read was successful, copy the data to userspace */
	if (!retval) {
          if (copy_to_user(buff, dev->bulk_in_buffer, actual_size))
			retval = -EFAULT;
		else
			retval = length;
	}

	return retval;
}

static ssize_t dev_write(struct file *fp, const char *buff, size_t length, loff_t *ppos )
{
	struct urb *out_urb;
	struct usb_skel *dev;
	char *buf = NULL;
	int ret_sub_urb;
	
		/* verificar se realmente temos alguns dados para escrever. */
		if(length == 0){
			pr_err("NENHUMA informação para escrever.\n");
			return length;
		}
		
		dev = (struct usb_skel *)fp->private_data;
		if (!dev){
			pr_err("Erro em *dev, write.\n");
			return 0;
		}
		
		out_urb = usb_alloc_urb(0, GFP_KERNEL);
		if( !out_urb )
		{
			/* A função usb_buffer_free() foi renomeada para usb_free_coherent() */
			usb_free_coherent(dev->udev, length, buf, out_urb->transfer_dma);
            usb_free_urb(out_urb);
            return -ENOMEM; // Out of memory
		}
		
	/* A função usb_buffer_alloc() foi renomeada para usb_alloc_coherent() */
	buf = usb_alloc_coherent(dev->udev, length, GFP_KERNEL, &out_urb->transfer_dma);
	if (!buf) {
		pr_err("ERRO alocar buf para usb.\n");
		return -ENOMEM;
	}
	
	if (copy_from_user(buf, buff, length)) {
		pr_err("Erro obter buf do user space.\n");
		return -EFAULT;
	}
		
		usb_fill_bulk_urb( out_urb, 
						   dev->udev,
                           usb_sndbulkpipe(dev->udev, ENDPOINT_ADDRESS_OUT),
                           buf, 
                           length, 
                           skel_write_bulk_callback, 
                           dev
                           );
		
		out_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		
		 /* Enviar os dados para fora do porto volume */
		 /* Return 0 on successful submissions. A negative error number otherwise. */
		 ret_sub_urb = usb_submit_urb(out_urb, GFP_KERNEL);
		 if( ret_sub_urb == 0 && out_urb->status == -EINPROGRESS ) { /* -EINPROGRESS => Operação em andamento. "errno.h" */
		 	printk(KERN_ALERT "--ENVIADO PARA USB--.\n");
		 }else{
		 	pr_err("Failed submitting write urb.\n");
		 	/* Liberar nosso buffer alocado */
    		/* A função usb_buffer_free() foi renomeada para usb_free_coherent() */
			usb_free_coherent(out_urb->dev, out_urb->transfer_buffer_length, out_urb->transfer_buffer, out_urb->transfer_dma);
		 }
		 
		
		usb_free_urb(out_urb);
	
	return length;
	
	/*int i;
	short count = 0;
	unsigned long ret = copy_from_user(msg, buff, length);
	
	if(ret){
		return -EFAULT;
	}else{
		for(i = 0; i < length && i < TAM_BUF_MSG; i++){
			msg[i] = *(buff + i);
			count++;
		}
	}
	return count;*/
}

 
module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Icaro freire");
MODULE_DESCRIPTION("treinamento de device driver; Conexão USB.");


