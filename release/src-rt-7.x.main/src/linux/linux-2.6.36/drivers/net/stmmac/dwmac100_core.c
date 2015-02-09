/*******************************************************************************
  This is the driver for the MAC 10/100 on-chip Ethernet controller
  currently tested on all the ST boards based on STb7109 and stx7200 SoCs.

  DWC Ether MAC 10/100 Universal version 4.0 has been used for developing
  this code.

  This only implements the mac core functions for this chip.

  Copyright (C) 2007-2009  STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
*******************************************************************************/

#include <linux/crc32.h>
#include "dwmac100.h"

static void dwmac100_core_init(unsigned long ioaddr)
{
	u32 value = readl(ioaddr + MAC_CONTROL);

	writel((value | MAC_CORE_INIT), ioaddr + MAC_CONTROL);

#ifdef STMMAC_VLAN_TAG_USED
	writel(ETH_P_8021Q, ioaddr + MAC_VLAN1);
#endif
}

static void dwmac100_dump_mac_regs(unsigned long ioaddr)
{
	pr_info("\t----------------------------------------------\n"
		"\t  DWMAC 100 CSR (base addr = 0x%8x)\n"
		"\t----------------------------------------------\n",
		(unsigned int)ioaddr);
	pr_info("\tcontrol reg (offset 0x%x): 0x%08x\n", MAC_CONTROL,
		readl(ioaddr + MAC_CONTROL));
	pr_info("\taddr HI (offset 0x%x): 0x%08x\n ", MAC_ADDR_HIGH,
		readl(ioaddr + MAC_ADDR_HIGH));
	pr_info("\taddr LO (offset 0x%x): 0x%08x\n", MAC_ADDR_LOW,
		readl(ioaddr + MAC_ADDR_LOW));
	pr_info("\tmulticast hash HI (offset 0x%x): 0x%08x\n",
		MAC_HASH_HIGH, readl(ioaddr + MAC_HASH_HIGH));
	pr_info("\tmulticast hash LO (offset 0x%x): 0x%08x\n",
		MAC_HASH_LOW, readl(ioaddr + MAC_HASH_LOW));
	pr_info("\tflow control (offset 0x%x): 0x%08x\n",
		MAC_FLOW_CTRL, readl(ioaddr + MAC_FLOW_CTRL));
	pr_info("\tVLAN1 tag (offset 0x%x): 0x%08x\n", MAC_VLAN1,
		readl(ioaddr + MAC_VLAN1));
	pr_info("\tVLAN2 tag (offset 0x%x): 0x%08x\n", MAC_VLAN2,
		readl(ioaddr + MAC_VLAN2));
	pr_info("\n\tMAC management counter registers\n");
	pr_info("\t MMC crtl (offset 0x%x): 0x%08x\n",
		MMC_CONTROL, readl(ioaddr + MMC_CONTROL));
	pr_info("\t MMC High Interrupt (offset 0x%x): 0x%08x\n",
		MMC_HIGH_INTR, readl(ioaddr + MMC_HIGH_INTR));
	pr_info("\t MMC Low Interrupt (offset 0x%x): 0x%08x\n",
		MMC_LOW_INTR, readl(ioaddr + MMC_LOW_INTR));
	pr_info("\t MMC High Interrupt Mask (offset 0x%x): 0x%08x\n",
		MMC_HIGH_INTR_MASK, readl(ioaddr + MMC_HIGH_INTR_MASK));
	pr_info("\t MMC Low Interrupt Mask (offset 0x%x): 0x%08x\n",
		MMC_LOW_INTR_MASK, readl(ioaddr + MMC_LOW_INTR_MASK));
}

static void dwmac100_irq_status(unsigned long ioaddr)
{
	return;
}

static void dwmac100_set_umac_addr(unsigned long ioaddr, unsigned char *addr,
				   unsigned int reg_n)
{
	stmmac_set_mac_addr(ioaddr, addr, MAC_ADDR_HIGH, MAC_ADDR_LOW);
}

static void dwmac100_get_umac_addr(unsigned long ioaddr, unsigned char *addr,
				   unsigned int reg_n)
{
	stmmac_get_mac_addr(ioaddr, addr, MAC_ADDR_HIGH, MAC_ADDR_LOW);
}

static void dwmac100_set_filter(struct net_device *dev)
{
	unsigned long ioaddr = dev->base_addr;
	u32 value = readl(ioaddr + MAC_CONTROL);

	if (dev->flags & IFF_PROMISC) {
		value |= MAC_CONTROL_PR;
		value &= ~(MAC_CONTROL_PM | MAC_CONTROL_IF | MAC_CONTROL_HO |
			   MAC_CONTROL_HP);
	} else if ((netdev_mc_count(dev) > HASH_TABLE_SIZE)
		   || (dev->flags & IFF_ALLMULTI)) {
		value |= MAC_CONTROL_PM;
		value &= ~(MAC_CONTROL_PR | MAC_CONTROL_IF | MAC_CONTROL_HO);
		writel(0xffffffff, ioaddr + MAC_HASH_HIGH);
		writel(0xffffffff, ioaddr + MAC_HASH_LOW);
	} else if (netdev_mc_empty(dev)) {	/* no multicast */
		value &= ~(MAC_CONTROL_PM | MAC_CONTROL_PR | MAC_CONTROL_IF |
			   MAC_CONTROL_HO | MAC_CONTROL_HP);
	} else {
		u32 mc_filter[2];
		struct netdev_hw_addr *ha;

		/* Perfect filter mode for physical address and Hash
		   filter for multicast */
		value |= MAC_CONTROL_HP;
		value &= ~(MAC_CONTROL_PM | MAC_CONTROL_PR |
			   MAC_CONTROL_IF | MAC_CONTROL_HO);

		memset(mc_filter, 0, sizeof(mc_filter));
		netdev_for_each_mc_addr(ha, dev) {
			/* The upper 6 bits of the calculated CRC are used to
			 * index the contens of the hash table */
			int bit_nr =
			    ether_crc(ETH_ALEN, ha->addr) >> 26;
			/* The most significant bit determines the register to
			 * use (H/L) while the other 5 bits determine the bit
			 * within the register. */
			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
		}
		writel(mc_filter[0], ioaddr + MAC_HASH_LOW);
		writel(mc_filter[1], ioaddr + MAC_HASH_HIGH);
	}

	writel(value, ioaddr + MAC_CONTROL);

	CHIP_DBG(KERN_INFO "%s: CTRL reg: 0x%08x Hash regs: "
	    "HI 0x%08x, LO 0x%08x\n",
	    __func__, readl(ioaddr + MAC_CONTROL),
	    readl(ioaddr + MAC_HASH_HIGH), readl(ioaddr + MAC_HASH_LOW));
}

static void dwmac100_flow_ctrl(unsigned long ioaddr, unsigned int duplex,
			       unsigned int fc, unsigned int pause_time)
{
	unsigned int flow = MAC_FLOW_CTRL_ENABLE;

	if (duplex)
		flow |= (pause_time << MAC_FLOW_CTRL_PT_SHIFT);
	writel(flow, ioaddr + MAC_FLOW_CTRL);
}

/* No PMT module supported for this Ethernet Controller.
 * Tested on ST platforms only.
 */
static void dwmac100_pmt(unsigned long ioaddr, unsigned long mode)
{
	return;
}

struct stmmac_ops dwmac100_ops = {
	.core_init = dwmac100_core_init,
	.dump_regs = dwmac100_dump_mac_regs,
	.host_irq_status = dwmac100_irq_status,
	.set_filter = dwmac100_set_filter,
	.flow_ctrl = dwmac100_flow_ctrl,
	.pmt = dwmac100_pmt,
	.set_umac_addr = dwmac100_set_umac_addr,
	.get_umac_addr = dwmac100_get_umac_addr,
};

struct mac_device_info *dwmac100_setup(unsigned long ioaddr)
{
	struct mac_device_info *mac;

	mac = kzalloc(sizeof(const struct mac_device_info), GFP_KERNEL);
	if (!mac)
		return NULL;

	pr_info("\tDWMAC100\n");

	mac->mac = &dwmac100_ops;
	mac->dma = &dwmac100_dma_ops;

	mac->pmt = PMT_NOT_SUPPORTED;
	mac->link.port = MAC_CONTROL_PS;
	mac->link.duplex = MAC_CONTROL_F;
	mac->link.speed = 0;
	mac->mii.addr = MAC_MII_ADDR;
	mac->mii.data = MAC_MII_DATA;

	return mac;
}
