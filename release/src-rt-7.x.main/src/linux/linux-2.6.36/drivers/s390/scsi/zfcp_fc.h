/*
 * zfcp device driver
 *
 * Fibre Channel related definitions and inline functions for the zfcp
 * device driver
 *
 * Copyright IBM Corporation 2009
 */

#ifndef ZFCP_FC_H
#define ZFCP_FC_H

#include <scsi/fc/fc_els.h>
#include <scsi/fc/fc_fcp.h>
#include <scsi/fc/fc_ns.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_tcq.h>
#include "zfcp_fsf.h"

#define ZFCP_FC_CT_SIZE_PAGE	  (PAGE_SIZE - sizeof(struct fc_ct_hdr))
#define ZFCP_FC_GPN_FT_ENT_PAGE	  (ZFCP_FC_CT_SIZE_PAGE \
					/ sizeof(struct fc_gpn_ft_resp))
#define ZFCP_FC_GPN_FT_NUM_BUFS	  4 /* memory pages */

#define ZFCP_FC_GPN_FT_MAX_SIZE	  (ZFCP_FC_GPN_FT_NUM_BUFS * PAGE_SIZE \
					- sizeof(struct fc_ct_hdr))
#define ZFCP_FC_GPN_FT_MAX_ENT	  (ZFCP_FC_GPN_FT_NUM_BUFS * \
					(ZFCP_FC_GPN_FT_ENT_PAGE + 1))

#define ZFCP_FC_CTELS_TMO	(2 * FC_DEF_R_A_TOV / 1000)

/**
 * struct zfcp_fc_event - FC HBAAPI event for internal queueing from irq context
 * @code: Event code
 * @data: Event data
 * @list: list_head for zfcp_fc_events list
 */
struct zfcp_fc_event {
	enum fc_host_event_code code;
	u32 data;
	struct list_head list;
};

/**
 * struct zfcp_fc_events - Infrastructure for posting FC events from irq context
 * @list: List for queueing of events from irq context to workqueue
 * @list_lock: Lock for event list
 * @work: work_struct for forwarding events in workqueue
*/
struct zfcp_fc_events {
	struct list_head list;
	spinlock_t list_lock;
	struct work_struct work;
};

/**
 * struct zfcp_fc_gid_pn_req - container for ct header plus gid_pn request
 * @ct_hdr: FC GS common transport header
 * @gid_pn: GID_PN request
 */
struct zfcp_fc_gid_pn_req {
	struct fc_ct_hdr	ct_hdr;
	struct fc_ns_gid_pn	gid_pn;
} __packed;

/**
 * struct zfcp_fc_gid_pn_resp - container for ct header plus gid_pn response
 * @ct_hdr: FC GS common transport header
 * @gid_pn: GID_PN response
 */
struct zfcp_fc_gid_pn_resp {
	struct fc_ct_hdr	ct_hdr;
	struct fc_gid_pn_resp	gid_pn;
} __packed;

/**
 * struct zfcp_fc_gid_pn - everything required in zfcp for gid_pn request
 * @ct: data passed to zfcp_fsf for issuing fsf request
 * @sg_req: scatterlist entry for request data
 * @sg_resp: scatterlist entry for response data
 * @gid_pn_req: GID_PN request data
 * @gid_pn_resp: GID_PN response data
 */
struct zfcp_fc_gid_pn {
	struct zfcp_fsf_ct_els ct;
	struct scatterlist sg_req;
	struct scatterlist sg_resp;
	struct zfcp_fc_gid_pn_req gid_pn_req;
	struct zfcp_fc_gid_pn_resp gid_pn_resp;
	struct zfcp_port *port;
};

/**
 * struct zfcp_fc_gpn_ft - container for ct header plus gpn_ft request
 * @ct_hdr: FC GS common transport header
 * @gpn_ft: GPN_FT request
 */
struct zfcp_fc_gpn_ft_req {
	struct fc_ct_hdr	ct_hdr;
	struct fc_ns_gid_ft	gpn_ft;
} __packed;

/**
 * struct zfcp_fc_gpn_ft_resp - container for ct header plus gpn_ft response
 * @ct_hdr: FC GS common transport header
 * @gpn_ft: Array of gpn_ft response data to fill one memory page
 */
struct zfcp_fc_gpn_ft_resp {
	struct fc_ct_hdr	ct_hdr;
	struct fc_gpn_ft_resp	gpn_ft[ZFCP_FC_GPN_FT_ENT_PAGE];
} __packed;

/**
 * struct zfcp_fc_gpn_ft - zfcp data for gpn_ft request
 * @ct: data passed to zfcp_fsf for issuing fsf request
 * @sg_req: scatter list entry for gpn_ft request
 * @sg_resp: scatter list entries for gpn_ft responses (per memory page)
 */
struct zfcp_fc_gpn_ft {
	struct zfcp_fsf_ct_els ct;
	struct scatterlist sg_req;
	struct scatterlist sg_resp[ZFCP_FC_GPN_FT_NUM_BUFS];
};

/**
 * struct zfcp_fc_els_adisc - everything required in zfcp for issuing ELS ADISC
 * @els: data required for issuing els fsf command
 * @req: scatterlist entry for ELS ADISC request
 * @resp: scatterlist entry for ELS ADISC response
 * @adisc_req: ELS ADISC request data
 * @adisc_resp: ELS ADISC response data
 */
struct zfcp_fc_els_adisc {
	struct zfcp_fsf_ct_els els;
	struct scatterlist req;
	struct scatterlist resp;
	struct fc_els_adisc adisc_req;
	struct fc_els_adisc adisc_resp;
};

/**
 * enum zfcp_fc_wka_status - FC WKA port status in zfcp
 * @ZFCP_FC_WKA_PORT_OFFLINE: Port is closed and not in use
 * @ZFCP_FC_WKA_PORT_CLOSING: The FSF "close port" request is pending
 * @ZFCP_FC_WKA_PORT_OPENING: The FSF "open port" request is pending
 * @ZFCP_FC_WKA_PORT_ONLINE: The port is open and the port handle is valid
 */
enum zfcp_fc_wka_status {
	ZFCP_FC_WKA_PORT_OFFLINE,
	ZFCP_FC_WKA_PORT_CLOSING,
	ZFCP_FC_WKA_PORT_OPENING,
	ZFCP_FC_WKA_PORT_ONLINE,
};

/**
 * struct zfcp_fc_wka_port - representation of well-known-address (WKA) FC port
 * @adapter: Pointer to adapter structure this WKA port belongs to
 * @completion_wq: Wait for completion of open/close command
 * @status: Current status of WKA port
 * @refcount: Reference count to keep port open as long as it is in use
 * @d_id: FC destination id or well-known-address
 * @handle: FSF handle for the open WKA port
 * @mutex: Mutex used during opening/closing state changes
 * @work: For delaying the closing of the WKA port
 */
struct zfcp_fc_wka_port {
	struct zfcp_adapter	*adapter;
	wait_queue_head_t	completion_wq;
	enum zfcp_fc_wka_status	status;
	atomic_t		refcount;
	u32			d_id;
	u32			handle;
	struct mutex		mutex;
	struct delayed_work	work;
};

/**
 * struct zfcp_fc_wka_ports - Data structures for FC generic services
 * @ms: FC Management service
 * @ts: FC time service
 * @ds: FC directory service
 * @as: FC alias service
 */
struct zfcp_fc_wka_ports {
	struct zfcp_fc_wka_port ms;
	struct zfcp_fc_wka_port ts;
	struct zfcp_fc_wka_port ds;
	struct zfcp_fc_wka_port as;
};

/**
 * zfcp_fc_scsi_to_fcp - setup FCP command with data from scsi_cmnd
 * @fcp: fcp_cmnd to setup
 * @scsi: scsi_cmnd where to get LUN, task attributes/flags and CDB
 */
static inline
void zfcp_fc_scsi_to_fcp(struct fcp_cmnd *fcp, struct scsi_cmnd *scsi)
{
	char tag[2];

	int_to_scsilun(scsi->device->lun, (struct scsi_lun *) &fcp->fc_lun);

	if (scsi_populate_tag_msg(scsi, tag)) {
		switch (tag[0]) {
		case MSG_ORDERED_TAG:
			fcp->fc_pri_ta |= FCP_PTA_ORDERED;
			break;
		case MSG_SIMPLE_TAG:
			fcp->fc_pri_ta |= FCP_PTA_SIMPLE;
			break;
		};
	} else
		fcp->fc_pri_ta = FCP_PTA_SIMPLE;

	if (scsi->sc_data_direction == DMA_FROM_DEVICE)
		fcp->fc_flags |= FCP_CFL_RDDATA;
	if (scsi->sc_data_direction == DMA_TO_DEVICE)
		fcp->fc_flags |= FCP_CFL_WRDATA;

	memcpy(fcp->fc_cdb, scsi->cmnd, scsi->cmd_len);

	fcp->fc_dl = scsi_bufflen(scsi);

	if (scsi_get_prot_type(scsi) == SCSI_PROT_DIF_TYPE1)
		fcp->fc_dl += fcp->fc_dl / scsi->device->sector_size * 8;
}

/**
 * zfcp_fc_fcp_tm - setup FCP command as task management command
 * @fcp: fcp_cmnd to setup
 * @dev: scsi_device where to send the task management command
 * @tm: task management flags to setup tm command
 */
static inline
void zfcp_fc_fcp_tm(struct fcp_cmnd *fcp, struct scsi_device *dev, u8 tm_flags)
{
	int_to_scsilun(dev->lun, (struct scsi_lun *) &fcp->fc_lun);
	fcp->fc_tm_flags |= tm_flags;
}

/**
 * zfcp_fc_evap_fcp_rsp - evaluate FCP RSP IU and update scsi_cmnd accordingly
 * @fcp_rsp: FCP RSP IU to evaluate
 * @scsi: SCSI command where to update status and sense buffer
 */
static inline
void zfcp_fc_eval_fcp_rsp(struct fcp_resp_with_ext *fcp_rsp,
			  struct scsi_cmnd *scsi)
{
	struct fcp_resp_rsp_info *rsp_info;
	char *sense;
	u32 sense_len, resid;
	u8 rsp_flags;

	set_msg_byte(scsi, COMMAND_COMPLETE);
	scsi->result |= fcp_rsp->resp.fr_status;

	rsp_flags = fcp_rsp->resp.fr_flags;

	if (unlikely(rsp_flags & FCP_RSP_LEN_VAL)) {
		rsp_info = (struct fcp_resp_rsp_info *) &fcp_rsp[1];
		if (rsp_info->rsp_code == FCP_TMF_CMPL)
			set_host_byte(scsi, DID_OK);
		else {
			set_host_byte(scsi, DID_ERROR);
			return;
		}
	}

	if (unlikely(rsp_flags & FCP_SNS_LEN_VAL)) {
		sense = (char *) &fcp_rsp[1];
		if (rsp_flags & FCP_RSP_LEN_VAL)
			sense += fcp_rsp->ext.fr_sns_len;
		sense_len = min(fcp_rsp->ext.fr_sns_len,
				(u32) SCSI_SENSE_BUFFERSIZE);
		memcpy(scsi->sense_buffer, sense, sense_len);
	}

	if (unlikely(rsp_flags & FCP_RESID_UNDER)) {
		resid = fcp_rsp->ext.fr_resid;
		scsi_set_resid(scsi, resid);
		if (scsi_bufflen(scsi) - resid < scsi->underflow &&
		     !(rsp_flags & FCP_SNS_LEN_VAL) &&
		     fcp_rsp->resp.fr_status == SAM_STAT_GOOD)
			set_host_byte(scsi, DID_ERROR);
	}
}

#endif
