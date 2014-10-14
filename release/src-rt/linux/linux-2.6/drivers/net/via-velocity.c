/*
 * This code is derived from the VIA reference driver (copyright message
 * below) provided to Red Hat by VIA Networking Technologies, Inc. for
 * addition to the Linux kernel.
 *
 * The code has been merged into one source file, cleaned up to follow
 * Linux coding style,  ported to the Linux 2.6 kernel tree and cleaned
 * for 64bit hardware platforms.
 *
 * TODO
 *	rx_copybreak/alignment
 *	More testing
 *
 * The changes are (c) Copyright 2004, Red Hat Inc. <alan@lxorguk.ukuu.org.uk>
 * Additional fixes and clean up: Francois Romieu
 *
 * This source has not been verified for use in safety critical systems.
 *
 * Please direct queries about the revamped driver to the linux-kernel
 * list not VIA.
 *
 * Original code:
 *
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This software may be redistributed and/or modified under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * Author: Chuang Liang-Shing, AJ Jiang
 *
 * Date: Jan 24, 2003
 *
 * MODULE_LICENSE("GPL");
 *
 */


#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/if.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/inetdevice.h>
#include <linux/reboot.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/in.h>
#include <linux/if_arp.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/crc-ccitt.h>
#include <linux/crc32.h>

#include "via-velocity.h"


static int velocity_nics;
static int msglevel = MSG_LEVEL_INFO;

/**
 *	mac_get_cam_mask	-	Read a CAM mask
 *	@regs: register block for this velocity
 *	@mask: buffer to store mask
 *
 *	Fetch the mask bits of the selected CAM and store them into the
 *	provided mask buffer.
 */
static void mac_get_cam_mask(struct mac_regs __iomem *regs, u8 *mask)
{
	int i;

	/* Select CAM mask */
	BYTE_REG_BITS_SET(CAMCR_PS_CAM_MASK, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);

	writeb(0, &regs->CAMADDR);

	/* read mask */
	for (i = 0; i < 8; i++)
		*mask++ = readb(&(regs->MARCAM[i]));

	/* disable CAMEN */
	writeb(0, &regs->CAMADDR);

	/* Select mar */
	BYTE_REG_BITS_SET(CAMCR_PS_MAR, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);
}


/**
 *	mac_set_cam_mask	-	Set a CAM mask
 *	@regs: register block for this velocity
 *	@mask: CAM mask to load
 *
 *	Store a new mask into a CAM
 */
static void mac_set_cam_mask(struct mac_regs __iomem *regs, u8 *mask)
{
	int i;
	/* Select CAM mask */
	BYTE_REG_BITS_SET(CAMCR_PS_CAM_MASK, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);

	writeb(CAMADDR_CAMEN, &regs->CAMADDR);

	for (i = 0; i < 8; i++)
		writeb(*mask++, &(regs->MARCAM[i]));

	/* disable CAMEN */
	writeb(0, &regs->CAMADDR);

	/* Select mar */
	BYTE_REG_BITS_SET(CAMCR_PS_MAR, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);
}

static void mac_set_vlan_cam_mask(struct mac_regs __iomem *regs, u8 *mask)
{
	int i;
	/* Select CAM mask */
	BYTE_REG_BITS_SET(CAMCR_PS_CAM_MASK, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);

	writeb(CAMADDR_CAMEN | CAMADDR_VCAMSL, &regs->CAMADDR);

	for (i = 0; i < 8; i++)
		writeb(*mask++, &(regs->MARCAM[i]));

	/* disable CAMEN */
	writeb(0, &regs->CAMADDR);

	/* Select mar */
	BYTE_REG_BITS_SET(CAMCR_PS_MAR, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);
}

/**
 *	mac_set_cam	-	set CAM data
 *	@regs: register block of this velocity
 *	@idx: Cam index
 *	@addr: 2 or 6 bytes of CAM data
 *
 *	Load an address or vlan tag into a CAM
 */
static void mac_set_cam(struct mac_regs __iomem *regs, int idx, const u8 *addr)
{
	int i;

	/* Select CAM mask */
	BYTE_REG_BITS_SET(CAMCR_PS_CAM_DATA, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);

	idx &= (64 - 1);

	writeb(CAMADDR_CAMEN | idx, &regs->CAMADDR);

	for (i = 0; i < 6; i++)
		writeb(*addr++, &(regs->MARCAM[i]));

	BYTE_REG_BITS_ON(CAMCR_CAMWR, &regs->CAMCR);

	udelay(10);

	writeb(0, &regs->CAMADDR);

	/* Select mar */
	BYTE_REG_BITS_SET(CAMCR_PS_MAR, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);
}

static void mac_set_vlan_cam(struct mac_regs __iomem *regs, int idx,
			     const u8 *addr)
{

	/* Select CAM mask */
	BYTE_REG_BITS_SET(CAMCR_PS_CAM_DATA, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);

	idx &= (64 - 1);

	writeb(CAMADDR_CAMEN | CAMADDR_VCAMSL | idx, &regs->CAMADDR);
	writew(*((u16 *) addr), &regs->MARCAM[0]);

	BYTE_REG_BITS_ON(CAMCR_CAMWR, &regs->CAMCR);

	udelay(10);

	writeb(0, &regs->CAMADDR);

	/* Select mar */
	BYTE_REG_BITS_SET(CAMCR_PS_MAR, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);
}


/**
 *	mac_wol_reset	-	reset WOL after exiting low power
 *	@regs: register block of this velocity
 *
 *	Called after we drop out of wake on lan mode in order to
 *	reset the Wake on lan features. This function doesn't restore
 *	the rest of the logic from the result of sleep/wakeup
 */
static void mac_wol_reset(struct mac_regs __iomem *regs)
{

	/* Turn off SWPTAG right after leaving power mode */
	BYTE_REG_BITS_OFF(STICKHW_SWPTAG, &regs->STICKHW);
	/* clear sticky bits */
	BYTE_REG_BITS_OFF((STICKHW_DS1 | STICKHW_DS0), &regs->STICKHW);

	BYTE_REG_BITS_OFF(CHIPGCR_FCGMII, &regs->CHIPGCR);
	BYTE_REG_BITS_OFF(CHIPGCR_FCMODE, &regs->CHIPGCR);
	/* disable force PME-enable */
	writeb(WOLCFG_PMEOVR, &regs->WOLCFGClr);
	/* disable power-event config bit */
	writew(0xFFFF, &regs->WOLCRClr);
	/* clear power status */
	writew(0xFFFF, &regs->WOLSRClr);
}

static const struct ethtool_ops velocity_ethtool_ops;

/*
    Define module options
*/

MODULE_AUTHOR("VIA Networking Technologies, Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("VIA Networking Velocity Family Gigabit Ethernet Adapter Driver");

#define VELOCITY_PARAM(N, D) \
	static int N[MAX_UNITS] = OPTION_DEFAULT;\
	module_param_array(N, int, NULL, 0); \
	MODULE_PARM_DESC(N, D);

#define RX_DESC_MIN     64
#define RX_DESC_MAX     255
#define RX_DESC_DEF     64
VELOCITY_PARAM(RxDescriptors, "Number of receive descriptors");

#define TX_DESC_MIN     16
#define TX_DESC_MAX     256
#define TX_DESC_DEF     64
VELOCITY_PARAM(TxDescriptors, "Number of transmit descriptors");

#define RX_THRESH_MIN   0
#define RX_THRESH_MAX   3
#define RX_THRESH_DEF   0
/* rx_thresh[] is used for controlling the receive fifo threshold.
   0: indicate the rxfifo threshold is 128 bytes.
   1: indicate the rxfifo threshold is 512 bytes.
   2: indicate the rxfifo threshold is 1024 bytes.
   3: indicate the rxfifo threshold is store & forward.
*/
VELOCITY_PARAM(rx_thresh, "Receive fifo threshold");

#define DMA_LENGTH_MIN  0
#define DMA_LENGTH_MAX  7
#define DMA_LENGTH_DEF  6

/* DMA_length[] is used for controlling the DMA length
   0: 8 DWORDs
   1: 16 DWORDs
   2: 32 DWORDs
   3: 64 DWORDs
   4: 128 DWORDs
   5: 256 DWORDs
   6: SF(flush till emply)
   7: SF(flush till emply)
*/
VELOCITY_PARAM(DMA_length, "DMA length");

#define IP_ALIG_DEF     0
/* IP_byte_align[] is used for IP header DWORD byte aligned
   0: indicate the IP header won't be DWORD byte aligned.(Default) .
   1: indicate the IP header will be DWORD byte aligned.
      In some environment, the IP header should be DWORD byte aligned,
      or the packet will be droped when we receive it. (eg: IPVS)
*/
VELOCITY_PARAM(IP_byte_align, "Enable IP header dword aligned");

#define FLOW_CNTL_DEF   1
#define FLOW_CNTL_MIN   1
#define FLOW_CNTL_MAX   5

/* flow_control[] is used for setting the flow control ability of NIC.
   1: hardware deafult - AUTO (default). Use Hardware default value in ANAR.
   2: enable TX flow control.
   3: enable RX flow control.
   4: enable RX/TX flow control.
   5: disable
*/
VELOCITY_PARAM(flow_control, "Enable flow control ability");

#define MED_LNK_DEF 0
#define MED_LNK_MIN 0
#define MED_LNK_MAX 5
/* speed_duplex[] is used for setting the speed and duplex mode of NIC.
   0: indicate autonegotiation for both speed and duplex mode
   1: indicate 100Mbps half duplex mode
   2: indicate 100Mbps full duplex mode
   3: indicate 10Mbps half duplex mode
   4: indicate 10Mbps full duplex mode
   5: indicate 1000Mbps full duplex mode

   Note:
   if EEPROM have been set to the force mode, this option is ignored
   by driver.
*/
VELOCITY_PARAM(speed_duplex, "Setting the speed and duplex mode");

#define VAL_PKT_LEN_DEF     0
/* ValPktLen[] is used for setting the checksum offload ability of NIC.
   0: Receive frame with invalid layer 2 length (Default)
   1: Drop frame with invalid layer 2 length
*/
VELOCITY_PARAM(ValPktLen, "Receiving or Drop invalid 802.3 frame");

#define WOL_OPT_DEF     0
#define WOL_OPT_MIN     0
#define WOL_OPT_MAX     7
/* wol_opts[] is used for controlling wake on lan behavior.
   0: Wake up if recevied a magic packet. (Default)
   1: Wake up if link status is on/off.
   2: Wake up if recevied an arp packet.
   4: Wake up if recevied any unicast packet.
   Those value can be sumed up to support more than one option.
*/
VELOCITY_PARAM(wol_opts, "Wake On Lan options");

static int rx_copybreak = 200;
module_param(rx_copybreak, int, 0644);
MODULE_PARM_DESC(rx_copybreak, "Copy breakpoint for copy-only-tiny-frames");

/*
 *	Internal board variants. At the moment we have only one
 */
static struct velocity_info_tbl chip_info_table[] = {
	{CHIP_TYPE_VT6110, "VIA Networking Velocity Family Gigabit Ethernet Adapter", 1, 0x00FFFFFFUL},
	{ }
};

/*
 *	Describe the PCI device identifiers that we support in this
 *	device driver. Used for hotplug autoloading.
 */
static DEFINE_PCI_DEVICE_TABLE(velocity_id_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_612X) },
	{ }
};

MODULE_DEVICE_TABLE(pci, velocity_id_table);

/**
 *	get_chip_name	- 	identifier to name
 *	@id: chip identifier
 *
 *	Given a chip identifier return a suitable description. Returns
 *	a pointer a static string valid while the driver is loaded.
 */
static const char __devinit *get_chip_name(enum chip_type chip_id)
{
	int i;
	for (i = 0; chip_info_table[i].name != NULL; i++)
		if (chip_info_table[i].chip_id == chip_id)
			break;
	return chip_info_table[i].name;
}

/**
 *	velocity_remove1	-	device unplug
 *	@pdev: PCI device being removed
 *
 *	Device unload callback. Called on an unplug or on module
 *	unload for each active device that is present. Disconnects
 *	the device from the network layer and frees all the resources
 */
static void __devexit velocity_remove1(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct velocity_info *vptr = netdev_priv(dev);

	unregister_netdev(dev);
	iounmap(vptr->mac_regs);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	free_netdev(dev);

	velocity_nics--;
}

/**
 *	velocity_set_int_opt	-	parser for integer options
 *	@opt: pointer to option value
 *	@val: value the user requested (or -1 for default)
 *	@min: lowest value allowed
 *	@max: highest value allowed
 *	@def: default value
 *	@name: property name
 *	@dev: device name
 *
 *	Set an integer property in the module options. This function does
 *	all the verification and checking as well as reporting so that
 *	we don't duplicate code for each option.
 */
static void __devinit velocity_set_int_opt(int *opt, int val, int min, int max, int def, char *name, const char *devname)
{
	if (val == -1)
		*opt = def;
	else if (val < min || val > max) {
		VELOCITY_PRT(MSG_LEVEL_INFO, KERN_NOTICE "%s: the value of parameter %s is invalid, the valid range is (%d-%d)\n",
					devname, name, min, max);
		*opt = def;
	} else {
		VELOCITY_PRT(MSG_LEVEL_INFO, KERN_INFO "%s: set value of parameter %s to %d\n",
					devname, name, val);
		*opt = val;
	}
}

/**
 *	velocity_set_bool_opt	-	parser for boolean options
 *	@opt: pointer to option value
 *	@val: value the user requested (or -1 for default)
 *	@def: default value (yes/no)
 *	@flag: numeric value to set for true.
 *	@name: property name
 *	@dev: device name
 *
 *	Set a boolean property in the module options. This function does
 *	all the verification and checking as well as reporting so that
 *	we don't duplicate code for each option.
 */
static void __devinit velocity_set_bool_opt(u32 *opt, int val, int def, u32 flag, char *name, const char *devname)
{
	(*opt) &= (~flag);
	if (val == -1)
		*opt |= (def ? flag : 0);
	else if (val < 0 || val > 1) {
		printk(KERN_NOTICE "%s: the value of parameter %s is invalid, the valid range is (0-1)\n",
			devname, name);
		*opt |= (def ? flag : 0);
	} else {
		printk(KERN_INFO "%s: set parameter %s to %s\n",
			devname, name, val ? "TRUE" : "FALSE");
		*opt |= (val ? flag : 0);
	}
}

/**
 *	velocity_get_options	-	set options on device
 *	@opts: option structure for the device
 *	@index: index of option to use in module options array
 *	@devname: device name
 *
 *	Turn the module and command options into a single structure
 *	for the current device
 */
static void __devinit velocity_get_options(struct velocity_opt *opts, int index, const char *devname)
{

	velocity_set_int_opt(&opts->rx_thresh, rx_thresh[index], RX_THRESH_MIN, RX_THRESH_MAX, RX_THRESH_DEF, "rx_thresh", devname);
	velocity_set_int_opt(&opts->DMA_length, DMA_length[index], DMA_LENGTH_MIN, DMA_LENGTH_MAX, DMA_LENGTH_DEF, "DMA_length", devname);
	velocity_set_int_opt(&opts->numrx, RxDescriptors[index], RX_DESC_MIN, RX_DESC_MAX, RX_DESC_DEF, "RxDescriptors", devname);
	velocity_set_int_opt(&opts->numtx, TxDescriptors[index], TX_DESC_MIN, TX_DESC_MAX, TX_DESC_DEF, "TxDescriptors", devname);

	velocity_set_int_opt(&opts->flow_cntl, flow_control[index], FLOW_CNTL_MIN, FLOW_CNTL_MAX, FLOW_CNTL_DEF, "flow_control", devname);
	velocity_set_bool_opt(&opts->flags, IP_byte_align[index], IP_ALIG_DEF, VELOCITY_FLAGS_IP_ALIGN, "IP_byte_align", devname);
	velocity_set_bool_opt(&opts->flags, ValPktLen[index], VAL_PKT_LEN_DEF, VELOCITY_FLAGS_VAL_PKT_LEN, "ValPktLen", devname);
	velocity_set_int_opt((int *) &opts->spd_dpx, speed_duplex[index], MED_LNK_MIN, MED_LNK_MAX, MED_LNK_DEF, "Media link mode", devname);
	velocity_set_int_opt((int *) &opts->wol_opts, wol_opts[index], WOL_OPT_MIN, WOL_OPT_MAX, WOL_OPT_DEF, "Wake On Lan options", devname);
	opts->numrx = (opts->numrx & ~3);
}

/**
 *	velocity_init_cam_filter	-	initialise CAM
 *	@vptr: velocity to program
 *
 *	Initialize the content addressable memory used for filters. Load
 *	appropriately according to the presence of VLAN
 */
static void velocity_init_cam_filter(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;

	/* Turn on MCFG_PQEN, turn off MCFG_RTGOPT */
	WORD_REG_BITS_SET(MCFG_PQEN, MCFG_RTGOPT, &regs->MCFG);
	WORD_REG_BITS_ON(MCFG_VIDFR, &regs->MCFG);

	/* Disable all CAMs */
	memset(vptr->vCAMmask, 0, sizeof(u8) * 8);
	memset(vptr->mCAMmask, 0, sizeof(u8) * 8);
	mac_set_vlan_cam_mask(regs, vptr->vCAMmask);
	mac_set_cam_mask(regs, vptr->mCAMmask);

	/* Enable VCAMs */
	if (vptr->vlgrp) {
		unsigned int vid, i = 0;

		if (!vlan_group_get_device(vptr->vlgrp, 0))
			WORD_REG_BITS_ON(MCFG_RTGOPT, &regs->MCFG);

		for (vid = 1; (vid < VLAN_VID_MASK); vid++) {
			if (vlan_group_get_device(vptr->vlgrp, vid)) {
				mac_set_vlan_cam(regs, i, (u8 *) &vid);
				vptr->vCAMmask[i / 8] |= 0x1 << (i % 8);
				if (++i >= VCAM_SIZE)
					break;
			}
		}
		mac_set_vlan_cam_mask(regs, vptr->vCAMmask);
	}
}

static void velocity_vlan_rx_register(struct net_device *dev,
				      struct vlan_group *grp)
{
	struct velocity_info *vptr = netdev_priv(dev);

	vptr->vlgrp = grp;
}

static void velocity_vlan_rx_add_vid(struct net_device *dev, unsigned short vid)
{
	struct velocity_info *vptr = netdev_priv(dev);

	spin_lock_irq(&vptr->lock);
	velocity_init_cam_filter(vptr);
	spin_unlock_irq(&vptr->lock);
}

static void velocity_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
{
	struct velocity_info *vptr = netdev_priv(dev);

	spin_lock_irq(&vptr->lock);
	vlan_group_set_device(vptr->vlgrp, vid, NULL);
	velocity_init_cam_filter(vptr);
	spin_unlock_irq(&vptr->lock);
}

static void velocity_init_rx_ring_indexes(struct velocity_info *vptr)
{
	vptr->rx.dirty = vptr->rx.filled = vptr->rx.curr = 0;
}

/**
 *	velocity_rx_reset	-	handle a receive reset
 *	@vptr: velocity we are resetting
 *
 *	Reset the ownership and status for the receive ring side.
 *	Hand all the receive queue to the NIC.
 */
static void velocity_rx_reset(struct velocity_info *vptr)
{

	struct mac_regs __iomem *regs = vptr->mac_regs;
	int i;

	velocity_init_rx_ring_indexes(vptr);

	/*
	 *	Init state, all RD entries belong to the NIC
	 */
	for (i = 0; i < vptr->options.numrx; ++i)
		vptr->rx.ring[i].rdesc0.len |= OWNED_BY_NIC;

	writew(vptr->options.numrx, &regs->RBRDU);
	writel(vptr->rx.pool_dma, &regs->RDBaseLo);
	writew(0, &regs->RDIdx);
	writew(vptr->options.numrx - 1, &regs->RDCSize);
}

/**
 *	velocity_get_opt_media_mode	-	get media selection
 *	@vptr: velocity adapter
 *
 *	Get the media mode stored in EEPROM or module options and load
 *	mii_status accordingly. The requested link state information
 *	is also returned.
 */
static u32 velocity_get_opt_media_mode(struct velocity_info *vptr)
{
	u32 status = 0;

	switch (vptr->options.spd_dpx) {
	case SPD_DPX_AUTO:
		status = VELOCITY_AUTONEG_ENABLE;
		break;
	case SPD_DPX_100_FULL:
		status = VELOCITY_SPEED_100 | VELOCITY_DUPLEX_FULL;
		break;
	case SPD_DPX_10_FULL:
		status = VELOCITY_SPEED_10 | VELOCITY_DUPLEX_FULL;
		break;
	case SPD_DPX_100_HALF:
		status = VELOCITY_SPEED_100;
		break;
	case SPD_DPX_10_HALF:
		status = VELOCITY_SPEED_10;
		break;
	case SPD_DPX_1000_FULL:
		status = VELOCITY_SPEED_1000 | VELOCITY_DUPLEX_FULL;
		break;
	}
	vptr->mii_status = status;
	return status;
}

/**
 *	safe_disable_mii_autopoll	-	autopoll off
 *	@regs: velocity registers
 *
 *	Turn off the autopoll and wait for it to disable on the chip
 */
static void safe_disable_mii_autopoll(struct mac_regs __iomem *regs)
{
	u16 ww;

	/*  turn off MAUTO */
	writeb(0, &regs->MIICR);
	for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
		udelay(1);
		if (BYTE_REG_BITS_IS_ON(MIISR_MIDLE, &regs->MIISR))
			break;
	}
}

/**
 *	enable_mii_autopoll	-	turn on autopolling
 *	@regs: velocity registers
 *
 *	Enable the MII link status autopoll feature on the Velocity
 *	hardware. Wait for it to enable.
 */
static void enable_mii_autopoll(struct mac_regs __iomem *regs)
{
	int ii;

	writeb(0, &(regs->MIICR));
	writeb(MIIADR_SWMPL, &regs->MIIADR);

	for (ii = 0; ii < W_MAX_TIMEOUT; ii++) {
		udelay(1);
		if (BYTE_REG_BITS_IS_ON(MIISR_MIDLE, &regs->MIISR))
			break;
	}

	writeb(MIICR_MAUTO, &regs->MIICR);

	for (ii = 0; ii < W_MAX_TIMEOUT; ii++) {
		udelay(1);
		if (!BYTE_REG_BITS_IS_ON(MIISR_MIDLE, &regs->MIISR))
			break;
	}

}

/**
 *	velocity_mii_read	-	read MII data
 *	@regs: velocity registers
 *	@index: MII register index
 *	@data: buffer for received data
 *
 *	Perform a single read of an MII 16bit register. Returns zero
 *	on success or -ETIMEDOUT if the PHY did not respond.
 */
static int velocity_mii_read(struct mac_regs __iomem *regs, u8 index, u16 *data)
{
	u16 ww;

	/*
	 *	Disable MIICR_MAUTO, so that mii addr can be set normally
	 */
	safe_disable_mii_autopoll(regs);

	writeb(index, &regs->MIIADR);

	BYTE_REG_BITS_ON(MIICR_RCMD, &regs->MIICR);

	for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
		if (!(readb(&regs->MIICR) & MIICR_RCMD))
			break;
	}

	*data = readw(&regs->MIIDATA);

	enable_mii_autopoll(regs);
	if (ww == W_MAX_TIMEOUT)
		return -ETIMEDOUT;
	return 0;
}


/**
 *	mii_check_media_mode	-	check media state
 *	@regs: velocity registers
 *
 *	Check the current MII status and determine the link status
 *	accordingly
 */
static u32 mii_check_media_mode(struct mac_regs __iomem *regs)
{
	u32 status = 0;
	u16 ANAR;

	if (!MII_REG_BITS_IS_ON(BMSR_LSTATUS, MII_BMSR, regs))
		status |= VELOCITY_LINK_FAIL;

	if (MII_REG_BITS_IS_ON(ADVERTISE_1000FULL, MII_CTRL1000, regs))
		status |= VELOCITY_SPEED_1000 | VELOCITY_DUPLEX_FULL;
	else if (MII_REG_BITS_IS_ON(ADVERTISE_1000HALF, MII_CTRL1000, regs))
		status |= (VELOCITY_SPEED_1000);
	else {
		velocity_mii_read(regs, MII_ADVERTISE, &ANAR);
		if (ANAR & ADVERTISE_100FULL)
			status |= (VELOCITY_SPEED_100 | VELOCITY_DUPLEX_FULL);
		else if (ANAR & ADVERTISE_100HALF)
			status |= VELOCITY_SPEED_100;
		else if (ANAR & ADVERTISE_10FULL)
			status |= (VELOCITY_SPEED_10 | VELOCITY_DUPLEX_FULL);
		else
			status |= (VELOCITY_SPEED_10);
	}

	if (MII_REG_BITS_IS_ON(BMCR_ANENABLE, MII_BMCR, regs)) {
		velocity_mii_read(regs, MII_ADVERTISE, &ANAR);
		if ((ANAR & (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF))
		    == (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF)) {
			if (MII_REG_BITS_IS_ON(ADVERTISE_1000HALF | ADVERTISE_1000FULL, MII_CTRL1000, regs))
				status |= VELOCITY_AUTONEG_ENABLE;
		}
	}

	return status;
}

/**
 *	velocity_mii_write	-	write MII data
 *	@regs: velocity registers
 *	@index: MII register index
 *	@data: 16bit data for the MII register
 *
 *	Perform a single write to an MII 16bit register. Returns zero
 *	on success or -ETIMEDOUT if the PHY did not respond.
 */
static int velocity_mii_write(struct mac_regs __iomem *regs, u8 mii_addr, u16 data)
{
	u16 ww;

	/*
	 *	Disable MIICR_MAUTO, so that mii addr can be set normally
	 */
	safe_disable_mii_autopoll(regs);

	/* MII reg offset */
	writeb(mii_addr, &regs->MIIADR);
	/* set MII data */
	writew(data, &regs->MIIDATA);

	/* turn on MIICR_WCMD */
	BYTE_REG_BITS_ON(MIICR_WCMD, &regs->MIICR);

	/* W_MAX_TIMEOUT is the timeout period */
	for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
		udelay(5);
		if (!(readb(&regs->MIICR) & MIICR_WCMD))
			break;
	}
	enable_mii_autopoll(regs);

	if (ww == W_MAX_TIMEOUT)
		return -ETIMEDOUT;
	return 0;
}

/**
 *	set_mii_flow_control	-	flow control setup
 *	@vptr: velocity interface
 *
 *	Set up the flow control on this interface according to
 *	the supplied user/eeprom options.
 */
static void set_mii_flow_control(struct velocity_info *vptr)
{
	/*Enable or Disable PAUSE in ANAR */
	switch (vptr->options.flow_cntl) {
	case FLOW_CNTL_TX:
		MII_REG_BITS_OFF(ADVERTISE_PAUSE_CAP, MII_ADVERTISE, vptr->mac_regs);
		MII_REG_BITS_ON(ADVERTISE_PAUSE_ASYM, MII_ADVERTISE, vptr->mac_regs);
		break;

	case FLOW_CNTL_RX:
		MII_REG_BITS_ON(ADVERTISE_PAUSE_CAP, MII_ADVERTISE, vptr->mac_regs);
		MII_REG_BITS_ON(ADVERTISE_PAUSE_ASYM, MII_ADVERTISE, vptr->mac_regs);
		break;

	case FLOW_CNTL_TX_RX:
		MII_REG_BITS_ON(ADVERTISE_PAUSE_CAP, MII_ADVERTISE, vptr->mac_regs);
		MII_REG_BITS_OFF(ADVERTISE_PAUSE_ASYM, MII_ADVERTISE, vptr->mac_regs);
		break;

	case FLOW_CNTL_DISABLE:
		MII_REG_BITS_OFF(ADVERTISE_PAUSE_CAP, MII_ADVERTISE, vptr->mac_regs);
		MII_REG_BITS_OFF(ADVERTISE_PAUSE_ASYM, MII_ADVERTISE, vptr->mac_regs);
		break;
	default:
		break;
	}
}

/**
 *	mii_set_auto_on		-	autonegotiate on
 *	@vptr: velocity
 *
 *	Enable autonegotation on this interface
 */
static void mii_set_auto_on(struct velocity_info *vptr)
{
	if (MII_REG_BITS_IS_ON(BMCR_ANENABLE, MII_BMCR, vptr->mac_regs))
		MII_REG_BITS_ON(BMCR_ANRESTART, MII_BMCR, vptr->mac_regs);
	else
		MII_REG_BITS_ON(BMCR_ANENABLE, MII_BMCR, vptr->mac_regs);
}

static u32 check_connection_type(struct mac_regs __iomem *regs)
{
	u32 status = 0;
	u8 PHYSR0;
	u16 ANAR;
	PHYSR0 = readb(&regs->PHYSR0);

	/*
	   if (!(PHYSR0 & PHYSR0_LINKGD))
	   status|=VELOCITY_LINK_FAIL;
	 */

	if (PHYSR0 & PHYSR0_FDPX)
		status |= VELOCITY_DUPLEX_FULL;

	if (PHYSR0 & PHYSR0_SPDG)
		status |= VELOCITY_SPEED_1000;
	else if (PHYSR0 & PHYSR0_SPD10)
		status |= VELOCITY_SPEED_10;
	else
		status |= VELOCITY_SPEED_100;

	if (MII_REG_BITS_IS_ON(BMCR_ANENABLE, MII_BMCR, regs)) {
		velocity_mii_read(regs, MII_ADVERTISE, &ANAR);
		if ((ANAR & (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF))
		    == (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF)) {
			if (MII_REG_BITS_IS_ON(ADVERTISE_1000HALF | ADVERTISE_1000FULL, MII_CTRL1000, regs))
				status |= VELOCITY_AUTONEG_ENABLE;
		}
	}

	return status;
}



/**
 *	velocity_set_media_mode		-	set media mode
 *	@mii_status: old MII link state
 *
 *	Check the media link state and configure the flow control
 *	PHY and also velocity hardware setup accordingly. In particular
 *	we need to set up CD polling and frame bursting.
 */
static int velocity_set_media_mode(struct velocity_info *vptr, u32 mii_status)
{
	u32 curr_status;
	struct mac_regs __iomem *regs = vptr->mac_regs;

	vptr->mii_status = mii_check_media_mode(vptr->mac_regs);
	curr_status = vptr->mii_status & (~VELOCITY_LINK_FAIL);

	/* Set mii link status */
	set_mii_flow_control(vptr);

	/*
	   Check if new status is consistent with current status
	   if (((mii_status & curr_status) & VELOCITY_AUTONEG_ENABLE) ||
	       (mii_status==curr_status)) {
	   vptr->mii_status=mii_check_media_mode(vptr->mac_regs);
	   vptr->mii_status=check_connection_type(vptr->mac_regs);
	   VELOCITY_PRT(MSG_LEVEL_INFO, "Velocity link no change\n");
	   return 0;
	   }
	 */

	if (PHYID_GET_PHY_ID(vptr->phy_id) == PHYID_CICADA_CS8201)
		MII_REG_BITS_ON(AUXCR_MDPPS, MII_NCONFIG, vptr->mac_regs);

	/*
	 *	If connection type is AUTO
	 */
	if (mii_status & VELOCITY_AUTONEG_ENABLE) {
		VELOCITY_PRT(MSG_LEVEL_INFO, "Velocity is AUTO mode\n");
		/* clear force MAC mode bit */
		BYTE_REG_BITS_OFF(CHIPGCR_FCMODE, &regs->CHIPGCR);
		/* set duplex mode of MAC according to duplex mode of MII */
		MII_REG_BITS_ON(ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF, MII_ADVERTISE, vptr->mac_regs);
		MII_REG_BITS_ON(ADVERTISE_1000FULL | ADVERTISE_1000HALF, MII_CTRL1000, vptr->mac_regs);
		MII_REG_BITS_ON(BMCR_SPEED1000, MII_BMCR, vptr->mac_regs);

		/* enable AUTO-NEGO mode */
		mii_set_auto_on(vptr);
	} else {
		u16 CTRL1000;
		u16 ANAR;
		u8 CHIPGCR;

		/*
		 * 1. if it's 3119, disable frame bursting in halfduplex mode
		 *    and enable it in fullduplex mode
		 * 2. set correct MII/GMII and half/full duplex mode in CHIPGCR
		 * 3. only enable CD heart beat counter in 10HD mode
		 */

		/* set force MAC mode bit */
		BYTE_REG_BITS_ON(CHIPGCR_FCMODE, &regs->CHIPGCR);

		CHIPGCR = readb(&regs->CHIPGCR);

		if (mii_status & VELOCITY_SPEED_1000)
			CHIPGCR |= CHIPGCR_FCGMII;
		else
			CHIPGCR &= ~CHIPGCR_FCGMII;

		if (mii_status & VELOCITY_DUPLEX_FULL) {
			CHIPGCR |= CHIPGCR_FCFDX;
			writeb(CHIPGCR, &regs->CHIPGCR);
			VELOCITY_PRT(MSG_LEVEL_INFO, "set Velocity to forced full mode\n");
			if (vptr->rev_id < REV_ID_VT3216_A0)
				BYTE_REG_BITS_OFF(TCR_TB2BDIS, &regs->TCR);
		} else {
			CHIPGCR &= ~CHIPGCR_FCFDX;
			VELOCITY_PRT(MSG_LEVEL_INFO, "set Velocity to forced half mode\n");
			writeb(CHIPGCR, &regs->CHIPGCR);
			if (vptr->rev_id < REV_ID_VT3216_A0)
				BYTE_REG_BITS_ON(TCR_TB2BDIS, &regs->TCR);
		}

		velocity_mii_read(vptr->mac_regs, MII_CTRL1000, &CTRL1000);
		CTRL1000 &= ~(ADVERTISE_1000FULL | ADVERTISE_1000HALF);
		if ((mii_status & VELOCITY_SPEED_1000) &&
		    (mii_status & VELOCITY_DUPLEX_FULL)) {
			CTRL1000 |= ADVERTISE_1000FULL;
		}
		velocity_mii_write(vptr->mac_regs, MII_CTRL1000, CTRL1000);

		if (!(mii_status & VELOCITY_DUPLEX_FULL) && (mii_status & VELOCITY_SPEED_10))
			BYTE_REG_BITS_OFF(TESTCFG_HBDIS, &regs->TESTCFG);
		else
			BYTE_REG_BITS_ON(TESTCFG_HBDIS, &regs->TESTCFG);

		/* MII_REG_BITS_OFF(BMCR_SPEED1000, MII_BMCR, vptr->mac_regs); */
		velocity_mii_read(vptr->mac_regs, MII_ADVERTISE, &ANAR);
		ANAR &= (~(ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF));
		if (mii_status & VELOCITY_SPEED_100) {
			if (mii_status & VELOCITY_DUPLEX_FULL)
				ANAR |= ADVERTISE_100FULL;
			else
				ANAR |= ADVERTISE_100HALF;
		} else if (mii_status & VELOCITY_SPEED_10) {
			if (mii_status & VELOCITY_DUPLEX_FULL)
				ANAR |= ADVERTISE_10FULL;
			else
				ANAR |= ADVERTISE_10HALF;
		}
		velocity_mii_write(vptr->mac_regs, MII_ADVERTISE, ANAR);
		/* enable AUTO-NEGO mode */
		mii_set_auto_on(vptr);
		/* MII_REG_BITS_ON(BMCR_ANENABLE, MII_BMCR, vptr->mac_regs); */
	}
	/* vptr->mii_status=mii_check_media_mode(vptr->mac_regs); */
	/* vptr->mii_status=check_connection_type(vptr->mac_regs); */
	return VELOCITY_LINK_CHANGE;
}

/**
 *	velocity_print_link_status	-	link status reporting
 *	@vptr: velocity to report on
 *
 *	Turn the link status of the velocity card into a kernel log
 *	description of the new link state, detailing speed and duplex
 *	status
 */
static void velocity_print_link_status(struct velocity_info *vptr)
{

	if (vptr->mii_status & VELOCITY_LINK_FAIL) {
		VELOCITY_PRT(MSG_LEVEL_INFO, KERN_NOTICE "%s: failed to detect cable link\n", vptr->dev->name);
	} else if (vptr->options.spd_dpx == SPD_DPX_AUTO) {
		VELOCITY_PRT(MSG_LEVEL_INFO, KERN_NOTICE "%s: Link auto-negotiation", vptr->dev->name);

		if (vptr->mii_status & VELOCITY_SPEED_1000)
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 1000M bps");
		else if (vptr->mii_status & VELOCITY_SPEED_100)
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 100M bps");
		else
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 10M bps");

		if (vptr->mii_status & VELOCITY_DUPLEX_FULL)
			VELOCITY_PRT(MSG_LEVEL_INFO, " full duplex\n");
		else
			VELOCITY_PRT(MSG_LEVEL_INFO, " half duplex\n");
	} else {
		VELOCITY_PRT(MSG_LEVEL_INFO, KERN_NOTICE "%s: Link forced", vptr->dev->name);
		switch (vptr->options.spd_dpx) {
		case SPD_DPX_1000_FULL:
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 1000M bps full duplex\n");
			break;
		case SPD_DPX_100_HALF:
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 100M bps half duplex\n");
			break;
		case SPD_DPX_100_FULL:
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 100M bps full duplex\n");
			break;
		case SPD_DPX_10_HALF:
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 10M bps half duplex\n");
			break;
		case SPD_DPX_10_FULL:
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 10M bps full duplex\n");
			break;
		default:
			break;
		}
	}
}

/**
 *	enable_flow_control_ability	-	flow control
 *	@vptr: veloity to configure
 *
 *	Set up flow control according to the flow control options
 *	determined by the eeprom/configuration.
 */
static void enable_flow_control_ability(struct velocity_info *vptr)
{

	struct mac_regs __iomem *regs = vptr->mac_regs;

	switch (vptr->options.flow_cntl) {

	case FLOW_CNTL_DEFAULT:
		if (BYTE_REG_BITS_IS_ON(PHYSR0_RXFLC, &regs->PHYSR0))
			writel(CR0_FDXRFCEN, &regs->CR0Set);
		else
			writel(CR0_FDXRFCEN, &regs->CR0Clr);

		if (BYTE_REG_BITS_IS_ON(PHYSR0_TXFLC, &regs->PHYSR0))
			writel(CR0_FDXTFCEN, &regs->CR0Set);
		else
			writel(CR0_FDXTFCEN, &regs->CR0Clr);
		break;

	case FLOW_CNTL_TX:
		writel(CR0_FDXTFCEN, &regs->CR0Set);
		writel(CR0_FDXRFCEN, &regs->CR0Clr);
		break;

	case FLOW_CNTL_RX:
		writel(CR0_FDXRFCEN, &regs->CR0Set);
		writel(CR0_FDXTFCEN, &regs->CR0Clr);
		break;

	case FLOW_CNTL_TX_RX:
		writel(CR0_FDXTFCEN, &regs->CR0Set);
		writel(CR0_FDXRFCEN, &regs->CR0Set);
		break;

	case FLOW_CNTL_DISABLE:
		writel(CR0_FDXRFCEN, &regs->CR0Clr);
		writel(CR0_FDXTFCEN, &regs->CR0Clr);
		break;

	default:
		break;
	}

}

/**
 *	velocity_soft_reset	-	soft reset
 *	@vptr: velocity to reset
 *
 *	Kick off a soft reset of the velocity adapter and then poll
 *	until the reset sequence has completed before returning.
 */
static int velocity_soft_reset(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	int i = 0;

	writel(CR0_SFRST, &regs->CR0Set);

	for (i = 0; i < W_MAX_TIMEOUT; i++) {
		udelay(5);
		if (!DWORD_REG_BITS_IS_ON(CR0_SFRST, &regs->CR0Set))
			break;
	}

	if (i == W_MAX_TIMEOUT) {
		writel(CR0_FORSRST, &regs->CR0Set);
		/* FIXME: PCI POSTING */
		/* delay 2ms */
		mdelay(2);
	}
	return 0;
}

/**
 *	velocity_set_multi	-	filter list change callback
 *	@dev: network device
 *
 *	Called by the network layer when the filter lists need to change
 *	for a velocity adapter. Reload the CAMs with the new address
 *	filter ruleset.
 */
static void velocity_set_multi(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	struct mac_regs __iomem *regs = vptr->mac_regs;
	u8 rx_mode;
	int i;
	struct netdev_hw_addr *ha;

	if (dev->flags & IFF_PROMISC) {	/* Set promiscuous. */
		writel(0xffffffff, &regs->MARCAM[0]);
		writel(0xffffffff, &regs->MARCAM[4]);
		rx_mode = (RCR_AM | RCR_AB | RCR_PROM);
	} else if ((netdev_mc_count(dev) > vptr->multicast_limit) ||
		   (dev->flags & IFF_ALLMULTI)) {
		writel(0xffffffff, &regs->MARCAM[0]);
		writel(0xffffffff, &regs->MARCAM[4]);
		rx_mode = (RCR_AM | RCR_AB);
	} else {
		int offset = MCAM_SIZE - vptr->multicast_limit;
		mac_get_cam_mask(regs, vptr->mCAMmask);

		i = 0;
		netdev_for_each_mc_addr(ha, dev) {
			mac_set_cam(regs, i + offset, ha->addr);
			vptr->mCAMmask[(offset + i) / 8] |= 1 << ((offset + i) & 7);
			i++;
		}

		mac_set_cam_mask(regs, vptr->mCAMmask);
		rx_mode = RCR_AM | RCR_AB | RCR_AP;
	}
	if (dev->mtu > 1500)
		rx_mode |= RCR_AL;

	BYTE_REG_BITS_ON(rx_mode, &regs->RCR);

}

/*
 * MII access , media link mode setting functions
 */

/**
 *	mii_init	-	set up MII
 *	@vptr: velocity adapter
 *	@mii_status:  links tatus
 *
 *	Set up the PHY for the current link state.
 */
static void mii_init(struct velocity_info *vptr, u32 mii_status)
{
	u16 BMCR;

	switch (PHYID_GET_PHY_ID(vptr->phy_id)) {
	case PHYID_CICADA_CS8201:
		/*
		 *	Reset to hardware default
		 */
		MII_REG_BITS_OFF((ADVERTISE_PAUSE_ASYM | ADVERTISE_PAUSE_CAP), MII_ADVERTISE, vptr->mac_regs);
		/*
		 *	Turn on ECHODIS bit in NWay-forced full mode and turn it
		 *	off it in NWay-forced half mode for NWay-forced v.s.
		 *	legacy-forced issue.
		 */
		if (vptr->mii_status & VELOCITY_DUPLEX_FULL)
			MII_REG_BITS_ON(TCSR_ECHODIS, MII_SREVISION, vptr->mac_regs);
		else
			MII_REG_BITS_OFF(TCSR_ECHODIS, MII_SREVISION, vptr->mac_regs);
		/*
		 *	Turn on Link/Activity LED enable bit for CIS8201
		 */
		MII_REG_BITS_ON(PLED_LALBE, MII_TPISTATUS, vptr->mac_regs);
		break;
	case PHYID_VT3216_32BIT:
	case PHYID_VT3216_64BIT:
		/*
		 *	Reset to hardware default
		 */
		MII_REG_BITS_ON((ADVERTISE_PAUSE_ASYM | ADVERTISE_PAUSE_CAP), MII_ADVERTISE, vptr->mac_regs);
		/*
		 *	Turn on ECHODIS bit in NWay-forced full mode and turn it
		 *	off it in NWay-forced half mode for NWay-forced v.s.
		 *	legacy-forced issue
		 */
		if (vptr->mii_status & VELOCITY_DUPLEX_FULL)
			MII_REG_BITS_ON(TCSR_ECHODIS, MII_SREVISION, vptr->mac_regs);
		else
			MII_REG_BITS_OFF(TCSR_ECHODIS, MII_SREVISION, vptr->mac_regs);
		break;

	case PHYID_MARVELL_1000:
	case PHYID_MARVELL_1000S:
		/*
		 *	Assert CRS on Transmit
		 */
		MII_REG_BITS_ON(PSCR_ACRSTX, MII_REG_PSCR, vptr->mac_regs);
		/*
		 *	Reset to hardware default
		 */
		MII_REG_BITS_ON((ADVERTISE_PAUSE_ASYM | ADVERTISE_PAUSE_CAP), MII_ADVERTISE, vptr->mac_regs);
		break;
	default:
		;
	}
	velocity_mii_read(vptr->mac_regs, MII_BMCR, &BMCR);
	if (BMCR & BMCR_ISOLATE) {
		BMCR &= ~BMCR_ISOLATE;
		velocity_mii_write(vptr->mac_regs, MII_BMCR, BMCR);
	}
}

/**
 * setup_queue_timers	-	Setup interrupt timers
 *
 * Setup interrupt frequency during suppression (timeout if the frame
 * count isn't filled).
 */
static void setup_queue_timers(struct velocity_info *vptr)
{
	/* Only for newer revisions */
	if (vptr->rev_id >= REV_ID_VT3216_A0) {
		u8 txqueue_timer = 0;
		u8 rxqueue_timer = 0;

		if (vptr->mii_status & (VELOCITY_SPEED_1000 |
				VELOCITY_SPEED_100)) {
			txqueue_timer = vptr->options.txqueue_timer;
			rxqueue_timer = vptr->options.rxqueue_timer;
		}

		writeb(txqueue_timer, &vptr->mac_regs->TQETMR);
		writeb(rxqueue_timer, &vptr->mac_regs->RQETMR);
	}
}
/**
 * setup_adaptive_interrupts  -  Setup interrupt suppression
 *
 * @vptr velocity adapter
 *
 * The velocity is able to suppress interrupt during high interrupt load.
 * This function turns on that feature.
 */
static void setup_adaptive_interrupts(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	u16 tx_intsup = vptr->options.tx_intsup;
	u16 rx_intsup = vptr->options.rx_intsup;

	/* Setup default interrupt mask (will be changed below) */
	vptr->int_mask = INT_MASK_DEF;

	/* Set Tx Interrupt Suppression Threshold */
	writeb(CAMCR_PS0, &regs->CAMCR);
	if (tx_intsup != 0) {
		vptr->int_mask &= ~(ISR_PTXI | ISR_PTX0I | ISR_PTX1I |
				ISR_PTX2I | ISR_PTX3I);
		writew(tx_intsup, &regs->ISRCTL);
	} else
		writew(ISRCTL_TSUPDIS, &regs->ISRCTL);

	/* Set Rx Interrupt Suppression Threshold */
	writeb(CAMCR_PS1, &regs->CAMCR);
	if (rx_intsup != 0) {
		vptr->int_mask &= ~ISR_PRXI;
		writew(rx_intsup, &regs->ISRCTL);
	} else
		writew(ISRCTL_RSUPDIS, &regs->ISRCTL);

	/* Select page to interrupt hold timer */
	writeb(0, &regs->CAMCR);
}

/**
 *	velocity_init_registers	-	initialise MAC registers
 *	@vptr: velocity to init
 *	@type: type of initialisation (hot or cold)
 *
 *	Initialise the MAC on a reset or on first set up on the
 *	hardware.
 */
static void velocity_init_registers(struct velocity_info *vptr,
				    enum velocity_init_type type)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	int i, mii_status;

	mac_wol_reset(regs);

	switch (type) {
	case VELOCITY_INIT_RESET:
	case VELOCITY_INIT_WOL:

		netif_stop_queue(vptr->dev);

		/*
		 *	Reset RX to prevent RX pointer not on the 4X location
		 */
		velocity_rx_reset(vptr);
		mac_rx_queue_run(regs);
		mac_rx_queue_wake(regs);

		mii_status = velocity_get_opt_media_mode(vptr);
		if (velocity_set_media_mode(vptr, mii_status) != VELOCITY_LINK_CHANGE) {
			velocity_print_link_status(vptr);
			if (!(vptr->mii_status & VELOCITY_LINK_FAIL))
				netif_wake_queue(vptr->dev);
		}

		enable_flow_control_ability(vptr);

		mac_clear_isr(regs);
		writel(CR0_STOP, &regs->CR0Clr);
		writel((CR0_DPOLL | CR0_TXON | CR0_RXON | CR0_STRT),
							&regs->CR0Set);

		break;

	case VELOCITY_INIT_COLD:
	default:
		/*
		 *	Do reset
		 */
		velocity_soft_reset(vptr);
		mdelay(5);

		mac_eeprom_reload(regs);
		for (i = 0; i < 6; i++)
			writeb(vptr->dev->dev_addr[i], &(regs->PAR[i]));

		/*
		 *	clear Pre_ACPI bit.
		 */
		BYTE_REG_BITS_OFF(CFGA_PACPI, &(regs->CFGA));
		mac_set_rx_thresh(regs, vptr->options.rx_thresh);
		mac_set_dma_length(regs, vptr->options.DMA_length);

		writeb(WOLCFG_SAM | WOLCFG_SAB, &regs->WOLCFGSet);
		/*
		 *	Back off algorithm use original IEEE standard
		 */
		BYTE_REG_BITS_SET(CFGB_OFSET, (CFGB_CRANDOM | CFGB_CAP | CFGB_MBA | CFGB_BAKOPT), &regs->CFGB);

		/*
		 *	Init CAM filter
		 */
		velocity_init_cam_filter(vptr);

		/*
		 *	Set packet filter: Receive directed and broadcast address
		 */
		velocity_set_multi(vptr->dev);

		/*
		 *	Enable MII auto-polling
		 */
		enable_mii_autopoll(regs);

		setup_adaptive_interrupts(vptr);

		writel(vptr->rx.pool_dma, &regs->RDBaseLo);
		writew(vptr->options.numrx - 1, &regs->RDCSize);
		mac_rx_queue_run(regs);
		mac_rx_queue_wake(regs);

		writew(vptr->options.numtx - 1, &regs->TDCSize);

		for (i = 0; i < vptr->tx.numq; i++) {
			writel(vptr->tx.pool_dma[i], &regs->TDBaseLo[i]);
			mac_tx_queue_run(regs, i);
		}

		init_flow_control_register(vptr);

		writel(CR0_STOP, &regs->CR0Clr);
		writel((CR0_DPOLL | CR0_TXON | CR0_RXON | CR0_STRT), &regs->CR0Set);

		mii_status = velocity_get_opt_media_mode(vptr);
		netif_stop_queue(vptr->dev);

		mii_init(vptr, mii_status);

		if (velocity_set_media_mode(vptr, mii_status) != VELOCITY_LINK_CHANGE) {
			velocity_print_link_status(vptr);
			if (!(vptr->mii_status & VELOCITY_LINK_FAIL))
				netif_wake_queue(vptr->dev);
		}

		enable_flow_control_ability(vptr);
		mac_hw_mibs_init(regs);
		mac_write_int_mask(vptr->int_mask, regs);
		mac_clear_isr(regs);

	}
}

static void velocity_give_many_rx_descs(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	int avail, dirty, unusable;

	/*
	 * RD number must be equal to 4X per hardware spec
	 * (programming guide rev 1.20, p.13)
	 */
	if (vptr->rx.filled < 4)
		return;

	wmb();

	unusable = vptr->rx.filled & 0x0003;
	dirty = vptr->rx.dirty - unusable;
	for (avail = vptr->rx.filled & 0xfffc; avail; avail--) {
		dirty = (dirty > 0) ? dirty - 1 : vptr->options.numrx - 1;
		vptr->rx.ring[dirty].rdesc0.len |= OWNED_BY_NIC;
	}

	writew(vptr->rx.filled & 0xfffc, &regs->RBRDU);
	vptr->rx.filled = unusable;
}

/**
 *	velocity_init_dma_rings	-	set up DMA rings
 *	@vptr: Velocity to set up
 *
 *	Allocate PCI mapped DMA rings for the receive and transmit layer
 *	to use.
 */
static int velocity_init_dma_rings(struct velocity_info *vptr)
{
	struct velocity_opt *opt = &vptr->options;
	const unsigned int rx_ring_size = opt->numrx * sizeof(struct rx_desc);
	const unsigned int tx_ring_size = opt->numtx * sizeof(struct tx_desc);
	struct pci_dev *pdev = vptr->pdev;
	dma_addr_t pool_dma;
	void *pool;
	unsigned int i;

	/*
	 * Allocate all RD/TD rings a single pool.
	 *
	 * pci_alloc_consistent() fulfills the requirement for 64 bytes
	 * alignment
	 */
	pool = pci_alloc_consistent(pdev, tx_ring_size * vptr->tx.numq +
				    rx_ring_size, &pool_dma);
	if (!pool) {
		dev_err(&pdev->dev, "%s : DMA memory allocation failed.\n",
			vptr->dev->name);
		return -ENOMEM;
	}

	vptr->rx.ring = pool;
	vptr->rx.pool_dma = pool_dma;

	pool += rx_ring_size;
	pool_dma += rx_ring_size;

	for (i = 0; i < vptr->tx.numq; i++) {
		vptr->tx.rings[i] = pool;
		vptr->tx.pool_dma[i] = pool_dma;
		pool += tx_ring_size;
		pool_dma += tx_ring_size;
	}

	return 0;
}

static void velocity_set_rxbufsize(struct velocity_info *vptr, int mtu)
{
	vptr->rx.buf_sz = (mtu <= ETH_DATA_LEN) ? PKT_BUF_SZ : mtu + 32;
}

/**
 *	velocity_alloc_rx_buf	-	allocate aligned receive buffer
 *	@vptr: velocity
 *	@idx: ring index
 *
 *	Allocate a new full sized buffer for the reception of a frame and
 *	map it into PCI space for the hardware to use. The hardware
 *	requires *64* byte alignment of the buffer which makes life
 *	less fun than would be ideal.
 */
static int velocity_alloc_rx_buf(struct velocity_info *vptr, int idx)
{
	struct rx_desc *rd = &(vptr->rx.ring[idx]);
	struct velocity_rd_info *rd_info = &(vptr->rx.info[idx]);

	rd_info->skb = dev_alloc_skb(vptr->rx.buf_sz + 64);
	if (rd_info->skb == NULL)
		return -ENOMEM;

	/*
	 *	Do the gymnastics to get the buffer head for data at
	 *	64byte alignment.
	 */
	skb_reserve(rd_info->skb,
			64 - ((unsigned long) rd_info->skb->data & 63));
	rd_info->skb_dma = pci_map_single(vptr->pdev, rd_info->skb->data,
					vptr->rx.buf_sz, PCI_DMA_FROMDEVICE);

	/*
	 *	Fill in the descriptor to match
	 */

	*((u32 *) & (rd->rdesc0)) = 0;
	rd->size = cpu_to_le16(vptr->rx.buf_sz) | RX_INTEN;
	rd->pa_low = cpu_to_le32(rd_info->skb_dma);
	rd->pa_high = 0;
	return 0;
}


static int velocity_rx_refill(struct velocity_info *vptr)
{
	int dirty = vptr->rx.dirty, done = 0;

	do {
		struct rx_desc *rd = vptr->rx.ring + dirty;

		/* Fine for an all zero Rx desc at init time as well */
		if (rd->rdesc0.len & OWNED_BY_NIC)
			break;

		if (!vptr->rx.info[dirty].skb) {
			if (velocity_alloc_rx_buf(vptr, dirty) < 0)
				break;
		}
		done++;
		dirty = (dirty < vptr->options.numrx - 1) ? dirty + 1 : 0;
	} while (dirty != vptr->rx.curr);

	if (done) {
		vptr->rx.dirty = dirty;
		vptr->rx.filled += done;
	}

	return done;
}

/**
 *	velocity_free_rd_ring	-	free receive ring
 *	@vptr: velocity to clean up
 *
 *	Free the receive buffers for each ring slot and any
 *	attached socket buffers that need to go away.
 */
static void velocity_free_rd_ring(struct velocity_info *vptr)
{
	int i;

	if (vptr->rx.info == NULL)
		return;

	for (i = 0; i < vptr->options.numrx; i++) {
		struct velocity_rd_info *rd_info = &(vptr->rx.info[i]);
		struct rx_desc *rd = vptr->rx.ring + i;

		memset(rd, 0, sizeof(*rd));

		if (!rd_info->skb)
			continue;
		pci_unmap_single(vptr->pdev, rd_info->skb_dma, vptr->rx.buf_sz,
				 PCI_DMA_FROMDEVICE);
		rd_info->skb_dma = 0;

		dev_kfree_skb(rd_info->skb);
		rd_info->skb = NULL;
	}

	kfree(vptr->rx.info);
	vptr->rx.info = NULL;
}



/**
 *	velocity_init_rd_ring	-	set up receive ring
 *	@vptr: velocity to configure
 *
 *	Allocate and set up the receive buffers for each ring slot and
 *	assign them to the network adapter.
 */
static int velocity_init_rd_ring(struct velocity_info *vptr)
{
	int ret = -ENOMEM;

	vptr->rx.info = kcalloc(vptr->options.numrx,
				sizeof(struct velocity_rd_info), GFP_KERNEL);
	if (!vptr->rx.info)
		goto out;

	velocity_init_rx_ring_indexes(vptr);

	if (velocity_rx_refill(vptr) != vptr->options.numrx) {
		VELOCITY_PRT(MSG_LEVEL_ERR, KERN_ERR
			"%s: failed to allocate RX buffer.\n", vptr->dev->name);
		velocity_free_rd_ring(vptr);
		goto out;
	}

	ret = 0;
out:
	return ret;
}

/**
 *	velocity_init_td_ring	-	set up transmit ring
 *	@vptr:	velocity
 *
 *	Set up the transmit ring and chain the ring pointers together.
 *	Returns zero on success or a negative posix errno code for
 *	failure.
 */
static int velocity_init_td_ring(struct velocity_info *vptr)
{
	int j;

	/* Init the TD ring entries */
	for (j = 0; j < vptr->tx.numq; j++) {

		vptr->tx.infos[j] = kcalloc(vptr->options.numtx,
					    sizeof(struct velocity_td_info),
					    GFP_KERNEL);
		if (!vptr->tx.infos[j])	{
			while (--j >= 0)
				kfree(vptr->tx.infos[j]);
			return -ENOMEM;
		}

		vptr->tx.tail[j] = vptr->tx.curr[j] = vptr->tx.used[j] = 0;
	}
	return 0;
}

/**
 *	velocity_free_dma_rings	-	free PCI ring pointers
 *	@vptr: Velocity to free from
 *
 *	Clean up the PCI ring buffers allocated to this velocity.
 */
static void velocity_free_dma_rings(struct velocity_info *vptr)
{
	const int size = vptr->options.numrx * sizeof(struct rx_desc) +
		vptr->options.numtx * sizeof(struct tx_desc) * vptr->tx.numq;

	pci_free_consistent(vptr->pdev, size, vptr->rx.ring, vptr->rx.pool_dma);
}


static int velocity_init_rings(struct velocity_info *vptr, int mtu)
{
	int ret;

	velocity_set_rxbufsize(vptr, mtu);

	ret = velocity_init_dma_rings(vptr);
	if (ret < 0)
		goto out;

	ret = velocity_init_rd_ring(vptr);
	if (ret < 0)
		goto err_free_dma_rings_0;

	ret = velocity_init_td_ring(vptr);
	if (ret < 0)
		goto err_free_rd_ring_1;
out:
	return ret;

err_free_rd_ring_1:
	velocity_free_rd_ring(vptr);
err_free_dma_rings_0:
	velocity_free_dma_rings(vptr);
	goto out;
}

/**
 *	velocity_free_tx_buf	-	free transmit buffer
 *	@vptr: velocity
 *	@tdinfo: buffer
 *
 *	Release an transmit buffer. If the buffer was preallocated then
 *	recycle it, if not then unmap the buffer.
 */
static void velocity_free_tx_buf(struct velocity_info *vptr,
		struct velocity_td_info *tdinfo, struct tx_desc *td)
{
	struct sk_buff *skb = tdinfo->skb;

	/*
	 *	Don't unmap the pre-allocated tx_bufs
	 */
	if (tdinfo->skb_dma) {
		int i;

		for (i = 0; i < tdinfo->nskb_dma; i++) {
			size_t pktlen = max_t(size_t, skb->len, ETH_ZLEN);

			/* For scatter-gather */
			if (skb_shinfo(skb)->nr_frags > 0)
				pktlen = max_t(size_t, pktlen,
						td->td_buf[i].size & ~TD_QUEUE);

			pci_unmap_single(vptr->pdev, tdinfo->skb_dma[i],
					le16_to_cpu(pktlen), PCI_DMA_TODEVICE);
		}
	}
	dev_kfree_skb_irq(skb);
	tdinfo->skb = NULL;
}


/*
 *	FIXME: could we merge this with velocity_free_tx_buf ?
 */
static void velocity_free_td_ring_entry(struct velocity_info *vptr,
							 int q, int n)
{
	struct velocity_td_info *td_info = &(vptr->tx.infos[q][n]);
	int i;

	if (td_info == NULL)
		return;

	if (td_info->skb) {
		for (i = 0; i < td_info->nskb_dma; i++) {
			if (td_info->skb_dma[i]) {
				pci_unmap_single(vptr->pdev, td_info->skb_dma[i],
					td_info->skb->len, PCI_DMA_TODEVICE);
				td_info->skb_dma[i] = 0;
			}
		}
		dev_kfree_skb(td_info->skb);
		td_info->skb = NULL;
	}
}

/**
 *	velocity_free_td_ring	-	free td ring
 *	@vptr: velocity
 *
 *	Free up the transmit ring for this particular velocity adapter.
 *	We free the ring contents but not the ring itself.
 */
static void velocity_free_td_ring(struct velocity_info *vptr)
{
	int i, j;

	for (j = 0; j < vptr->tx.numq; j++) {
		if (vptr->tx.infos[j] == NULL)
			continue;
		for (i = 0; i < vptr->options.numtx; i++)
			velocity_free_td_ring_entry(vptr, j, i);

		kfree(vptr->tx.infos[j]);
		vptr->tx.infos[j] = NULL;
	}
}


static void velocity_free_rings(struct velocity_info *vptr)
{
	velocity_free_td_ring(vptr);
	velocity_free_rd_ring(vptr);
	velocity_free_dma_rings(vptr);
}

/**
 *	velocity_error	-	handle error from controller
 *	@vptr: velocity
 *	@status: card status
 *
 *	Process an error report from the hardware and attempt to recover
 *	the card itself. At the moment we cannot recover from some
 *	theoretically impossible errors but this could be fixed using
 *	the pci_device_failed logic to bounce the hardware
 *
 */
static void velocity_error(struct velocity_info *vptr, int status)
{

	if (status & ISR_TXSTLI) {
		struct mac_regs __iomem *regs = vptr->mac_regs;

		printk(KERN_ERR "TD structure error TDindex=%hx\n", readw(&regs->TDIdx[0]));
		BYTE_REG_BITS_ON(TXESR_TDSTR, &regs->TXESR);
		writew(TRDCSR_RUN, &regs->TDCSRClr);
		netif_stop_queue(vptr->dev);

		/* FIXME: port over the pci_device_failed code and use it
		   here */
	}

	if (status & ISR_SRCI) {
		struct mac_regs __iomem *regs = vptr->mac_regs;
		int linked;

		if (vptr->options.spd_dpx == SPD_DPX_AUTO) {
			vptr->mii_status = check_connection_type(regs);

			/*
			 *	If it is a 3119, disable frame bursting in
			 *	halfduplex mode and enable it in fullduplex
			 *	 mode
			 */
			if (vptr->rev_id < REV_ID_VT3216_A0) {
				if (vptr->mii_status & VELOCITY_DUPLEX_FULL)
					BYTE_REG_BITS_ON(TCR_TB2BDIS, &regs->TCR);
				else
					BYTE_REG_BITS_OFF(TCR_TB2BDIS, &regs->TCR);
			}
			/*
			 *	Only enable CD heart beat counter in 10HD mode
			 */
			if (!(vptr->mii_status & VELOCITY_DUPLEX_FULL) && (vptr->mii_status & VELOCITY_SPEED_10))
				BYTE_REG_BITS_OFF(TESTCFG_HBDIS, &regs->TESTCFG);
			else
				BYTE_REG_BITS_ON(TESTCFG_HBDIS, &regs->TESTCFG);

			setup_queue_timers(vptr);
		}
		/*
		 *	Get link status from PHYSR0
		 */
		linked = readb(&regs->PHYSR0) & PHYSR0_LINKGD;

		if (linked) {
			vptr->mii_status &= ~VELOCITY_LINK_FAIL;
			netif_carrier_on(vptr->dev);
		} else {
			vptr->mii_status |= VELOCITY_LINK_FAIL;
			netif_carrier_off(vptr->dev);
		}

		velocity_print_link_status(vptr);
		enable_flow_control_ability(vptr);

		/*
		 *	Re-enable auto-polling because SRCI will disable
		 *	auto-polling
		 */

		enable_mii_autopoll(regs);

		if (vptr->mii_status & VELOCITY_LINK_FAIL)
			netif_stop_queue(vptr->dev);
		else
			netif_wake_queue(vptr->dev);

	};
	if (status & ISR_MIBFI)
		velocity_update_hw_mibs(vptr);
	if (status & ISR_LSTEI)
		mac_rx_queue_wake(vptr->mac_regs);
}

/**
 *	tx_srv		-	transmit interrupt service
 *	@vptr; Velocity
 *
 *	Scan the queues looking for transmitted packets that
 *	we can complete and clean up. Update any statistics as
 *	necessary/
 */
static int velocity_tx_srv(struct velocity_info *vptr)
{
	struct tx_desc *td;
	int qnum;
	int full = 0;
	int idx;
	int works = 0;
	struct velocity_td_info *tdinfo;
	struct net_device_stats *stats = &vptr->dev->stats;

	for (qnum = 0; qnum < vptr->tx.numq; qnum++) {
		for (idx = vptr->tx.tail[qnum]; vptr->tx.used[qnum] > 0;
			idx = (idx + 1) % vptr->options.numtx) {

			/*
			 *	Get Tx Descriptor
			 */
			td = &(vptr->tx.rings[qnum][idx]);
			tdinfo = &(vptr->tx.infos[qnum][idx]);

			if (td->tdesc0.len & OWNED_BY_NIC)
				break;

			if ((works++ > 15))
				break;

			if (td->tdesc0.TSR & TSR0_TERR) {
				stats->tx_errors++;
				stats->tx_dropped++;
				if (td->tdesc0.TSR & TSR0_CDH)
					stats->tx_heartbeat_errors++;
				if (td->tdesc0.TSR & TSR0_CRS)
					stats->tx_carrier_errors++;
				if (td->tdesc0.TSR & TSR0_ABT)
					stats->tx_aborted_errors++;
				if (td->tdesc0.TSR & TSR0_OWC)
					stats->tx_window_errors++;
			} else {
				stats->tx_packets++;
				stats->tx_bytes += tdinfo->skb->len;
			}
			velocity_free_tx_buf(vptr, tdinfo, td);
			vptr->tx.used[qnum]--;
		}
		vptr->tx.tail[qnum] = idx;

		if (AVAIL_TD(vptr, qnum) < 1)
			full = 1;
	}
	/*
	 *	Look to see if we should kick the transmit network
	 *	layer for more work.
	 */
	if (netif_queue_stopped(vptr->dev) && (full == 0) &&
	    (!(vptr->mii_status & VELOCITY_LINK_FAIL))) {
		netif_wake_queue(vptr->dev);
	}
	return works;
}

/**
 *	velocity_rx_csum	-	checksum process
 *	@rd: receive packet descriptor
 *	@skb: network layer packet buffer
 *
 *	Process the status bits for the received packet and determine
 *	if the checksum was computed and verified by the hardware
 */
static inline void velocity_rx_csum(struct rx_desc *rd, struct sk_buff *skb)
{
	skb_checksum_none_assert(skb);

	if (rd->rdesc1.CSM & CSM_IPKT) {
		if (rd->rdesc1.CSM & CSM_IPOK) {
			if ((rd->rdesc1.CSM & CSM_TCPKT) ||
					(rd->rdesc1.CSM & CSM_UDPKT)) {
				if (!(rd->rdesc1.CSM & CSM_TUPOK))
					return;
			}
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		}
	}
}

/**
 *	velocity_rx_copy	-	in place Rx copy for small packets
 *	@rx_skb: network layer packet buffer candidate
 *	@pkt_size: received data size
 *	@rd: receive packet descriptor
 *	@dev: network device
 *
 *	Replace the current skb that is scheduled for Rx processing by a
 *	shorter, immediately allocated skb, if the received packet is small
 *	enough. This function returns a negative value if the received
 *	packet is too big or if memory is exhausted.
 */
static int velocity_rx_copy(struct sk_buff **rx_skb, int pkt_size,
			    struct velocity_info *vptr)
{
	int ret = -1;
	if (pkt_size < rx_copybreak) {
		struct sk_buff *new_skb;

		new_skb = netdev_alloc_skb_ip_align(vptr->dev, pkt_size);
		if (new_skb) {
			new_skb->ip_summed = rx_skb[0]->ip_summed;
			skb_copy_from_linear_data(*rx_skb, new_skb->data, pkt_size);
			*rx_skb = new_skb;
			ret = 0;
		}

	}
	return ret;
}

/**
 *	velocity_iph_realign	-	IP header alignment
 *	@vptr: velocity we are handling
 *	@skb: network layer packet buffer
 *	@pkt_size: received data size
 *
 *	Align IP header on a 2 bytes boundary. This behavior can be
 *	configured by the user.
 */
static inline void velocity_iph_realign(struct velocity_info *vptr,
					struct sk_buff *skb, int pkt_size)
{
	if (vptr->flags & VELOCITY_FLAGS_IP_ALIGN) {
		memmove(skb->data + 2, skb->data, pkt_size);
		skb_reserve(skb, 2);
	}
}


/**
 *	velocity_receive_frame	-	received packet processor
 *	@vptr: velocity we are handling
 *	@idx: ring index
 *
 *	A packet has arrived. We process the packet and if appropriate
 *	pass the frame up the network stack
 */
static int velocity_receive_frame(struct velocity_info *vptr, int idx)
{
	void (*pci_action)(struct pci_dev *, dma_addr_t, size_t, int);
	struct net_device_stats *stats = &vptr->dev->stats;
	struct velocity_rd_info *rd_info = &(vptr->rx.info[idx]);
	struct rx_desc *rd = &(vptr->rx.ring[idx]);
	int pkt_len = le16_to_cpu(rd->rdesc0.len) & 0x3fff;
	struct sk_buff *skb;

	if (rd->rdesc0.RSR & (RSR_STP | RSR_EDP)) {
		VELOCITY_PRT(MSG_LEVEL_VERBOSE, KERN_ERR " %s : the received frame span multple RDs.\n", vptr->dev->name);
		stats->rx_length_errors++;
		return -EINVAL;
	}

	if (rd->rdesc0.RSR & RSR_MAR)
		stats->multicast++;

	skb = rd_info->skb;

	pci_dma_sync_single_for_cpu(vptr->pdev, rd_info->skb_dma,
				    vptr->rx.buf_sz, PCI_DMA_FROMDEVICE);

	/*
	 *	Drop frame not meeting IEEE 802.3
	 */

	if (vptr->flags & VELOCITY_FLAGS_VAL_PKT_LEN) {
		if (rd->rdesc0.RSR & RSR_RL) {
			stats->rx_length_errors++;
			return -EINVAL;
		}
	}

	pci_action = pci_dma_sync_single_for_device;

	velocity_rx_csum(rd, skb);

	if (velocity_rx_copy(&skb, pkt_len, vptr) < 0) {
		velocity_iph_realign(vptr, skb, pkt_len);
		pci_action = pci_unmap_single;
		rd_info->skb = NULL;
	}

	pci_action(vptr->pdev, rd_info->skb_dma, vptr->rx.buf_sz,
		   PCI_DMA_FROMDEVICE);

	skb_put(skb, pkt_len - 4);
	skb->protocol = eth_type_trans(skb, vptr->dev);

	if (vptr->vlgrp && (rd->rdesc0.RSR & RSR_DETAG)) {
		vlan_hwaccel_rx(skb, vptr->vlgrp,
				swab16(le16_to_cpu(rd->rdesc1.PQTAG)));
	} else
		netif_rx(skb);

	stats->rx_bytes += pkt_len;

	return 0;
}


/**
 *	velocity_rx_srv		-	service RX interrupt
 *	@vptr: velocity
 *
 *	Walk the receive ring of the velocity adapter and remove
 *	any received packets from the receive queue. Hand the ring
 *	slots back to the adapter for reuse.
 */
static int velocity_rx_srv(struct velocity_info *vptr, int budget_left)
{
	struct net_device_stats *stats = &vptr->dev->stats;
	int rd_curr = vptr->rx.curr;
	int works = 0;

	while (works < budget_left) {
		struct rx_desc *rd = vptr->rx.ring + rd_curr;

		if (!vptr->rx.info[rd_curr].skb)
			break;

		if (rd->rdesc0.len & OWNED_BY_NIC)
			break;

		rmb();

		/*
		 *	Don't drop CE or RL error frame although RXOK is off
		 */
		if (rd->rdesc0.RSR & (RSR_RXOK | RSR_CE | RSR_RL)) {
			if (velocity_receive_frame(vptr, rd_curr) < 0)
				stats->rx_dropped++;
		} else {
			if (rd->rdesc0.RSR & RSR_CRC)
				stats->rx_crc_errors++;
			if (rd->rdesc0.RSR & RSR_FAE)
				stats->rx_frame_errors++;

			stats->rx_dropped++;
		}

		rd->size |= RX_INTEN;

		rd_curr++;
		if (rd_curr >= vptr->options.numrx)
			rd_curr = 0;
		works++;
	}

	vptr->rx.curr = rd_curr;

	if ((works > 0) && (velocity_rx_refill(vptr) > 0))
		velocity_give_many_rx_descs(vptr);

	VAR_USED(stats);
	return works;
}

static int velocity_poll(struct napi_struct *napi, int budget)
{
	struct velocity_info *vptr = container_of(napi,
			struct velocity_info, napi);
	unsigned int rx_done;
	unsigned long flags;

	spin_lock_irqsave(&vptr->lock, flags);
	/*
	 * Do rx and tx twice for performance (taken from the VIA
	 * out-of-tree driver).
	 */
	rx_done = velocity_rx_srv(vptr, budget / 2);
	velocity_tx_srv(vptr);
	rx_done += velocity_rx_srv(vptr, budget - rx_done);
	velocity_tx_srv(vptr);

	/* If budget not fully consumed, exit the polling mode */
	if (rx_done < budget) {
		napi_complete(napi);
		mac_enable_int(vptr->mac_regs);
	}
	spin_unlock_irqrestore(&vptr->lock, flags);

	return rx_done;
}

/**
 *	velocity_intr		-	interrupt callback
 *	@irq: interrupt number
 *	@dev_instance: interrupting device
 *
 *	Called whenever an interrupt is generated by the velocity
 *	adapter IRQ line. We may not be the source of the interrupt
 *	and need to identify initially if we are, and if not exit as
 *	efficiently as possible.
 */
static irqreturn_t velocity_intr(int irq, void *dev_instance)
{
	struct net_device *dev = dev_instance;
	struct velocity_info *vptr = netdev_priv(dev);
	u32 isr_status;

	spin_lock(&vptr->lock);
	isr_status = mac_read_isr(vptr->mac_regs);

	/* Not us ? */
	if (isr_status == 0) {
		spin_unlock(&vptr->lock);
		return IRQ_NONE;
	}

	/* Ack the interrupt */
	mac_write_isr(vptr->mac_regs, isr_status);

	if (likely(napi_schedule_prep(&vptr->napi))) {
		mac_disable_int(vptr->mac_regs);
		__napi_schedule(&vptr->napi);
	}

	if (isr_status & (~(ISR_PRXI | ISR_PPRXI | ISR_PTXI | ISR_PPTXI)))
		velocity_error(vptr, isr_status);

	spin_unlock(&vptr->lock);

	return IRQ_HANDLED;
}

/**
 *	velocity_open		-	interface activation callback
 *	@dev: network layer device to open
 *
 *	Called when the network layer brings the interface up. Returns
 *	a negative posix error code on failure, or zero on success.
 *
 *	All the ring allocation and set up is done on open for this
 *	adapter to minimise memory usage when inactive
 */
static int velocity_open(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	int ret;

	ret = velocity_init_rings(vptr, dev->mtu);
	if (ret < 0)
		goto out;

	/* Ensure chip is running */
	pci_set_power_state(vptr->pdev, PCI_D0);

	velocity_init_registers(vptr, VELOCITY_INIT_COLD);

	ret = request_irq(vptr->pdev->irq, velocity_intr, IRQF_SHARED,
			  dev->name, dev);
	if (ret < 0) {
		/* Power down the chip */
		pci_set_power_state(vptr->pdev, PCI_D3hot);
		velocity_free_rings(vptr);
		goto out;
	}

	velocity_give_many_rx_descs(vptr);

	mac_enable_int(vptr->mac_regs);
	netif_start_queue(dev);
	napi_enable(&vptr->napi);
	vptr->flags |= VELOCITY_FLAGS_OPENED;
out:
	return ret;
}

/**
 *	velocity_shutdown	-	shut down the chip
 *	@vptr: velocity to deactivate
 *
 *	Shuts down the internal operations of the velocity and
 *	disables interrupts, autopolling, transmit and receive
 */
static void velocity_shutdown(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	mac_disable_int(regs);
	writel(CR0_STOP, &regs->CR0Set);
	writew(0xFFFF, &regs->TDCSRClr);
	writeb(0xFF, &regs->RDCSRClr);
	safe_disable_mii_autopoll(regs);
	mac_clear_isr(regs);
}

/**
 *	velocity_change_mtu	-	MTU change callback
 *	@dev: network device
 *	@new_mtu: desired MTU
 *
 *	Handle requests from the networking layer for MTU change on
 *	this interface. It gets called on a change by the network layer.
 *	Return zero for success or negative posix error code.
 */
static int velocity_change_mtu(struct net_device *dev, int new_mtu)
{
	struct velocity_info *vptr = netdev_priv(dev);
	int ret = 0;

	if ((new_mtu < VELOCITY_MIN_MTU) || new_mtu > (VELOCITY_MAX_MTU)) {
		VELOCITY_PRT(MSG_LEVEL_ERR, KERN_NOTICE "%s: Invalid MTU.\n",
				vptr->dev->name);
		ret = -EINVAL;
		goto out_0;
	}

	if (!netif_running(dev)) {
		dev->mtu = new_mtu;
		goto out_0;
	}

	if (dev->mtu != new_mtu) {
		struct velocity_info *tmp_vptr;
		unsigned long flags;
		struct rx_info rx;
		struct tx_info tx;

		tmp_vptr = kzalloc(sizeof(*tmp_vptr), GFP_KERNEL);
		if (!tmp_vptr) {
			ret = -ENOMEM;
			goto out_0;
		}

		tmp_vptr->dev = dev;
		tmp_vptr->pdev = vptr->pdev;
		tmp_vptr->options = vptr->options;
		tmp_vptr->tx.numq = vptr->tx.numq;

		ret = velocity_init_rings(tmp_vptr, new_mtu);
		if (ret < 0)
			goto out_free_tmp_vptr_1;

		spin_lock_irqsave(&vptr->lock, flags);

		netif_stop_queue(dev);
		velocity_shutdown(vptr);

		rx = vptr->rx;
		tx = vptr->tx;

		vptr->rx = tmp_vptr->rx;
		vptr->tx = tmp_vptr->tx;

		tmp_vptr->rx = rx;
		tmp_vptr->tx = tx;

		dev->mtu = new_mtu;

		velocity_init_registers(vptr, VELOCITY_INIT_COLD);

		velocity_give_many_rx_descs(vptr);

		mac_enable_int(vptr->mac_regs);
		netif_start_queue(dev);

		spin_unlock_irqrestore(&vptr->lock, flags);

		velocity_free_rings(tmp_vptr);

out_free_tmp_vptr_1:
		kfree(tmp_vptr);
	}
out_0:
	return ret;
}

/**
 *	velocity_mii_ioctl		-	MII ioctl handler
 *	@dev: network device
 *	@ifr: the ifreq block for the ioctl
 *	@cmd: the command
 *
 *	Process MII requests made via ioctl from the network layer. These
 *	are used by tools like kudzu to interrogate the link state of the
 *	hardware
 */
static int velocity_mii_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct velocity_info *vptr = netdev_priv(dev);
	struct mac_regs __iomem *regs = vptr->mac_regs;
	unsigned long flags;
	struct mii_ioctl_data *miidata = if_mii(ifr);
	int err;

	switch (cmd) {
	case SIOCGMIIPHY:
		miidata->phy_id = readb(&regs->MIIADR) & 0x1f;
		break;
	case SIOCGMIIREG:
		if (velocity_mii_read(vptr->mac_regs, miidata->reg_num & 0x1f, &(miidata->val_out)) < 0)
			return -ETIMEDOUT;
		break;
	case SIOCSMIIREG:
		spin_lock_irqsave(&vptr->lock, flags);
		err = velocity_mii_write(vptr->mac_regs, miidata->reg_num & 0x1f, miidata->val_in);
		spin_unlock_irqrestore(&vptr->lock, flags);
		check_connection_type(vptr->mac_regs);
		if (err)
			return err;
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}


/**
 *	velocity_ioctl		-	ioctl entry point
 *	@dev: network device
 *	@rq: interface request ioctl
 *	@cmd: command code
 *
 *	Called when the user issues an ioctl request to the network
 *	device in question. The velocity interface supports MII.
 */
static int velocity_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct velocity_info *vptr = netdev_priv(dev);
	int ret;

	/* If we are asked for information and the device is power
	   saving then we need to bring the device back up to talk to it */

	if (!netif_running(dev))
		pci_set_power_state(vptr->pdev, PCI_D0);

	switch (cmd) {
	case SIOCGMIIPHY:	/* Get address of MII PHY in use. */
	case SIOCGMIIREG:	/* Read MII PHY register. */
	case SIOCSMIIREG:	/* Write to MII PHY register. */
		ret = velocity_mii_ioctl(dev, rq, cmd);
		break;

	default:
		ret = -EOPNOTSUPP;
	}
	if (!netif_running(dev))
		pci_set_power_state(vptr->pdev, PCI_D3hot);


	return ret;
}

/**
 *	velocity_get_status	-	statistics callback
 *	@dev: network device
 *
 *	Callback from the network layer to allow driver statistics
 *	to be resynchronized with hardware collected state. In the
 *	case of the velocity we need to pull the MIB counters from
 *	the hardware into the counters before letting the network
 *	layer display them.
 */
static struct net_device_stats *velocity_get_stats(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);

	/* If the hardware is down, don't touch MII */
	if (!netif_running(dev))
		return &dev->stats;

	spin_lock_irq(&vptr->lock);
	velocity_update_hw_mibs(vptr);
	spin_unlock_irq(&vptr->lock);

	dev->stats.rx_packets = vptr->mib_counter[HW_MIB_ifRxAllPkts];
	dev->stats.rx_errors = vptr->mib_counter[HW_MIB_ifRxErrorPkts];
	dev->stats.rx_length_errors = vptr->mib_counter[HW_MIB_ifInRangeLengthErrors];

//  unsigned long   rx_dropped;     /* no space in linux buffers    */
	dev->stats.collisions = vptr->mib_counter[HW_MIB_ifTxEtherCollisions];
	/* detailed rx_errors: */
//  unsigned long   rx_length_errors;
//  unsigned long   rx_over_errors;     /* receiver ring buff overflow  */
	dev->stats.rx_crc_errors = vptr->mib_counter[HW_MIB_ifRxPktCRCE];
//  unsigned long   rx_frame_errors;    /* recv'd frame alignment error */
//  unsigned long   rx_fifo_errors;     /* recv'r fifo overrun      */
//  unsigned long   rx_missed_errors;   /* receiver missed packet   */

	/* detailed tx_errors */
//  unsigned long   tx_fifo_errors;

	return &dev->stats;
}

/**
 *	velocity_close		-	close adapter callback
 *	@dev: network device
 *
 *	Callback from the network layer when the velocity is being
 *	deactivated by the network layer
 */
static int velocity_close(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);

	napi_disable(&vptr->napi);
	netif_stop_queue(dev);
	velocity_shutdown(vptr);

	if (vptr->flags & VELOCITY_FLAGS_WOL_ENABLED)
		velocity_get_ip(vptr);
	if (dev->irq != 0)
		free_irq(dev->irq, dev);

	/* Power down the chip */
	pci_set_power_state(vptr->pdev, PCI_D3hot);

	velocity_free_rings(vptr);

	vptr->flags &= (~VELOCITY_FLAGS_OPENED);
	return 0;
}

/**
 *	velocity_xmit		-	transmit packet callback
 *	@skb: buffer to transmit
 *	@dev: network device
 *
 *	Called by the networ layer to request a packet is queued to
 *	the velocity. Returns zero on success.
 */
static netdev_tx_t velocity_xmit(struct sk_buff *skb,
				 struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	int qnum = 0;
	struct tx_desc *td_ptr;
	struct velocity_td_info *tdinfo;
	unsigned long flags;
	int pktlen;
	int index, prev;
	int i = 0;

	if (skb_padto(skb, ETH_ZLEN))
		goto out;

	/* The hardware can handle at most 7 memory segments, so merge
	 * the skb if there are more */
	if (skb_shinfo(skb)->nr_frags > 6 && __skb_linearize(skb)) {
		kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	pktlen = skb_shinfo(skb)->nr_frags == 0 ?
			max_t(unsigned int, skb->len, ETH_ZLEN) :
				skb_headlen(skb);

	spin_lock_irqsave(&vptr->lock, flags);

	index = vptr->tx.curr[qnum];
	td_ptr = &(vptr->tx.rings[qnum][index]);
	tdinfo = &(vptr->tx.infos[qnum][index]);

	td_ptr->tdesc1.TCR = TCR0_TIC;
	td_ptr->td_buf[0].size &= ~TD_QUEUE;

	/*
	 *	Map the linear network buffer into PCI space and
	 *	add it to the transmit ring.
	 */
	tdinfo->skb = skb;
	tdinfo->skb_dma[0] = pci_map_single(vptr->pdev, skb->data, pktlen, PCI_DMA_TODEVICE);
	td_ptr->tdesc0.len = cpu_to_le16(pktlen);
	td_ptr->td_buf[0].pa_low = cpu_to_le32(tdinfo->skb_dma[0]);
	td_ptr->td_buf[0].pa_high = 0;
	td_ptr->td_buf[0].size = cpu_to_le16(pktlen);

	/* Handle fragments */
	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		tdinfo->skb_dma[i + 1] = pci_map_page(vptr->pdev, frag->page,
				frag->page_offset, frag->size,
				PCI_DMA_TODEVICE);

		td_ptr->td_buf[i + 1].pa_low = cpu_to_le32(tdinfo->skb_dma[i + 1]);
		td_ptr->td_buf[i + 1].pa_high = 0;
		td_ptr->td_buf[i + 1].size = cpu_to_le16(frag->size);
	}
	tdinfo->nskb_dma = i + 1;

	td_ptr->tdesc1.cmd = TCPLS_NORMAL + (tdinfo->nskb_dma + 1) * 16;

	if (vlan_tx_tag_present(skb)) {
		td_ptr->tdesc1.vlan = cpu_to_le16(vlan_tx_tag_get(skb));
		td_ptr->tdesc1.TCR |= TCR0_VETAG;
	}

	/*
	 *	Handle hardware checksum
	 */
	if ((dev->features & NETIF_F_IP_CSUM) &&
	    (skb->ip_summed == CHECKSUM_PARTIAL)) {
		const struct iphdr *ip = ip_hdr(skb);
		if (ip->protocol == IPPROTO_TCP)
			td_ptr->tdesc1.TCR |= TCR0_TCPCK;
		else if (ip->protocol == IPPROTO_UDP)
			td_ptr->tdesc1.TCR |= (TCR0_UDPCK);
		td_ptr->tdesc1.TCR |= TCR0_IPCK;
	}

	prev = index - 1;
	if (prev < 0)
		prev = vptr->options.numtx - 1;
	td_ptr->tdesc0.len |= OWNED_BY_NIC;
	vptr->tx.used[qnum]++;
	vptr->tx.curr[qnum] = (index + 1) % vptr->options.numtx;

	if (AVAIL_TD(vptr, qnum) < 1)
		netif_stop_queue(dev);

	td_ptr = &(vptr->tx.rings[qnum][prev]);
	td_ptr->td_buf[0].size |= TD_QUEUE;
	mac_tx_queue_wake(vptr->mac_regs, qnum);

	spin_unlock_irqrestore(&vptr->lock, flags);
out:
	return NETDEV_TX_OK;
}


static const struct net_device_ops velocity_netdev_ops = {
	.ndo_open		= velocity_open,
	.ndo_stop		= velocity_close,
	.ndo_start_xmit		= velocity_xmit,
	.ndo_get_stats		= velocity_get_stats,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_set_multicast_list	= velocity_set_multi,
	.ndo_change_mtu		= velocity_change_mtu,
	.ndo_do_ioctl		= velocity_ioctl,
	.ndo_vlan_rx_add_vid	= velocity_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= velocity_vlan_rx_kill_vid,
	.ndo_vlan_rx_register	= velocity_vlan_rx_register,
};

/**
 *	velocity_init_info	-	init private data
 *	@pdev: PCI device
 *	@vptr: Velocity info
 *	@info: Board type
 *
 *	Set up the initial velocity_info struct for the device that has been
 *	discovered.
 */
static void __devinit velocity_init_info(struct pci_dev *pdev,
					 struct velocity_info *vptr,
					 const struct velocity_info_tbl *info)
{
	memset(vptr, 0, sizeof(struct velocity_info));

	vptr->pdev = pdev;
	vptr->chip_id = info->chip_id;
	vptr->tx.numq = info->txqueue;
	vptr->multicast_limit = MCAM_SIZE;
	spin_lock_init(&vptr->lock);
}

/**
 *	velocity_get_pci_info	-	retrieve PCI info for device
 *	@vptr: velocity device
 *	@pdev: PCI device it matches
 *
 *	Retrieve the PCI configuration space data that interests us from
 *	the kernel PCI layer
 */
static int __devinit velocity_get_pci_info(struct velocity_info *vptr, struct pci_dev *pdev)
{
	vptr->rev_id = pdev->revision;

	pci_set_master(pdev);

	vptr->ioaddr = pci_resource_start(pdev, 0);
	vptr->memaddr = pci_resource_start(pdev, 1);

	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_IO)) {
		dev_err(&pdev->dev,
			   "region #0 is not an I/O resource, aborting.\n");
		return -EINVAL;
	}

	if ((pci_resource_flags(pdev, 1) & IORESOURCE_IO)) {
		dev_err(&pdev->dev,
			   "region #1 is an I/O resource, aborting.\n");
		return -EINVAL;
	}

	if (pci_resource_len(pdev, 1) < VELOCITY_IO_SIZE) {
		dev_err(&pdev->dev, "region #1 is too small.\n");
		return -EINVAL;
	}
	vptr->pdev = pdev;

	return 0;
}

/**
 *	velocity_print_info	-	per driver data
 *	@vptr: velocity
 *
 *	Print per driver data as the kernel driver finds Velocity
 *	hardware
 */
static void __devinit velocity_print_info(struct velocity_info *vptr)
{
	struct net_device *dev = vptr->dev;

	printk(KERN_INFO "%s: %s\n", dev->name, get_chip_name(vptr->chip_id));
	printk(KERN_INFO "%s: Ethernet Address: %pM\n",
		dev->name, dev->dev_addr);
}

static u32 velocity_get_link(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	struct mac_regs __iomem *regs = vptr->mac_regs;
	return BYTE_REG_BITS_IS_ON(PHYSR0_LINKGD, &regs->PHYSR0) ? 1 : 0;
}


/**
 *	velocity_found1		-	set up discovered velocity card
 *	@pdev: PCI device
 *	@ent: PCI device table entry that matched
 *
 *	Configure a discovered adapter from scratch. Return a negative
 *	errno error code on failure paths.
 */
static int __devinit velocity_found1(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	static int first = 1;
	struct net_device *dev;
	int i;
	const char *drv_string;
	const struct velocity_info_tbl *info = &chip_info_table[ent->driver_data];
	struct velocity_info *vptr;
	struct mac_regs __iomem *regs;
	int ret = -ENOMEM;

	/* FIXME: this driver, like almost all other ethernet drivers,
	 * can support more than MAX_UNITS.
	 */
	if (velocity_nics >= MAX_UNITS) {
		dev_notice(&pdev->dev, "already found %d NICs.\n",
			   velocity_nics);
		return -ENODEV;
	}

	dev = alloc_etherdev(sizeof(struct velocity_info));
	if (!dev) {
		dev_err(&pdev->dev, "allocate net device failed.\n");
		goto out;
	}

	/* Chain it all together */

	SET_NETDEV_DEV(dev, &pdev->dev);
	vptr = netdev_priv(dev);


	if (first) {
		printk(KERN_INFO "%s Ver. %s\n",
			VELOCITY_FULL_DRV_NAM, VELOCITY_VERSION);
		printk(KERN_INFO "Copyright (c) 2002, 2003 VIA Networking Technologies, Inc.\n");
		printk(KERN_INFO "Copyright (c) 2004 Red Hat Inc.\n");
		first = 0;
	}

	velocity_init_info(pdev, vptr, info);

	vptr->dev = dev;

	ret = pci_enable_device(pdev);
	if (ret < 0)
		goto err_free_dev;

	dev->irq = pdev->irq;

	ret = velocity_get_pci_info(vptr, pdev);
	if (ret < 0) {
		/* error message already printed */
		goto err_disable;
	}

	ret = pci_request_regions(pdev, VELOCITY_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "No PCI resources.\n");
		goto err_disable;
	}

	regs = ioremap(vptr->memaddr, VELOCITY_IO_SIZE);
	if (regs == NULL) {
		ret = -EIO;
		goto err_release_res;
	}

	vptr->mac_regs = regs;

	mac_wol_reset(regs);

	dev->base_addr = vptr->ioaddr;

	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = readb(&regs->PAR[i]);


	drv_string = dev_driver_string(&pdev->dev);

	velocity_get_options(&vptr->options, velocity_nics, drv_string);

	/*
	 *	Mask out the options cannot be set to the chip
	 */

	vptr->options.flags &= info->flags;

	/*
	 *	Enable the chip specified capbilities
	 */

	vptr->flags = vptr->options.flags | (info->flags & 0xFF000000UL);

	vptr->wol_opts = vptr->options.wol_opts;
	vptr->flags |= VELOCITY_FLAGS_WOL_ENABLED;

	vptr->phy_id = MII_GET_PHY_ID(vptr->mac_regs);

	dev->irq = pdev->irq;
	dev->netdev_ops = &velocity_netdev_ops;
	dev->ethtool_ops = &velocity_ethtool_ops;
	netif_napi_add(dev, &vptr->napi, velocity_poll, VELOCITY_NAPI_WEIGHT);

	dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_FILTER |
		NETIF_F_HW_VLAN_RX | NETIF_F_IP_CSUM;

	ret = register_netdev(dev);
	if (ret < 0)
		goto err_iounmap;

	if (!velocity_get_link(dev)) {
		netif_carrier_off(dev);
		vptr->mii_status |= VELOCITY_LINK_FAIL;
	}

	velocity_print_info(vptr);
	pci_set_drvdata(pdev, dev);

	/* and leave the chip powered down */

	pci_set_power_state(pdev, PCI_D3hot);
	velocity_nics++;
out:
	return ret;

err_iounmap:
	iounmap(regs);
err_release_res:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
err_free_dev:
	free_netdev(dev);
	goto out;
}


#ifdef CONFIG_PM
/**
 *	wol_calc_crc		-	WOL CRC
 *	@pattern: data pattern
 *	@mask_pattern: mask
 *
 *	Compute the wake on lan crc hashes for the packet header
 *	we are interested in.
 */
static u16 wol_calc_crc(int size, u8 *pattern, u8 *mask_pattern)
{
	u16 crc = 0xFFFF;
	u8 mask;
	int i, j;

	for (i = 0; i < size; i++) {
		mask = mask_pattern[i];

		/* Skip this loop if the mask equals to zero */
		if (mask == 0x00)
			continue;

		for (j = 0; j < 8; j++) {
			if ((mask & 0x01) == 0) {
				mask >>= 1;
				continue;
			}
			mask >>= 1;
			crc = crc_ccitt(crc, &(pattern[i * 8 + j]), 1);
		}
	}
	/*	Finally, invert the result once to get the correct data */
	crc = ~crc;
	return bitrev32(crc) >> 16;
}

/**
 *	velocity_set_wol	-	set up for wake on lan
 *	@vptr: velocity to set WOL status on
 *
 *	Set a card up for wake on lan either by unicast or by
 *	ARP packet.
 *
 *	FIXME: check static buffer is safe here
 */
static int velocity_set_wol(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	enum speed_opt spd_dpx = vptr->options.spd_dpx;
	static u8 buf[256];
	int i;

	static u32 mask_pattern[2][4] = {
		{0x00203000, 0x000003C0, 0x00000000, 0x0000000}, /* ARP */
		{0xfffff000, 0xffffffff, 0xffffffff, 0x000ffff}	 /* Magic Packet */
	};

	writew(0xFFFF, &regs->WOLCRClr);
	writeb(WOLCFG_SAB | WOLCFG_SAM, &regs->WOLCFGSet);
	writew(WOLCR_MAGIC_EN, &regs->WOLCRSet);

	/*
	   if (vptr->wol_opts & VELOCITY_WOL_PHY)
	   writew((WOLCR_LINKON_EN|WOLCR_LINKOFF_EN), &regs->WOLCRSet);
	 */

	if (vptr->wol_opts & VELOCITY_WOL_UCAST)
		writew(WOLCR_UNICAST_EN, &regs->WOLCRSet);

	if (vptr->wol_opts & VELOCITY_WOL_ARP) {
		struct arp_packet *arp = (struct arp_packet *) buf;
		u16 crc;
		memset(buf, 0, sizeof(struct arp_packet) + 7);

		for (i = 0; i < 4; i++)
			writel(mask_pattern[0][i], &regs->ByteMask[0][i]);

		arp->type = htons(ETH_P_ARP);
		arp->ar_op = htons(1);

		memcpy(arp->ar_tip, vptr->ip_addr, 4);

		crc = wol_calc_crc((sizeof(struct arp_packet) + 7) / 8, buf,
				(u8 *) & mask_pattern[0][0]);

		writew(crc, &regs->PatternCRC[0]);
		writew(WOLCR_ARP_EN, &regs->WOLCRSet);
	}

	BYTE_REG_BITS_ON(PWCFG_WOLTYPE, &regs->PWCFGSet);
	BYTE_REG_BITS_ON(PWCFG_LEGACY_WOLEN, &regs->PWCFGSet);

	writew(0x0FFF, &regs->WOLSRClr);

	if (spd_dpx == SPD_DPX_1000_FULL)
		goto mac_done;

	if (spd_dpx != SPD_DPX_AUTO)
		goto advertise_done;

	if (vptr->mii_status & VELOCITY_AUTONEG_ENABLE) {
		if (PHYID_GET_PHY_ID(vptr->phy_id) == PHYID_CICADA_CS8201)
			MII_REG_BITS_ON(AUXCR_MDPPS, MII_NCONFIG, vptr->mac_regs);

		MII_REG_BITS_OFF(ADVERTISE_1000FULL | ADVERTISE_1000HALF, MII_CTRL1000, vptr->mac_regs);
	}

	if (vptr->mii_status & VELOCITY_SPEED_1000)
		MII_REG_BITS_ON(BMCR_ANRESTART, MII_BMCR, vptr->mac_regs);

advertise_done:
	BYTE_REG_BITS_ON(CHIPGCR_FCMODE, &regs->CHIPGCR);

	{
		u8 GCR;
		GCR = readb(&regs->CHIPGCR);
		GCR = (GCR & ~CHIPGCR_FCGMII) | CHIPGCR_FCFDX;
		writeb(GCR, &regs->CHIPGCR);
	}

mac_done:
	BYTE_REG_BITS_OFF(ISR_PWEI, &regs->ISR);
	/* Turn on SWPTAG just before entering power mode */
	BYTE_REG_BITS_ON(STICKHW_SWPTAG, &regs->STICKHW);
	/* Go to bed ..... */
	BYTE_REG_BITS_ON((STICKHW_DS1 | STICKHW_DS0), &regs->STICKHW);

	return 0;
}

/**
 *	velocity_save_context	-	save registers
 *	@vptr: velocity
 *	@context: buffer for stored context
 *
 *	Retrieve the current configuration from the velocity hardware
 *	and stash it in the context structure, for use by the context
 *	restore functions. This allows us to save things we need across
 *	power down states
 */
static void velocity_save_context(struct velocity_info *vptr, struct velocity_context *context)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	u16 i;
	u8 __iomem *ptr = (u8 __iomem *)regs;

	for (i = MAC_REG_PAR; i < MAC_REG_CR0_CLR; i += 4)
		*((u32 *) (context->mac_reg + i)) = readl(ptr + i);

	for (i = MAC_REG_MAR; i < MAC_REG_TDCSR_CLR; i += 4)
		*((u32 *) (context->mac_reg + i)) = readl(ptr + i);

	for (i = MAC_REG_RDBASE_LO; i < MAC_REG_FIFO_TEST0; i += 4)
		*((u32 *) (context->mac_reg + i)) = readl(ptr + i);

}

static int velocity_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct velocity_info *vptr = netdev_priv(dev);
	unsigned long flags;

	if (!netif_running(vptr->dev))
		return 0;

	netif_device_detach(vptr->dev);

	spin_lock_irqsave(&vptr->lock, flags);
	pci_save_state(pdev);
#ifdef ETHTOOL_GWOL
	if (vptr->flags & VELOCITY_FLAGS_WOL_ENABLED) {
		velocity_get_ip(vptr);
		velocity_save_context(vptr, &vptr->context);
		velocity_shutdown(vptr);
		velocity_set_wol(vptr);
		pci_enable_wake(pdev, PCI_D3hot, 1);
		pci_set_power_state(pdev, PCI_D3hot);
	} else {
		velocity_save_context(vptr, &vptr->context);
		velocity_shutdown(vptr);
		pci_disable_device(pdev);
		pci_set_power_state(pdev, pci_choose_state(pdev, state));
	}
#else
	pci_set_power_state(pdev, pci_choose_state(pdev, state));
#endif
	spin_unlock_irqrestore(&vptr->lock, flags);
	return 0;
}

/**
 *	velocity_restore_context	-	restore registers
 *	@vptr: velocity
 *	@context: buffer for stored context
 *
 *	Reload the register configuration from the velocity context
 *	created by velocity_save_context.
 */
static void velocity_restore_context(struct velocity_info *vptr, struct velocity_context *context)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	int i;
	u8 __iomem *ptr = (u8 __iomem *)regs;

	for (i = MAC_REG_PAR; i < MAC_REG_CR0_SET; i += 4)
		writel(*((u32 *) (context->mac_reg + i)), ptr + i);

	/* Just skip cr0 */
	for (i = MAC_REG_CR1_SET; i < MAC_REG_CR0_CLR; i++) {
		/* Clear */
		writeb(~(*((u8 *) (context->mac_reg + i))), ptr + i + 4);
		/* Set */
		writeb(*((u8 *) (context->mac_reg + i)), ptr + i);
	}

	for (i = MAC_REG_MAR; i < MAC_REG_IMR; i += 4)
		writel(*((u32 *) (context->mac_reg + i)), ptr + i);

	for (i = MAC_REG_RDBASE_LO; i < MAC_REG_FIFO_TEST0; i += 4)
		writel(*((u32 *) (context->mac_reg + i)), ptr + i);

	for (i = MAC_REG_TDCSR_SET; i <= MAC_REG_RDCSR_SET; i++)
		writeb(*((u8 *) (context->mac_reg + i)), ptr + i);
}

static int velocity_resume(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct velocity_info *vptr = netdev_priv(dev);
	unsigned long flags;
	int i;

	if (!netif_running(vptr->dev))
		return 0;

	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, 0, 0);
	pci_restore_state(pdev);

	mac_wol_reset(vptr->mac_regs);

	spin_lock_irqsave(&vptr->lock, flags);
	velocity_restore_context(vptr, &vptr->context);
	velocity_init_registers(vptr, VELOCITY_INIT_WOL);
	mac_disable_int(vptr->mac_regs);

	velocity_tx_srv(vptr);

	for (i = 0; i < vptr->tx.numq; i++) {
		if (vptr->tx.used[i])
			mac_tx_queue_wake(vptr->mac_regs, i);
	}

	mac_enable_int(vptr->mac_regs);
	spin_unlock_irqrestore(&vptr->lock, flags);
	netif_device_attach(vptr->dev);

	return 0;
}
#endif

/*
 *	Definition for our device driver. The PCI layer interface
 *	uses this to handle all our card discover and plugging
 */
static struct pci_driver velocity_driver = {
      .name	= VELOCITY_NAME,
      .id_table	= velocity_id_table,
      .probe	= velocity_found1,
      .remove	= __devexit_p(velocity_remove1),
#ifdef CONFIG_PM
      .suspend	= velocity_suspend,
      .resume	= velocity_resume,
#endif
};


/**
 *	velocity_ethtool_up	-	pre hook for ethtool
 *	@dev: network device
 *
 *	Called before an ethtool operation. We need to make sure the
 *	chip is out of D3 state before we poke at it.
 */
static int velocity_ethtool_up(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	if (!netif_running(dev))
		pci_set_power_state(vptr->pdev, PCI_D0);
	return 0;
}

/**
 *	velocity_ethtool_down	-	post hook for ethtool
 *	@dev: network device
 *
 *	Called after an ethtool operation. Restore the chip back to D3
 *	state if it isn't running.
 */
static void velocity_ethtool_down(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	if (!netif_running(dev))
		pci_set_power_state(vptr->pdev, PCI_D3hot);
}

static int velocity_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct velocity_info *vptr = netdev_priv(dev);
	struct mac_regs __iomem *regs = vptr->mac_regs;
	u32 status;
	status = check_connection_type(vptr->mac_regs);

	cmd->supported = SUPPORTED_TP |
			SUPPORTED_Autoneg |
			SUPPORTED_10baseT_Half |
			SUPPORTED_10baseT_Full |
			SUPPORTED_100baseT_Half |
			SUPPORTED_100baseT_Full |
			SUPPORTED_1000baseT_Half |
			SUPPORTED_1000baseT_Full;

	cmd->advertising = ADVERTISED_TP | ADVERTISED_Autoneg;
	if (vptr->options.spd_dpx == SPD_DPX_AUTO) {
		cmd->advertising |=
			ADVERTISED_10baseT_Half |
			ADVERTISED_10baseT_Full |
			ADVERTISED_100baseT_Half |
			ADVERTISED_100baseT_Full |
			ADVERTISED_1000baseT_Half |
			ADVERTISED_1000baseT_Full;
	} else {
		switch (vptr->options.spd_dpx) {
		case SPD_DPX_1000_FULL:
			cmd->advertising |= ADVERTISED_1000baseT_Full;
			break;
		case SPD_DPX_100_HALF:
			cmd->advertising |= ADVERTISED_100baseT_Half;
			break;
		case SPD_DPX_100_FULL:
			cmd->advertising |= ADVERTISED_100baseT_Full;
			break;
		case SPD_DPX_10_HALF:
			cmd->advertising |= ADVERTISED_10baseT_Half;
			break;
		case SPD_DPX_10_FULL:
			cmd->advertising |= ADVERTISED_10baseT_Full;
			break;
		default:
			break;
		}
	}
	if (status & VELOCITY_SPEED_1000)
		cmd->speed = SPEED_1000;
	else if (status & VELOCITY_SPEED_100)
		cmd->speed = SPEED_100;
	else
		cmd->speed = SPEED_10;
	cmd->autoneg = (status & VELOCITY_AUTONEG_ENABLE) ? AUTONEG_ENABLE : AUTONEG_DISABLE;
	cmd->port = PORT_TP;
	cmd->transceiver = XCVR_INTERNAL;
	cmd->phy_address = readb(&regs->MIIADR) & 0x1F;

	if (status & VELOCITY_DUPLEX_FULL)
		cmd->duplex = DUPLEX_FULL;
	else
		cmd->duplex = DUPLEX_HALF;

	return 0;
}

static int velocity_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct velocity_info *vptr = netdev_priv(dev);
	u32 curr_status;
	u32 new_status = 0;
	int ret = 0;

	curr_status = check_connection_type(vptr->mac_regs);
	curr_status &= (~VELOCITY_LINK_FAIL);

	new_status |= ((cmd->autoneg) ? VELOCITY_AUTONEG_ENABLE : 0);
	new_status |= ((cmd->speed == SPEED_1000) ? VELOCITY_SPEED_1000 : 0);
	new_status |= ((cmd->speed == SPEED_100) ? VELOCITY_SPEED_100 : 0);
	new_status |= ((cmd->speed == SPEED_10) ? VELOCITY_SPEED_10 : 0);
	new_status |= ((cmd->duplex == DUPLEX_FULL) ? VELOCITY_DUPLEX_FULL : 0);

	if ((new_status & VELOCITY_AUTONEG_ENABLE) &&
	    (new_status != (curr_status | VELOCITY_AUTONEG_ENABLE))) {
		ret = -EINVAL;
	} else {
		enum speed_opt spd_dpx;

		if (new_status & VELOCITY_AUTONEG_ENABLE)
			spd_dpx = SPD_DPX_AUTO;
		else if ((new_status & VELOCITY_SPEED_1000) &&
			 (new_status & VELOCITY_DUPLEX_FULL)) {
			spd_dpx = SPD_DPX_1000_FULL;
		} else if (new_status & VELOCITY_SPEED_100)
			spd_dpx = (new_status & VELOCITY_DUPLEX_FULL) ?
				SPD_DPX_100_FULL : SPD_DPX_100_HALF;
		else if (new_status & VELOCITY_SPEED_10)
			spd_dpx = (new_status & VELOCITY_DUPLEX_FULL) ?
				SPD_DPX_10_FULL : SPD_DPX_10_HALF;
		else
			return -EOPNOTSUPP;

		vptr->options.spd_dpx = spd_dpx;

		velocity_set_media_mode(vptr, new_status);
	}

	return ret;
}

static void velocity_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct velocity_info *vptr = netdev_priv(dev);
	strcpy(info->driver, VELOCITY_NAME);
	strcpy(info->version, VELOCITY_VERSION);
	strcpy(info->bus_info, pci_name(vptr->pdev));
}

static void velocity_ethtool_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct velocity_info *vptr = netdev_priv(dev);
	wol->supported = WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_ARP;
	wol->wolopts |= WAKE_MAGIC;
	/*
	   if (vptr->wol_opts & VELOCITY_WOL_PHY)
		   wol.wolopts|=WAKE_PHY;
			 */
	if (vptr->wol_opts & VELOCITY_WOL_UCAST)
		wol->wolopts |= WAKE_UCAST;
	if (vptr->wol_opts & VELOCITY_WOL_ARP)
		wol->wolopts |= WAKE_ARP;
	memcpy(&wol->sopass, vptr->wol_passwd, 6);
}

static int velocity_ethtool_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct velocity_info *vptr = netdev_priv(dev);

	if (!(wol->wolopts & (WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_ARP)))
		return -EFAULT;
	vptr->wol_opts = VELOCITY_WOL_MAGIC;

	/*
	   if (wol.wolopts & WAKE_PHY) {
	   vptr->wol_opts|=VELOCITY_WOL_PHY;
	   vptr->flags |=VELOCITY_FLAGS_WOL_ENABLED;
	   }
	 */

	if (wol->wolopts & WAKE_MAGIC) {
		vptr->wol_opts |= VELOCITY_WOL_MAGIC;
		vptr->flags |= VELOCITY_FLAGS_WOL_ENABLED;
	}
	if (wol->wolopts & WAKE_UCAST) {
		vptr->wol_opts |= VELOCITY_WOL_UCAST;
		vptr->flags |= VELOCITY_FLAGS_WOL_ENABLED;
	}
	if (wol->wolopts & WAKE_ARP) {
		vptr->wol_opts |= VELOCITY_WOL_ARP;
		vptr->flags |= VELOCITY_FLAGS_WOL_ENABLED;
	}
	memcpy(vptr->wol_passwd, wol->sopass, 6);
	return 0;
}

static u32 velocity_get_msglevel(struct net_device *dev)
{
	return msglevel;
}

static void velocity_set_msglevel(struct net_device *dev, u32 value)
{
	 msglevel = value;
}

static int get_pending_timer_val(int val)
{
	int mult_bits = val >> 6;
	int mult = 1;

	switch (mult_bits)
	{
	case 1:
		mult = 4; break;
	case 2:
		mult = 16; break;
	case 3:
		mult = 64; break;
	case 0:
	default:
		break;
	}

	return (val & 0x3f) * mult;
}

static void set_pending_timer_val(int *val, u32 us)
{
	u8 mult = 0;
	u8 shift = 0;

	if (us >= 0x3f) {
		mult = 1; /* mult with 4 */
		shift = 2;
	}
	if (us >= 0x3f * 4) {
		mult = 2; /* mult with 16 */
		shift = 4;
	}
	if (us >= 0x3f * 16) {
		mult = 3; /* mult with 64 */
		shift = 6;
	}

	*val = (mult << 6) | ((us >> shift) & 0x3f);
}


static int velocity_get_coalesce(struct net_device *dev,
		struct ethtool_coalesce *ecmd)
{
	struct velocity_info *vptr = netdev_priv(dev);

	ecmd->tx_max_coalesced_frames = vptr->options.tx_intsup;
	ecmd->rx_max_coalesced_frames = vptr->options.rx_intsup;

	ecmd->rx_coalesce_usecs = get_pending_timer_val(vptr->options.rxqueue_timer);
	ecmd->tx_coalesce_usecs = get_pending_timer_val(vptr->options.txqueue_timer);

	return 0;
}

static int velocity_set_coalesce(struct net_device *dev,
		struct ethtool_coalesce *ecmd)
{
	struct velocity_info *vptr = netdev_priv(dev);
	int max_us = 0x3f * 64;
	unsigned long flags;

	/* 6 bits of  */
	if (ecmd->tx_coalesce_usecs > max_us)
		return -EINVAL;
	if (ecmd->rx_coalesce_usecs > max_us)
		return -EINVAL;

	if (ecmd->tx_max_coalesced_frames > 0xff)
		return -EINVAL;
	if (ecmd->rx_max_coalesced_frames > 0xff)
		return -EINVAL;

	vptr->options.rx_intsup = ecmd->rx_max_coalesced_frames;
	vptr->options.tx_intsup = ecmd->tx_max_coalesced_frames;

	set_pending_timer_val(&vptr->options.rxqueue_timer,
			ecmd->rx_coalesce_usecs);
	set_pending_timer_val(&vptr->options.txqueue_timer,
			ecmd->tx_coalesce_usecs);

	/* Setup the interrupt suppression and queue timers */
	spin_lock_irqsave(&vptr->lock, flags);
	mac_disable_int(vptr->mac_regs);
	setup_adaptive_interrupts(vptr);
	setup_queue_timers(vptr);

	mac_write_int_mask(vptr->int_mask, vptr->mac_regs);
	mac_clear_isr(vptr->mac_regs);
	mac_enable_int(vptr->mac_regs);
	spin_unlock_irqrestore(&vptr->lock, flags);

	return 0;
}

static const struct ethtool_ops velocity_ethtool_ops = {
	.get_settings	=	velocity_get_settings,
	.set_settings	=	velocity_set_settings,
	.get_drvinfo	=	velocity_get_drvinfo,
	.set_tx_csum	=	ethtool_op_set_tx_csum,
	.get_tx_csum	=	ethtool_op_get_tx_csum,
	.get_wol	=	velocity_ethtool_get_wol,
	.set_wol	=	velocity_ethtool_set_wol,
	.get_msglevel	=	velocity_get_msglevel,
	.set_msglevel	=	velocity_set_msglevel,
	.set_sg 	=	ethtool_op_set_sg,
	.get_link	=	velocity_get_link,
	.get_coalesce	=	velocity_get_coalesce,
	.set_coalesce	=	velocity_set_coalesce,
	.begin		=	velocity_ethtool_up,
	.complete	=	velocity_ethtool_down
};

#ifdef CONFIG_PM
#ifdef CONFIG_INET
static int velocity_netdev_event(struct notifier_block *nb, unsigned long notification, void *ptr)
{
	struct in_ifaddr *ifa = (struct in_ifaddr *) ptr;
	struct net_device *dev = ifa->ifa_dev->dev;

	if (dev_net(dev) == &init_net &&
	    dev->netdev_ops == &velocity_netdev_ops)
		velocity_get_ip(netdev_priv(dev));

	return NOTIFY_DONE;
}
#endif	/* CONFIG_INET */
#endif	/* CONFIG_PM */

#if defined(CONFIG_PM) && defined(CONFIG_INET)
static struct notifier_block velocity_inetaddr_notifier = {
      .notifier_call	= velocity_netdev_event,
};

static void velocity_register_notifier(void)
{
	register_inetaddr_notifier(&velocity_inetaddr_notifier);
}

static void velocity_unregister_notifier(void)
{
	unregister_inetaddr_notifier(&velocity_inetaddr_notifier);
}

#else

#define velocity_register_notifier()	do {} while (0)
#define velocity_unregister_notifier()	do {} while (0)

#endif	/* defined(CONFIG_PM) && defined(CONFIG_INET) */

/**
 *	velocity_init_module	-	load time function
 *
 *	Called when the velocity module is loaded. The PCI driver
 *	is registered with the PCI layer, and in turn will call
 *	the probe functions for each velocity adapter installed
 *	in the system.
 */
static int __init velocity_init_module(void)
{
	int ret;

	velocity_register_notifier();
	ret = pci_register_driver(&velocity_driver);
	if (ret < 0)
		velocity_unregister_notifier();
	return ret;
}

/**
 *	velocity_cleanup	-	module unload
 *
 *	When the velocity hardware is unloaded this function is called.
 *	It will clean up the notifiers and the unregister the PCI
 *	driver interface for this hardware. This in turn cleans up
 *	all discovered interfaces before returning from the function
 */
static void __exit velocity_cleanup_module(void)
{
	velocity_unregister_notifier();
	pci_unregister_driver(&velocity_driver);
}

module_init(velocity_init_module);
module_exit(velocity_cleanup_module);
