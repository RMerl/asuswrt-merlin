/*
 * Driver for Vitesse PHYs
 *
 * Author: Kriston Carson
 *
 * Copyright (c) 2005 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>

/* Vitesse Extended Control Register 1 */
#define MII_VSC8244_EXT_CON1           0x17
#define MII_VSC8244_EXTCON1_INIT       0x0000
#define MII_VSC8244_EXTCON1_TX_SKEW_MASK	0x0c00
#define MII_VSC8244_EXTCON1_RX_SKEW_MASK	0x0300
#define MII_VSC8244_EXTCON1_TX_SKEW	0x0800
#define MII_VSC8244_EXTCON1_RX_SKEW	0x0200

/* Vitesse Interrupt Mask Register */
#define MII_VSC8244_IMASK		0x19
#define MII_VSC8244_IMASK_IEN		0x8000
#define MII_VSC8244_IMASK_SPEED		0x4000
#define MII_VSC8244_IMASK_LINK		0x2000
#define MII_VSC8244_IMASK_DUPLEX	0x1000
#define MII_VSC8244_IMASK_MASK		0xf000

#define MII_VSC8221_IMASK_MASK		0xa000

/* Vitesse Interrupt Status Register */
#define MII_VSC8244_ISTAT		0x1a
#define MII_VSC8244_ISTAT_STATUS	0x8000
#define MII_VSC8244_ISTAT_SPEED		0x4000
#define MII_VSC8244_ISTAT_LINK		0x2000
#define MII_VSC8244_ISTAT_DUPLEX	0x1000

/* Vitesse Auxiliary Control/Status Register */
#define MII_VSC8244_AUX_CONSTAT        	0x1c
#define MII_VSC8244_AUXCONSTAT_INIT    	0x0000
#define MII_VSC8244_AUXCONSTAT_DUPLEX  	0x0020
#define MII_VSC8244_AUXCONSTAT_SPEED   	0x0018
#define MII_VSC8244_AUXCONSTAT_GBIT    	0x0010
#define MII_VSC8244_AUXCONSTAT_100     	0x0008

#define MII_VSC8221_AUXCONSTAT_INIT	0x0004 /* need to set this bit? */
#define MII_VSC8221_AUXCONSTAT_RESERVED	0x0004

#define PHY_ID_VSC8244			0x000fc6c0
#define PHY_ID_VSC8221			0x000fc550

MODULE_DESCRIPTION("Vitesse PHY driver");
MODULE_AUTHOR("Kriston Carson");
MODULE_LICENSE("GPL");

static int vsc824x_config_init(struct phy_device *phydev)
{
	int extcon;
	int err;

	err = phy_write(phydev, MII_VSC8244_AUX_CONSTAT,
			MII_VSC8244_AUXCONSTAT_INIT);
	if (err < 0)
		return err;

	extcon = phy_read(phydev, MII_VSC8244_EXT_CON1);

	if (extcon < 0)
		return err;

	extcon &= ~(MII_VSC8244_EXTCON1_TX_SKEW_MASK |
			MII_VSC8244_EXTCON1_RX_SKEW_MASK);

	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID)
		extcon |= (MII_VSC8244_EXTCON1_TX_SKEW |
				MII_VSC8244_EXTCON1_RX_SKEW);

	err = phy_write(phydev, MII_VSC8244_EXT_CON1, extcon);

	return err;
}

static int vsc824x_ack_interrupt(struct phy_device *phydev)
{
	int err = 0;
	
	/*
	 * Don't bother to ACK the interrupts if interrupts
	 * are disabled.  The 824x cannot clear the interrupts
	 * if they are disabled.
	 */
	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_read(phydev, MII_VSC8244_ISTAT);

	return (err < 0) ? err : 0;
}

static int vsc82xx_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, MII_VSC8244_IMASK,
			phydev->drv->phy_id == PHY_ID_VSC8244 ?
				MII_VSC8244_IMASK_MASK :
				MII_VSC8221_IMASK_MASK);
	else {
		/*
		 * The Vitesse PHY cannot clear the interrupt
		 * once it has disabled them, so we clear them first
		 */
		err = phy_read(phydev, MII_VSC8244_ISTAT);

		if (err < 0)
			return err;

		err = phy_write(phydev, MII_VSC8244_IMASK, 0);
	}

	return err;
}

/* Vitesse 824x */
static struct phy_driver vsc8244_driver = {
	.phy_id		= PHY_ID_VSC8244,
	.name		= "Vitesse VSC8244",
	.phy_id_mask	= 0x000fffc0,
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_init	= &vsc824x_config_init,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.ack_interrupt	= &vsc824x_ack_interrupt,
	.config_intr	= &vsc82xx_config_intr,
	.driver 	= { .owner = THIS_MODULE,},
};

static int vsc8221_config_init(struct phy_device *phydev)
{
	int err;

	err = phy_write(phydev, MII_VSC8244_AUX_CONSTAT,
			MII_VSC8221_AUXCONSTAT_INIT);
	return err;

	/* Perhaps we should set EXT_CON1 based on the interface?
	   Options are 802.3Z SerDes or SGMII */
}

/* Vitesse 8221 */
static struct phy_driver vsc8221_driver = {
	.phy_id		= PHY_ID_VSC8221,
	.phy_id_mask	= 0x000ffff0,
	.name		= "Vitesse VSC8221",
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_init	= &vsc8221_config_init,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.ack_interrupt	= &vsc824x_ack_interrupt,
	.config_intr	= &vsc82xx_config_intr,
	.driver 	= { .owner = THIS_MODULE,},
};

static int __init vsc82xx_init(void)
{
	int err;

	err = phy_driver_register(&vsc8244_driver);
	if (err < 0)
		return err;
	err = phy_driver_register(&vsc8221_driver);
	if (err < 0)
		phy_driver_unregister(&vsc8244_driver);
	return err;
}

static void __exit vsc82xx_exit(void)
{
	phy_driver_unregister(&vsc8244_driver);
	phy_driver_unregister(&vsc8221_driver);
}

module_init(vsc82xx_init);
module_exit(vsc82xx_exit);

static struct mdio_device_id __maybe_unused vitesse_tbl[] = {
	{ PHY_ID_VSC8244, 0x000fffc0 },
	{ PHY_ID_VSC8221, 0x000ffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, vitesse_tbl);
