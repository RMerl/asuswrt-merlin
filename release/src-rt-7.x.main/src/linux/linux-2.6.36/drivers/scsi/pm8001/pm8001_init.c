/*
 * PMC-Sierra SPC 8001 SAS/SATA based host adapters driver
 *
 * Copyright (c) 2008-2009 USI Co., Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 */

#include <linux/slab.h>
#include "pm8001_sas.h"
#include "pm8001_chips.h"

static struct scsi_transport_template *pm8001_stt;

static const struct pm8001_chip_info pm8001_chips[] = {
	[chip_8001] = {  8, &pm8001_8001_dispatch,},
};
static int pm8001_id;

LIST_HEAD(hba_list);

/**
 * The main structure which LLDD must register for scsi core.
 */
static struct scsi_host_template pm8001_sht = {
	.module			= THIS_MODULE,
	.name			= DRV_NAME,
	.queuecommand		= sas_queuecommand,
	.target_alloc		= sas_target_alloc,
	.slave_configure	= pm8001_slave_configure,
	.slave_destroy		= sas_slave_destroy,
	.scan_finished		= pm8001_scan_finished,
	.scan_start		= pm8001_scan_start,
	.change_queue_depth	= sas_change_queue_depth,
	.change_queue_type	= sas_change_queue_type,
	.bios_param		= sas_bios_param,
	.can_queue		= 1,
	.cmd_per_lun		= 1,
	.this_id		= -1,
	.sg_tablesize		= SG_ALL,
	.max_sectors		= SCSI_DEFAULT_MAX_SECTORS,
	.use_clustering		= ENABLE_CLUSTERING,
	.eh_device_reset_handler = sas_eh_device_reset_handler,
	.eh_bus_reset_handler	= sas_eh_bus_reset_handler,
	.slave_alloc		= pm8001_slave_alloc,
	.target_destroy		= sas_target_destroy,
	.ioctl			= sas_ioctl,
	.shost_attrs		= pm8001_host_attrs,
};

/**
 * Sas layer call this function to execute specific task.
 */
static struct sas_domain_function_template pm8001_transport_ops = {
	.lldd_dev_found		= pm8001_dev_found,
	.lldd_dev_gone		= pm8001_dev_gone,

	.lldd_execute_task	= pm8001_queue_command,
	.lldd_control_phy	= pm8001_phy_control,

	.lldd_abort_task	= pm8001_abort_task,
	.lldd_abort_task_set	= pm8001_abort_task_set,
	.lldd_clear_aca		= pm8001_clear_aca,
	.lldd_clear_task_set	= pm8001_clear_task_set,
	.lldd_I_T_nexus_reset   = pm8001_I_T_nexus_reset,
	.lldd_lu_reset		= pm8001_lu_reset,
	.lldd_query_task	= pm8001_query_task,
};

/**
 *pm8001_phy_init - initiate our adapter phys
 *@pm8001_ha: our hba structure.
 *@phy_id: phy id.
 */
static void __devinit pm8001_phy_init(struct pm8001_hba_info *pm8001_ha,
	int phy_id)
{
	struct pm8001_phy *phy = &pm8001_ha->phy[phy_id];
	struct asd_sas_phy *sas_phy = &phy->sas_phy;
	phy->phy_state = 0;
	phy->pm8001_ha = pm8001_ha;
	sas_phy->enabled = (phy_id < pm8001_ha->chip->n_phy) ? 1 : 0;
	sas_phy->class = SAS;
	sas_phy->iproto = SAS_PROTOCOL_ALL;
	sas_phy->tproto = 0;
	sas_phy->type = PHY_TYPE_PHYSICAL;
	sas_phy->role = PHY_ROLE_INITIATOR;
	sas_phy->oob_mode = OOB_NOT_CONNECTED;
	sas_phy->linkrate = SAS_LINK_RATE_UNKNOWN;
	sas_phy->id = phy_id;
	sas_phy->sas_addr = &pm8001_ha->sas_addr[0];
	sas_phy->frame_rcvd = &phy->frame_rcvd[0];
	sas_phy->ha = (struct sas_ha_struct *)pm8001_ha->shost->hostdata;
	sas_phy->lldd_phy = phy;
}

/**
 *pm8001_free - free hba
 *@pm8001_ha:	our hba structure.
 *
 */
static void pm8001_free(struct pm8001_hba_info *pm8001_ha)
{
	int i;
	struct pm8001_wq *wq;

	if (!pm8001_ha)
		return;

	for (i = 0; i < USI_MAX_MEMCNT; i++) {
		if (pm8001_ha->memoryMap.region[i].virt_ptr != NULL) {
			pci_free_consistent(pm8001_ha->pdev,
				pm8001_ha->memoryMap.region[i].element_size,
				pm8001_ha->memoryMap.region[i].virt_ptr,
				pm8001_ha->memoryMap.region[i].phys_addr);
			}
	}
	PM8001_CHIP_DISP->chip_iounmap(pm8001_ha);
	if (pm8001_ha->shost)
		scsi_host_put(pm8001_ha->shost);
	list_for_each_entry(wq, &pm8001_ha->wq_list, entry)
		cancel_delayed_work(&wq->work_q);
	kfree(pm8001_ha->tags);
	kfree(pm8001_ha);
}

#ifdef PM8001_USE_TASKLET
static void pm8001_tasklet(unsigned long opaque)
{
	struct pm8001_hba_info *pm8001_ha;
	pm8001_ha = (struct pm8001_hba_info *)opaque;;
	if (unlikely(!pm8001_ha))
		BUG_ON(1);
	PM8001_CHIP_DISP->isr(pm8001_ha);
}
#endif


 /**
  * pm8001_interrupt - when HBA originate a interrupt,we should invoke this
  * dispatcher to handle each case.
  * @irq: irq number.
  * @opaque: the passed general host adapter struct
  */
static irqreturn_t pm8001_interrupt(int irq, void *opaque)
{
	struct pm8001_hba_info *pm8001_ha;
	irqreturn_t ret = IRQ_HANDLED;
	struct sas_ha_struct *sha = opaque;
	pm8001_ha = sha->lldd_ha;
	if (unlikely(!pm8001_ha))
		return IRQ_NONE;
	if (!PM8001_CHIP_DISP->is_our_interupt(pm8001_ha))
		return IRQ_NONE;
#ifdef PM8001_USE_TASKLET
	tasklet_schedule(&pm8001_ha->tasklet);
#else
	ret = PM8001_CHIP_DISP->isr(pm8001_ha);
#endif
	return ret;
}

/**
 * pm8001_alloc - initiate our hba structure and 6 DMAs area.
 * @pm8001_ha:our hba structure.
 *
 */
static int __devinit pm8001_alloc(struct pm8001_hba_info *pm8001_ha)
{
	int i;
	spin_lock_init(&pm8001_ha->lock);
	for (i = 0; i < pm8001_ha->chip->n_phy; i++) {
		pm8001_phy_init(pm8001_ha, i);
		pm8001_ha->port[i].wide_port_phymap = 0;
		pm8001_ha->port[i].port_attached = 0;
		pm8001_ha->port[i].port_state = 0;
		INIT_LIST_HEAD(&pm8001_ha->port[i].list);
	}

	pm8001_ha->tags = kzalloc(PM8001_MAX_CCB, GFP_KERNEL);
	if (!pm8001_ha->tags)
		goto err_out;
	/* MPI Memory region 1 for AAP Event Log for fw */
	pm8001_ha->memoryMap.region[AAP1].num_elements = 1;
	pm8001_ha->memoryMap.region[AAP1].element_size = PM8001_EVENT_LOG_SIZE;
	pm8001_ha->memoryMap.region[AAP1].total_len = PM8001_EVENT_LOG_SIZE;
	pm8001_ha->memoryMap.region[AAP1].alignment = 32;

	/* MPI Memory region 2 for IOP Event Log for fw */
	pm8001_ha->memoryMap.region[IOP].num_elements = 1;
	pm8001_ha->memoryMap.region[IOP].element_size = PM8001_EVENT_LOG_SIZE;
	pm8001_ha->memoryMap.region[IOP].total_len = PM8001_EVENT_LOG_SIZE;
	pm8001_ha->memoryMap.region[IOP].alignment = 32;

	/* MPI Memory region 3 for consumer Index of inbound queues */
	pm8001_ha->memoryMap.region[CI].num_elements = 1;
	pm8001_ha->memoryMap.region[CI].element_size = 4;
	pm8001_ha->memoryMap.region[CI].total_len = 4;
	pm8001_ha->memoryMap.region[CI].alignment = 4;

	/* MPI Memory region 4 for producer Index of outbound queues */
	pm8001_ha->memoryMap.region[PI].num_elements = 1;
	pm8001_ha->memoryMap.region[PI].element_size = 4;
	pm8001_ha->memoryMap.region[PI].total_len = 4;
	pm8001_ha->memoryMap.region[PI].alignment = 4;

	/* MPI Memory region 5 inbound queues */
	pm8001_ha->memoryMap.region[IB].num_elements = 256;
	pm8001_ha->memoryMap.region[IB].element_size = 64;
	pm8001_ha->memoryMap.region[IB].total_len = 256 * 64;
	pm8001_ha->memoryMap.region[IB].alignment = 64;

	/* MPI Memory region 6 inbound queues */
	pm8001_ha->memoryMap.region[OB].num_elements = 256;
	pm8001_ha->memoryMap.region[OB].element_size = 64;
	pm8001_ha->memoryMap.region[OB].total_len = 256 * 64;
	pm8001_ha->memoryMap.region[OB].alignment = 64;

	/* Memory region write DMA*/
	pm8001_ha->memoryMap.region[NVMD].num_elements = 1;
	pm8001_ha->memoryMap.region[NVMD].element_size = 4096;
	pm8001_ha->memoryMap.region[NVMD].total_len = 4096;
	/* Memory region for devices*/
	pm8001_ha->memoryMap.region[DEV_MEM].num_elements = 1;
	pm8001_ha->memoryMap.region[DEV_MEM].element_size = PM8001_MAX_DEVICES *
		sizeof(struct pm8001_device);
	pm8001_ha->memoryMap.region[DEV_MEM].total_len = PM8001_MAX_DEVICES *
		sizeof(struct pm8001_device);

	/* Memory region for ccb_info*/
	pm8001_ha->memoryMap.region[CCB_MEM].num_elements = 1;
	pm8001_ha->memoryMap.region[CCB_MEM].element_size = PM8001_MAX_CCB *
		sizeof(struct pm8001_ccb_info);
	pm8001_ha->memoryMap.region[CCB_MEM].total_len = PM8001_MAX_CCB *
		sizeof(struct pm8001_ccb_info);

	for (i = 0; i < USI_MAX_MEMCNT; i++) {
		if (pm8001_mem_alloc(pm8001_ha->pdev,
			&pm8001_ha->memoryMap.region[i].virt_ptr,
			&pm8001_ha->memoryMap.region[i].phys_addr,
			&pm8001_ha->memoryMap.region[i].phys_addr_hi,
			&pm8001_ha->memoryMap.region[i].phys_addr_lo,
			pm8001_ha->memoryMap.region[i].total_len,
			pm8001_ha->memoryMap.region[i].alignment) != 0) {
				PM8001_FAIL_DBG(pm8001_ha,
					pm8001_printk("Mem%d alloc failed\n",
					i));
				goto err_out;
		}
	}

	pm8001_ha->devices = pm8001_ha->memoryMap.region[DEV_MEM].virt_ptr;
	for (i = 0; i < PM8001_MAX_DEVICES; i++) {
		pm8001_ha->devices[i].dev_type = NO_DEVICE;
		pm8001_ha->devices[i].id = i;
		pm8001_ha->devices[i].device_id = PM8001_MAX_DEVICES;
		pm8001_ha->devices[i].running_req = 0;
	}
	pm8001_ha->ccb_info = pm8001_ha->memoryMap.region[CCB_MEM].virt_ptr;
	for (i = 0; i < PM8001_MAX_CCB; i++) {
		pm8001_ha->ccb_info[i].ccb_dma_handle =
			pm8001_ha->memoryMap.region[CCB_MEM].phys_addr +
			i * sizeof(struct pm8001_ccb_info);
		pm8001_ha->ccb_info[i].task = NULL;
		pm8001_ha->ccb_info[i].ccb_tag = 0xffffffff;
		pm8001_ha->ccb_info[i].device = NULL;
		++pm8001_ha->tags_num;
	}
	pm8001_ha->flags = PM8001F_INIT_TIME;
	/* Initialize tags */
	pm8001_tag_init(pm8001_ha);
	return 0;
err_out:
	return 1;
}

/**
 * pm8001_ioremap - remap the pci high physical address to kernal virtual
 * address so that we can access them.
 * @pm8001_ha:our hba structure.
 */
static int pm8001_ioremap(struct pm8001_hba_info *pm8001_ha)
{
	u32 bar;
	u32 logicalBar = 0;
	struct pci_dev *pdev;

	pdev = pm8001_ha->pdev;
	/* map pci mem (PMC pci base 0-3)*/
	for (bar = 0; bar < 6; bar++) {
		/*
		** logical BARs for SPC:
		** bar 0 and 1 - logical BAR0
		** bar 2 and 3 - logical BAR1
		** bar4 - logical BAR2
		** bar5 - logical BAR3
		** Skip the appropriate assignments:
		*/
		if ((bar == 1) || (bar == 3))
			continue;
		if (pci_resource_flags(pdev, bar) & IORESOURCE_MEM) {
			pm8001_ha->io_mem[logicalBar].membase =
				pci_resource_start(pdev, bar);
			pm8001_ha->io_mem[logicalBar].membase &=
				(u32)PCI_BASE_ADDRESS_MEM_MASK;
			pm8001_ha->io_mem[logicalBar].memsize =
				pci_resource_len(pdev, bar);
			pm8001_ha->io_mem[logicalBar].memvirtaddr =
				ioremap(pm8001_ha->io_mem[logicalBar].membase,
				pm8001_ha->io_mem[logicalBar].memsize);
			PM8001_INIT_DBG(pm8001_ha,
				pm8001_printk("PCI: bar %d, logicalBar %d "
				"virt_addr=%lx,len=%d\n", bar, logicalBar,
				(unsigned long)
				pm8001_ha->io_mem[logicalBar].memvirtaddr,
				pm8001_ha->io_mem[logicalBar].memsize));
		} else {
			pm8001_ha->io_mem[logicalBar].membase	= 0;
			pm8001_ha->io_mem[logicalBar].memsize	= 0;
			pm8001_ha->io_mem[logicalBar].memvirtaddr = 0;
		}
		logicalBar++;
	}
	return 0;
}

/**
 * pm8001_pci_alloc - initialize our ha card structure
 * @pdev: pci device.
 * @ent: ent
 * @shost: scsi host struct which has been initialized before.
 */
static struct pm8001_hba_info *__devinit
pm8001_pci_alloc(struct pci_dev *pdev, u32 chip_id, struct Scsi_Host *shost)
{
	struct pm8001_hba_info *pm8001_ha;
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);


	pm8001_ha = sha->lldd_ha;
	if (!pm8001_ha)
		return NULL;

	pm8001_ha->pdev = pdev;
	pm8001_ha->dev = &pdev->dev;
	pm8001_ha->chip_id = chip_id;
	pm8001_ha->chip = &pm8001_chips[pm8001_ha->chip_id];
	pm8001_ha->irq = pdev->irq;
	pm8001_ha->sas = sha;
	pm8001_ha->shost = shost;
	pm8001_ha->id = pm8001_id++;
	INIT_LIST_HEAD(&pm8001_ha->wq_list);
	pm8001_ha->logging_level = 0x01;
	sprintf(pm8001_ha->name, "%s%d", DRV_NAME, pm8001_ha->id);
#ifdef PM8001_USE_TASKLET
	tasklet_init(&pm8001_ha->tasklet, pm8001_tasklet,
		(unsigned long)pm8001_ha);
#endif
	pm8001_ioremap(pm8001_ha);
	if (!pm8001_alloc(pm8001_ha))
		return pm8001_ha;
	pm8001_free(pm8001_ha);
	return NULL;
}

/**
 * pci_go_44 - pm8001 specified, its DMA is 44 bit rather than 64 bit
 * @pdev: pci device.
 */
static int pci_go_44(struct pci_dev *pdev)
{
	int rc;

	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(44))) {
		rc = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(44));
		if (rc) {
			rc = pci_set_consistent_dma_mask(pdev,
				DMA_BIT_MASK(32));
			if (rc) {
				dev_printk(KERN_ERR, &pdev->dev,
					"44-bit DMA enable failed\n");
				return rc;
			}
		}
	} else {
		rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (rc) {
			dev_printk(KERN_ERR, &pdev->dev,
				"32-bit DMA enable failed\n");
			return rc;
		}
		rc = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (rc) {
			dev_printk(KERN_ERR, &pdev->dev,
				"32-bit consistent DMA enable failed\n");
			return rc;
		}
	}
	return rc;
}

/**
 * pm8001_prep_sas_ha_init - allocate memory in general hba struct && init them.
 * @shost: scsi host which has been allocated outside.
 * @chip_info: our ha struct.
 */
static int __devinit pm8001_prep_sas_ha_init(struct Scsi_Host * shost,
	const struct pm8001_chip_info *chip_info)
{
	int phy_nr, port_nr;
	struct asd_sas_phy **arr_phy;
	struct asd_sas_port **arr_port;
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);

	phy_nr = chip_info->n_phy;
	port_nr = phy_nr;
	memset(sha, 0x00, sizeof(*sha));
	arr_phy = kcalloc(phy_nr, sizeof(void *), GFP_KERNEL);
	if (!arr_phy)
		goto exit;
	arr_port = kcalloc(port_nr, sizeof(void *), GFP_KERNEL);
	if (!arr_port)
		goto exit_free2;

	sha->sas_phy = arr_phy;
	sha->sas_port = arr_port;
	sha->lldd_ha = kzalloc(sizeof(struct pm8001_hba_info), GFP_KERNEL);
	if (!sha->lldd_ha)
		goto exit_free1;

	shost->transportt = pm8001_stt;
	shost->max_id = PM8001_MAX_DEVICES;
	shost->max_lun = 8;
	shost->max_channel = 0;
	shost->unique_id = pm8001_id;
	shost->max_cmd_len = 16;
	shost->can_queue = PM8001_CAN_QUEUE;
	shost->cmd_per_lun = 32;
	return 0;
exit_free1:
	kfree(arr_port);
exit_free2:
	kfree(arr_phy);
exit:
	return -1;
}

/**
 * pm8001_post_sas_ha_init - initialize general hba struct defined in libsas
 * @shost: scsi host which has been allocated outside
 * @chip_info: our ha struct.
 */
static void  __devinit pm8001_post_sas_ha_init(struct Scsi_Host *shost,
	const struct pm8001_chip_info *chip_info)
{
	int i = 0;
	struct pm8001_hba_info *pm8001_ha;
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);

	pm8001_ha = sha->lldd_ha;
	for (i = 0; i < chip_info->n_phy; i++) {
		sha->sas_phy[i] = &pm8001_ha->phy[i].sas_phy;
		sha->sas_port[i] = &pm8001_ha->port[i].sas_port;
	}
	sha->sas_ha_name = DRV_NAME;
	sha->dev = pm8001_ha->dev;

	sha->lldd_module = THIS_MODULE;
	sha->sas_addr = &pm8001_ha->sas_addr[0];
	sha->num_phys = chip_info->n_phy;
	sha->lldd_max_execute_num = 1;
	sha->lldd_queue_size = PM8001_CAN_QUEUE;
	sha->core.shost = shost;
}

/**
 * pm8001_init_sas_add - initialize sas address
 * @chip_info: our ha struct.
 *
 * Currently we just set the fixed SAS address to our HBA,for manufacture,
 * it should read from the EEPROM
 */
static void pm8001_init_sas_add(struct pm8001_hba_info *pm8001_ha)
{
	u8 i;
#ifdef PM8001_READ_VPD
	DECLARE_COMPLETION_ONSTACK(completion);
	struct pm8001_ioctl_payload payload;
	pm8001_ha->nvmd_completion = &completion;
	payload.minor_function = 0;
	payload.length = 128;
	payload.func_specific = kzalloc(128, GFP_KERNEL);
	PM8001_CHIP_DISP->get_nvmd_req(pm8001_ha, &payload);
	wait_for_completion(&completion);
	for (i = 0; i < pm8001_ha->chip->n_phy; i++) {
		memcpy(&pm8001_ha->phy[i].dev_sas_addr, pm8001_ha->sas_addr,
			SAS_ADDR_SIZE);
		PM8001_INIT_DBG(pm8001_ha,
			pm8001_printk("phy %d sas_addr = %016llx \n", i,
			pm8001_ha->phy[i].dev_sas_addr));
	}
#else
	for (i = 0; i < pm8001_ha->chip->n_phy; i++) {
		pm8001_ha->phy[i].dev_sas_addr = 0x50010c600047f9d0ULL;
		pm8001_ha->phy[i].dev_sas_addr =
			cpu_to_be64((u64)
				(*(u64 *)&pm8001_ha->phy[i].dev_sas_addr));
	}
	memcpy(pm8001_ha->sas_addr, &pm8001_ha->phy[0].dev_sas_addr,
		SAS_ADDR_SIZE);
#endif
}

#ifdef PM8001_USE_MSIX
/**
 * pm8001_setup_msix - enable MSI-X interrupt
 * @chip_info: our ha struct.
 * @irq_handler: irq_handler
 */
static u32 pm8001_setup_msix(struct pm8001_hba_info *pm8001_ha,
	irq_handler_t irq_handler)
{
	u32 i = 0, j = 0;
	u32 number_of_intr = 1;
	int flag = 0;
	u32 max_entry;
	int rc;
	max_entry = sizeof(pm8001_ha->msix_entries) /
		sizeof(pm8001_ha->msix_entries[0]);
	flag |= IRQF_DISABLED;
	for (i = 0; i < max_entry ; i++)
		pm8001_ha->msix_entries[i].entry = i;
	rc = pci_enable_msix(pm8001_ha->pdev, pm8001_ha->msix_entries,
		number_of_intr);
	pm8001_ha->number_of_intr = number_of_intr;
	if (!rc) {
		for (i = 0; i < number_of_intr; i++) {
			if (request_irq(pm8001_ha->msix_entries[i].vector,
				irq_handler, flag, DRV_NAME,
				SHOST_TO_SAS_HA(pm8001_ha->shost))) {
				for (j = 0; j < i; j++)
					free_irq(
					pm8001_ha->msix_entries[j].vector,
					SHOST_TO_SAS_HA(pm8001_ha->shost));
				pci_disable_msix(pm8001_ha->pdev);
				break;
			}
		}
	}
	return rc;
}
#endif

/**
 * pm8001_request_irq - register interrupt
 * @chip_info: our ha struct.
 */
static u32 pm8001_request_irq(struct pm8001_hba_info *pm8001_ha)
{
	struct pci_dev *pdev;
	irq_handler_t irq_handler = pm8001_interrupt;
	int rc;

	pdev = pm8001_ha->pdev;

#ifdef PM8001_USE_MSIX
	if (pci_find_capability(pdev, PCI_CAP_ID_MSIX))
		return pm8001_setup_msix(pm8001_ha, irq_handler);
	else
		goto intx;
#endif

intx:
	/* intialize the INT-X interrupt */
	rc = request_irq(pdev->irq, irq_handler, IRQF_SHARED, DRV_NAME,
		SHOST_TO_SAS_HA(pm8001_ha->shost));
	return rc;
}

/**
 * pm8001_pci_probe - probe supported device
 * @pdev: pci device which kernel has been prepared for.
 * @ent: pci device id
 *
 * This function is the main initialization function, when register a new
 * pci driver it is invoked, all struct an hardware initilization should be done
 * here, also, register interrupt
 */
static int __devinit pm8001_pci_probe(struct pci_dev *pdev,
	const struct pci_device_id *ent)
{
	unsigned int rc;
	u32	pci_reg;
	struct pm8001_hba_info *pm8001_ha;
	struct Scsi_Host *shost = NULL;
	const struct pm8001_chip_info *chip;

	dev_printk(KERN_INFO, &pdev->dev,
		"pm8001: driver version %s\n", DRV_VERSION);
	rc = pci_enable_device(pdev);
	if (rc)
		goto err_out_enable;
	pci_set_master(pdev);
	/*
	 * Enable pci slot busmaster by setting pci command register.
	 * This is required by FW for Cyclone card.
	 */

	pci_read_config_dword(pdev, PCI_COMMAND, &pci_reg);
	pci_reg |= 0x157;
	pci_write_config_dword(pdev, PCI_COMMAND, pci_reg);
	rc = pci_request_regions(pdev, DRV_NAME);
	if (rc)
		goto err_out_disable;
	rc = pci_go_44(pdev);
	if (rc)
		goto err_out_regions;

	shost = scsi_host_alloc(&pm8001_sht, sizeof(void *));
	if (!shost) {
		rc = -ENOMEM;
		goto err_out_regions;
	}
	chip = &pm8001_chips[ent->driver_data];
	SHOST_TO_SAS_HA(shost) =
		kzalloc(sizeof(struct sas_ha_struct), GFP_KERNEL);
	if (!SHOST_TO_SAS_HA(shost)) {
		rc = -ENOMEM;
		goto err_out_free_host;
	}

	rc = pm8001_prep_sas_ha_init(shost, chip);
	if (rc) {
		rc = -ENOMEM;
		goto err_out_free;
	}
	pci_set_drvdata(pdev, SHOST_TO_SAS_HA(shost));
	pm8001_ha = pm8001_pci_alloc(pdev, chip_8001, shost);
	if (!pm8001_ha) {
		rc = -ENOMEM;
		goto err_out_free;
	}
	list_add_tail(&pm8001_ha->list, &hba_list);
	PM8001_CHIP_DISP->chip_soft_rst(pm8001_ha, 0x252acbcd);
	rc = PM8001_CHIP_DISP->chip_init(pm8001_ha);
	if (rc)
		goto err_out_ha_free;

	rc = scsi_add_host(shost, &pdev->dev);
	if (rc)
		goto err_out_ha_free;
	rc = pm8001_request_irq(pm8001_ha);
	if (rc)
		goto err_out_shost;

	PM8001_CHIP_DISP->interrupt_enable(pm8001_ha);
	pm8001_init_sas_add(pm8001_ha);
	pm8001_post_sas_ha_init(shost, chip);
	rc = sas_register_ha(SHOST_TO_SAS_HA(shost));
	if (rc)
		goto err_out_shost;
	scsi_scan_host(pm8001_ha->shost);
	return 0;

err_out_shost:
	scsi_remove_host(pm8001_ha->shost);
err_out_ha_free:
	pm8001_free(pm8001_ha);
err_out_free:
	kfree(SHOST_TO_SAS_HA(shost));
err_out_free_host:
	kfree(shost);
err_out_regions:
	pci_release_regions(pdev);
err_out_disable:
	pci_disable_device(pdev);
err_out_enable:
	return rc;
}

static void __devexit pm8001_pci_remove(struct pci_dev *pdev)
{
	struct sas_ha_struct *sha = pci_get_drvdata(pdev);
	struct pm8001_hba_info *pm8001_ha;
	int i;
	pm8001_ha = sha->lldd_ha;
	pci_set_drvdata(pdev, NULL);
	sas_unregister_ha(sha);
	sas_remove_host(pm8001_ha->shost);
	list_del(&pm8001_ha->list);
	scsi_remove_host(pm8001_ha->shost);
	PM8001_CHIP_DISP->interrupt_disable(pm8001_ha);
	PM8001_CHIP_DISP->chip_soft_rst(pm8001_ha, 0x252acbcd);

#ifdef PM8001_USE_MSIX
	for (i = 0; i < pm8001_ha->number_of_intr; i++)
		synchronize_irq(pm8001_ha->msix_entries[i].vector);
	for (i = 0; i < pm8001_ha->number_of_intr; i++)
		free_irq(pm8001_ha->msix_entries[i].vector, sha);
	pci_disable_msix(pdev);
#else
	free_irq(pm8001_ha->irq, sha);
#endif
#ifdef PM8001_USE_TASKLET
	tasklet_kill(&pm8001_ha->tasklet);
#endif
	pm8001_free(pm8001_ha);
	kfree(sha->sas_phy);
	kfree(sha->sas_port);
	kfree(sha);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

/**
 * pm8001_pci_suspend - power management suspend main entry point
 * @pdev: PCI device struct
 * @state: PM state change to (usually PCI_D3)
 *
 * Returns 0 success, anything else error.
 */
static int pm8001_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct sas_ha_struct *sha = pci_get_drvdata(pdev);
	struct pm8001_hba_info *pm8001_ha;
	int i , pos;
	u32 device_state;
	pm8001_ha = sha->lldd_ha;
	flush_scheduled_work();
	scsi_block_requests(pm8001_ha->shost);
	pos = pci_find_capability(pdev, PCI_CAP_ID_PM);
	if (pos == 0) {
		printk(KERN_ERR " PCI PM not supported\n");
		return -ENODEV;
	}
	PM8001_CHIP_DISP->interrupt_disable(pm8001_ha);
	PM8001_CHIP_DISP->chip_soft_rst(pm8001_ha, 0x252acbcd);
#ifdef PM8001_USE_MSIX
	for (i = 0; i < pm8001_ha->number_of_intr; i++)
		synchronize_irq(pm8001_ha->msix_entries[i].vector);
	for (i = 0; i < pm8001_ha->number_of_intr; i++)
		free_irq(pm8001_ha->msix_entries[i].vector, sha);
	pci_disable_msix(pdev);
#else
	free_irq(pm8001_ha->irq, sha);
#endif
#ifdef PM8001_USE_TASKLET
	tasklet_kill(&pm8001_ha->tasklet);
#endif
	device_state = pci_choose_state(pdev, state);
	pm8001_printk("pdev=0x%p, slot=%s, entering "
		      "operating state [D%d]\n", pdev,
		      pm8001_ha->name, device_state);
	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, device_state);
	return 0;
}

/**
 * pm8001_pci_resume - power management resume main entry point
 * @pdev: PCI device struct
 *
 * Returns 0 success, anything else error.
 */
static int pm8001_pci_resume(struct pci_dev *pdev)
{
	struct sas_ha_struct *sha = pci_get_drvdata(pdev);
	struct pm8001_hba_info *pm8001_ha;
	int rc;
	u32 device_state;
	pm8001_ha = sha->lldd_ha;
	device_state = pdev->current_state;

	pm8001_printk("pdev=0x%p, slot=%s, resuming from previous "
		"operating state [D%d]\n", pdev, pm8001_ha->name, device_state);

	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);
	rc = pci_enable_device(pdev);
	if (rc) {
		pm8001_printk("slot=%s Enable device failed during resume\n",
			      pm8001_ha->name);
		goto err_out_enable;
	}

	pci_set_master(pdev);
	rc = pci_go_44(pdev);
	if (rc)
		goto err_out_disable;

	PM8001_CHIP_DISP->chip_soft_rst(pm8001_ha, 0x252acbcd);
	rc = PM8001_CHIP_DISP->chip_init(pm8001_ha);
	if (rc)
		goto err_out_disable;
	PM8001_CHIP_DISP->interrupt_disable(pm8001_ha);
	rc = pm8001_request_irq(pm8001_ha);
	if (rc)
		goto err_out_disable;
	#ifdef PM8001_USE_TASKLET
	tasklet_init(&pm8001_ha->tasklet, pm8001_tasklet,
		    (unsigned long)pm8001_ha);
	#endif
	PM8001_CHIP_DISP->interrupt_enable(pm8001_ha);
	scsi_unblock_requests(pm8001_ha->shost);
	return 0;

err_out_disable:
	scsi_remove_host(pm8001_ha->shost);
	pci_disable_device(pdev);
err_out_enable:
	return rc;
}

static struct pci_device_id __devinitdata pm8001_pci_table[] = {
	{
		PCI_VDEVICE(PMC_Sierra, 0x8001), chip_8001
	},
	{
		PCI_DEVICE(0x117c, 0x0042),
		.driver_data = chip_8001
	},
	{} /* terminate list */
};

static struct pci_driver pm8001_pci_driver = {
	.name		= DRV_NAME,
	.id_table	= pm8001_pci_table,
	.probe		= pm8001_pci_probe,
	.remove		= __devexit_p(pm8001_pci_remove),
	.suspend	= pm8001_pci_suspend,
	.resume		= pm8001_pci_resume,
};

/**
 *	pm8001_init - initialize scsi transport template
 */
static int __init pm8001_init(void)
{
	int rc;
	pm8001_id = 0;
	pm8001_stt = sas_domain_attach_transport(&pm8001_transport_ops);
	if (!pm8001_stt)
		return -ENOMEM;
	rc = pci_register_driver(&pm8001_pci_driver);
	if (rc)
		goto err_out;
	return 0;
err_out:
	sas_release_transport(pm8001_stt);
	return rc;
}

static void __exit pm8001_exit(void)
{
	pci_unregister_driver(&pm8001_pci_driver);
	sas_release_transport(pm8001_stt);
}

module_init(pm8001_init);
module_exit(pm8001_exit);

MODULE_AUTHOR("Jack Wang <jack_wang@usish.com>");
MODULE_DESCRIPTION("PMC-Sierra PM8001 SAS/SATA controller driver");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, pm8001_pci_table);
