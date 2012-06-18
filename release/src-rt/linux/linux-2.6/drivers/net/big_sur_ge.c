/*
 * drivers/net/big_sur_ge.c - Driver for PMC-Sierra Big Sur ethernet ports
 *
 * Copyright (C) 2003 PMC-Sierra Inc.
 * Author : Manish Lachwani (lachwani@pmc-sierra.com)
 * Copyright (C) 2003 Ralf Baechle (ralf@linux-mips.org)
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
 *
 */

/*************************************************************************
 * Description :
 *
 * The driver has three modes of operation: FIFO non-DMA, Simple DMA
 * and SG DMA. There is also a Polled mode and an Interrupt mode of
 * operation. SG DMA should do zerocopy and check offload. Probably,
 * zerocopy on the Rx might also work. Simple DMA is the non-zerocpy
 * case on the Tx and the Rx.
 *
 * We turn on Simple DMA and interrupt mode. Although, support has been
 * added for the SG mode also but not for the polled mode. This is a
 * Fast Ethernet driver although there will be support for Gigabit soon.
 *
 * The driver is divided into two parts: Hardware dependent and a
 * Hardware independent. There is currently no support for checksum offload
 * zerocopy and Rx NAPI. There is support for Interrupt Mitigation.
 ****************************************************************************/

/*************************************************************
 * Hardware Indepenent Part of the driver
 *************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/mii.h>
#include <asm/io.h>
#include <asm/irq.h>

#include "big_sur_ge.h"

#define TX_TIMEOUT (60*HZ)	/* Transmission timeout is 60 seconds. */

static struct net_device *dev_list = NULL;
static DEFINE_SPINLOCK(dev_lock);

typedef enum DUPLEX { UNKNOWN, HALF_DUPLEX, FULL_DUPLEX } DUPLEX;

/* Big Sur Ethernet MAC structure */
struct big_sur_ge_enet {
	struct net_device_stats stats;	/* Statistics for this device */
	struct net_device *next_dev;	/* The next device in dev_list */
	struct timer_list phy_timer;	/* PHY monitoring timer */
	u32 index;		/* Which interface is this */
	u32 save_base_address;	/* Saved physical base address */
	struct sk_buff *saved_skb;	/* skb being transmitted */
	spinlock_t lock;	/* For atomic access to saved_skb */
	u8 mii_addr;		/* The MII address of the PHY */
	big_sur_ge emac;	/* GE driver structure */
};

/* Manish : For testing purposes only */
static unsigned char big_sur_mac_addr_base[6] = "00:11:22:33:44:55";

/*********************************************************************
 * Function Prototypes (whole bunch of them)
 *********************************************************************/
unsigned long big_sur_ge_dma_control(xdma_channel *);
void big_sur_ge_dma_reset(xdma_channel *);
static void handle_fifo_intr(big_sur_ge *);
void big_sur_ge_check_fifo_recv_error(big_sur_ge *);
void big_sur_ge_check_fifo_send_error(big_sur_ge *);
static int big_sur_ge_config_fifo(big_sur_ge *);
big_sur_ge_config *big_sur_ge_lookup_config(unsigned int);
static int big_sur_ge_config_dma(big_sur_ge *);
void big_sur_ge_enet_reset(big_sur_ge *);
void big_sur_ge_check_mac_error(big_sur_ge *, unsigned long);

/*********************************************************************
 * DMA Channel Initialization
 **********************************************************************/
static int big_sur_ge_dma_init(xdma_channel * dma, unsigned long base_address)
{
	dma->reg_base_address = base_address;
	dma->get_ptr = NULL;
	dma->put_ptr = NULL;
	dma->commit_ptr = NULL;
	dma->last_ptr = NULL;
	dma->total_desc_count = (unsigned long) NULL;
	dma->active_desc_count = (unsigned long) NULL;
	dma->ready = 1;		/* DMA channel is ready */

	big_sur_ge_dma_reset(dma);

	return 0;
}

/*********************************************************************
 * Is the DMA channel ready yet ?
 **********************************************************************/
static int big_sur_ge_dma_ready(xdma_channel * dma)
{
	return dma->ready == 1;
}

/*********************************************************************
 * Perform the self test on the DMA channel
 **********************************************************************/
#define BIG_SUR_GE_CONTROL_REG_RESET_MASK	0x98000000

static int big_sur_ge_dma_self_test(xdma_channel * dma)
{
	unsigned long reg_data;

	big_sur_ge_dma_reset(dma);

	reg_data = big_sur_ge_dma_control(dma);
	if (reg_data != BIG_SUR_GE_CONTROL_REG_RESET_MASK) {
		printk(KERN_ERR "DMA Channel Self Test Failed \n");
		return -1;
	}

	return 0;
}

/*********************************************************************
 * Reset the DMA channel
 **********************************************************************/
static void big_sur_ge_dma_reset(xdma_channel * dma)
{
	BIG_SUR_GE_WRITE(dma->reg_base_address + BIG_SUR_GE_RST_REG_OFFSET,
			 BIG_SUR_GE_RESET_MASK);
}

/*********************************************************************
 * Get control of the DMA channel
 **********************************************************************/
static unsigned long big_sur_ge_dma_control(xdma_channel * dma)
{
	return BIG_SUR_GE_READ(dma->reg_base_address +
			       BIG_SUR_GE_DMAC_REG_OFFSET);
}

/*********************************************************************
 * Set control of the DMA channel
 **********************************************************************/
static void big_sur_ge_set_dma_control(xdma_channel * dma, unsigned long control)
{
	BIG_SUR_GE_WRITE(dma->reg_base_address +
			 BIG_SUR_GE_DMAC_REG_OFFSET, control);
}

/*********************************************************************
 * Get the status of the DMA channel
 *********************************************************************/
static unsigned long big_sur_ge_dma_status(xdma_channel * dma)
{
	return BIG_SUR_GE_READ(dma->reg_base_address +
			       BIG_SUR_GE_DMAS_REG_OFFSET);
}

/*********************************************************************
 * Set the interrupt status of the DMA channel
 *********************************************************************/
static void big_sur_ge_set_intr_status(xdma_channel * dma, unsigned long status)
{
	BIG_SUR_GE_WRITE(dma->reg_base_address + BIG_SUR_GE_IS_REG_OFFSET,
			 status);
}

/*********************************************************************
 * Get the interrupt status of the DMA channel
 *********************************************************************/
static unsigned long big_sur_ge_get_intr_status(xdma_channel * dma)
{
	return BIG_SUR_GE_READ(dma->reg_base_address +
			       BIG_SUR_GE_IS_REG_OFFSET);
}

/*********************************************************************
 * Set the Interrupt Enable
 *********************************************************************/
static void big_sur_ge_set_intr_enable(xdma_channel * dma, unsigned long enable)
{
	BIG_SUR_GE_WRITE(dma->reg_base_address + BIG_SUR_GE_IE_REG_OFFSET,
			 enable);
}

/*********************************************************************
 * Get the Interrupt Enable field to make a check
 *********************************************************************/
static unsigned long big_sur_ge_get_intr_enable(xdma_channel * dma)
{
	return BIG_SUR_GE_READ(dma->reg_base_address +
			       BIG_SUR_GE_IE_REG_OFFSET);
}

/*********************************************************************
 * Transfer the data over the DMA channel
 *********************************************************************/
static void big_sur_ge_dma_transfer(xdma_channel * dma, unsigned long *source,
			     unsigned long *dest, unsigned long length)
{
	BIG_SUR_GE_WRITE(dma->reg_base_address + BIG_SUR_GE_SA_REG_OFFSET,
			 (unsigned long) source);

	BIG_SUR_GE_WRITE(dma->reg_base_address + BIG_SUR_GE_DA_REG_OFFSET,
			 (unsigned long) dest);

	BIG_SUR_GE_WRITE(dma->reg_base_address + BIG_SUR_GE_LEN_REG_OFFSET,
			 length);
}

/*********************************************************************
 * Get the DMA descriptor
 *********************************************************************/
static int big_sur_ge_get_descriptor(xdma_channel * dma,
			      xbuf_descriptor ** buffer_desc)
{
	unsigned long reg_data;

	reg_data = xbuf_descriptor_GetControl(dma->get_ptr);
	xbuf_descriptor_SetControl(dma->get_ptr, reg_data |
				   BIG_SUR_GE_DMACR_SG_DISABLE_MASK);

	*buffer_desc = dma->get_ptr;

	dma->get_ptr = xbuf_descriptor_GetNextPtr(dma->get_ptr);
	dma->active_desc_count--;

	return 0;
}

/*********************************************************************
 * Get the packet count
 *********************************************************************/
static int big_sur_ge_get_packet_count(xdma_channel * dma)
{
	return (BIG_SUR_GE_READ
		(dma->reg_base_address + BIG_SUR_GE_UPC_REG_OFFSET));
}

/*********************************************************************
 * Descrement the packet count
 *********************************************************************/
static void big_sur_ge_decr_packet_count(xdma_channel * dma)
{
	unsigned long reg_data;

	reg_data =
	    BIG_SUR_GE_READ(dma->base_address + BIG_SUR_GE_UPC_REG_OFFSET);
	if (reg_data > 0)
		BIG_SUR_GE_WRITE(dma->base_address +
				 BIG_SUR_GE_UPC_REG_OFFSET, 1);
}

/****************************************************************************
 * Start of the code that deals with the Packet Fifo
 *****************************************************************************/

/****************************************************************************
 * Init the packet fifo
 ****************************************************************************/
static int packet_fifo_init(packet_fifo * fifo, u32 reg, u32 data)
{
	fifo->reg_base_addr = reg;
	fifo->data_base_address = data;
	fifo->ready_status = 1;

	BIG_SUR_GE_FIFO_RESET(fifo);

	return 0;
}

/****************************************************************************
 * Packet fifo self test
 ****************************************************************************/
static int packet_fifo_self_test(packet_fifo * fifo, unsigned long type)
{
	unsigned long reg_data;

	BIG_SUR_GE_FIFO_RESET(fifo);
	reg_data =
	    BIG_SUR_GE_READ(fifo->reg_base_addr +
			    BIG_SUR_GE_COUNT_STATUS_REG_OFFSET);

	if (type == BIG_SUR_GE_READ_FIFO_TYPE) {
		if (reg_data != BIG_SUR_GE_EMPTY_FULL_MASK) {
			printk(KERN_ERR "Read FIFO not empty \n");
			return -1;
		}
	} else if (!(reg_data & BIG_SUR_GE_EMPTY_FULL_MASK)) {
		printk(KERN_ERR "Write FIFO is full \n");
		return -1;
	}

	return 0;
}

/****************************************************************************
 * Packet FIFO read
 ****************************************************************************/
static int packet_fifo_read(packet_fifo * fifo, u8 * buffer, unsigned int len)
{
	unsigned long fifo_count, word_count, extra_byte;
	unsigned long *buffer_data = (unsigned long *) buffer;

	fifo_count =
	    BIG_SUR_GE_READ(fifo->reg_base_addr +
			    BIG_SUR_GE_FIFO_WIDTH_BYTE_COUNT);
	fifo_count &= BIG_SUR_GE_COUNT_MASK;

	if ((fifo_count * BIG_SUR_GE_FIFO_WIDTH_BYTE_COUNT) < len)
		return -1;

	word_count = len / BIG_SUR_GE_FIFO_WIDTH_BYTE_COUNT;
	extra_byte = len % BIG_SUR_GE_FIFO_WIDTH_BYTE_COUNT;

	for (fifo_count = 0; fifo_count < word_count; fifo_count++)
		buffer_data[fifo_count] =
		    BIG_SUR_GE_READ(fifo->reg_base_addr);

	if (extra_byte > 0) {
		unsigned long last_word;
		int *extra_buffer_data =
		    (int *) (buffer_data + word_count);

		last_word = BIG_SUR_GE_READ(fifo->data_base_address);
		if (extra_byte == 1)
			extra_buffer_data[0] = (int) (last_word << 24);
		else if (extra_byte == 2) {
			extra_buffer_data[0] = (int) (last_word << 24);
			extra_buffer_data[1] = (int) (last_word << 16);
		} else if (extra_byte == 3) {
			extra_buffer_data[0] = (int) (last_word << 24);
			extra_buffer_data[1] = (int) (last_word << 16);
			extra_buffer_data[2] = (int) (last_word << 8);
		}
	}

	return 0;
}

/*****************************************************************************
 * Write the data into the packet fifo
 *****************************************************************************/
static int packet_fifo_write(packet_fifo * fifo, int *buffer, int len)
{
	unsigned long fifo_count, word_count, extra_byte;
	unsigned long *buffer_data = (unsigned long *) buffer;

	fifo_count =
	    BIG_SUR_GE_READ(fifo->reg_base_addr +
			    BIG_SUR_GE_FIFO_WIDTH_BYTE_COUNT);
	fifo_count &= BIG_SUR_GE_COUNT_MASK;

	word_count = len / BIG_SUR_GE_FIFO_WIDTH_BYTE_COUNT;
	extra_byte = len % BIG_SUR_GE_FIFO_WIDTH_BYTE_COUNT;

	/* You should see what the ppc driver does here. It just slobbers */
	if (extra_byte > 0)
		if (fifo_count > (word_count + 1)) {
			printk(KERN_ERR
			       "No room in the packet send fifo \n");
			return -1;
		}

	for (fifo_count = 0; fifo_count < word_count; fifo_count++)
		BIG_SUR_GE_WRITE(fifo->data_base_address,
				 buffer_data[fifo_count]);


	if (extra_byte > 0) {
		unsigned long last_word = 0;
		int *extra_buffer_data =
		    (int *) (buffer_data + word_count);

		if (extra_byte == 1)
			last_word = extra_buffer_data[0] << 24;
		else if (extra_byte == 2)
			last_word = (extra_buffer_data[0] << 24 |
				     extra_buffer_data[1] << 16);

		else if (extra_byte == 3)
			last_word = (extra_buffer_data[0] << 24 |
				     extra_buffer_data[1] << 16 |
				     extra_buffer_data[2] << 8);


		BIG_SUR_GE_WRITE(fifo->data_base_address, last_word);
	}

	return 0;
}


/*****************************************************************************
 * Interrupt handlers: We handle any errors associated with the FIFO.
 * FIFO is for simple dma case and we do want to handle the simple DMA
 * case. We dont handle the Scatter Gather DMA for now since it is not working.
 ******************************************************************************/

/*********************************************************************************
 * FIFO send for Simple DMA with Interrupts
 **********************************************************************************/
static int big_sur_ge_enet_fifo_send(big_sur_ge * emac, u8 * buffer,
			      unsigned long byte_cnt)
{
	unsigned long int_status, reg_data;

	/* Silly checks here that we really dont need */
	if (!emac->started)
		return -1;

	if (emac->polled)
		return -1;

	if (emac->dma_sg)
		return -1;

	int_status =
	    BIG_SUR_GE_READ(emac->base_address + XIIF_V123B_IISR_OFFSET);
	if (int_status & BIG_SUR_GE_EIR_XMIT_LFIFO_FULL_MASK) {
		printk(KERN_ERR "Tx FIFO error: Queue is Full \n");
		return -1;
	}

	/*
	 * Write the data to the FIFO in the hardware
	 */
	if ((BIG_SUR_GE_GET_COUNT(&emac->send_fifo) *
	     sizeof(unsigned long)) < byte_cnt) {
		printk(KERN_ERR "Send FIFO on chip is full \n");
		return -1;
	}

	if (big_sur_ge_dma_status(&emac->send_channel) &
	    BIG_SUR_GE_DMASR_BUSY_MASK) {
		printk(KERN_ERR "Send channel FIFO engine busy \n");
		return -1;
	}

	big_sur_ge_set_dma_control(&emac->send_channel,
				   BIG_SUR_GE_DMACR_SOURCE_INCR_MASK |
				   BIG_SUR_GE_DMACR_DEST_LOCAL_MASK |
				   BIG_SUR_GE_DMACR_SG_DISABLE_MASK);

	big_sur_ge_dma_transfer(&emac->send_channel,
				(unsigned long *) buffer,
				(unsigned long *) (emac->base_address +
						   BIG_SUR_GE_PFIFO_TXDATA_OFFSET),
				byte_cnt);

	reg_data = big_sur_ge_dma_status(&emac->send_channel);
	while (reg_data & BIG_SUR_GE_DMASR_BUSY_MASK) {
		reg_data = big_sur_ge_dma_status(&emac->recv_channel);
		if (!(reg_data & BIG_SUR_GE_DMASR_BUSY_MASK))
			break;
	}

	if ((reg_data & BIG_SUR_GE_DMASR_BUS_ERROR_MASK) ||
	    (reg_data & BIG_SUR_GE_DMASR_BUS_TIMEOUT_MASK)) {
		printk(KERN_ERR "Send side DMA error \n");
		return -1;
	}

	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_TPLR_OFFSET,
			 byte_cnt);

	return 0;
}

/*************************************************************************
 * FIFO receive for Simple DMA case
 *************************************************************************/
static int big_sur_ge_enet_fifo_recv(big_sur_ge * emac, u8 * buffer,
			      unsigned long *byte_cnt)
{
	unsigned long int_status, reg_data;

	/* Silly checks here that we really dont need */
	if (!emac->started)
		return -1;

	if (emac->polled)
		return -1;

	if (emac->dma_sg)
		return -1;

	if (*byte_cnt < BIG_SUR_GE_MAX_FRAME_SIZE)
		return -1;

	int_status =
	    BIG_SUR_GE_READ(emac->base_address + XIIF_V123B_IISR_OFFSET);
	if (int_status & BIG_SUR_GE_EIR_RECV_LFIFO_EMPTY_MASK) {
		BIG_SUR_GE_WRITE(emac->base_address +
				 XIIF_V123B_IISR_OFFSET,
				 BIG_SUR_GE_EIR_RECV_LFIFO_EMPTY_MASK);
		return -1;
	}

	if (big_sur_ge_dma_status(&emac->recv_channel) &
	    BIG_SUR_GE_DMASR_BUSY_MASK) {
		printk(KERN_ERR "Rx side DMA Engine busy \n");
		return -1;
	}

	if (BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_RPLR_OFFSET) ==
	    0) {
		printk(KERN_ERR "MAC has the FIFO packet length 0 \n");
		return -1;
	}

	/* For the simple DMA case only */
	big_sur_ge_set_dma_control(&emac->recv_channel,
				   BIG_SUR_GE_DMACR_DEST_INCR_MASK |
				   BIG_SUR_GE_DMACR_SOURCE_LOCAL_MASK |
				   BIG_SUR_GE_DMACR_SG_DISABLE_MASK);

	if (packet_fifo_read(&emac->recv_fifo, buffer,
			     BIG_SUR_GE_READ(emac->base_address +
					     BIG_SUR_GE_RPLR_OFFSET)) ==
	    -1) {
		printk(KERN_ERR "Not enough space in the FIFO \n");
		return -1;
	}

	big_sur_ge_dma_transfer(&emac->recv_channel,
				(unsigned long *) (emac->base_address +
						   BIG_SUR_GE_PFIFO_RXDATA_OFFSET),
				(unsigned long *)
				buffer,
				BIG_SUR_GE_READ(emac->base_address +
						BIG_SUR_GE_RPLR_OFFSET));

	reg_data = big_sur_ge_dma_status(&emac->recv_channel);
	while (reg_data & BIG_SUR_GE_DMASR_BUSY_MASK) {
		reg_data = big_sur_ge_dma_status(&emac->recv_channel);
		if (!(reg_data & BIG_SUR_GE_DMASR_BUSY_MASK))
			break;
	}

	if ((reg_data & BIG_SUR_GE_DMASR_BUS_ERROR_MASK) ||
	    (reg_data & BIG_SUR_GE_DMASR_BUS_TIMEOUT_MASK)) {
		printk(KERN_ERR "DMA Bus Error \n");
		return -1;
	}

	*byte_cnt =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_RPLR_OFFSET);

	return 0;
}

static irqreturn_t big_sur_ge_int_handler(int irq, void *dev_id)
{
	struct net_device *netdev = dev_id;
	struct big_sur_ge_enet *lp = netdev->priv;
	big_sur_ge *emac = (big_sur_ge *)emac_ptr;
	void *emac_ptr = &lp->emac;
	unsigned long int_status;

	int_status =
	    BIG_SUR_GE_READ(emac->base_address + XIIF_V123B_DIPR_OFFSET);
	if (int_status & BIG_SUR_GE_IPIF_EMAC_MASK)
		handle_fifo_intr(emac);

	if (int_status & BIG_SUR_GE_IPIF_RECV_FIFO_MASK)
		big_sur_ge_check_fifo_recv_error(emac);

	if (int_status & BIG_SUR_GE_IPIF_SEND_FIFO_MASK)
		big_sur_ge_check_fifo_send_error(emac);

	if (int_status & XIIF_V123B_ERROR_MASK)
		BIG_SUR_GE_WRITE(emac->base_address +
				 XIIF_V123B_DISR_OFFSET,
				 XIIF_V123B_ERROR_MASK);

	return IRQ_HANDLED;
}

/****************************************************************************
 * Set the FIFO send handler
 ***************************************************************************/
static void big_sur_ge_set_fifo_send_handler(big_sur_ge * emac, void *call_back,
				      big_sur_fifo_handler function)
{
	emac->big_sur_ge_fifo_send_handler = function;
	emac->fifo_send_ref = call_back;
}

/****************************************************************************
 * Set the FIFO recv handler
 ***************************************************************************/
static void big_sur_ge_set_fifo_recv_handler(big_sur_ge * emac, void *call_back,
				      big_sur_fifo_handler function)
{
	emac->big_sur_ge_fifo_recv_handler = function;
	emac->fifo_recv_ref = call_back;
}

/****************************************************************************
 * Main Fifo intr handler
 ***************************************************************************/
static void handle_fifo_intr(big_sur_ge * emac)
{
	unsigned long int_status;

	/* Ack the interrupts asap */
	int_status =
	    BIG_SUR_GE_READ(emac->base_address + XIIF_V123B_IISR_OFFSET);
	BIG_SUR_GE_WRITE(emac->base_address + XIIF_V123B_IISR_OFFSET,
			 int_status);

	/* Process the Rx side */
	if (int_status & BIG_SUR_GE_EIR_RECV_DONE_MASK) {
		emac->big_sur_ge_fifo_recv_handler(&emac->fifo_recv_ref);
		BIG_SUR_GE_WRITE(emac->base_address +
				 XIIF_V123B_IISR_OFFSET,
				 BIG_SUR_GE_EIR_RECV_DONE_MASK);
	}

	if (int_status & BIG_SUR_GE_EIR_XMIT_DONE_MASK) {
		/* We dont collect stats and hence we dont need to get status */

		emac->big_sur_ge_fifo_send_handler(emac->fifo_recv_ref);
		BIG_SUR_GE_WRITE(emac->base_address +
				 XIIF_V123B_IISR_OFFSET,
				 BIG_SUR_GE_EIR_XMIT_DONE_MASK);
	}

	big_sur_ge_check_mac_error(emac, int_status);
}

/******************************************************************
 * Handle the Receive side DMA interrupts. The PPC driver has
 * callbacks all over the place. This has been eliminated here by
 * using the following approach:
 *
 * The ISR is set to the main interrrupt handler. This will handle
 * all the interrupts including the ones for DMA. In this main isr,
 * we determine if we need to call recv or send side intr functions.
 * Pretty complex but thats the way it is now.
 *******************************************************************/
static void big_sur_ge_handle_recv_intr(big_sur_ge * emac)
{
	unsigned long int_status;

	int_status = big_sur_ge_get_intr_status(&emac->recv_channel);
	if (int_status & (BIG_SUR_GE_IXR_PKT_THRESHOLD_MASK |
			  BIG_SUR_GE_IXR_PKT_WAIT_BOUND_MASK)) {
		u32 num_packets;
		u32 num_processed;
		u32 num_buffers;
		u32 num_bytes;
		xbuf_descriptor *first_desc_ptr = NULL;
		xbuf_descriptor *buffer_desc;
		int is_last = 0;

		/* The number of packets we need to process on the Rx */
		num_packets =
		    big_sur_ge_get_packet_count(&emac->recv_channel);

		for (num_processed = 0; num_processed < num_packets;
		     num_processed++) {
			while (!is_last) {
				if (big_sur_ge_get_descriptor
				    (&emac->recv_channel,
				     &buffer_desc) == -1)
					break;

				if (first_desc_ptr == NULL)
					first_desc_ptr = buffer_desc;

				num_bytes +=
				    xbuf_descriptor_GetLength(buffer_desc);

				if (xbuf_descriptor_IsLastStatus
				    (buffer_desc)) {
					is_last = 1;
				}

				num_buffers++;
			}

			/* Number of buffers is always 1 since we dont do SG */

			/*
			 * Only for SG DMA which is currently not supported. In the
			 * future, as we have SG channel working, we will code this
			 * receive side routine. For now, do nothing. This is never
			 * called from FIFO mode - Manish
			 */
			big_sur_ge_decr_packet_count(&emac->recv_channel);
		}
	}

	/* Ack the interrupts */
	big_sur_ge_set_intr_status(&emac->recv_channel, int_status);

	if (int_status & BIG_SUR_GE_IXR_DMA_ERROR_MASK) {
		/* We need a reset here */
	}

	big_sur_ge_set_intr_status(&emac->recv_channel, int_status);
}

/****************************************************************
 * Handle the send side DMA interrupt
 ****************************************************************/
static void big_sur_ge_handle_send_intr(big_sur_ge * emac)
{
	unsigned long int_status;

	int_status = big_sur_ge_get_intr_status(&emac->send_channel);

	if (int_status & (BIG_SUR_GE_IXR_PKT_THRESHOLD_MASK |
			  BIG_SUR_GE_IXR_PKT_WAIT_BOUND_MASK)) {
		unsigned long num_frames = 0;
		unsigned long num_processed = 0;
		unsigned long num_buffers = 0;
		unsigned long num_bytes = 0;
		unsigned long is_last = 0;
		xbuf_descriptor *first_desc_ptr = NULL;
		xbuf_descriptor *buffer_desc;

		num_frames =
		    big_sur_ge_get_packet_count(&emac->send_channel);

		for (num_processed = 0; num_processed < num_frames;
		     num_processed++) {
			while (!is_last) {
				if (big_sur_ge_get_descriptor
				    (&emac->send_channel, &buffer_desc)
				    == -1) {
					break;
				}

				if (first_desc_ptr == NULL)
					first_desc_ptr = buffer_desc;

				num_bytes +=
				    xbuf_descriptor_GetLength(buffer_desc);
				if (xbuf_descriptor_IsLastControl
				    (buffer_desc))
					is_last = 1;

				num_buffers++;
			}

			/*
			 * Only for SG DMA which is currently not supported. In the
			 * future, as we have SG channel working, we will code this
			 * receive side routine. For now, do nothing. This is never
			 * called from FIFO mode - Manish
			 */
			big_sur_ge_decr_packet_count(&emac->send_channel);
		}
	}

	/* Ack the interrupts and reset DMA channel if necessary */
	big_sur_ge_set_intr_status(&emac->send_channel, int_status);
	if (int_status & BIG_SUR_GE_IXR_DMA_ERROR_MASK) {
		/* Manish : need reset */
	}

	big_sur_ge_set_intr_status(&emac->send_channel, int_status);
}

/*****************************************************************
 * For now, the MAC address errors dont trigger a update of the
 * stats. There is no stats framework in place. Hence, we just
 * check for the errors below and do a reset if needed.
 *****************************************************************/
static void big_sur_ge_check_mac_error(big_sur_ge * emac,
				unsigned long int_status)
{
	if (int_status & (BIG_SUR_GE_EIR_RECV_DFIFO_OVER_MASK |
			  BIG_SUR_GE_EIR_RECV_LFIFO_OVER_MASK |
			  BIG_SUR_GE_EIR_RECV_LFIFO_UNDER_MASK |
			  BIG_SUR_GE_EIR_RECV_ERROR_MASK |
			  BIG_SUR_GE_EIR_RECV_MISSED_FRAME_MASK |
			  BIG_SUR_GE_EIR_RECV_COLLISION_MASK |
			  BIG_SUR_GE_EIR_RECV_FCS_ERROR_MASK |
			  BIG_SUR_GE_EIR_RECV_LEN_ERROR_MASK |
			  BIG_SUR_GE_EIR_RECV_SHORT_ERROR_MASK |
			  BIG_SUR_GE_EIR_RECV_LONG_ERROR_MASK |
			  BIG_SUR_GE_EIR_RECV_ALIGN_ERROR_MASK |
			  BIG_SUR_GE_EIR_XMIT_SFIFO_OVER_MASK |
			  BIG_SUR_GE_EIR_XMIT_LFIFO_OVER_MASK |
			  BIG_SUR_GE_EIR_XMIT_SFIFO_UNDER_MASK |
			  BIG_SUR_GE_EIR_XMIT_LFIFO_UNDER_MASK)) {

		BIG_SUR_GE_WRITE(emac->base_address +
				 XIIF_V123B_IIER_OFFSET, 0);
		/*
		 * Manish Reset the MAC here
		 */
	}
}

/*****************************************************************
 * Check for FIFO Recv errors
 *****************************************************************/
static void big_sur_ge_check_fifo_recv_error(big_sur_ge * emac)
{
	if (BIG_SUR_GE_IS_DEADLOCKED(&emac->recv_fifo)) {
		unsigned long intr_enable;

		intr_enable =
		    BIG_SUR_GE_READ(emac->base_address +
				    XIIF_V123B_DIER_OFFSET);
		BIG_SUR_GE_WRITE(emac->base_address +
				 XIIF_V123B_DIER_OFFSET,
				 intr_enable &
				 ~(BIG_SUR_GE_IPIF_RECV_FIFO_MASK));

	}
}

/*****************************************************************
 * Check for FIFO Send errors
 *****************************************************************/
static void big_sur_ge_check_fifo_send_error(big_sur_ge * emac)
{
	if (BIG_SUR_GE_IS_DEADLOCKED(&emac->send_fifo)) {
		unsigned long intr_enable;

		intr_enable =
		    BIG_SUR_GE_READ(emac->base_address +
				    XIIF_V123B_DIER_OFFSET);
		BIG_SUR_GE_WRITE(emac->base_address +
				 XIIF_V123B_DIER_OFFSET,
				 intr_enable &
				 ~(BIG_SUR_GE_IPIF_SEND_FIFO_MASK));
	}
}

/*****************************************************************
 * GE unit init
 ****************************************************************/
static int big_sur_ge_enet_init(big_sur_ge * emac, unsigned int device_id)
{
	unsigned long reg_data;
	big_sur_ge_config *config;
	int err;

	/* Assume that the device has been stopped */

	config = big_sur_ge_lookup_config(device_id);
	if (config == NULL)
		return -1;

	emac->ready = 0;
	emac->started = 0;
	emac->dma_sg = 0;	/* This MAC has no support for Scatter Gather DMA */
	emac->has_mii = config->has_mii;
	emac->has_mcast_hash_table = 0;
	emac->dma_config = config->dma_config;

	/*
	 * Initialize the FIFO send and recv handlers to the stub handlers.
	 * We only deal with the FIFO mode of operation since SG is not supported.
	 * Also, there is no error handler. We try to handle as much of error as
	 * possible and then return. No error codes also.
	 */

	emac->base_address = config->base_address;

	if (big_sur_ge_config_dma(emac) == -1)
		return -1;

	err = big_sur_ge_config_fifo(emac);
	if (err == -1)
		return err;

	/* Now, we know that the FIFO initialized successfully. So, set the ready flag */
	emac->ready = 1;

	/* Do we need a PHY reset here also. It did cause problems on some boards */
	big_sur_ge_enet_reset(emac);

	/* PHY reset code. Remove if causes a problem on the board */
	reg_data =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_ECR_OFFSET);
	reg_data &= ~(BIG_SUR_GE_ECR_PHY_ENABLE_MASK);
	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_ECR_OFFSET,
			 reg_data);
	reg_data |= BIG_SUR_GE_ECR_PHY_ENABLE_MASK;
	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_ECR_OFFSET,
			 reg_data);

	return 0;
}

/*******************************************************************
 * Start the GE unit for Tx, Rx and Interrupts
 *******************************************************************/
static int big_sur_ge_start(big_sur_ge * emac)
{
	unsigned long reg_data;

	/*
	 * Basic mode of operation is polled and interrupt mode. We disable the polled
	 * mode for good. We may use the polled mode for Rx NAPI but that does not
	 * require all the interrupts to be disabled
	 */

	emac->polled = 0;

	/*
	 * DMA: Three modes of operation - simple, FIFO, SG. SG is surely not working
	 * and so is kept off using the dma_sg flag. Simple and FIFO work. But, we may
	 * not use FIFO at all. So, we enable the interrupts below
	 */

	BIG_SUR_GE_WRITE(emac->base_address + XIIF_V123B_DIER_OFFSET,
			 BIG_SUR_GE_IPIF_FIFO_DFT_MASK |
			 XIIF_V123B_ERROR_MASK);

	BIG_SUR_GE_WRITE(emac->base_address + XIIF_V123B_IIER_OFFSET,
			 BIG_SUR_GE_EIR_DFT_FIFO_MASK);

	/* Toggle the started flag */
	emac->started = 1;

	/* Start the Tx and Rx units respectively */
	reg_data =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_ECR_OFFSET);
	reg_data &=
	    ~(BIG_SUR_GE_ECR_XMIT_RESET_MASK |
	      BIG_SUR_GE_ECR_RECV_RESET_MASK);
	reg_data |=
	    (BIG_SUR_GE_ECR_XMIT_ENABLE_MASK |
	     BIG_SUR_GE_ECR_RECV_ENABLE_MASK);

	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_ECR_OFFSET,
			 reg_data);

	return 0;
}

/**************************************************************************
 * Stop the GE unit
 **************************************************************************/
static int big_sur_ge_stop(big_sur_ge * emac)
{
	unsigned long reg_data;

	/* We assume that the device is not already stopped */
	if (!emac->started)
		return 0;

	/* Disable the Tx and Rx unit respectively */
	reg_data =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_ECR_OFFSET);
	reg_data &=
	    ~(BIG_SUR_GE_ECR_XMIT_ENABLE_MASK |
	      BIG_SUR_GE_ECR_RECV_ENABLE_MASK);
	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_ECR_OFFSET,
			 reg_data);

	/* Disable the interrupts */
	BIG_SUR_GE_WRITE(emac->base_address + XIIF_V123B_DGIER_OFFSET, 0);

	/* Toggle the started flag */
	emac->started = 0;

	return 0;
}

/************************************************************************
 * Reset the GE MAC unit
 *************************************************************************/
static void big_sur_ge_enet_reset(big_sur_ge * emac)
{
	unsigned long reg_data;

	(void) big_sur_ge_stop(emac);

	BIG_SUR_GE_WRITE(emac->base_address + XIIF_V123B_RESETR_OFFSET,
			 XIIF_V123B_RESET_MASK);

	/*
	 * For now, configure the receiver to not strip off FCS and padding since
	 * this is not currently supported. In the future, just take the default
	 * and provide the option for the user to change this behavior.
	 */
	reg_data =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_ECR_OFFSET);
	reg_data &=
	    ~(BIG_SUR_GE_ECR_RECV_PAD_ENABLE_MASK |
	      BIG_SUR_GE_ECR_RECV_FCS_ENABLE_MASK);
	reg_data &= ~(BIG_SUR_GE_ECR_RECV_STRIP_ENABLE_MASK);
	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_ECR_OFFSET,
			 reg_data);
}

/*************************************************************************
 * Set the MAC address of the GE mac unit
 *************************************************************************/
static int big_sur_ge_set_mac_address(big_sur_ge * emac, unsigned char *addr)
{
	unsigned long mac_addr = 0;

	/* Device is started and so mac address must be set */
	if (emac->started == 1)
		return 0;

	mac_addr = ((addr[0] << 8) | addr[1]);
	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_SAH_OFFSET,
			 mac_addr);

	mac_addr |= ((addr[2] << 24) | (addr[3] << 16) |
		     (addr[4] << 8) | addr[5]);

	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_SAL_OFFSET,
			 mac_addr);

	return 0;
}

/****************************************************************************
 * Get the MAC address of the GE MAC unit
 ***************************************************************************/
static void big_sur_ge_get_mac_unit(big_sur_ge * emac, unsigned int *addr)
{
	unsigned long mac_addr_hi, mac_addr_lo;

	mac_addr_hi =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_SAH_OFFSET);
	mac_addr_lo =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_SAL_OFFSET);

	addr[0] = (mac_addr_hi >> 8);
	addr[1] = mac_addr_hi;

	addr[2] = (mac_addr_lo >> 24);
	addr[3] = (mac_addr_lo >> 16);
	addr[4] = (mac_addr_lo >> 8);
	addr[5] = mac_addr_lo;
}

/*********************************************************************************
 * Configure the GE MAC for DMA capabilities. Not for Scatter Gather, only Simple
 *********************************************************************************/
static int big_sur_ge_config_dma(big_sur_ge * emac)
{
	if (big_sur_ge_dma_init(&emac->recv_channel, emac->base_address +
				BIG_SUR_GE_DMA_RECV_OFFSET) == -1) {
		printk(KERN_ERR "Could not initialize the DMA unit  \n");
		return -1;
	}

	if (big_sur_ge_dma_init(&emac->send_channel, emac->base_address +
				BIG_SUR_GE_DMA_SEND_OFFSET) == -1) {
		printk(KERN_ERR "Could not initialize the DMA unit  \n");
		return -1;
	}

	return 0;
}

/******************************************************************************
 * Configure the FIFO for simple DMA
 ******************************************************************************/
static int big_sur_ge_config_fifo(big_sur_ge * emac)
{
	int err = 0;

	err = packet_fifo_init(&emac->recv_fifo, emac->base_address +
			       BIG_SUR_GE_PFIFO_RXREG_OFFSET,
			       emac->base_address +
			       BIG_SUR_GE_PFIFO_RXDATA_OFFSET);

	if (err == -1) {
		printk(KERN_ERR
		       "Could not initialize Rx packet FIFO for Simple DMA \n");
		return err;
	}

	err = packet_fifo_init(&emac->send_fifo, emac->base_address +
			       BIG_SUR_GE_PFIFO_TXREG_OFFSET,
			       emac->base_address +
			       BIG_SUR_GE_PFIFO_TXDATA_OFFSET);

	if (err == -1) {
		printk(KERN_ERR
		       "Could not initialize Tx packet FIFO for Simple DMA \n");
	}

	return err;
}

#define BIG_SUR_GE_NUM_INSTANCES	2


/**********************************************************************************
 * Look up the config of the MAC
 **********************************************************************************/
static big_sur_ge_config *big_sur_ge_lookup_config(unsigned int device_id)
{
	big_sur_ge_config *config = NULL;
	int i = 0;

	for (i = 0; i < BIG_SUR_GE_NUM_INSTANCES; i++) {
		/* Manish : Init the config here */
		break;
	}

	return config;
}

typedef struct {
	unsigned long option;
	unsigned long mask;
} option_map;

static option_map option_table[] = {
	{BIG_SUR_GE_UNICAST_OPTION, BIG_SUR_GE_ECR_UNICAST_ENABLE_MASK},
	{BIG_SUR_GE_BROADCAST_OPTION, BIG_SUR_GE_ECR_BROAD_ENABLE_MASK},
	{BIG_SUR_GE_PROMISC_OPTION, BIG_SUR_GE_ECR_PROMISC_ENABLE_MASK},
	{BIG_SUR_GE_FDUPLEX_OPTION, BIG_SUR_GE_ECR_FULL_DUPLEX_MASK},
	{BIG_SUR_GE_LOOPBACK_OPTION, BIG_SUR_GE_ECR_LOOPBACK_MASK},
	{BIG_SUR_GE_MULTICAST_OPTION, BIG_SUR_GE_ECR_MULTI_ENABLE_MASK},
	{BIG_SUR_GE_FLOW_CONTROL_OPTION, BIG_SUR_GE_ECR_PAUSE_FRAME_MASK},
	{BIG_SUR_GE_INSERT_PAD_OPTION,
	 BIG_SUR_GE_ECR_XMIT_PAD_ENABLE_MASK},
	{BIG_SUR_GE_INSERT_FCS_OPTION,
	 BIG_SUR_GE_ECR_XMIT_FCS_ENABLE_MASK},
	{BIG_SUR_GE_INSERT_ADDR_OPTION,
	 BIG_SUR_GE_ECR_XMIT_ADDR_INSERT_MASK},
	{BIG_SUR_GE_OVWRT_ADDR_OPTION,
	 BIG_SUR_GE_ECR_XMIT_ADDR_OVWRT_MASK},
	{BIG_SUR_GE_STRIP_PAD_OPTION, BIG_SUR_GE_ECR_RECV_PAD_ENABLE_MASK},
	{BIG_SUR_GE_STRIP_FCS_OPTION, BIG_SUR_GE_ECR_RECV_FCS_ENABLE_MASK},
	{BIG_SUR_GE_STRIP_PAD_FCS_OPTION,
	 BIG_SUR_GE_ECR_RECV_STRIP_ENABLE_MASK}
};

#define BIG_SUR_GE_NUM_OPTIONS		(sizeof(option_table) / sizeof(option_map))

/**********************************************************************
 * Set the options for the GE
 **********************************************************************/
static int big_sur_ge_set_options(big_sur_ge * emac, unsigned long option_flag)
{
	unsigned long reg_data;
	unsigned int index;

	/* Assume that the device is stopped before calling this function */

	reg_data =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_ECR_OFFSET);
	for (index = 0; index < BIG_SUR_GE_NUM_OPTIONS; index++) {
		if (option_flag & option_table[index].option)
			reg_data |= option_table[index].mask;
		else
			reg_data &= ~(option_table[index].mask);

	}

	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_ECR_OFFSET,
			 reg_data);

	/* No polled option */
	emac->polled = 0;

	return 0;
}

/*******************************************************
 * Get the options from the GE
 *******************************************************/
static unsigned long big_sur_ge_get_options(big_sur_ge * emac)
{
	unsigned long option_flag = 0, reg_data;
	unsigned int index;

	reg_data =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_ECR_OFFSET);

	for (index = 0; index < BIG_SUR_GE_NUM_OPTIONS; index++) {
		if (option_flag & option_table[index].option)
			reg_data |= option_table[index].mask;
	}

	/* No polled mode */

	return option_flag;
}

/********************************************************
 * Set the Inter frame gap
 ********************************************************/
static int big_sur_ge_set_frame_gap(big_sur_ge * emac, int part1, int part2)
{
	unsigned long config;

	/* Assume that the device is stopped before calling this */

	config = ((part1 << BIG_SUR_GE_IFGP_PART1_SHIFT) |
		  (part2 << BIG_SUR_GE_IFGP_PART2_SHIFT));

	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_IFGP_OFFSET,
			 config);

	return 0;
}

/********************************************************
 * Get the Inter frame gap
 ********************************************************/
static void big_sur_ge_get_frame_gap(big_sur_ge * emac, int *part1, int *part2)
{
	unsigned long config;

	config =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_IFGP_OFFSET);
	*part1 =
	    ((config & BIG_SUR_GE_IFGP_PART1_SHIFT) >>
	     BIG_SUR_GE_IFGP_PART1_SHIFT);
	*part2 =
	    ((config & BIG_SUR_GE_IFGP_PART2_SHIFT) >>
	     BIG_SUR_GE_IFGP_PART2_SHIFT);
}

/*******************************************************************
 * PHY specific functions for the MAC
 *******************************************************************/
#define BIG_SUR_GE_MAX_PHY_ADDR		32
#define BIG_SUR_GE_MAX_PHY_REG		32

/*******************************************************************
 * Read the PHY reg
 *******************************************************************/
static int big_sur_ge_phy_read(big_sur_ge * emac, unsigned long addr,
			unsigned long reg_num, unsigned int *data)
{
	unsigned long mii_control, mii_data;

	if (!emac->has_mii)
		return -1;

	mii_control =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_MGTCR_OFFSET);
	if (mii_control & BIG_SUR_GE_MGTCR_START_MASK) {
		printk(KERN_ERR "PHY busy \n");
		return -1;
	}

	mii_control = (addr << BIG_SUR_GE_MGTCR_PHY_ADDR_SHIFT);
	mii_control |= (reg_num << BIG_SUR_GE_MGTCR_REG_ADDR_SHIFT);
	mii_control |=
	    (BIG_SUR_GE_MGTCR_RW_NOT_MASK | BIG_SUR_GE_MGTCR_START_MASK |
	     BIG_SUR_GE_MGTCR_MII_ENABLE_MASK);

	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_MGTCR_OFFSET,
			 mii_control);

	while (mii_control & BIG_SUR_GE_MGTCR_START_MASK)
		if (!(mii_control & BIG_SUR_GE_MGTCR_START_MASK))
			break;

	mii_data =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_MGTDR_OFFSET);
	*data = (unsigned int) mii_data;

	return 0;
}

/**********************************************************************
 * Write to the PHY register
 **********************************************************************/
static int big_sur_ge_phy_write(big_sur_ge * emac, unsigned long addr,
			 unsigned long reg_num, unsigned int data)
{
	unsigned long mii_control;

	if (!emac->has_mii)
		return -1;

	mii_control =
	    BIG_SUR_GE_READ(emac->base_address + BIG_SUR_GE_MGTCR_OFFSET);
	if (mii_control & BIG_SUR_GE_MGTCR_START_MASK) {
		printk(KERN_ERR "PHY busy \n");
		return -1;
	}

	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_MGTDR_OFFSET,
			 (unsigned long) data);

	mii_control = (addr << BIG_SUR_GE_MGTCR_PHY_ADDR_SHIFT);
	mii_control |= (reg_num << BIG_SUR_GE_MGTCR_REG_ADDR_SHIFT);
	mii_control |=
	    (BIG_SUR_GE_MGTCR_START_MASK |
	     BIG_SUR_GE_MGTCR_MII_ENABLE_MASK);

	BIG_SUR_GE_WRITE(emac->base_address + BIG_SUR_GE_MGTCR_OFFSET,
			 mii_control);

	while (mii_control & BIG_SUR_GE_MGTCR_START_MASK)
		if (!(mii_control & BIG_SUR_GE_MGTCR_START_MASK))
			break;

	return 0;
}






/********************************************************************
 * The hardware dependent part of the driver begins here
 ********************************************************************/


/*******************************************************************
 * Reset the GE system
 *******************************************************************/
static void big_sur_ge_reset(struct net_device *netdev, DUPLEX duplex)
{
	struct big_sur_ge_enet *lp = netdev->priv;
	struct sk_buff *skb;
	unsigned long options;
	int ifcfg1, ifcfg2;

	/* Stop the queue */
	netif_stop_queue(netdev);

	big_sur_ge_get_frame_gap(&lp->emac, &ifcfg1, &ifcfg2);
	options = big_sur_ge_get_options(&lp->emac);
	switch (duplex) {
	case HALF_DUPLEX:
		options &= ~(BIG_SUR_GE_FDUPLEX_OPTION);
		break;

	case FULL_DUPLEX:
		options |= BIG_SUR_GE_FDUPLEX_OPTION;
		break;

	case UNKNOWN:
		break;
	}

	/* There is no support for SG DMA in a 100 Mpbs NIC */

	big_sur_ge_enet_reset(&lp->emac);

	/* Set the necessary options for the MAC unit */
	big_sur_ge_set_mac_address(&lp->emac, netdev->dev_addr);
	big_sur_ge_set_frame_gap(&lp->emac, ifcfg1, ifcfg2);
	big_sur_ge_set_options(&lp->emac, options);

	(void) big_sur_ge_start(&lp->emac);

	spin_lock_irq(&lp->lock);
	skb = lp->saved_skb;
	lp->saved_skb = NULL;
	spin_unlock_irq(&lp->lock);

	if (skb)
		dev_kfree_skb(skb);

	/* Wake the queue */
	netif_wake_queue(netdev);
}

/********************************************************************
 * Get the PHY status
 *******************************************************************/
static int big_sur_ge_get_phy_status(struct net_device *netdev,
				     DUPLEX * duplex, int *linkup)
{
	struct big_sur_ge_enet *lp = netdev->priv;
	unsigned int reg_data;
	int err = 0;

	err =
	    big_sur_ge_phy_read(&lp->emac, lp->mii_addr, MII_BMCR,
				&reg_data);
	if (err == -1) {
		printk(KERN_ERR "%s: Could not read PHY control register",
		       netdev->name);
		return err;
	}

	if (!(reg_data & BMCR_ANENABLE)) {
		if (reg_data & BMCR_FULLDPLX)
			*duplex = FULL_DUPLEX;
		else
			*duplex = HALF_DUPLEX;
	} else {
		unsigned int advertise, partner, neg;

		err =
		    big_sur_ge_phy_read(&lp->emac, lp->mii_addr,
					MII_ADVERTISE, &advertise);
		if (err == -1) {
			printk(KERN_ERR
			       "%s: Could not read PHY control register",
			       netdev->name);
			return err;
		}

		err =
		    big_sur_ge_phy_read(&lp->emac, lp->mii_addr, MII_LPA,
					&partner);
		if (err == -1) {
			printk(KERN_ERR
			       "%s: Could not read PHY control register",
			       netdev->name);
			return err;
		}

		neg = advertise & partner & ADVERTISE_ALL;
		if (neg & ADVERTISE_100FULL)
			*duplex = FULL_DUPLEX;
		else if (neg & ADVERTISE_100HALF)
			*duplex = HALF_DUPLEX;
		else if (neg & ADVERTISE_10FULL)
			*duplex = FULL_DUPLEX;
		else
			*duplex = HALF_DUPLEX;

		err =
		    big_sur_ge_phy_read(&lp->emac, lp->mii_addr, MII_BMSR,
					&reg_data);
		if (err == -1) {
			printk(KERN_ERR
			       "%s: Could not read PHY control register",
			       netdev->name);
			return err;
		}

		*linkup = (reg_data & BMSR_LSTATUS) != 0;

	}
	return 0;
}

/************************************************************
 * Poll the MII for duplex and link status
 ***********************************************************/
static void big_sur_ge_poll_mii(unsigned long data)
{
	struct net_device *netdev = (struct net_device *) data;
	struct big_sur_ge_enet *lp = netdev->priv;
	unsigned long options;
	DUPLEX mac_duplex, phy_duplex;
	int phy_carrier, netif_carrier;

	if (big_sur_ge_get_phy_status(netdev, &phy_duplex, &phy_carrier) ==
	    -1) {
		printk(KERN_ERR "%s: Terminating link monitoring.\n",
		       netdev->name);
		return;
	}

	options = big_sur_ge_get_options(&lp->emac);
	if (options & BIG_SUR_GE_FDUPLEX_OPTION)
		mac_duplex = FULL_DUPLEX;
	else
		mac_duplex = HALF_DUPLEX;

	if (mac_duplex != phy_duplex) {
		disable_irq(netdev->irq);
		big_sur_ge_reset(netdev, phy_duplex);
		enable_irq(netdev->irq);
	}

	netif_carrier = netif_carrier_ok(netdev) != 0;

	if (phy_carrier != netif_carrier) {
		if (phy_carrier) {
			printk(KERN_INFO "%s: Link carrier restored.\n",
			       netdev->name);
			netif_carrier_on(netdev);
		} else {
			printk(KERN_INFO "%s: Link carrier lost.\n",
			       netdev->name);
			netif_carrier_off(netdev);
		}
	}

	/* Set up the timer so we'll get called again in 2 seconds. */
	lp->phy_timer.expires = jiffies + 2 * HZ;
	add_timer(&lp->phy_timer);
}

/**************************************************************
 * Open the network interface
 *************************************************************/
static int big_sur_ge_open(struct net_device *netdev)
{
	struct big_sur_ge_enet *lp = netdev->priv;
	unsigned long options;
	DUPLEX phy_duplex, mac_duplex;
	int phy_carrier, retval;

	(void) big_sur_ge_stop(&lp->emac);

	if (big_sur_ge_set_mac_address(&lp->emac, netdev->dev_addr) == -1) {
		printk(KERN_ERR "%s: Could not set MAC address.\n",
		       netdev->name);
		return -EIO;
	}

	options = big_sur_ge_get_options(&lp->emac);

	retval =
	    request_irq(netdev->irq, &big_sur_ge_int_handler, 0,
			netdev->name, netdev);
	if (retval) {
		printk(KERN_ERR
		       "%s: Could not allocate interrupt %d.\n",
		       netdev->name, netdev->irq);

		return retval;
	}

	if (!
	    (big_sur_ge_get_phy_status(netdev, &phy_duplex, &phy_carrier)))
	{
		if (options & BIG_SUR_GE_FDUPLEX_OPTION)
			mac_duplex = FULL_DUPLEX;
		else
			mac_duplex = HALF_DUPLEX;

		if (mac_duplex != phy_duplex) {
			switch (phy_duplex) {
			case HALF_DUPLEX:
				options &= ~(BIG_SUR_GE_FDUPLEX_OPTION);
				break;
			case FULL_DUPLEX:
				options |= BIG_SUR_GE_FDUPLEX_OPTION;
				break;
			case UNKNOWN:
				break;
			}

			big_sur_ge_set_options(&lp->emac, options);
		}
	}

	if (big_sur_ge_start(&lp->emac) == -1) {
		printk(KERN_ERR "%s: Could not start device.\n",
		       netdev->name);
		free_irq(netdev->irq, netdev);
		return -EBUSY;
	}

	netif_start_queue(netdev);

	lp->phy_timer.expires = jiffies + 2 * HZ;
	lp->phy_timer.data = (unsigned long) netdev;
	lp->phy_timer.function = &big_sur_ge_poll_mii;
	add_timer(&lp->phy_timer);

	return 0;
}

/*********************************************************************
 * Close the network device interface
 *********************************************************************/
static int big_sur_ge_close(struct net_device *netdev)
{
	struct big_sur_ge_enet *lp = netdev->priv;

	del_timer_sync(&lp->phy_timer);
	netif_stop_queue(netdev);

	free_irq(netdev->irq, netdev);

	if (big_sur_ge_stop(&lp->emac) == -1) {
		printk(KERN_ERR "%s: Could not stop device.\n",
		       netdev->name);
		return -EBUSY;
	}

	return 0;
}

/*********************************************************************
 * Get the network device stats. For now, do nothing
 *********************************************************************/
static struct net_device_stats *big_sur_ge_get_stats(struct net_device
						     *netdev)
{
	/* Do nothing */
	return (struct net_device_stats *) 0;
}

/********************************************************************
 * FIFO send for a packet that needs to be transmitted
 ********************************************************************/
static int big_sur_ge_fifo_send(struct sk_buff *orig_skb,
				struct net_device *netdev)
{
	struct big_sur_ge_enet *lp = netdev->priv;
	struct sk_buff *new_skb;
	unsigned int len, align;

	netif_stop_queue(netdev);
	len = orig_skb->len;

	if (!(new_skb = dev_alloc_skb(len + 4))) {
		dev_kfree_skb(orig_skb);
		printk(KERN_ERR
		       "%s: Could not allocate transmit buffer.\n",
		       netdev->name);
		netif_wake_queue(netdev);
		return -EBUSY;
	}

	align = 4 - ((unsigned long) new_skb->data & 3);
	if (align != 4)
		skb_reserve(new_skb, align);

	skb_put(new_skb, len);
	memcpy(new_skb->data, orig_skb->data, len);

	dev_kfree_skb(orig_skb);

	lp->saved_skb = new_skb;
	if (big_sur_ge_enet_fifo_send(&lp->emac, (u8 *) new_skb->data, len)
	    == -1) {
		spin_lock_irq(&lp->lock);
		new_skb = lp->saved_skb;
		lp->saved_skb = NULL;
		spin_unlock_irq(&lp->lock);

		dev_kfree_skb(new_skb);
		printk(KERN_ERR "%s: Could not transmit buffer.\n",
		       netdev->name);
		netif_wake_queue(netdev);
		return -EIO;
	}
	return 0;
}

/**********************************************************************
 * Call the fifo send handler
 **********************************************************************/
static void big_sur_ge_fifo_send_handler(void *callback)
{
	struct net_device *netdev = (struct net_device *) callback;
	struct big_sur_ge_enet *lp = netdev->priv;
	struct sk_buff *skb;

	spin_lock_irq(&lp->lock);
	skb = lp->saved_skb;
	lp->saved_skb = NULL;
	spin_unlock_irq(&lp->lock);

	if (skb)
		dev_kfree_skb(skb);

	netif_wake_queue(netdev);
}

/**********************************************************************
 * Handle the timeout of the ethernet device
 **********************************************************************/
static void big_sur_ge_tx_timeout(struct net_device *netdev)
{
	printk
	    ("%s: Exceeded transmit timeout of %lu ms.	Resetting mac.\n",
	     netdev->name, TX_TIMEOUT * 1000UL / HZ);

	disable_irq(netdev->irq);
	big_sur_ge_reset(netdev, UNKNOWN);
	enable_irq(netdev->irq);
}

/*********************************************************************
 * When in FIFO mode, the callback function for packets received
 *********************************************************************/
static void big_sur_ge_fifo_recv_handler(void *callback)
{
	struct net_device *netdev = (struct net_device *) callback;
	struct big_sur_ge_enet *lp = netdev->priv;
	struct sk_buff *skb;
	unsigned long len = BIG_SUR_GE_MAX_FRAME_SIZE;
	unsigned int align;

	if (!(skb = dev_alloc_skb(len + 4))) {
		printk(KERN_ERR "%s: Could not allocate receive buffer.\n",
		       netdev->name);
		return;
	}

	align = 4 - ((unsigned long) skb->data & 3);
	if (align != 4)
		skb_reserve(skb, align);

	if (big_sur_ge_enet_fifo_recv(&lp->emac, (u8 *) skb->data, &len) ==
	    -1) {
		dev_kfree_skb(skb);

		printk(KERN_ERR "%s: Could not receive buffer \n",
		       netdev->name);
		netdev->tx_timeout = NULL;
		big_sur_ge_reset(netdev, UNKNOWN);
		netdev->tx_timeout = big_sur_ge_tx_timeout;
	}

	skb_put(skb, len);	/* Tell the skb how much data we got. */
	skb->dev = netdev;	/* Fill out required meta-data. */
	skb->protocol = eth_type_trans(skb, netdev);

	netif_rx(skb);		/* Send the packet upstream. */
}

/*********************************************************************
 * Set the Multicast Hash list
 *********************************************************************/
static void big_sur_ge_set_multicast_hash_list(struct net_device *netdev)
{
	struct big_sur_ge_enet *lp = netdev->priv;
	unsigned long options;

	disable_irq(netdev->irq);
	local_bh_disable();

	(void) big_sur_ge_stop(&lp->emac);
	options = big_sur_ge_get_options(&lp->emac);
	options &=
	    ~(BIG_SUR_GE_PROMISC_OPTION | BIG_SUR_GE_MULTICAST_OPTION);

	/* Do nothing for now */

	(void) big_sur_ge_start(&lp->emac);
	local_bh_enable();
	enable_irq(netdev->irq);
}

/***********************************************************************
 * IOCTL support
 ***********************************************************************/
static int big_sur_ge_ioctl(struct net_device *netdev, struct ifreq *rq,
			    int cmd)
{
	struct big_sur_ge_enet *lp = netdev->priv;
	struct mii_ioctl_data *data =
	    (struct mii_ioctl_data *) &rq->ifr_data;

	switch (cmd) {
	case SIOCGMIIPHY:	/* Get address of MII PHY in use. */
	case SIOCDEVPRIVATE:	/* for binary compat, remove in 2.5 */
		data->phy_id = lp->mii_addr;

	case SIOCGMIIREG:	/* Read MII PHY register. */
	case SIOCDEVPRIVATE + 1:	/* for binary compat, remove in 2.5 */
		if (data->phy_id > 31 || data->reg_num > 31)
			return -ENXIO;

		del_timer_sync(&lp->phy_timer);

		if (big_sur_ge_phy_read(&lp->emac, data->phy_id,
					data->reg_num,
					&data->val_out) == -1) {
			printk(KERN_ERR "%s: Could not read from PHY",
			       netdev->name);
			return -EBUSY;
		}

		lp->phy_timer.expires = jiffies + 2 * HZ;
		add_timer(&lp->phy_timer);

		return 0;

	case SIOCSMIIREG:	/* Write MII PHY register. */
	case SIOCDEVPRIVATE + 2:	/* for binary compat, remove in 2.5 */
		if (data->phy_id > 31 || data->reg_num > 31)
			return -ENXIO;

		del_timer_sync(&lp->phy_timer);

		if (big_sur_ge_phy_write
		    (&lp->emac, data->phy_id, data->reg_num,
		     data->val_in) == -1) {
			printk(KERN_ERR "%s: Could not write to PHY",
			       netdev->name);
			return -EBUSY;
		}

		lp->phy_timer.expires = jiffies + 2 * HZ;
		add_timer(&lp->phy_timer);

		return 0;

	default:
		return -EOPNOTSUPP;
	}
}

/*****************************************************************
 * Get the config from the config table
 *****************************************************************/
static big_sur_ge_config *big_sur_ge_get_config(int index)
{
	/* Manish */
	return (big_sur_ge_config *) 0;
}

/*****************************************************************
 * Release the network device structure
 *****************************************************************/
static void big_sur_ge_remove_head(void)
{
	struct net_device *netdev;
	struct big_sur_ge_enet *lp;
	big_sur_ge_config *config;

	spin_lock(&dev_lock);
	netdev = dev_list;
	lp = netdev->priv;

	spin_unlock(&dev_lock);

	config = big_sur_ge_get_config(lp->index);
	iounmap((void *) config->base_address);
	config->base_address = lp->save_base_address;

	if (lp->saved_skb)
		dev_kfree_skb(lp->saved_skb);
	kfree(lp);

	unregister_netdev(netdev);
	kfree(netdev);
}

/*****************************************************************
 * Initial Function to probe the network interface
 *****************************************************************/
static int __init big_sur_ge_probe(int index)
{
	static const unsigned long remap_size =
	    BIG_SUR_GE_EMAC_0_HIGHADDR - BIG_SUR_GE_EMAC_0_BASEADDR + 1;
	struct net_device *netdev;
	struct big_sur_ge_enet *lp;
	big_sur_ge_config *config;
	unsigned int irq;
	unsigned long maddr;
	goto err;

	switch (index) {
	case 0:
		irq = (31 - BIG_SUR_GE_INTC_0_EMAC_0_VEC_ID);
		break;
	case 1:
		irq = (31 - BIG_SUR_GE_INTC_1_EMAC_1_VEC_ID);
		break;
	case 2:
		irq = (31 - BIG_SUR_GE_INTC_2_EMAC_2_VEC_ID);
		break;
	default:
		err = -ENODEV;
		goto out;
	}

	config = big_sur_ge_get_config(index);
	if (!config) {
		err = -ENODEV;
		goto out;
	}

	netdev = alloc_etherdev(sizeof(big_sur_ge_config));

	if (!netdev) {
		err = -ENOMEM;
		goto out;
	}

	SET_MODULE_OWNER(netdev);

	netdev->irq = irq;

	lp = (struct big_sur_ge_enet *) netdev->priv;
	memset(lp, 0, sizeof(struct big_sur_ge_enet));
	spin_lock_init(&lp->lock);
	spin_lock(&dev_lock);
	lp->next_dev = dev_list;
	dev_list = netdev;
	spin_unlock(&dev_lock);

	lp->save_base_address = config->base_address;
	config->base_address =
	    (unsigned long) ioremap(lp->save_base_address, remap_size);
	if (!config->base_address) {
		err = -ENOMEM;
		goto out_unlock;
	}

	if (big_sur_ge_enet_init(&lp->emac, config->device_id) == -1) {
		printk(KERN_ERR "%s: Could not initialize device.\n",
		       netdev->name);
		err = -ENODEV;
		goto out_unmap;
	}

	/* Manish: dev_addr value */
	memcpy(netdev->dev_addr, big_sur_mac_addr_base, 6);
	if (big_sur_ge_set_mac_address(&lp->emac, netdev->dev_addr) == -1) {
		printk(KERN_ERR "%s: Could not set MAC address.\n",
		       netdev->name);
		err = -EIO;
		goto out_unmap;
	}

	/*
	 * There is no Scatter Gather support but there is a Simple DMA support
	 */
	big_sur_ge_set_fifo_recv_handler(&lp->emac, netdev,
					 big_sur_ge_fifo_recv_handler);
	big_sur_ge_set_fifo_send_handler(&lp->emac, netdev,
					 big_sur_ge_fifo_send_handler);
	netdev->hard_start_xmit = big_sur_ge_fifo_send;

	lp->mii_addr = 0xFF;

	for (maddr = 0; maddr < 31; maddr++) {
		unsigned int reg_data;

		if (big_sur_ge_phy_read
		    (&lp->emac, maddr, MII_BMCR, &reg_data) == 0) {
			lp->mii_addr = maddr;
			break;
		}
	}

	if (lp->mii_addr == 0xFF) {
		lp->mii_addr = 0;
		printk(KERN_WARNING
		       "%s: No PHY detected.  Assuming a PHY at address %d.\n",
		       netdev->name, lp->mii_addr);
	}

	netdev->open = big_sur_ge_open;
	netdev->stop = big_sur_ge_close;
	netdev->get_stats = big_sur_ge_get_stats;	/* Does nothing */
	netdev->do_ioctl = big_sur_ge_ioctl;
	netdev->tx_timeout = big_sur_ge_tx_timeout;
	netdev->watchdog_timeo = TX_TIMEOUT;

	err = register_netdev(netdev))
	if (!err)
		goto out_unmap;

	printk(KERN_INFO "%s: PMC-Sierra Big Sur Ethernet Device %d  at 0x%08X "
	       "mapped to 0x%08X, irq=%d\n", netdev->name, index,
	       lp->save_base_address, config->base_address, netdev->irq);

	return ret;

out_unmap:
	iounmap(config->base_address);

out_unlock:
	big_sur_ge_remove_head();

out:
	return ret;
}

static int __init big_sur_ge_init(void)
{
	int index = 0;

	while (big_sur_ge_probe(index++) == 0);

	return (index > 1) ? 0 : -ENODEV;
}

static void __exit big_sur_ge_cleanup(void)
{
	while (dev_list)
		big_sur_ge_remove_head();
}

module_init(big_sur_ge_init);
module_exit(big_sur_ge_cleanup);

MODULE_AUTHOR("Manish Lachwani <lachwani@pmc-sierra.com>");
MODULE_DESCRIPTION("PMC-Sierra Big Sur Ethernet MAC Driver");
MODULE_LICENSE("GPL");
