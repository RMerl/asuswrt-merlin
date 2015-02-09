/*
 * Linux ARCnet driver - COM20020 PCMCIA support
 * 
 * Written 1994-1999 by Avery Pennarun,
 *    based on an ISA version by David Woodhouse.
 * Derived from ibmtr_cs.c by Steve Kipisz (pcmcia-cs 3.1.4)
 *    which was derived from pcnet_cs.c by David Hinds.
 * Some additional portions derived from skeleton.c by Donald Becker.
 *
 * Special thanks to Contemporary Controls, Inc. (www.ccontrols.com)
 *  for sponsoring the further development of this driver.
 *
 * **********************
 *
 * The original copyright of skeleton.c was as follows:
 *
 * skeleton.c Written 1993 by Donald Becker.
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.  This software may only be used
 * and distributed according to the terms of the GNU General Public License as
 * modified by SRC, incorporated herein by reference.
 * 
 * **********************
 * Changes:
 * Arnaldo Carvalho de Melo <acme@conectiva.com.br> - 08/08/2000
 * - reorganize kmallocs in com20020_attach, checking all for failure
 *   and releasing the previous allocations if one fails
 * **********************
 * 
 * For more details, see drivers/net/arcnet.c
 *
 * **********************
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/arcdevice.h>
#include <linux/com20020.h>

#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>

#include <asm/io.h>
#include <asm/system.h>

#define VERSION "arcnet: COM20020 PCMCIA support loaded.\n"

#ifdef DEBUG

static void regdump(struct net_device *dev)
{
    int ioaddr = dev->base_addr;
    int count;
    
    printk("com20020 register dump:\n");
    for (count = ioaddr; count < ioaddr + 16; count++)
    {
	if (!(count % 16))
	    printk("\n%04X: ", count);
	printk("%02X ", inb(count));
    }
    printk("\n");
    
    printk("buffer0 dump:\n");
	/* set up the address register */
        count = 0;
	outb((count >> 8) | RDDATAflag | AUTOINCflag, _ADDR_HI);
	outb(count & 0xff, _ADDR_LO);
    
    for (count = 0; count < 256+32; count++)
    {
	if (!(count % 16))
	    printk("\n%04X: ", count);
	
	/* copy the data */
	printk("%02X ", inb(_MEMDATA));
    }
    printk("\n");
}

#else

static inline void regdump(struct net_device *dev) { }

#endif


/*====================================================================*/

/* Parameters that can be set with 'insmod' */

static int node;
static int timeout = 3;
static int backplane;
static int clockp;
static int clockm;

module_param(node, int, 0);
module_param(timeout, int, 0);
module_param(backplane, int, 0);
module_param(clockp, int, 0);
module_param(clockm, int, 0);

MODULE_LICENSE("GPL");

/*====================================================================*/

static int com20020_config(struct pcmcia_device *link);
static void com20020_release(struct pcmcia_device *link);

static void com20020_detach(struct pcmcia_device *p_dev);

/*====================================================================*/

typedef struct com20020_dev_t {
    struct net_device       *dev;
} com20020_dev_t;

/*======================================================================

    com20020_attach() creates an "instance" of the driver, allocating
    local data structures for one device.  The device is registered
    with Card Services.

======================================================================*/

static int com20020_probe(struct pcmcia_device *p_dev)
{
    com20020_dev_t *info;
    struct net_device *dev;
    struct arcnet_local *lp;

    dev_dbg(&p_dev->dev, "com20020_attach()\n");

    /* Create new network device */
    info = kzalloc(sizeof(struct com20020_dev_t), GFP_KERNEL);
    if (!info)
	goto fail_alloc_info;

    dev = alloc_arcdev("");
    if (!dev)
	goto fail_alloc_dev;

    lp = netdev_priv(dev);
    lp->timeout = timeout;
    lp->backplane = backplane;
    lp->clockp = clockp;
    lp->clockm = clockm & 3;
    lp->hw.owner = THIS_MODULE;

    /* fill in our module parameters as defaults */
    dev->dev_addr[0] = node;

    p_dev->resource[0]->flags |= IO_DATA_PATH_WIDTH_8;
    p_dev->resource[0]->end = 16;
    p_dev->conf.Attributes = CONF_ENABLE_IRQ;
    p_dev->conf.IntType = INT_MEMORY_AND_IO;

    info->dev = dev;
    p_dev->priv = info;

    return com20020_config(p_dev);

fail_alloc_dev:
    kfree(info);
fail_alloc_info:
    return -ENOMEM;
} /* com20020_attach */

/*======================================================================

    This deletes a driver "instance".  The device is de-registered
    with Card Services.  If it has been released, all local data
    structures are freed.  Otherwise, the structures will be freed
    when the device is released.

======================================================================*/

static void com20020_detach(struct pcmcia_device *link)
{
    struct com20020_dev_t *info = link->priv;
    struct net_device *dev = info->dev;

    dev_dbg(&link->dev, "detach...\n");

    dev_dbg(&link->dev, "com20020_detach\n");

    dev_dbg(&link->dev, "unregister...\n");

    unregister_netdev(dev);

    /*
     * this is necessary because we register our IRQ separately
     * from card services.
     */
    if (dev->irq)
	    free_irq(dev->irq, dev);

    com20020_release(link);

    /* Unlink device structure, free bits */
    dev_dbg(&link->dev, "unlinking...\n");
    if (link->priv)
    {
	dev = info->dev;
	if (dev)
	{
	    dev_dbg(&link->dev, "kfree...\n");
	    free_netdev(dev);
	}
	dev_dbg(&link->dev, "kfree2...\n");
	kfree(info);
    }

} /* com20020_detach */

/*======================================================================

    com20020_config() is scheduled to run after a CARD_INSERTION event
    is received, to configure the PCMCIA socket, and to make the
    device available to the system.

======================================================================*/

static int com20020_config(struct pcmcia_device *link)
{
    struct arcnet_local *lp;
    com20020_dev_t *info;
    struct net_device *dev;
    int i, ret;
    int ioaddr;

    info = link->priv;
    dev = info->dev;

    dev_dbg(&link->dev, "config...\n");

    dev_dbg(&link->dev, "com20020_config\n");

    dev_dbg(&link->dev, "baseport1 is %Xh\n",
	    (unsigned int) link->resource[0]->start);

    i = -ENODEV;
    link->io_lines = 16;

    if (!link->resource[0]->start)
    {
	for (ioaddr = 0x100; ioaddr < 0x400; ioaddr += 0x10)
	{
	    link->resource[0]->start = ioaddr;
	    i = pcmcia_request_io(link);
	    if (i == 0)
		break;
	}
    }
    else
	i = pcmcia_request_io(link);
    
    if (i != 0)
    {
	dev_dbg(&link->dev, "requestIO failed totally!\n");
	goto failed;
    }
	
    ioaddr = dev->base_addr = link->resource[0]->start;
    dev_dbg(&link->dev, "got ioaddr %Xh\n", ioaddr);

    dev_dbg(&link->dev, "request IRQ %d\n",
	    link->irq);
    if (!link->irq)
    {
	dev_dbg(&link->dev, "requestIRQ failed totally!\n");
	goto failed;
    }

    dev->irq = link->irq;

    ret = pcmcia_request_configuration(link, &link->conf);
    if (ret)
	    goto failed;

    if (com20020_check(dev))
    {
	regdump(dev);
	goto failed;
    }
    
    lp = netdev_priv(dev);
    lp->card_name = "PCMCIA COM20020";
    lp->card_flags = ARC_CAN_10MBIT; /* pretend all of them can 10Mbit */

    SET_NETDEV_DEV(dev, &link->dev);

    i = com20020_found(dev, 0);	/* calls register_netdev */
    
    if (i != 0) {
	dev_printk(KERN_NOTICE, &link->dev,
		"com20020_cs: com20020_found() failed\n");
	goto failed;
    }

    dev_dbg(&link->dev,KERN_INFO "%s: port %#3lx, irq %d\n",
           dev->name, dev->base_addr, dev->irq);
    return 0;

failed:
    dev_dbg(&link->dev, "com20020_config failed...\n");
    com20020_release(link);
    return -ENODEV;
} /* com20020_config */

/*======================================================================

    After a card is removed, com20020_release() will unregister the net
    device, and release the PCMCIA configuration.  If the device is
    still open, this will be postponed until it is closed.

======================================================================*/

static void com20020_release(struct pcmcia_device *link)
{
	dev_dbg(&link->dev, "com20020_release\n");
	pcmcia_disable_device(link);
}

static int com20020_suspend(struct pcmcia_device *link)
{
	com20020_dev_t *info = link->priv;
	struct net_device *dev = info->dev;

	if (link->open)
		netif_device_detach(dev);

	return 0;
}

static int com20020_resume(struct pcmcia_device *link)
{
	com20020_dev_t *info = link->priv;
	struct net_device *dev = info->dev;

	if (link->open) {
		int ioaddr = dev->base_addr;
		struct arcnet_local *lp = netdev_priv(dev);
		ARCRESET;
	}

	return 0;
}

static struct pcmcia_device_id com20020_ids[] = {
	PCMCIA_DEVICE_PROD_ID12("Contemporary Control Systems, Inc.",
			"PCM20 Arcnet Adapter", 0x59991666, 0x95dfffaf),
	PCMCIA_DEVICE_PROD_ID12("SoHard AG",
			"SH ARC PCMCIA", 0xf8991729, 0x69dff0c7),
	PCMCIA_DEVICE_NULL
};
MODULE_DEVICE_TABLE(pcmcia, com20020_ids);

static struct pcmcia_driver com20020_cs_driver = {
	.owner		= THIS_MODULE,
	.drv		= {
		.name	= "com20020_cs",
	},
	.probe		= com20020_probe,
	.remove		= com20020_detach,
	.id_table	= com20020_ids,
	.suspend	= com20020_suspend,
	.resume		= com20020_resume,
};

static int __init init_com20020_cs(void)
{
	return pcmcia_register_driver(&com20020_cs_driver);
}

static void __exit exit_com20020_cs(void)
{
	pcmcia_unregister_driver(&com20020_cs_driver);
}

module_init(init_com20020_cs);
module_exit(exit_com20020_cs);
