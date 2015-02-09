/*
 *
 *		Linux MegaRAID driver for SAS based RAID controllers
 *
 * Copyright (c) 2003-2005  LSI Corporation.
 *
 *	   This program is free software; you can redistribute it and/or
 *	   modify it under the terms of the GNU General Public License
 *	   as published by the Free Software Foundation; either version
 *	   2 of the License, or (at your option) any later version.
 *
 * FILE		: megaraid_sas.c
 * Version     : v00.00.04.17.1-rc1
 *
 * Authors:
 *	(email-id : megaraidlinux@lsi.com)
 * 	Sreenivas Bagalkote
 * 	Sumant Patro
 *	Bo Yang
 *
 * List of supported controllers
 *
 * OEM	Product Name			VID	DID	SSVID	SSID
 * ---	------------			---	---	----	----
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/smp_lock.h>
#include <linux/uio.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/compat.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>
#include <linux/poll.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include "megaraid_sas.h"

/*
 * poll_mode_io:1- schedule complete completion from q cmd
 */
static unsigned int poll_mode_io;
module_param_named(poll_mode_io, poll_mode_io, int, 0);
MODULE_PARM_DESC(poll_mode_io,
	"Complete cmds from IO path, (default=0)");

MODULE_LICENSE("GPL");
MODULE_VERSION(MEGASAS_VERSION);
MODULE_AUTHOR("megaraidlinux@lsi.com");
MODULE_DESCRIPTION("LSI MegaRAID SAS Driver");

/*
 * PCI ID table for all supported controllers
 */
static struct pci_device_id megasas_pci_table[] = {

	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1064R)},
	/* xscale IOP */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078R)},
	/* ppc IOP */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078DE)},
	/* ppc IOP */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078GEN2)},
	/* gen2*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0079GEN2)},
	/* gen2*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0073SKINNY)},
	/* skinny*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0071SKINNY)},
	/* skinny*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_VERDE_ZCR)},
	/* xscale IOP, vega */
	{PCI_DEVICE(PCI_VENDOR_ID_DELL, PCI_DEVICE_ID_DELL_PERC5)},
	/* xscale IOP */
	{}
};

MODULE_DEVICE_TABLE(pci, megasas_pci_table);

static int megasas_mgmt_majorno;
static struct megasas_mgmt_info megasas_mgmt_info;
static struct fasync_struct *megasas_async_queue;
static DEFINE_MUTEX(megasas_async_queue_mutex);

static int megasas_poll_wait_aen;
static DECLARE_WAIT_QUEUE_HEAD(megasas_poll_wait);
static u32 support_poll_for_event;
static u32 megasas_dbg_lvl;

/* define lock for aen poll */
spinlock_t poll_aen_lock;

static void
megasas_complete_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd,
		     u8 alt_status);

/**
 * megasas_get_cmd -	Get a command from the free pool
 * @instance:		Adapter soft state
 *
 * Returns a free command from the pool
 */
static struct megasas_cmd *megasas_get_cmd(struct megasas_instance
						  *instance)
{
	unsigned long flags;
	struct megasas_cmd *cmd = NULL;

	spin_lock_irqsave(&instance->cmd_pool_lock, flags);

	if (!list_empty(&instance->cmd_pool)) {
		cmd = list_entry((&instance->cmd_pool)->next,
				 struct megasas_cmd, list);
		list_del_init(&cmd->list);
	} else {
		printk(KERN_ERR "megasas: Command pool empty!\n");
	}

	spin_unlock_irqrestore(&instance->cmd_pool_lock, flags);
	return cmd;
}

/**
 * megasas_return_cmd -	Return a cmd to free command pool
 * @instance:		Adapter soft state
 * @cmd:		Command packet to be returned to free command pool
 */
static inline void
megasas_return_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	unsigned long flags;

	spin_lock_irqsave(&instance->cmd_pool_lock, flags);

	cmd->scmd = NULL;
	list_add_tail(&cmd->list, &instance->cmd_pool);

	spin_unlock_irqrestore(&instance->cmd_pool_lock, flags);
}


/**
*	The following functions are defined for xscale 
*	(deviceid : 1064R, PERC5) controllers
*/

/**
 * megasas_enable_intr_xscale -	Enables interrupts
 * @regs:			MFI register set
 */
static inline void
megasas_enable_intr_xscale(struct megasas_register_set __iomem * regs)
{
	writel(1, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_xscale -Disables interrupt
 * @regs:			MFI register set
 */
static inline void
megasas_disable_intr_xscale(struct megasas_register_set __iomem * regs)
{
	u32 mask = 0x1f;
	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_xscale - returns the current FW status value
 * @regs:			MFI register set
 */
static u32
megasas_read_fw_status_reg_xscale(struct megasas_register_set __iomem * regs)
{
	return readl(&(regs)->outbound_msg_0);
}
/**
 * megasas_clear_interrupt_xscale -	Check & clear interrupt
 * @regs:				MFI register set
 */
static int 
megasas_clear_intr_xscale(struct megasas_register_set __iomem * regs)
{
	u32 status;
	/*
	 * Check if it is our interrupt
	 */
	status = readl(&regs->outbound_intr_status);

	if (!(status & MFI_OB_INTR_STATUS_MASK)) {
		return 1;
	}

	/*
	 * Clear the interrupt by writing back the same value
	 */
	writel(status, &regs->outbound_intr_status);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_status);

	return 0;
}

/**
 * megasas_fire_cmd_xscale -	Sends command to the FW
 * @frame_phys_addr :		Physical address of cmd
 * @frame_count :		Number of frames for the command
 * @regs :			MFI register set
 */
static inline void 
megasas_fire_cmd_xscale(struct megasas_instance *instance,
		dma_addr_t frame_phys_addr,
		u32 frame_count,
		struct megasas_register_set __iomem *regs)
{
	writel((frame_phys_addr >> 3)|(frame_count),
	       &(regs)->inbound_queue_port);
}

static struct megasas_instance_template megasas_instance_template_xscale = {

	.fire_cmd = megasas_fire_cmd_xscale,
	.enable_intr = megasas_enable_intr_xscale,
	.disable_intr = megasas_disable_intr_xscale,
	.clear_intr = megasas_clear_intr_xscale,
	.read_fw_status_reg = megasas_read_fw_status_reg_xscale,
};

/**
*	This is the end of set of functions & definitions specific 
*	to xscale (deviceid : 1064R, PERC5) controllers
*/

/**
*	The following functions are defined for ppc (deviceid : 0x60) 
* 	controllers
*/

/**
 * megasas_enable_intr_ppc -	Enables interrupts
 * @regs:			MFI register set
 */
static inline void
megasas_enable_intr_ppc(struct megasas_register_set __iomem * regs)
{
	writel(0xFFFFFFFF, &(regs)->outbound_doorbell_clear);
    
	writel(~0x80000004, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_ppc -	Disable interrupt
 * @regs:			MFI register set
 */
static inline void
megasas_disable_intr_ppc(struct megasas_register_set __iomem * regs)
{
	u32 mask = 0xFFFFFFFF;
	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_ppc - returns the current FW status value
 * @regs:			MFI register set
 */
static u32
megasas_read_fw_status_reg_ppc(struct megasas_register_set __iomem * regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}

/**
 * megasas_clear_interrupt_ppc -	Check & clear interrupt
 * @regs:				MFI register set
 */
static int 
megasas_clear_intr_ppc(struct megasas_register_set __iomem * regs)
{
	u32 status;
	/*
	 * Check if it is our interrupt
	 */
	status = readl(&regs->outbound_intr_status);

	if (!(status & MFI_REPLY_1078_MESSAGE_INTERRUPT)) {
		return 1;
	}

	/*
	 * Clear the interrupt by writing back the same value
	 */
	writel(status, &regs->outbound_doorbell_clear);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_doorbell_clear);

	return 0;
}
/**
 * megasas_fire_cmd_ppc -	Sends command to the FW
 * @frame_phys_addr :		Physical address of cmd
 * @frame_count :		Number of frames for the command
 * @regs :			MFI register set
 */
static inline void 
megasas_fire_cmd_ppc(struct megasas_instance *instance,
		dma_addr_t frame_phys_addr,
		u32 frame_count,
		struct megasas_register_set __iomem *regs)
{
	writel((frame_phys_addr | (frame_count<<1))|1, 
			&(regs)->inbound_queue_port);
}

static struct megasas_instance_template megasas_instance_template_ppc = {
	
	.fire_cmd = megasas_fire_cmd_ppc,
	.enable_intr = megasas_enable_intr_ppc,
	.disable_intr = megasas_disable_intr_ppc,
	.clear_intr = megasas_clear_intr_ppc,
	.read_fw_status_reg = megasas_read_fw_status_reg_ppc,
};

/**
 * megasas_enable_intr_skinny -	Enables interrupts
 * @regs:			MFI register set
 */
static inline void
megasas_enable_intr_skinny(struct megasas_register_set __iomem *regs)
{
	writel(0xFFFFFFFF, &(regs)->outbound_intr_mask);

	writel(~MFI_SKINNY_ENABLE_INTERRUPT_MASK, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_skinny -	Disables interrupt
 * @regs:			MFI register set
 */
static inline void
megasas_disable_intr_skinny(struct megasas_register_set __iomem *regs)
{
	u32 mask = 0xFFFFFFFF;
	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_skinny - returns the current FW status value
 * @regs:			MFI register set
 */
static u32
megasas_read_fw_status_reg_skinny(struct megasas_register_set __iomem *regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}

/**
 * megasas_clear_interrupt_skinny -	Check & clear interrupt
 * @regs:				MFI register set
 */
static int
megasas_clear_intr_skinny(struct megasas_register_set __iomem *regs)
{
	u32 status;
	/*
	 * Check if it is our interrupt
	 */
	status = readl(&regs->outbound_intr_status);

	if (!(status & MFI_SKINNY_ENABLE_INTERRUPT_MASK)) {
		return 1;
	}

	/*
	 * Clear the interrupt by writing back the same value
	 */
	writel(status, &regs->outbound_intr_status);

	/*
	* dummy read to flush PCI
	*/
	readl(&regs->outbound_intr_status);

	return 0;
}

/**
 * megasas_fire_cmd_skinny -	Sends command to the FW
 * @frame_phys_addr :		Physical address of cmd
 * @frame_count :		Number of frames for the command
 * @regs :			MFI register set
 */
static inline void
megasas_fire_cmd_skinny(struct megasas_instance *instance,
			dma_addr_t frame_phys_addr,
			u32 frame_count,
			struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->fire_lock, flags);
	writel(0, &(regs)->inbound_high_queue_port);
	writel((frame_phys_addr | (frame_count<<1))|1,
		&(regs)->inbound_low_queue_port);
	spin_unlock_irqrestore(&instance->fire_lock, flags);
}

static struct megasas_instance_template megasas_instance_template_skinny = {

	.fire_cmd = megasas_fire_cmd_skinny,
	.enable_intr = megasas_enable_intr_skinny,
	.disable_intr = megasas_disable_intr_skinny,
	.clear_intr = megasas_clear_intr_skinny,
	.read_fw_status_reg = megasas_read_fw_status_reg_skinny,
};


/**
*	The following functions are defined for gen2 (deviceid : 0x78 0x79)
*	controllers
*/

/**
 * megasas_enable_intr_gen2 -  Enables interrupts
 * @regs:                      MFI register set
 */
static inline void
megasas_enable_intr_gen2(struct megasas_register_set __iomem *regs)
{
	writel(0xFFFFFFFF, &(regs)->outbound_doorbell_clear);

	/* write ~0x00000005 (4 & 1) to the intr mask*/
	writel(~MFI_GEN2_ENABLE_INTERRUPT_MASK, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_gen2 - Disables interrupt
 * @regs:                      MFI register set
 */
static inline void
megasas_disable_intr_gen2(struct megasas_register_set __iomem *regs)
{
	u32 mask = 0xFFFFFFFF;
	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_gen2 - returns the current FW status value
 * @regs:                      MFI register set
 */
static u32
megasas_read_fw_status_reg_gen2(struct megasas_register_set __iomem *regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}

/**
 * megasas_clear_interrupt_gen2 -      Check & clear interrupt
 * @regs:                              MFI register set
 */
static int
megasas_clear_intr_gen2(struct megasas_register_set __iomem *regs)
{
	u32 status;
	/*
	 * Check if it is our interrupt
	 */
	status = readl(&regs->outbound_intr_status);

	if (!(status & MFI_GEN2_ENABLE_INTERRUPT_MASK))
		return 1;

	/*
	 * Clear the interrupt by writing back the same value
	 */
	writel(status, &regs->outbound_doorbell_clear);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_status);

	return 0;
}
/**
 * megasas_fire_cmd_gen2 -     Sends command to the FW
 * @frame_phys_addr :          Physical address of cmd
 * @frame_count :              Number of frames for the command
 * @regs :                     MFI register set
 */
static inline void
megasas_fire_cmd_gen2(struct megasas_instance *instance,
			dma_addr_t frame_phys_addr,
			u32 frame_count,
			struct megasas_register_set __iomem *regs)
{
	writel((frame_phys_addr | (frame_count<<1))|1,
			&(regs)->inbound_queue_port);
}

static struct megasas_instance_template megasas_instance_template_gen2 = {

	.fire_cmd = megasas_fire_cmd_gen2,
	.enable_intr = megasas_enable_intr_gen2,
	.disable_intr = megasas_disable_intr_gen2,
	.clear_intr = megasas_clear_intr_gen2,
	.read_fw_status_reg = megasas_read_fw_status_reg_gen2,
};

/**
*	This is the end of set of functions & definitions
* 	specific to ppc (deviceid : 0x60) controllers
*/

/**
 * megasas_issue_polled -	Issues a polling command
 * @instance:			Adapter soft state
 * @cmd:			Command packet to be issued 
 *
 * For polling, MFI requires the cmd_status to be set to 0xFF before posting.
 */
static int
megasas_issue_polled(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	int i;
	u32 msecs = MFI_POLL_TIMEOUT_SECS * 1000;

	struct megasas_header *frame_hdr = &cmd->frame->hdr;

	frame_hdr->cmd_status = 0xFF;
	frame_hdr->flags |= MFI_FRAME_DONT_POST_IN_REPLY_QUEUE;

	/*
	 * Issue the frame using inbound queue port
	 */
	instance->instancet->fire_cmd(instance,
			cmd->frame_phys_addr, 0, instance->reg_set);

	/*
	 * Wait for cmd_status to change
	 */
	for (i = 0; (i < msecs) && (frame_hdr->cmd_status == 0xff); i++) {
		rmb();
		msleep(1);
	}

	if (frame_hdr->cmd_status == 0xff)
		return -ETIME;

	return 0;
}

/**
 * megasas_issue_blocked_cmd -	Synchronous wrapper around regular FW cmds
 * @instance:			Adapter soft state
 * @cmd:			Command to be issued
 *
 * This function waits on an event for the command to be returned from ISR.
 * Max wait time is MEGASAS_INTERNAL_CMD_WAIT_TIME secs
 * Used to issue ioctl commands.
 */
static int
megasas_issue_blocked_cmd(struct megasas_instance *instance,
			  struct megasas_cmd *cmd)
{
	cmd->cmd_status = ENODATA;

	instance->instancet->fire_cmd(instance,
			cmd->frame_phys_addr, 0, instance->reg_set);

	wait_event_timeout(instance->int_cmd_wait_q, (cmd->cmd_status != ENODATA),
		MEGASAS_INTERNAL_CMD_WAIT_TIME*HZ);

	return 0;
}

/**
 * megasas_issue_blocked_abort_cmd -	Aborts previously issued cmd
 * @instance:				Adapter soft state
 * @cmd_to_abort:			Previously issued cmd to be aborted
 *
 * MFI firmware can abort previously issued AEN comamnd (automatic event
 * notification). The megasas_issue_blocked_abort_cmd() issues such abort
 * cmd and waits for return status.
 * Max wait time is MEGASAS_INTERNAL_CMD_WAIT_TIME secs
 */
static int
megasas_issue_blocked_abort_cmd(struct megasas_instance *instance,
				struct megasas_cmd *cmd_to_abort)
{
	struct megasas_cmd *cmd;
	struct megasas_abort_frame *abort_fr;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return -1;

	abort_fr = &cmd->frame->abort;

	/*
	 * Prepare and issue the abort frame
	 */
	abort_fr->cmd = MFI_CMD_ABORT;
	abort_fr->cmd_status = 0xFF;
	abort_fr->flags = 0;
	abort_fr->abort_context = cmd_to_abort->index;
	abort_fr->abort_mfi_phys_addr_lo = cmd_to_abort->frame_phys_addr;
	abort_fr->abort_mfi_phys_addr_hi = 0;

	cmd->sync_cmd = 1;
	cmd->cmd_status = 0xFF;

	instance->instancet->fire_cmd(instance,
			cmd->frame_phys_addr, 0, instance->reg_set);

	/*
	 * Wait for this cmd to complete
	 */
	wait_event_timeout(instance->abort_cmd_wait_q, (cmd->cmd_status != 0xFF),
		MEGASAS_INTERNAL_CMD_WAIT_TIME*HZ);

	megasas_return_cmd(instance, cmd);
	return 0;
}

/**
 * megasas_make_sgl32 -	Prepares 32-bit SGL
 * @instance:		Adapter soft state
 * @scp:		SCSI command from the mid-layer
 * @mfi_sgl:		SGL to be filled in
 *
 * If successful, this function returns the number of SG elements. Otherwise,
 * it returnes -1.
 */
static int
megasas_make_sgl32(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	sge_count = scsi_dma_map(scp);
	BUG_ON(sge_count < 0);

	if (sge_count) {
		scsi_for_each_sg(scp, os_sgl, sge_count, i) {
			mfi_sgl->sge32[i].length = sg_dma_len(os_sgl);
			mfi_sgl->sge32[i].phys_addr = sg_dma_address(os_sgl);
		}
	}
	return sge_count;
}

/**
 * megasas_make_sgl64 -	Prepares 64-bit SGL
 * @instance:		Adapter soft state
 * @scp:		SCSI command from the mid-layer
 * @mfi_sgl:		SGL to be filled in
 *
 * If successful, this function returns the number of SG elements. Otherwise,
 * it returnes -1.
 */
static int
megasas_make_sgl64(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	sge_count = scsi_dma_map(scp);
	BUG_ON(sge_count < 0);

	if (sge_count) {
		scsi_for_each_sg(scp, os_sgl, sge_count, i) {
			mfi_sgl->sge64[i].length = sg_dma_len(os_sgl);
			mfi_sgl->sge64[i].phys_addr = sg_dma_address(os_sgl);
		}
	}
	return sge_count;
}

/**
 * megasas_make_sgl_skinny - Prepares IEEE SGL
 * @instance:           Adapter soft state
 * @scp:                SCSI command from the mid-layer
 * @mfi_sgl:            SGL to be filled in
 *
 * If successful, this function returns the number of SG elements. Otherwise,
 * it returnes -1.
 */
static int
megasas_make_sgl_skinny(struct megasas_instance *instance,
		struct scsi_cmnd *scp, union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	sge_count = scsi_dma_map(scp);

	if (sge_count) {
		scsi_for_each_sg(scp, os_sgl, sge_count, i) {
			mfi_sgl->sge_skinny[i].length = sg_dma_len(os_sgl);
			mfi_sgl->sge_skinny[i].phys_addr =
						sg_dma_address(os_sgl);
		}
	}
	return sge_count;
}

 /**
 * megasas_get_frame_count - Computes the number of frames
 * @frame_type		: type of frame- io or pthru frame
 * @sge_count		: number of sg elements
 *
 * Returns the number of frames required for numnber of sge's (sge_count)
 */

static u32 megasas_get_frame_count(struct megasas_instance *instance,
			u8 sge_count, u8 frame_type)
{
	int num_cnt;
	int sge_bytes;
	u32 sge_sz;
	u32 frame_count=0;

	sge_sz = (IS_DMA64) ? sizeof(struct megasas_sge64) :
	    sizeof(struct megasas_sge32);

	if (instance->flag_ieee) {
		sge_sz = sizeof(struct megasas_sge_skinny);
	}

	/*
	 * Main frame can contain 2 SGEs for 64-bit SGLs and
	 * 3 SGEs for 32-bit SGLs for ldio &
	 * 1 SGEs for 64-bit SGLs and
	 * 2 SGEs for 32-bit SGLs for pthru frame
	 */
	if (unlikely(frame_type == PTHRU_FRAME)) {
		if (instance->flag_ieee == 1) {
			num_cnt = sge_count - 1;
		} else if (IS_DMA64)
			num_cnt = sge_count - 1;
		else
			num_cnt = sge_count - 2;
	} else {
		if (instance->flag_ieee == 1) {
			num_cnt = sge_count - 1;
		} else if (IS_DMA64)
			num_cnt = sge_count - 2;
		else
			num_cnt = sge_count - 3;
	}

	if(num_cnt>0){
		sge_bytes = sge_sz * num_cnt;

		frame_count = (sge_bytes / MEGAMFI_FRAME_SIZE) +
		    ((sge_bytes % MEGAMFI_FRAME_SIZE) ? 1 : 0) ;
	}
	/* Main frame */
	frame_count +=1;

	if (frame_count > 7)
		frame_count = 8;
	return frame_count;
}

/**
 * megasas_build_dcdb -	Prepares a direct cdb (DCDB) command
 * @instance:		Adapter soft state
 * @scp:		SCSI command
 * @cmd:		Command to be prepared in
 *
 * This function prepares CDB commands. These are typcially pass-through
 * commands to the devices.
 */
static int
megasas_build_dcdb(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   struct megasas_cmd *cmd)
{
	u32 is_logical;
	u32 device_id;
	u16 flags = 0;
	struct megasas_pthru_frame *pthru;

	is_logical = MEGASAS_IS_LOGICAL(scp);
	device_id = MEGASAS_DEV_INDEX(instance, scp);
	pthru = (struct megasas_pthru_frame *)cmd->frame;

	if (scp->sc_data_direction == PCI_DMA_TODEVICE)
		flags = MFI_FRAME_DIR_WRITE;
	else if (scp->sc_data_direction == PCI_DMA_FROMDEVICE)
		flags = MFI_FRAME_DIR_READ;
	else if (scp->sc_data_direction == PCI_DMA_NONE)
		flags = MFI_FRAME_DIR_NONE;

	if (instance->flag_ieee == 1) {
		flags |= MFI_FRAME_IEEE;
	}

	/*
	 * Prepare the DCDB frame
	 */
	pthru->cmd = (is_logical) ? MFI_CMD_LD_SCSI_IO : MFI_CMD_PD_SCSI_IO;
	pthru->cmd_status = 0x0;
	pthru->scsi_status = 0x0;
	pthru->target_id = device_id;
	pthru->lun = scp->device->lun;
	pthru->cdb_len = scp->cmd_len;
	pthru->timeout = 0;
	pthru->pad_0 = 0;
	pthru->flags = flags;
	pthru->data_xfer_len = scsi_bufflen(scp);

	memcpy(pthru->cdb, scp->cmnd, scp->cmd_len);

	/*
	* If the command is for the tape device, set the
	* pthru timeout to the os layer timeout value.
	*/
	if (scp->device->type == TYPE_TAPE) {
		if ((scp->request->timeout / HZ) > 0xFFFF)
			pthru->timeout = 0xFFFF;
		else
			pthru->timeout = scp->request->timeout / HZ;
	}

	/*
	 * Construct SGL
	 */
	if (instance->flag_ieee == 1) {
		pthru->flags |= MFI_FRAME_SGL64;
		pthru->sge_count = megasas_make_sgl_skinny(instance, scp,
						      &pthru->sgl);
	} else if (IS_DMA64) {
		pthru->flags |= MFI_FRAME_SGL64;
		pthru->sge_count = megasas_make_sgl64(instance, scp,
						      &pthru->sgl);
	} else
		pthru->sge_count = megasas_make_sgl32(instance, scp,
						      &pthru->sgl);

	if (pthru->sge_count > instance->max_num_sge) {
		printk(KERN_ERR "megasas: DCDB two many SGE NUM=%x\n",
			pthru->sge_count);
		return 0;
	}

	/*
	 * Sense info specific
	 */
	pthru->sense_len = SCSI_SENSE_BUFFERSIZE;
	pthru->sense_buf_phys_addr_hi = 0;
	pthru->sense_buf_phys_addr_lo = cmd->sense_phys_addr;

	/*
	 * Compute the total number of frames this command consumes. FW uses
	 * this number to pull sufficient number of frames from host memory.
	 */
	cmd->frame_count = megasas_get_frame_count(instance, pthru->sge_count,
							PTHRU_FRAME);

	return cmd->frame_count;
}

/**
 * megasas_build_ldio -	Prepares IOs to logical devices
 * @instance:		Adapter soft state
 * @scp:		SCSI command
 * @cmd:		Command to be prepared
 *
 * Frames (and accompanying SGLs) for regular SCSI IOs use this function.
 */
static int
megasas_build_ldio(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   struct megasas_cmd *cmd)
{
	u32 device_id;
	u8 sc = scp->cmnd[0];
	u16 flags = 0;
	struct megasas_io_frame *ldio;

	device_id = MEGASAS_DEV_INDEX(instance, scp);
	ldio = (struct megasas_io_frame *)cmd->frame;

	if (scp->sc_data_direction == PCI_DMA_TODEVICE)
		flags = MFI_FRAME_DIR_WRITE;
	else if (scp->sc_data_direction == PCI_DMA_FROMDEVICE)
		flags = MFI_FRAME_DIR_READ;

	if (instance->flag_ieee == 1) {
		flags |= MFI_FRAME_IEEE;
	}

	/*
	 * Prepare the Logical IO frame: 2nd bit is zero for all read cmds
	 */
	ldio->cmd = (sc & 0x02) ? MFI_CMD_LD_WRITE : MFI_CMD_LD_READ;
	ldio->cmd_status = 0x0;
	ldio->scsi_status = 0x0;
	ldio->target_id = device_id;
	ldio->timeout = 0;
	ldio->reserved_0 = 0;
	ldio->pad_0 = 0;
	ldio->flags = flags;
	ldio->start_lba_hi = 0;
	ldio->access_byte = (scp->cmd_len != 6) ? scp->cmnd[1] : 0;

	/*
	 * 6-byte READ(0x08) or WRITE(0x0A) cdb
	 */
	if (scp->cmd_len == 6) {
		ldio->lba_count = (u32) scp->cmnd[4];
		ldio->start_lba_lo = ((u32) scp->cmnd[1] << 16) |
		    ((u32) scp->cmnd[2] << 8) | (u32) scp->cmnd[3];

		ldio->start_lba_lo &= 0x1FFFFF;
	}

	/*
	 * 10-byte READ(0x28) or WRITE(0x2A) cdb
	 */
	else if (scp->cmd_len == 10) {
		ldio->lba_count = (u32) scp->cmnd[8] |
		    ((u32) scp->cmnd[7] << 8);
		ldio->start_lba_lo = ((u32) scp->cmnd[2] << 24) |
		    ((u32) scp->cmnd[3] << 16) |
		    ((u32) scp->cmnd[4] << 8) | (u32) scp->cmnd[5];
	}

	/*
	 * 12-byte READ(0xA8) or WRITE(0xAA) cdb
	 */
	else if (scp->cmd_len == 12) {
		ldio->lba_count = ((u32) scp->cmnd[6] << 24) |
		    ((u32) scp->cmnd[7] << 16) |
		    ((u32) scp->cmnd[8] << 8) | (u32) scp->cmnd[9];

		ldio->start_lba_lo = ((u32) scp->cmnd[2] << 24) |
		    ((u32) scp->cmnd[3] << 16) |
		    ((u32) scp->cmnd[4] << 8) | (u32) scp->cmnd[5];
	}

	/*
	 * 16-byte READ(0x88) or WRITE(0x8A) cdb
	 */
	else if (scp->cmd_len == 16) {
		ldio->lba_count = ((u32) scp->cmnd[10] << 24) |
		    ((u32) scp->cmnd[11] << 16) |
		    ((u32) scp->cmnd[12] << 8) | (u32) scp->cmnd[13];

		ldio->start_lba_lo = ((u32) scp->cmnd[6] << 24) |
		    ((u32) scp->cmnd[7] << 16) |
		    ((u32) scp->cmnd[8] << 8) | (u32) scp->cmnd[9];

		ldio->start_lba_hi = ((u32) scp->cmnd[2] << 24) |
		    ((u32) scp->cmnd[3] << 16) |
		    ((u32) scp->cmnd[4] << 8) | (u32) scp->cmnd[5];

	}

	/*
	 * Construct SGL
	 */
	if (instance->flag_ieee) {
		ldio->flags |= MFI_FRAME_SGL64;
		ldio->sge_count = megasas_make_sgl_skinny(instance, scp,
					      &ldio->sgl);
	} else if (IS_DMA64) {
		ldio->flags |= MFI_FRAME_SGL64;
		ldio->sge_count = megasas_make_sgl64(instance, scp, &ldio->sgl);
	} else
		ldio->sge_count = megasas_make_sgl32(instance, scp, &ldio->sgl);

	if (ldio->sge_count > instance->max_num_sge) {
		printk(KERN_ERR "megasas: build_ld_io: sge_count = %x\n",
			ldio->sge_count);
		return 0;
	}

	/*
	 * Sense info specific
	 */
	ldio->sense_len = SCSI_SENSE_BUFFERSIZE;
	ldio->sense_buf_phys_addr_hi = 0;
	ldio->sense_buf_phys_addr_lo = cmd->sense_phys_addr;

	/*
	 * Compute the total number of frames this command consumes. FW uses
	 * this number to pull sufficient number of frames from host memory.
	 */
	cmd->frame_count = megasas_get_frame_count(instance,
			ldio->sge_count, IO_FRAME);

	return cmd->frame_count;
}

/**
 * megasas_is_ldio -		Checks if the cmd is for logical drive
 * @scmd:			SCSI command
 *	
 * Called by megasas_queue_command to find out if the command to be queued
 * is a logical drive command	
 */
static inline int megasas_is_ldio(struct scsi_cmnd *cmd)
{
	if (!MEGASAS_IS_LOGICAL(cmd))
		return 0;
	switch (cmd->cmnd[0]) {
	case READ_10:
	case WRITE_10:
	case READ_12:
	case WRITE_12:
	case READ_6:
	case WRITE_6:
	case READ_16:
	case WRITE_16:
		return 1;
	default:
		return 0;
	}
}

 /**
 * megasas_dump_pending_frames -	Dumps the frame address of all pending cmds
 *                              	in FW
 * @instance:				Adapter soft state
 */
static inline void
megasas_dump_pending_frames(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	int i,n;
	union megasas_sgl *mfi_sgl;
	struct megasas_io_frame *ldio;
	struct megasas_pthru_frame *pthru;
	u32 sgcount;
	u32 max_cmd = instance->max_fw_cmds;

	printk(KERN_ERR "\nmegasas[%d]: Dumping Frame Phys Address of all pending cmds in FW\n",instance->host->host_no);
	printk(KERN_ERR "megasas[%d]: Total OS Pending cmds : %d\n",instance->host->host_no,atomic_read(&instance->fw_outstanding));
	if (IS_DMA64)
		printk(KERN_ERR "\nmegasas[%d]: 64 bit SGLs were sent to FW\n",instance->host->host_no);
	else
		printk(KERN_ERR "\nmegasas[%d]: 32 bit SGLs were sent to FW\n",instance->host->host_no);

	printk(KERN_ERR "megasas[%d]: Pending OS cmds in FW : \n",instance->host->host_no);
	for (i = 0; i < max_cmd; i++) {
		cmd = instance->cmd_list[i];
		if(!cmd->scmd)
			continue;
		printk(KERN_ERR "megasas[%d]: Frame addr :0x%08lx : ",instance->host->host_no,(unsigned long)cmd->frame_phys_addr);
		if (megasas_is_ldio(cmd->scmd)){
			ldio = (struct megasas_io_frame *)cmd->frame;
			mfi_sgl = &ldio->sgl;
			sgcount = ldio->sge_count;
			printk(KERN_ERR "megasas[%d]: frame count : 0x%x, Cmd : 0x%x, Tgt id : 0x%x, lba lo : 0x%x, lba_hi : 0x%x, sense_buf addr : 0x%x,sge count : 0x%x\n",instance->host->host_no, cmd->frame_count,ldio->cmd,ldio->target_id, ldio->start_lba_lo,ldio->start_lba_hi,ldio->sense_buf_phys_addr_lo,sgcount);
		}
		else {
			pthru = (struct megasas_pthru_frame *) cmd->frame;
			mfi_sgl = &pthru->sgl;
			sgcount = pthru->sge_count;
			printk(KERN_ERR "megasas[%d]: frame count : 0x%x, Cmd : 0x%x, Tgt id : 0x%x, lun : 0x%x, cdb_len : 0x%x, data xfer len : 0x%x, sense_buf addr : 0x%x,sge count : 0x%x\n",instance->host->host_no,cmd->frame_count,pthru->cmd,pthru->target_id,pthru->lun,pthru->cdb_len , pthru->data_xfer_len,pthru->sense_buf_phys_addr_lo,sgcount);
		}
	if(megasas_dbg_lvl & MEGASAS_DBG_LVL){
		for (n = 0; n < sgcount; n++){
			if (IS_DMA64)
				printk(KERN_ERR "megasas: sgl len : 0x%x, sgl addr : 0x%08lx ",mfi_sgl->sge64[n].length , (unsigned long)mfi_sgl->sge64[n].phys_addr) ;
			else
				printk(KERN_ERR "megasas: sgl len : 0x%x, sgl addr : 0x%x ",mfi_sgl->sge32[n].length , mfi_sgl->sge32[n].phys_addr) ;
			}
		}
		printk(KERN_ERR "\n");
	} /*for max_cmd*/
	printk(KERN_ERR "\nmegasas[%d]: Pending Internal cmds in FW : \n",instance->host->host_no);
	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		if(cmd->sync_cmd == 1){
			printk(KERN_ERR "0x%08lx : ", (unsigned long)cmd->frame_phys_addr);
		}
	}
	printk(KERN_ERR "megasas[%d]: Dumping Done.\n\n",instance->host->host_no);
}

/**
 * megasas_queue_command -	Queue entry point
 * @scmd:			SCSI command to be queued
 * @done:			Callback entry point
 */
static int
megasas_queue_command(struct scsi_cmnd *scmd, void (*done) (struct scsi_cmnd *))
{
	u32 frame_count;
	struct megasas_cmd *cmd;
	struct megasas_instance *instance;

	instance = (struct megasas_instance *)
	    scmd->device->host->hostdata;

	/* Don't process if we have already declared adapter dead */
	if (instance->hw_crit_error)
		return SCSI_MLQUEUE_HOST_BUSY;

	scmd->scsi_done = done;
	scmd->result = 0;

	if (MEGASAS_IS_LOGICAL(scmd) &&
	    (scmd->device->id >= MEGASAS_MAX_LD || scmd->device->lun)) {
		scmd->result = DID_BAD_TARGET << 16;
		goto out_done;
	}

	switch (scmd->cmnd[0]) {
	case SYNCHRONIZE_CACHE:
		/*
		 * FW takes care of flush cache on its own
		 * No need to send it down
		 */
		scmd->result = DID_OK << 16;
		goto out_done;
	default:
		break;
	}

	cmd = megasas_get_cmd(instance);
	if (!cmd)
		return SCSI_MLQUEUE_HOST_BUSY;

	/*
	 * Logical drive command
	 */
	if (megasas_is_ldio(scmd))
		frame_count = megasas_build_ldio(instance, scmd, cmd);
	else
		frame_count = megasas_build_dcdb(instance, scmd, cmd);

	if (!frame_count)
		goto out_return_cmd;

	cmd->scmd = scmd;
	scmd->SCp.ptr = (char *)cmd;

	/*
	 * Issue the command to the FW
	 */
	atomic_inc(&instance->fw_outstanding);

	instance->instancet->fire_cmd(instance, cmd->frame_phys_addr,
				cmd->frame_count-1, instance->reg_set);
	/*
	 * Check if we have pend cmds to be completed
	 */
	if (poll_mode_io && atomic_read(&instance->fw_outstanding))
		tasklet_schedule(&instance->isr_tasklet);


	return 0;

 out_return_cmd:
	megasas_return_cmd(instance, cmd);
 out_done:
	done(scmd);
	return 0;
}

static struct megasas_instance *megasas_lookup_instance(u16 host_no)
{
	int i;

	for (i = 0; i < megasas_mgmt_info.max_index; i++) {

		if ((megasas_mgmt_info.instance[i]) &&
		    (megasas_mgmt_info.instance[i]->host->host_no == host_no))
			return megasas_mgmt_info.instance[i];
	}

	return NULL;
}

static int megasas_slave_configure(struct scsi_device *sdev)
{
	u16             pd_index = 0;
	struct  megasas_instance *instance ;

	instance = megasas_lookup_instance(sdev->host->host_no);

	if (sdev->channel < MEGASAS_MAX_PD_CHANNELS &&
				sdev->type == TYPE_DISK) {
		pd_index = (sdev->channel * MEGASAS_MAX_DEV_PER_CHANNEL) +
								sdev->id;
		if (instance->pd_list[pd_index].driveState ==
						MR_PD_STATE_SYSTEM) {
			blk_queue_rq_timeout(sdev->request_queue,
				MEGASAS_DEFAULT_CMD_TIMEOUT * HZ);
			return 0;
		}
		return -ENXIO;
	}

	/*
	* The RAID firmware may require extended timeouts.
	*/
	blk_queue_rq_timeout(sdev->request_queue,
		MEGASAS_DEFAULT_CMD_TIMEOUT * HZ);
	return 0;
}

static int megasas_slave_alloc(struct scsi_device *sdev)
{
	u16             pd_index = 0;
	struct megasas_instance *instance ;
	instance = megasas_lookup_instance(sdev->host->host_no);
	if ((sdev->channel < MEGASAS_MAX_PD_CHANNELS) &&
				(sdev->type == TYPE_DISK)) {
		/*
		 * Open the OS scan to the SYSTEM PD
		 */
		pd_index =
			(sdev->channel * MEGASAS_MAX_DEV_PER_CHANNEL) +
			sdev->id;
		if ((instance->pd_list[pd_index].driveState ==
					MR_PD_STATE_SYSTEM) &&
			(instance->pd_list[pd_index].driveType ==
						TYPE_DISK)) {
			return 0;
		}
		return -ENXIO;
	}
	return 0;
}

/**
 * megasas_complete_cmd_dpc	 -	Returns FW's controller structure
 * @instance_addr:			Address of adapter soft state
 *
 * Tasklet to complete cmds
 */
static void megasas_complete_cmd_dpc(unsigned long instance_addr)
{
	u32 producer;
	u32 consumer;
	u32 context;
	struct megasas_cmd *cmd;
	struct megasas_instance *instance =
				(struct megasas_instance *)instance_addr;
	unsigned long flags;

	/* If we have already declared adapter dead, donot complete cmds */
	if (instance->hw_crit_error)
		return;

	spin_lock_irqsave(&instance->completion_lock, flags);

	producer = *instance->producer;
	consumer = *instance->consumer;

	while (consumer != producer) {
		context = instance->reply_queue[consumer];

		cmd = instance->cmd_list[context];

		megasas_complete_cmd(instance, cmd, DID_OK);

		consumer++;
		if (consumer == (instance->max_fw_cmds + 1)) {
			consumer = 0;
		}
	}

	*instance->consumer = producer;

	spin_unlock_irqrestore(&instance->completion_lock, flags);

	/*
	 * Check if we can restore can_queue
	 */
	if (instance->flag & MEGASAS_FW_BUSY
		&& time_after(jiffies, instance->last_time + 5 * HZ)
		&& atomic_read(&instance->fw_outstanding) < 17) {

		spin_lock_irqsave(instance->host->host_lock, flags);
		instance->flag &= ~MEGASAS_FW_BUSY;
		if ((instance->pdev->device ==
			PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
			(instance->pdev->device ==
			PCI_DEVICE_ID_LSI_SAS0071SKINNY)) {
			instance->host->can_queue =
				instance->max_fw_cmds - MEGASAS_SKINNY_INT_CMDS;
		} else
			instance->host->can_queue =
				instance->max_fw_cmds - MEGASAS_INT_CMDS;

		spin_unlock_irqrestore(instance->host->host_lock, flags);
	}
}

/**
 * megasas_wait_for_outstanding -	Wait for all outstanding cmds
 * @instance:				Adapter soft state
 *
 * This function waits for upto MEGASAS_RESET_WAIT_TIME seconds for FW to
 * complete all its outstanding commands. Returns error if one or more IOs
 * are pending after this time period. It also marks the controller dead.
 */
static int megasas_wait_for_outstanding(struct megasas_instance *instance)
{
	int i;
	u32 wait_time = MEGASAS_RESET_WAIT_TIME;

	for (i = 0; i < wait_time; i++) {

		int outstanding = atomic_read(&instance->fw_outstanding);

		if (!outstanding)
			break;

		if (!(i % MEGASAS_RESET_NOTICE_INTERVAL)) {
			printk(KERN_NOTICE "megasas: [%2d]waiting for %d "
			       "commands to complete\n",i,outstanding);
			/*
			 * Call cmd completion routine. Cmd to be
			 * be completed directly without depending on isr.
			 */
			megasas_complete_cmd_dpc((unsigned long)instance);
		}

		msleep(1000);
	}

	if (atomic_read(&instance->fw_outstanding)) {
		/*
		* Send signal to FW to stop processing any pending cmds.
		* The controller will be taken offline by the OS now.
		*/
		if ((instance->pdev->device ==
			PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
			(instance->pdev->device ==
			PCI_DEVICE_ID_LSI_SAS0071SKINNY)) {
			writel(MFI_STOP_ADP,
				&instance->reg_set->reserved_0[0]);
		} else {
			writel(MFI_STOP_ADP,
				&instance->reg_set->inbound_doorbell);
		}
		megasas_dump_pending_frames(instance);
		instance->hw_crit_error = 1;
		return FAILED;
	}

	return SUCCESS;
}

/**
 * megasas_generic_reset -	Generic reset routine
 * @scmd:			Mid-layer SCSI command
 *
 * This routine implements a generic reset handler for device, bus and host
 * reset requests. Device, bus and host specific reset handlers can use this
 * function after they do their specific tasks.
 */
static int megasas_generic_reset(struct scsi_cmnd *scmd)
{
	int ret_val;
	struct megasas_instance *instance;

	instance = (struct megasas_instance *)scmd->device->host->hostdata;

	scmd_printk(KERN_NOTICE, scmd, "megasas: RESET -%ld cmd=%x retries=%x\n",
		 scmd->serial_number, scmd->cmnd[0], scmd->retries);

	if (instance->hw_crit_error) {
		printk(KERN_ERR "megasas: cannot recover from previous reset "
		       "failures\n");
		return FAILED;
	}

	ret_val = megasas_wait_for_outstanding(instance);
	if (ret_val == SUCCESS)
		printk(KERN_NOTICE "megasas: reset successful \n");
	else
		printk(KERN_ERR "megasas: failed to do reset\n");

	return ret_val;
}

/**
 * megasas_reset_timer - quiesce the adapter if required
 * @scmd:		scsi cmnd
 *
 * Sets the FW busy flag and reduces the host->can_queue if the
 * cmd has not been completed within the timeout period.
 */
static enum
blk_eh_timer_return megasas_reset_timer(struct scsi_cmnd *scmd)
{
	struct megasas_cmd *cmd = (struct megasas_cmd *)scmd->SCp.ptr;
	struct megasas_instance *instance;
	unsigned long flags;

	if (time_after(jiffies, scmd->jiffies_at_alloc +
				(MEGASAS_DEFAULT_CMD_TIMEOUT * 2) * HZ)) {
		return BLK_EH_NOT_HANDLED;
	}

	instance = cmd->instance;
	if (!(instance->flag & MEGASAS_FW_BUSY)) {
		/* FW is busy, throttle IO */
		spin_lock_irqsave(instance->host->host_lock, flags);

		instance->host->can_queue = 16;
		instance->last_time = jiffies;
		instance->flag |= MEGASAS_FW_BUSY;

		spin_unlock_irqrestore(instance->host->host_lock, flags);
	}
	return BLK_EH_RESET_TIMER;
}

/**
 * megasas_reset_device -	Device reset handler entry point
 */
static int megasas_reset_device(struct scsi_cmnd *scmd)
{
	int ret;

	/*
	 * First wait for all commands to complete
	 */
	ret = megasas_generic_reset(scmd);

	return ret;
}

/**
 * megasas_reset_bus_host -	Bus & host reset handler entry point
 */
static int megasas_reset_bus_host(struct scsi_cmnd *scmd)
{
	int ret;

	/*
	 * First wait for all commands to complete
	 */
	ret = megasas_generic_reset(scmd);

	return ret;
}

/**
 * megasas_bios_param - Returns disk geometry for a disk
 * @sdev: 		device handle
 * @bdev:		block device
 * @capacity:		drive capacity
 * @geom:		geometry parameters
 */
static int
megasas_bios_param(struct scsi_device *sdev, struct block_device *bdev,
		 sector_t capacity, int geom[])
{
	int heads;
	int sectors;
	sector_t cylinders;
	unsigned long tmp;
	/* Default heads (64) & sectors (32) */
	heads = 64;
	sectors = 32;

	tmp = heads * sectors;
	cylinders = capacity;

	sector_div(cylinders, tmp);

	/*
	 * Handle extended translation size for logical drives > 1Gb
	 */

	if (capacity >= 0x200000) {
		heads = 255;
		sectors = 63;
		tmp = heads*sectors;
		cylinders = capacity;
		sector_div(cylinders, tmp);
	}

	geom[0] = heads;
	geom[1] = sectors;
	geom[2] = cylinders;

	return 0;
}

static void megasas_aen_polling(struct work_struct *work);

/**
 * megasas_service_aen -	Processes an event notification
 * @instance:			Adapter soft state
 * @cmd:			AEN command completed by the ISR
 *
 * For AEN, driver sends a command down to FW that is held by the FW till an
 * event occurs. When an event of interest occurs, FW completes the command
 * that it was previously holding.
 *
 * This routines sends SIGIO signal to processes that have registered with the
 * driver for AEN.
 */
static void
megasas_service_aen(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	unsigned long flags;
	/*
	 * Don't signal app if it is just an aborted previously registered aen
	 */
	if ((!cmd->abort_aen) && (instance->unload == 0)) {
		spin_lock_irqsave(&poll_aen_lock, flags);
		megasas_poll_wait_aen = 1;
		spin_unlock_irqrestore(&poll_aen_lock, flags);
		wake_up(&megasas_poll_wait);
		kill_fasync(&megasas_async_queue, SIGIO, POLL_IN);
	}
	else
		cmd->abort_aen = 0;

	instance->aen_cmd = NULL;
	megasas_return_cmd(instance, cmd);

	if (instance->unload == 0) {
		struct megasas_aen_event *ev;
		ev = kzalloc(sizeof(*ev), GFP_ATOMIC);
		if (!ev) {
			printk(KERN_ERR "megasas_service_aen: out of memory\n");
		} else {
			ev->instance = instance;
			instance->ev = ev;
			INIT_WORK(&ev->hotplug_work, megasas_aen_polling);
			schedule_delayed_work(
				(struct delayed_work *)&ev->hotplug_work, 0);
		}
	}
}

/*
 * Scsi host template for megaraid_sas driver
 */
static struct scsi_host_template megasas_template = {

	.module = THIS_MODULE,
	.name = "LSI SAS based MegaRAID driver",
	.proc_name = "megaraid_sas",
	.slave_configure = megasas_slave_configure,
	.slave_alloc = megasas_slave_alloc,
	.queuecommand = megasas_queue_command,
	.eh_device_reset_handler = megasas_reset_device,
	.eh_bus_reset_handler = megasas_reset_bus_host,
	.eh_host_reset_handler = megasas_reset_bus_host,
	.eh_timed_out = megasas_reset_timer,
	.bios_param = megasas_bios_param,
	.use_clustering = ENABLE_CLUSTERING,
};

/**
 * megasas_complete_int_cmd -	Completes an internal command
 * @instance:			Adapter soft state
 * @cmd:			Command to be completed
 *
 * The megasas_issue_blocked_cmd() function waits for a command to complete
 * after it issues a command. This function wakes up that waiting routine by
 * calling wake_up() on the wait queue.
 */
static void
megasas_complete_int_cmd(struct megasas_instance *instance,
			 struct megasas_cmd *cmd)
{
	cmd->cmd_status = cmd->frame->io.cmd_status;

	if (cmd->cmd_status == ENODATA) {
		cmd->cmd_status = 0;
	}
	wake_up(&instance->int_cmd_wait_q);
}

/**
 * megasas_complete_abort -	Completes aborting a command
 * @instance:			Adapter soft state
 * @cmd:			Cmd that was issued to abort another cmd
 *
 * The megasas_issue_blocked_abort_cmd() function waits on abort_cmd_wait_q 
 * after it issues an abort on a previously issued command. This function 
 * wakes up all functions waiting on the same wait queue.
 */
static void
megasas_complete_abort(struct megasas_instance *instance,
		       struct megasas_cmd *cmd)
{
	if (cmd->sync_cmd) {
		cmd->sync_cmd = 0;
		cmd->cmd_status = 0;
		wake_up(&instance->abort_cmd_wait_q);
	}

	return;
}

/**
 * megasas_complete_cmd -	Completes a command
 * @instance:			Adapter soft state
 * @cmd:			Command to be completed
 * @alt_status:			If non-zero, use this value as status to 
 * 				SCSI mid-layer instead of the value returned
 * 				by the FW. This should be used if caller wants
 * 				an alternate status (as in the case of aborted
 * 				commands)
 */
static void
megasas_complete_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd,
		     u8 alt_status)
{
	int exception = 0;
	struct megasas_header *hdr = &cmd->frame->hdr;
	unsigned long flags;

	if (cmd->scmd)
		cmd->scmd->SCp.ptr = NULL;

	switch (hdr->cmd) {

	case MFI_CMD_PD_SCSI_IO:
	case MFI_CMD_LD_SCSI_IO:

		/*
		 * MFI_CMD_PD_SCSI_IO and MFI_CMD_LD_SCSI_IO could have been
		 * issued either through an IO path or an IOCTL path. If it
		 * was via IOCTL, we will send it to internal completion.
		 */
		if (cmd->sync_cmd) {
			cmd->sync_cmd = 0;
			megasas_complete_int_cmd(instance, cmd);
			break;
		}

	case MFI_CMD_LD_READ:
	case MFI_CMD_LD_WRITE:

		if (alt_status) {
			cmd->scmd->result = alt_status << 16;
			exception = 1;
		}

		if (exception) {

			atomic_dec(&instance->fw_outstanding);

			scsi_dma_unmap(cmd->scmd);
			cmd->scmd->scsi_done(cmd->scmd);
			megasas_return_cmd(instance, cmd);

			break;
		}

		switch (hdr->cmd_status) {

		case MFI_STAT_OK:
			cmd->scmd->result = DID_OK << 16;
			break;

		case MFI_STAT_SCSI_IO_FAILED:
		case MFI_STAT_LD_INIT_IN_PROGRESS:
			cmd->scmd->result =
			    (DID_ERROR << 16) | hdr->scsi_status;
			break;

		case MFI_STAT_SCSI_DONE_WITH_ERROR:

			cmd->scmd->result = (DID_OK << 16) | hdr->scsi_status;

			if (hdr->scsi_status == SAM_STAT_CHECK_CONDITION) {
				memset(cmd->scmd->sense_buffer, 0,
				       SCSI_SENSE_BUFFERSIZE);
				memcpy(cmd->scmd->sense_buffer, cmd->sense,
				       hdr->sense_len);

				cmd->scmd->result |= DRIVER_SENSE << 24;
			}

			break;

		case MFI_STAT_LD_OFFLINE:
		case MFI_STAT_DEVICE_NOT_FOUND:
			cmd->scmd->result = DID_BAD_TARGET << 16;
			break;

		default:
			printk(KERN_DEBUG "megasas: MFI FW status %#x\n",
			       hdr->cmd_status);
			cmd->scmd->result = DID_ERROR << 16;
			break;
		}

		atomic_dec(&instance->fw_outstanding);

		scsi_dma_unmap(cmd->scmd);
		cmd->scmd->scsi_done(cmd->scmd);
		megasas_return_cmd(instance, cmd);

		break;

	case MFI_CMD_SMP:
	case MFI_CMD_STP:
	case MFI_CMD_DCMD:
		if (cmd->frame->dcmd.opcode == MR_DCMD_CTRL_EVENT_GET_INFO ||
			cmd->frame->dcmd.opcode == MR_DCMD_CTRL_EVENT_GET) {
			spin_lock_irqsave(&poll_aen_lock, flags);
			megasas_poll_wait_aen = 0;
			spin_unlock_irqrestore(&poll_aen_lock, flags);
		}

		/*
		 * See if got an event notification
		 */
		if (cmd->frame->dcmd.opcode == MR_DCMD_CTRL_EVENT_WAIT)
			megasas_service_aen(instance, cmd);
		else
			megasas_complete_int_cmd(instance, cmd);

		break;

	case MFI_CMD_ABORT:
		/*
		 * Cmd issued to abort another cmd returned
		 */
		megasas_complete_abort(instance, cmd);
		break;

	default:
		printk("megasas: Unknown command completed! [0x%X]\n",
		       hdr->cmd);
		break;
	}
}

/**
 * megasas_deplete_reply_queue -	Processes all completed commands
 * @instance:				Adapter soft state
 * @alt_status:				Alternate status to be returned to
 * 					SCSI mid-layer instead of the status
 * 					returned by the FW
 */
static int
megasas_deplete_reply_queue(struct megasas_instance *instance, u8 alt_status)
{
	/*
	 * Check if it is our interrupt
	 * Clear the interrupt 
	 */
	if(instance->instancet->clear_intr(instance->reg_set))
		return IRQ_NONE;

	if (instance->hw_crit_error)
		goto out_done;
        /*
	 * Schedule the tasklet for cmd completion
	 */
	tasklet_schedule(&instance->isr_tasklet);
out_done:
	return IRQ_HANDLED;
}

/**
 * megasas_isr - isr entry point
 */
static irqreturn_t megasas_isr(int irq, void *devp)
{
	return megasas_deplete_reply_queue((struct megasas_instance *)devp,
					   DID_OK);
}

/**
 * megasas_transition_to_ready -	Move the FW to READY state
 * @instance:				Adapter soft state
 *
 * During the initialization, FW passes can potentially be in any one of
 * several possible states. If the FW in operational, waiting-for-handshake
 * states, driver must take steps to bring it to ready state. Otherwise, it
 * has to wait for the ready state.
 */
static int
megasas_transition_to_ready(struct megasas_instance* instance)
{
	int i;
	u8 max_wait;
	u32 fw_state;
	u32 cur_state;
	u32 abs_state, curr_abs_state;

	fw_state = instance->instancet->read_fw_status_reg(instance->reg_set) & MFI_STATE_MASK;

	if (fw_state != MFI_STATE_READY)
 		printk(KERN_INFO "megasas: Waiting for FW to come to ready"
 		       " state\n");

	while (fw_state != MFI_STATE_READY) {

		abs_state =
		instance->instancet->read_fw_status_reg(instance->reg_set);

		switch (fw_state) {

		case MFI_STATE_FAULT:

			printk(KERN_DEBUG "megasas: FW in FAULT state!!\n");
			return -ENODEV;

		case MFI_STATE_WAIT_HANDSHAKE:
			/*
			 * Set the CLR bit in inbound doorbell
			 */
			if ((instance->pdev->device ==
				PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
				(instance->pdev->device ==
				PCI_DEVICE_ID_LSI_SAS0071SKINNY)) {

				writel(
				  MFI_INIT_CLEAR_HANDSHAKE|MFI_INIT_HOTPLUG,
				  &instance->reg_set->reserved_0[0]);
			} else {
				writel(
				    MFI_INIT_CLEAR_HANDSHAKE|MFI_INIT_HOTPLUG,
					&instance->reg_set->inbound_doorbell);
			}

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_WAIT_HANDSHAKE;
			break;

		case MFI_STATE_BOOT_MESSAGE_PENDING:
			if ((instance->pdev->device ==
				PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
			(instance->pdev->device ==
				PCI_DEVICE_ID_LSI_SAS0071SKINNY)) {
				writel(MFI_INIT_HOTPLUG,
				&instance->reg_set->reserved_0[0]);
			} else
				writel(MFI_INIT_HOTPLUG,
					&instance->reg_set->inbound_doorbell);

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_BOOT_MESSAGE_PENDING;
			break;

		case MFI_STATE_OPERATIONAL:
			/*
			 * Bring it to READY state; assuming max wait 10 secs
			 */
			instance->instancet->disable_intr(instance->reg_set);
			if ((instance->pdev->device ==
				PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
				(instance->pdev->device ==
				PCI_DEVICE_ID_LSI_SAS0071SKINNY)) {
				writel(MFI_RESET_FLAGS,
					&instance->reg_set->reserved_0[0]);
			} else
				writel(MFI_RESET_FLAGS,
					&instance->reg_set->inbound_doorbell);

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_OPERATIONAL;
			break;

		case MFI_STATE_UNDEFINED:
			/*
			 * This state should not last for more than 2 seconds
			 */
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_UNDEFINED;
			break;

		case MFI_STATE_BB_INIT:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_BB_INIT;
			break;

		case MFI_STATE_FW_INIT:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FW_INIT;
			break;

		case MFI_STATE_FW_INIT_2:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FW_INIT_2;
			break;

		case MFI_STATE_DEVICE_SCAN:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_DEVICE_SCAN;
			break;

		case MFI_STATE_FLUSH_CACHE:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FLUSH_CACHE;
			break;

		default:
			printk(KERN_DEBUG "megasas: Unknown state 0x%x\n",
			       fw_state);
			return -ENODEV;
		}

		/*
		 * The cur_state should not last for more than max_wait secs
		 */
		for (i = 0; i < (max_wait * 1000); i++) {
			fw_state = instance->instancet->read_fw_status_reg(instance->reg_set) &  
					MFI_STATE_MASK ;
		curr_abs_state =
		instance->instancet->read_fw_status_reg(instance->reg_set);

			if (abs_state == curr_abs_state) {
				msleep(1);
			} else
				break;
		}

		/*
		 * Return error if fw_state hasn't changed after max_wait
		 */
		if (curr_abs_state == abs_state) {
			printk(KERN_DEBUG "FW state [%d] hasn't changed "
			       "in %d secs\n", fw_state, max_wait);
			return -ENODEV;
		}
	};
 	printk(KERN_INFO "megasas: FW now in Ready state\n");

	return 0;
}

/**
 * megasas_teardown_frame_pool -	Destroy the cmd frame DMA pool
 * @instance:				Adapter soft state
 */
static void megasas_teardown_frame_pool(struct megasas_instance *instance)
{
	int i;
	u32 max_cmd = instance->max_fw_cmds;
	struct megasas_cmd *cmd;

	if (!instance->frame_dma_pool)
		return;

	/*
	 * Return all frames to pool
	 */
	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		if (cmd->frame)
			pci_pool_free(instance->frame_dma_pool, cmd->frame,
				      cmd->frame_phys_addr);

		if (cmd->sense)
			pci_pool_free(instance->sense_dma_pool, cmd->sense,
				      cmd->sense_phys_addr);
	}

	/*
	 * Now destroy the pool itself
	 */
	pci_pool_destroy(instance->frame_dma_pool);
	pci_pool_destroy(instance->sense_dma_pool);

	instance->frame_dma_pool = NULL;
	instance->sense_dma_pool = NULL;
}

/**
 * megasas_create_frame_pool -	Creates DMA pool for cmd frames
 * @instance:			Adapter soft state
 *
 * Each command packet has an embedded DMA memory buffer that is used for
 * filling MFI frame and the SG list that immediately follows the frame. This
 * function creates those DMA memory buffers for each command packet by using
 * PCI pool facility.
 */
static int megasas_create_frame_pool(struct megasas_instance *instance)
{
	int i;
	u32 max_cmd;
	u32 sge_sz;
	u32 sgl_sz;
	u32 total_sz;
	u32 frame_count;
	struct megasas_cmd *cmd;

	max_cmd = instance->max_fw_cmds;

	/*
	 * Size of our frame is 64 bytes for MFI frame, followed by max SG
	 * elements and finally SCSI_SENSE_BUFFERSIZE bytes for sense buffer
	 */
	sge_sz = (IS_DMA64) ? sizeof(struct megasas_sge64) :
	    sizeof(struct megasas_sge32);

	if (instance->flag_ieee) {
		sge_sz = sizeof(struct megasas_sge_skinny);
	}

	/*
	 * Calculated the number of 64byte frames required for SGL
	 */
	sgl_sz = sge_sz * instance->max_num_sge;
	frame_count = (sgl_sz + MEGAMFI_FRAME_SIZE - 1) / MEGAMFI_FRAME_SIZE;

	/*
	 * We need one extra frame for the MFI command
	 */
	frame_count++;

	total_sz = MEGAMFI_FRAME_SIZE * frame_count;
	/*
	 * Use DMA pool facility provided by PCI layer
	 */
	instance->frame_dma_pool = pci_pool_create("megasas frame pool",
						   instance->pdev, total_sz, 64,
						   0);

	if (!instance->frame_dma_pool) {
		printk(KERN_DEBUG "megasas: failed to setup frame pool\n");
		return -ENOMEM;
	}

	instance->sense_dma_pool = pci_pool_create("megasas sense pool",
						   instance->pdev, 128, 4, 0);

	if (!instance->sense_dma_pool) {
		printk(KERN_DEBUG "megasas: failed to setup sense pool\n");

		pci_pool_destroy(instance->frame_dma_pool);
		instance->frame_dma_pool = NULL;

		return -ENOMEM;
	}

	/*
	 * Allocate and attach a frame to each of the commands in cmd_list.
	 * By making cmd->index as the context instead of the &cmd, we can
	 * always use 32bit context regardless of the architecture
	 */
	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		cmd->frame = pci_pool_alloc(instance->frame_dma_pool,
					    GFP_KERNEL, &cmd->frame_phys_addr);

		cmd->sense = pci_pool_alloc(instance->sense_dma_pool,
					    GFP_KERNEL, &cmd->sense_phys_addr);

		/*
		 * megasas_teardown_frame_pool() takes care of freeing
		 * whatever has been allocated
		 */
		if (!cmd->frame || !cmd->sense) {
			printk(KERN_DEBUG "megasas: pci_pool_alloc failed \n");
			megasas_teardown_frame_pool(instance);
			return -ENOMEM;
		}

		cmd->frame->io.context = cmd->index;
		cmd->frame->io.pad_0 = 0;
	}

	return 0;
}

/**
 * megasas_free_cmds -	Free all the cmds in the free cmd pool
 * @instance:		Adapter soft state
 */
static void megasas_free_cmds(struct megasas_instance *instance)
{
	int i;
	/* First free the MFI frame pool */
	megasas_teardown_frame_pool(instance);

	/* Free all the commands in the cmd_list */
	for (i = 0; i < instance->max_fw_cmds; i++)
		kfree(instance->cmd_list[i]);

	/* Free the cmd_list buffer itself */
	kfree(instance->cmd_list);
	instance->cmd_list = NULL;

	INIT_LIST_HEAD(&instance->cmd_pool);
}

/**
 * megasas_alloc_cmds -	Allocates the command packets
 * @instance:		Adapter soft state
 *
 * Each command that is issued to the FW, whether IO commands from the OS or
 * internal commands like IOCTLs, are wrapped in local data structure called
 * megasas_cmd. The frame embedded in this megasas_cmd is actually issued to
 * the FW.
 *
 * Each frame has a 32-bit field called context (tag). This context is used
 * to get back the megasas_cmd from the frame when a frame gets completed in
 * the ISR. Typically the address of the megasas_cmd itself would be used as
 * the context. But we wanted to keep the differences between 32 and 64 bit
 * systems to the mininum. We always use 32 bit integers for the context. In
 * this driver, the 32 bit values are the indices into an array cmd_list.
 * This array is used only to look up the megasas_cmd given the context. The
 * free commands themselves are maintained in a linked list called cmd_pool.
 */
static int megasas_alloc_cmds(struct megasas_instance *instance)
{
	int i;
	int j;
	u32 max_cmd;
	struct megasas_cmd *cmd;

	max_cmd = instance->max_fw_cmds;

	/*
	 * instance->cmd_list is an array of struct megasas_cmd pointers.
	 * Allocate the dynamic array first and then allocate individual
	 * commands.
	 */
	instance->cmd_list = kcalloc(max_cmd, sizeof(struct megasas_cmd*), GFP_KERNEL);

	if (!instance->cmd_list) {
		printk(KERN_DEBUG "megasas: out of memory\n");
		return -ENOMEM;
	}


	for (i = 0; i < max_cmd; i++) {
		instance->cmd_list[i] = kmalloc(sizeof(struct megasas_cmd),
						GFP_KERNEL);

		if (!instance->cmd_list[i]) {

			for (j = 0; j < i; j++)
				kfree(instance->cmd_list[j]);

			kfree(instance->cmd_list);
			instance->cmd_list = NULL;

			return -ENOMEM;
		}
	}

	/*
	 * Add all the commands to command pool (instance->cmd_pool)
	 */
	for (i = 0; i < max_cmd; i++) {
		cmd = instance->cmd_list[i];
		memset(cmd, 0, sizeof(struct megasas_cmd));
		cmd->index = i;
		cmd->instance = instance;

		list_add_tail(&cmd->list, &instance->cmd_pool);
	}

	/*
	 * Create a frame pool and assign one frame to each cmd
	 */
	if (megasas_create_frame_pool(instance)) {
		printk(KERN_DEBUG "megasas: Error creating frame DMA pool\n");
		megasas_free_cmds(instance);
	}

	return 0;
}

/*
 * megasas_get_pd_list_info -	Returns FW's pd_list structure
 * @instance:				Adapter soft state
 * @pd_list:				pd_list structure
 *
 * Issues an internal command (DCMD) to get the FW's controller PD
 * list structure.  This information is mainly used to find out SYSTEM
 * supported by the FW.
 */
static int
megasas_get_pd_list(struct megasas_instance *instance)
{
	int ret = 0, pd_index = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct MR_PD_LIST *ci;
	struct MR_PD_ADDRESS *pd_addr;
	dma_addr_t ci_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas (get_pd_list): Failed to get cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
		  MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST), &ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem for pd_list\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->mbox.b[0] = MR_PD_QUERY_TYPE_EXPOSED_TO_HOST;
	dcmd->mbox.b[1] = 0;
	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0xFF;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST);
	dcmd->opcode = MR_DCMD_PD_LIST_QUERY;
	dcmd->sgl.sge32[0].phys_addr = ci_h;
	dcmd->sgl.sge32[0].length = MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST);

	if (!megasas_issue_polled(instance, cmd)) {
		ret = 0;
	} else {
		ret = -1;
	}

	/*
	* the following function will get the instance PD LIST.
	*/

	pd_addr = ci->addr;

	if ( ret == 0 &&
		(ci->count <
		  (MEGASAS_MAX_PD_CHANNELS * MEGASAS_MAX_DEV_PER_CHANNEL))) {

		memset(instance->pd_list, 0,
			MEGASAS_MAX_PD * sizeof(struct megasas_pd_list));

		for (pd_index = 0; pd_index < ci->count; pd_index++) {

			instance->pd_list[pd_addr->deviceId].tid	=
							pd_addr->deviceId;
			instance->pd_list[pd_addr->deviceId].driveType	=
							pd_addr->scsiDevType;
			instance->pd_list[pd_addr->deviceId].driveState	=
							MR_PD_STATE_SYSTEM;
			pd_addr++;
		}
	}

	pci_free_consistent(instance->pdev,
				MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST),
				ci, ci_h);
	megasas_return_cmd(instance, cmd);

	return ret;
}

/*
 * megasas_get_ld_list_info -	Returns FW's ld_list structure
 * @instance:				Adapter soft state
 * @ld_list:				ld_list structure
 *
 * Issues an internal command (DCMD) to get the FW's controller PD
 * list structure.  This information is mainly used to find out SYSTEM
 * supported by the FW.
 */
static int
megasas_get_ld_list(struct megasas_instance *instance)
{
	int ret = 0, ld_index = 0, ids = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct MR_LD_LIST *ci;
	dma_addr_t ci_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas_get_ld_list: Failed to get cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
				sizeof(struct MR_LD_LIST),
				&ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem in get_ld_list\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0xFF;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->data_xfer_len = sizeof(struct MR_LD_LIST);
	dcmd->opcode = MR_DCMD_LD_GET_LIST;
	dcmd->sgl.sge32[0].phys_addr = ci_h;
	dcmd->sgl.sge32[0].length = sizeof(struct MR_LD_LIST);
	dcmd->pad_0  = 0;

	if (!megasas_issue_polled(instance, cmd)) {
		ret = 0;
	} else {
		ret = -1;
	}

	/* the following function will get the instance PD LIST */

	if ((ret == 0) && (ci->ldCount < MAX_LOGICAL_DRIVES)) {
		memset(instance->ld_ids, 0xff, MEGASAS_MAX_LD_IDS);

		for (ld_index = 0; ld_index < ci->ldCount; ld_index++) {
			if (ci->ldList[ld_index].state != 0) {
				ids = ci->ldList[ld_index].ref.targetId;
				instance->ld_ids[ids] =
					ci->ldList[ld_index].ref.targetId;
			}
		}
	}

	pci_free_consistent(instance->pdev,
				sizeof(struct MR_LD_LIST),
				ci,
				ci_h);

	megasas_return_cmd(instance, cmd);
	return ret;
}

/**
 * megasas_get_controller_info -	Returns FW's controller structure
 * @instance:				Adapter soft state
 * @ctrl_info:				Controller information structure
 *
 * Issues an internal command (DCMD) to get the FW's controller structure.
 * This information is mainly used to find out the maximum IO transfer per
 * command supported by the FW.
 */
static int
megasas_get_ctrl_info(struct megasas_instance *instance,
		      struct megasas_ctrl_info *ctrl_info)
{
	int ret = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct megasas_ctrl_info *ci;
	dma_addr_t ci_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas: Failed to get a free cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
				  sizeof(struct megasas_ctrl_info), &ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem for ctrl info\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0xFF;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = sizeof(struct megasas_ctrl_info);
	dcmd->opcode = MR_DCMD_CTRL_GET_INFO;
	dcmd->sgl.sge32[0].phys_addr = ci_h;
	dcmd->sgl.sge32[0].length = sizeof(struct megasas_ctrl_info);

	if (!megasas_issue_polled(instance, cmd)) {
		ret = 0;
		memcpy(ctrl_info, ci, sizeof(struct megasas_ctrl_info));
	} else {
		ret = -1;
	}

	pci_free_consistent(instance->pdev, sizeof(struct megasas_ctrl_info),
			    ci, ci_h);

	megasas_return_cmd(instance, cmd);
	return ret;
}

/**
 * megasas_issue_init_mfi -	Initializes the FW
 * @instance:		Adapter soft state
 *
 * Issues the INIT MFI cmd
 */
static int
megasas_issue_init_mfi(struct megasas_instance *instance)
{
	u32 context;

	struct megasas_cmd *cmd;

	struct megasas_init_frame *init_frame;
	struct megasas_init_queue_info *initq_info;
	dma_addr_t init_frame_h;
	dma_addr_t initq_info_h;

	/*
	 * Prepare a init frame. Note the init frame points to queue info
	 * structure. Each frame has SGL allocated after first 64 bytes. For
	 * this frame - since we don't need any SGL - we use SGL's space as
	 * queue info structure
	 *
	 * We will not get a NULL command below. We just created the pool.
	 */
	cmd = megasas_get_cmd(instance);

	init_frame = (struct megasas_init_frame *)cmd->frame;
	initq_info = (struct megasas_init_queue_info *)
		((unsigned long)init_frame + 64);

	init_frame_h = cmd->frame_phys_addr;
	initq_info_h = init_frame_h + 64;

	context = init_frame->context;
	memset(init_frame, 0, MEGAMFI_FRAME_SIZE);
	memset(initq_info, 0, sizeof(struct megasas_init_queue_info));
	init_frame->context = context;

	initq_info->reply_queue_entries = instance->max_fw_cmds + 1;
	initq_info->reply_queue_start_phys_addr_lo = instance->reply_queue_h;

	initq_info->producer_index_phys_addr_lo = instance->producer_h;
	initq_info->consumer_index_phys_addr_lo = instance->consumer_h;

	init_frame->cmd = MFI_CMD_INIT;
	init_frame->cmd_status = 0xFF;
	init_frame->queue_info_new_phys_addr_lo = initq_info_h;

	init_frame->data_xfer_len = sizeof(struct megasas_init_queue_info);

	/*
	 * disable the intr before firing the init frame to FW
	 */
	instance->instancet->disable_intr(instance->reg_set);

	/*
	 * Issue the init frame in polled mode
	 */

	if (megasas_issue_polled(instance, cmd)) {
		printk(KERN_ERR "megasas: Failed to init firmware\n");
		megasas_return_cmd(instance, cmd);
		goto fail_fw_init;
	}

	megasas_return_cmd(instance, cmd);

	return 0;

fail_fw_init:
	return -EINVAL;
}

/**
 * megasas_start_timer - Initializes a timer object
 * @instance:		Adapter soft state
 * @timer:		timer object to be initialized
 * @fn:			timer function
 * @interval:		time interval between timer function call
 */
static inline void
megasas_start_timer(struct megasas_instance *instance,
			struct timer_list *timer,
			void *fn, unsigned long interval)
{
	init_timer(timer);
	timer->expires = jiffies + interval;
	timer->data = (unsigned long)instance;
	timer->function = fn;
	add_timer(timer);
}

/**
 * megasas_io_completion_timer - Timer fn
 * @instance_addr:	Address of adapter soft state
 *
 * Schedules tasklet for cmd completion
 * if poll_mode_io is set
 */
static void
megasas_io_completion_timer(unsigned long instance_addr)
{
	struct megasas_instance *instance =
			(struct megasas_instance *)instance_addr;

	if (atomic_read(&instance->fw_outstanding))
		tasklet_schedule(&instance->isr_tasklet);

	/* Restart timer */
	if (poll_mode_io)
		mod_timer(&instance->io_completion_timer,
			jiffies + MEGASAS_COMPLETION_TIMER_INTERVAL);
}

/**
 * megasas_init_mfi -	Initializes the FW
 * @instance:		Adapter soft state
 *
 * This is the main function for initializing MFI firmware.
 */
static int megasas_init_mfi(struct megasas_instance *instance)
{
	u32 context_sz;
	u32 reply_q_sz;
	u32 max_sectors_1;
	u32 max_sectors_2;
	u32 tmp_sectors;
	struct megasas_register_set __iomem *reg_set;
	struct megasas_ctrl_info *ctrl_info;
	/*
	 * Map the message registers
	 */
	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS1078GEN2) ||
		(instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0071SKINNY) ||
		(instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
		(instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0079GEN2)) {
		instance->base_addr = pci_resource_start(instance->pdev, 1);
	} else {
		instance->base_addr = pci_resource_start(instance->pdev, 0);
	}

	if (pci_request_selected_regions(instance->pdev,
		pci_select_bars(instance->pdev, IORESOURCE_MEM),
		"megasas: LSI")) {
		printk(KERN_DEBUG "megasas: IO memory region busy!\n");
		return -EBUSY;
	}

	instance->reg_set = ioremap_nocache(instance->base_addr, 8192);

	if (!instance->reg_set) {
		printk(KERN_DEBUG "megasas: Failed to map IO mem\n");
		goto fail_ioremap;
	}

	reg_set = instance->reg_set;

	switch(instance->pdev->device)
	{
		case PCI_DEVICE_ID_LSI_SAS1078R:
		case PCI_DEVICE_ID_LSI_SAS1078DE:
			instance->instancet = &megasas_instance_template_ppc;
			break;
		case PCI_DEVICE_ID_LSI_SAS1078GEN2:
		case PCI_DEVICE_ID_LSI_SAS0079GEN2:
			instance->instancet = &megasas_instance_template_gen2;
			break;
		case PCI_DEVICE_ID_LSI_SAS0073SKINNY:
		case PCI_DEVICE_ID_LSI_SAS0071SKINNY:
			instance->instancet = &megasas_instance_template_skinny;
			break;
		case PCI_DEVICE_ID_LSI_SAS1064R:
		case PCI_DEVICE_ID_DELL_PERC5:
		default:
			instance->instancet = &megasas_instance_template_xscale;
			break;
	}

	/*
	 * We expect the FW state to be READY
	 */
	if (megasas_transition_to_ready(instance))
		goto fail_ready_state;

	/*
	 * Get various operational parameters from status register
	 */
	instance->max_fw_cmds = instance->instancet->read_fw_status_reg(reg_set) & 0x00FFFF;
	/*
	 * Reduce the max supported cmds by 1. This is to ensure that the
	 * reply_q_sz (1 more than the max cmd that driver may send)
	 * does not exceed max cmds that the FW can support
	 */
	instance->max_fw_cmds = instance->max_fw_cmds-1;
	instance->max_num_sge = (instance->instancet->read_fw_status_reg(reg_set) & 0xFF0000) >> 
					0x10;
	/*
	 * Create a pool of commands
	 */
	if (megasas_alloc_cmds(instance))
		goto fail_alloc_cmds;

	/*
	 * Allocate memory for reply queue. Length of reply queue should
	 * be _one_ more than the maximum commands handled by the firmware.
	 *
	 * Note: When FW completes commands, it places corresponding contex
	 * values in this circular reply queue. This circular queue is a fairly
	 * typical producer-consumer queue. FW is the producer (of completed
	 * commands) and the driver is the consumer.
	 */
	context_sz = sizeof(u32);
	reply_q_sz = context_sz * (instance->max_fw_cmds + 1);

	instance->reply_queue = pci_alloc_consistent(instance->pdev,
						     reply_q_sz,
						     &instance->reply_queue_h);

	if (!instance->reply_queue) {
		printk(KERN_DEBUG "megasas: Out of DMA mem for reply queue\n");
		goto fail_reply_queue;
	}

	if (megasas_issue_init_mfi(instance))
		goto fail_fw_init;

	memset(instance->pd_list, 0 ,
		(MEGASAS_MAX_PD * sizeof(struct megasas_pd_list)));
	megasas_get_pd_list(instance);

	memset(instance->ld_ids, 0xff, MEGASAS_MAX_LD_IDS);
	megasas_get_ld_list(instance);

	ctrl_info = kmalloc(sizeof(struct megasas_ctrl_info), GFP_KERNEL);

	/*
	 * Compute the max allowed sectors per IO: The controller info has two
	 * limits on max sectors. Driver should use the minimum of these two.
	 *
	 * 1 << stripe_sz_ops.min = max sectors per strip
	 *
	 * Note that older firmwares ( < FW ver 30) didn't report information
	 * to calculate max_sectors_1. So the number ended up as zero always.
	 */
	tmp_sectors = 0;
	if (ctrl_info && !megasas_get_ctrl_info(instance, ctrl_info)) {

		max_sectors_1 = (1 << ctrl_info->stripe_sz_ops.min) *
		    ctrl_info->max_strips_per_io;
		max_sectors_2 = ctrl_info->max_request_size;

		tmp_sectors = min_t(u32, max_sectors_1 , max_sectors_2);
	}

	instance->max_sectors_per_req = instance->max_num_sge *
						PAGE_SIZE / 512;
	if (tmp_sectors && (instance->max_sectors_per_req > tmp_sectors))
		instance->max_sectors_per_req = tmp_sectors;

	kfree(ctrl_info);

        /*
	* Setup tasklet for cmd completion
	*/

	tasklet_init(&instance->isr_tasklet, megasas_complete_cmd_dpc,
		(unsigned long)instance);

	/* Initialize the cmd completion timer */
	if (poll_mode_io)
		megasas_start_timer(instance, &instance->io_completion_timer,
				megasas_io_completion_timer,
				MEGASAS_COMPLETION_TIMER_INTERVAL);
	return 0;

      fail_fw_init:

	pci_free_consistent(instance->pdev, reply_q_sz,
			    instance->reply_queue, instance->reply_queue_h);
      fail_reply_queue:
	megasas_free_cmds(instance);

      fail_alloc_cmds:
      fail_ready_state:
	iounmap(instance->reg_set);

      fail_ioremap:
	pci_release_selected_regions(instance->pdev,
		pci_select_bars(instance->pdev, IORESOURCE_MEM));

	return -EINVAL;
}

/**
 * megasas_release_mfi -	Reverses the FW initialization
 * @intance:			Adapter soft state
 */
static void megasas_release_mfi(struct megasas_instance *instance)
{
	u32 reply_q_sz = sizeof(u32) * (instance->max_fw_cmds + 1);

	pci_free_consistent(instance->pdev, reply_q_sz,
			    instance->reply_queue, instance->reply_queue_h);

	megasas_free_cmds(instance);

	iounmap(instance->reg_set);

	pci_release_selected_regions(instance->pdev,
		pci_select_bars(instance->pdev, IORESOURCE_MEM));
}

/**
 * megasas_get_seq_num -	Gets latest event sequence numbers
 * @instance:			Adapter soft state
 * @eli:			FW event log sequence numbers information
 *
 * FW maintains a log of all events in a non-volatile area. Upper layers would
 * usually find out the latest sequence number of the events, the seq number at
 * the boot etc. They would "read" all the events below the latest seq number
 * by issuing a direct fw cmd (DCMD). For the future events (beyond latest seq
 * number), they would subsribe to AEN (asynchronous event notification) and
 * wait for the events to happen.
 */
static int
megasas_get_seq_num(struct megasas_instance *instance,
		    struct megasas_evt_log_info *eli)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct megasas_evt_log_info *el_info;
	dma_addr_t el_info_h = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;
	el_info = pci_alloc_consistent(instance->pdev,
				       sizeof(struct megasas_evt_log_info),
				       &el_info_h);

	if (!el_info) {
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(el_info, 0, sizeof(*el_info));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = sizeof(struct megasas_evt_log_info);
	dcmd->opcode = MR_DCMD_CTRL_EVENT_GET_INFO;
	dcmd->sgl.sge32[0].phys_addr = el_info_h;
	dcmd->sgl.sge32[0].length = sizeof(struct megasas_evt_log_info);

	megasas_issue_blocked_cmd(instance, cmd);

	/*
	 * Copy the data back into callers buffer
	 */
	memcpy(eli, el_info, sizeof(struct megasas_evt_log_info));

	pci_free_consistent(instance->pdev, sizeof(struct megasas_evt_log_info),
			    el_info, el_info_h);

	megasas_return_cmd(instance, cmd);

	return 0;
}

/**
 * megasas_register_aen -	Registers for asynchronous event notification
 * @instance:			Adapter soft state
 * @seq_num:			The starting sequence number
 * @class_locale:		Class of the event
 *
 * This function subscribes for AEN for events beyond the @seq_num. It requests
 * to be notified if and only if the event is of type @class_locale
 */
static int
megasas_register_aen(struct megasas_instance *instance, u32 seq_num,
		     u32 class_locale_word)
{
	int ret_val;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	union megasas_evt_class_locale curr_aen;
	union megasas_evt_class_locale prev_aen;

	/*
	 * If there an AEN pending already (aen_cmd), check if the
	 * class_locale of that pending AEN is inclusive of the new
	 * AEN request we currently have. If it is, then we don't have
	 * to do anything. In other words, whichever events the current
	 * AEN request is subscribing to, have already been subscribed
	 * to.
	 *
	 * If the old_cmd is _not_ inclusive, then we have to abort
	 * that command, form a class_locale that is superset of both
	 * old and current and re-issue to the FW
	 */

	curr_aen.word = class_locale_word;

	if (instance->aen_cmd) {

		prev_aen.word = instance->aen_cmd->frame->dcmd.mbox.w[1];

		/*
		 * A class whose enum value is smaller is inclusive of all
		 * higher values. If a PROGRESS (= -1) was previously
		 * registered, then a new registration requests for higher
		 * classes need not be sent to FW. They are automatically
		 * included.
		 *
		 * Locale numbers don't have such hierarchy. They are bitmap
		 * values
		 */
		if ((prev_aen.members.class <= curr_aen.members.class) &&
		    !((prev_aen.members.locale & curr_aen.members.locale) ^
		      curr_aen.members.locale)) {
			/*
			 * Previously issued event registration includes
			 * current request. Nothing to do.
			 */
			return 0;
		} else {
			curr_aen.members.locale |= prev_aen.members.locale;

			if (prev_aen.members.class < curr_aen.members.class)
				curr_aen.members.class = prev_aen.members.class;

			instance->aen_cmd->abort_aen = 1;
			ret_val = megasas_issue_blocked_abort_cmd(instance,
								  instance->
								  aen_cmd);

			if (ret_val) {
				printk(KERN_DEBUG "megasas: Failed to abort "
				       "previous AEN command\n");
				return ret_val;
			}
		}
	}

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return -ENOMEM;

	dcmd = &cmd->frame->dcmd;

	memset(instance->evt_detail, 0, sizeof(struct megasas_evt_detail));

	/*
	 * Prepare DCMD for aen registration
	 */
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_READ;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = sizeof(struct megasas_evt_detail);
	dcmd->opcode = MR_DCMD_CTRL_EVENT_WAIT;
	dcmd->mbox.w[0] = seq_num;
	dcmd->mbox.w[1] = curr_aen.word;
	dcmd->sgl.sge32[0].phys_addr = (u32) instance->evt_detail_h;
	dcmd->sgl.sge32[0].length = sizeof(struct megasas_evt_detail);

	if (instance->aen_cmd != NULL) {
		megasas_return_cmd(instance, cmd);
		return 0;
	}

	/*
	 * Store reference to the cmd used to register for AEN. When an
	 * application wants us to register for AEN, we have to abort this
	 * cmd and re-register with a new EVENT LOCALE supplied by that app
	 */
	instance->aen_cmd = cmd;

	/*
	 * Issue the aen registration frame
	 */
	instance->instancet->fire_cmd(instance,
			cmd->frame_phys_addr, 0, instance->reg_set);

	return 0;
}

/**
 * megasas_start_aen -	Subscribes to AEN during driver load time
 * @instance:		Adapter soft state
 */
static int megasas_start_aen(struct megasas_instance *instance)
{
	struct megasas_evt_log_info eli;
	union megasas_evt_class_locale class_locale;

	/*
	 * Get the latest sequence number from FW
	 */
	memset(&eli, 0, sizeof(eli));

	if (megasas_get_seq_num(instance, &eli))
		return -1;

	/*
	 * Register AEN with FW for latest sequence number plus 1
	 */
	class_locale.members.reserved = 0;
	class_locale.members.locale = MR_EVT_LOCALE_ALL;
	class_locale.members.class = MR_EVT_CLASS_DEBUG;

	return megasas_register_aen(instance, eli.newest_seq_num + 1,
				    class_locale.word);
}

/**
 * megasas_io_attach -	Attaches this driver to SCSI mid-layer
 * @instance:		Adapter soft state
 */
static int megasas_io_attach(struct megasas_instance *instance)
{
	struct Scsi_Host *host = instance->host;

	/*
	 * Export parameters required by SCSI mid-layer
	 */
	host->irq = instance->pdev->irq;
	host->unique_id = instance->unique_id;
	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
		(instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0071SKINNY)) {
		host->can_queue =
			instance->max_fw_cmds - MEGASAS_SKINNY_INT_CMDS;
	} else
		host->can_queue =
			instance->max_fw_cmds - MEGASAS_INT_CMDS;
	host->this_id = instance->init_id;
	host->sg_tablesize = instance->max_num_sge;
	host->max_sectors = instance->max_sectors_per_req;
	host->cmd_per_lun = 128;
	host->max_channel = MEGASAS_MAX_CHANNELS - 1;
	host->max_id = MEGASAS_MAX_DEV_PER_CHANNEL;
	host->max_lun = MEGASAS_MAX_LUN;
	host->max_cmd_len = 16;

	/*
	 * Notify the mid-layer about the new controller
	 */
	if (scsi_add_host(host, &instance->pdev->dev)) {
		printk(KERN_DEBUG "megasas: scsi_add_host failed\n");
		return -ENODEV;
	}

	/*
	 * Trigger SCSI to scan our drives
	 */
	scsi_scan_host(host);
	return 0;
}

static int
megasas_set_dma_mask(struct pci_dev *pdev)
{
	/*
	 * All our contollers are capable of performing 64-bit DMA
	 */
	if (IS_DMA64) {
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) != 0) {

			if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0)
				goto fail_set_dma_mask;
		}
	} else {
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0)
			goto fail_set_dma_mask;
	}
	return 0;

fail_set_dma_mask:
	return 1;
}

/**
 * megasas_probe_one -	PCI hotplug entry point
 * @pdev:		PCI device structure
 * @id:			PCI ids of supported hotplugged adapter	
 */
static int __devinit
megasas_probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rval;
	struct Scsi_Host *host;
	struct megasas_instance *instance;

	/*
	 * Announce PCI information
	 */
	printk(KERN_INFO "megasas: %#4.04x:%#4.04x:%#4.04x:%#4.04x: ",
	       pdev->vendor, pdev->device, pdev->subsystem_vendor,
	       pdev->subsystem_device);

	printk("bus %d:slot %d:func %d\n",
	       pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

	/*
	 * PCI prepping: enable device set bus mastering and dma mask
	 */
	rval = pci_enable_device_mem(pdev);

	if (rval) {
		return rval;
	}

	pci_set_master(pdev);

	if (megasas_set_dma_mask(pdev))
		goto fail_set_dma_mask;

	host = scsi_host_alloc(&megasas_template,
			       sizeof(struct megasas_instance));

	if (!host) {
		printk(KERN_DEBUG "megasas: scsi_host_alloc failed\n");
		goto fail_alloc_instance;
	}

	instance = (struct megasas_instance *)host->hostdata;
	memset(instance, 0, sizeof(*instance));

	instance->producer = pci_alloc_consistent(pdev, sizeof(u32),
						  &instance->producer_h);
	instance->consumer = pci_alloc_consistent(pdev, sizeof(u32),
						  &instance->consumer_h);

	if (!instance->producer || !instance->consumer) {
		printk(KERN_DEBUG "megasas: Failed to allocate memory for "
		       "producer, consumer\n");
		goto fail_alloc_dma_buf;
	}

	*instance->producer = 0;
	*instance->consumer = 0;
	megasas_poll_wait_aen = 0;
	instance->flag_ieee = 0;
	instance->ev = NULL;

	instance->evt_detail = pci_alloc_consistent(pdev,
						    sizeof(struct
							   megasas_evt_detail),
						    &instance->evt_detail_h);

	if (!instance->evt_detail) {
		printk(KERN_DEBUG "megasas: Failed to allocate memory for "
		       "event detail structure\n");
		goto fail_alloc_dma_buf;
	}

	/*
	 * Initialize locks and queues
	 */
	INIT_LIST_HEAD(&instance->cmd_pool);

	atomic_set(&instance->fw_outstanding,0);

	init_waitqueue_head(&instance->int_cmd_wait_q);
	init_waitqueue_head(&instance->abort_cmd_wait_q);

	spin_lock_init(&instance->cmd_pool_lock);
	spin_lock_init(&instance->fire_lock);
	spin_lock_init(&instance->completion_lock);
	spin_lock_init(&poll_aen_lock);

	mutex_init(&instance->aen_mutex);

	/*
	 * Initialize PCI related and misc parameters
	 */
	instance->pdev = pdev;
	instance->host = host;
	instance->unique_id = pdev->bus->number << 8 | pdev->devfn;
	instance->init_id = MEGASAS_DEFAULT_INIT_ID;

	if ((instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0073SKINNY) ||
		(instance->pdev->device == PCI_DEVICE_ID_LSI_SAS0071SKINNY)) {
		instance->flag_ieee = 1;
		sema_init(&instance->ioctl_sem, MEGASAS_SKINNY_INT_CMDS);
	} else
		sema_init(&instance->ioctl_sem, MEGASAS_INT_CMDS);

	megasas_dbg_lvl = 0;
	instance->flag = 0;
	instance->unload = 1;
	instance->last_time = 0;

	/*
	 * Initialize MFI Firmware
	 */
	if (megasas_init_mfi(instance))
		goto fail_init_mfi;

	/*
	 * Register IRQ
	 */
	if (request_irq(pdev->irq, megasas_isr, IRQF_SHARED, "megasas", instance)) {
		printk(KERN_DEBUG "megasas: Failed to register IRQ\n");
		goto fail_irq;
	}

	instance->instancet->enable_intr(instance->reg_set);

	/*
	 * Store instance in PCI softstate
	 */
	pci_set_drvdata(pdev, instance);

	/*
	 * Add this controller to megasas_mgmt_info structure so that it
	 * can be exported to management applications
	 */
	megasas_mgmt_info.count++;
	megasas_mgmt_info.instance[megasas_mgmt_info.max_index] = instance;
	megasas_mgmt_info.max_index++;

	/*
	 * Initiate AEN (Asynchronous Event Notification)
	 */
	if (megasas_start_aen(instance)) {
		printk(KERN_DEBUG "megasas: start aen failed\n");
		goto fail_start_aen;
	}

	/*
	 * Register with SCSI mid-layer
	 */
	if (megasas_io_attach(instance))
		goto fail_io_attach;

	instance->unload = 0;
	return 0;

      fail_start_aen:
      fail_io_attach:
	megasas_mgmt_info.count--;
	megasas_mgmt_info.instance[megasas_mgmt_info.max_index] = NULL;
	megasas_mgmt_info.max_index--;

	pci_set_drvdata(pdev, NULL);
	instance->instancet->disable_intr(instance->reg_set);
	free_irq(instance->pdev->irq, instance);

	megasas_release_mfi(instance);

      fail_irq:
      fail_init_mfi:
      fail_alloc_dma_buf:
	if (instance->evt_detail)
		pci_free_consistent(pdev, sizeof(struct megasas_evt_detail),
				    instance->evt_detail,
				    instance->evt_detail_h);

	if (instance->producer)
		pci_free_consistent(pdev, sizeof(u32), instance->producer,
				    instance->producer_h);
	if (instance->consumer)
		pci_free_consistent(pdev, sizeof(u32), instance->consumer,
				    instance->consumer_h);
	scsi_host_put(host);

      fail_alloc_instance:
      fail_set_dma_mask:
	pci_disable_device(pdev);

	return -ENODEV;
}

/**
 * megasas_flush_cache -	Requests FW to flush all its caches
 * @instance:			Adapter soft state
 */
static void megasas_flush_cache(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return;

	dcmd = &cmd->frame->dcmd;

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 0;
	dcmd->flags = MFI_FRAME_DIR_NONE;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = 0;
	dcmd->opcode = MR_DCMD_CTRL_CACHE_FLUSH;
	dcmd->mbox.b[0] = MR_FLUSH_CTRL_CACHE | MR_FLUSH_DISK_CACHE;

	megasas_issue_blocked_cmd(instance, cmd);

	megasas_return_cmd(instance, cmd);

	return;
}

/**
 * megasas_shutdown_controller -	Instructs FW to shutdown the controller
 * @instance:				Adapter soft state
 * @opcode:				Shutdown/Hibernate
 */
static void megasas_shutdown_controller(struct megasas_instance *instance,
					u32 opcode)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return;

	if (instance->aen_cmd)
		megasas_issue_blocked_abort_cmd(instance, instance->aen_cmd);

	dcmd = &cmd->frame->dcmd;

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 0;
	dcmd->flags = MFI_FRAME_DIR_NONE;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = 0;
	dcmd->opcode = opcode;

	megasas_issue_blocked_cmd(instance, cmd);

	megasas_return_cmd(instance, cmd);

	return;
}

#ifdef CONFIG_PM
/**
 * megasas_suspend -	driver suspend entry point
 * @pdev:		PCI device structure
 * @state:		PCI power state to suspend routine
 */
static int
megasas_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct Scsi_Host *host;
	struct megasas_instance *instance;

	instance = pci_get_drvdata(pdev);
	host = instance->host;
	instance->unload = 1;

	if (poll_mode_io)
		del_timer_sync(&instance->io_completion_timer);

	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_HIBERNATE_SHUTDOWN);

	/* cancel the delayed work if this work still in queue */
	if (instance->ev != NULL) {
		struct megasas_aen_event *ev = instance->ev;
		cancel_delayed_work(
			(struct delayed_work *)&ev->hotplug_work);
		flush_scheduled_work();
		instance->ev = NULL;
	}

	tasklet_kill(&instance->isr_tasklet);

	pci_set_drvdata(instance->pdev, instance);
	instance->instancet->disable_intr(instance->reg_set);
	free_irq(instance->pdev->irq, instance);

	pci_save_state(pdev);
	pci_disable_device(pdev);

	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}

/**
 * megasas_resume-      driver resume entry point
 * @pdev:               PCI device structure
 */
static int
megasas_resume(struct pci_dev *pdev)
{
	int rval;
	struct Scsi_Host *host;
	struct megasas_instance *instance;

	instance = pci_get_drvdata(pdev);
	host = instance->host;
	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);

	/*
	 * PCI prepping: enable device set bus mastering and dma mask
	 */
	rval = pci_enable_device_mem(pdev);

	if (rval) {
		printk(KERN_ERR "megasas: Enable device failed\n");
		return rval;
	}

	pci_set_master(pdev);

	if (megasas_set_dma_mask(pdev))
		goto fail_set_dma_mask;

	/*
	 * Initialize MFI Firmware
	 */

	*instance->producer = 0;
	*instance->consumer = 0;

	atomic_set(&instance->fw_outstanding, 0);

	/*
	 * We expect the FW state to be READY
	 */
	if (megasas_transition_to_ready(instance))
		goto fail_ready_state;

	if (megasas_issue_init_mfi(instance))
		goto fail_init_mfi;

	tasklet_init(&instance->isr_tasklet, megasas_complete_cmd_dpc,
			(unsigned long)instance);

	/*
	 * Register IRQ
	 */
	if (request_irq(pdev->irq, megasas_isr, IRQF_SHARED,
		"megasas", instance)) {
		printk(KERN_ERR "megasas: Failed to register IRQ\n");
		goto fail_irq;
	}

	instance->instancet->enable_intr(instance->reg_set);

	/*
	 * Initiate AEN (Asynchronous Event Notification)
	 */
	if (megasas_start_aen(instance))
		printk(KERN_ERR "megasas: Start AEN failed\n");

	/* Initialize the cmd completion timer */
	if (poll_mode_io)
		megasas_start_timer(instance, &instance->io_completion_timer,
				megasas_io_completion_timer,
				MEGASAS_COMPLETION_TIMER_INTERVAL);
	instance->unload = 0;

	return 0;

fail_irq:
fail_init_mfi:
	if (instance->evt_detail)
		pci_free_consistent(pdev, sizeof(struct megasas_evt_detail),
				instance->evt_detail,
				instance->evt_detail_h);

	if (instance->producer)
		pci_free_consistent(pdev, sizeof(u32), instance->producer,
				instance->producer_h);
	if (instance->consumer)
		pci_free_consistent(pdev, sizeof(u32), instance->consumer,
				instance->consumer_h);
	scsi_host_put(host);

fail_set_dma_mask:
fail_ready_state:

	pci_disable_device(pdev);

	return -ENODEV;
}
#else
#define megasas_suspend	NULL
#define megasas_resume	NULL
#endif

/**
 * megasas_detach_one -	PCI hot"un"plug entry point
 * @pdev:		PCI device structure
 */
static void __devexit megasas_detach_one(struct pci_dev *pdev)
{
	int i;
	struct Scsi_Host *host;
	struct megasas_instance *instance;

	instance = pci_get_drvdata(pdev);
	instance->unload = 1;
	host = instance->host;

	if (poll_mode_io)
		del_timer_sync(&instance->io_completion_timer);

	scsi_remove_host(instance->host);
	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_CTRL_SHUTDOWN);

	/* cancel the delayed work if this work still in queue*/
	if (instance->ev != NULL) {
		struct megasas_aen_event *ev = instance->ev;
		cancel_delayed_work(
			(struct delayed_work *)&ev->hotplug_work);
		flush_scheduled_work();
		instance->ev = NULL;
	}

	tasklet_kill(&instance->isr_tasklet);

	/*
	 * Take the instance off the instance array. Note that we will not
	 * decrement the max_index. We let this array be sparse array
	 */
	for (i = 0; i < megasas_mgmt_info.max_index; i++) {
		if (megasas_mgmt_info.instance[i] == instance) {
			megasas_mgmt_info.count--;
			megasas_mgmt_info.instance[i] = NULL;

			break;
		}
	}

	pci_set_drvdata(instance->pdev, NULL);

	instance->instancet->disable_intr(instance->reg_set);

	free_irq(instance->pdev->irq, instance);

	megasas_release_mfi(instance);

	pci_free_consistent(pdev, sizeof(struct megasas_evt_detail),
			    instance->evt_detail, instance->evt_detail_h);

	pci_free_consistent(pdev, sizeof(u32), instance->producer,
			    instance->producer_h);

	pci_free_consistent(pdev, sizeof(u32), instance->consumer,
			    instance->consumer_h);

	scsi_host_put(host);

	pci_set_drvdata(pdev, NULL);

	pci_disable_device(pdev);

	return;
}

/**
 * megasas_shutdown -	Shutdown entry point
 * @device:		Generic device structure
 */
static void megasas_shutdown(struct pci_dev *pdev)
{
	struct megasas_instance *instance = pci_get_drvdata(pdev);
	instance->unload = 1;
	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_CTRL_SHUTDOWN);
}

/**
 * megasas_mgmt_open -	char node "open" entry point
 */
static int megasas_mgmt_open(struct inode *inode, struct file *filep)
{
	cycle_kernel_lock();
	/*
	 * Allow only those users with admin rights
	 */
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	return 0;
}

/**
 * megasas_mgmt_fasync -	Async notifier registration from applications
 *
 * This function adds the calling process to a driver global queue. When an
 * event occurs, SIGIO will be sent to all processes in this queue.
 */
static int megasas_mgmt_fasync(int fd, struct file *filep, int mode)
{
	int rc;

	mutex_lock(&megasas_async_queue_mutex);

	rc = fasync_helper(fd, filep, mode, &megasas_async_queue);

	mutex_unlock(&megasas_async_queue_mutex);

	if (rc >= 0) {
		/* For sanity check when we get ioctl */
		filep->private_data = filep;
		return 0;
	}

	printk(KERN_DEBUG "megasas: fasync_helper failed [%d]\n", rc);

	return rc;
}

/**
 * megasas_mgmt_poll -  char node "poll" entry point
 * */
static unsigned int megasas_mgmt_poll(struct file *file, poll_table *wait)
{
	unsigned int mask;
	unsigned long flags;
	poll_wait(file, &megasas_poll_wait, wait);
	spin_lock_irqsave(&poll_aen_lock, flags);
	if (megasas_poll_wait_aen)
		mask =   (POLLIN | POLLRDNORM);
	else
		mask = 0;
	spin_unlock_irqrestore(&poll_aen_lock, flags);
	return mask;
}

/**
 * megasas_mgmt_fw_ioctl -	Issues management ioctls to FW
 * @instance:			Adapter soft state
 * @argp:			User's ioctl packet
 */
static int
megasas_mgmt_fw_ioctl(struct megasas_instance *instance,
		      struct megasas_iocpacket __user * user_ioc,
		      struct megasas_iocpacket *ioc)
{
	struct megasas_sge32 *kern_sge32;
	struct megasas_cmd *cmd;
	void *kbuff_arr[MAX_IOCTL_SGE];
	dma_addr_t buf_handle = 0;
	int error = 0, i;
	void *sense = NULL;
	dma_addr_t sense_handle;
	unsigned long *sense_ptr;

	memset(kbuff_arr, 0, sizeof(kbuff_arr));

	if (ioc->sge_count > MAX_IOCTL_SGE) {
		printk(KERN_DEBUG "megasas: SGE count [%d] >  max limit [%d]\n",
		       ioc->sge_count, MAX_IOCTL_SGE);
		return -EINVAL;
	}

	cmd = megasas_get_cmd(instance);
	if (!cmd) {
		printk(KERN_DEBUG "megasas: Failed to get a cmd packet\n");
		return -ENOMEM;
	}

	/*
	 * User's IOCTL packet has 2 frames (maximum). Copy those two
	 * frames into our cmd's frames. cmd->frame's context will get
	 * overwritten when we copy from user's frames. So set that value
	 * alone separately
	 */
	memcpy(cmd->frame, ioc->frame.raw, 2 * MEGAMFI_FRAME_SIZE);
	cmd->frame->hdr.context = cmd->index;
	cmd->frame->hdr.pad_0 = 0;

	/*
	 * The management interface between applications and the fw uses
	 * MFI frames. E.g, RAID configuration changes, LD property changes
	 * etc are accomplishes through different kinds of MFI frames. The
	 * driver needs to care only about substituting user buffers with
	 * kernel buffers in SGLs. The location of SGL is embedded in the
	 * struct iocpacket itself.
	 */
	kern_sge32 = (struct megasas_sge32 *)
	    ((unsigned long)cmd->frame + ioc->sgl_off);

	/*
	 * For each user buffer, create a mirror buffer and copy in
	 */
	for (i = 0; i < ioc->sge_count; i++) {
		kbuff_arr[i] = dma_alloc_coherent(&instance->pdev->dev,
						    ioc->sgl[i].iov_len,
						    &buf_handle, GFP_KERNEL);
		if (!kbuff_arr[i]) {
			printk(KERN_DEBUG "megasas: Failed to alloc "
			       "kernel SGL buffer for IOCTL \n");
			error = -ENOMEM;
			goto out;
		}

		/*
		 * We don't change the dma_coherent_mask, so
		 * pci_alloc_consistent only returns 32bit addresses
		 */
		kern_sge32[i].phys_addr = (u32) buf_handle;
		kern_sge32[i].length = ioc->sgl[i].iov_len;

		/*
		 * We created a kernel buffer corresponding to the
		 * user buffer. Now copy in from the user buffer
		 */
		if (copy_from_user(kbuff_arr[i], ioc->sgl[i].iov_base,
				   (u32) (ioc->sgl[i].iov_len))) {
			error = -EFAULT;
			goto out;
		}
	}

	if (ioc->sense_len) {
		sense = dma_alloc_coherent(&instance->pdev->dev, ioc->sense_len,
					     &sense_handle, GFP_KERNEL);
		if (!sense) {
			error = -ENOMEM;
			goto out;
		}

		sense_ptr =
		(unsigned long *) ((unsigned long)cmd->frame + ioc->sense_off);
		*sense_ptr = sense_handle;
	}

	/*
	 * Set the sync_cmd flag so that the ISR knows not to complete this
	 * cmd to the SCSI mid-layer
	 */
	cmd->sync_cmd = 1;
	megasas_issue_blocked_cmd(instance, cmd);
	cmd->sync_cmd = 0;

	/*
	 * copy out the kernel buffers to user buffers
	 */
	for (i = 0; i < ioc->sge_count; i++) {
		if (copy_to_user(ioc->sgl[i].iov_base, kbuff_arr[i],
				 ioc->sgl[i].iov_len)) {
			error = -EFAULT;
			goto out;
		}
	}

	/*
	 * copy out the sense
	 */
	if (ioc->sense_len) {
		/*
		 * sense_ptr points to the location that has the user
		 * sense buffer address
		 */
		sense_ptr = (unsigned long *) ((unsigned long)ioc->frame.raw +
				ioc->sense_off);

		if (copy_to_user((void __user *)((unsigned long)(*sense_ptr)),
				 sense, ioc->sense_len)) {
			printk(KERN_ERR "megasas: Failed to copy out to user "
					"sense data\n");
			error = -EFAULT;
			goto out;
		}
	}

	/*
	 * copy the status codes returned by the fw
	 */
	if (copy_to_user(&user_ioc->frame.hdr.cmd_status,
			 &cmd->frame->hdr.cmd_status, sizeof(u8))) {
		printk(KERN_DEBUG "megasas: Error copying out cmd_status\n");
		error = -EFAULT;
	}

      out:
	if (sense) {
		dma_free_coherent(&instance->pdev->dev, ioc->sense_len,
				    sense, sense_handle);
	}

	for (i = 0; i < ioc->sge_count && kbuff_arr[i]; i++) {
		dma_free_coherent(&instance->pdev->dev,
				    kern_sge32[i].length,
				    kbuff_arr[i], kern_sge32[i].phys_addr);
	}

	megasas_return_cmd(instance, cmd);
	return error;
}

static int megasas_mgmt_ioctl_fw(struct file *file, unsigned long arg)
{
	struct megasas_iocpacket __user *user_ioc =
	    (struct megasas_iocpacket __user *)arg;
	struct megasas_iocpacket *ioc;
	struct megasas_instance *instance;
	int error;

	ioc = kmalloc(sizeof(*ioc), GFP_KERNEL);
	if (!ioc)
		return -ENOMEM;

	if (copy_from_user(ioc, user_ioc, sizeof(*ioc))) {
		error = -EFAULT;
		goto out_kfree_ioc;
	}

	instance = megasas_lookup_instance(ioc->host_no);
	if (!instance) {
		error = -ENODEV;
		goto out_kfree_ioc;
	}

	if (instance->hw_crit_error == 1) {
		printk(KERN_DEBUG "Controller in Crit ERROR\n");
		error = -ENODEV;
		goto out_kfree_ioc;
	}

	if (instance->unload == 1) {
		error = -ENODEV;
		goto out_kfree_ioc;
	}

	/*
	 * We will allow only MEGASAS_INT_CMDS number of parallel ioctl cmds
	 */
	if (down_interruptible(&instance->ioctl_sem)) {
		error = -ERESTARTSYS;
		goto out_kfree_ioc;
	}
	error = megasas_mgmt_fw_ioctl(instance, user_ioc, ioc);
	up(&instance->ioctl_sem);

      out_kfree_ioc:
	kfree(ioc);
	return error;
}

static int megasas_mgmt_ioctl_aen(struct file *file, unsigned long arg)
{
	struct megasas_instance *instance;
	struct megasas_aen aen;
	int error;

	if (file->private_data != file) {
		printk(KERN_DEBUG "megasas: fasync_helper was not "
		       "called first\n");
		return -EINVAL;
	}

	if (copy_from_user(&aen, (void __user *)arg, sizeof(aen)))
		return -EFAULT;

	instance = megasas_lookup_instance(aen.host_no);

	if (!instance)
		return -ENODEV;

	if (instance->hw_crit_error == 1) {
		error = -ENODEV;
	}

	if (instance->unload == 1) {
		return -ENODEV;
	}

	mutex_lock(&instance->aen_mutex);
	error = megasas_register_aen(instance, aen.seq_num,
				     aen.class_locale_word);
	mutex_unlock(&instance->aen_mutex);
	return error;
}

/**
 * megasas_mgmt_ioctl -	char node ioctl entry point
 */
static long
megasas_mgmt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case MEGASAS_IOC_FIRMWARE:
		return megasas_mgmt_ioctl_fw(file, arg);

	case MEGASAS_IOC_GET_AEN:
		return megasas_mgmt_ioctl_aen(file, arg);
	}

	return -ENOTTY;
}

#ifdef CONFIG_COMPAT
static int megasas_mgmt_compat_ioctl_fw(struct file *file, unsigned long arg)
{
	struct compat_megasas_iocpacket __user *cioc =
	    (struct compat_megasas_iocpacket __user *)arg;
	struct megasas_iocpacket __user *ioc =
	    compat_alloc_user_space(sizeof(struct megasas_iocpacket));
	int i;
	int error = 0;
	compat_uptr_t ptr;

	if (clear_user(ioc, sizeof(*ioc)))
		return -EFAULT;

	if (copy_in_user(&ioc->host_no, &cioc->host_no, sizeof(u16)) ||
	    copy_in_user(&ioc->sgl_off, &cioc->sgl_off, sizeof(u32)) ||
	    copy_in_user(&ioc->sense_off, &cioc->sense_off, sizeof(u32)) ||
	    copy_in_user(&ioc->sense_len, &cioc->sense_len, sizeof(u32)) ||
	    copy_in_user(ioc->frame.raw, cioc->frame.raw, 128) ||
	    copy_in_user(&ioc->sge_count, &cioc->sge_count, sizeof(u32)))
		return -EFAULT;

	/*
	 * The sense_ptr is used in megasas_mgmt_fw_ioctl only when
	 * sense_len is not null, so prepare the 64bit value under
	 * the same condition.
	 */
	if (ioc->sense_len) {
		void __user **sense_ioc_ptr =
			(void __user **)(ioc->frame.raw + ioc->sense_off);
		compat_uptr_t *sense_cioc_ptr =
			(compat_uptr_t *)(cioc->frame.raw + cioc->sense_off);
		if (get_user(ptr, sense_cioc_ptr) ||
		    put_user(compat_ptr(ptr), sense_ioc_ptr))
			return -EFAULT;
	}

	for (i = 0; i < MAX_IOCTL_SGE; i++) {
		if (get_user(ptr, &cioc->sgl[i].iov_base) ||
		    put_user(compat_ptr(ptr), &ioc->sgl[i].iov_base) ||
		    copy_in_user(&ioc->sgl[i].iov_len,
				 &cioc->sgl[i].iov_len, sizeof(compat_size_t)))
			return -EFAULT;
	}

	error = megasas_mgmt_ioctl_fw(file, (unsigned long)ioc);

	if (copy_in_user(&cioc->frame.hdr.cmd_status,
			 &ioc->frame.hdr.cmd_status, sizeof(u8))) {
		printk(KERN_DEBUG "megasas: error copy_in_user cmd_status\n");
		return -EFAULT;
	}
	return error;
}

static long
megasas_mgmt_compat_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	switch (cmd) {
	case MEGASAS_IOC_FIRMWARE32:
		return megasas_mgmt_compat_ioctl_fw(file, arg);
	case MEGASAS_IOC_GET_AEN:
		return megasas_mgmt_ioctl_aen(file, arg);
	}

	return -ENOTTY;
}
#endif

/*
 * File operations structure for management interface
 */
static const struct file_operations megasas_mgmt_fops = {
	.owner = THIS_MODULE,
	.open = megasas_mgmt_open,
	.fasync = megasas_mgmt_fasync,
	.unlocked_ioctl = megasas_mgmt_ioctl,
	.poll = megasas_mgmt_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = megasas_mgmt_compat_ioctl,
#endif
};

/*
 * PCI hotplug support registration structure
 */
static struct pci_driver megasas_pci_driver = {

	.name = "megaraid_sas",
	.id_table = megasas_pci_table,
	.probe = megasas_probe_one,
	.remove = __devexit_p(megasas_detach_one),
	.suspend = megasas_suspend,
	.resume = megasas_resume,
	.shutdown = megasas_shutdown,
};

/*
 * Sysfs driver attributes
 */
static ssize_t megasas_sysfs_show_version(struct device_driver *dd, char *buf)
{
	return snprintf(buf, strlen(MEGASAS_VERSION) + 2, "%s\n",
			MEGASAS_VERSION);
}

static DRIVER_ATTR(version, S_IRUGO, megasas_sysfs_show_version, NULL);

static ssize_t
megasas_sysfs_show_release_date(struct device_driver *dd, char *buf)
{
	return snprintf(buf, strlen(MEGASAS_RELDATE) + 2, "%s\n",
			MEGASAS_RELDATE);
}

static DRIVER_ATTR(release_date, S_IRUGO, megasas_sysfs_show_release_date,
		   NULL);

static ssize_t
megasas_sysfs_show_support_poll_for_event(struct device_driver *dd, char *buf)
{
	return sprintf(buf, "%u\n", support_poll_for_event);
}

static DRIVER_ATTR(support_poll_for_event, S_IRUGO,
			megasas_sysfs_show_support_poll_for_event, NULL);

static ssize_t
megasas_sysfs_show_dbg_lvl(struct device_driver *dd, char *buf)
{
	return sprintf(buf, "%u\n", megasas_dbg_lvl);
}

static ssize_t
megasas_sysfs_set_dbg_lvl(struct device_driver *dd, const char *buf, size_t count)
{
	int retval = count;
	if(sscanf(buf,"%u",&megasas_dbg_lvl)<1){
		printk(KERN_ERR "megasas: could not set dbg_lvl\n");
		retval = -EINVAL;
	}
	return retval;
}

static DRIVER_ATTR(dbg_lvl, S_IRUGO|S_IWUSR, megasas_sysfs_show_dbg_lvl,
		megasas_sysfs_set_dbg_lvl);

static ssize_t
megasas_sysfs_show_poll_mode_io(struct device_driver *dd, char *buf)
{
	return sprintf(buf, "%u\n", poll_mode_io);
}

static ssize_t
megasas_sysfs_set_poll_mode_io(struct device_driver *dd,
				const char *buf, size_t count)
{
	int retval = count;
	int tmp = poll_mode_io;
	int i;
	struct megasas_instance *instance;

	if (sscanf(buf, "%u", &poll_mode_io) < 1) {
		printk(KERN_ERR "megasas: could not set poll_mode_io\n");
		retval = -EINVAL;
	}

	/*
	 * Check if poll_mode_io is already set or is same as previous value
	 */
	if ((tmp && poll_mode_io) || (tmp == poll_mode_io))
		goto out;

	if (poll_mode_io) {
		/*
		 * Start timers for all adapters
		 */
		for (i = 0; i < megasas_mgmt_info.max_index; i++) {
			instance = megasas_mgmt_info.instance[i];
			if (instance) {
				megasas_start_timer(instance,
					&instance->io_completion_timer,
					megasas_io_completion_timer,
					MEGASAS_COMPLETION_TIMER_INTERVAL);
			}
		}
	} else {
		/*
		 * Delete timers for all adapters
		 */
		for (i = 0; i < megasas_mgmt_info.max_index; i++) {
			instance = megasas_mgmt_info.instance[i];
			if (instance)
				del_timer_sync(&instance->io_completion_timer);
		}
	}

out:
	return retval;
}

static void
megasas_aen_polling(struct work_struct *work)
{
	struct megasas_aen_event *ev =
		container_of(work, struct megasas_aen_event, hotplug_work);
	struct megasas_instance *instance = ev->instance;
	union megasas_evt_class_locale class_locale;
	struct  Scsi_Host *host;
	struct  scsi_device *sdev1;
	u16     pd_index = 0;
	u16	ld_index = 0;
	int     i, j, doscan = 0;
	u32 seq_num;
	int error;

	if (!instance) {
		printk(KERN_ERR "invalid instance!\n");
		kfree(ev);
		return;
	}
	instance->ev = NULL;
	host = instance->host;
	if (instance->evt_detail) {

		switch (instance->evt_detail->code) {
		case MR_EVT_PD_INSERTED:
			if (megasas_get_pd_list(instance) == 0) {
			for (i = 0; i < MEGASAS_MAX_PD_CHANNELS; i++) {
				for (j = 0;
				j < MEGASAS_MAX_DEV_PER_CHANNEL;
				j++) {

				pd_index =
				(i * MEGASAS_MAX_DEV_PER_CHANNEL) + j;

				sdev1 =
				scsi_device_lookup(host, i, j, 0);

				if (instance->pd_list[pd_index].driveState
						== MR_PD_STATE_SYSTEM) {
						if (!sdev1) {
						scsi_add_device(host, i, j, 0);
						}

					if (sdev1)
						scsi_device_put(sdev1);
					}
				}
			}
			}
			doscan = 0;
			break;

		case MR_EVT_PD_REMOVED:
			if (megasas_get_pd_list(instance) == 0) {
			megasas_get_pd_list(instance);
			for (i = 0; i < MEGASAS_MAX_PD_CHANNELS; i++) {
				for (j = 0;
				j < MEGASAS_MAX_DEV_PER_CHANNEL;
				j++) {

				pd_index =
				(i * MEGASAS_MAX_DEV_PER_CHANNEL) + j;

				sdev1 =
				scsi_device_lookup(host, i, j, 0);

				if (instance->pd_list[pd_index].driveState
					== MR_PD_STATE_SYSTEM) {
					if (sdev1) {
						scsi_device_put(sdev1);
					}
				} else {
					if (sdev1) {
						scsi_remove_device(sdev1);
						scsi_device_put(sdev1);
					}
				}
				}
			}
			}
			doscan = 0;
			break;

		case MR_EVT_LD_OFFLINE:
		case MR_EVT_LD_DELETED:
			megasas_get_ld_list(instance);
			for (i = 0; i < MEGASAS_MAX_LD_CHANNELS; i++) {
				for (j = 0;
				j < MEGASAS_MAX_DEV_PER_CHANNEL;
				j++) {

				ld_index =
				(i * MEGASAS_MAX_DEV_PER_CHANNEL) + j;

				sdev1 = scsi_device_lookup(host,
					i + MEGASAS_MAX_LD_CHANNELS,
					j,
					0);

				if (instance->ld_ids[ld_index] != 0xff) {
					if (sdev1) {
						scsi_device_put(sdev1);
					}
				} else {
					if (sdev1) {
						scsi_remove_device(sdev1);
						scsi_device_put(sdev1);
					}
				}
				}
			}
			doscan = 0;
			break;
		case MR_EVT_LD_CREATED:
			megasas_get_ld_list(instance);
			for (i = 0; i < MEGASAS_MAX_LD_CHANNELS; i++) {
				for (j = 0;
					j < MEGASAS_MAX_DEV_PER_CHANNEL;
					j++) {
					ld_index =
					(i * MEGASAS_MAX_DEV_PER_CHANNEL) + j;

					sdev1 = scsi_device_lookup(host,
						i+MEGASAS_MAX_LD_CHANNELS,
						j, 0);

					if (instance->ld_ids[ld_index] !=
								0xff) {
						if (!sdev1) {
							scsi_add_device(host,
								i + 2,
								j, 0);
						}
					}
					if (sdev1) {
						scsi_device_put(sdev1);
					}
				}
			}
			doscan = 0;
			break;
		case MR_EVT_CTRL_HOST_BUS_SCAN_REQUESTED:
		case MR_EVT_FOREIGN_CFG_IMPORTED:
			doscan = 1;
			break;
		default:
			doscan = 0;
			break;
		}
	} else {
		printk(KERN_ERR "invalid evt_detail!\n");
		kfree(ev);
		return;
	}

	if (doscan) {
		printk(KERN_INFO "scanning ...\n");
		megasas_get_pd_list(instance);
		for (i = 0; i < MEGASAS_MAX_PD_CHANNELS; i++) {
			for (j = 0; j < MEGASAS_MAX_DEV_PER_CHANNEL; j++) {
				pd_index = i*MEGASAS_MAX_DEV_PER_CHANNEL + j;
				sdev1 = scsi_device_lookup(host, i, j, 0);
				if (instance->pd_list[pd_index].driveState ==
							MR_PD_STATE_SYSTEM) {
					if (!sdev1) {
						scsi_add_device(host, i, j, 0);
					}
					if (sdev1)
						scsi_device_put(sdev1);
				} else {
					if (sdev1) {
						scsi_remove_device(sdev1);
						scsi_device_put(sdev1);
					}
				}
			}
		}

		megasas_get_ld_list(instance);
		for (i = 0; i < MEGASAS_MAX_LD_CHANNELS; i++) {
			for (j = 0; j < MEGASAS_MAX_DEV_PER_CHANNEL; j++) {
				ld_index =
				(i * MEGASAS_MAX_DEV_PER_CHANNEL) + j;

				sdev1 = scsi_device_lookup(host,
					i+MEGASAS_MAX_LD_CHANNELS, j, 0);
				if (instance->ld_ids[ld_index] != 0xff) {
					if (!sdev1) {
						scsi_add_device(host,
								i+2,
								j, 0);
					} else {
						scsi_device_put(sdev1);
					}
				} else {
					if (sdev1) {
						scsi_remove_device(sdev1);
						scsi_device_put(sdev1);
					}
				}
			}
		}
	}

	if ( instance->aen_cmd != NULL ) {
		kfree(ev);
		return ;
	}

	seq_num = instance->evt_detail->seq_num + 1;

	/* Register AEN with FW for latest sequence number plus 1 */
	class_locale.members.reserved = 0;
	class_locale.members.locale = MR_EVT_LOCALE_ALL;
	class_locale.members.class = MR_EVT_CLASS_DEBUG;
	mutex_lock(&instance->aen_mutex);
	error = megasas_register_aen(instance, seq_num,
					class_locale.word);
	mutex_unlock(&instance->aen_mutex);

	if (error)
		printk(KERN_ERR "register aen failed error %x\n", error);

	kfree(ev);
}


static DRIVER_ATTR(poll_mode_io, S_IRUGO|S_IWUSR,
		megasas_sysfs_show_poll_mode_io,
		megasas_sysfs_set_poll_mode_io);

/**
 * megasas_init - Driver load entry point
 */
static int __init megasas_init(void)
{
	int rval;

	/*
	 * Announce driver version and other information
	 */
	printk(KERN_INFO "megasas: %s %s\n", MEGASAS_VERSION,
	       MEGASAS_EXT_VERSION);

	support_poll_for_event = 2;

	memset(&megasas_mgmt_info, 0, sizeof(megasas_mgmt_info));

	/*
	 * Register character device node
	 */
	rval = register_chrdev(0, "megaraid_sas_ioctl", &megasas_mgmt_fops);

	if (rval < 0) {
		printk(KERN_DEBUG "megasas: failed to open device node\n");
		return rval;
	}

	megasas_mgmt_majorno = rval;

	/*
	 * Register ourselves as PCI hotplug module
	 */
	rval = pci_register_driver(&megasas_pci_driver);

	if (rval) {
		printk(KERN_DEBUG "megasas: PCI hotplug regisration failed \n");
		goto err_pcidrv;
	}

	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_version);
	if (rval)
		goto err_dcf_attr_ver;
	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_release_date);
	if (rval)
		goto err_dcf_rel_date;

	rval = driver_create_file(&megasas_pci_driver.driver,
				&driver_attr_support_poll_for_event);
	if (rval)
		goto err_dcf_support_poll_for_event;

	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_dbg_lvl);
	if (rval)
		goto err_dcf_dbg_lvl;
	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_poll_mode_io);
	if (rval)
		goto err_dcf_poll_mode_io;

	return rval;

err_dcf_poll_mode_io:
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_dbg_lvl);
err_dcf_dbg_lvl:
	driver_remove_file(&megasas_pci_driver.driver,
			&driver_attr_support_poll_for_event);

err_dcf_support_poll_for_event:
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_release_date);

err_dcf_rel_date:
	driver_remove_file(&megasas_pci_driver.driver, &driver_attr_version);
err_dcf_attr_ver:
	pci_unregister_driver(&megasas_pci_driver);
err_pcidrv:
	unregister_chrdev(megasas_mgmt_majorno, "megaraid_sas_ioctl");
  	return rval;
}

/**
 * megasas_exit - Driver unload entry point
 */
static void __exit megasas_exit(void)
{
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_poll_mode_io);
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_dbg_lvl);
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_release_date);
	driver_remove_file(&megasas_pci_driver.driver, &driver_attr_version);

	pci_unregister_driver(&megasas_pci_driver);
	unregister_chrdev(megasas_mgmt_majorno, "megaraid_sas_ioctl");
}

module_init(megasas_init);
module_exit(megasas_exit);
