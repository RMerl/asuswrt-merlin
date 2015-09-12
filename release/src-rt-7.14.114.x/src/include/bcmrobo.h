/*
 * RoboSwitch setup functions
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcmrobo.h 523374 2014-12-30 01:42:48Z $
 */

#ifndef _bcm_robo_h_
#define _bcm_robo_h_

#define	DEVID5325	0x25	/* 5325 (Not really but we fake it) */
#define	DEVID5395	0x95	/* 5395 */
#define	DEVID5397	0x97	/* 5397 */
#define	DEVID5398	0x98	/* 5398 */
#define	DEVID53115	0x3115	/* 53115 */
#define	DEVID53125	0x3125	/* 53125 */

/*
 * MODELID:
 * 0x53010: BCM4707, Select Low SKU device if SKU ID[1:0] = 01.
 * 0x53011: BCM4708, Select Middle SKU device if SKU ID[1:0] = 10.
 * 0x53012: BCM4709, Select High SKU device if SKU ID[1:0] = 00.
 * Note: The SKU ID[1:0] is loaded from OTP configuration data.
 */
#define DEVID53010	0x53010	/* 53010 */
#define DEVID53011	0x53011	/* 53011 */
#define DEVID53012	0x53012	/* 53012 */
#define DEVID53018	0x53018	/* 53018 */
#define DEVID53019	0x53019	/* 53019 */
#define DEVID53030	0x53030	/* 53030 */
#define ROBO_IS_BCM5301X(id) ((id) == DEVID53010 || (id) == DEVID53011 || (id) == DEVID53012 || \
(id) == DEVID53018 || (id) == DEVID53019 || (id) == DEVID53030)

/* Power save duty cycle times */
#define MAX_NO_PHYS		5
#define PWRSAVE_SLEEP_TIME	12
#define PWRSAVE_WAKE_TIME	3

/* Power save modes for the switch */
#define ROBO_PWRSAVE_NORMAL 		0
#define ROBO_PWRSAVE_AUTO		1
#define ROBO_PWRSAVE_MANUAL		2
#define ROBO_PWRSAVE_AUTO_MANUAL 	3

#define ROBO_IS_PWRSAVE_MANUAL(r) ((r)->pwrsave_mode_manual)
#define ROBO_IS_PWRSAVE_AUTO(r) ((r)->pwrsave_mode_auto)

/* SRAB interface */
/* Access switch registers through SRAB (Switch Register Access Bridge) */
#define REG_VERSION_ID		0x40
#define REG_CTRL_PORT0_GMIIPO	0x58	/* 53012: GMII Port0 Override register */
#define REG_CTRL_PORT1_GMIIPO	0x59	/* 53012: GMII Port1 Override register */
#define REG_CTRL_PORT2_GMIIPO	0x5a	/* 53012: GMII Port2 Override register */
#define REG_CTRL_PORT3_GMIIPO	0x5b	/* 53012: GMII Port3 Override register */
#define REG_CTRL_PORT4_GMIIPO	0x5c	/* 53012: GMII Port4 Override register */
#define REG_CTRL_PORT5_GMIIPO	0x5d	/* 53012: GMII Port5 Override register */
#define REG_CTRL_PORT7_GMIIPO	0x5f	/* 53012: GMII Port7 Override register */

/* Command and status register of the SRAB */
#define CFG_F_sra_rst_MASK		(1 << 2)
#define CFG_F_sra_write_MASK		(1 << 1)
#define CFG_F_sra_gordyn_MASK		(1 << 0)
#define CFG_F_sra_page_R		24
#define CFG_F_sra_offset_R		16

/* Switch interface controls */
#define CFG_F_sw_init_done_MASK		(1 << 6)
#define CFG_F_rcareq_MASK		(1 << 3)
#define CFG_F_rcagnt_MASK		(1 << 4)

#ifdef ETAGG
#if defined(BCMFA) || defined(BCMAGG)
/* BCM header registrants */
#define BHDR_FA			(1 << 0)
#define BHDR_AGG		(1 << 1)
#define BHDR_REGISTRANTS	(BHDR_FA | BHDR_AGG)

typedef enum bhdr_ports {
	BHDR_PORT8,
	BHDR_PORT5,
	BHDR_PORT7,
	MAX_BHDR_PORTS
} bhdr_ports_t;
#endif /* BCMFA || BCMAGG */
#define MAX_AGG_GROUP	2
#endif	/* ETAGG */

#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

typedef volatile struct {
	uint32	PAD[11];
	uint32	cmdstat;	/* 0x2c, command and status register of the SRAB */
	uint32	wd_h;		/* 0x30, high order word of write data to switch registe */
	uint32	wd_l;		/* 0x34, low order word of write data to switch registe */
	uint32	rd_h;		/* 0x38, high order word of read data from switch register */
	uint32	rd_l;		/* 0x3c, low order word of read data from switch register */
	uint32	ctrls;		/* 0x40, switch interface controls */
	uint32	intr;		/* 0x44, the register captures interrupt pulses from the switch */
} srabregs_t;

/* Forward declaration */
typedef struct robo_info_s robo_info_t;

/* Device access/config oprands */
typedef struct {
	/* low level routines */
	void (*enable_mgmtif)(robo_info_t *robo);	/* enable mgmt i/f, optional */
	void (*disable_mgmtif)(robo_info_t *robo);	/* disable mgmt i/f, optional */
	int (*write_reg)(robo_info_t *robo, uint8 page, uint8 reg, void *val, int len);
	int (*read_reg)(robo_info_t *robo, uint8 page, uint8 reg, void *val, int len);
	/* description */
	char *desc;
} dev_ops_t;


typedef	uint16 (*miird_f)(void *h, int add, int off);
typedef	void (*miiwr_f)(void *h, int add, int off, uint16 val);

/* Private state per RoboSwitch */
struct robo_info_s {
	si_t	*sih;			/* SiliconBackplane handle */
	char	*vars;			/* nvram variables handle */
	void	*h;			/* dev handle */
	uint32	devid;			/* Device id for the switch */
	uint32	corerev;		/* Core rev of internal switch */

	dev_ops_t *ops;			/* device ops */
	uint8	page;			/* current page */

	/* SPI */
	uint32	ss, sck, mosi, miso;	/* GPIO mapping */

	/* MII */
	miird_f	miird;
	miiwr_f	miiwr;

	/* SRAB */
	srabregs_t *srabregs;

	uint16	prev_status;		/* link status of switch ports */
	uint32	pwrsave_mode_manual; 	/* bitmap of ports in manual power save */
	uint32	pwrsave_mode_auto; 	/* bitmap of ports in auto power save mode */
	uint32	pwrsave_sleep_time;	/* sleep time for manual power save mode */
	uint32	pwrsave_wake_time;	/* wakeup time for manual power save mode */
	uint8	pwrsave_phys;		/* Phys that can be put into power save mode */
	uint8	pwrsave_mode_phys[MAX_NO_PHYS];         /* Power save mode on the switch */
	bool	eee_status;
#ifdef PLC
	/* PLC */
	bool	plc_hw;			/* PLC chip */
#endif /* PLC */
#ifdef BCMFA
	int		aux_pid;
#endif /* BCMFA */
#ifdef ETAGG
#if defined(BCMFA) || defined(BCMAGG)
	uint8	bhdr_registered[MAX_BHDR_PORTS];	/* registered for using bcm header */
#endif /* BCMFA || BCMAGG */
#endif	/* ETAGG */
};

/* Power Save mode related functions */
extern int32 robo_power_save_mode_get(robo_info_t *robo, int32 phy);
extern int32 robo_power_save_mode_set(robo_info_t *robo, int32 mode, int32 phy);
extern void robo_power_save_mode_update(robo_info_t *robo);
extern int robo_power_save_mode(robo_info_t *robo, int mode, int phy);
extern int robo_power_save_toggle(robo_info_t *robo, int normal);

extern robo_info_t *bcm_robo_attach(si_t *sih, void *h, char *vars, miird_f miird, miiwr_f miiwr);
extern void bcm_robo_detach(robo_info_t *robo);
extern int bcm_robo_enable_device(robo_info_t *robo);
extern int bcm_robo_config_vlan(robo_info_t *robo, uint8 *mac_addr);
extern int bcm_robo_enable_switch(robo_info_t *robo);
extern int bcm_robo_flow_control(robo_info_t *robo, bool set);

#ifdef BCMDBG
extern void robo_dump_regs(robo_info_t *robo, struct bcmstrbuf *b);
#endif /* BCMDBG */

extern void robo_watchdog(robo_info_t *robo);
extern void robo_eee_advertise_init(robo_info_t *robo);

#ifdef PLC
extern void robo_plc_hw_init(robo_info_t *robo);
#endif /* PLC */

#ifdef ETAGG
#if defined(BCMFA) || defined(BCMAGG)
extern int32 robo_bhdr_register(robo_info_t *robo, bhdr_ports_t bhdr_port, uint8 registrant);
extern int32 robo_bhdr_unregister(robo_info_t *robo, bhdr_ports_t bhdr_port, uint8 registrant);
#endif /* BCMFA || BCMAGG */
#endif	/* ETAGG */

#ifdef BCMFA
extern void robo_fa_aux_init(robo_info_t *robo);
extern void robo_fa_aux_enable(robo_info_t *robo, bool enable);
extern void robo_fa_enable(robo_info_t *robo, bool on, bool bhdr);
#endif

#ifdef BCMAGG
extern int32 robo_update_agg_group(robo_info_t *robo, int group, uint32 portmap);
extern uint16 robo_get_linksts(robo_info_t *robo);
extern int32 robo_get_portsts(robo_info_t *robo, int32 pid, uint32 *link, uint32 *speed,
	uint32 *duplex);
extern int32 robo_permit_fwd_rsv_mcast(robo_info_t *robo);
#endif /* BCMAGG */
#endif /* _bcm_robo_h_ */
