/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2010 QLogic Corporation
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#include "qla_def.h"

#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/slab.h>

#include <scsi/scsi_tcq.h>
#include <scsi/scsicam.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>

/*
 * Driver version
 */
char qla2x00_version_str[40];

static int apidev_major;

/*
 * SRB allocation cache
 */
static struct kmem_cache *srb_cachep;

/*
 * CT6 CTX allocation cache
 */
static struct kmem_cache *ctx_cachep;

int ql2xlogintimeout = 20;
module_param(ql2xlogintimeout, int, S_IRUGO);
MODULE_PARM_DESC(ql2xlogintimeout,
		"Login timeout value in seconds.");

int qlport_down_retry;
module_param(qlport_down_retry, int, S_IRUGO);
MODULE_PARM_DESC(qlport_down_retry,
		"Maximum number of command retries to a port that returns "
		"a PORT-DOWN status.");

int ql2xplogiabsentdevice;
module_param(ql2xplogiabsentdevice, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xplogiabsentdevice,
		"Option to enable PLOGI to devices that are not present after "
		"a Fabric scan.  This is needed for several broken switches. "
		"Default is 0 - no PLOGI. 1 - perfom PLOGI.");

int ql2xloginretrycount = 0;
module_param(ql2xloginretrycount, int, S_IRUGO);
MODULE_PARM_DESC(ql2xloginretrycount,
		"Specify an alternate value for the NVRAM login retry count.");

int ql2xallocfwdump = 1;
module_param(ql2xallocfwdump, int, S_IRUGO);
MODULE_PARM_DESC(ql2xallocfwdump,
		"Option to enable allocation of memory for a firmware dump "
		"during HBA initialization.  Memory allocation requirements "
		"vary by ISP type.  Default is 1 - allocate memory.");

int ql2xextended_error_logging;
module_param(ql2xextended_error_logging, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xextended_error_logging,
		"Option to enable extended error logging, "
		"Default is 0 - no logging. 1 - log errors.");

int ql2xshiftctondsd = 6;
module_param(ql2xshiftctondsd, int, S_IRUGO);
MODULE_PARM_DESC(ql2xshiftctondsd,
		"Set to control shifting of command type processing "
		"based on total number of SG elements.");

static void qla2x00_free_device(scsi_qla_host_t *);

int ql2xfdmienable=1;
module_param(ql2xfdmienable, int, S_IRUGO);
MODULE_PARM_DESC(ql2xfdmienable,
		"Enables FDMI registrations. "
		"0 - no FDMI. Default is 1 - perform FDMI.");

#define MAX_Q_DEPTH    32
static int ql2xmaxqdepth = MAX_Q_DEPTH;
module_param(ql2xmaxqdepth, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xmaxqdepth,
		"Maximum queue depth to report for target devices.");

/* Do not change the value of this after module load */
int ql2xenabledif = 1;
module_param(ql2xenabledif, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xenabledif,
		" Enable T10-CRC-DIF "
		" Default is 0 - No DIF Support. 1 - Enable it");

int ql2xenablehba_err_chk;
module_param(ql2xenablehba_err_chk, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql2xenablehba_err_chk,
		" Enable T10-CRC-DIF Error isolation by HBA"
		" Default is 0 - Error isolation disabled, 1 - Enable it");

int ql2xiidmaenable=1;
module_param(ql2xiidmaenable, int, S_IRUGO);
MODULE_PARM_DESC(ql2xiidmaenable,
		"Enables iIDMA settings "
		"Default is 1 - perform iIDMA. 0 - no iIDMA.");

int ql2xmaxqueues = 1;
module_param(ql2xmaxqueues, int, S_IRUGO);
MODULE_PARM_DESC(ql2xmaxqueues,
		"Enables MQ settings "
		"Default is 1 for single queue. Set it to number "
		"of queues in MQ mode.");

int ql2xmultique_tag;
module_param(ql2xmultique_tag, int, S_IRUGO);
MODULE_PARM_DESC(ql2xmultique_tag,
		"Enables CPU affinity settings for the driver "
		"Default is 0 for no affinity of request and response IO. "
		"Set it to 1 to turn on the cpu affinity.");

int ql2xfwloadbin;
module_param(ql2xfwloadbin, int, S_IRUGO);
MODULE_PARM_DESC(ql2xfwloadbin,
		"Option to specify location from which to load ISP firmware:\n"
		" 2 -- load firmware via the request_firmware() (hotplug)\n"
		"      interface.\n"
		" 1 -- load firmware from flash.\n"
		" 0 -- use default semantics.\n");

int ql2xetsenable;
module_param(ql2xetsenable, int, S_IRUGO);
MODULE_PARM_DESC(ql2xetsenable,
		"Enables firmware ETS burst."
		"Default is 0 - skip ETS enablement.");

int ql2xdbwr = 1;
module_param(ql2xdbwr, int, S_IRUGO);
MODULE_PARM_DESC(ql2xdbwr,
	"Option to specify scheme for request queue posting\n"
	" 0 -- Regular doorbell.\n"
	" 1 -- CAMRAM doorbell (faster).\n");

int ql2xtargetreset = 1;
module_param(ql2xtargetreset, int, S_IRUGO);
MODULE_PARM_DESC(ql2xtargetreset,
		 "Enable target reset."
		 "Default is 1 - use hw defaults.");

int ql2xgffidenable;
module_param(ql2xgffidenable, int, S_IRUGO);
MODULE_PARM_DESC(ql2xgffidenable,
		"Enables GFF_ID checks of port type. "
		"Default is 0 - Do not use GFF_ID information.");

int ql2xasynctmfenable;
module_param(ql2xasynctmfenable, int, S_IRUGO);
MODULE_PARM_DESC(ql2xasynctmfenable,
		"Enables issue of TM IOCBs asynchronously via IOCB mechanism"
		"Default is 0 - Issue TM IOCBs via mailbox mechanism.");
/*
 * SCSI host template entry points
 */
static int qla2xxx_slave_configure(struct scsi_device * device);
static int qla2xxx_slave_alloc(struct scsi_device *);
static int qla2xxx_scan_finished(struct Scsi_Host *, unsigned long time);
static void qla2xxx_scan_start(struct Scsi_Host *);
static void qla2xxx_slave_destroy(struct scsi_device *);
static int qla2xxx_queuecommand(struct Scsi_Host *h, struct scsi_cmnd *cmd);
static int qla2xxx_eh_abort(struct scsi_cmnd *);
static int qla2xxx_eh_device_reset(struct scsi_cmnd *);
static int qla2xxx_eh_target_reset(struct scsi_cmnd *);
static int qla2xxx_eh_bus_reset(struct scsi_cmnd *);
static int qla2xxx_eh_host_reset(struct scsi_cmnd *);

static int qla2x00_change_queue_depth(struct scsi_device *, int, int);
static int qla2x00_change_queue_type(struct scsi_device *, int);

struct scsi_host_template qla2xxx_driver_template = {
	.module			= THIS_MODULE,
	.name			= QLA2XXX_DRIVER_NAME,
	.queuecommand		= qla2xxx_queuecommand,

	.eh_abort_handler	= qla2xxx_eh_abort,
	.eh_device_reset_handler = qla2xxx_eh_device_reset,
	.eh_target_reset_handler = qla2xxx_eh_target_reset,
	.eh_bus_reset_handler	= qla2xxx_eh_bus_reset,
	.eh_host_reset_handler	= qla2xxx_eh_host_reset,

	.slave_configure	= qla2xxx_slave_configure,

	.slave_alloc		= qla2xxx_slave_alloc,
	.slave_destroy		= qla2xxx_slave_destroy,
	.scan_finished		= qla2xxx_scan_finished,
	.scan_start		= qla2xxx_scan_start,
	.change_queue_depth	= qla2x00_change_queue_depth,
	.change_queue_type	= qla2x00_change_queue_type,
	.this_id		= -1,
	.cmd_per_lun		= 3,
	.use_clustering		= ENABLE_CLUSTERING,
	.sg_tablesize		= SG_ALL,

	.max_sectors		= 0xFFFF,
	.shost_attrs		= qla2x00_host_attrs,
};

static struct scsi_transport_template *qla2xxx_transport_template = NULL;
struct scsi_transport_template *qla2xxx_transport_vport_template = NULL;

/* TODO Convert to inlines
 *
 * Timer routines
 */

__inline__ void
qla2x00_start_timer(scsi_qla_host_t *vha, void *func, unsigned long interval)
{
	init_timer(&vha->timer);
	vha->timer.expires = jiffies + interval * HZ;
	vha->timer.data = (unsigned long)vha;
	vha->timer.function = (void (*)(unsigned long))func;
	add_timer(&vha->timer);
	vha->timer_active = 1;
}

static inline void
qla2x00_restart_timer(scsi_qla_host_t *vha, unsigned long interval)
{
	/* Currently used for 82XX only. */
	if (vha->device_flags & DFLG_DEV_FAILED)
		return;

	mod_timer(&vha->timer, jiffies + interval * HZ);
}

static __inline__ void
qla2x00_stop_timer(scsi_qla_host_t *vha)
{
	del_timer_sync(&vha->timer);
	vha->timer_active = 0;
}

static int qla2x00_do_dpc(void *data);

static void qla2x00_rst_aen(scsi_qla_host_t *);

static int qla2x00_mem_alloc(struct qla_hw_data *, uint16_t, uint16_t,
	struct req_que **, struct rsp_que **);
static void qla2x00_free_fw_dump(struct qla_hw_data *);
static void qla2x00_mem_free(struct qla_hw_data *);
static void qla2x00_sp_free_dma(srb_t *);

/* -------------------------------------------------------------------------- */
static int qla2x00_alloc_queues(struct qla_hw_data *ha)
{
	ha->req_q_map = kzalloc(sizeof(struct req_que *) * ha->max_req_queues,
				GFP_KERNEL);
	if (!ha->req_q_map) {
		qla_printk(KERN_WARNING, ha,
			"Unable to allocate memory for request queue ptrs\n");
		goto fail_req_map;
	}

	ha->rsp_q_map = kzalloc(sizeof(struct rsp_que *) * ha->max_rsp_queues,
				GFP_KERNEL);
	if (!ha->rsp_q_map) {
		qla_printk(KERN_WARNING, ha,
			"Unable to allocate memory for response queue ptrs\n");
		goto fail_rsp_map;
	}
	set_bit(0, ha->rsp_qid_map);
	set_bit(0, ha->req_qid_map);
	return 1;

fail_rsp_map:
	kfree(ha->req_q_map);
	ha->req_q_map = NULL;
fail_req_map:
	return -ENOMEM;
}

static void qla2x00_free_req_que(struct qla_hw_data *ha, struct req_que *req)
{
	if (req && req->ring)
		dma_free_coherent(&ha->pdev->dev,
		(req->length + 1) * sizeof(request_t),
		req->ring, req->dma);

	kfree(req);
	req = NULL;
}

static void qla2x00_free_rsp_que(struct qla_hw_data *ha, struct rsp_que *rsp)
{
	if (rsp && rsp->ring)
		dma_free_coherent(&ha->pdev->dev,
		(rsp->length + 1) * sizeof(response_t),
		rsp->ring, rsp->dma);

	kfree(rsp);
	rsp = NULL;
}

static void qla2x00_free_queues(struct qla_hw_data *ha)
{
	struct req_que *req;
	struct rsp_que *rsp;
	int cnt;

	for (cnt = 0; cnt < ha->max_req_queues; cnt++) {
		req = ha->req_q_map[cnt];
		qla2x00_free_req_que(ha, req);
	}
	kfree(ha->req_q_map);
	ha->req_q_map = NULL;

	for (cnt = 0; cnt < ha->max_rsp_queues; cnt++) {
		rsp = ha->rsp_q_map[cnt];
		qla2x00_free_rsp_que(ha, rsp);
	}
	kfree(ha->rsp_q_map);
	ha->rsp_q_map = NULL;
}

static int qla25xx_setup_mode(struct scsi_qla_host *vha)
{
	uint16_t options = 0;
	int ques, req, ret;
	struct qla_hw_data *ha = vha->hw;

	if (!(ha->fw_attributes & BIT_6)) {
		qla_printk(KERN_INFO, ha,
			"Firmware is not multi-queue capable\n");
		goto fail;
	}
	if (ql2xmultique_tag) {
		/* create a request queue for IO */
		options |= BIT_7;
		req = qla25xx_create_req_que(ha, options, 0, 0, -1,
			QLA_DEFAULT_QUE_QOS);
		if (!req) {
			qla_printk(KERN_WARNING, ha,
				"Can't create request queue\n");
			goto fail;
		}
		ha->wq = alloc_workqueue("qla2xxx_wq", WQ_MEM_RECLAIM, 1);
		vha->req = ha->req_q_map[req];
		options |= BIT_1;
		for (ques = 1; ques < ha->max_rsp_queues; ques++) {
			ret = qla25xx_create_rsp_que(ha, options, 0, 0, req);
			if (!ret) {
				qla_printk(KERN_WARNING, ha,
					"Response Queue create failed\n");
				goto fail2;
			}
		}
		ha->flags.cpu_affinity_enabled = 1;

		DEBUG2(qla_printk(KERN_INFO, ha,
			"CPU affinity mode enabled, no. of response"
			" queues:%d, no. of request queues:%d\n",
			ha->max_rsp_queues, ha->max_req_queues));
	}
	return 0;
fail2:
	qla25xx_delete_queues(vha);
	destroy_workqueue(ha->wq);
	ha->wq = NULL;
fail:
	ha->mqenable = 0;
	kfree(ha->req_q_map);
	kfree(ha->rsp_q_map);
	ha->max_req_queues = ha->max_rsp_queues = 1;
	return 1;
}

static char *
qla2x00_pci_info_str(struct scsi_qla_host *vha, char *str)
{
	struct qla_hw_data *ha = vha->hw;
	static char *pci_bus_modes[] = {
		"33", "66", "100", "133",
	};
	uint16_t pci_bus;

	strcpy(str, "PCI");
	pci_bus = (ha->pci_attr & (BIT_9 | BIT_10)) >> 9;
	if (pci_bus) {
		strcat(str, "-X (");
		strcat(str, pci_bus_modes[pci_bus]);
	} else {
		pci_bus = (ha->pci_attr & BIT_8) >> 8;
		strcat(str, " (");
		strcat(str, pci_bus_modes[pci_bus]);
	}
	strcat(str, " MHz)");

	return (str);
}

static char *
qla24xx_pci_info_str(struct scsi_qla_host *vha, char *str)
{
	static char *pci_bus_modes[] = { "33", "66", "100", "133", };
	struct qla_hw_data *ha = vha->hw;
	uint32_t pci_bus;
	int pcie_reg;

	pcie_reg = pci_find_capability(ha->pdev, PCI_CAP_ID_EXP);
	if (pcie_reg) {
		char lwstr[6];
		uint16_t pcie_lstat, lspeed, lwidth;

		pcie_reg += 0x12;
		pci_read_config_word(ha->pdev, pcie_reg, &pcie_lstat);
		lspeed = pcie_lstat & (BIT_0 | BIT_1 | BIT_2 | BIT_3);
		lwidth = (pcie_lstat &
		    (BIT_4 | BIT_5 | BIT_6 | BIT_7 | BIT_8 | BIT_9)) >> 4;

		strcpy(str, "PCIe (");
		if (lspeed == 1)
			strcat(str, "2.5GT/s ");
		else if (lspeed == 2)
			strcat(str, "5.0GT/s ");
		else
			strcat(str, "<unknown> ");
		snprintf(lwstr, sizeof(lwstr), "x%d)", lwidth);
		strcat(str, lwstr);

		return str;
	}

	strcpy(str, "PCI");
	pci_bus = (ha->pci_attr & CSRX_PCIX_BUS_MODE_MASK) >> 8;
	if (pci_bus == 0 || pci_bus == 8) {
		strcat(str, " (");
		strcat(str, pci_bus_modes[pci_bus >> 3]);
	} else {
		strcat(str, "-X ");
		if (pci_bus & BIT_2)
			strcat(str, "Mode 2");
		else
			strcat(str, "Mode 1");
		strcat(str, " (");
		strcat(str, pci_bus_modes[pci_bus & ~BIT_2]);
	}
	strcat(str, " MHz)");

	return str;
}

static char *
qla2x00_fw_version_str(struct scsi_qla_host *vha, char *str)
{
	char un_str[10];
	struct qla_hw_data *ha = vha->hw;

	sprintf(str, "%d.%02d.%02d ", ha->fw_major_version,
	    ha->fw_minor_version,
	    ha->fw_subminor_version);

	if (ha->fw_attributes & BIT_9) {
		strcat(str, "FLX");
		return (str);
	}

	switch (ha->fw_attributes & 0xFF) {
	case 0x7:
		strcat(str, "EF");
		break;
	case 0x17:
		strcat(str, "TP");
		break;
	case 0x37:
		strcat(str, "IP");
		break;
	case 0x77:
		strcat(str, "VI");
		break;
	default:
		sprintf(un_str, "(%x)", ha->fw_attributes);
		strcat(str, un_str);
		break;
	}
	if (ha->fw_attributes & 0x100)
		strcat(str, "X");

	return (str);
}

static char *
qla24xx_fw_version_str(struct scsi_qla_host *vha, char *str)
{
	struct qla_hw_data *ha = vha->hw;

	sprintf(str, "%d.%02d.%02d (%x)", ha->fw_major_version,
	    ha->fw_minor_version, ha->fw_subminor_version, ha->fw_attributes);
	return str;
}

static inline srb_t *
qla2x00_get_new_sp(scsi_qla_host_t *vha, fc_port_t *fcport,
	struct scsi_cmnd *cmd)
{
	srb_t *sp;
	struct qla_hw_data *ha = vha->hw;

	sp = mempool_alloc(ha->srb_mempool, GFP_ATOMIC);
	if (!sp)
		return sp;

	atomic_set(&sp->ref_count, 1);
	sp->fcport = fcport;
	sp->cmd = cmd;
	sp->flags = 0;
	CMD_SP(cmd) = (void *)sp;
	sp->ctx = NULL;

	return sp;
}

static int
qla2xxx_queuecommand(struct Scsi_Host *host, struct scsi_cmnd *cmd)
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	fc_port_t *fcport = (struct fc_port *) cmd->device->hostdata;
	struct fc_rport *rport = starget_to_rport(scsi_target(cmd->device));
	struct qla_hw_data *ha = vha->hw;
	struct scsi_qla_host *base_vha = pci_get_drvdata(ha->pdev);
	srb_t *sp;
	int rval;

	if (ha->flags.eeh_busy) {
		if (ha->flags.pci_channel_io_perm_failure)
			cmd->result = DID_NO_CONNECT << 16;
		else
			cmd->result = DID_REQUEUE << 16;
		goto qc24_fail_command;
	}

	rval = fc_remote_port_chkready(rport);
	if (rval) {
		cmd->result = rval;
		goto qc24_fail_command;
	}

	if (!vha->flags.difdix_supported &&
		scsi_get_prot_op(cmd) != SCSI_PROT_NORMAL) {
			DEBUG2(qla_printk(KERN_ERR, ha,
			    "DIF Cap Not Reg, fail DIF capable cmd's:%x\n",
			    cmd->cmnd[0]));
			cmd->result = DID_NO_CONNECT << 16;
			goto qc24_fail_command;
	}
	if (atomic_read(&fcport->state) != FCS_ONLINE) {
		if (atomic_read(&fcport->state) == FCS_DEVICE_DEAD ||
			atomic_read(&base_vha->loop_state) == LOOP_DEAD) {
			cmd->result = DID_NO_CONNECT << 16;
			goto qc24_fail_command;
		}
		goto qc24_target_busy;
	}

	sp = qla2x00_get_new_sp(base_vha, fcport, cmd);
	if (!sp)
		goto qc24_host_busy;

	rval = ha->isp_ops->start_scsi(sp);
	if (rval != QLA_SUCCESS)
		goto qc24_host_busy_free_sp;

	return 0;

qc24_host_busy_free_sp:
	qla2x00_sp_free_dma(sp);
	mempool_free(sp, ha->srb_mempool);

qc24_host_busy:
	return SCSI_MLQUEUE_HOST_BUSY;

qc24_target_busy:
	return SCSI_MLQUEUE_TARGET_BUSY;

qc24_fail_command:
	cmd->scsi_done(cmd);

	return 0;
}

/*
 * qla2x00_eh_wait_on_command
 *    Waits for the command to be returned by the Firmware for some
 *    max time.
 *
 * Input:
 *    cmd = Scsi Command to wait on.
 *
 * Return:
 *    Not Found : 0
 *    Found : 1
 */
static int
qla2x00_eh_wait_on_command(struct scsi_cmnd *cmd)
{
#define ABORT_POLLING_PERIOD	1000
#define ABORT_WAIT_ITER		((10 * 1000) / (ABORT_POLLING_PERIOD))
	unsigned long wait_iter = ABORT_WAIT_ITER;
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	struct qla_hw_data *ha = vha->hw;
	int ret = QLA_SUCCESS;

	if (unlikely(pci_channel_offline(ha->pdev)) || ha->flags.eeh_busy) {
		DEBUG17(qla_printk(KERN_WARNING, ha, "return:eh_wait\n"));
		return ret;
	}

	while (CMD_SP(cmd) && wait_iter--) {
		msleep(ABORT_POLLING_PERIOD);
	}
	if (CMD_SP(cmd))
		ret = QLA_FUNCTION_FAILED;

	return ret;
}

/*
 * qla2x00_wait_for_hba_online
 *    Wait till the HBA is online after going through
 *    <= MAX_RETRIES_OF_ISP_ABORT  or
 *    finally HBA is disabled ie marked offline
 *
 * Input:
 *     ha - pointer to host adapter structure
 *
 * Note:
 *    Does context switching-Release SPIN_LOCK
 *    (if any) before calling this routine.
 *
 * Return:
 *    Success (Adapter is online) : 0
 *    Failed  (Adapter is offline/disabled) : 1
 */
int
qla2x00_wait_for_hba_online(scsi_qla_host_t *vha)
{
	int		return_status;
	unsigned long	wait_online;
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	wait_online = jiffies + (MAX_LOOP_TIMEOUT * HZ);
	while (((test_bit(ISP_ABORT_NEEDED, &base_vha->dpc_flags)) ||
	    test_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags) ||
	    test_bit(ISP_ABORT_RETRY, &base_vha->dpc_flags) ||
	    ha->dpc_active) && time_before(jiffies, wait_online)) {

		msleep(1000);
	}
	if (base_vha->flags.online)
		return_status = QLA_SUCCESS;
	else
		return_status = QLA_FUNCTION_FAILED;

	return (return_status);
}

/*
 * qla2x00_wait_for_reset_ready
 *    Wait till the HBA is online after going through
 *    <= MAX_RETRIES_OF_ISP_ABORT  or
 *    finally HBA is disabled ie marked offline or flash
 *    operations are in progress.
 *
 * Input:
 *     ha - pointer to host adapter structure
 *
 * Note:
 *    Does context switching-Release SPIN_LOCK
 *    (if any) before calling this routine.
 *
 * Return:
 *    Success (Adapter is online/no flash ops) : 0
 *    Failed  (Adapter is offline/disabled/flash ops in progress) : 1
 */
static int
qla2x00_wait_for_reset_ready(scsi_qla_host_t *vha)
{
	int		return_status;
	unsigned long	wait_online;
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	wait_online = jiffies + (MAX_LOOP_TIMEOUT * HZ);
	while (((test_bit(ISP_ABORT_NEEDED, &base_vha->dpc_flags)) ||
	    test_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags) ||
	    test_bit(ISP_ABORT_RETRY, &base_vha->dpc_flags) ||
	    ha->optrom_state != QLA_SWAITING ||
	    ha->dpc_active) && time_before(jiffies, wait_online))
		msleep(1000);

	if (base_vha->flags.online &&  ha->optrom_state == QLA_SWAITING)
		return_status = QLA_SUCCESS;
	else
		return_status = QLA_FUNCTION_FAILED;

	DEBUG2(printk("%s return_status=%d\n", __func__, return_status));

	return return_status;
}

int
qla2x00_wait_for_chip_reset(scsi_qla_host_t *vha)
{
	int		return_status;
	unsigned long	wait_reset;
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	wait_reset = jiffies + (MAX_LOOP_TIMEOUT * HZ);
	while (((test_bit(ISP_ABORT_NEEDED, &base_vha->dpc_flags)) ||
	    test_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags) ||
	    test_bit(ISP_ABORT_RETRY, &base_vha->dpc_flags) ||
	    ha->dpc_active) && time_before(jiffies, wait_reset)) {

		msleep(1000);

		if (!test_bit(ISP_ABORT_NEEDED, &base_vha->dpc_flags) &&
		    ha->flags.chip_reset_done)
			break;
	}
	if (ha->flags.chip_reset_done)
		return_status = QLA_SUCCESS;
	else
		return_status = QLA_FUNCTION_FAILED;

	return return_status;
}

/*
 * qla2x00_wait_for_loop_ready
 *    Wait for MAX_LOOP_TIMEOUT(5 min) value for loop
 *    to be in LOOP_READY state.
 * Input:
 *     ha - pointer to host adapter structure
 *
 * Note:
 *    Does context switching-Release SPIN_LOCK
 *    (if any) before calling this routine.
 *
 *
 * Return:
 *    Success (LOOP_READY) : 0
 *    Failed  (LOOP_NOT_READY) : 1
 */
static inline int
qla2x00_wait_for_loop_ready(scsi_qla_host_t *vha)
{
	int 	 return_status = QLA_SUCCESS;
	unsigned long loop_timeout ;
	struct qla_hw_data *ha = vha->hw;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	/* wait for 5 min at the max for loop to be ready */
	loop_timeout = jiffies + (MAX_LOOP_TIMEOUT * HZ);

	while ((!atomic_read(&base_vha->loop_down_timer) &&
	    atomic_read(&base_vha->loop_state) == LOOP_DOWN) ||
	    atomic_read(&base_vha->loop_state) != LOOP_READY) {
		if (atomic_read(&base_vha->loop_state) == LOOP_DEAD) {
			return_status = QLA_FUNCTION_FAILED;
			break;
		}
		msleep(1000);
		if (time_after_eq(jiffies, loop_timeout)) {
			return_status = QLA_FUNCTION_FAILED;
			break;
		}
	}
	return (return_status);
}

static void
sp_get(struct srb *sp)
{
	atomic_inc(&sp->ref_count);
}

/**************************************************************************
* qla2xxx_eh_abort
*
* Description:
*    The abort function will abort the specified command.
*
* Input:
*    cmd = Linux SCSI command packet to be aborted.
*
* Returns:
*    Either SUCCESS or FAILED.
*
* Note:
*    Only return FAILED if command not returned by firmware.
**************************************************************************/
static int
qla2xxx_eh_abort(struct scsi_cmnd *cmd)
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	srb_t *sp;
	int ret;
	unsigned int id, lun;
	unsigned long flags;
	int wait = 0;
	struct qla_hw_data *ha = vha->hw;

	if (!CMD_SP(cmd))
		return SUCCESS;

	ret = fc_block_scsi_eh(cmd);
	if (ret != 0)
		return ret;
	ret = SUCCESS;

	id = cmd->device->id;
	lun = cmd->device->lun;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	sp = (srb_t *) CMD_SP(cmd);
	if (!sp) {
		spin_unlock_irqrestore(&ha->hardware_lock, flags);
		return SUCCESS;
	}

	DEBUG2(printk("%s(%ld): aborting sp %p from RISC.",
	    __func__, vha->host_no, sp));

	/* Get a reference to the sp and drop the lock.*/
	sp_get(sp);

	spin_unlock_irqrestore(&ha->hardware_lock, flags);
	if (ha->isp_ops->abort_command(sp)) {
		DEBUG2(printk("%s(%ld): abort_command "
		"mbx failed.\n", __func__, vha->host_no));
		ret = FAILED;
	} else {
		DEBUG3(printk("%s(%ld): abort_command "
		"mbx success.\n", __func__, vha->host_no));
		wait = 1;
	}
	qla2x00_sp_compl(ha, sp);

	/* Wait for the command to be returned. */
	if (wait) {
		if (qla2x00_eh_wait_on_command(cmd) != QLA_SUCCESS) {
			qla_printk(KERN_ERR, ha,
			    "scsi(%ld:%d:%d): Abort handler timed out -- %x.\n",
			    vha->host_no, id, lun, ret);
			ret = FAILED;
		}
	}

	qla_printk(KERN_INFO, ha,
	    "scsi(%ld:%d:%d): Abort command issued -- %d %x.\n",
	    vha->host_no, id, lun, wait, ret);

	return ret;
}

int
qla2x00_eh_wait_for_pending_commands(scsi_qla_host_t *vha, unsigned int t,
	unsigned int l, enum nexus_wait_type type)
{
	int cnt, match, status;
	unsigned long flags;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req;
	srb_t *sp;

	status = QLA_SUCCESS;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	req = vha->req;
	for (cnt = 1; status == QLA_SUCCESS &&
		cnt < MAX_OUTSTANDING_COMMANDS; cnt++) {
		sp = req->outstanding_cmds[cnt];
		if (!sp)
			continue;
		if ((sp->ctx) && !IS_PROT_IO(sp))
			continue;
		if (vha->vp_idx != sp->fcport->vha->vp_idx)
			continue;
		match = 0;
		switch (type) {
		case WAIT_HOST:
			match = 1;
			break;
		case WAIT_TARGET:
			match = sp->cmd->device->id == t;
			break;
		case WAIT_LUN:
			match = (sp->cmd->device->id == t &&
				sp->cmd->device->lun == l);
			break;
		}
		if (!match)
			continue;

		spin_unlock_irqrestore(&ha->hardware_lock, flags);
		status = qla2x00_eh_wait_on_command(sp->cmd);
		spin_lock_irqsave(&ha->hardware_lock, flags);
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	return status;
}

static char *reset_errors[] = {
	"HBA not online",
	"HBA not ready",
	"Task management failed",
	"Waiting for command completions",
};

static int
__qla2xxx_eh_generic_reset(char *name, enum nexus_wait_type type,
    struct scsi_cmnd *cmd, int (*do_reset)(struct fc_port *, unsigned int, int))
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	fc_port_t *fcport = (struct fc_port *) cmd->device->hostdata;
	int err;

	if (!fcport)
		return FAILED;

	err = fc_block_scsi_eh(cmd);
	if (err != 0)
		return err;

	qla_printk(KERN_INFO, vha->hw, "scsi(%ld:%d:%d): %s RESET ISSUED.\n",
	    vha->host_no, cmd->device->id, cmd->device->lun, name);

	err = 0;
	if (qla2x00_wait_for_hba_online(vha) != QLA_SUCCESS)
		goto eh_reset_failed;
	err = 1;
	if (qla2x00_wait_for_loop_ready(vha) != QLA_SUCCESS)
		goto eh_reset_failed;
	err = 2;
	if (do_reset(fcport, cmd->device->lun, cmd->request->cpu + 1)
		!= QLA_SUCCESS)
		goto eh_reset_failed;
	err = 3;
	if (qla2x00_eh_wait_for_pending_commands(vha, cmd->device->id,
	    cmd->device->lun, type) != QLA_SUCCESS)
		goto eh_reset_failed;

	qla_printk(KERN_INFO, vha->hw, "scsi(%ld:%d:%d): %s RESET SUCCEEDED.\n",
	    vha->host_no, cmd->device->id, cmd->device->lun, name);

	return SUCCESS;

eh_reset_failed:
	qla_printk(KERN_INFO, vha->hw, "scsi(%ld:%d:%d): %s RESET FAILED: %s.\n"
	    , vha->host_no, cmd->device->id, cmd->device->lun, name,
	    reset_errors[err]);
	return FAILED;
}

static int
qla2xxx_eh_device_reset(struct scsi_cmnd *cmd)
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	struct qla_hw_data *ha = vha->hw;

	return __qla2xxx_eh_generic_reset("DEVICE", WAIT_LUN, cmd,
	    ha->isp_ops->lun_reset);
}

static int
qla2xxx_eh_target_reset(struct scsi_cmnd *cmd)
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	struct qla_hw_data *ha = vha->hw;

	return __qla2xxx_eh_generic_reset("TARGET", WAIT_TARGET, cmd,
	    ha->isp_ops->target_reset);
}

/**************************************************************************
* qla2xxx_eh_bus_reset
*
* Description:
*    The bus reset function will reset the bus and abort any executing
*    commands.
*
* Input:
*    cmd = Linux SCSI command packet of the command that cause the
*          bus reset.
*
* Returns:
*    SUCCESS/FAILURE (defined as macro in scsi.h).
*
**************************************************************************/
static int
qla2xxx_eh_bus_reset(struct scsi_cmnd *cmd)
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	fc_port_t *fcport = (struct fc_port *) cmd->device->hostdata;
	int ret = FAILED;
	unsigned int id, lun;

	id = cmd->device->id;
	lun = cmd->device->lun;

	if (!fcport)
		return ret;

	ret = fc_block_scsi_eh(cmd);
	if (ret != 0)
		return ret;
	ret = FAILED;

	qla_printk(KERN_INFO, vha->hw,
	    "scsi(%ld:%d:%d): BUS RESET ISSUED.\n", vha->host_no, id, lun);

	if (qla2x00_wait_for_hba_online(vha) != QLA_SUCCESS) {
		DEBUG2(printk("%s failed:board disabled\n",__func__));
		goto eh_bus_reset_done;
	}

	if (qla2x00_wait_for_loop_ready(vha) == QLA_SUCCESS) {
		if (qla2x00_loop_reset(vha) == QLA_SUCCESS)
			ret = SUCCESS;
	}
	if (ret == FAILED)
		goto eh_bus_reset_done;

	/* Flush outstanding commands. */
	if (qla2x00_eh_wait_for_pending_commands(vha, 0, 0, WAIT_HOST) !=
	    QLA_SUCCESS)
		ret = FAILED;

eh_bus_reset_done:
	qla_printk(KERN_INFO, vha->hw, "%s: reset %s\n", __func__,
	    (ret == FAILED) ? "failed" : "succeeded");

	return ret;
}

/**************************************************************************
* qla2xxx_eh_host_reset
*
* Description:
*    The reset function will reset the Adapter.
*
* Input:
*      cmd = Linux SCSI command packet of the command that cause the
*            adapter reset.
*
* Returns:
*      Either SUCCESS or FAILED.
*
* Note:
**************************************************************************/
static int
qla2xxx_eh_host_reset(struct scsi_cmnd *cmd)
{
	scsi_qla_host_t *vha = shost_priv(cmd->device->host);
	fc_port_t *fcport = (struct fc_port *) cmd->device->hostdata;
	struct qla_hw_data *ha = vha->hw;
	int ret = FAILED;
	unsigned int id, lun;
	scsi_qla_host_t *base_vha = pci_get_drvdata(ha->pdev);

	id = cmd->device->id;
	lun = cmd->device->lun;

	if (!fcport)
		return ret;

	ret = fc_block_scsi_eh(cmd);
	if (ret != 0)
		return ret;
	ret = FAILED;

	qla_printk(KERN_INFO, ha,
	    "scsi(%ld:%d:%d): ADAPTER RESET ISSUED.\n", vha->host_no, id, lun);

	if (qla2x00_wait_for_reset_ready(vha) != QLA_SUCCESS)
		goto eh_host_reset_lock;

	/*
	 * Fixme-may be dpc thread is active and processing
	 * loop_resync,so wait a while for it to
	 * be completed and then issue big hammer.Otherwise
	 * it may cause I/O failure as big hammer marks the
	 * devices as lost kicking of the port_down_timer
	 * while dpc is stuck for the mailbox to complete.
	 */
	qla2x00_wait_for_loop_ready(vha);
	if (vha != base_vha) {
		if (qla2x00_vp_abort_isp(vha))
			goto eh_host_reset_lock;
	} else {
		if (IS_QLA82XX(vha->hw)) {
			if (!qla82xx_fcoe_ctx_reset(vha)) {
				/* Ctx reset success */
				ret = SUCCESS;
				goto eh_host_reset_lock;
			}
			/* fall thru if ctx reset failed */
		}
		if (ha->wq)
			flush_workqueue(ha->wq);

		set_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);
		if (ha->isp_ops->abort_isp(base_vha)) {
			clear_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);
			/* failed. schedule dpc to try */
			set_bit(ISP_ABORT_NEEDED, &base_vha->dpc_flags);

			if (qla2x00_wait_for_hba_online(vha) != QLA_SUCCESS)
				goto eh_host_reset_lock;
		}
		clear_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);
	}

	/* Waiting for command to be returned to OS.*/
	if (qla2x00_eh_wait_for_pending_commands(vha, 0, 0, WAIT_HOST) ==
		QLA_SUCCESS)
		ret = SUCCESS;

eh_host_reset_lock:
	qla_printk(KERN_INFO, ha, "%s: reset %s\n", __func__,
	    (ret == FAILED) ? "failed" : "succeeded");

	return ret;
}

/*
* qla2x00_loop_reset
*      Issue loop reset.
*
* Input:
*      ha = adapter block pointer.
*
* Returns:
*      0 = success
*/
int
qla2x00_loop_reset(scsi_qla_host_t *vha)
{
	int ret;
	struct fc_port *fcport;
	struct qla_hw_data *ha = vha->hw;

	if (ql2xtargetreset == 1 && ha->flags.enable_target_reset) {
		list_for_each_entry(fcport, &vha->vp_fcports, list) {
			if (fcport->port_type != FCT_TARGET)
				continue;

			ret = ha->isp_ops->target_reset(fcport, 0, 0);
			if (ret != QLA_SUCCESS) {
				DEBUG2_3(printk("%s(%ld): bus_reset failed: "
				    "target_reset=%d d_id=%x.\n", __func__,
				    vha->host_no, ret, fcport->d_id.b24));
			}
		}
	}

	if (ha->flags.enable_lip_full_login && !IS_QLA8XXX_TYPE(ha)) {
		ret = qla2x00_full_login_lip(vha);
		if (ret != QLA_SUCCESS) {
			DEBUG2_3(printk("%s(%ld): failed: "
			    "full_login_lip=%d.\n", __func__, vha->host_no,
			    ret));
		}
		atomic_set(&vha->loop_state, LOOP_DOWN);
		atomic_set(&vha->loop_down_timer, LOOP_DOWN_TIME);
		qla2x00_mark_all_devices_lost(vha, 0);
		qla2x00_wait_for_loop_ready(vha);
	}

	if (ha->flags.enable_lip_reset) {
		ret = qla2x00_lip_reset(vha);
		if (ret != QLA_SUCCESS) {
			DEBUG2_3(printk("%s(%ld): failed: "
			    "lip_reset=%d.\n", __func__, vha->host_no, ret));
		} else
			qla2x00_wait_for_loop_ready(vha);
	}

	/* Issue marker command only when we are going to start the I/O */
	vha->marker_needed = 1;

	return QLA_SUCCESS;
}

void
qla2x00_abort_all_cmds(scsi_qla_host_t *vha, int res)
{
	int que, cnt;
	unsigned long flags;
	srb_t *sp;
	struct srb_ctx *ctx;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	for (que = 0; que < ha->max_req_queues; que++) {
		req = ha->req_q_map[que];
		if (!req)
			continue;
		for (cnt = 1; cnt < MAX_OUTSTANDING_COMMANDS; cnt++) {
			sp = req->outstanding_cmds[cnt];
			if (sp) {
				req->outstanding_cmds[cnt] = NULL;
				if (!sp->ctx ||
					(sp->flags & SRB_FCP_CMND_DMA_VALID) ||
					IS_PROT_IO(sp)) {
					sp->cmd->result = res;
					qla2x00_sp_compl(ha, sp);
				} else {
					ctx = sp->ctx;
					if (ctx->type == SRB_LOGIN_CMD ||
					    ctx->type == SRB_LOGOUT_CMD) {
						ctx->u.iocb_cmd->free(sp);
					} else {
						struct fc_bsg_job *bsg_job =
						    ctx->u.bsg_job;
						if (bsg_job->request->msgcode
						    == FC_BSG_HST_CT)
							kfree(sp->fcport);
						bsg_job->req->errors = 0;
						bsg_job->reply->result = res;
						bsg_job->job_done(bsg_job);
						kfree(sp->ctx);
						mempool_free(sp,
							ha->srb_mempool);
					}
				}
			}
		}
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

static int
qla2xxx_slave_alloc(struct scsi_device *sdev)
{
	struct fc_rport *rport = starget_to_rport(scsi_target(sdev));

	if (!rport || fc_remote_port_chkready(rport))
		return -ENXIO;

	sdev->hostdata = *(fc_port_t **)rport->dd_data;

	return 0;
}

static int
qla2xxx_slave_configure(struct scsi_device *sdev)
{
	scsi_qla_host_t *vha = shost_priv(sdev->host);
	struct req_que *req = vha->req;

	if (sdev->tagged_supported)
		scsi_activate_tcq(sdev, req->max_q_depth);
	else
		scsi_deactivate_tcq(sdev, req->max_q_depth);
	return 0;
}

static void
qla2xxx_slave_destroy(struct scsi_device *sdev)
{
	sdev->hostdata = NULL;
}

static void qla2x00_handle_queue_full(struct scsi_device *sdev, int qdepth)
{
	fc_port_t *fcport = (struct fc_port *) sdev->hostdata;

	if (!scsi_track_queue_full(sdev, qdepth))
		return;

	DEBUG2(qla_printk(KERN_INFO, fcport->vha->hw,
		"scsi(%ld:%d:%d:%d): Queue depth adjusted-down to %d.\n",
		fcport->vha->host_no, sdev->channel, sdev->id, sdev->lun,
		sdev->queue_depth));
}

static void qla2x00_adjust_sdev_qdepth_up(struct scsi_device *sdev, int qdepth)
{
	fc_port_t *fcport = sdev->hostdata;
	struct scsi_qla_host *vha = fcport->vha;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req = NULL;

	req = vha->req;
	if (!req)
		return;

	if (req->max_q_depth <= sdev->queue_depth || req->max_q_depth < qdepth)
		return;

	if (sdev->ordered_tags)
		scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, qdepth);
	else
		scsi_adjust_queue_depth(sdev, MSG_SIMPLE_TAG, qdepth);

	DEBUG2(qla_printk(KERN_INFO, ha,
	       "scsi(%ld:%d:%d:%d): Queue depth adjusted-up to %d.\n",
	       fcport->vha->host_no, sdev->channel, sdev->id, sdev->lun,
	       sdev->queue_depth));
}

static int
qla2x00_change_queue_depth(struct scsi_device *sdev, int qdepth, int reason)
{
	switch (reason) {
	case SCSI_QDEPTH_DEFAULT:
		scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), qdepth);
		break;
	case SCSI_QDEPTH_QFULL:
		qla2x00_handle_queue_full(sdev, qdepth);
		break;
	case SCSI_QDEPTH_RAMP_UP:
		qla2x00_adjust_sdev_qdepth_up(sdev, qdepth);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return sdev->queue_depth;
}

static int
qla2x00_change_queue_type(struct scsi_device *sdev, int tag_type)
{
	if (sdev->tagged_supported) {
		scsi_set_tag_type(sdev, tag_type);
		if (tag_type)
			scsi_activate_tcq(sdev, sdev->queue_depth);
		else
			scsi_deactivate_tcq(sdev, sdev->queue_depth);
	} else
		tag_type = 0;

	return tag_type;
}

/**
 * qla2x00_config_dma_addressing() - Configure OS DMA addressing method.
 * @ha: HA context
 *
 * At exit, the @ha's flags.enable_64bit_addressing set to indicated
 * supported addressing method.
 */
static void
qla2x00_config_dma_addressing(struct qla_hw_data *ha)
{
	/* Assume a 32bit DMA mask. */
	ha->flags.enable_64bit_addressing = 0;

	if (!dma_set_mask(&ha->pdev->dev, DMA_BIT_MASK(64))) {
		/* Any upper-dword bits set? */
		if (MSD(dma_get_required_mask(&ha->pdev->dev)) &&
		    !pci_set_consistent_dma_mask(ha->pdev, DMA_BIT_MASK(64))) {
			/* Ok, a 64bit DMA mask is applicable. */
			ha->flags.enable_64bit_addressing = 1;
			ha->isp_ops->calc_req_entries = qla2x00_calc_iocbs_64;
			ha->isp_ops->build_iocbs = qla2x00_build_scsi_iocbs_64;
			return;
		}
	}

	dma_set_mask(&ha->pdev->dev, DMA_BIT_MASK(32));
	pci_set_consistent_dma_mask(ha->pdev, DMA_BIT_MASK(32));
}

static void
qla2x00_enable_intrs(struct qla_hw_data *ha)
{
	unsigned long flags = 0;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	ha->interrupts_on = 1;
	/* enable risc and host interrupts */
	WRT_REG_WORD(&reg->ictrl, ICR_EN_INT | ICR_EN_RISC);
	RD_REG_WORD(&reg->ictrl);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

}

static void
qla2x00_disable_intrs(struct qla_hw_data *ha)
{
	unsigned long flags = 0;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	ha->interrupts_on = 0;
	/* disable risc and host interrupts */
	WRT_REG_WORD(&reg->ictrl, 0);
	RD_REG_WORD(&reg->ictrl);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

static void
qla24xx_enable_intrs(struct qla_hw_data *ha)
{
	unsigned long flags = 0;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	ha->interrupts_on = 1;
	WRT_REG_DWORD(&reg->ictrl, ICRX_EN_RISC_INT);
	RD_REG_DWORD(&reg->ictrl);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

static void
qla24xx_disable_intrs(struct qla_hw_data *ha)
{
	unsigned long flags = 0;
	struct device_reg_24xx __iomem *reg = &ha->iobase->isp24;

	if (IS_NOPOLLING_TYPE(ha))
		return;
	spin_lock_irqsave(&ha->hardware_lock, flags);
	ha->interrupts_on = 0;
	WRT_REG_DWORD(&reg->ictrl, 0);
	RD_REG_DWORD(&reg->ictrl);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

static struct isp_operations qla2100_isp_ops = {
	.pci_config		= qla2100_pci_config,
	.reset_chip		= qla2x00_reset_chip,
	.chip_diag		= qla2x00_chip_diag,
	.config_rings		= qla2x00_config_rings,
	.reset_adapter		= qla2x00_reset_adapter,
	.nvram_config		= qla2x00_nvram_config,
	.update_fw_options	= qla2x00_update_fw_options,
	.load_risc		= qla2x00_load_risc,
	.pci_info_str		= qla2x00_pci_info_str,
	.fw_version_str		= qla2x00_fw_version_str,
	.intr_handler		= qla2100_intr_handler,
	.enable_intrs		= qla2x00_enable_intrs,
	.disable_intrs		= qla2x00_disable_intrs,
	.abort_command		= qla2x00_abort_command,
	.target_reset		= qla2x00_abort_target,
	.lun_reset		= qla2x00_lun_reset,
	.fabric_login		= qla2x00_login_fabric,
	.fabric_logout		= qla2x00_fabric_logout,
	.calc_req_entries	= qla2x00_calc_iocbs_32,
	.build_iocbs		= qla2x00_build_scsi_iocbs_32,
	.prep_ms_iocb		= qla2x00_prep_ms_iocb,
	.prep_ms_fdmi_iocb	= qla2x00_prep_ms_fdmi_iocb,
	.read_nvram		= qla2x00_read_nvram_data,
	.write_nvram		= qla2x00_write_nvram_data,
	.fw_dump		= qla2100_fw_dump,
	.beacon_on		= NULL,
	.beacon_off		= NULL,
	.beacon_blink		= NULL,
	.read_optrom		= qla2x00_read_optrom_data,
	.write_optrom		= qla2x00_write_optrom_data,
	.get_flash_version	= qla2x00_get_flash_version,
	.start_scsi		= qla2x00_start_scsi,
	.abort_isp		= qla2x00_abort_isp,
};

static struct isp_operations qla2300_isp_ops = {
	.pci_config		= qla2300_pci_config,
	.reset_chip		= qla2x00_reset_chip,
	.chip_diag		= qla2x00_chip_diag,
	.config_rings		= qla2x00_config_rings,
	.reset_adapter		= qla2x00_reset_adapter,
	.nvram_config		= qla2x00_nvram_config,
	.update_fw_options	= qla2x00_update_fw_options,
	.load_risc		= qla2x00_load_risc,
	.pci_info_str		= qla2x00_pci_info_str,
	.fw_version_str		= qla2x00_fw_version_str,
	.intr_handler		= qla2300_intr_handler,
	.enable_intrs		= qla2x00_enable_intrs,
	.disable_intrs		= qla2x00_disable_intrs,
	.abort_command		= qla2x00_abort_command,
	.target_reset		= qla2x00_abort_target,
	.lun_reset		= qla2x00_lun_reset,
	.fabric_login		= qla2x00_login_fabric,
	.fabric_logout		= qla2x00_fabric_logout,
	.calc_req_entries	= qla2x00_calc_iocbs_32,
	.build_iocbs		= qla2x00_build_scsi_iocbs_32,
	.prep_ms_iocb		= qla2x00_prep_ms_iocb,
	.prep_ms_fdmi_iocb	= qla2x00_prep_ms_fdmi_iocb,
	.read_nvram		= qla2x00_read_nvram_data,
	.write_nvram		= qla2x00_write_nvram_data,
	.fw_dump		= qla2300_fw_dump,
	.beacon_on		= qla2x00_beacon_on,
	.beacon_off		= qla2x00_beacon_off,
	.beacon_blink		= qla2x00_beacon_blink,
	.read_optrom		= qla2x00_read_optrom_data,
	.write_optrom		= qla2x00_write_optrom_data,
	.get_flash_version	= qla2x00_get_flash_version,
	.start_scsi		= qla2x00_start_scsi,
	.abort_isp		= qla2x00_abort_isp,
};

static struct isp_operations qla24xx_isp_ops = {
	.pci_config		= qla24xx_pci_config,
	.reset_chip		= qla24xx_reset_chip,
	.chip_diag		= qla24xx_chip_diag,
	.config_rings		= qla24xx_config_rings,
	.reset_adapter		= qla24xx_reset_adapter,
	.nvram_config		= qla24xx_nvram_config,
	.update_fw_options	= qla24xx_update_fw_options,
	.load_risc		= qla24xx_load_risc,
	.pci_info_str		= qla24xx_pci_info_str,
	.fw_version_str		= qla24xx_fw_version_str,
	.intr_handler		= qla24xx_intr_handler,
	.enable_intrs		= qla24xx_enable_intrs,
	.disable_intrs		= qla24xx_disable_intrs,
	.abort_command		= qla24xx_abort_command,
	.target_reset		= qla24xx_abort_target,
	.lun_reset		= qla24xx_lun_reset,
	.fabric_login		= qla24xx_login_fabric,
	.fabric_logout		= qla24xx_fabric_logout,
	.calc_req_entries	= NULL,
	.build_iocbs		= NULL,
	.prep_ms_iocb		= qla24xx_prep_ms_iocb,
	.prep_ms_fdmi_iocb	= qla24xx_prep_ms_fdmi_iocb,
	.read_nvram		= qla24xx_read_nvram_data,
	.write_nvram		= qla24xx_write_nvram_data,
	.fw_dump		= qla24xx_fw_dump,
	.beacon_on		= qla24xx_beacon_on,
	.beacon_off		= qla24xx_beacon_off,
	.beacon_blink		= qla24xx_beacon_blink,
	.read_optrom		= qla24xx_read_optrom_data,
	.write_optrom		= qla24xx_write_optrom_data,
	.get_flash_version	= qla24xx_get_flash_version,
	.start_scsi		= qla24xx_start_scsi,
	.abort_isp		= qla2x00_abort_isp,
};

static struct isp_operations qla25xx_isp_ops = {
	.pci_config		= qla25xx_pci_config,
	.reset_chip		= qla24xx_reset_chip,
	.chip_diag		= qla24xx_chip_diag,
	.config_rings		= qla24xx_config_rings,
	.reset_adapter		= qla24xx_reset_adapter,
	.nvram_config		= qla24xx_nvram_config,
	.update_fw_options	= qla24xx_update_fw_options,
	.load_risc		= qla24xx_load_risc,
	.pci_info_str		= qla24xx_pci_info_str,
	.fw_version_str		= qla24xx_fw_version_str,
	.intr_handler		= qla24xx_intr_handler,
	.enable_intrs		= qla24xx_enable_intrs,
	.disable_intrs		= qla24xx_disable_intrs,
	.abort_command		= qla24xx_abort_command,
	.target_reset		= qla24xx_abort_target,
	.lun_reset		= qla24xx_lun_reset,
	.fabric_login		= qla24xx_login_fabric,
	.fabric_logout		= qla24xx_fabric_logout,
	.calc_req_entries	= NULL,
	.build_iocbs		= NULL,
	.prep_ms_iocb		= qla24xx_prep_ms_iocb,
	.prep_ms_fdmi_iocb	= qla24xx_prep_ms_fdmi_iocb,
	.read_nvram		= qla25xx_read_nvram_data,
	.write_nvram		= qla25xx_write_nvram_data,
	.fw_dump		= qla25xx_fw_dump,
	.beacon_on		= qla24xx_beacon_on,
	.beacon_off		= qla24xx_beacon_off,
	.beacon_blink		= qla24xx_beacon_blink,
	.read_optrom		= qla25xx_read_optrom_data,
	.write_optrom		= qla24xx_write_optrom_data,
	.get_flash_version	= qla24xx_get_flash_version,
	.start_scsi		= qla24xx_dif_start_scsi,
	.abort_isp		= qla2x00_abort_isp,
};

static struct isp_operations qla81xx_isp_ops = {
	.pci_config		= qla25xx_pci_config,
	.reset_chip		= qla24xx_reset_chip,
	.chip_diag		= qla24xx_chip_diag,
	.config_rings		= qla24xx_config_rings,
	.reset_adapter		= qla24xx_reset_adapter,
	.nvram_config		= qla81xx_nvram_config,
	.update_fw_options	= qla81xx_update_fw_options,
	.load_risc		= qla81xx_load_risc,
	.pci_info_str		= qla24xx_pci_info_str,
	.fw_version_str		= qla24xx_fw_version_str,
	.intr_handler		= qla24xx_intr_handler,
	.enable_intrs		= qla24xx_enable_intrs,
	.disable_intrs		= qla24xx_disable_intrs,
	.abort_command		= qla24xx_abort_command,
	.target_reset		= qla24xx_abort_target,
	.lun_reset		= qla24xx_lun_reset,
	.fabric_login		= qla24xx_login_fabric,
	.fabric_logout		= qla24xx_fabric_logout,
	.calc_req_entries	= NULL,
	.build_iocbs		= NULL,
	.prep_ms_iocb		= qla24xx_prep_ms_iocb,
	.prep_ms_fdmi_iocb	= qla24xx_prep_ms_fdmi_iocb,
	.read_nvram		= NULL,
	.write_nvram		= NULL,
	.fw_dump		= qla81xx_fw_dump,
	.beacon_on		= qla24xx_beacon_on,
	.beacon_off		= qla24xx_beacon_off,
	.beacon_blink		= qla24xx_beacon_blink,
	.read_optrom		= qla25xx_read_optrom_data,
	.write_optrom		= qla24xx_write_optrom_data,
	.get_flash_version	= qla24xx_get_flash_version,
	.start_scsi		= qla24xx_dif_start_scsi,
	.abort_isp		= qla2x00_abort_isp,
};

static struct isp_operations qla82xx_isp_ops = {
	.pci_config		= qla82xx_pci_config,
	.reset_chip		= qla82xx_reset_chip,
	.chip_diag		= qla24xx_chip_diag,
	.config_rings		= qla82xx_config_rings,
	.reset_adapter		= qla24xx_reset_adapter,
	.nvram_config		= qla81xx_nvram_config,
	.update_fw_options	= qla24xx_update_fw_options,
	.load_risc		= qla82xx_load_risc,
	.pci_info_str		= qla82xx_pci_info_str,
	.fw_version_str		= qla24xx_fw_version_str,
	.intr_handler		= qla82xx_intr_handler,
	.enable_intrs		= qla82xx_enable_intrs,
	.disable_intrs		= qla82xx_disable_intrs,
	.abort_command		= qla24xx_abort_command,
	.target_reset		= qla24xx_abort_target,
	.lun_reset		= qla24xx_lun_reset,
	.fabric_login		= qla24xx_login_fabric,
	.fabric_logout		= qla24xx_fabric_logout,
	.calc_req_entries	= NULL,
	.build_iocbs		= NULL,
	.prep_ms_iocb		= qla24xx_prep_ms_iocb,
	.prep_ms_fdmi_iocb	= qla24xx_prep_ms_fdmi_iocb,
	.read_nvram		= qla24xx_read_nvram_data,
	.write_nvram		= qla24xx_write_nvram_data,
	.fw_dump		= qla24xx_fw_dump,
	.beacon_on		= qla24xx_beacon_on,
	.beacon_off		= qla24xx_beacon_off,
	.beacon_blink		= qla24xx_beacon_blink,
	.read_optrom		= qla82xx_read_optrom_data,
	.write_optrom		= qla82xx_write_optrom_data,
	.get_flash_version	= qla24xx_get_flash_version,
	.start_scsi             = qla82xx_start_scsi,
	.abort_isp		= qla82xx_abort_isp,
};

static inline void
qla2x00_set_isp_flags(struct qla_hw_data *ha)
{
	ha->device_type = DT_EXTENDED_IDS;
	switch (ha->pdev->device) {
	case PCI_DEVICE_ID_QLOGIC_ISP2100:
		ha->device_type |= DT_ISP2100;
		ha->device_type &= ~DT_EXTENDED_IDS;
		ha->fw_srisc_address = RISC_START_ADDRESS_2100;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2200:
		ha->device_type |= DT_ISP2200;
		ha->device_type &= ~DT_EXTENDED_IDS;
		ha->fw_srisc_address = RISC_START_ADDRESS_2100;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2300:
		ha->device_type |= DT_ISP2300;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->fw_srisc_address = RISC_START_ADDRESS_2300;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2312:
		ha->device_type |= DT_ISP2312;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->fw_srisc_address = RISC_START_ADDRESS_2300;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2322:
		ha->device_type |= DT_ISP2322;
		ha->device_type |= DT_ZIO_SUPPORTED;
		if (ha->pdev->subsystem_vendor == 0x1028 &&
		    ha->pdev->subsystem_device == 0x0170)
			ha->device_type |= DT_OEM_001;
		ha->fw_srisc_address = RISC_START_ADDRESS_2300;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP6312:
		ha->device_type |= DT_ISP6312;
		ha->fw_srisc_address = RISC_START_ADDRESS_2300;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP6322:
		ha->device_type |= DT_ISP6322;
		ha->fw_srisc_address = RISC_START_ADDRESS_2300;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2422:
		ha->device_type |= DT_ISP2422;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->device_type |= DT_FWI2;
		ha->device_type |= DT_IIDMA;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2432:
		ha->device_type |= DT_ISP2432;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->device_type |= DT_FWI2;
		ha->device_type |= DT_IIDMA;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP8432:
		ha->device_type |= DT_ISP8432;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->device_type |= DT_FWI2;
		ha->device_type |= DT_IIDMA;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP5422:
		ha->device_type |= DT_ISP5422;
		ha->device_type |= DT_FWI2;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP5432:
		ha->device_type |= DT_ISP5432;
		ha->device_type |= DT_FWI2;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP2532:
		ha->device_type |= DT_ISP2532;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->device_type |= DT_FWI2;
		ha->device_type |= DT_IIDMA;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP8001:
		ha->device_type |= DT_ISP8001;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->device_type |= DT_FWI2;
		ha->device_type |= DT_IIDMA;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		break;
	case PCI_DEVICE_ID_QLOGIC_ISP8021:
		ha->device_type |= DT_ISP8021;
		ha->device_type |= DT_ZIO_SUPPORTED;
		ha->device_type |= DT_FWI2;
		ha->fw_srisc_address = RISC_START_ADDRESS_2400;
		/* Initialize 82XX ISP flags */
		qla82xx_init_flags(ha);
		break;
	}

	if (IS_QLA82XX(ha))
		ha->port_no = !(ha->portnum & 1);
	else
		/* Get adapter physical port no from interrupt pin register. */
		pci_read_config_byte(ha->pdev, PCI_INTERRUPT_PIN, &ha->port_no);

	if (ha->port_no & 1)
		ha->flags.port0 = 1;
	else
		ha->flags.port0 = 0;
}

static int
qla2x00_iospace_config(struct qla_hw_data *ha)
{
	resource_size_t pio;
	uint16_t msix;
	int cpus;

	if (IS_QLA82XX(ha))
		return qla82xx_iospace_config(ha);

	if (pci_request_selected_regions(ha->pdev, ha->bars,
	    QLA2XXX_DRIVER_NAME)) {
		qla_printk(KERN_WARNING, ha,
		    "Failed to reserve PIO/MMIO regions (%s)\n",
		    pci_name(ha->pdev));

		goto iospace_error_exit;
	}
	if (!(ha->bars & 1))
		goto skip_pio;

	/* We only need PIO for Flash operations on ISP2312 v2 chips. */
	pio = pci_resource_start(ha->pdev, 0);
	if (pci_resource_flags(ha->pdev, 0) & IORESOURCE_IO) {
		if (pci_resource_len(ha->pdev, 0) < MIN_IOBASE_LEN) {
			qla_printk(KERN_WARNING, ha,
			    "Invalid PCI I/O region size (%s)...\n",
				pci_name(ha->pdev));
			pio = 0;
		}
	} else {
		qla_printk(KERN_WARNING, ha,
		    "region #0 not a PIO resource (%s)...\n",
		    pci_name(ha->pdev));
		pio = 0;
	}
	ha->pio_address = pio;

skip_pio:
	/* Use MMIO operations for all accesses. */
	if (!(pci_resource_flags(ha->pdev, 1) & IORESOURCE_MEM)) {
		qla_printk(KERN_ERR, ha,
		    "region #1 not an MMIO resource (%s), aborting\n",
		    pci_name(ha->pdev));
		goto iospace_error_exit;
	}
	if (pci_resource_len(ha->pdev, 1) < MIN_IOBASE_LEN) {
		qla_printk(KERN_ERR, ha,
		    "Invalid PCI mem region size (%s), aborting\n",
			pci_name(ha->pdev));
		goto iospace_error_exit;
	}

	ha->iobase = ioremap(pci_resource_start(ha->pdev, 1), MIN_IOBASE_LEN);
	if (!ha->iobase) {
		qla_printk(KERN_ERR, ha,
		    "cannot remap MMIO (%s), aborting\n", pci_name(ha->pdev));

		goto iospace_error_exit;
	}

	/* Determine queue resources */
	ha->max_req_queues = ha->max_rsp_queues = 1;
	if ((ql2xmaxqueues <= 1 && !ql2xmultique_tag) ||
		(ql2xmaxqueues > 1 && ql2xmultique_tag) ||
		(!IS_QLA25XX(ha) && !IS_QLA81XX(ha)))
		goto mqiobase_exit;

	ha->mqiobase = ioremap(pci_resource_start(ha->pdev, 3),
			pci_resource_len(ha->pdev, 3));
	if (ha->mqiobase) {
		/* Read MSIX vector size of the board */
		pci_read_config_word(ha->pdev, QLA_PCI_MSIX_CONTROL, &msix);
		ha->msix_count = msix;
		/* Max queues are bounded by available msix vectors */
		/* queue 0 uses two msix vectors */
		if (ql2xmultique_tag) {
			cpus = num_online_cpus();
			ha->max_rsp_queues = (ha->msix_count - 1 > cpus) ?
				(cpus + 1) : (ha->msix_count - 1);
			ha->max_req_queues = 2;
		} else if (ql2xmaxqueues > 1) {
			ha->max_req_queues = ql2xmaxqueues > QLA_MQ_SIZE ?
						QLA_MQ_SIZE : ql2xmaxqueues;
			DEBUG2(qla_printk(KERN_INFO, ha, "QoS mode set, max no"
			" of request queues:%d\n", ha->max_req_queues));
		}
		qla_printk(KERN_INFO, ha,
			"MSI-X vector count: %d\n", msix);
	} else
		qla_printk(KERN_INFO, ha, "BAR 3 not enabled\n");

mqiobase_exit:
	ha->msix_count = ha->max_rsp_queues + 1;
	return (0);

iospace_error_exit:
	return (-ENOMEM);
}

static void
qla2xxx_scan_start(struct Scsi_Host *shost)
{
	scsi_qla_host_t *vha = shost_priv(shost);

	if (vha->hw->flags.running_gold_fw)
		return;

	set_bit(LOOP_RESYNC_NEEDED, &vha->dpc_flags);
	set_bit(LOCAL_LOOP_UPDATE, &vha->dpc_flags);
	set_bit(RSCN_UPDATE, &vha->dpc_flags);
	set_bit(NPIV_CONFIG_NEEDED, &vha->dpc_flags);
}

static int
qla2xxx_scan_finished(struct Scsi_Host *shost, unsigned long time)
{
	scsi_qla_host_t *vha = shost_priv(shost);

	if (!vha->host)
		return 1;
	if (time > vha->hw->loop_reset_delay * HZ)
		return 1;

	return atomic_read(&vha->loop_state) == LOOP_READY;
}

/*
 * PCI driver interface
 */
static int __devinit
qla2x00_probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int	ret = -ENODEV;
	struct Scsi_Host *host;
	scsi_qla_host_t *base_vha = NULL;
	struct qla_hw_data *ha;
	char pci_info[30];
	char fw_str[30];
	struct scsi_host_template *sht;
	int bars, max_id, mem_only = 0;
	uint16_t req_length = 0, rsp_length = 0;
	struct req_que *req = NULL;
	struct rsp_que *rsp = NULL;

	bars = pci_select_bars(pdev, IORESOURCE_MEM | IORESOURCE_IO);
	sht = &qla2xxx_driver_template;
	if (pdev->device == PCI_DEVICE_ID_QLOGIC_ISP2422 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP2432 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP8432 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP5422 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP5432 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP2532 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP8001 ||
	    pdev->device == PCI_DEVICE_ID_QLOGIC_ISP8021) {
		bars = pci_select_bars(pdev, IORESOURCE_MEM);
		mem_only = 1;
	}

	if (mem_only) {
		if (pci_enable_device_mem(pdev))
			goto probe_out;
	} else {
		if (pci_enable_device(pdev))
			goto probe_out;
	}

	/* This may fail but that's ok */
	pci_enable_pcie_error_reporting(pdev);

	ha = kzalloc(sizeof(struct qla_hw_data), GFP_KERNEL);
	if (!ha) {
		DEBUG(printk("Unable to allocate memory for ha\n"));
		goto probe_out;
	}
	ha->pdev = pdev;

	/* Clear our data area */
	ha->bars = bars;
	ha->mem_only = mem_only;
	spin_lock_init(&ha->hardware_lock);
	spin_lock_init(&ha->vport_slock);

	/* Set ISP-type information. */
	qla2x00_set_isp_flags(ha);

	/* Set EEH reset type to fundamental if required by hba */
	if ( IS_QLA24XX(ha) || IS_QLA25XX(ha) || IS_QLA81XX(ha)) {
		pdev->needs_freset = 1;
	}

	/* Configure PCI I/O space */
	ret = qla2x00_iospace_config(ha);
	if (ret)
		goto probe_hw_failed;

	qla_printk(KERN_INFO, ha,
	    "Found an ISP%04X, irq %d, iobase 0x%p\n", pdev->device, pdev->irq,
	    ha->iobase);

	ha->prev_topology = 0;
	ha->init_cb_size = sizeof(init_cb_t);
	ha->link_data_rate = PORT_SPEED_UNKNOWN;
	ha->optrom_size = OPTROM_SIZE_2300;

	/* Assign ISP specific operations. */
	max_id = MAX_TARGETS_2200;
	if (IS_QLA2100(ha)) {
		max_id = MAX_TARGETS_2100;
		ha->mbx_count = MAILBOX_REGISTER_COUNT_2100;
		req_length = REQUEST_ENTRY_CNT_2100;
		rsp_length = RESPONSE_ENTRY_CNT_2100;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2100;
		ha->gid_list_info_size = 4;
		ha->flash_conf_off = ~0;
		ha->flash_data_off = ~0;
		ha->nvram_conf_off = ~0;
		ha->nvram_data_off = ~0;
		ha->isp_ops = &qla2100_isp_ops;
	} else if (IS_QLA2200(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT;
		req_length = REQUEST_ENTRY_CNT_2200;
		rsp_length = RESPONSE_ENTRY_CNT_2100;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2100;
		ha->gid_list_info_size = 4;
		ha->flash_conf_off = ~0;
		ha->flash_data_off = ~0;
		ha->nvram_conf_off = ~0;
		ha->nvram_data_off = ~0;
		ha->isp_ops = &qla2100_isp_ops;
	} else if (IS_QLA23XX(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT;
		req_length = REQUEST_ENTRY_CNT_2200;
		rsp_length = RESPONSE_ENTRY_CNT_2300;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2300;
		ha->gid_list_info_size = 6;
		if (IS_QLA2322(ha) || IS_QLA6322(ha))
			ha->optrom_size = OPTROM_SIZE_2322;
		ha->flash_conf_off = ~0;
		ha->flash_data_off = ~0;
		ha->nvram_conf_off = ~0;
		ha->nvram_data_off = ~0;
		ha->isp_ops = &qla2300_isp_ops;
	} else if (IS_QLA24XX_TYPE(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT;
		req_length = REQUEST_ENTRY_CNT_24XX;
		rsp_length = RESPONSE_ENTRY_CNT_2300;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2300;
		ha->init_cb_size = sizeof(struct mid_init_cb_24xx);
		ha->gid_list_info_size = 8;
		ha->optrom_size = OPTROM_SIZE_24XX;
		ha->nvram_npiv_size = QLA_MAX_VPORTS_QLA24XX;
		ha->isp_ops = &qla24xx_isp_ops;
		ha->flash_conf_off = FARX_ACCESS_FLASH_CONF;
		ha->flash_data_off = FARX_ACCESS_FLASH_DATA;
		ha->nvram_conf_off = FARX_ACCESS_NVRAM_CONF;
		ha->nvram_data_off = FARX_ACCESS_NVRAM_DATA;
	} else if (IS_QLA25XX(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT;
		req_length = REQUEST_ENTRY_CNT_24XX;
		rsp_length = RESPONSE_ENTRY_CNT_2300;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2300;
		ha->init_cb_size = sizeof(struct mid_init_cb_24xx);
		ha->gid_list_info_size = 8;
		ha->optrom_size = OPTROM_SIZE_25XX;
		ha->nvram_npiv_size = QLA_MAX_VPORTS_QLA25XX;
		ha->isp_ops = &qla25xx_isp_ops;
		ha->flash_conf_off = FARX_ACCESS_FLASH_CONF;
		ha->flash_data_off = FARX_ACCESS_FLASH_DATA;
		ha->nvram_conf_off = FARX_ACCESS_NVRAM_CONF;
		ha->nvram_data_off = FARX_ACCESS_NVRAM_DATA;
	} else if (IS_QLA81XX(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT;
		req_length = REQUEST_ENTRY_CNT_24XX;
		rsp_length = RESPONSE_ENTRY_CNT_2300;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2300;
		ha->init_cb_size = sizeof(struct mid_init_cb_81xx);
		ha->gid_list_info_size = 8;
		ha->optrom_size = OPTROM_SIZE_81XX;
		ha->nvram_npiv_size = QLA_MAX_VPORTS_QLA25XX;
		ha->isp_ops = &qla81xx_isp_ops;
		ha->flash_conf_off = FARX_ACCESS_FLASH_CONF_81XX;
		ha->flash_data_off = FARX_ACCESS_FLASH_DATA_81XX;
		ha->nvram_conf_off = ~0;
		ha->nvram_data_off = ~0;
	} else if (IS_QLA82XX(ha)) {
		ha->mbx_count = MAILBOX_REGISTER_COUNT;
		req_length = REQUEST_ENTRY_CNT_82XX;
		rsp_length = RESPONSE_ENTRY_CNT_82XX;
		ha->max_loop_id = SNS_LAST_LOOP_ID_2300;
		ha->init_cb_size = sizeof(struct mid_init_cb_81xx);
		ha->gid_list_info_size = 8;
		ha->optrom_size = OPTROM_SIZE_82XX;
		ha->nvram_npiv_size = QLA_MAX_VPORTS_QLA25XX;
		ha->isp_ops = &qla82xx_isp_ops;
		ha->flash_conf_off = FARX_ACCESS_FLASH_CONF;
		ha->flash_data_off = FARX_ACCESS_FLASH_DATA;
		ha->nvram_conf_off = FARX_ACCESS_NVRAM_CONF;
		ha->nvram_data_off = FARX_ACCESS_NVRAM_DATA;
	}

	mutex_init(&ha->vport_lock);
	init_completion(&ha->mbx_cmd_comp);
	complete(&ha->mbx_cmd_comp);
	init_completion(&ha->mbx_intr_comp);
	init_completion(&ha->dcbx_comp);

	set_bit(0, (unsigned long *) ha->vp_idx_map);

	qla2x00_config_dma_addressing(ha);
	ret = qla2x00_mem_alloc(ha, req_length, rsp_length, &req, &rsp);
	if (!ret) {
		qla_printk(KERN_WARNING, ha,
		    "[ERROR] Failed to allocate memory for adapter\n");

		goto probe_hw_failed;
	}

	req->max_q_depth = MAX_Q_DEPTH;
	if (ql2xmaxqdepth != 0 && ql2xmaxqdepth <= 0xffffU)
		req->max_q_depth = ql2xmaxqdepth;


	base_vha = qla2x00_create_host(sht, ha);
	if (!base_vha) {
		qla_printk(KERN_WARNING, ha,
		    "[ERROR] Failed to allocate memory for scsi_host\n");

		ret = -ENOMEM;
		qla2x00_mem_free(ha);
		qla2x00_free_req_que(ha, req);
		qla2x00_free_rsp_que(ha, rsp);
		goto probe_hw_failed;
	}

	pci_set_drvdata(pdev, base_vha);

	host = base_vha->host;
	base_vha->req = req;
	host->can_queue = req->length + 128;
	if (IS_QLA2XXX_MIDTYPE(ha))
		base_vha->mgmt_svr_loop_id = 10 + base_vha->vp_idx;
	else
		base_vha->mgmt_svr_loop_id = MANAGEMENT_SERVER +
						base_vha->vp_idx;

	/* Set the SG table size based on ISP type */
	if (!IS_FWI2_CAPABLE(ha)) {
		if (IS_QLA2100(ha))
			host->sg_tablesize = 32;
	} else {
		if (!IS_QLA82XX(ha))
			host->sg_tablesize = QLA_SG_ALL;
	}

	host->max_id = max_id;
	host->this_id = 255;
	host->cmd_per_lun = 3;
	host->unique_id = host->host_no;
	if ((IS_QLA25XX(ha) || IS_QLA81XX(ha)) && ql2xenabledif)
		host->max_cmd_len = 32;
	else
		host->max_cmd_len = MAX_CMDSZ;
	host->max_channel = MAX_BUSES - 1;
	host->max_lun = MAX_LUNS;
	host->transportt = qla2xxx_transport_template;
	sht->vendor_id = (SCSI_NL_VID_TYPE_PCI | PCI_VENDOR_ID_QLOGIC);

	/* Set up the irqs */
	ret = qla2x00_request_irqs(ha, rsp);
	if (ret)
		goto probe_init_failed;

	pci_save_state(pdev);

	/* Alloc arrays of request and response ring ptrs */
que_init:
	if (!qla2x00_alloc_queues(ha)) {
		qla_printk(KERN_WARNING, ha,
		"[ERROR] Failed to allocate memory for queue"
		" pointers\n");
		goto probe_init_failed;
	}

	ha->rsp_q_map[0] = rsp;
	ha->req_q_map[0] = req;
	rsp->req = req;
	req->rsp = rsp;
	set_bit(0, ha->req_qid_map);
	set_bit(0, ha->rsp_qid_map);
	/* FWI2-capable only. */
	req->req_q_in = &ha->iobase->isp24.req_q_in;
	req->req_q_out = &ha->iobase->isp24.req_q_out;
	rsp->rsp_q_in = &ha->iobase->isp24.rsp_q_in;
	rsp->rsp_q_out = &ha->iobase->isp24.rsp_q_out;
	if (ha->mqenable) {
		req->req_q_in = &ha->mqiobase->isp25mq.req_q_in;
		req->req_q_out = &ha->mqiobase->isp25mq.req_q_out;
		rsp->rsp_q_in = &ha->mqiobase->isp25mq.rsp_q_in;
		rsp->rsp_q_out =  &ha->mqiobase->isp25mq.rsp_q_out;
	}

	if (IS_QLA82XX(ha)) {
		req->req_q_out = &ha->iobase->isp82.req_q_out[0];
		rsp->rsp_q_in = &ha->iobase->isp82.rsp_q_in[0];
		rsp->rsp_q_out = &ha->iobase->isp82.rsp_q_out[0];
	}

	if (qla2x00_initialize_adapter(base_vha)) {
		qla_printk(KERN_WARNING, ha,
		    "Failed to initialize adapter\n");

		DEBUG2(printk("scsi(%ld): Failed to initialize adapter - "
		    "Adapter flags %x.\n",
		    base_vha->host_no, base_vha->device_flags));

		if (IS_QLA82XX(ha)) {
			qla82xx_idc_lock(ha);
			qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE,
				QLA82XX_DEV_FAILED);
			qla82xx_idc_unlock(ha);
			qla_printk(KERN_INFO, ha, "HW State: FAILED\n");
		}

		ret = -ENODEV;
		goto probe_failed;
	}

	if (ha->mqenable) {
		if (qla25xx_setup_mode(base_vha)) {
			qla_printk(KERN_WARNING, ha,
				"Can't create queues, falling back to single"
				" queue mode\n");
			goto que_init;
		}
	}

	if (ha->flags.running_gold_fw)
		goto skip_dpc;

	/*
	 * Startup the kernel thread for this host adapter
	 */
	ha->dpc_thread = kthread_create(qla2x00_do_dpc, ha,
			"%s_dpc", base_vha->host_str);
	if (IS_ERR(ha->dpc_thread)) {
		qla_printk(KERN_WARNING, ha,
		    "Unable to start DPC thread!\n");
		ret = PTR_ERR(ha->dpc_thread);
		goto probe_failed;
	}

skip_dpc:
	list_add_tail(&base_vha->list, &ha->vp_list);
	base_vha->host->irq = ha->pdev->irq;

	/* Initialized the timer */
	qla2x00_start_timer(base_vha, qla2x00_timer, WATCH_INTERVAL);

	DEBUG2(printk("DEBUG: detect hba %ld at address = %p\n",
	    base_vha->host_no, ha));

	if ((IS_QLA25XX(ha) || IS_QLA81XX(ha)) && ql2xenabledif) {
		if (ha->fw_attributes & BIT_4) {
			base_vha->flags.difdix_supported = 1;
			DEBUG18(qla_printk(KERN_INFO, ha,
			    "Registering for DIF/DIX type 1 and 3"
			    " protection.\n"));
			scsi_host_set_prot(host,
			    SHOST_DIF_TYPE1_PROTECTION
			    | SHOST_DIF_TYPE2_PROTECTION
			    | SHOST_DIF_TYPE3_PROTECTION
			    | SHOST_DIX_TYPE1_PROTECTION
			    | SHOST_DIX_TYPE2_PROTECTION
			    | SHOST_DIX_TYPE3_PROTECTION);
			scsi_host_set_guard(host, SHOST_DIX_GUARD_CRC);
		} else
			base_vha->flags.difdix_supported = 0;
	}

	ha->isp_ops->enable_intrs(ha);

	ret = scsi_add_host(host, &pdev->dev);
	if (ret)
		goto probe_failed;

	base_vha->flags.init_done = 1;
	base_vha->flags.online = 1;

	scsi_scan_host(host);

	qla2x00_alloc_sysfs_attr(base_vha);

	qla2x00_init_host_attr(base_vha);

	qla2x00_dfs_setup(base_vha);

	qla_printk(KERN_INFO, ha, "\n"
	    " QLogic Fibre Channel HBA Driver: %s\n"
	    "  QLogic %s - %s\n"
	    "  ISP%04X: %s @ %s hdma%c, host#=%ld, fw=%s\n",
	    qla2x00_version_str, ha->model_number,
	    ha->model_desc ? ha->model_desc : "", pdev->device,
	    ha->isp_ops->pci_info_str(base_vha, pci_info), pci_name(pdev),
	    ha->flags.enable_64bit_addressing ? '+' : '-', base_vha->host_no,
	    ha->isp_ops->fw_version_str(base_vha, fw_str));

	return 0;

probe_init_failed:
	qla2x00_free_req_que(ha, req);
	qla2x00_free_rsp_que(ha, rsp);
	ha->max_req_queues = ha->max_rsp_queues = 0;

probe_failed:
	if (base_vha->timer_active)
		qla2x00_stop_timer(base_vha);
	base_vha->flags.online = 0;
	if (ha->dpc_thread) {
		struct task_struct *t = ha->dpc_thread;

		ha->dpc_thread = NULL;
		kthread_stop(t);
	}

	qla2x00_free_device(base_vha);

	scsi_host_put(base_vha->host);

probe_hw_failed:
	if (IS_QLA82XX(ha)) {
		qla82xx_idc_lock(ha);
		qla82xx_clear_drv_active(ha);
		qla82xx_idc_unlock(ha);
		iounmap((device_reg_t __iomem *)ha->nx_pcibase);
		if (!ql2xdbwr)
			iounmap((device_reg_t __iomem *)ha->nxdb_wr_ptr);
	} else {
		if (ha->iobase)
			iounmap(ha->iobase);
	}
	pci_release_selected_regions(ha->pdev, ha->bars);
	kfree(ha);
	ha = NULL;

probe_out:
	pci_disable_device(pdev);
	return ret;
}

static void
qla2x00_shutdown(struct pci_dev *pdev)
{
	scsi_qla_host_t *vha;
	struct qla_hw_data  *ha;

	vha = pci_get_drvdata(pdev);
	ha = vha->hw;

	/* Turn-off FCE trace */
	if (ha->flags.fce_enabled) {
		qla2x00_disable_fce_trace(vha, NULL, NULL);
		ha->flags.fce_enabled = 0;
	}

	/* Turn-off EFT trace */
	if (ha->eft)
		qla2x00_disable_eft_trace(vha);

	/* Stop currently executing firmware. */
	qla2x00_try_to_stop_firmware(vha);

	/* Turn adapter off line */
	vha->flags.online = 0;

	/* turn-off interrupts on the card */
	if (ha->interrupts_on) {
		vha->flags.init_done = 0;
		ha->isp_ops->disable_intrs(ha);
	}

	qla2x00_free_irqs(vha);

	qla2x00_free_fw_dump(ha);
}

static void
qla2x00_remove_one(struct pci_dev *pdev)
{
	scsi_qla_host_t *base_vha, *vha;
	struct qla_hw_data  *ha;
	unsigned long flags;

	base_vha = pci_get_drvdata(pdev);
	ha = base_vha->hw;

	mutex_lock(&ha->vport_lock);
	while (ha->cur_vport_count) {
		struct Scsi_Host *scsi_host;

		spin_lock_irqsave(&ha->vport_slock, flags);

		BUG_ON(base_vha->list.next == &ha->vp_list);
		/* This assumes first entry in ha->vp_list is always base vha */
		vha = list_first_entry(&base_vha->list, scsi_qla_host_t, list);
		scsi_host = scsi_host_get(vha->host);

		spin_unlock_irqrestore(&ha->vport_slock, flags);
		mutex_unlock(&ha->vport_lock);

		fc_vport_terminate(vha->fc_vport);
		scsi_host_put(vha->host);

		mutex_lock(&ha->vport_lock);
	}
	mutex_unlock(&ha->vport_lock);

	set_bit(UNLOADING, &base_vha->dpc_flags);

	qla2x00_abort_all_cmds(base_vha, DID_NO_CONNECT << 16);

	qla2x00_dfs_remove(base_vha);

	qla84xx_put_chip(base_vha);

	/* Disable timer */
	if (base_vha->timer_active)
		qla2x00_stop_timer(base_vha);

	base_vha->flags.online = 0;

	/* Flush the work queue and remove it */
	if (ha->wq) {
		flush_workqueue(ha->wq);
		destroy_workqueue(ha->wq);
		ha->wq = NULL;
	}

	/* Kill the kernel thread for this host */
	if (ha->dpc_thread) {
		struct task_struct *t = ha->dpc_thread;

		/*
		 * qla2xxx_wake_dpc checks for ->dpc_thread
		 * so we need to zero it out.
		 */
		ha->dpc_thread = NULL;
		kthread_stop(t);
	}

	qla2x00_free_sysfs_attr(base_vha);

	fc_remove_host(base_vha->host);

	scsi_remove_host(base_vha->host);

	qla2x00_free_device(base_vha);

	scsi_host_put(base_vha->host);

	if (IS_QLA82XX(ha)) {
		qla82xx_idc_lock(ha);
		qla82xx_clear_drv_active(ha);
		qla82xx_idc_unlock(ha);

		iounmap((device_reg_t __iomem *)ha->nx_pcibase);
		if (!ql2xdbwr)
			iounmap((device_reg_t __iomem *)ha->nxdb_wr_ptr);
	} else {
		if (ha->iobase)
			iounmap(ha->iobase);

		if (ha->mqiobase)
			iounmap(ha->mqiobase);
	}

	pci_release_selected_regions(ha->pdev, ha->bars);
	kfree(ha);
	ha = NULL;

	pci_disable_pcie_error_reporting(pdev);

	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
}

static void
qla2x00_free_device(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;

	qla2x00_abort_all_cmds(vha, DID_NO_CONNECT << 16);

	/* Disable timer */
	if (vha->timer_active)
		qla2x00_stop_timer(vha);

	/* Kill the kernel thread for this host */
	if (ha->dpc_thread) {
		struct task_struct *t = ha->dpc_thread;

		/*
		 * qla2xxx_wake_dpc checks for ->dpc_thread
		 * so we need to zero it out.
		 */
		ha->dpc_thread = NULL;
		kthread_stop(t);
	}

	qla25xx_delete_queues(vha);

	if (ha->flags.fce_enabled)
		qla2x00_disable_fce_trace(vha, NULL, NULL);

	if (ha->eft)
		qla2x00_disable_eft_trace(vha);

	/* Stop currently executing firmware. */
	qla2x00_try_to_stop_firmware(vha);

	vha->flags.online = 0;

	/* turn-off interrupts on the card */
	if (ha->interrupts_on) {
		vha->flags.init_done = 0;
		ha->isp_ops->disable_intrs(ha);
	}

	qla2x00_free_irqs(vha);

	qla2x00_free_fcports(vha);

	qla2x00_mem_free(ha);

	qla2x00_free_queues(ha);
}

void qla2x00_free_fcports(struct scsi_qla_host *vha)
{
	fc_port_t *fcport, *tfcport;

	list_for_each_entry_safe(fcport, tfcport, &vha->vp_fcports, list) {
		list_del(&fcport->list);
		kfree(fcport);
		fcport = NULL;
	}
}

static inline void
qla2x00_schedule_rport_del(struct scsi_qla_host *vha, fc_port_t *fcport,
    int defer)
{
	struct fc_rport *rport;
	scsi_qla_host_t *base_vha;
	unsigned long flags;

	if (!fcport->rport)
		return;

	rport = fcport->rport;
	if (defer) {
		base_vha = pci_get_drvdata(vha->hw->pdev);
		spin_lock_irqsave(vha->host->host_lock, flags);
		fcport->drport = rport;
		spin_unlock_irqrestore(vha->host->host_lock, flags);
		set_bit(FCPORT_UPDATE_NEEDED, &base_vha->dpc_flags);
		qla2xxx_wake_dpc(base_vha);
	} else
		fc_remote_port_delete(rport);
}

/*
 * qla2x00_mark_device_lost Updates fcport state when device goes offline.
 *
 * Input: ha = adapter block pointer.  fcport = port structure pointer.
 *
 * Return: None.
 *
 * Context:
 */
void qla2x00_mark_device_lost(scsi_qla_host_t *vha, fc_port_t *fcport,
    int do_login, int defer)
{
	if (atomic_read(&fcport->state) == FCS_ONLINE &&
	    vha->vp_idx == fcport->vp_idx) {
		atomic_set(&fcport->state, FCS_DEVICE_LOST);
		qla2x00_schedule_rport_del(vha, fcport, defer);
	}
	/*
	 * We may need to retry the login, so don't change the state of the
	 * port but do the retries.
	 */
	if (atomic_read(&fcport->state) != FCS_DEVICE_DEAD)
		atomic_set(&fcport->state, FCS_DEVICE_LOST);

	if (!do_login)
		return;

	if (fcport->login_retry == 0) {
		fcport->login_retry = vha->hw->login_retry_count;
		set_bit(RELOGIN_NEEDED, &vha->dpc_flags);

		DEBUG(printk("scsi(%ld): Port login retry: "
		    "%02x%02x%02x%02x%02x%02x%02x%02x, "
		    "id = 0x%04x retry cnt=%d\n",
		    vha->host_no,
		    fcport->port_name[0],
		    fcport->port_name[1],
		    fcport->port_name[2],
		    fcport->port_name[3],
		    fcport->port_name[4],
		    fcport->port_name[5],
		    fcport->port_name[6],
		    fcport->port_name[7],
		    fcport->loop_id,
		    fcport->login_retry));
	}
}

/*
 * qla2x00_mark_all_devices_lost
 *	Updates fcport state when device goes offline.
 *
 * Input:
 *	ha = adapter block pointer.
 *	fcport = port structure pointer.
 *
 * Return:
 *	None.
 *
 * Context:
 */
void
qla2x00_mark_all_devices_lost(scsi_qla_host_t *vha, int defer)
{
	fc_port_t *fcport;

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
		if (vha->vp_idx != 0 && vha->vp_idx != fcport->vp_idx)
			continue;

		/*
		 * No point in marking the device as lost, if the device is
		 * already DEAD.
		 */
		if (atomic_read(&fcport->state) == FCS_DEVICE_DEAD)
			continue;
		if (atomic_read(&fcport->state) == FCS_ONLINE) {
			atomic_set(&fcport->state, FCS_DEVICE_LOST);
			if (defer)
				qla2x00_schedule_rport_del(vha, fcport, defer);
			else if (vha->vp_idx == fcport->vp_idx)
				qla2x00_schedule_rport_del(vha, fcport, defer);
		}
	}
}

/*
* qla2x00_mem_alloc
*      Allocates adapter memory.
*
* Returns:
*      0  = success.
*      !0  = failure.
*/
static int
qla2x00_mem_alloc(struct qla_hw_data *ha, uint16_t req_len, uint16_t rsp_len,
	struct req_que **req, struct rsp_que **rsp)
{
	char	name[16];

	ha->init_cb = dma_alloc_coherent(&ha->pdev->dev, ha->init_cb_size,
		&ha->init_cb_dma, GFP_KERNEL);
	if (!ha->init_cb)
		goto fail;

	ha->gid_list = dma_alloc_coherent(&ha->pdev->dev, GID_LIST_SIZE,
		&ha->gid_list_dma, GFP_KERNEL);
	if (!ha->gid_list)
		goto fail_free_init_cb;

	ha->srb_mempool = mempool_create_slab_pool(SRB_MIN_REQ, srb_cachep);
	if (!ha->srb_mempool)
		goto fail_free_gid_list;

	if (IS_QLA82XX(ha)) {
		/* Allocate cache for CT6 Ctx. */
		if (!ctx_cachep) {
			ctx_cachep = kmem_cache_create("qla2xxx_ctx",
				sizeof(struct ct6_dsd), 0,
				SLAB_HWCACHE_ALIGN, NULL);
			if (!ctx_cachep)
				goto fail_free_gid_list;
		}
		ha->ctx_mempool = mempool_create_slab_pool(SRB_MIN_REQ,
			ctx_cachep);
		if (!ha->ctx_mempool)
			goto fail_free_srb_mempool;
	}

	/* Get memory for cached NVRAM */
	ha->nvram = kzalloc(MAX_NVRAM_SIZE, GFP_KERNEL);
	if (!ha->nvram)
		goto fail_free_ctx_mempool;

	snprintf(name, sizeof(name), "%s_%d", QLA2XXX_DRIVER_NAME,
		ha->pdev->device);
	ha->s_dma_pool = dma_pool_create(name, &ha->pdev->dev,
		DMA_POOL_SIZE, 8, 0);
	if (!ha->s_dma_pool)
		goto fail_free_nvram;

	if (IS_QLA82XX(ha) || ql2xenabledif) {
		ha->dl_dma_pool = dma_pool_create(name, &ha->pdev->dev,
			DSD_LIST_DMA_POOL_SIZE, 8, 0);
		if (!ha->dl_dma_pool) {
			qla_printk(KERN_WARNING, ha,
			    "Memory Allocation failed - dl_dma_pool\n");
			goto fail_s_dma_pool;
		}

		ha->fcp_cmnd_dma_pool = dma_pool_create(name, &ha->pdev->dev,
			FCP_CMND_DMA_POOL_SIZE, 8, 0);
		if (!ha->fcp_cmnd_dma_pool) {
			qla_printk(KERN_WARNING, ha,
			    "Memory Allocation failed - fcp_cmnd_dma_pool\n");
			goto fail_dl_dma_pool;
		}
	}

	/* Allocate memory for SNS commands */
	if (IS_QLA2100(ha) || IS_QLA2200(ha)) {
	/* Get consistent memory allocated for SNS commands */
		ha->sns_cmd = dma_alloc_coherent(&ha->pdev->dev,
		sizeof(struct sns_cmd_pkt), &ha->sns_cmd_dma, GFP_KERNEL);
		if (!ha->sns_cmd)
			goto fail_dma_pool;
	} else {
	/* Get consistent memory allocated for MS IOCB */
		ha->ms_iocb = dma_pool_alloc(ha->s_dma_pool, GFP_KERNEL,
			&ha->ms_iocb_dma);
		if (!ha->ms_iocb)
			goto fail_dma_pool;
	/* Get consistent memory allocated for CT SNS commands */
		ha->ct_sns = dma_alloc_coherent(&ha->pdev->dev,
			sizeof(struct ct_sns_pkt), &ha->ct_sns_dma, GFP_KERNEL);
		if (!ha->ct_sns)
			goto fail_free_ms_iocb;
	}

	/* Allocate memory for request ring */
	*req = kzalloc(sizeof(struct req_que), GFP_KERNEL);
	if (!*req) {
		DEBUG(printk("Unable to allocate memory for req\n"));
		goto fail_req;
	}
	(*req)->length = req_len;
	(*req)->ring = dma_alloc_coherent(&ha->pdev->dev,
		((*req)->length + 1) * sizeof(request_t),
		&(*req)->dma, GFP_KERNEL);
	if (!(*req)->ring) {
		DEBUG(printk("Unable to allocate memory for req_ring\n"));
		goto fail_req_ring;
	}
	/* Allocate memory for response ring */
	*rsp = kzalloc(sizeof(struct rsp_que), GFP_KERNEL);
	if (!*rsp) {
		qla_printk(KERN_WARNING, ha,
			"Unable to allocate memory for rsp\n");
		goto fail_rsp;
	}
	(*rsp)->hw = ha;
	(*rsp)->length = rsp_len;
	(*rsp)->ring = dma_alloc_coherent(&ha->pdev->dev,
		((*rsp)->length + 1) * sizeof(response_t),
		&(*rsp)->dma, GFP_KERNEL);
	if (!(*rsp)->ring) {
		qla_printk(KERN_WARNING, ha,
			"Unable to allocate memory for rsp_ring\n");
		goto fail_rsp_ring;
	}
	(*req)->rsp = *rsp;
	(*rsp)->req = *req;
	/* Allocate memory for NVRAM data for vports */
	if (ha->nvram_npiv_size) {
		ha->npiv_info = kzalloc(sizeof(struct qla_npiv_entry) *
					ha->nvram_npiv_size, GFP_KERNEL);
		if (!ha->npiv_info) {
			qla_printk(KERN_WARNING, ha,
				"Unable to allocate memory for npiv info\n");
			goto fail_npiv_info;
		}
	} else
		ha->npiv_info = NULL;

	/* Get consistent memory allocated for EX-INIT-CB. */
	if (IS_QLA8XXX_TYPE(ha)) {
		ha->ex_init_cb = dma_pool_alloc(ha->s_dma_pool, GFP_KERNEL,
		    &ha->ex_init_cb_dma);
		if (!ha->ex_init_cb)
			goto fail_ex_init_cb;
	}

	INIT_LIST_HEAD(&ha->gbl_dsd_list);

	/* Get consistent memory allocated for Async Port-Database. */
	if (!IS_FWI2_CAPABLE(ha)) {
		ha->async_pd = dma_pool_alloc(ha->s_dma_pool, GFP_KERNEL,
			&ha->async_pd_dma);
		if (!ha->async_pd)
			goto fail_async_pd;
	}

	INIT_LIST_HEAD(&ha->vp_list);
	return 1;

fail_async_pd:
	dma_pool_free(ha->s_dma_pool, ha->ex_init_cb, ha->ex_init_cb_dma);
fail_ex_init_cb:
	kfree(ha->npiv_info);
fail_npiv_info:
	dma_free_coherent(&ha->pdev->dev, ((*rsp)->length + 1) *
		sizeof(response_t), (*rsp)->ring, (*rsp)->dma);
	(*rsp)->ring = NULL;
	(*rsp)->dma = 0;
fail_rsp_ring:
	kfree(*rsp);
fail_rsp:
	dma_free_coherent(&ha->pdev->dev, ((*req)->length + 1) *
		sizeof(request_t), (*req)->ring, (*req)->dma);
	(*req)->ring = NULL;
	(*req)->dma = 0;
fail_req_ring:
	kfree(*req);
fail_req:
	dma_free_coherent(&ha->pdev->dev, sizeof(struct ct_sns_pkt),
		ha->ct_sns, ha->ct_sns_dma);
	ha->ct_sns = NULL;
	ha->ct_sns_dma = 0;
fail_free_ms_iocb:
	dma_pool_free(ha->s_dma_pool, ha->ms_iocb, ha->ms_iocb_dma);
	ha->ms_iocb = NULL;
	ha->ms_iocb_dma = 0;
fail_dma_pool:
	if (IS_QLA82XX(ha) || ql2xenabledif) {
		dma_pool_destroy(ha->fcp_cmnd_dma_pool);
		ha->fcp_cmnd_dma_pool = NULL;
	}
fail_dl_dma_pool:
	if (IS_QLA82XX(ha) || ql2xenabledif) {
		dma_pool_destroy(ha->dl_dma_pool);
		ha->dl_dma_pool = NULL;
	}
fail_s_dma_pool:
	dma_pool_destroy(ha->s_dma_pool);
	ha->s_dma_pool = NULL;
fail_free_nvram:
	kfree(ha->nvram);
	ha->nvram = NULL;
fail_free_ctx_mempool:
	mempool_destroy(ha->ctx_mempool);
	ha->ctx_mempool = NULL;
fail_free_srb_mempool:
	mempool_destroy(ha->srb_mempool);
	ha->srb_mempool = NULL;
fail_free_gid_list:
	dma_free_coherent(&ha->pdev->dev, GID_LIST_SIZE, ha->gid_list,
	ha->gid_list_dma);
	ha->gid_list = NULL;
	ha->gid_list_dma = 0;
fail_free_init_cb:
	dma_free_coherent(&ha->pdev->dev, ha->init_cb_size, ha->init_cb,
	ha->init_cb_dma);
	ha->init_cb = NULL;
	ha->init_cb_dma = 0;
fail:
	DEBUG(printk("%s: Memory allocation failure\n", __func__));
	return -ENOMEM;
}

/*
* qla2x00_free_fw_dump
*	Frees fw dump stuff.
*
* Input:
*	ha = adapter block pointer.
*/
static void
qla2x00_free_fw_dump(struct qla_hw_data *ha)
{
	if (ha->fce)
		dma_free_coherent(&ha->pdev->dev, FCE_SIZE, ha->fce,
		    ha->fce_dma);

	if (ha->fw_dump) {
		if (ha->eft)
			dma_free_coherent(&ha->pdev->dev,
			    ntohl(ha->fw_dump->eft_size), ha->eft, ha->eft_dma);
		vfree(ha->fw_dump);
	}
	ha->fce = NULL;
	ha->fce_dma = 0;
	ha->eft = NULL;
	ha->eft_dma = 0;
	ha->fw_dump = NULL;
	ha->fw_dumped = 0;
	ha->fw_dump_reading = 0;
}

/*
* qla2x00_mem_free
*      Frees all adapter allocated memory.
*
* Input:
*      ha = adapter block pointer.
*/
static void
qla2x00_mem_free(struct qla_hw_data *ha)
{
	qla2x00_free_fw_dump(ha);

	if (ha->srb_mempool)
		mempool_destroy(ha->srb_mempool);

	if (ha->dcbx_tlv)
		dma_free_coherent(&ha->pdev->dev, DCBX_TLV_DATA_SIZE,
		    ha->dcbx_tlv, ha->dcbx_tlv_dma);

	if (ha->xgmac_data)
		dma_free_coherent(&ha->pdev->dev, XGMAC_DATA_SIZE,
		    ha->xgmac_data, ha->xgmac_data_dma);

	if (ha->sns_cmd)
		dma_free_coherent(&ha->pdev->dev, sizeof(struct sns_cmd_pkt),
		ha->sns_cmd, ha->sns_cmd_dma);

	if (ha->ct_sns)
		dma_free_coherent(&ha->pdev->dev, sizeof(struct ct_sns_pkt),
		ha->ct_sns, ha->ct_sns_dma);

	if (ha->sfp_data)
		dma_pool_free(ha->s_dma_pool, ha->sfp_data, ha->sfp_data_dma);

	if (ha->edc_data)
		dma_pool_free(ha->s_dma_pool, ha->edc_data, ha->edc_data_dma);

	if (ha->ms_iocb)
		dma_pool_free(ha->s_dma_pool, ha->ms_iocb, ha->ms_iocb_dma);

	if (ha->ex_init_cb)
		dma_pool_free(ha->s_dma_pool,
			ha->ex_init_cb, ha->ex_init_cb_dma);

	if (ha->async_pd)
		dma_pool_free(ha->s_dma_pool, ha->async_pd, ha->async_pd_dma);

	if (ha->s_dma_pool)
		dma_pool_destroy(ha->s_dma_pool);

	if (ha->gid_list)
		dma_free_coherent(&ha->pdev->dev, GID_LIST_SIZE, ha->gid_list,
		ha->gid_list_dma);

	if (IS_QLA82XX(ha)) {
		if (!list_empty(&ha->gbl_dsd_list)) {
			struct dsd_dma *dsd_ptr, *tdsd_ptr;

			/* clean up allocated prev pool */
			list_for_each_entry_safe(dsd_ptr,
				tdsd_ptr, &ha->gbl_dsd_list, list) {
				dma_pool_free(ha->dl_dma_pool,
				dsd_ptr->dsd_addr, dsd_ptr->dsd_list_dma);
				list_del(&dsd_ptr->list);
				kfree(dsd_ptr);
			}
		}
	}

	if (ha->dl_dma_pool)
		dma_pool_destroy(ha->dl_dma_pool);

	if (ha->fcp_cmnd_dma_pool)
		dma_pool_destroy(ha->fcp_cmnd_dma_pool);

	if (ha->ctx_mempool)
		mempool_destroy(ha->ctx_mempool);

	if (ha->init_cb)
		dma_free_coherent(&ha->pdev->dev, ha->init_cb_size,
			ha->init_cb, ha->init_cb_dma);
	vfree(ha->optrom_buffer);
	kfree(ha->nvram);
	kfree(ha->npiv_info);

	ha->srb_mempool = NULL;
	ha->ctx_mempool = NULL;
	ha->sns_cmd = NULL;
	ha->sns_cmd_dma = 0;
	ha->ct_sns = NULL;
	ha->ct_sns_dma = 0;
	ha->ms_iocb = NULL;
	ha->ms_iocb_dma = 0;
	ha->init_cb = NULL;
	ha->init_cb_dma = 0;
	ha->ex_init_cb = NULL;
	ha->ex_init_cb_dma = 0;
	ha->async_pd = NULL;
	ha->async_pd_dma = 0;

	ha->s_dma_pool = NULL;
	ha->dl_dma_pool = NULL;
	ha->fcp_cmnd_dma_pool = NULL;

	ha->gid_list = NULL;
	ha->gid_list_dma = 0;
}

struct scsi_qla_host *qla2x00_create_host(struct scsi_host_template *sht,
						struct qla_hw_data *ha)
{
	struct Scsi_Host *host;
	struct scsi_qla_host *vha = NULL;

	host = scsi_host_alloc(sht, sizeof(scsi_qla_host_t));
	if (host == NULL) {
		printk(KERN_WARNING
		"qla2xxx: Couldn't allocate host from scsi layer!\n");
		goto fail;
	}

	/* Clear our data area */
	vha = shost_priv(host);
	memset(vha, 0, sizeof(scsi_qla_host_t));

	vha->host = host;
	vha->host_no = host->host_no;
	vha->hw = ha;

	INIT_LIST_HEAD(&vha->vp_fcports);
	INIT_LIST_HEAD(&vha->work_list);
	INIT_LIST_HEAD(&vha->list);

	spin_lock_init(&vha->work_lock);

	sprintf(vha->host_str, "%s_%ld", QLA2XXX_DRIVER_NAME, vha->host_no);
	return vha;

fail:
	return vha;
}

static struct qla_work_evt *
qla2x00_alloc_work(struct scsi_qla_host *vha, enum qla_work_type type)
{
	struct qla_work_evt *e;
	uint8_t bail;

	QLA_VHA_MARK_BUSY(vha, bail);
	if (bail)
		return NULL;

	e = kzalloc(sizeof(struct qla_work_evt), GFP_ATOMIC);
	if (!e) {
		QLA_VHA_MARK_NOT_BUSY(vha);
		return NULL;
	}

	INIT_LIST_HEAD(&e->list);
	e->type = type;
	e->flags = QLA_EVT_FLAG_FREE;
	return e;
}

static int
qla2x00_post_work(struct scsi_qla_host *vha, struct qla_work_evt *e)
{
	unsigned long flags;

	spin_lock_irqsave(&vha->work_lock, flags);
	list_add_tail(&e->list, &vha->work_list);
	spin_unlock_irqrestore(&vha->work_lock, flags);
	qla2xxx_wake_dpc(vha);

	return QLA_SUCCESS;
}

int
qla2x00_post_aen_work(struct scsi_qla_host *vha, enum fc_host_event_code code,
    u32 data)
{
	struct qla_work_evt *e;

	e = qla2x00_alloc_work(vha, QLA_EVT_AEN);
	if (!e)
		return QLA_FUNCTION_FAILED;

	e->u.aen.code = code;
	e->u.aen.data = data;
	return qla2x00_post_work(vha, e);
}

int
qla2x00_post_idc_ack_work(struct scsi_qla_host *vha, uint16_t *mb)
{
	struct qla_work_evt *e;

	e = qla2x00_alloc_work(vha, QLA_EVT_IDC_ACK);
	if (!e)
		return QLA_FUNCTION_FAILED;

	memcpy(e->u.idc_ack.mb, mb, QLA_IDC_ACK_REGS * sizeof(uint16_t));
	return qla2x00_post_work(vha, e);
}

#define qla2x00_post_async_work(name, type)	\
int qla2x00_post_async_##name##_work(		\
    struct scsi_qla_host *vha,			\
    fc_port_t *fcport, uint16_t *data)		\
{						\
	struct qla_work_evt *e;			\
						\
	e = qla2x00_alloc_work(vha, type);	\
	if (!e)					\
		return QLA_FUNCTION_FAILED;	\
						\
	e->u.logio.fcport = fcport;		\
	if (data) {				\
		e->u.logio.data[0] = data[0];	\
		e->u.logio.data[1] = data[1];	\
	}					\
	return qla2x00_post_work(vha, e);	\
}

qla2x00_post_async_work(login, QLA_EVT_ASYNC_LOGIN);
qla2x00_post_async_work(login_done, QLA_EVT_ASYNC_LOGIN_DONE);
qla2x00_post_async_work(logout, QLA_EVT_ASYNC_LOGOUT);
qla2x00_post_async_work(logout_done, QLA_EVT_ASYNC_LOGOUT_DONE);
qla2x00_post_async_work(adisc, QLA_EVT_ASYNC_ADISC);
qla2x00_post_async_work(adisc_done, QLA_EVT_ASYNC_ADISC_DONE);

int
qla2x00_post_uevent_work(struct scsi_qla_host *vha, u32 code)
{
	struct qla_work_evt *e;

	e = qla2x00_alloc_work(vha, QLA_EVT_UEVENT);
	if (!e)
		return QLA_FUNCTION_FAILED;

	e->u.uevent.code = code;
	return qla2x00_post_work(vha, e);
}

static void
qla2x00_uevent_emit(struct scsi_qla_host *vha, u32 code)
{
	char event_string[40];
	char *envp[] = { event_string, NULL };

	switch (code) {
	case QLA_UEVENT_CODE_FW_DUMP:
		snprintf(event_string, sizeof(event_string), "FW_DUMP=%ld",
		    vha->host_no);
		break;
	default:
		/* do nothing */
		break;
	}
	kobject_uevent_env(&vha->hw->pdev->dev.kobj, KOBJ_CHANGE, envp);
}

void
qla2x00_do_work(struct scsi_qla_host *vha)
{
	struct qla_work_evt *e, *tmp;
	unsigned long flags;
	LIST_HEAD(work);

	spin_lock_irqsave(&vha->work_lock, flags);
	list_splice_init(&vha->work_list, &work);
	spin_unlock_irqrestore(&vha->work_lock, flags);

	list_for_each_entry_safe(e, tmp, &work, list) {
		list_del_init(&e->list);

		switch (e->type) {
		case QLA_EVT_AEN:
			fc_host_post_event(vha->host, fc_get_event_number(),
			    e->u.aen.code, e->u.aen.data);
			break;
		case QLA_EVT_IDC_ACK:
			qla81xx_idc_ack(vha, e->u.idc_ack.mb);
			break;
		case QLA_EVT_ASYNC_LOGIN:
			qla2x00_async_login(vha, e->u.logio.fcport,
			    e->u.logio.data);
			break;
		case QLA_EVT_ASYNC_LOGIN_DONE:
			qla2x00_async_login_done(vha, e->u.logio.fcport,
			    e->u.logio.data);
			break;
		case QLA_EVT_ASYNC_LOGOUT:
			qla2x00_async_logout(vha, e->u.logio.fcport);
			break;
		case QLA_EVT_ASYNC_LOGOUT_DONE:
			qla2x00_async_logout_done(vha, e->u.logio.fcport,
			    e->u.logio.data);
			break;
		case QLA_EVT_ASYNC_ADISC:
			qla2x00_async_adisc(vha, e->u.logio.fcport,
			    e->u.logio.data);
			break;
		case QLA_EVT_ASYNC_ADISC_DONE:
			qla2x00_async_adisc_done(vha, e->u.logio.fcport,
			    e->u.logio.data);
			break;
		case QLA_EVT_UEVENT:
			qla2x00_uevent_emit(vha, e->u.uevent.code);
			break;
		}
		if (e->flags & QLA_EVT_FLAG_FREE)
			kfree(e);

		/* For each work completed decrement vha ref count */
		QLA_VHA_MARK_NOT_BUSY(vha);
	}
}

/* Relogins all the fcports of a vport
 * Context: dpc thread
 */
void qla2x00_relogin(struct scsi_qla_host *vha)
{
	fc_port_t       *fcport;
	int status;
	uint16_t        next_loopid = 0;
	struct qla_hw_data *ha = vha->hw;
	uint16_t data[2];

	list_for_each_entry(fcport, &vha->vp_fcports, list) {
	/*
	 * If the port is not ONLINE then try to login
	 * to it if we haven't run out of retries.
	 */
		if (atomic_read(&fcport->state) != FCS_ONLINE &&
		    fcport->login_retry && !(fcport->flags & FCF_ASYNC_SENT)) {
			fcport->login_retry--;
			if (fcport->flags & FCF_FABRIC_DEVICE) {
				if (fcport->flags & FCF_FCP2_DEVICE)
					ha->isp_ops->fabric_logout(vha,
							fcport->loop_id,
							fcport->d_id.b.domain,
							fcport->d_id.b.area,
							fcport->d_id.b.al_pa);

				if (IS_ALOGIO_CAPABLE(ha)) {
					fcport->flags |= FCF_ASYNC_SENT;
					data[0] = 0;
					data[1] = QLA_LOGIO_LOGIN_RETRIED;
					status = qla2x00_post_async_login_work(
					    vha, fcport, data);
					if (status == QLA_SUCCESS)
						continue;
					/* Attempt a retry. */
					status = 1;
				} else
					status = qla2x00_fabric_login(vha,
					    fcport, &next_loopid);
			} else
				status = qla2x00_local_device_login(vha,
								fcport);

			if (status == QLA_SUCCESS) {
				fcport->old_loop_id = fcport->loop_id;

				DEBUG(printk("scsi(%ld): port login OK: logged "
				"in ID 0x%x\n", vha->host_no, fcport->loop_id));

				qla2x00_update_fcport(vha, fcport);

			} else if (status == 1) {
				set_bit(RELOGIN_NEEDED, &vha->dpc_flags);
				/* retry the login again */
				DEBUG(printk("scsi(%ld): Retrying"
				" %d login again loop_id 0x%x\n",
				vha->host_no, fcport->login_retry,
						fcport->loop_id));
			} else {
				fcport->login_retry = 0;
			}

			if (fcport->login_retry == 0 && status != QLA_SUCCESS)
				fcport->loop_id = FC_NO_LOOP_ID;
		}
		if (test_bit(LOOP_RESYNC_NEEDED, &vha->dpc_flags))
			break;
	}
}

/**************************************************************************
* qla2x00_do_dpc
*   This kernel thread is a task that is schedule by the interrupt handler
*   to perform the background processing for interrupts.
*
* Notes:
* This task always run in the context of a kernel thread.  It
* is kick-off by the driver's detect code and starts up
* up one per adapter. It immediately goes to sleep and waits for
* some fibre event.  When either the interrupt handler or
* the timer routine detects a event it will one of the task
* bits then wake us up.
**************************************************************************/
static int
qla2x00_do_dpc(void *data)
{
	int		rval;
	scsi_qla_host_t *base_vha;
	struct qla_hw_data *ha;

	ha = (struct qla_hw_data *)data;
	base_vha = pci_get_drvdata(ha->pdev);

	set_user_nice(current, -20);

	set_current_state(TASK_INTERRUPTIBLE);
	while (!kthread_should_stop()) {
		DEBUG3(printk("qla2x00: DPC handler sleeping\n"));

		schedule();
		__set_current_state(TASK_RUNNING);

		DEBUG3(printk("qla2x00: DPC handler waking up\n"));

		/* Initialization not yet finished. Don't do anything yet. */
		if (!base_vha->flags.init_done)
			continue;

		if (ha->flags.eeh_busy) {
			DEBUG17(qla_printk(KERN_WARNING, ha,
			    "qla2x00_do_dpc: dpc_flags: %lx\n",
			    base_vha->dpc_flags));
			continue;
		}

		DEBUG3(printk("scsi(%ld): DPC handler\n", base_vha->host_no));

		ha->dpc_active = 1;

		if (ha->flags.mbox_busy) {
			ha->dpc_active = 0;
			continue;
		}

		qla2x00_do_work(base_vha);

		if (IS_QLA82XX(ha)) {
			if (test_and_clear_bit(ISP_UNRECOVERABLE,
				&base_vha->dpc_flags)) {
				qla82xx_idc_lock(ha);
				qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE,
					QLA82XX_DEV_FAILED);
				qla82xx_idc_unlock(ha);
				qla_printk(KERN_INFO, ha,
					"HW State: FAILED\n");
				qla82xx_device_state_handler(base_vha);
				continue;
			}

			if (test_and_clear_bit(FCOE_CTX_RESET_NEEDED,
				&base_vha->dpc_flags)) {

				DEBUG(printk(KERN_INFO
					"scsi(%ld): dpc: sched "
					"qla82xx_fcoe_ctx_reset ha = %p\n",
					base_vha->host_no, ha));
				if (!(test_and_set_bit(ABORT_ISP_ACTIVE,
					&base_vha->dpc_flags))) {
					if (qla82xx_fcoe_ctx_reset(base_vha)) {
						/* FCoE-ctx reset failed.
						 * Escalate to chip-reset
						 */
						set_bit(ISP_ABORT_NEEDED,
							&base_vha->dpc_flags);
					}
					clear_bit(ABORT_ISP_ACTIVE,
						&base_vha->dpc_flags);
				}

				DEBUG(printk("scsi(%ld): dpc:"
					" qla82xx_fcoe_ctx_reset end\n",
					base_vha->host_no));
			}
		}

		if (test_and_clear_bit(ISP_ABORT_NEEDED,
						&base_vha->dpc_flags)) {

			DEBUG(printk("scsi(%ld): dpc: sched "
			    "qla2x00_abort_isp ha = %p\n",
			    base_vha->host_no, ha));
			if (!(test_and_set_bit(ABORT_ISP_ACTIVE,
			    &base_vha->dpc_flags))) {

				if (ha->isp_ops->abort_isp(base_vha)) {
					/* failed. retry later */
					set_bit(ISP_ABORT_NEEDED,
					    &base_vha->dpc_flags);
				}
				clear_bit(ABORT_ISP_ACTIVE,
						&base_vha->dpc_flags);
			}

			DEBUG(printk("scsi(%ld): dpc: qla2x00_abort_isp end\n",
			    base_vha->host_no));
		}

		if (test_bit(FCPORT_UPDATE_NEEDED, &base_vha->dpc_flags)) {
			qla2x00_update_fcports(base_vha);
			clear_bit(FCPORT_UPDATE_NEEDED, &base_vha->dpc_flags);
		}

		if (test_bit(ISP_QUIESCE_NEEDED, &base_vha->dpc_flags)) {
			DEBUG(printk(KERN_INFO "scsi(%ld): dpc: sched "
			    "qla2x00_quiesce_needed ha = %p\n",
			    base_vha->host_no, ha));
			qla82xx_device_state_handler(base_vha);
			clear_bit(ISP_QUIESCE_NEEDED, &base_vha->dpc_flags);
			if (!ha->flags.quiesce_owner) {
				qla2x00_perform_loop_resync(base_vha);

				qla82xx_idc_lock(ha);
				qla82xx_clear_qsnt_ready(base_vha);
				qla82xx_idc_unlock(ha);
			}
		}

		if (test_and_clear_bit(RESET_MARKER_NEEDED,
							&base_vha->dpc_flags) &&
		    (!(test_and_set_bit(RESET_ACTIVE, &base_vha->dpc_flags)))) {

			DEBUG(printk("scsi(%ld): qla2x00_reset_marker()\n",
			    base_vha->host_no));

			qla2x00_rst_aen(base_vha);
			clear_bit(RESET_ACTIVE, &base_vha->dpc_flags);
		}

		/* Retry each device up to login retry count */
		if ((test_and_clear_bit(RELOGIN_NEEDED,
						&base_vha->dpc_flags)) &&
		    !test_bit(LOOP_RESYNC_NEEDED, &base_vha->dpc_flags) &&
		    atomic_read(&base_vha->loop_state) != LOOP_DOWN) {

			DEBUG(printk("scsi(%ld): qla2x00_port_login()\n",
					base_vha->host_no));
			qla2x00_relogin(base_vha);

			DEBUG(printk("scsi(%ld): qla2x00_port_login - end\n",
			    base_vha->host_no));
		}

		if (test_and_clear_bit(LOOP_RESYNC_NEEDED,
							&base_vha->dpc_flags)) {

			DEBUG(printk("scsi(%ld): qla2x00_loop_resync()\n",
				base_vha->host_no));

			if (!(test_and_set_bit(LOOP_RESYNC_ACTIVE,
			    &base_vha->dpc_flags))) {

				rval = qla2x00_loop_resync(base_vha);

				clear_bit(LOOP_RESYNC_ACTIVE,
						&base_vha->dpc_flags);
			}

			DEBUG(printk("scsi(%ld): qla2x00_loop_resync - end\n",
			    base_vha->host_no));
		}

		if (test_bit(NPIV_CONFIG_NEEDED, &base_vha->dpc_flags) &&
		    atomic_read(&base_vha->loop_state) == LOOP_READY) {
			clear_bit(NPIV_CONFIG_NEEDED, &base_vha->dpc_flags);
			qla2xxx_flash_npiv_conf(base_vha);
		}

		if (!ha->interrupts_on)
			ha->isp_ops->enable_intrs(ha);

		if (test_and_clear_bit(BEACON_BLINK_NEEDED,
					&base_vha->dpc_flags))
			ha->isp_ops->beacon_blink(base_vha);

		qla2x00_do_dpc_all_vps(base_vha);

		ha->dpc_active = 0;
		set_current_state(TASK_INTERRUPTIBLE);
	} /* End of while(1) */
	__set_current_state(TASK_RUNNING);

	DEBUG(printk("scsi(%ld): DPC handler exiting\n", base_vha->host_no));

	/*
	 * Make sure that nobody tries to wake us up again.
	 */
	ha->dpc_active = 0;

	/* Cleanup any residual CTX SRBs. */
	qla2x00_abort_all_cmds(base_vha, DID_NO_CONNECT << 16);

	return 0;
}

void
qla2xxx_wake_dpc(struct scsi_qla_host *vha)
{
	struct qla_hw_data *ha = vha->hw;
	struct task_struct *t = ha->dpc_thread;

	if (!test_bit(UNLOADING, &vha->dpc_flags) && t)
		wake_up_process(t);
}

/*
*  qla2x00_rst_aen
*      Processes asynchronous reset.
*
* Input:
*      ha  = adapter block pointer.
*/
static void
qla2x00_rst_aen(scsi_qla_host_t *vha)
{
	if (vha->flags.online && !vha->flags.reset_active &&
	    !atomic_read(&vha->loop_down_timer) &&
	    !(test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags))) {
		do {
			clear_bit(RESET_MARKER_NEEDED, &vha->dpc_flags);

			/*
			 * Issue marker command only when we are going to start
			 * the I/O.
			 */
			vha->marker_needed = 1;
		} while (!atomic_read(&vha->loop_down_timer) &&
		    (test_bit(RESET_MARKER_NEEDED, &vha->dpc_flags)));
	}
}

static void
qla2x00_sp_free_dma(srb_t *sp)
{
	struct scsi_cmnd *cmd = sp->cmd;
	struct qla_hw_data *ha = sp->fcport->vha->hw;

	if (sp->flags & SRB_DMA_VALID) {
		scsi_dma_unmap(cmd);
		sp->flags &= ~SRB_DMA_VALID;
	}

	if (sp->flags & SRB_CRC_PROT_DMA_VALID) {
		dma_unmap_sg(&ha->pdev->dev, scsi_prot_sglist(cmd),
		    scsi_prot_sg_count(cmd), cmd->sc_data_direction);
		sp->flags &= ~SRB_CRC_PROT_DMA_VALID;
	}

	if (sp->flags & SRB_CRC_CTX_DSD_VALID) {
		/* List assured to be having elements */
		qla2x00_clean_dsd_pool(ha, sp);
		sp->flags &= ~SRB_CRC_CTX_DSD_VALID;
	}

	if (sp->flags & SRB_CRC_CTX_DMA_VALID) {
		dma_pool_free(ha->dl_dma_pool, sp->ctx,
		    ((struct crc_context *)sp->ctx)->crc_ctx_dma);
		sp->flags &= ~SRB_CRC_CTX_DMA_VALID;
	}

	CMD_SP(cmd) = NULL;
}

static void
qla2x00_sp_final_compl(struct qla_hw_data *ha, srb_t *sp)
{
	struct scsi_cmnd *cmd = sp->cmd;

	qla2x00_sp_free_dma(sp);

	if (sp->flags & SRB_FCP_CMND_DMA_VALID) {
		struct ct6_dsd *ctx = sp->ctx;
		dma_pool_free(ha->fcp_cmnd_dma_pool, ctx->fcp_cmnd,
			ctx->fcp_cmnd_dma);
		list_splice(&ctx->dsd_list, &ha->gbl_dsd_list);
		ha->gbl_dsd_inuse -= ctx->dsd_use_cnt;
		ha->gbl_dsd_avail += ctx->dsd_use_cnt;
		mempool_free(sp->ctx, ha->ctx_mempool);
		sp->ctx = NULL;
	}

	mempool_free(sp, ha->srb_mempool);
	cmd->scsi_done(cmd);
}

void
qla2x00_sp_compl(struct qla_hw_data *ha, srb_t *sp)
{
	if (atomic_read(&sp->ref_count) == 0) {
		DEBUG2(qla_printk(KERN_WARNING, ha,
		    "SP reference-count to ZERO -- sp=%p\n", sp));
		DEBUG2(BUG());
		return;
	}
	if (!atomic_dec_and_test(&sp->ref_count))
		return;
	qla2x00_sp_final_compl(ha, sp);
}

/**************************************************************************
*   qla2x00_timer
*
* Description:
*   One second timer
*
* Context: Interrupt
***************************************************************************/
void
qla2x00_timer(scsi_qla_host_t *vha)
{
	unsigned long	cpu_flags = 0;
	int		start_dpc = 0;
	int		index;
	srb_t		*sp;
	uint16_t        w;
	struct qla_hw_data *ha = vha->hw;
	struct req_que *req;

	if (ha->flags.eeh_busy) {
		qla2x00_restart_timer(vha, WATCH_INTERVAL);
		return;
	}

	/* Hardware read to raise pending EEH errors during mailbox waits. */
	if (!pci_channel_offline(ha->pdev))
		pci_read_config_word(ha->pdev, PCI_VENDOR_ID, &w);

	/* Make sure qla82xx_watchdog is run only for physical port */
	if (!vha->vp_idx && IS_QLA82XX(ha)) {
		if (test_bit(ISP_QUIESCE_NEEDED, &vha->dpc_flags))
			start_dpc++;
		qla82xx_watchdog(vha);
	}

	/* Loop down handler. */
	if (atomic_read(&vha->loop_down_timer) > 0 &&
	    !(test_bit(ABORT_ISP_ACTIVE, &vha->dpc_flags))
		&& vha->flags.online) {

		if (atomic_read(&vha->loop_down_timer) ==
		    vha->loop_down_abort_time) {

			DEBUG(printk("scsi(%ld): Loop Down - aborting the "
			    "queues before time expire\n",
			    vha->host_no));

			if (!IS_QLA2100(ha) && vha->link_down_timeout)
				atomic_set(&vha->loop_state, LOOP_DEAD);

			/*
			 * Schedule an ISP abort to return any FCP2-device
			 * commands.
			 */
			/* NPIV - scan physical port only */
			if (!vha->vp_idx) {
				spin_lock_irqsave(&ha->hardware_lock,
				    cpu_flags);
				req = ha->req_q_map[0];
				for (index = 1;
				    index < MAX_OUTSTANDING_COMMANDS;
				    index++) {
					fc_port_t *sfcp;

					sp = req->outstanding_cmds[index];
					if (!sp)
						continue;
					if (sp->ctx && !IS_PROT_IO(sp))
						continue;
					sfcp = sp->fcport;
					if (!(sfcp->flags & FCF_FCP2_DEVICE))
						continue;

					set_bit(ISP_ABORT_NEEDED,
							&vha->dpc_flags);
					break;
				}
				spin_unlock_irqrestore(&ha->hardware_lock,
								cpu_flags);
			}
			start_dpc++;
		}

		/* if the loop has been down for 4 minutes, reinit adapter */
		if (atomic_dec_and_test(&vha->loop_down_timer) != 0) {
			if (!(vha->device_flags & DFLG_NO_CABLE)) {
				DEBUG(printk("scsi(%ld): Loop down - "
				    "aborting ISP.\n",
				    vha->host_no));
				qla_printk(KERN_WARNING, ha,
				    "Loop down - aborting ISP.\n");

				set_bit(ISP_ABORT_NEEDED, &vha->dpc_flags);
			}
		}
		DEBUG3(printk("scsi(%ld): Loop Down - seconds remaining %d\n",
		    vha->host_no,
		    atomic_read(&vha->loop_down_timer)));
	}

	/* Check if beacon LED needs to be blinked for physical host only */
	if (!vha->vp_idx && (ha->beacon_blink_led == 1)) {
		set_bit(BEACON_BLINK_NEEDED, &vha->dpc_flags);
		start_dpc++;
	}

	/* Process any deferred work. */
	if (!list_empty(&vha->work_list))
		start_dpc++;

	/* Schedule the DPC routine if needed */
	if ((test_bit(ISP_ABORT_NEEDED, &vha->dpc_flags) ||
	    test_bit(LOOP_RESYNC_NEEDED, &vha->dpc_flags) ||
	    test_bit(FCPORT_UPDATE_NEEDED, &vha->dpc_flags) ||
	    start_dpc ||
	    test_bit(RESET_MARKER_NEEDED, &vha->dpc_flags) ||
	    test_bit(BEACON_BLINK_NEEDED, &vha->dpc_flags) ||
	    test_bit(ISP_UNRECOVERABLE, &vha->dpc_flags) ||
	    test_bit(FCOE_CTX_RESET_NEEDED, &vha->dpc_flags) ||
	    test_bit(VP_DPC_NEEDED, &vha->dpc_flags) ||
	    test_bit(RELOGIN_NEEDED, &vha->dpc_flags)))
		qla2xxx_wake_dpc(vha);

	qla2x00_restart_timer(vha, WATCH_INTERVAL);
}

/* Firmware interface routines. */

#define FW_BLOBS	8
#define FW_ISP21XX	0
#define FW_ISP22XX	1
#define FW_ISP2300	2
#define FW_ISP2322	3
#define FW_ISP24XX	4
#define FW_ISP25XX	5
#define FW_ISP81XX	6
#define FW_ISP82XX	7

#define FW_FILE_ISP21XX	"ql2100_fw.bin"
#define FW_FILE_ISP22XX	"ql2200_fw.bin"
#define FW_FILE_ISP2300	"ql2300_fw.bin"
#define FW_FILE_ISP2322	"ql2322_fw.bin"
#define FW_FILE_ISP24XX	"ql2400_fw.bin"
#define FW_FILE_ISP25XX	"ql2500_fw.bin"
#define FW_FILE_ISP81XX	"ql8100_fw.bin"
#define FW_FILE_ISP82XX	"ql8200_fw.bin"

static DEFINE_MUTEX(qla_fw_lock);

static struct fw_blob qla_fw_blobs[FW_BLOBS] = {
	{ .name = FW_FILE_ISP21XX, .segs = { 0x1000, 0 }, },
	{ .name = FW_FILE_ISP22XX, .segs = { 0x1000, 0 }, },
	{ .name = FW_FILE_ISP2300, .segs = { 0x800, 0 }, },
	{ .name = FW_FILE_ISP2322, .segs = { 0x800, 0x1c000, 0x1e000, 0 }, },
	{ .name = FW_FILE_ISP24XX, },
	{ .name = FW_FILE_ISP25XX, },
	{ .name = FW_FILE_ISP81XX, },
	{ .name = FW_FILE_ISP82XX, },
};

struct fw_blob *
qla2x00_request_firmware(scsi_qla_host_t *vha)
{
	struct qla_hw_data *ha = vha->hw;
	struct fw_blob *blob;

	blob = NULL;
	if (IS_QLA2100(ha)) {
		blob = &qla_fw_blobs[FW_ISP21XX];
	} else if (IS_QLA2200(ha)) {
		blob = &qla_fw_blobs[FW_ISP22XX];
	} else if (IS_QLA2300(ha) || IS_QLA2312(ha) || IS_QLA6312(ha)) {
		blob = &qla_fw_blobs[FW_ISP2300];
	} else if (IS_QLA2322(ha) || IS_QLA6322(ha)) {
		blob = &qla_fw_blobs[FW_ISP2322];
	} else if (IS_QLA24XX_TYPE(ha)) {
		blob = &qla_fw_blobs[FW_ISP24XX];
	} else if (IS_QLA25XX(ha)) {
		blob = &qla_fw_blobs[FW_ISP25XX];
	} else if (IS_QLA81XX(ha)) {
		blob = &qla_fw_blobs[FW_ISP81XX];
	} else if (IS_QLA82XX(ha)) {
		blob = &qla_fw_blobs[FW_ISP82XX];
	}

	mutex_lock(&qla_fw_lock);
	if (blob->fw)
		goto out;

	if (request_firmware(&blob->fw, blob->name, &ha->pdev->dev)) {
		DEBUG2(printk("scsi(%ld): Failed to load firmware image "
		    "(%s).\n", vha->host_no, blob->name));
		blob->fw = NULL;
		blob = NULL;
		goto out;
	}

out:
	mutex_unlock(&qla_fw_lock);
	return blob;
}

static void
qla2x00_release_firmware(void)
{
	int idx;

	mutex_lock(&qla_fw_lock);
	for (idx = 0; idx < FW_BLOBS; idx++)
		if (qla_fw_blobs[idx].fw)
			release_firmware(qla_fw_blobs[idx].fw);
	mutex_unlock(&qla_fw_lock);
}

static pci_ers_result_t
qla2xxx_pci_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	scsi_qla_host_t *vha = pci_get_drvdata(pdev);
	struct qla_hw_data *ha = vha->hw;

	DEBUG2(qla_printk(KERN_WARNING, ha, "error_detected:state %x\n",
	    state));

	switch (state) {
	case pci_channel_io_normal:
		ha->flags.eeh_busy = 0;
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		ha->flags.eeh_busy = 1;
		/* For ISP82XX complete any pending mailbox cmd */
		if (IS_QLA82XX(ha)) {
			ha->flags.isp82xx_fw_hung = 1;
			if (ha->flags.mbox_busy) {
				ha->flags.mbox_int = 1;
				DEBUG2(qla_printk(KERN_ERR, ha,
					"Due to pci channel io frozen, doing premature "
					"completion of mbx command\n"));
				complete(&ha->mbx_intr_comp);
			}
		}
		qla2x00_free_irqs(vha);
		pci_disable_device(pdev);
		/* Return back all IOs */
		qla2x00_abort_all_cmds(vha, DID_RESET << 16);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		ha->flags.pci_channel_io_perm_failure = 1;
		qla2x00_abort_all_cmds(vha, DID_NO_CONNECT << 16);
		return PCI_ERS_RESULT_DISCONNECT;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t
qla2xxx_pci_mmio_enabled(struct pci_dev *pdev)
{
	int risc_paused = 0;
	uint32_t stat;
	unsigned long flags;
	scsi_qla_host_t *base_vha = pci_get_drvdata(pdev);
	struct qla_hw_data *ha = base_vha->hw;
	struct device_reg_2xxx __iomem *reg = &ha->iobase->isp;
	struct device_reg_24xx __iomem *reg24 = &ha->iobase->isp24;

	if (IS_QLA82XX(ha))
		return PCI_ERS_RESULT_RECOVERED;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	if (IS_QLA2100(ha) || IS_QLA2200(ha)){
		stat = RD_REG_DWORD(&reg->hccr);
		if (stat & HCCR_RISC_PAUSE)
			risc_paused = 1;
	} else if (IS_QLA23XX(ha)) {
		stat = RD_REG_DWORD(&reg->u.isp2300.host_status);
		if (stat & HSR_RISC_PAUSED)
			risc_paused = 1;
	} else if (IS_FWI2_CAPABLE(ha)) {
		stat = RD_REG_DWORD(&reg24->host_status);
		if (stat & HSRX_RISC_PAUSED)
			risc_paused = 1;
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	if (risc_paused) {
		qla_printk(KERN_INFO, ha, "RISC paused -- mmio_enabled, "
		    "Dumping firmware!\n");
		ha->isp_ops->fw_dump(base_vha, 0);

		return PCI_ERS_RESULT_NEED_RESET;
	} else
		return PCI_ERS_RESULT_RECOVERED;
}

uint32_t qla82xx_error_recovery(scsi_qla_host_t *base_vha)
{
	uint32_t rval = QLA_FUNCTION_FAILED;
	uint32_t drv_active = 0;
	struct qla_hw_data *ha = base_vha->hw;
	int fn;
	struct pci_dev *other_pdev = NULL;

	DEBUG17(qla_printk(KERN_INFO, ha,
	    "scsi(%ld): In qla82xx_error_recovery\n", base_vha->host_no));

	set_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);

	if (base_vha->flags.online) {
		/* Abort all outstanding commands,
		 * so as to be requeued later */
		qla2x00_abort_isp_cleanup(base_vha);
	}


	fn = PCI_FUNC(ha->pdev->devfn);
	while (fn > 0) {
		fn--;
		DEBUG17(qla_printk(KERN_INFO, ha,
		    "Finding pci device at function = 0x%x\n", fn));
		other_pdev =
		    pci_get_domain_bus_and_slot(pci_domain_nr(ha->pdev->bus),
		    ha->pdev->bus->number, PCI_DEVFN(PCI_SLOT(ha->pdev->devfn),
		    fn));

		if (!other_pdev)
			continue;
		if (atomic_read(&other_pdev->enable_cnt)) {
			DEBUG17(qla_printk(KERN_INFO, ha,
			    "Found PCI func available and enabled at 0x%x\n",
			    fn));
			pci_dev_put(other_pdev);
			break;
		}
		pci_dev_put(other_pdev);
	}

	if (!fn) {
		/* Reset owner */
		DEBUG17(qla_printk(KERN_INFO, ha,
		    "This devfn is reset owner = 0x%x\n", ha->pdev->devfn));
		qla82xx_idc_lock(ha);

		qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE,
		    QLA82XX_DEV_INITIALIZING);

		qla82xx_wr_32(ha, QLA82XX_CRB_DRV_IDC_VERSION,
		    QLA82XX_IDC_VERSION);

		drv_active = qla82xx_rd_32(ha, QLA82XX_CRB_DRV_ACTIVE);
		DEBUG17(qla_printk(KERN_INFO, ha,
		    "drv_active = 0x%x\n", drv_active));

		qla82xx_idc_unlock(ha);
		/* Reset if device is not already reset
		 * drv_active would be 0 if a reset has already been done
		 */
		if (drv_active)
			rval = qla82xx_start_firmware(base_vha);
		else
			rval = QLA_SUCCESS;
		qla82xx_idc_lock(ha);

		if (rval != QLA_SUCCESS) {
			qla_printk(KERN_INFO, ha, "HW State: FAILED\n");
			qla82xx_clear_drv_active(ha);
			qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE,
			    QLA82XX_DEV_FAILED);
		} else {
			qla_printk(KERN_INFO, ha, "HW State: READY\n");
			qla82xx_wr_32(ha, QLA82XX_CRB_DEV_STATE,
			    QLA82XX_DEV_READY);
			qla82xx_idc_unlock(ha);
			ha->flags.isp82xx_fw_hung = 0;
			rval = qla82xx_restart_isp(base_vha);
			qla82xx_idc_lock(ha);
			/* Clear driver state register */
			qla82xx_wr_32(ha, QLA82XX_CRB_DRV_STATE, 0);
			qla82xx_set_drv_active(base_vha);
		}
		qla82xx_idc_unlock(ha);
	} else {
		DEBUG17(qla_printk(KERN_INFO, ha,
		    "This devfn is not reset owner = 0x%x\n", ha->pdev->devfn));
		if ((qla82xx_rd_32(ha, QLA82XX_CRB_DEV_STATE) ==
		    QLA82XX_DEV_READY)) {
			ha->flags.isp82xx_fw_hung = 0;
			rval = qla82xx_restart_isp(base_vha);
			qla82xx_idc_lock(ha);
			qla82xx_set_drv_active(base_vha);
			qla82xx_idc_unlock(ha);
		}
	}
	clear_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);

	return rval;
}

static pci_ers_result_t
qla2xxx_pci_slot_reset(struct pci_dev *pdev)
{
	pci_ers_result_t ret = PCI_ERS_RESULT_DISCONNECT;
	scsi_qla_host_t *base_vha = pci_get_drvdata(pdev);
	struct qla_hw_data *ha = base_vha->hw;
	struct rsp_que *rsp;
	int rc, retries = 10;

	DEBUG17(qla_printk(KERN_WARNING, ha, "slot_reset\n"));

	/* Workaround: qla2xxx driver which access hardware earlier
	 * needs error state to be pci_channel_io_online.
	 * Otherwise mailbox command timesout.
	 */
	pdev->error_state = pci_channel_io_normal;

	pci_restore_state(pdev);

	/* pci_restore_state() clears the saved_state flag of the device
	 * save restored state which resets saved_state flag
	 */
	pci_save_state(pdev);

	if (ha->mem_only)
		rc = pci_enable_device_mem(pdev);
	else
		rc = pci_enable_device(pdev);

	if (rc) {
		qla_printk(KERN_WARNING, ha,
		    "Can't re-enable PCI device after reset.\n");
		goto exit_slot_reset;
	}

	rsp = ha->rsp_q_map[0];
	if (qla2x00_request_irqs(ha, rsp))
		goto exit_slot_reset;

	if (ha->isp_ops->pci_config(base_vha))
		goto exit_slot_reset;

	if (IS_QLA82XX(ha)) {
		if (qla82xx_error_recovery(base_vha) == QLA_SUCCESS) {
			ret = PCI_ERS_RESULT_RECOVERED;
			goto exit_slot_reset;
		} else
			goto exit_slot_reset;
	}

	while (ha->flags.mbox_busy && retries--)
		msleep(1000);

	set_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);
	if (ha->isp_ops->abort_isp(base_vha) == QLA_SUCCESS)
		ret =  PCI_ERS_RESULT_RECOVERED;
	clear_bit(ABORT_ISP_ACTIVE, &base_vha->dpc_flags);


exit_slot_reset:
	DEBUG17(qla_printk(KERN_WARNING, ha,
	    "slot_reset-return:ret=%x\n", ret));

	return ret;
}

static void
qla2xxx_pci_resume(struct pci_dev *pdev)
{
	scsi_qla_host_t *base_vha = pci_get_drvdata(pdev);
	struct qla_hw_data *ha = base_vha->hw;
	int ret;

	DEBUG17(qla_printk(KERN_WARNING, ha, "pci_resume\n"));

	ret = qla2x00_wait_for_hba_online(base_vha);
	if (ret != QLA_SUCCESS) {
		qla_printk(KERN_ERR, ha,
		    "the device failed to resume I/O "
		    "from slot/link_reset");
	}

	pci_cleanup_aer_uncorrect_error_status(pdev);

	ha->flags.eeh_busy = 0;
}

static struct pci_error_handlers qla2xxx_err_handler = {
	.error_detected = qla2xxx_pci_error_detected,
	.mmio_enabled = qla2xxx_pci_mmio_enabled,
	.slot_reset = qla2xxx_pci_slot_reset,
	.resume = qla2xxx_pci_resume,
};

static struct pci_device_id qla2xxx_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2100) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2200) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2300) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2312) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2322) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP6312) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP6322) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2422) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2432) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP8432) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP5422) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP5432) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP2532) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP8001) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP8021) },
	{ 0 },
};
MODULE_DEVICE_TABLE(pci, qla2xxx_pci_tbl);

static struct pci_driver qla2xxx_pci_driver = {
	.name		= QLA2XXX_DRIVER_NAME,
	.driver		= {
		.owner		= THIS_MODULE,
	},
	.id_table	= qla2xxx_pci_tbl,
	.probe		= qla2x00_probe_one,
	.remove		= qla2x00_remove_one,
	.shutdown	= qla2x00_shutdown,
	.err_handler	= &qla2xxx_err_handler,
};

static struct file_operations apidev_fops = {
	.owner = THIS_MODULE,
	.llseek = noop_llseek,
};

/**
 * qla2x00_module_init - Module initialization.
 **/
static int __init
qla2x00_module_init(void)
{
	int ret = 0;

	/* Allocate cache for SRBs. */
	srb_cachep = kmem_cache_create("qla2xxx_srbs", sizeof(srb_t), 0,
	    SLAB_HWCACHE_ALIGN, NULL);
	if (srb_cachep == NULL) {
		printk(KERN_ERR
		    "qla2xxx: Unable to allocate SRB cache...Failing load!\n");
		return -ENOMEM;
	}

	/* Derive version string. */
	strcpy(qla2x00_version_str, QLA2XXX_VERSION);
	if (ql2xextended_error_logging)
		strcat(qla2x00_version_str, "-debug");

	qla2xxx_transport_template =
	    fc_attach_transport(&qla2xxx_transport_functions);
	if (!qla2xxx_transport_template) {
		kmem_cache_destroy(srb_cachep);
		return -ENODEV;
	}

	apidev_major = register_chrdev(0, QLA2XXX_APIDEV, &apidev_fops);
	if (apidev_major < 0) {
		printk(KERN_WARNING "qla2xxx: Unable to register char device "
		    "%s\n", QLA2XXX_APIDEV);
	}

	qla2xxx_transport_vport_template =
	    fc_attach_transport(&qla2xxx_transport_vport_functions);
	if (!qla2xxx_transport_vport_template) {
		kmem_cache_destroy(srb_cachep);
		fc_release_transport(qla2xxx_transport_template);
		return -ENODEV;
	}

	printk(KERN_INFO "QLogic Fibre Channel HBA Driver: %s\n",
	    qla2x00_version_str);
	ret = pci_register_driver(&qla2xxx_pci_driver);
	if (ret) {
		kmem_cache_destroy(srb_cachep);
		fc_release_transport(qla2xxx_transport_template);
		fc_release_transport(qla2xxx_transport_vport_template);
	}
	return ret;
}

/**
 * qla2x00_module_exit - Module cleanup.
 **/
static void __exit
qla2x00_module_exit(void)
{
	unregister_chrdev(apidev_major, QLA2XXX_APIDEV);
	pci_unregister_driver(&qla2xxx_pci_driver);
	qla2x00_release_firmware();
	kmem_cache_destroy(srb_cachep);
	if (ctx_cachep)
		kmem_cache_destroy(ctx_cachep);
	fc_release_transport(qla2xxx_transport_template);
	fc_release_transport(qla2xxx_transport_vport_template);
}

module_init(qla2x00_module_init);
module_exit(qla2x00_module_exit);

MODULE_AUTHOR("QLogic Corporation");
MODULE_DESCRIPTION("QLogic Fibre Channel HBA Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(QLA2XXX_VERSION);
MODULE_FIRMWARE(FW_FILE_ISP21XX);
MODULE_FIRMWARE(FW_FILE_ISP22XX);
MODULE_FIRMWARE(FW_FILE_ISP2300);
MODULE_FIRMWARE(FW_FILE_ISP2322);
MODULE_FIRMWARE(FW_FILE_ISP24XX);
MODULE_FIRMWARE(FW_FILE_ISP25XX);
