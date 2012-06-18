/*
 * RoboSwitch setup functions
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
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
 * $Id: bcmrobo.h,v 13.13 2009-08-14 02:45:34 Exp $
 */

#ifndef _bcm_robo_h_
#define _bcm_robo_h_

#define	DEVID5325	0x25	/* 5325 (Not really but we fake it) */
#define	DEVID5395	0x95	/* 5395 */
#define	DEVID5397	0x97	/* 5397 */
#define	DEVID5398	0x98	/* 5398 */
#define	DEVID53115	0x3115	/* 53115 */
#define	DEVID53125	0x3125	/* 53125 */

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
	uint16	devid;			/* Device id for the switch */
	uint32	corerev;		/* Core rev of internal switch */

	dev_ops_t *ops;			/* device ops */
	uint8	page;			/* current page */

	/* SPI */
	uint32	ss, sck, mosi, miso;	/* GPIO mapping */

	/* MII */
	miird_f	miird;
	miiwr_f	miiwr;

	uint16	prev_status;		/* link status of switch ports */
	uint32	pwrsave_mode_manual; 	/* bitmap of ports in manual power save */
	uint32	pwrsave_mode_auto; 	/* bitmap of ports in auto power save mode */
	uint8	pwrsave_phys; 		/* Phys that can be put into power save mode */
	uint8	pwrsave_mode_phys[MAX_NO_PHYS];         /* Power save mode on the switch */   
#ifdef PLC
	/* PLC */
	bool	plc_hw;			/* PLC chip */
#endif /* PLC */
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

#ifdef BCMDBG
extern void robo_dump_regs(robo_info_t *robo, struct bcmstrbuf *b);
#endif /* BCMDBG */

#ifdef PLC
extern void robo_plc_hw_init(robo_info_t *robo);
#endif /* PLC */

#endif /* _bcm_robo_h_ */
