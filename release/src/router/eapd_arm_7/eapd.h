/*
 * Broadcom EAP dispatcher (EAPD) module include file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: eapd.h 430059 2013-10-17 01:54:43Z $
 */

#ifndef _EAPD_H_
#define _EAPD_H_

#include <typedefs.h>
#include <proto/ethernet.h>
#include <wlioctl.h>

#ifndef EAPD_WKSP_AUTO_CONFIG
#define EAPD_WKSP_AUTO_CONFIG	1
#endif

/* Message levels */
#define EAPD_ERROR_VAL		0x00000001
#define EAPD_INFO_VAL		0x00000002

extern uint eapd_msg_level;

#define EAPDBANNER(fmt, arg...)	do { \
		printf(" EAPD>> %s(%d): "fmt, __FUNCTION__, __LINE__ , ##arg);} while (0)

#define EAPD_ERROR(fmt, arg...)
#define EAPD_INFO(fmt, arg...)
#define EAPD_PRINT(fmt, arg...)	printf(fmt , ##arg)

#define EAPD_WKSP_FLAG_SHUTDOWN			0x1
#define EAPD_WKSP_FLAG_DUMP			0x2

#define EAPD_WKSP_RECV_DATA_MAX_LEN		4096


#define EAPD_WKSP_MIN_CMD_LINE_ARGS		16
#define EAPD_WKSP_MAX_CMD_LINE_ARGS		128
#define EAPD_WKSP_MAX_NO_BRIDGE			256
#define EAPD_WKSP_MAX_NO_BRCM			58
#define EAPD_WKSP_MAX_NO_IFNAMES		66

#define EAPD_WKSP_MAX_SUPPLICANTS		128
/* Supplicant cache */
#define EAPD_PAE_HASH(ea) \
((((unsigned char *) ea)[3] ^ ((unsigned char *) ea)[4] ^ ((unsigned char *) ea)[5]) & \
(EAPD_WKSP_MAX_SUPPLICANTS - 1))

typedef struct eapd_sta {
	bool		used;		/* flags use of item */
	time_t		last_use;	/* use timestamp */
	struct eapd_sta	*next;
	struct ether_addr	ea;	/* STA's ethernet address */
	struct ether_addr	bssid;	/* wl if hwaddr which sta comes in */
	char		ifname[IFNAMSIZ];
	ushort		pae_state;
	ushort		pae_id;
	uint32		mode;		/* Authentication mode */
	uint8		eapol_version;	/* eapol version */
} eapd_sta_t;

typedef struct eapd_socket {
	char		ifname[IFNAMSIZ];
	int		drvSocket;		/* raw socket to communicate with driver */
	int		ifindex;
	int		inuseCount;
	int		flag;
} eapd_brcm_socket_t, eapd_preauth_socket_t;

typedef struct eapd_cb {
	char			ifname[IFNAMSIZ];
	int			flags;
	eapd_brcm_socket_t	*brcmSocket;
	eapd_preauth_socket_t	preauthSocket;	/* only need by NAS */
	struct eapd_cb		*next;
} eapd_cb_t;

typedef struct eapd_app {
	char	ifnames[IFNAMSIZ * EAPD_WKSP_MAX_NO_IFNAMES]; /* interface names */
	int	appSocket; /* loopback socket to communicate with application */
	uchar	bitvec[WL_EVENTING_MASK_LEN]; /* for each application which need brcmevent */
	eapd_cb_t	*cb; /* for each interface which running application */
} eapd_app_t, eapd_wps_t, eapd_nas_t, eapd_ses_t, eapd_wai_t, eapd_dcs_t, eapd_mevent_t, eapd_bsd_t;

typedef struct eapd_wksp {
	uchar			packet[EAPD_WKSP_RECV_DATA_MAX_LEN];
	int			brcmSocketCount;
	eapd_brcm_socket_t	brcmSocket[EAPD_WKSP_MAX_NO_BRCM];
	eapd_sta_t		sta[EAPD_WKSP_MAX_SUPPLICANTS];
	eapd_sta_t		*sta_hashed[EAPD_WKSP_MAX_SUPPLICANTS];
	eapd_wps_t		wps;
	eapd_nas_t		nas;
	eapd_ses_t		ses;
	eapd_wai_t		wai;
	eapd_dcs_t		dcs;
	eapd_mevent_t		mevent;
	eapd_bsd_t		bsd;
	int			flags;
	fd_set			fdset;
	int			fdmax;
	int			difSocket; /* socket to receive dynamic interface events */
} eapd_wksp_t;

typedef enum {
	EAPD_SEARCH_ONLY = 0,
	EAPD_SEARCH_ENTER
} eapd_lookup_mode_t;

typedef enum {
	EAPD_STA_MODE_UNKNOW = 0,
	EAPD_STA_MODE_WPS,
	EAPD_STA_MODE_WPS_ENR,
	EAPD_STA_MODE_SES,
	EAPD_STA_MODE_NAS,
	EAPD_STA_MODE_NAS_PSK,
	EAPD_STA_MODE_WAI
} eapd_sta_mode_t;

typedef enum {
	EAPD_APP_UNKNOW = 0,
	EAPD_APP_WPS,
	EAPD_APP_SES,
	EAPD_APP_NAS,
	EAPD_APP_WAI,
	EAPD_APP_DCS,
	EAPD_APP_MEVENT,
	EAPD_APP_BSD
} eapd_app_mode_t;

/* PAE states */
typedef enum {
	EAPD_INITIALIZE = 0,
	EAPD_IDENTITY
} eapd_pae_state_t;

/* EAPD interface application capability */
#define EAPD_CAP_SES	0x1
#define EAPD_CAP_NAS	0x2
#define EAPD_CAP_WPS	0x4
#define EAPD_CAP_WAI	0x8
#define EAPD_CAP_DCS	0x10
#define EAPD_CAP_MEVENT	0x20
#define EAPD_CAP_BSD	0x40

/* Apps */
int wps_app_init(eapd_wksp_t *nwksp);
int wps_app_deinit(eapd_wksp_t *nwksp);
int wps_app_monitor_sendup(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from);
#if EAPD_WKSP_AUTO_CONFIG
int wps_app_enabled(char *name);
#endif
void wps_app_set_eventmask(eapd_app_t *app);
int wps_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from);
void wps_app_recv_handler(eapd_wksp_t *nwksp, char *wlifname, eapd_cb_t *from,
	uint8 *pData, int *pLen, struct ether_addr *ap_ea);

int ses_app_init(eapd_wksp_t *nwksp);
int ses_app_deinit(eapd_wksp_t *nwksp);
int ses_app_sendup(eapd_wksp_t *nwksp, uint8 *pData, int pLen, char *fromlan);
#if EAPD_WKSP_AUTO_CONFIG
int ses_app_enabled(char *name);
#endif
void ses_app_set_eventmask(eapd_app_t *app);
int ses_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from);
void ses_app_recv_handler(eapd_wksp_t *nwksp, char *wlifname, eapd_cb_t *from,
	uint8 *pData, int *pLen);

int nas_app_init(eapd_wksp_t *nwksp);
int nas_app_deinit(eapd_wksp_t *nwksp);
int nas_app_sendup(eapd_wksp_t *nwksp, uint8 *pData, int pLen, char *fromlan);
#if EAPD_WKSP_AUTO_CONFIG
int nas_app_enabled(char *name);
#endif
void nas_app_set_eventmask(eapd_app_t *app);
int nas_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from);
void nas_app_recv_handler(eapd_wksp_t *nwksp, char *wlifname, eapd_cb_t *from,
	uint8 *pData, int *pLen);
int nas_open_dif_sockets(eapd_wksp_t *nwksp, eapd_cb_t *cb);
void nas_close_dif_sockets(eapd_wksp_t *nwksp, eapd_cb_t *cb);

int wai_app_init(eapd_wksp_t *nwksp);
int wai_app_deinit(eapd_wksp_t *nwksp);
int wai_app_sendup(eapd_wksp_t *nwksp, uint8 *pData, int pLen, char *fromlan);
#if EAPD_WKSP_AUTO_CONFIG
int wai_app_enabled(char *name);
#endif
void wai_app_set_eventmask(eapd_app_t *app);
int wai_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from);
void wai_app_recv_handler(eapd_wksp_t *nwksp, eapd_cb_t *from, uint8 *pData, int *pLen);

#ifdef BCM_DCS
int dcs_app_init(eapd_wksp_t *nwksp);
int dcs_app_deinit(eapd_wksp_t *nwksp);
int dcs_app_sendup(eapd_wksp_t *nwksp, uint8 *pData, int pLen, char *fromlan);
#if EAPD_WKSP_AUTO_CONFIG
int dcs_app_enabled(char *name);
#endif
void dcs_app_set_eventmask(eapd_app_t *app);
int dcs_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from);
void dcs_app_recv_handler(eapd_wksp_t *nwksp, eapd_cb_t *from,
	uint8 *pData, int *pLen);
#endif /* BCM_DCS */

#ifdef BCM_MEVENT
int mevent_app_init(eapd_wksp_t *nwksp);
int mevent_app_deinit(eapd_wksp_t *nwksp);
int mevent_app_sendup(eapd_wksp_t *nwksp, uint8 *pData, int pLen, char *fromlan);
#if EAPD_WKSP_AUTO_CONFIG
int mevent_app_enabled(char *name);
#endif
void mevent_app_set_eventmask(eapd_app_t *app);
int mevent_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from);
void mevent_app_recv_handler(eapd_wksp_t *nwksp, eapd_cb_t *from,
	uint8 *pData, int *pLen);
#endif /* BCM_MEVENT */

#ifdef BCM_BSD
int bsd_app_init(eapd_wksp_t *nwksp);
int bsd_app_deinit(eapd_wksp_t *nwksp);
int bsd_app_sendup(eapd_wksp_t *nwksp, uint8 *pData, int pLen, char *fromlan);
#if EAPD_WKSP_AUTO_CONFIG
int bsd_app_enabled(char *name);
#endif
void bsd_app_set_eventmask(eapd_app_t *app);
int bsd_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from);
void bsd_app_recv_handler(eapd_wksp_t *nwksp, eapd_cb_t *from,
	uint8 *pData, int *pLen);
#endif /* BCM_BSD */

/* OS independent function */
void eapd_wksp_display_usage(void);
eapd_wksp_t * eapd_wksp_alloc_workspace(void);
int eapd_wksp_auto_config(eapd_wksp_t *nwksp);
int eapd_wksp_parse_cmd(int argc, char *argv[], eapd_wksp_t *nwksp);
int eapd_wksp_init(eapd_wksp_t *nwksp);
void eapd_wksp_dispatch(eapd_wksp_t *nwksp);
int eapd_wksp_deinit(eapd_wksp_t *nwksp);
void eapd_wksp_free_workspace(eapd_wksp_t * nwksp);
int eapd_wksp_main_loop(eapd_wksp_t *nwksp);
void eapd_wksp_cleanup(eapd_wksp_t *nwksp);
void eapd_wksp_clear_inited(void);
int eapd_wksp_is_inited(void);

eapd_sta_t* sta_lookup(eapd_wksp_t *nwksp, struct ether_addr *sta_ea, struct ether_addr *bssid_ea,
	char *ifname, eapd_lookup_mode_t mode);
void sta_remove(eapd_wksp_t *nwksp, eapd_sta_t *sta);

eapd_brcm_socket_t* eapd_add_brcm(eapd_wksp_t *nwksp, char *ifname);
int eapd_del_brcm(eapd_wksp_t *nwksp, eapd_brcm_socket_t *sock);
eapd_brcm_socket_t* eapd_find_brcm(eapd_wksp_t *nwksp, char *ifname);

void eapd_brcm_recv_handler(eapd_wksp_t *nwksp, eapd_brcm_socket_t *from, uint8 *pData, int *pLen);
extern void eapd_preauth_recv_handler(eapd_wksp_t *nwksp, char *from, uint8 *pData, int *pLen);

/* OS dependent function */
void eapd_eapol_canned_send(eapd_wksp_t *nwksp, struct eapd_socket *Socket, eapd_sta_t *sta,
                                                           unsigned char code, unsigned char type);
void eapd_message_send(eapd_wksp_t *nwksp, struct eapd_socket *Socket, uint8 *pData, int pLen);
int eapd_brcm_open(eapd_wksp_t *nwksp, eapd_brcm_socket_t *sock);
int eapd_brcm_close(int drvSocket);
int eapd_preauth_open(eapd_wksp_t *nwksp, eapd_preauth_socket_t *sock);
int eapd_preauth_close(int drvSocket);
int eapd_safe_get_conf(char *outval, int outval_size, char *name);
size_t eapd_message_read(int fd, void *buf, size_t nbytes);
#endif /* _EAPD_H_ */
