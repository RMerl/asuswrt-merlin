/*
 * Copyright (c) 2005-2010 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 *
 * Linux driver for Brocade Fibre Channel Host Bus Adapter.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (GPL) Version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __BFAD_IM_H__
#define __BFAD_IM_H__

#include "bfa_fcs.h"

#define FCPI_NAME " fcpim"

#ifndef KOBJ_NAME_LEN
#define KOBJ_NAME_LEN           20
#endif

bfa_status_t bfad_im_module_init(void);
void bfad_im_module_exit(void);
bfa_status_t bfad_im_probe(struct bfad_s *bfad);
void bfad_im_probe_undo(struct bfad_s *bfad);
bfa_status_t bfad_im_port_new(struct bfad_s *bfad, struct bfad_port_s *port);
void bfad_im_port_delete(struct bfad_s *bfad, struct bfad_port_s *port);
void bfad_im_port_clean(struct bfad_im_port_s *im_port);
int  bfad_im_scsi_host_alloc(struct bfad_s *bfad,
		struct bfad_im_port_s *im_port, struct device *dev);
void bfad_im_scsi_host_free(struct bfad_s *bfad,
				struct bfad_im_port_s *im_port);

#define MAX_FCP_TARGET 1024
#define MAX_FCP_LUN 16384
#define BFAD_TARGET_RESET_TMO 60
#define BFAD_LUN_RESET_TMO 60
#define ScsiResult(host_code, scsi_code) (((host_code) << 16) | scsi_code)
#define BFA_QUEUE_FULL_RAMP_UP_TIME 120

/*
 * itnim flags
 */
#define IO_DONE_BIT			0

struct bfad_itnim_data_s {
	struct bfad_itnim_s *itnim;
};

struct bfad_im_port_s {
	struct bfad_s         *bfad;
	struct bfad_port_s    *port;
	struct work_struct port_delete_work;
	int             idr_id;
	u16        cur_scsi_id;
	u16	flags;
	struct list_head binding_list;
	struct Scsi_Host *shost;
	struct list_head itnim_mapped_list;
	struct fc_vport *fc_vport;
};

enum bfad_itnim_state {
	ITNIM_STATE_NONE,
	ITNIM_STATE_ONLINE,
	ITNIM_STATE_OFFLINE_PENDING,
	ITNIM_STATE_OFFLINE,
	ITNIM_STATE_TIMEOUT,
	ITNIM_STATE_FREE,
};

/*
 * Per itnim data structure
 */
struct bfad_itnim_s {
	struct list_head list_entry;
	struct bfa_fcs_itnim_s fcs_itnim;
	struct work_struct itnim_work;
	u32        flags;
	enum bfad_itnim_state state;
	struct bfad_im_s *im;
	struct bfad_im_port_s *im_port;
	struct bfad_rport_s *drv_rport;
	struct fc_rport *fc_rport;
	struct bfa_itnim_s *bfa_itnim;
	u16        scsi_tgt_id;
	u16        queue_work;
	unsigned long	last_ramp_up_time;
	unsigned long	last_queue_full_time;
};

enum bfad_binding_type {
	FCP_PWWN_BINDING = 0x1,
	FCP_NWWN_BINDING = 0x2,
	FCP_FCID_BINDING = 0x3,
};

struct bfad_fcp_binding {
	struct list_head list_entry;
	enum bfad_binding_type binding_type;
	u16        scsi_target_id;
	u32        fc_id;
	wwn_t           nwwn;
	wwn_t           pwwn;
};

struct bfad_im_s {
	struct bfad_s         *bfad;
	struct workqueue_struct *drv_workq;
	char            drv_workq_name[KOBJ_NAME_LEN];
};

struct Scsi_Host *bfad_scsi_host_alloc(struct bfad_im_port_s *im_port,
				struct bfad_s *);
bfa_status_t bfad_thread_workq(struct bfad_s *bfad);
void bfad_destroy_workq(struct bfad_im_s *im);
void bfad_fc_host_init(struct bfad_im_port_s *im_port);
void bfad_scsi_host_free(struct bfad_s *bfad,
				 struct bfad_im_port_s *im_port);
void bfad_ramp_up_qdepth(struct bfad_itnim_s *itnim,
				 struct scsi_device *sdev);
void bfad_handle_qfull(struct bfad_itnim_s *itnim, struct scsi_device *sdev);
struct bfad_itnim_s *bfad_get_itnim(struct bfad_im_port_s *im_port, int id);

extern struct scsi_host_template bfad_im_scsi_host_template;
extern struct scsi_host_template bfad_im_vport_template;
extern struct fc_function_template bfad_im_fc_function_template;
extern struct fc_function_template bfad_im_vport_fc_function_template;
extern struct scsi_transport_template *bfad_im_scsi_transport_template;
extern struct scsi_transport_template *bfad_im_scsi_vport_transport_template;

extern struct device_attribute *bfad_im_host_attrs[];
extern struct device_attribute *bfad_im_vport_attrs[];

irqreturn_t bfad_intx(int irq, void *dev_id);

/* Firmware releated */
#define BFAD_FW_FILE_CT_FC      "ctfw_fc.bin"
#define BFAD_FW_FILE_CT_CNA     "ctfw_cna.bin"
#define BFAD_FW_FILE_CB_FC      "cbfw_fc.bin"

u32 *bfad_get_firmware_buf(struct pci_dev *pdev);
u32 *bfad_read_firmware(struct pci_dev *pdev, u32 **bfi_image,
		u32 *bfi_image_size, char *fw_name);

static inline u32 *
bfad_load_fwimg(struct pci_dev *pdev)
{
	return bfad_get_firmware_buf(pdev);
}

static inline void
bfad_free_fwimg(void)
{
	if (bfi_image_ct_fc_size && bfi_image_ct_fc)
		vfree(bfi_image_ct_fc);
	if (bfi_image_ct_cna_size && bfi_image_ct_cna)
		vfree(bfi_image_ct_cna);
	if (bfi_image_cb_fc_size && bfi_image_cb_fc)
		vfree(bfi_image_cb_fc);
}
#endif
