/*
 * Copyright (c) 2005-2009 Brocade Communications Systems, Inc.
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

#ifndef __BFA_IOC_H__
#define __BFA_IOC_H__

#include <cs/bfa_sm.h>
#include <bfi/bfi.h>
#include <bfi/bfi_ioc.h>
#include <bfi/bfi_boot.h>
#include <bfa_timer.h>

/**
 * PCI device information required by IOC
 */
struct bfa_pcidev_s {
	int             pci_slot;
	u8         pci_func;
	u16	device_id;
	bfa_os_addr_t   pci_bar_kva;
};

/**
 * Structure used to remember the DMA-able memory block's KVA and Physical
 * Address
 */
struct bfa_dma_s {
	void		*kva;	/*! Kernel virtual address	*/
	u64	pa;	/*! Physical address		*/
};

#define BFA_DMA_ALIGN_SZ	256
#define BFA_ROUNDUP(_l, _s)	(((_l) + ((_s) - 1)) & ~((_s) - 1))



#define bfa_dma_addr_set(dma_addr, pa)	\
		__bfa_dma_addr_set(&dma_addr, (u64)pa)

static inline void
__bfa_dma_addr_set(union bfi_addr_u *dma_addr, u64 pa)
{
	dma_addr->a32.addr_lo = (u32) pa;
	dma_addr->a32.addr_hi = (u32) (bfa_os_u32(pa));
}


#define bfa_dma_be_addr_set(dma_addr, pa)	\
		__bfa_dma_be_addr_set(&dma_addr, (u64)pa)
static inline void
__bfa_dma_be_addr_set(union bfi_addr_u *dma_addr, u64 pa)
{
	dma_addr->a32.addr_lo = (u32) bfa_os_htonl(pa);
	dma_addr->a32.addr_hi = (u32) bfa_os_htonl(bfa_os_u32(pa));
}

struct bfa_ioc_regs_s {
	bfa_os_addr_t   hfn_mbox_cmd;
	bfa_os_addr_t   hfn_mbox;
	bfa_os_addr_t   lpu_mbox_cmd;
	bfa_os_addr_t   lpu_mbox;
	bfa_os_addr_t   pss_ctl_reg;
	bfa_os_addr_t   pss_err_status_reg;
	bfa_os_addr_t   app_pll_fast_ctl_reg;
	bfa_os_addr_t   app_pll_slow_ctl_reg;
	bfa_os_addr_t   ioc_sem_reg;
	bfa_os_addr_t   ioc_usage_sem_reg;
	bfa_os_addr_t   ioc_init_sem_reg;
	bfa_os_addr_t   ioc_usage_reg;
	bfa_os_addr_t   host_page_num_fn;
	bfa_os_addr_t   heartbeat;
	bfa_os_addr_t   ioc_fwstate;
	bfa_os_addr_t   ll_halt;
	bfa_os_addr_t   err_set;
	bfa_os_addr_t   shirq_isr_next;
	bfa_os_addr_t   shirq_msk_next;
	bfa_os_addr_t   smem_page_start;
	u32	smem_pg0;
};

#define bfa_reg_read(_raddr)	bfa_os_reg_read(_raddr)
#define bfa_reg_write(_raddr, _val)	bfa_os_reg_write(_raddr, _val)
#define bfa_mem_read(_raddr, _off)	bfa_os_mem_read(_raddr, _off)
#define bfa_mem_write(_raddr, _off, _val)	\
					bfa_os_mem_write(_raddr, _off, _val)
/**
 * IOC Mailbox structures
 */
struct bfa_mbox_cmd_s {
	struct list_head		qe;
	u32	msg[BFI_IOC_MSGSZ];
};

/**
 * IOC mailbox module
 */
typedef void (*bfa_ioc_mbox_mcfunc_t)(void *cbarg, struct bfi_mbmsg_s *m);
struct bfa_ioc_mbox_mod_s {
	struct list_head	cmd_q;		/*  pending mbox queue	*/
	int		nmclass;	/*  number of handlers */
	struct {
		bfa_ioc_mbox_mcfunc_t	cbfn;	/*  message handlers	*/
		void			*cbarg;
	} mbhdlr[BFI_MC_MAX];
};

/**
 * IOC callback function interfaces
 */
typedef void (*bfa_ioc_enable_cbfn_t)(void *bfa, enum bfa_status status);
typedef void (*bfa_ioc_disable_cbfn_t)(void *bfa);
typedef void (*bfa_ioc_hbfail_cbfn_t)(void *bfa);
typedef void (*bfa_ioc_reset_cbfn_t)(void *bfa);
struct bfa_ioc_cbfn_s {
	bfa_ioc_enable_cbfn_t	enable_cbfn;
	bfa_ioc_disable_cbfn_t	disable_cbfn;
	bfa_ioc_hbfail_cbfn_t	hbfail_cbfn;
	bfa_ioc_reset_cbfn_t	reset_cbfn;
};

/**
 * Heartbeat failure notification queue element.
 */
struct bfa_ioc_hbfail_notify_s {
	struct list_head		qe;
	bfa_ioc_hbfail_cbfn_t	cbfn;
	void			*cbarg;
};

/**
 * Initialize a heartbeat failure notification structure
 */
#define bfa_ioc_hbfail_init(__notify, __cbfn, __cbarg) do {	\
	(__notify)->cbfn = (__cbfn);      \
	(__notify)->cbarg = (__cbarg);      \
} while (0)

struct bfa_ioc_s {
	bfa_fsm_t		fsm;
	struct bfa_s		*bfa;
	struct bfa_pcidev_s	pcidev;
	struct bfa_timer_mod_s 	*timer_mod;
	struct bfa_timer_s 	ioc_timer;
	struct bfa_timer_s 	sem_timer;
	u32		hb_count;
	u32		retry_count;
	struct list_head		hb_notify_q;
	void			*dbg_fwsave;
	int			dbg_fwsave_len;
	bfa_boolean_t		dbg_fwsave_once;
	enum bfi_mclass		ioc_mc;
	struct bfa_ioc_regs_s 	ioc_regs;
	struct bfa_trc_mod_s	*trcmod;
	struct bfa_aen_s	*aen;
	struct bfa_log_mod_s	*logm;
	struct bfa_ioc_drv_stats_s	stats;
	bfa_boolean_t		auto_recover;
	bfa_boolean_t		fcmode;
	bfa_boolean_t		ctdev;
	bfa_boolean_t		cna;
	bfa_boolean_t		pllinit;
	u8			port_id;

	struct bfa_dma_s	attr_dma;
	struct bfi_ioc_attr_s	*attr;
	struct bfa_ioc_cbfn_s	*cbfn;
	struct bfa_ioc_mbox_mod_s mbox_mod;
	struct bfa_ioc_hwif_s   *ioc_hwif;
};

struct bfa_ioc_hwif_s {
	bfa_status_t    (*ioc_pll_init) (struct bfa_ioc_s *ioc);
	bfa_boolean_t   (*ioc_firmware_lock)    (struct bfa_ioc_s *ioc);
	void            (*ioc_firmware_unlock)  (struct bfa_ioc_s *ioc);
	void		(*ioc_reg_init) (struct bfa_ioc_s *ioc);
	void		(*ioc_map_port) (struct bfa_ioc_s *ioc);
	void		(*ioc_isr_mode_set)     (struct bfa_ioc_s *ioc,
						bfa_boolean_t msix);
	void            (*ioc_notify_hbfail)    (struct bfa_ioc_s *ioc);
	void            (*ioc_ownership_reset)  (struct bfa_ioc_s *ioc);
};

#define bfa_ioc_pcifn(__ioc)		((__ioc)->pcidev.pci_func)
#define bfa_ioc_devid(__ioc)		((__ioc)->pcidev.device_id)
#define bfa_ioc_bar0(__ioc)		((__ioc)->pcidev.pci_bar_kva)
#define bfa_ioc_portid(__ioc)		((__ioc)->port_id)
#define bfa_ioc_fetch_stats(__ioc, __stats) \
		(((__stats)->drv_stats) = (__ioc)->stats)
#define bfa_ioc_clr_stats(__ioc)	\
		bfa_os_memset(&(__ioc)->stats, 0, sizeof((__ioc)->stats))
#define bfa_ioc_maxfrsize(__ioc)	((__ioc)->attr->maxfrsize)
#define bfa_ioc_rx_bbcredit(__ioc)	((__ioc)->attr->rx_bbcredit)
#define bfa_ioc_speed_sup(__ioc)	\
	BFI_ADAPTER_GETP(SPEED, (__ioc)->attr->adapter_prop)
#define bfa_ioc_get_nports(__ioc)       \
	BFI_ADAPTER_GETP(NPORTS, (__ioc)->attr->adapter_prop)

#define bfa_ioc_stats(_ioc, _stats)     ((_ioc)->stats._stats++)
#define BFA_IOC_FWIMG_MINSZ     (16 * 1024)
#define BFA_IOC_FWIMG_TYPE(__ioc)                                       \
	(((__ioc)->ctdev) ?                                             \
	 (((__ioc)->fcmode) ? BFI_IMAGE_CT_FC : BFI_IMAGE_CT_CNA) :     \
	 BFI_IMAGE_CB_FC)

#define BFA_IOC_FLASH_CHUNK_NO(off)             (off / BFI_FLASH_CHUNK_SZ_WORDS)
#define BFA_IOC_FLASH_OFFSET_IN_CHUNK(off)      (off % BFI_FLASH_CHUNK_SZ_WORDS)
#define BFA_IOC_FLASH_CHUNK_ADDR(chunkno)  (chunkno * BFI_FLASH_CHUNK_SZ_WORDS)

/**
 * IOC mailbox interface
 */
void bfa_ioc_mbox_queue(struct bfa_ioc_s *ioc, struct bfa_mbox_cmd_s *cmd);
void bfa_ioc_mbox_register(struct bfa_ioc_s *ioc,
		bfa_ioc_mbox_mcfunc_t *mcfuncs);
void bfa_ioc_mbox_isr(struct bfa_ioc_s *ioc);
void bfa_ioc_mbox_send(struct bfa_ioc_s *ioc, void *ioc_msg, int len);
void bfa_ioc_msgget(struct bfa_ioc_s *ioc, void *mbmsg);
void bfa_ioc_mbox_regisr(struct bfa_ioc_s *ioc, enum bfi_mclass mc,
		bfa_ioc_mbox_mcfunc_t cbfn, void *cbarg);

/**
 * IOC interfaces
 */
#define bfa_ioc_pll_init(__ioc) ((__ioc)->ioc_hwif->ioc_pll_init(__ioc))
#define bfa_ioc_isr_mode_set(__ioc, __msix)                     \
			((__ioc)->ioc_hwif->ioc_isr_mode_set(__ioc, __msix))
#define bfa_ioc_ownership_reset(__ioc)                          \
			((__ioc)->ioc_hwif->ioc_ownership_reset(__ioc))

void bfa_ioc_set_ct_hwif(struct bfa_ioc_s *ioc);
void bfa_ioc_set_cb_hwif(struct bfa_ioc_s *ioc);
void bfa_ioc_attach(struct bfa_ioc_s *ioc, void *bfa,
		struct bfa_ioc_cbfn_s *cbfn, struct bfa_timer_mod_s *timer_mod,
		struct bfa_trc_mod_s *trcmod,
		struct bfa_aen_s *aen, struct bfa_log_mod_s *logm);
void bfa_ioc_detach(struct bfa_ioc_s *ioc);
void bfa_ioc_pci_init(struct bfa_ioc_s *ioc, struct bfa_pcidev_s *pcidev,
		enum bfi_mclass mc);
u32 bfa_ioc_meminfo(void);
void bfa_ioc_mem_claim(struct bfa_ioc_s *ioc,  u8 *dm_kva, u64 dm_pa);
void bfa_ioc_enable(struct bfa_ioc_s *ioc);
void bfa_ioc_disable(struct bfa_ioc_s *ioc);
bfa_boolean_t bfa_ioc_intx_claim(struct bfa_ioc_s *ioc);

void bfa_ioc_boot(struct bfa_ioc_s *ioc, u32 boot_type, u32 boot_param);
void bfa_ioc_isr(struct bfa_ioc_s *ioc, struct bfi_mbmsg_s *msg);
void bfa_ioc_error_isr(struct bfa_ioc_s *ioc);
bfa_boolean_t bfa_ioc_is_operational(struct bfa_ioc_s *ioc);
bfa_boolean_t bfa_ioc_is_disabled(struct bfa_ioc_s *ioc);
bfa_boolean_t bfa_ioc_fw_mismatch(struct bfa_ioc_s *ioc);
bfa_boolean_t bfa_ioc_adapter_is_disabled(struct bfa_ioc_s *ioc);
void bfa_ioc_cfg_complete(struct bfa_ioc_s *ioc);
enum bfa_ioc_type_e bfa_ioc_get_type(struct bfa_ioc_s *ioc);
void bfa_ioc_get_adapter_serial_num(struct bfa_ioc_s *ioc, char *serial_num);
void bfa_ioc_get_adapter_fw_ver(struct bfa_ioc_s *ioc, char *fw_ver);
void bfa_ioc_get_adapter_optrom_ver(struct bfa_ioc_s *ioc, char *optrom_ver);
void bfa_ioc_get_adapter_model(struct bfa_ioc_s *ioc, char *model);
void bfa_ioc_get_adapter_manufacturer(struct bfa_ioc_s *ioc,
	char *manufacturer);
void bfa_ioc_get_pci_chip_rev(struct bfa_ioc_s *ioc, char *chip_rev);
enum bfa_ioc_state bfa_ioc_get_state(struct bfa_ioc_s *ioc);

void bfa_ioc_get_attr(struct bfa_ioc_s *ioc, struct bfa_ioc_attr_s *ioc_attr);
void bfa_ioc_get_adapter_attr(struct bfa_ioc_s *ioc,
		struct bfa_adapter_attr_s *ad_attr);
int bfa_ioc_debug_trcsz(bfa_boolean_t auto_recover);
void bfa_ioc_debug_memclaim(struct bfa_ioc_s *ioc, void *dbg_fwsave);
bfa_status_t bfa_ioc_debug_fwsave(struct bfa_ioc_s *ioc, void *trcdata,
		int *trclen);
void bfa_ioc_debug_fwsave_clear(struct bfa_ioc_s *ioc);
bfa_status_t bfa_ioc_debug_fwtrc(struct bfa_ioc_s *ioc, void *trcdata,
				 int *trclen);
u32 bfa_ioc_smem_pgnum(struct bfa_ioc_s *ioc, u32 fmaddr);
u32 bfa_ioc_smem_pgoff(struct bfa_ioc_s *ioc, u32 fmaddr);
void bfa_ioc_set_fcmode(struct bfa_ioc_s *ioc);
bfa_boolean_t bfa_ioc_get_fcmode(struct bfa_ioc_s *ioc);
void bfa_ioc_hbfail_register(struct bfa_ioc_s *ioc,
	struct bfa_ioc_hbfail_notify_s *notify);
bfa_boolean_t bfa_ioc_sem_get(bfa_os_addr_t sem_reg);
void bfa_ioc_sem_release(bfa_os_addr_t sem_reg);
void bfa_ioc_hw_sem_release(struct bfa_ioc_s *ioc);
void bfa_ioc_fwver_get(struct bfa_ioc_s *ioc,
			struct bfi_ioc_image_hdr_s *fwhdr);
bfa_boolean_t bfa_ioc_fwver_cmp(struct bfa_ioc_s *ioc,
			struct bfi_ioc_image_hdr_s *fwhdr);
void bfa_ioc_aen_post(struct bfa_ioc_s *ioc, enum bfa_ioc_aen_event event);

/*
 * bfa mfg wwn API functions
 */
wwn_t bfa_ioc_get_pwwn(struct bfa_ioc_s *ioc);
wwn_t bfa_ioc_get_nwwn(struct bfa_ioc_s *ioc);
mac_t bfa_ioc_get_mac(struct bfa_ioc_s *ioc);
wwn_t bfa_ioc_get_mfg_pwwn(struct bfa_ioc_s *ioc);
wwn_t bfa_ioc_get_mfg_nwwn(struct bfa_ioc_s *ioc);
mac_t bfa_ioc_get_mfg_mac(struct bfa_ioc_s *ioc);
u64 bfa_ioc_get_adid(struct bfa_ioc_s *ioc);

#endif /* __BFA_IOC_H__ */
