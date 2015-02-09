/*======================================================================

    A driver for Future Domain-compatible PCMCIA SCSI cards

    fdomain_cs.c 1.47 2001/10/13 00:08:52

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU General Public License version 2 (the "GPL"), in
    which case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.
    
======================================================================*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <scsi/scsi.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <scsi/scsi_ioctl.h>

#include "scsi.h"
#include <scsi/scsi_host.h>
#include "fdomain.h"

#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>

/*====================================================================*/

/* Module parameters */

MODULE_AUTHOR("David Hinds <dahinds@users.sourceforge.net>");
MODULE_DESCRIPTION("Future Domain PCMCIA SCSI driver");
MODULE_LICENSE("Dual MPL/GPL");

/*====================================================================*/

typedef struct scsi_info_t {
	struct pcmcia_device	*p_dev;
    struct Scsi_Host	*host;
} scsi_info_t;


static void fdomain_release(struct pcmcia_device *link);
static void fdomain_detach(struct pcmcia_device *p_dev);
static int fdomain_config(struct pcmcia_device *link);

static int fdomain_probe(struct pcmcia_device *link)
{
	scsi_info_t *info;

	dev_dbg(&link->dev, "fdomain_attach()\n");

	/* Create new SCSI device */
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->p_dev = link;
	link->priv = info;
	link->resource[0]->end = 0x10;
	link->resource[0]->flags |= IO_DATA_PATH_WIDTH_AUTO;
	link->conf.Attributes = CONF_ENABLE_IRQ;
	link->conf.IntType = INT_MEMORY_AND_IO;
	link->conf.Present = PRESENT_OPTION;

	return fdomain_config(link);
} /* fdomain_attach */

/*====================================================================*/

static void fdomain_detach(struct pcmcia_device *link)
{
	dev_dbg(&link->dev, "fdomain_detach\n");

	fdomain_release(link);

	kfree(link->priv);
} /* fdomain_detach */

/*====================================================================*/

static int fdomain_config_check(struct pcmcia_device *p_dev,
				cistpl_cftable_entry_t *cfg,
				cistpl_cftable_entry_t *dflt,
				unsigned int vcc,
				void *priv_data)
{
	p_dev->io_lines = 10;
	p_dev->resource[0]->start = cfg->io.win[0].base;
	return pcmcia_request_io(p_dev);
}


static int fdomain_config(struct pcmcia_device *link)
{
    scsi_info_t *info = link->priv;
    int ret;
    char str[22];
    struct Scsi_Host *host;

    dev_dbg(&link->dev, "fdomain_config\n");

    ret = pcmcia_loop_config(link, fdomain_config_check, NULL);
    if (ret)
	    goto failed;

    if (!link->irq)
	    goto failed;
    ret = pcmcia_request_configuration(link, &link->conf);
    if (ret)
	    goto failed;

    /* A bad hack... */
    release_region(link->resource[0]->start, resource_size(link->resource[0]));

    /* Set configuration options for the fdomain driver */
    sprintf(str, "%d,%d", (unsigned int) link->resource[0]->start, link->irq);
    fdomain_setup(str);

    host = __fdomain_16x0_detect(&fdomain_driver_template);
    if (!host) {
        printk(KERN_INFO "fdomain_cs: no SCSI devices found\n");
	goto failed;
    }

    if (scsi_add_host(host, NULL))
	    goto failed;
    scsi_scan_host(host);

    info->host = host;

    return 0;

failed:
    fdomain_release(link);
    return -ENODEV;
} /* fdomain_config */

/*====================================================================*/

static void fdomain_release(struct pcmcia_device *link)
{
	scsi_info_t *info = link->priv;

	dev_dbg(&link->dev, "fdomain_release\n");

	scsi_remove_host(info->host);
	pcmcia_disable_device(link);
	scsi_unregister(info->host);
}

/*====================================================================*/

static int fdomain_resume(struct pcmcia_device *link)
{
	fdomain_16x0_bus_reset(NULL);

	return 0;
}

static struct pcmcia_device_id fdomain_ids[] = {
	PCMCIA_DEVICE_PROD_ID12("IBM Corp.", "SCSI PCMCIA Card", 0xe3736c88, 0x859cad20),
	PCMCIA_DEVICE_PROD_ID1("SCSI PCMCIA Adapter Card", 0x8dacb57e),
	PCMCIA_DEVICE_PROD_ID12(" SIMPLE TECHNOLOGY Corporation", "SCSI PCMCIA Credit Card Controller", 0x182bdafe, 0xc80d106f),
	PCMCIA_DEVICE_NULL,
};
MODULE_DEVICE_TABLE(pcmcia, fdomain_ids);

static struct pcmcia_driver fdomain_cs_driver = {
	.owner		= THIS_MODULE,
	.drv		= {
		.name	= "fdomain_cs",
	},
	.probe		= fdomain_probe,
	.remove		= fdomain_detach,
	.id_table       = fdomain_ids,
	.resume		= fdomain_resume,
};

static int __init init_fdomain_cs(void)
{
	return pcmcia_register_driver(&fdomain_cs_driver);
}

static void __exit exit_fdomain_cs(void)
{
	pcmcia_unregister_driver(&fdomain_cs_driver);
}

module_init(init_fdomain_cs);
module_exit(exit_fdomain_cs);
