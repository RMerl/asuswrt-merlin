/*
 * drivers/net/titan_ge.c - Driver for Titan ethernet ports
 *
 * Copyright (C) 2003 PMC-Sierra Inc.
 * Author : Manish Lachwani (lachwani@pmc-sierra.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * The MAC unit of the Titan consists of the following:
 *
 * -> XDMA Engine to move data to from the memory to the MAC packet FIFO
 * -> FIFO is where the incoming and outgoing data is placed
 * -> TRTG is the unit that pulls the data from the FIFO for Tx and pushes
 *    the data into the FIFO for Rx
 * -> TMAC is the outgoing MAC interface and RMAC is the incoming.
 * -> AFX is the address filtering block
 * -> GMII block to communicate with the PHY
 *
 * Rx will look like the following:
 * GMII --> RMAC --> AFX --> TRTG --> Rx FIFO --> XDMA --> CPU memory
 *
 * Tx will look like the following:
 * CPU memory --> XDMA --> Tx FIFO --> TRTG --> TMAC --> GMII
 *
 * The Titan driver has support for the following performance features:
 * -> Rx side checksumming
 * -> Jumbo Frames
 * -> Interrupt Coalscing
 * -> Rx NAPI
 * -> SKB Recycling
 * -> Transmit/Receive descriptors in SRAM
 * -> Fast routing for IP forwarding
 */

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ip.h>
#include <linux/init.h>
#include <linux/in.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/mii.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/prefetch.h>

/* For MII specifc registers, titan_mdio.h should be included */
#include <net/ip.h>

#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/types.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/titan_dep.h>

#include "titan_ge.h"
#include "titan_mdio.h"

/* Static Function Declarations	 */
static int titan_ge_eth_open(struct net_device *);
static void titan_ge_eth_stop(struct net_device *);
static struct net_device_stats *titan_ge_get_stats(struct net_device *);
static int titan_ge_init_rx_desc_ring(titan_ge_port_info *, int, int,
				      unsigned long, unsigned long,
				      unsigned long);
static int titan_ge_init_tx_desc_ring(titan_ge_port_info *, int,
				      unsigned long, unsigned long);

static int titan_ge_open(struct net_device *);
static int titan_ge_start_xmit(struct sk_buff *, struct net_device *);
static int titan_ge_stop(struct net_device *);

static unsigned long titan_ge_tx_coal(unsigned long, int);

static void titan_ge_port_reset(unsigned int);
static int titan_ge_free_tx_queue(titan_ge_port_info *);
static int titan_ge_rx_task(struct net_device *, titan_ge_port_info *);
static int titan_ge_port_start(struct net_device *, titan_ge_port_info *);

static int titan_ge_return_tx_desc(titan_ge_port_info *, int);

/*
 * Some configuration for the FIFO and the XDMA channel needs
 * to be done only once for all the ports. This flag controls
 * that
 */
static unsigned long config_done;

/*
 * One time out of memory flag
 */
static unsigned int oom_flag;

static int titan_ge_poll(struct net_device *netdev, int *budget);

static int titan_ge_receive_queue(struct net_device *, unsigned int);

static struct platform_device *titan_ge_device[3];

/* MAC Address */
extern unsigned char titan_ge_mac_addr_base[6];

unsigned long titan_ge_base;
static unsigned long titan_ge_sram;

static char titan_string[] = "titan";

/*
 * The Titan GE has two alignment requirements:
 * -> skb->data to be cacheline aligned (32 byte)
 * -> IP header alignment to 16 bytes
 *
 * The latter is not implemented. So, that results in an extra copy on
 * the Rx. This is a big performance hog. For the former case, the
 * dev_alloc_skb() has been replaced with titan_ge_alloc_skb(). The size
 * requested is calculated:
 *
 * Ethernet Frame Size : 1518
 * Ethernet Header     : 14
 * Future Titan change for IP header alignment : 2
 *
 * Hence, we allocate (1518 + 14 + 2+ 64) = 1580 bytes.  For IP header
 * alignment, we use skb_reserve().
 */

#define ALIGNED_RX_SKB_ADDR(addr) \
	((((unsigned long)(addr) + (64UL - 1UL)) \
	& ~(64UL - 1UL)) - (unsigned long)(addr))

#define titan_ge_alloc_skb(__length, __gfp_flags) \
({      struct sk_buff *__skb; \
	__skb = alloc_skb((__length) + 64, (__gfp_flags)); \
	if(__skb) { \
		int __offset = (int) ALIGNED_RX_SKB_ADDR(__skb->data); \
		if(__offset) \
			skb_reserve(__skb, __offset); \
	} \
	__skb; \
})

/*
 * Configure the GMII block of the Titan based on what the PHY tells us
 */
static void titan_ge_gmii_config(int port_num)
{
	unsigned int reg_data = 0, phy_reg;
	int err;

	err = titan_ge_mdio_read(port_num, TITAN_GE_MDIO_PHY_STATUS, &phy_reg);

	if (err == TITAN_GE_MDIO_ERROR) {
		printk(KERN_ERR
		       "Could not read PHY control register 0x11 \n");
		printk(KERN_ERR
			"Setting speed to 1000 Mbps and Duplex to Full \n");

		return;
	}

	err = titan_ge_mdio_write(port_num, TITAN_GE_MDIO_PHY_IE, 0);

	if (phy_reg & 0x8000) {
		if (phy_reg & 0x2000) {
			/* Full Duplex and 1000 Mbps */
			TITAN_GE_WRITE((TITAN_GE_GMII_CONFIG_MODE +
					(port_num << 12)), 0x201);
		}  else {
			/* Half Duplex and 1000 Mbps */
			TITAN_GE_WRITE((TITAN_GE_GMII_CONFIG_MODE +
					(port_num << 12)), 0x2201);
			}
	}
	if (phy_reg & 0x4000) {
		if (phy_reg & 0x2000) {
			/* Full Duplex and 100 Mbps */
			TITAN_GE_WRITE((TITAN_GE_GMII_CONFIG_MODE +
					(port_num << 12)), 0x100);
		} else {
			/* Half Duplex and 100 Mbps */
			TITAN_GE_WRITE((TITAN_GE_GMII_CONFIG_MODE +
					(port_num << 12)), 0x2100);
		}
	}
	reg_data = TITAN_GE_READ(TITAN_GE_GMII_CONFIG_GENERAL +
				(port_num << 12));
	reg_data |= 0x3;
	TITAN_GE_WRITE((TITAN_GE_GMII_CONFIG_GENERAL +
			(port_num << 12)), reg_data);
}

/*
 * Enable the TMAC if it is not
 */
static void titan_ge_enable_tx(unsigned int port_num)
{
	unsigned long reg_data;

	reg_data = TITAN_GE_READ(TITAN_GE_TMAC_CONFIG_1 + (port_num << 12));
	if (!(reg_data & 0x8000)) {
		printk("TMAC disabled for port %d!! \n", port_num);

		reg_data |= 0x0001;	/* Enable TMAC */
		reg_data |= 0x4000;	/* CRC Check Enable */
		reg_data |= 0x2000;	/* Padding enable */
		reg_data |= 0x0800;	/* CRC Add enable */
		reg_data |= 0x0080;	/* PAUSE frame */

		TITAN_GE_WRITE((TITAN_GE_TMAC_CONFIG_1 +
				(port_num << 12)), reg_data);
	}
}

/*
 * Tx Timeout function
 */
static void titan_ge_tx_timeout(struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);

	printk(KERN_INFO "%s: TX timeout  ", netdev->name);
	printk(KERN_INFO "Resetting card \n");

	/* Do the reset outside of interrupt context */
	schedule_work(&titan_ge_eth->tx_timeout_task);
}

/*
 * Update the AFX tables for UC and MC for slice 0 only
 */
static void titan_ge_update_afx(titan_ge_port_info * titan_ge_eth)
{
	int port = titan_ge_eth->port_num;
	unsigned int i;
	volatile unsigned long reg_data = 0;
	u8 p_addr[6];

	memcpy(p_addr, titan_ge_eth->port_mac_addr, 6);

	/* Set the MAC address here for TMAC and RMAC */
	TITAN_GE_WRITE((TITAN_GE_TMAC_STATION_HI + (port << 12)),
		       ((p_addr[5] << 8) | p_addr[4]));
	TITAN_GE_WRITE((TITAN_GE_TMAC_STATION_MID + (port << 12)),
		       ((p_addr[3] << 8) | p_addr[2]));
	TITAN_GE_WRITE((TITAN_GE_TMAC_STATION_LOW + (port << 12)),
		       ((p_addr[1] << 8) | p_addr[0]));

	TITAN_GE_WRITE((TITAN_GE_RMAC_STATION_HI + (port << 12)),
		       ((p_addr[5] << 8) | p_addr[4]));
	TITAN_GE_WRITE((TITAN_GE_RMAC_STATION_MID + (port << 12)),
		       ((p_addr[3] << 8) | p_addr[2]));
	TITAN_GE_WRITE((TITAN_GE_RMAC_STATION_LOW + (port << 12)),
		       ((p_addr[1] << 8) | p_addr[0]));

	TITAN_GE_WRITE((0x112c | (port << 12)), 0x1);
	/* Configure the eight address filters */
	for (i = 0; i < 8; i++) {
		/* Select each of the eight filters */
		TITAN_GE_WRITE((TITAN_GE_AFX_ADDRS_FILTER_CTRL_2 +
				(port << 12)), i);

		/* Configure the match */
		reg_data = 0x9;	/* Forward Enable Bit */
		TITAN_GE_WRITE((TITAN_GE_AFX_ADDRS_FILTER_CTRL_0 +
				(port << 12)), reg_data);

		/* Finally, AFX Exact Match Address Registers */
		TITAN_GE_WRITE((TITAN_GE_AFX_EXACT_MATCH_LOW + (port << 12)),
			       ((p_addr[1] << 8) | p_addr[0]));
		TITAN_GE_WRITE((TITAN_GE_AFX_EXACT_MATCH_MID + (port << 12)),
			       ((p_addr[3] << 8) | p_addr[2]));
		TITAN_GE_WRITE((TITAN_GE_AFX_EXACT_MATCH_HIGH + (port << 12)),
			       ((p_addr[5] << 8) | p_addr[4]));

		/* VLAN id set to 0 */
		TITAN_GE_WRITE((TITAN_GE_AFX_EXACT_MATCH_VID +
				(port << 12)), 0);
	}
}

/*
 * Actual Routine to reset the adapter when the timeout occurred
 */
static void titan_ge_tx_timeout_task(struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	int port = titan_ge_eth->port_num;

	printk("Titan GE: Transmit timed out. Resetting ... \n");

	/* Dump debug info */
	printk(KERN_ERR "TRTG cause : %x \n",
			TITAN_GE_READ(0x100c + (port << 12)));

	/* Fix this for the other ports */
	printk(KERN_ERR "FIFO cause : %x \n", TITAN_GE_READ(0x482c));
	printk(KERN_ERR "IE cause : %x \n", TITAN_GE_READ(0x0040));
	printk(KERN_ERR "XDMA GDI ERROR : %x \n",
			TITAN_GE_READ(0x5008 + (port << 8)));
	printk(KERN_ERR "CHANNEL ERROR: %x \n",
			TITAN_GE_READ(TITAN_GE_CHANNEL0_INTERRUPT
						+ (port << 8)));

	netif_device_detach(netdev);
	titan_ge_port_reset(titan_ge_eth->port_num);
	titan_ge_port_start(netdev, titan_ge_eth);
	netif_device_attach(netdev);
}

/*
 * Change the MTU of the Ethernet Device
 */
static int titan_ge_change_mtu(struct net_device *netdev, int new_mtu)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	unsigned long flags;

	if ((new_mtu > 9500) || (new_mtu < 64))
		return -EINVAL;

	spin_lock_irqsave(&titan_ge_eth->lock, flags);

	netdev->mtu = new_mtu;

	/* Now we have to reopen the interface so that SKBs with the new
	 * size will be allocated */

	if (netif_running(netdev)) {
		titan_ge_eth_stop(netdev);

		if (titan_ge_eth_open(netdev) != TITAN_OK) {
			printk(KERN_ERR
			       "%s: Fatal error on opening device\n",
			       netdev->name);
			spin_unlock_irqrestore(&titan_ge_eth->lock, flags);
			return -1;
		}
	}

	spin_unlock_irqrestore(&titan_ge_eth->lock, flags);
	return 0;
}

/*
 * Titan Gbe Interrupt Handler. All the three ports send interrupt to one line
 * only. Once an interrupt is triggered, figure out the port and then check
 * the channel.
 */
static irqreturn_t titan_ge_int_handler(int irq, void *dev_id)
{
	struct net_device *netdev = (struct net_device *) dev_id;
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	unsigned int port_num = titan_ge_eth->port_num;
	unsigned int reg_data;
	unsigned int eth_int_cause_error = 0, is;
	unsigned long eth_int_cause1;
	int err = 0;
#ifdef CONFIG_SMP
	unsigned long eth_int_cause2;
#endif

	/* Ack the CPU interrupt */
	switch (port_num) {
	case 0:
		is = OCD_READ(RM9000x2_OCD_INTP0STATUS1);
		OCD_WRITE(RM9000x2_OCD_INTP0CLEAR1, is);

#ifdef CONFIG_SMP
		is = OCD_READ(RM9000x2_OCD_INTP1STATUS1);
		OCD_WRITE(RM9000x2_OCD_INTP1CLEAR1, is);
#endif
		break;

	case 1:
		is = OCD_READ(RM9000x2_OCD_INTP0STATUS0);
		OCD_WRITE(RM9000x2_OCD_INTP0CLEAR0, is);

#ifdef CONFIG_SMP
		is = OCD_READ(RM9000x2_OCD_INTP1STATUS0);
		OCD_WRITE(RM9000x2_OCD_INTP1CLEAR0, is);
#endif
		break;

	case 2:
		is = OCD_READ(RM9000x2_OCD_INTP0STATUS4);
		OCD_WRITE(RM9000x2_OCD_INTP0CLEAR4, is);

#ifdef CONFIG_SMP
		is = OCD_READ(RM9000x2_OCD_INTP1STATUS4);
		OCD_WRITE(RM9000x2_OCD_INTP1CLEAR4, is);
#endif
	}

	eth_int_cause1 = TITAN_GE_READ(TITAN_GE_INTR_XDMA_CORE_A);
#ifdef CONFIG_SMP
	eth_int_cause2 = TITAN_GE_READ(TITAN_GE_INTR_XDMA_CORE_B);
#endif

	/* Spurious interrupt */
#ifdef CONFIG_SMP
	if ( (eth_int_cause1 == 0) && (eth_int_cause2 == 0)) {
#else
	if (eth_int_cause1 == 0) {
#endif
		eth_int_cause_error = TITAN_GE_READ(TITAN_GE_CHANNEL0_INTERRUPT +
					(port_num << 8));

		if (eth_int_cause_error == 0)
			return IRQ_NONE;
	}

	/* Handle Tx first. No need to ack interrupts */
#ifdef CONFIG_SMP
	if ( (eth_int_cause1 & 0x20202) ||
		(eth_int_cause2 & 0x20202) )
#else
	if (eth_int_cause1 & 0x20202)
#endif
		titan_ge_free_tx_queue(titan_ge_eth);

	/* Handle the Rx next */
#ifdef CONFIG_SMP
	if ( (eth_int_cause1 & 0x10101) ||
		(eth_int_cause2 & 0x10101)) {
#else
	if (eth_int_cause1 & 0x10101) {
#endif
		if (netif_rx_schedule_prep(netdev)) {
			unsigned int ack;

			ack = TITAN_GE_READ(TITAN_GE_INTR_XDMA_IE);
			/* Disable Tx and Rx both */
			if (port_num == 0)
				ack &= ~(0x3);
			if (port_num == 1)
				ack &= ~(0x300);

			if (port_num == 2)
				ack &= ~(0x30000);

			/* Interrupts have been disabled */
			TITAN_GE_WRITE(TITAN_GE_INTR_XDMA_IE, ack);

			__netif_rx_schedule(netdev);
		}
	}

	/* Handle error interrupts */
	if (eth_int_cause_error && (eth_int_cause_error != 0x2)) {
		printk(KERN_ERR
			"XDMA Channel Error : %x  on port %d\n",
			eth_int_cause_error, port_num);

		printk(KERN_ERR
			"XDMA GDI Hardware error : %x  on port %d\n",
			TITAN_GE_READ(0x5008 + (port_num << 8)), port_num);

		printk(KERN_ERR
			"XDMA currently has %d Rx descriptors \n",
			TITAN_GE_READ(0x5048 + (port_num << 8)));

		printk(KERN_ERR
			"XDMA currently has prefetcted %d Rx descriptors \n",
			TITAN_GE_READ(0x505c + (port_num << 8)));

		TITAN_GE_WRITE((TITAN_GE_CHANNEL0_INTERRUPT +
			       (port_num << 8)), eth_int_cause_error);
	}

	/*
	 * PHY interrupt to inform abt the changes. Reading the
	 * PHY Status register will clear the interrupt
	 */
	if ((!(eth_int_cause1 & 0x30303)) &&
		(eth_int_cause_error == 0)) {
		err =
		    titan_ge_mdio_read(port_num,
			       TITAN_GE_MDIO_PHY_IS, &reg_data);

		if (reg_data & 0x0400) {
			/* Link status change */
			titan_ge_mdio_read(port_num,
				   TITAN_GE_MDIO_PHY_STATUS, &reg_data);
			if (!(reg_data & 0x0400)) {
				/* Link is down */
				netif_carrier_off(netdev);
				netif_stop_queue(netdev);
			} else {
				/* Link is up */
				netif_carrier_on(netdev);
				netif_wake_queue(netdev);

				/* Enable the queue */
				titan_ge_enable_tx(port_num);
			}
		}
	}

	return IRQ_HANDLED;
}

/*
 * Multicast and Promiscuous mode set. The
 * set_multi entry point is called whenever the
 * multicast address list or the network interface
 * flags are updated.
 */
static void titan_ge_set_multi(struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	unsigned int port_num = titan_ge_eth->port_num;
	unsigned long reg_data;

	reg_data = TITAN_GE_READ(TITAN_GE_AFX_ADDRS_FILTER_CTRL_1 +
				(port_num << 12));

	if (netdev->flags & IFF_PROMISC) {
		reg_data |= 0x2;
	}
	else if (netdev->flags & IFF_ALLMULTI) {
		reg_data |= 0x01;
		reg_data |= 0x400; /* Use the 64-bit Multicast Hash bin */
	}
	else {
		reg_data = 0x2;
	}

	TITAN_GE_WRITE((TITAN_GE_AFX_ADDRS_FILTER_CTRL_1 +
			(port_num << 12)), reg_data);
	if (reg_data & 0x01) {
		TITAN_GE_WRITE((TITAN_GE_AFX_MULTICAST_HASH_LOW +
				(port_num << 12)), 0xffff);
		TITAN_GE_WRITE((TITAN_GE_AFX_MULTICAST_HASH_MIDLOW +
				(port_num << 12)), 0xffff);
		TITAN_GE_WRITE((TITAN_GE_AFX_MULTICAST_HASH_MIDHI +
				(port_num << 12)), 0xffff);
		TITAN_GE_WRITE((TITAN_GE_AFX_MULTICAST_HASH_HI +
				(port_num << 12)), 0xffff);
	}
}

/*
 * Open the network device
 */
static int titan_ge_open(struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	unsigned int port_num = titan_ge_eth->port_num;
	unsigned int irq = TITAN_ETH_PORT_IRQ - port_num;
	int retval;

	retval = request_irq(irq, titan_ge_int_handler,
		     SA_INTERRUPT | SA_SAMPLE_RANDOM , netdev->name, netdev);

	if (retval != 0) {
		printk(KERN_ERR "Cannot assign IRQ number to TITAN GE \n");
		return -1;
	}

	netdev->irq = irq;
	printk(KERN_INFO "Assigned IRQ %d to port %d\n", irq, port_num);

	spin_lock_irq(&(titan_ge_eth->lock));

	if (titan_ge_eth_open(netdev) != TITAN_OK) {
		spin_unlock_irq(&(titan_ge_eth->lock));
		printk("%s: Error opening interface \n", netdev->name);
		free_irq(netdev->irq, netdev);
		return -EBUSY;
	}

	spin_unlock_irq(&(titan_ge_eth->lock));

	return 0;
}

/*
 * Allocate the SKBs for the Rx ring. Also used
 * for refilling the queue
 */
static int titan_ge_rx_task(struct net_device *netdev,
				titan_ge_port_info *titan_ge_port)
{
	struct device *device = &titan_ge_device[titan_ge_port->port_num]->dev;
	volatile titan_ge_rx_desc *rx_desc;
	struct sk_buff *skb;
	int rx_used_desc;
	int count = 0;

	while (titan_ge_port->rx_ring_skbs < titan_ge_port->rx_ring_size) {

	/* First try to get the skb from the recycler */
#ifdef TITAN_GE_JUMBO_FRAMES
		skb = titan_ge_alloc_skb(TITAN_GE_JUMBO_BUFSIZE, GFP_ATOMIC);
#else
		skb = titan_ge_alloc_skb(TITAN_GE_STD_BUFSIZE, GFP_ATOMIC);
#endif
		if (unlikely(!skb)) {
			/* OOM, set the flag */
			printk("OOM \n");
			oom_flag = 1;
			break;
		}
		count++;
		skb->dev = netdev;

		titan_ge_port->rx_ring_skbs++;

		rx_used_desc = titan_ge_port->rx_used_desc_q;
		rx_desc = &(titan_ge_port->rx_desc_area[rx_used_desc]);

#ifdef TITAN_GE_JUMBO_FRAMES
		rx_desc->buffer_addr = dma_map_single(device, skb->data,
				TITAN_GE_JUMBO_BUFSIZE - 2, DMA_FROM_DEVICE);
#else
		rx_desc->buffer_addr = dma_map_single(device, skb->data,
				TITAN_GE_STD_BUFSIZE - 2, DMA_FROM_DEVICE);
#endif

		titan_ge_port->rx_skb[rx_used_desc] = skb;
		rx_desc->cmd_sts = TITAN_GE_RX_BUFFER_OWNED;

		titan_ge_port->rx_used_desc_q =
			(rx_used_desc + 1) % TITAN_GE_RX_QUEUE;
	}

	return count;
}

/*
 * Actual init of the Tital GE port. There is one register for
 * the channel configuration
 */
static void titan_port_init(struct net_device *netdev,
			    titan_ge_port_info * titan_ge_eth)
{
	unsigned long reg_data;

	titan_ge_port_reset(titan_ge_eth->port_num);

	/* First reset the TMAC */
	reg_data = TITAN_GE_READ(TITAN_GE_CHANNEL0_CONFIG);
	reg_data |= 0x80000000;
	TITAN_GE_WRITE(TITAN_GE_CHANNEL0_CONFIG, reg_data);

	udelay(30);

	reg_data = TITAN_GE_READ(TITAN_GE_CHANNEL0_CONFIG);
	reg_data &= ~(0xc0000000);
	TITAN_GE_WRITE(TITAN_GE_CHANNEL0_CONFIG, reg_data);

	/* Now reset the RMAC */
	reg_data = TITAN_GE_READ(TITAN_GE_CHANNEL0_CONFIG);
	reg_data |= 0x00080000;
	TITAN_GE_WRITE(TITAN_GE_CHANNEL0_CONFIG, reg_data);

	udelay(30);

	reg_data = TITAN_GE_READ(TITAN_GE_CHANNEL0_CONFIG);
	reg_data &= ~(0x000c0000);
	TITAN_GE_WRITE(TITAN_GE_CHANNEL0_CONFIG, reg_data);
}

/*
 * Start the port. All the hardware specific configuration
 * for the XDMA, Tx FIFO, Rx FIFO, TMAC, RMAC, TRTG and AFX
 * go here
 */
static int titan_ge_port_start(struct net_device *netdev,
				titan_ge_port_info * titan_port)
{
	volatile unsigned long reg_data, reg_data1;
	int port_num = titan_port->port_num;
	int count = 0;
	unsigned long reg_data_1;

	if (config_done == 0) {
		reg_data = TITAN_GE_READ(0x0004);
		reg_data |= 0x100;
		TITAN_GE_WRITE(0x0004, reg_data);

		reg_data &= ~(0x100);
		TITAN_GE_WRITE(0x0004, reg_data);

		/* Turn on GMII/MII mode and turn off TBI mode */
		reg_data = TITAN_GE_READ(TITAN_GE_TSB_CTRL_1);
		reg_data |= 0x00000700;
		reg_data &= ~(0x00800000); /* Fencing */

		TITAN_GE_WRITE(0x000c, 0x00001100);

		TITAN_GE_WRITE(TITAN_GE_TSB_CTRL_1, reg_data);

		/* Set the CPU Resource Limit register */
		TITAN_GE_WRITE(0x00f8, 0x8);

		/* Be conservative when using the BIU buffers */
		TITAN_GE_WRITE(0x0068, 0x4);
	}

	titan_port->tx_threshold = 0;
	titan_port->rx_threshold = 0;

	/* We need to write the descriptors for Tx and Rx */
	TITAN_GE_WRITE((TITAN_GE_CHANNEL0_TX_DESC + (port_num << 8)),
		       (unsigned long) titan_port->tx_dma);
	TITAN_GE_WRITE((TITAN_GE_CHANNEL0_RX_DESC + (port_num << 8)),
		       (unsigned long) titan_port->rx_dma);

	if (config_done == 0) {
		/* Step 1:  XDMA config	*/
		reg_data = TITAN_GE_READ(TITAN_GE_XDMA_CONFIG);
		reg_data &= ~(0x80000000);      /* clear reset */
		reg_data |= 0x1 << 29;	/* sparse tx descriptor spacing */
		reg_data |= 0x1 << 28;	/* sparse rx descriptor spacing */
		reg_data |= (0x1 << 23) | (0x1 << 24);  /* Descriptor Coherency */
		reg_data |= (0x1 << 21) | (0x1 << 22);  /* Data Coherency */
		TITAN_GE_WRITE(TITAN_GE_XDMA_CONFIG, reg_data);
	}

	/* IR register for the XDMA */
	reg_data = TITAN_GE_READ(TITAN_GE_GDI_INTERRUPT_ENABLE + (port_num << 8));
	reg_data |= 0x80068000; /* No Rx_OOD */
	TITAN_GE_WRITE((TITAN_GE_GDI_INTERRUPT_ENABLE + (port_num << 8)), reg_data);

	/* Start the Tx and Rx XDMA controller */
	reg_data = TITAN_GE_READ(TITAN_GE_CHANNEL0_CONFIG + (port_num << 8));
	reg_data &= 0x4fffffff;     /* Clear tx reset */
	reg_data &= 0xfff4ffff;     /* Clear rx reset */

#ifdef TITAN_GE_JUMBO_FRAMES
	reg_data |= 0xa0 | 0x30030000;
#else
	reg_data |= 0x40 | 0x20030000;
#endif

#ifndef CONFIG_SMP
	reg_data &= ~(0x10);
	reg_data |= 0x0f; /* All of the packet */
#endif

	TITAN_GE_WRITE((TITAN_GE_CHANNEL0_CONFIG + (port_num << 8)), reg_data);

	/* Rx desc count */
	count = titan_ge_rx_task(netdev, titan_port);
	TITAN_GE_WRITE((0x5048 + (port_num << 8)), count);
	count = TITAN_GE_READ(0x5048 + (port_num << 8));

	udelay(30);

	/*
	 * Step 2:  Configure the SDQPF, i.e. FIFO
	 */
	if (config_done == 0) {
		reg_data = TITAN_GE_READ(TITAN_GE_SDQPF_RXFIFO_CTL);
		reg_data = 0x1;
		TITAN_GE_WRITE(TITAN_GE_SDQPF_RXFIFO_CTL, reg_data);
		reg_data &= ~(0x1);
		TITAN_GE_WRITE(TITAN_GE_SDQPF_RXFIFO_CTL, reg_data);
		reg_data = TITAN_GE_READ(TITAN_GE_SDQPF_RXFIFO_CTL);
		TITAN_GE_WRITE(TITAN_GE_SDQPF_RXFIFO_CTL, reg_data);

		reg_data = TITAN_GE_READ(TITAN_GE_SDQPF_TXFIFO_CTL);
		reg_data = 0x1;
		TITAN_GE_WRITE(TITAN_GE_SDQPF_TXFIFO_CTL, reg_data);
		reg_data &= ~(0x1);
		TITAN_GE_WRITE(TITAN_GE_SDQPF_TXFIFO_CTL, reg_data);
		reg_data = TITAN_GE_READ(TITAN_GE_SDQPF_TXFIFO_CTL);
		TITAN_GE_WRITE(TITAN_GE_SDQPF_TXFIFO_CTL, reg_data);
	}
	/*
	 * Enable RX FIFO 0, 4 and 8
	 */
	if (port_num == 0) {
		reg_data = TITAN_GE_READ(TITAN_GE_SDQPF_RXFIFO_0);

		reg_data |= 0x100000;
		reg_data |= (0xff << 10);

		TITAN_GE_WRITE(TITAN_GE_SDQPF_RXFIFO_0, reg_data);
		/*
		 * BAV2,BAV and DAV settings for the Rx FIFO
		 */
		reg_data1 = TITAN_GE_READ(0x4844);
		reg_data1 |= ( (0x10 << 20) | (0x10 << 10) | 0x1);
		TITAN_GE_WRITE(0x4844, reg_data1);

		reg_data &= ~(0x00100000);
		reg_data |= 0x200000;

		TITAN_GE_WRITE(TITAN_GE_SDQPF_RXFIFO_0, reg_data);

		reg_data = TITAN_GE_READ(TITAN_GE_SDQPF_TXFIFO_0);
		reg_data |= 0x100000;

		TITAN_GE_WRITE(TITAN_GE_SDQPF_TXFIFO_0, reg_data);

		reg_data |= (0xff << 10);

		TITAN_GE_WRITE(TITAN_GE_SDQPF_TXFIFO_0, reg_data);

		/*
		 * BAV2, BAV and DAV settings for the Tx FIFO
		 */
		reg_data1 = TITAN_GE_READ(0x4944);
		reg_data1 = ( (0x1 << 20) | (0x1 << 10) | 0x10);

		TITAN_GE_WRITE(0x4944, reg_data1);

		reg_data &= ~(0x00100000);
		reg_data |= 0x200000;

		TITAN_GE_WRITE(TITAN_GE_SDQPF_TXFIFO_0, reg_data);

	}

	if (port_num == 1) {
		reg_data = TITAN_GE_READ(0x4870);

		reg_data |= 0x100000;
		reg_data |= (0xff << 10) | (0xff + 1);

		TITAN_GE_WRITE(0x4870, reg_data);
		/*
		 * BAV2,BAV and DAV settings for the Rx FIFO
		 */
		reg_data1 = TITAN_GE_READ(0x4874);
		reg_data1 |= ( (0x10 << 20) | (0x10 << 10) | 0x1);
		TITAN_GE_WRITE(0x4874, reg_data1);

		reg_data &= ~(0x00100000);
		reg_data |= 0x200000;

		TITAN_GE_WRITE(0x4870, reg_data);

		reg_data = TITAN_GE_READ(0x494c);
		reg_data |= 0x100000;

		TITAN_GE_WRITE(0x494c, reg_data);
		reg_data |= (0xff << 10) | (0xff + 1);
		TITAN_GE_WRITE(0x494c, reg_data);

		/*
		 * BAV2, BAV and DAV settings for the Tx FIFO
		 */
		reg_data1 = TITAN_GE_READ(0x4950);
		reg_data1 = ( (0x1 << 20) | (0x1 << 10) | 0x10);

		TITAN_GE_WRITE(0x4950, reg_data1);

		reg_data &= ~(0x00100000);
		reg_data |= 0x200000;

		TITAN_GE_WRITE(0x494c, reg_data);
	}

	/*
	 * Titan 1.2 revision does support port #2
	 */
	if (port_num == 2) {
		/*
		 * Put the descriptors in the SRAM
		 */
		reg_data = TITAN_GE_READ(0x48a0);

		reg_data |= 0x100000;
		reg_data |= (0xff << 10) | (2*(0xff + 1));

		TITAN_GE_WRITE(0x48a0, reg_data);
		/*
		 * BAV2,BAV and DAV settings for the Rx FIFO
		 */
		reg_data1 = TITAN_GE_READ(0x48a4);
		reg_data1 |= ( (0x10 << 20) | (0x10 << 10) | 0x1);
		TITAN_GE_WRITE(0x48a4, reg_data1);

		reg_data &= ~(0x00100000);
		reg_data |= 0x200000;

		TITAN_GE_WRITE(0x48a0, reg_data);

		reg_data = TITAN_GE_READ(0x4958);
		reg_data |= 0x100000;

		TITAN_GE_WRITE(0x4958, reg_data);
		reg_data |= (0xff << 10) | (2*(0xff + 1));
		TITAN_GE_WRITE(0x4958, reg_data);

		/*
		 * BAV2, BAV and DAV settings for the Tx FIFO
		 */
		reg_data1 = TITAN_GE_READ(0x495c);
		reg_data1 = ( (0x1 << 20) | (0x1 << 10) | 0x10);

		TITAN_GE_WRITE(0x495c, reg_data1);

		reg_data &= ~(0x00100000);
		reg_data |= 0x200000;

		TITAN_GE_WRITE(0x4958, reg_data);
	}

	if (port_num == 2) {
		reg_data = TITAN_GE_READ(0x48a0);

		reg_data |= 0x100000;
		reg_data |= (0xff << 10) | (2*(0xff + 1));

		TITAN_GE_WRITE(0x48a0, reg_data);
		/*
		 * BAV2,BAV and DAV settings for the Rx FIFO
		 */
		reg_data1 = TITAN_GE_READ(0x48a4);
		reg_data1 |= ( (0x10 << 20) | (0x10 << 10) | 0x1);
		TITAN_GE_WRITE(0x48a4, reg_data1);

		reg_data &= ~(0x00100000);
		reg_data |= 0x200000;

		TITAN_GE_WRITE(0x48a0, reg_data);

		reg_data = TITAN_GE_READ(0x4958);
		reg_data |= 0x100000;

		TITAN_GE_WRITE(0x4958, reg_data);
		reg_data |= (0xff << 10) | (2*(0xff + 1));
		TITAN_GE_WRITE(0x4958, reg_data);

		/*
		 * BAV2, BAV and DAV settings for the Tx FIFO
		 */
		reg_data1 = TITAN_GE_READ(0x495c);
		reg_data1 = ( (0x1 << 20) | (0x1 << 10) | 0x10);

		TITAN_GE_WRITE(0x495c, reg_data1);

		reg_data &= ~(0x00100000);
		reg_data |= 0x200000;

		TITAN_GE_WRITE(0x4958, reg_data);
	}

	/*
	 * Step 3:  TRTG block enable
	 */
	reg_data = TITAN_GE_READ(TITAN_GE_TRTG_CONFIG + (port_num << 12));

	/*
	 * This is the 1.2 revision of the chip. It has fix for the
	 * IP header alignment. Now, the IP header begins at an
	 * aligned address and this wont need an extra copy in the
	 * driver. This performance drawback existed in the previous
	 * versions of the silicon
	 */
	reg_data_1 = TITAN_GE_READ(0x103c + (port_num << 12));
	reg_data_1 |= 0x40000000;
	TITAN_GE_WRITE((0x103c + (port_num << 12)), reg_data_1);

	reg_data_1 |= 0x04000000;
	TITAN_GE_WRITE((0x103c + (port_num << 12)), reg_data_1);

	mdelay(5);

	reg_data_1 &= ~(0x04000000);
	TITAN_GE_WRITE((0x103c + (port_num << 12)), reg_data_1);

	mdelay(5);

	reg_data |= 0x0001;
	TITAN_GE_WRITE((TITAN_GE_TRTG_CONFIG + (port_num << 12)), reg_data);

	/*
	 * Step 4:  Start the Tx activity
	 */
	TITAN_GE_WRITE((TITAN_GE_TMAC_CONFIG_2 + (port_num << 12)), 0xe197);
#ifdef TITAN_GE_JUMBO_FRAMES
	TITAN_GE_WRITE((0x1258 + (port_num << 12)), 0x4000);
#endif
	reg_data = TITAN_GE_READ(TITAN_GE_TMAC_CONFIG_1 + (port_num << 12));
	reg_data |= 0x0001;	/* Enable TMAC */
	reg_data |= 0x6c70;	/* PAUSE also set */

	TITAN_GE_WRITE((TITAN_GE_TMAC_CONFIG_1 + (port_num << 12)), reg_data);

	udelay(30);

	/* Destination Address drop bit */
	reg_data = TITAN_GE_READ(TITAN_GE_RMAC_CONFIG_2 + (port_num << 12));
	reg_data |= 0x218;        /* DA_DROP bit and pause */
	TITAN_GE_WRITE((TITAN_GE_RMAC_CONFIG_2 + (port_num << 12)), reg_data);

	TITAN_GE_WRITE((0x1218 + (port_num << 12)), 0x3);

#ifdef TITAN_GE_JUMBO_FRAMES
	TITAN_GE_WRITE((0x1208 + (port_num << 12)), 0x4000);
#endif
	/* Start the Rx activity */
	reg_data = TITAN_GE_READ(TITAN_GE_RMAC_CONFIG_1 + (port_num << 12));
	reg_data |= 0x0001;	/* RMAC Enable */
	reg_data |= 0x0010;	/* CRC Check enable */
	reg_data |= 0x0040;	/* Min Frame check enable */
	reg_data |= 0x4400;	/* Max Frame check enable */

	TITAN_GE_WRITE((TITAN_GE_RMAC_CONFIG_1 + (port_num << 12)), reg_data);

	udelay(30);

	/*
	 * Enable the Interrupts for Tx and Rx
	 */
	reg_data1 = TITAN_GE_READ(TITAN_GE_INTR_XDMA_IE);

	if (port_num == 0) {
		reg_data1 |= 0x3;
#ifdef CONFIG_SMP
		TITAN_GE_WRITE(0x0038, 0x003);
#else
		TITAN_GE_WRITE(0x0038, 0x303);
#endif
	}

	if (port_num == 1) {
		reg_data1 |= 0x300;
	}

	if (port_num == 2)
		reg_data1 |= 0x30000;

	TITAN_GE_WRITE(TITAN_GE_INTR_XDMA_IE, reg_data1);
	TITAN_GE_WRITE(0x003c, 0x300);

	if (config_done == 0) {
		TITAN_GE_WRITE(0x0024, 0x04000024);	/* IRQ vector */
		TITAN_GE_WRITE(0x0020, 0x000fb000);	/* INTMSG base */
	}

	/* Priority */
	reg_data = TITAN_GE_READ(0x1038 + (port_num << 12));
	reg_data &= ~(0x00f00000);
	TITAN_GE_WRITE((0x1038 + (port_num << 12)), reg_data);

	/* Step 5:  GMII config */
	titan_ge_gmii_config(port_num);

	if (config_done == 0) {
		TITAN_GE_WRITE(0x1a80, 0);
		config_done = 1;
	}

	return TITAN_OK;
}

/*
 * Function to queue the packet for the Ethernet device
 */
static void titan_ge_tx_queue(titan_ge_port_info * titan_ge_eth,
				struct sk_buff * skb)
{
	struct device *device = &titan_ge_device[titan_ge_eth->port_num]->dev;
	unsigned int curr_desc = titan_ge_eth->tx_curr_desc_q;
	volatile titan_ge_tx_desc *tx_curr;
	int port_num = titan_ge_eth->port_num;

	tx_curr = &(titan_ge_eth->tx_desc_area[curr_desc]);
	tx_curr->buffer_addr =
		dma_map_single(device, skb->data, skb_headlen(skb),
			       DMA_TO_DEVICE);

	titan_ge_eth->tx_skb[curr_desc] = (struct sk_buff *) skb;
	tx_curr->buffer_len = skb_headlen(skb);

	/* Last descriptor enables interrupt and changes ownership */
	tx_curr->cmd_sts = 0x1 | (1 << 15) | (1 << 5);

	/* Kick the XDMA to start the transfer from memory to the FIFO */
	TITAN_GE_WRITE((0x5044 + (port_num << 8)), 0x1);

	/* Current descriptor updated */
	titan_ge_eth->tx_curr_desc_q = (curr_desc + 1) % TITAN_GE_TX_QUEUE;

	/* Prefetch the next descriptor */
	prefetch((const void *)
		 &titan_ge_eth->tx_desc_area[titan_ge_eth->tx_curr_desc_q]);
}

/*
 * Actually does the open of the Ethernet device
 */
static int titan_ge_eth_open(struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	unsigned int port_num = titan_ge_eth->port_num;
	struct device *device = &titan_ge_device[port_num]->dev;
	unsigned long reg_data;
	unsigned int phy_reg;
	int err = 0;

	/* Stop the Rx activity */
	reg_data = TITAN_GE_READ(TITAN_GE_RMAC_CONFIG_1 + (port_num << 12));
	reg_data &= ~(0x00000001);
	TITAN_GE_WRITE((TITAN_GE_RMAC_CONFIG_1 + (port_num << 12)), reg_data);

	/* Clear the port interrupts */
	TITAN_GE_WRITE((TITAN_GE_CHANNEL0_INTERRUPT + (port_num << 8)), 0x0);

	if (config_done == 0) {
		TITAN_GE_WRITE(TITAN_GE_INTR_XDMA_CORE_A, 0);
		TITAN_GE_WRITE(TITAN_GE_INTR_XDMA_CORE_B, 0);
	}

	/* Set the MAC Address */
	memcpy(titan_ge_eth->port_mac_addr, netdev->dev_addr, 6);

	if (config_done == 0)
		titan_port_init(netdev, titan_ge_eth);

	titan_ge_update_afx(titan_ge_eth);

	/* Allocate the Tx ring now */
	titan_ge_eth->tx_ring_skbs = 0;
	titan_ge_eth->tx_ring_size = TITAN_GE_TX_QUEUE;

	/* Allocate space in the SRAM for the descriptors */
	titan_ge_eth->tx_desc_area = (titan_ge_tx_desc *)
		(titan_ge_sram + TITAN_TX_RING_BYTES * port_num);
	titan_ge_eth->tx_dma = TITAN_SRAM_BASE + TITAN_TX_RING_BYTES * port_num;

	if (!titan_ge_eth->tx_desc_area) {
		printk(KERN_ERR
		       "%s: Cannot allocate Tx Ring (size %d bytes) for port %d\n",
		       netdev->name, TITAN_TX_RING_BYTES, port_num);
		return -ENOMEM;
	}

	memset(titan_ge_eth->tx_desc_area, 0, titan_ge_eth->tx_desc_area_size);

	/* Now initialize the Tx descriptor ring */
	titan_ge_init_tx_desc_ring(titan_ge_eth,
				   titan_ge_eth->tx_ring_size,
				   (unsigned long) titan_ge_eth->tx_desc_area,
				   (unsigned long) titan_ge_eth->tx_dma);

	/* Allocate the Rx ring now */
	titan_ge_eth->rx_ring_size = TITAN_GE_RX_QUEUE;
	titan_ge_eth->rx_ring_skbs = 0;

	titan_ge_eth->rx_desc_area =
		(titan_ge_rx_desc *)(titan_ge_sram + 0x1000 + TITAN_RX_RING_BYTES * port_num);

	titan_ge_eth->rx_dma = TITAN_SRAM_BASE + 0x1000 + TITAN_RX_RING_BYTES * port_num;

	if (!titan_ge_eth->rx_desc_area) {
		printk(KERN_ERR "%s: Cannot allocate Rx Ring (size %d bytes)\n",
		       netdev->name, TITAN_RX_RING_BYTES);

		printk(KERN_ERR "%s: Freeing previously allocated TX queues...",
		       netdev->name);

		dma_free_coherent(device, titan_ge_eth->tx_desc_area_size,
				    (void *) titan_ge_eth->tx_desc_area,
				    titan_ge_eth->tx_dma);

		return -ENOMEM;
	}

	memset(titan_ge_eth->rx_desc_area, 0, titan_ge_eth->rx_desc_area_size);

	/* Now initialize the Rx ring */
#ifdef TITAN_GE_JUMBO_FRAMES
	if ((titan_ge_init_rx_desc_ring
	    (titan_ge_eth, titan_ge_eth->rx_ring_size, TITAN_GE_JUMBO_BUFSIZE,
	     (unsigned long) titan_ge_eth->rx_desc_area, 0,
	      (unsigned long) titan_ge_eth->rx_dma)) == 0)
#else
	if ((titan_ge_init_rx_desc_ring
	     (titan_ge_eth, titan_ge_eth->rx_ring_size, TITAN_GE_STD_BUFSIZE,
	      (unsigned long) titan_ge_eth->rx_desc_area, 0,
	      (unsigned long) titan_ge_eth->rx_dma)) == 0)
#endif
		panic("%s: Error initializing RX Ring\n", netdev->name);

	/* Fill the Rx ring with the SKBs */
	titan_ge_port_start(netdev, titan_ge_eth);

	/*
	 * Check if Interrupt Coalscing needs to be turned on. The
	 * values specified in the register is multiplied by
	 * (8 x 64 nanoseconds) to determine when an interrupt should
	 * be sent to the CPU.
	 */

	if (TITAN_GE_TX_COAL) {
		titan_ge_eth->tx_int_coal =
		    titan_ge_tx_coal(TITAN_GE_TX_COAL, port_num);
	}

	err = titan_ge_mdio_read(port_num, TITAN_GE_MDIO_PHY_STATUS, &phy_reg);
	if (err == TITAN_GE_MDIO_ERROR) {
		printk(KERN_ERR
		       "Could not read PHY control register 0x11 \n");
		return TITAN_ERROR;
	}
	if (!(phy_reg & 0x0400)) {
		netif_carrier_off(netdev);
		netif_stop_queue(netdev);
		return TITAN_ERROR;
	} else {
		netif_carrier_on(netdev);
		netif_start_queue(netdev);
	}

	return TITAN_OK;
}

/*
 * Queue the packet for Tx. Currently no support for zero copy,
 * checksum offload and Scatter Gather. The chip does support
 * Scatter Gather only. But, that wont help here since zero copy
 * requires support for Tx checksumming also.
 */
int titan_ge_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	unsigned long flags;
	struct net_device_stats *stats;
//printk("titan_ge_start_xmit\n");

	stats = &titan_ge_eth->stats;
	spin_lock_irqsave(&titan_ge_eth->lock, flags);

	if ((TITAN_GE_TX_QUEUE - titan_ge_eth->tx_ring_skbs) <=
	    (skb_shinfo(skb)->nr_frags + 1)) {
		netif_stop_queue(netdev);
		spin_unlock_irqrestore(&titan_ge_eth->lock, flags);
		printk(KERN_ERR "Tx OOD \n");
		return 1;
	}

	titan_ge_tx_queue(titan_ge_eth, skb);
	titan_ge_eth->tx_ring_skbs++;

	if (TITAN_GE_TX_QUEUE <= (titan_ge_eth->tx_ring_skbs + 4)) {
		spin_unlock_irqrestore(&titan_ge_eth->lock, flags);
		titan_ge_free_tx_queue(titan_ge_eth);
		spin_lock_irqsave(&titan_ge_eth->lock, flags);
	}

	stats->tx_bytes += skb->len;
	stats->tx_packets++;

	spin_unlock_irqrestore(&titan_ge_eth->lock, flags);

	netdev->trans_start = jiffies;

	return 0;
}

/*
 * Actually does the Rx. Rx side checksumming supported.
 */
static int titan_ge_rx(struct net_device *netdev, int port_num,
			titan_ge_port_info * titan_ge_port,
		       titan_ge_packet * packet)
{
	int rx_curr_desc, rx_used_desc;
	volatile titan_ge_rx_desc *rx_desc;

	rx_curr_desc = titan_ge_port->rx_curr_desc_q;
	rx_used_desc = titan_ge_port->rx_used_desc_q;

	if (((rx_curr_desc + 1) % TITAN_GE_RX_QUEUE) == rx_used_desc)
		return TITAN_ERROR;

	rx_desc = &(titan_ge_port->rx_desc_area[rx_curr_desc]);

	if (rx_desc->cmd_sts & TITAN_GE_RX_BUFFER_OWNED)
		return TITAN_ERROR;

	packet->skb = titan_ge_port->rx_skb[rx_curr_desc];
	packet->len = (rx_desc->cmd_sts & 0x7fff);

	/*
	 * At this point, we dont know if the checksumming
	 * actually helps relieve CPU. So, keep it for
	 * port 0 only
	 */
	packet->checksum = ntohs((rx_desc->buffer & 0xffff0000) >> 16);
	packet->cmd_sts = rx_desc->cmd_sts;

	titan_ge_port->rx_curr_desc_q = (rx_curr_desc + 1) % TITAN_GE_RX_QUEUE;

	/* Prefetch the next descriptor */
	prefetch((const void *)
	       &titan_ge_port->rx_desc_area[titan_ge_port->rx_curr_desc_q + 1]);

	return TITAN_OK;
}

/*
 * Free the Tx queue of the used SKBs
 */
static int titan_ge_free_tx_queue(titan_ge_port_info *titan_ge_eth)
{
	unsigned long flags;

	/* Take the lock */
	spin_lock_irqsave(&(titan_ge_eth->lock), flags);

	while (titan_ge_return_tx_desc(titan_ge_eth, titan_ge_eth->port_num) == 0)
		if (titan_ge_eth->tx_ring_skbs != 1)
			titan_ge_eth->tx_ring_skbs--;

	spin_unlock_irqrestore(&titan_ge_eth->lock, flags);

	return TITAN_OK;
}

/*
 * Threshold beyond which we do the cleaning of
 * Tx queue and new allocation for the Rx
 * queue
 */
#define	TX_THRESHOLD	4
#define	RX_THRESHOLD	10

/*
 * Receive the packets and send it to the kernel.
 */
static int titan_ge_receive_queue(struct net_device *netdev, unsigned int max)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	unsigned int port_num = titan_ge_eth->port_num;
	titan_ge_packet packet;
	struct net_device_stats *stats;
	struct sk_buff *skb;
	unsigned long received_packets = 0;
	unsigned int ack;

	stats = &titan_ge_eth->stats;

	while ((--max)
	       && (titan_ge_rx(netdev, port_num, titan_ge_eth, &packet) == TITAN_OK)) {
		skb = (struct sk_buff *) packet.skb;

		titan_ge_eth->rx_ring_skbs--;

		if (--titan_ge_eth->rx_work_limit < 0)
			break;
		received_packets++;

		stats->rx_packets++;
		stats->rx_bytes += packet.len;

		if ((packet.cmd_sts & TITAN_GE_RX_PERR) ||
			(packet.cmd_sts & TITAN_GE_RX_OVERFLOW_ERROR) ||
			(packet.cmd_sts & TITAN_GE_RX_TRUNC) ||
			(packet.cmd_sts & TITAN_GE_RX_CRC_ERROR)) {
				stats->rx_dropped++;
				dev_kfree_skb_any(skb);

				continue;
		}
		/*
		 * Either support fast path or slow path. Decision
		 * making can really slow down the performance. The
		 * idea is to cut down the number of checks and improve
		 * the fastpath.
		 */

		skb_put(skb, packet.len - 2);

		/*
		 * Increment data pointer by two since thats where
		 * the MAC starts
		 */
		skb_reserve(skb, 2);
		skb->protocol = eth_type_trans(skb, netdev);
		netif_receive_skb(skb);

		if (titan_ge_eth->rx_threshold > RX_THRESHOLD) {
			ack = titan_ge_rx_task(netdev, titan_ge_eth);
			TITAN_GE_WRITE((0x5048 + (port_num << 8)), ack);
			titan_ge_eth->rx_threshold = 0;
		} else
			titan_ge_eth->rx_threshold++;

		if (titan_ge_eth->tx_threshold > TX_THRESHOLD) {
			titan_ge_eth->tx_threshold = 0;
			titan_ge_free_tx_queue(titan_ge_eth);
		}
		else
			titan_ge_eth->tx_threshold++;

	}
	return received_packets;
}


/*
 * Enable the Rx side interrupts
 */
static void titan_ge_enable_int(unsigned int port_num,
			titan_ge_port_info *titan_ge_eth,
			struct net_device *netdev)
{
	unsigned long reg_data = TITAN_GE_READ(TITAN_GE_INTR_XDMA_IE);

	if (port_num == 0)
		reg_data |= 0x3;
	if (port_num == 1)
		reg_data |= 0x300;
	if (port_num == 2)
		reg_data |= 0x30000;

	/* Re-enable interrupts */
	TITAN_GE_WRITE(TITAN_GE_INTR_XDMA_IE, reg_data);
}

/*
 * Main function to handle the polling for Rx side NAPI.
 * Receive interrupts have been disabled at this point.
 * The poll schedules the transmit followed by receive.
 */
static int titan_ge_poll(struct net_device *netdev, int *budget)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	int port_num = titan_ge_eth->port_num;
	int work_done = 0;
	unsigned long flags, status;

	titan_ge_eth->rx_work_limit = *budget;
	if (titan_ge_eth->rx_work_limit > netdev->quota)
		titan_ge_eth->rx_work_limit = netdev->quota;

	do {
		/* Do the transmit cleaning work here */
		titan_ge_free_tx_queue(titan_ge_eth);

		/* Ack the Rx interrupts */
		if (port_num == 0)
			TITAN_GE_WRITE(TITAN_GE_INTR_XDMA_CORE_A, 0x3);
		if (port_num == 1)
			TITAN_GE_WRITE(TITAN_GE_INTR_XDMA_CORE_A, 0x300);
		if (port_num == 2)
			TITAN_GE_WRITE(TITAN_GE_INTR_XDMA_CORE_A, 0x30000);

		work_done += titan_ge_receive_queue(netdev, 0);

		/* Out of quota and there is work to be done */
		if (titan_ge_eth->rx_work_limit < 0)
			goto not_done;

		/* Receive alloc_skb could lead to OOM */
		if (oom_flag == 1) {
			oom_flag = 0;
			goto oom;
		}

		status = TITAN_GE_READ(TITAN_GE_INTR_XDMA_CORE_A);
	} while (status & 0x30300);

	/* If we are here, then no more interrupts to process */
	goto done;

not_done:
	*budget -= work_done;
	netdev->quota -= work_done;
	return 1;

oom:
	printk(KERN_ERR "OOM \n");
	netif_rx_complete(netdev);
	return 0;

done:
	/*
	 * No more packets on the poll list. Turn the interrupts
	 * back on and we should be able to catch the new
	 * packets in the interrupt handler
	 */
	if (!work_done)
		work_done = 1;

	*budget -= work_done;
	netdev->quota -= work_done;

	spin_lock_irqsave(&titan_ge_eth->lock, flags);

	/* Remove us from the poll list */
	netif_rx_complete(netdev);

	/* Re-enable interrupts */
	titan_ge_enable_int(port_num, titan_ge_eth, netdev);

	spin_unlock_irqrestore(&titan_ge_eth->lock, flags);

	return 0;
}

/*
 * Close the network device
 */
int titan_ge_stop(struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);

	spin_lock_irq(&(titan_ge_eth->lock));
	titan_ge_eth_stop(netdev);
	free_irq(netdev->irq, netdev);
	spin_unlock_irq(&titan_ge_eth->lock);

	return TITAN_OK;
}

/*
 * Free the Tx ring
 */
static void titan_ge_free_tx_rings(struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	unsigned int port_num = titan_ge_eth->port_num;
	unsigned int curr;
	unsigned long reg_data;

	/* Stop the Tx DMA */
	reg_data = TITAN_GE_READ(TITAN_GE_CHANNEL0_CONFIG +
				(port_num << 8));
	reg_data |= 0xc0000000;
	TITAN_GE_WRITE((TITAN_GE_CHANNEL0_CONFIG +
			(port_num << 8)), reg_data);

	/* Disable the TMAC */
	reg_data = TITAN_GE_READ(TITAN_GE_TMAC_CONFIG_1 +
				(port_num << 12));
	reg_data &= ~(0x00000001);
	TITAN_GE_WRITE((TITAN_GE_TMAC_CONFIG_1 +
			(port_num << 12)), reg_data);

	for (curr = 0;
	     (titan_ge_eth->tx_ring_skbs) && (curr < TITAN_GE_TX_QUEUE);
	     curr++) {
		if (titan_ge_eth->tx_skb[curr]) {
			dev_kfree_skb(titan_ge_eth->tx_skb[curr]);
			titan_ge_eth->tx_ring_skbs--;
		}
	}

	if (titan_ge_eth->tx_ring_skbs != 0)
		printk
		    ("%s: Error on Tx descriptor free - could not free %d"
		     " descriptors\n", netdev->name,
		     titan_ge_eth->tx_ring_skbs);

#ifndef TITAN_RX_RING_IN_SRAM
	dma_free_coherent(&titan_ge_device[port_num]->dev,
			  titan_ge_eth->tx_desc_area_size,
			  (void *) titan_ge_eth->tx_desc_area,
			  titan_ge_eth->tx_dma);
#endif
}

/*
 * Free the Rx ring
 */
static void titan_ge_free_rx_rings(struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	unsigned int port_num = titan_ge_eth->port_num;
	unsigned int curr;
	unsigned long reg_data;

	/* Stop the Rx DMA */
	reg_data = TITAN_GE_READ(TITAN_GE_CHANNEL0_CONFIG +
				(port_num << 8));
	reg_data |= 0x000c0000;
	TITAN_GE_WRITE((TITAN_GE_CHANNEL0_CONFIG +
			(port_num << 8)), reg_data);

	/* Disable the RMAC */
	reg_data = TITAN_GE_READ(TITAN_GE_RMAC_CONFIG_1 +
				(port_num << 12));
	reg_data &= ~(0x00000001);
	TITAN_GE_WRITE((TITAN_GE_RMAC_CONFIG_1 +
			(port_num << 12)), reg_data);

	for (curr = 0;
	     titan_ge_eth->rx_ring_skbs && (curr < TITAN_GE_RX_QUEUE);
	     curr++) {
		if (titan_ge_eth->rx_skb[curr]) {
			dev_kfree_skb(titan_ge_eth->rx_skb[curr]);
			titan_ge_eth->rx_ring_skbs--;
		}
	}

	if (titan_ge_eth->rx_ring_skbs != 0)
		printk(KERN_ERR
		       "%s: Error in freeing Rx Ring. %d skb's still"
		       " stuck in RX Ring - ignoring them\n", netdev->name,
		       titan_ge_eth->rx_ring_skbs);

#ifndef TITAN_RX_RING_IN_SRAM
	dma_free_coherent(&titan_ge_device[port_num]->dev,
			  titan_ge_eth->rx_desc_area_size,
			  (void *) titan_ge_eth->rx_desc_area,
			  titan_ge_eth->rx_dma);
#endif
}

/*
 * Actually does the stop of the Ethernet device
 */
static void titan_ge_eth_stop(struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);

	netif_stop_queue(netdev);

	titan_ge_port_reset(titan_ge_eth->port_num);

	titan_ge_free_tx_rings(netdev);
	titan_ge_free_rx_rings(netdev);

	/* Disable the Tx and Rx Interrupts for all channels */
	TITAN_GE_WRITE(TITAN_GE_INTR_XDMA_IE, 0x0);
}

/*
 * Update the MAC address. Note that we have to write the
 * address in three station registers, 16 bits each. And this
 * has to be done for TMAC and RMAC
 */
static void titan_ge_update_mac_address(struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);
	unsigned int port_num = titan_ge_eth->port_num;
	u8 p_addr[6];

	memcpy(titan_ge_eth->port_mac_addr, netdev->dev_addr, 6);
	memcpy(p_addr, netdev->dev_addr, 6);

	/* Update the Address Filtering Match tables */
	titan_ge_update_afx(titan_ge_eth);

	printk("Station MAC : %d %d %d %d %d %d  \n",
		p_addr[5], p_addr[4], p_addr[3],
		p_addr[2], p_addr[1], p_addr[0]);

	/* Set the MAC address here for TMAC and RMAC */
	TITAN_GE_WRITE((TITAN_GE_TMAC_STATION_HI + (port_num << 12)),
		       ((p_addr[5] << 8) | p_addr[4]));
	TITAN_GE_WRITE((TITAN_GE_TMAC_STATION_MID + (port_num << 12)),
		       ((p_addr[3] << 8) | p_addr[2]));
	TITAN_GE_WRITE((TITAN_GE_TMAC_STATION_LOW + (port_num << 12)),
		       ((p_addr[1] << 8) | p_addr[0]));

	TITAN_GE_WRITE((TITAN_GE_RMAC_STATION_HI + (port_num << 12)),
		       ((p_addr[5] << 8) | p_addr[4]));
	TITAN_GE_WRITE((TITAN_GE_RMAC_STATION_MID + (port_num << 12)),
		       ((p_addr[3] << 8) | p_addr[2]));
	TITAN_GE_WRITE((TITAN_GE_RMAC_STATION_LOW + (port_num << 12)),
		       ((p_addr[1] << 8) | p_addr[0]));
}

/*
 * Set the MAC address of the Ethernet device
 */
static int titan_ge_set_mac_address(struct net_device *dev, void *addr)
{
	titan_ge_port_info *tp = netdev_priv(dev);
	struct sockaddr *sa = addr;

	memcpy(dev->dev_addr, sa->sa_data, dev->addr_len);

	spin_lock_irq(&tp->lock);
	titan_ge_update_mac_address(dev);
	spin_unlock_irq(&tp->lock);

	return 0;
}

/*
 * Get the Ethernet device stats
 */
static struct net_device_stats *titan_ge_get_stats(struct net_device *netdev)
{
	titan_ge_port_info *titan_ge_eth = netdev_priv(netdev);

	return &titan_ge_eth->stats;
}

/*
 * Initialize the Rx descriptor ring for the Titan Ge
 */
static int titan_ge_init_rx_desc_ring(titan_ge_port_info * titan_eth_port,
				      int rx_desc_num,
				      int rx_buff_size,
				      unsigned long rx_desc_base_addr,
				      unsigned long rx_buff_base_addr,
				      unsigned long rx_dma)
{
	volatile titan_ge_rx_desc *rx_desc;
	unsigned long buffer_addr;
	int index;
	unsigned long titan_ge_rx_desc_bus = rx_dma;

	buffer_addr = rx_buff_base_addr;
	rx_desc = (titan_ge_rx_desc *) rx_desc_base_addr;

	/* Check alignment */
	if (rx_buff_base_addr & 0xF)
		return 0;

	/* Check Rx buffer size */
	if ((rx_buff_size < 8) || (rx_buff_size > TITAN_GE_MAX_RX_BUFFER))
		return 0;

	/* 64-bit alignment
	if ((rx_buff_base_addr + rx_buff_size) & 0x7)
		return 0; */

	/* Initialize the Rx desc ring */
	for (index = 0; index < rx_desc_num; index++) {
		titan_ge_rx_desc_bus += sizeof(titan_ge_rx_desc);
		rx_desc[index].cmd_sts = 0;
		rx_desc[index].buffer_addr = buffer_addr;
		titan_eth_port->rx_skb[index] = NULL;
		buffer_addr += rx_buff_size;
	}

	titan_eth_port->rx_curr_desc_q = 0;
	titan_eth_port->rx_used_desc_q = 0;

	titan_eth_port->rx_desc_area = (titan_ge_rx_desc *) rx_desc_base_addr;
	titan_eth_port->rx_desc_area_size =
	    rx_desc_num * sizeof(titan_ge_rx_desc);

	titan_eth_port->rx_dma = rx_dma;

	return TITAN_OK;
}

/*
 * Initialize the Tx descriptor ring. Descriptors in the SRAM
 */
static int titan_ge_init_tx_desc_ring(titan_ge_port_info * titan_ge_port,
				      int tx_desc_num,
				      unsigned long tx_desc_base_addr,
				      unsigned long tx_dma)
{
	titan_ge_tx_desc *tx_desc;
	int index;
	unsigned long titan_ge_tx_desc_bus = tx_dma;

	if (tx_desc_base_addr & 0xF)
		return 0;

	tx_desc = (titan_ge_tx_desc *) tx_desc_base_addr;

	for (index = 0; index < tx_desc_num; index++) {
		titan_ge_port->tx_dma_array[index] =
		    (dma_addr_t) titan_ge_tx_desc_bus;
		titan_ge_tx_desc_bus += sizeof(titan_ge_tx_desc);
		tx_desc[index].cmd_sts = 0x0000;
		tx_desc[index].buffer_len = 0;
		tx_desc[index].buffer_addr = 0x00000000;
		titan_ge_port->tx_skb[index] = NULL;
	}

	titan_ge_port->tx_curr_desc_q = 0;
	titan_ge_port->tx_used_desc_q = 0;

	titan_ge_port->tx_desc_area = (titan_ge_tx_desc *) tx_desc_base_addr;
	titan_ge_port->tx_desc_area_size =
	    tx_desc_num * sizeof(titan_ge_tx_desc);

	titan_ge_port->tx_dma = tx_dma;
	return TITAN_OK;
}

/*
 * Initialize the device as an Ethernet device
 */
static int __init titan_ge_probe(struct device *device)
{
	titan_ge_port_info *titan_ge_eth;
	struct net_device *netdev;
	int port = to_platform_device(device)->id;
	int err;

	netdev = alloc_etherdev(sizeof(titan_ge_port_info));
	if (!netdev) {
		err = -ENODEV;
		goto out;
	}

	netdev->open = titan_ge_open;
	netdev->stop = titan_ge_stop;
	netdev->hard_start_xmit = titan_ge_start_xmit;
	netdev->get_stats = titan_ge_get_stats;
	netdev->set_multicast_list = titan_ge_set_multi;
	netdev->set_mac_address = titan_ge_set_mac_address;

	/* Tx timeout */
	netdev->tx_timeout = titan_ge_tx_timeout;
	netdev->watchdog_timeo = 2 * HZ;

	/* Set these to very high values */
	netdev->poll = titan_ge_poll;
	netdev->weight = 64;

	netdev->tx_queue_len = TITAN_GE_TX_QUEUE;
	netif_carrier_off(netdev);
	netdev->base_addr = 0;

	netdev->change_mtu = titan_ge_change_mtu;

	titan_ge_eth = netdev_priv(netdev);
	/* Allocation of memory for the driver structures */

	titan_ge_eth->port_num = port;

	/* Configure the Tx timeout handler */
	INIT_WORK(&titan_ge_eth->tx_timeout_task,
		  (void (*)(void *)) titan_ge_tx_timeout_task, netdev);

	spin_lock_init(&titan_ge_eth->lock);

	/* set MAC addresses */
	memcpy(netdev->dev_addr, titan_ge_mac_addr_base, 6);
	netdev->dev_addr[5] += port;

	err = register_netdev(netdev);

	if (err)
		goto out_free_netdev;

	printk(KERN_NOTICE
	       "%s: port %d with MAC address %02x:%02x:%02x:%02x:%02x:%02x\n",
	       netdev->name, port, netdev->dev_addr[0],
	       netdev->dev_addr[1], netdev->dev_addr[2],
	       netdev->dev_addr[3], netdev->dev_addr[4],
	       netdev->dev_addr[5]);

	printk(KERN_NOTICE "Rx NAPI supported, Tx Coalescing ON \n");

	return 0;

out_free_netdev:
	kfree(netdev);

out:
	return err;
}

static void __devexit titan_device_remove(struct device *device)
{
}

/*
 * Reset the Ethernet port
 */
static void titan_ge_port_reset(unsigned int port_num)
{
	unsigned int reg_data;

	/* Stop the Tx port activity */
	reg_data = TITAN_GE_READ(TITAN_GE_TMAC_CONFIG_1 +
				(port_num << 12));
	reg_data &= ~(0x0001);
	TITAN_GE_WRITE((TITAN_GE_TMAC_CONFIG_1 +
			(port_num << 12)), reg_data);

	/* Stop the Rx port activity */
	reg_data = TITAN_GE_READ(TITAN_GE_RMAC_CONFIG_1 +
				(port_num << 12));
	reg_data &= ~(0x0001);
	TITAN_GE_WRITE((TITAN_GE_RMAC_CONFIG_1 +
			(port_num << 12)), reg_data);

	return;
}

/*
 * Return the Tx desc after use by the XDMA
 */
static int titan_ge_return_tx_desc(titan_ge_port_info * titan_ge_eth, int port)
{
	int tx_desc_used;
	struct sk_buff *skb;

	tx_desc_used = titan_ge_eth->tx_used_desc_q;

	/* return right away */
	if (tx_desc_used == titan_ge_eth->tx_curr_desc_q)
		return TITAN_ERROR;

	/* Now the critical stuff */
	skb = titan_ge_eth->tx_skb[tx_desc_used];

	dev_kfree_skb_any(skb);

	titan_ge_eth->tx_skb[tx_desc_used] = NULL;
	titan_ge_eth->tx_used_desc_q =
	    (tx_desc_used + 1) % TITAN_GE_TX_QUEUE;

	return 0;
}

/*
 * Coalescing for the Tx path
 */
static unsigned long titan_ge_tx_coal(unsigned long delay, int port)
{
	unsigned long rx_delay;

	rx_delay = TITAN_GE_READ(TITAN_GE_INT_COALESCING);
	delay = (delay << 16) | rx_delay;

	TITAN_GE_WRITE(TITAN_GE_INT_COALESCING, delay);
	TITAN_GE_WRITE(0x5038, delay);

	return delay;
}

static struct device_driver titan_soc_driver = {
	.name   = titan_string,
	.bus    = &platform_bus_type,
	.probe  = titan_ge_probe,
	.remove = __devexit_p(titan_device_remove),
};

static void titan_platform_release (struct device *device)
{
	struct platform_device *pldev;

	/* free device */
	pldev = to_platform_device (device);
	kfree (pldev);
}

/*
 * Register the Titan GE with the kernel
 */
static int __init titan_ge_init_module(void)
{
	struct platform_device *pldev;
	unsigned int version, device;
	int i;

	printk(KERN_NOTICE
	       "PMC-Sierra TITAN 10/100/1000 Ethernet Driver \n");

	titan_ge_base = (unsigned long) ioremap(TITAN_GE_BASE, TITAN_GE_SIZE);
	if (!titan_ge_base) {
		printk("Mapping Titan GE failed\n");
		goto out;
	}

	device = TITAN_GE_READ(TITAN_GE_DEVICE_ID);
	version = (device & 0x000f0000) >> 16;
	device &= 0x0000ffff;

	printk(KERN_NOTICE "Device Id : %x,  Version : %x \n", device, version);

#ifdef TITAN_RX_RING_IN_SRAM
	titan_ge_sram = (unsigned long) ioremap(TITAN_SRAM_BASE,
						TITAN_SRAM_SIZE);
	if (!titan_ge_sram) {
		printk("Mapping Titan SRAM failed\n");
		goto out_unmap_ge;
	}
#endif

	if (driver_register(&titan_soc_driver)) {
		printk(KERN_ERR "Driver registration failed\n");
		goto out_unmap_sram;
	}

	for (i = 0; i < 3; i++) {
		titan_ge_device[i] = NULL;

		if (!(pldev = kmalloc (sizeof (*pldev), GFP_KERNEL)))
			continue;

		memset (pldev, 0, sizeof (*pldev));
		pldev->name		= titan_string;
		pldev->id		= i;
		pldev->dev.release	= titan_platform_release;
		titan_ge_device[i]	= pldev;

		if (platform_device_register (pldev)) {
			kfree (pldev);
			titan_ge_device[i] = NULL;
			continue;
		}

		if (!pldev->dev.driver) {
			/*
			 * The driver was not bound to this device, there was
			 * no hardware at this address. Unregister it, as the
			 * release fuction will take care of freeing the
			 * allocated structure
			 */
			titan_ge_device[i] = NULL;
			platform_device_unregister (pldev);
		}
	}

	return 0;

out_unmap_sram:
	iounmap((void *)titan_ge_sram);

out_unmap_ge:
	iounmap((void *)titan_ge_base);

out:
	return -ENOMEM;
}

/*
 * Unregister the Titan GE from the kernel
 */
static void __exit titan_ge_cleanup_module(void)
{
	int i;

	driver_unregister(&titan_soc_driver);

	for (i = 0; i < 3; i++) {
		if (titan_ge_device[i]) {
			platform_device_unregister (titan_ge_device[i]);
			titan_ge_device[i] = NULL;
		}
	}

	iounmap((void *)titan_ge_sram);
	iounmap((void *)titan_ge_base);
}

MODULE_AUTHOR("Manish Lachwani <lachwani@pmc-sierra.com>");
MODULE_DESCRIPTION("Titan GE Ethernet driver");
MODULE_LICENSE("GPL");

module_init(titan_ge_init_module);
module_exit(titan_ge_cleanup_module);
