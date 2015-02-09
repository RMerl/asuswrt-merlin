/*
 * C-Media CMI8788 driver for Asus Xonar cards
 *
 * Copyright (c) Clemens Ladisch <clemens@ladisch.de>
 *
 *
 *  This driver is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.
 *
 *  This driver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this driver; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/pci.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include "xonar.h"

MODULE_AUTHOR("Clemens Ladisch <clemens@ladisch.de>");
MODULE_DESCRIPTION("Asus AVx00 driver");
MODULE_LICENSE("GPL v2");
MODULE_SUPPORTED_DEVICE("{{Asus,AV100},{Asus,AV200}}");

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "card index");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "enable card");

static DEFINE_PCI_DEVICE_TABLE(xonar_ids) = {
	{ OXYGEN_PCI_SUBID(0x1043, 0x8269) },
	{ OXYGEN_PCI_SUBID(0x1043, 0x8275) },
	{ OXYGEN_PCI_SUBID(0x1043, 0x82b7) },
	{ OXYGEN_PCI_SUBID(0x1043, 0x8314) },
	{ OXYGEN_PCI_SUBID(0x1043, 0x8327) },
	{ OXYGEN_PCI_SUBID(0x1043, 0x834f) },
	{ OXYGEN_PCI_SUBID(0x1043, 0x835c) },
	{ OXYGEN_PCI_SUBID(0x1043, 0x835d) },
	{ OXYGEN_PCI_SUBID(0x1043, 0x838e) },
	{ OXYGEN_PCI_SUBID_BROKEN_EEPROM },
	{ }
};
MODULE_DEVICE_TABLE(pci, xonar_ids);

static int __devinit get_xonar_model(struct oxygen *chip,
				     const struct pci_device_id *id)
{
	if (get_xonar_pcm179x_model(chip, id) >= 0)
		return 0;
	if (get_xonar_cs43xx_model(chip, id) >= 0)
		return 0;
	if (get_xonar_wm87x6_model(chip, id) >= 0)
		return 0;
	return -EINVAL;
}

static int __devinit xonar_probe(struct pci_dev *pci,
				 const struct pci_device_id *pci_id)
{
	static int dev;
	int err;

	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) {
		++dev;
		return -ENOENT;
	}
	err = oxygen_pci_probe(pci, index[dev], id[dev], THIS_MODULE,
			       xonar_ids, get_xonar_model);
	if (err >= 0)
		++dev;
	return err;
}

static struct pci_driver xonar_driver = {
	.name = "AV200",
	.id_table = xonar_ids,
	.probe = xonar_probe,
	.remove = __devexit_p(oxygen_pci_remove),
#ifdef CONFIG_PM
	.suspend = oxygen_pci_suspend,
	.resume = oxygen_pci_resume,
#endif
	.shutdown = oxygen_pci_shutdown,
};

static int __init alsa_card_xonar_init(void)
{
	return pci_register_driver(&xonar_driver);
}

static void __exit alsa_card_xonar_exit(void)
{
	pci_unregister_driver(&xonar_driver);
}

module_init(alsa_card_xonar_init)
module_exit(alsa_card_xonar_exit)
