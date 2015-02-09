
/*
    card-ad1816a.c - driver for ADI SoundPort AD1816A based soundcards.
    Copyright (C) 2000 by Massimo Piccioni <dafastidio@libero.it>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#include <linux/init.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/pnp.h>
#include <linux/moduleparam.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/ad1816a.h>
#include <sound/mpu401.h>
#include <sound/opl3.h>

#define PFX "ad1816a: "

MODULE_AUTHOR("Massimo Piccioni <dafastidio@libero.it>");
MODULE_DESCRIPTION("AD1816A, AD1815");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{Highscreen,Sound-Boostar 16 3D},"
		"{Analog Devices,AD1815},"
		"{Analog Devices,AD1816A},"
		"{TerraTec,Base 64},"
		"{TerraTec,AudioSystem EWS64S},"
		"{Aztech/Newcom SC-16 3D},"
		"{Shark Predator ISA}}");

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 1-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	/* ID for this card */
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_ISAPNP;	/* Enable this card */
static long port[SNDRV_CARDS] = SNDRV_DEFAULT_PORT;	/* PnP setup */
static long mpu_port[SNDRV_CARDS] = SNDRV_DEFAULT_PORT;	/* PnP setup */
static long fm_port[SNDRV_CARDS] = SNDRV_DEFAULT_PORT;	/* PnP setup */
static int irq[SNDRV_CARDS] = SNDRV_DEFAULT_IRQ;	/* Pnp setup */
static int mpu_irq[SNDRV_CARDS] = SNDRV_DEFAULT_IRQ;	/* Pnp setup */
static int dma1[SNDRV_CARDS] = SNDRV_DEFAULT_DMA;	/* PnP setup */
static int dma2[SNDRV_CARDS] = SNDRV_DEFAULT_DMA;	/* PnP setup */
static int clockfreq[SNDRV_CARDS];

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for ad1816a based soundcard.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for ad1816a based soundcard.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable ad1816a based soundcard.");
module_param_array(clockfreq, int, NULL, 0444);
MODULE_PARM_DESC(clockfreq, "Clock frequency for ad1816a driver (default = 0).");

struct snd_card_ad1816a {
	struct pnp_dev *dev;
	struct pnp_dev *devmpu;
};

static struct pnp_card_device_id snd_ad1816a_pnpids[] = {
	/* Analog Devices AD1815 */
	{ .id = "ADS7150", .devs = { { .id = "ADS7150" }, { .id = "ADS7151" } } },
	/* Analog Device AD1816? */
	{ .id = "ADS7180", .devs = { { .id = "ADS7180" }, { .id = "ADS7181" } } },
	/* Analog Devices AD1816A - added by Kenneth Platz <kxp@atl.hp.com> */
	{ .id = "ADS7181", .devs = { { .id = "ADS7180" }, { .id = "ADS7181" } } },
	/* Analog Devices AD1816A - Aztech/Newcom SC-16 3D */
	{ .id = "AZT1022", .devs = { { .id = "AZT1018" }, { .id = "AZT2002" } } },
	/* Highscreen Sound-Boostar 16 3D - added by Stefan Behnel */
	{ .id = "LWC1061", .devs = { { .id = "ADS7180" }, { .id = "ADS7181" } } },
	/* Highscreen Sound-Boostar 16 3D */
	{ .id = "MDK1605", .devs = { { .id = "ADS7180" }, { .id = "ADS7181" } } },
	/* Shark Predator ISA - added by Ken Arromdee */
	{ .id = "SMM7180", .devs = { { .id = "ADS7180" }, { .id = "ADS7181" } } },
	/* Analog Devices AD1816A - Terratec AudioSystem EWS64 S */
	{ .id = "TER1112", .devs = { { .id = "ADS7180" }, { .id = "ADS7181" } } },
	/* Analog Devices AD1816A - Terratec AudioSystem EWS64 S */
	{ .id = "TER1112", .devs = { { .id = "TER1100" }, { .id = "TER1101" } } },
	/* Analog Devices AD1816A - Terratec Base 64 */
	{ .id = "TER1411", .devs = { { .id = "ADS7180" }, { .id = "ADS7181" } } },
	/* end */
	{ .id = "" }
};

MODULE_DEVICE_TABLE(pnp_card, snd_ad1816a_pnpids);


#define	DRIVER_NAME	"snd-card-ad1816a"


static int __devinit snd_card_ad1816a_pnp(int dev, struct snd_card_ad1816a *acard,
					  struct pnp_card_link *card,
					  const struct pnp_card_device_id *id)
{
	struct pnp_dev *pdev;
	int err;

	acard->dev = pnp_request_card_device(card, id->devs[0].id, NULL);
	if (acard->dev == NULL)
		return -EBUSY;

	acard->devmpu = pnp_request_card_device(card, id->devs[1].id, NULL);
	if (acard->devmpu == NULL) {
		mpu_port[dev] = -1;
		snd_printk(KERN_WARNING PFX "MPU401 device busy, skipping.\n");
	}

	pdev = acard->dev;

	err = pnp_activate_dev(pdev);
	if (err < 0) {
		printk(KERN_ERR PFX "AUDIO PnP configure failure\n");
		return -EBUSY;
	}

	port[dev] = pnp_port_start(pdev, 2);
	fm_port[dev] = pnp_port_start(pdev, 1);
	dma1[dev] = pnp_dma(pdev, 0);
	dma2[dev] = pnp_dma(pdev, 1);
	irq[dev] = pnp_irq(pdev, 0);

	if (acard->devmpu == NULL)
		return 0;

	pdev = acard->devmpu;

	err = pnp_activate_dev(pdev);
	if (err < 0) {
		printk(KERN_ERR PFX "MPU401 PnP configure failure\n");
		mpu_port[dev] = -1;
		acard->devmpu = NULL;
	} else {
		mpu_port[dev] = pnp_port_start(pdev, 0);
		mpu_irq[dev] = pnp_irq(pdev, 0);
	}

	return 0;
}

static int __devinit snd_card_ad1816a_probe(int dev, struct pnp_card_link *pcard,
					    const struct pnp_card_device_id *pid)
{
	int error;
	struct snd_card *card;
	struct snd_card_ad1816a *acard;
	struct snd_ad1816a *chip;
	struct snd_opl3 *opl3;
	struct snd_timer *timer;

	error = snd_card_create(index[dev], id[dev], THIS_MODULE,
				sizeof(struct snd_card_ad1816a), &card);
	if (error < 0)
		return error;
	acard = (struct snd_card_ad1816a *)card->private_data;

	if ((error = snd_card_ad1816a_pnp(dev, acard, pcard, pid))) {
		snd_card_free(card);
		return error;
	}
	snd_card_set_dev(card, &pcard->card->dev);

	if ((error = snd_ad1816a_create(card, port[dev],
					irq[dev],
					dma1[dev],
					dma2[dev],
					&chip)) < 0) {
		snd_card_free(card);
		return error;
	}
	if (clockfreq[dev] >= 5000 && clockfreq[dev] <= 100000)
		chip->clock_freq = clockfreq[dev];

	strcpy(card->driver, "AD1816A");
	strcpy(card->shortname, "ADI SoundPort AD1816A");
	sprintf(card->longname, "%s, SS at 0x%lx, irq %d, dma %d&%d",
		card->shortname, chip->port, irq[dev], dma1[dev], dma2[dev]);

	if ((error = snd_ad1816a_pcm(chip, 0, NULL)) < 0) {
		snd_card_free(card);
		return error;
	}

	if ((error = snd_ad1816a_mixer(chip)) < 0) {
		snd_card_free(card);
		return error;
	}

	error = snd_ad1816a_timer(chip, 0, &timer);
	if (error < 0) {
		snd_card_free(card);
		return error;
	}

	if (mpu_port[dev] > 0) {
		if (snd_mpu401_uart_new(card, 0, MPU401_HW_MPU401,
					mpu_port[dev], 0, mpu_irq[dev], IRQF_DISABLED,
					NULL) < 0)
			printk(KERN_ERR PFX "no MPU-401 device at 0x%lx.\n", mpu_port[dev]);
	}

	if (fm_port[dev] > 0) {
		if (snd_opl3_create(card,
				    fm_port[dev], fm_port[dev] + 2,
				    OPL3_HW_AUTO, 0, &opl3) < 0) {
			printk(KERN_ERR PFX "no OPL device at 0x%lx-0x%lx.\n", fm_port[dev], fm_port[dev] + 2);
		} else {
			error = snd_opl3_hwdep_new(opl3, 0, 1, NULL);
			if (error < 0) {
				snd_card_free(card);
				return error;
			}
		}
	}

	if ((error = snd_card_register(card)) < 0) {
		snd_card_free(card);
		return error;
	}
	pnp_set_card_drvdata(pcard, card);
	return 0;
}

static unsigned int __devinitdata ad1816a_devices;

static int __devinit snd_ad1816a_pnp_detect(struct pnp_card_link *card,
					    const struct pnp_card_device_id *id)
{
	static int dev;
	int res;

	for ( ; dev < SNDRV_CARDS; dev++) {
		if (!enable[dev])
			continue;
		res = snd_card_ad1816a_probe(dev, card, id);
		if (res < 0)
			return res;
		dev++;
		ad1816a_devices++;
		return 0;
	}
        return -ENODEV;
}

static void __devexit snd_ad1816a_pnp_remove(struct pnp_card_link * pcard)
{
	snd_card_free(pnp_get_card_drvdata(pcard));
	pnp_set_card_drvdata(pcard, NULL);
}

static struct pnp_card_driver ad1816a_pnpc_driver = {
	.flags		= PNP_DRIVER_RES_DISABLE,
	.name		= "ad1816a",
	.id_table	= snd_ad1816a_pnpids,
	.probe		= snd_ad1816a_pnp_detect,
	.remove		= __devexit_p(snd_ad1816a_pnp_remove),
};

static int __init alsa_card_ad1816a_init(void)
{
	int err;

	err = pnp_register_card_driver(&ad1816a_pnpc_driver);
	if (err)
		return err;

	if (!ad1816a_devices) {
		pnp_unregister_card_driver(&ad1816a_pnpc_driver);
#ifdef MODULE
		printk(KERN_ERR "no AD1816A based soundcards found.\n");
#endif	/* MODULE */
		return -ENODEV;
	}
	return 0;
}

static void __exit alsa_card_ad1816a_exit(void)
{
	pnp_unregister_card_driver(&ad1816a_pnpc_driver);
}

module_init(alsa_card_ad1816a_init)
module_exit(alsa_card_ad1816a_exit)
