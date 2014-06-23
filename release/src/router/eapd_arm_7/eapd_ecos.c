/*
 * Ecos-specific portion of EAPD
 * (OS dependent file)
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: eapd_ecos.c 307770 2012-01-12 15:58:51Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <typedefs.h>
#include <unistd.h>
#include <ecos_oslib.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <cyg/io/file.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <proto/eap.h>
#include <bcmnvram.h>
#include <eapd.h>

static eapd_wksp_t *eapd_nwksp = NULL;

static cyg_handle_t	eapd_main_hdl;
static char		eapd_main_stack[8*1024];
static cyg_thread	eapd_thread;
static int _eapd_pid = 0;

static void
eapd_hup_hdlr(int sig)
{
	if (eapd_nwksp)
		eapd_nwksp->flags |= EAPD_WKSP_FLAG_SHUTDOWN;

	return;
}

void
eapd_dump_hdlr(int sig)
{
	if (eapd_nwksp)
		eapd_nwksp->flags |= EAPD_WKSP_FLAG_DUMP;

	return;
}

/* Send a canned EAPOL packet */
void
eapd_eapol_canned_send(eapd_wksp_t *nwksp, struct eapd_socket *Socket, eapd_sta_t *sta,
	unsigned char code, unsigned char type)
{
	eapol_header_t eapol;
	eap_header_t eap;
	int len;
	char *packet;
	char *ptr;

	memcpy(&eapol.eth.ether_dhost, &sta->ea, ETHER_ADDR_LEN);
	memcpy(&eapol.eth.ether_shost, &sta->bssid, ETHER_ADDR_LEN);

	eapol.eth.ether_type = htons(ETHER_TYPE_802_1X);
	eapol.version = sta->eapol_version;
	eapol.type = EAP_PACKET;
	/* With type, eap.type is used.  So, the legnth will be 4+1 */
	len = type ? (EAP_HEADER_LEN + 1) : EAP_HEADER_LEN;
	eapol.length = htons(len);

	eap.code = code;
	eap.id = sta->pae_id;
	eap.length = eapol.length;

	/* This is optional, if type is 0, this field won't be used. */
	eap.type = type;

	/* Allocate a buffer to send ? */
	packet = (char *)malloc(EAPOL_HEADER_LEN + len);
	if (packet == NULL)
		return;

	ptr = packet;
	memcpy(ptr, &eapol, EAPOL_HEADER_LEN);

	ptr += EAPOL_HEADER_LEN;
	memcpy(ptr, &eap, len);

	/* Write to device */
	write(Socket->drvSocket, packet, len + EAPOL_HEADER_LEN);

	free(packet);
	return;
}

void
eapd_message_send(eapd_wksp_t *nwksp, struct eapd_socket *Socket, uint8 *pData, int pLen)
{
	/* We should handle fragment here */
	write(Socket->drvSocket, pData, pLen);
}

int
eapd_brcm_open(eapd_wksp_t *nwksp, eapd_brcm_socket_t *sock)
{
	char devname[32];

	if (nwksp == NULL || sock == NULL) {
		EAPD_ERROR("Wrong arguments...\n");
		return -1;
	}

	sprintf(devname, "/dev/net/eapol/%s/%d", sock->ifname, ETHER_TYPE_BRCM);
	sock->drvSocket = open(devname, O_RDWR);
	if (sock->drvSocket < 0) {
		EAPD_ERROR("open socket error!!\n");
		return -1;
	}

	/* at least one use it */
	sock->inuseCount = 1;

	EAPD_INFO("%s: BRCM socket %d opened\n", sock->ifname, sock->drvSocket);

	return 0;
}

int
eapd_brcm_close(int drvSocket)
{
	close(drvSocket);
	return 0;
}

int
eapd_preauth_open(eapd_wksp_t *nwksp, eapd_preauth_socket_t *sock)
{
	char devname[32];

	if (nwksp == NULL || sock == NULL) {
		EAPD_ERROR("Wrong arguments...\n");
		return -1;
	}

	sprintf(devname, "/dev/net/eapol/%s/%d", sock->ifname, ETHER_TYPE_802_1X_PREAUTH);
	sock->drvSocket = open(devname, O_RDWR);
	if (sock->drvSocket < 0) {
		EAPD_ERROR("open socket error!!\n");
		return -1;
	}

	/* at least one use it */
	sock->inuseCount = 1;

	EAPD_INFO("%s: preauth socket %d opened\n", sock->ifname, sock->drvSocket);

	return 0;
}

int
eapd_preauth_close(int drvSocket)
{
	close(drvSocket);
	return 0;
}

/*
 * Configuration APIs
 */
int eapd_safe_get_conf(char *outval, int outval_size, char *name)
{
	char *val;

	if (name == NULL || outval == NULL) {
		if (outval)
			memset(outval, 0, outval_size);
		return -1;
	}

	val = nvram_safe_get(name);
	if (!strcmp(val, ""))
		memset(outval, 0, outval_size);
	else
		snprintf(outval, outval_size, "%s", val);
	return 0;
}

int eapd_main(void)
{
#ifdef BCMDBG
	char *dbg;

	/* get eapd_msg_level from nvram */
	if ((dbg = nvram_safe_get("eapd_dbg"))) {
		eapd_msg_level = (uint)strtoul(dbg, NULL, 0);
	}
#endif /* BCMDBG */

	EAPD_INFO("EAP Dispatch Start...\n");

	/* fill up EAPD task pid */
	_eapd_pid = oslib_pid();

	/* alloc eapd work space */
	if (!(eapd_nwksp = eapd_wksp_alloc_workspace())) {
		EAPD_ERROR("Unable to allocate wksp memory. Quitting...\n");
		return -1;
	}

	if (eapd_wksp_auto_config(eapd_nwksp)) {
		EAPD_ERROR("Unable to auto config. Quitting...\n");
		eapd_wksp_cleanup(eapd_nwksp);
		return -1;
	}

	/* run main loop to dispatch messages */
	eapd_wksp_main_loop(eapd_nwksp);

	return 0;
}

void
eapd_start(void)
{
	if (!_eapd_pid ||
	     !oslib_waitpid(_eapd_pid, NULL)) {
		cyg_thread_create(7,
		              (cyg_thread_entry_t *)eapd_main,
		              (cyg_addrword_t)NULL,
		              "EAPD",
		              eapd_main_stack,
		              sizeof(eapd_main_stack),
		              &eapd_main_hdl,
		              &eapd_thread);
		cyg_thread_resume(eapd_main_hdl);
		EAPD_PRINT("EAPD task started\n");
	}
}

void
eapd_stop(void)
{
	if (_eapd_pid) {
		/* set stop flag */
		eapd_hup_hdlr(0);

		/* wait till the eapd_main task is dead */
		while (oslib_waitpid(_eapd_pid, NULL))
			cyg_thread_delay(10);
		_eapd_pid = 0;
		EAPD_PRINT("EAPD task stopped\n");
	}
}

size_t
eapd_message_read(int fd, void *buf, size_t nbytes)
{
	return (read(fd,buf,nbytes));
}
