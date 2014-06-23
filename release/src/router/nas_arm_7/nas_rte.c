/* Network Authentication Service deamon (RTE)
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: nas_rte.c 241182 2011-02-17 21:50:03Z $
 */


#include <typedefs.h>
#include <time.h>
#include <osl.h>
#include <hndrte.h>
#include <wlutils.h>
#include <assert.h>

#include <bcmcrypto/md5.h>

#include <relay_rte.h>

#include "nas_wksp.h"

/* RTE Layer 2 protocol handlers */
/* WPA */
static bool wpa_recv(hndrte_dev_t *dev, void *ctx, struct lbuf *lb, uint16 ethtype);
static ethtype_handler_t wpa_handler = {NULL, ETHER_TYPE_BRCM, wpa_recv, };

extern void bsp_reboot(void);

hndrte_dev_t *wldev;
int naswl_sendpkt(struct lbuf *lbf);


void nas_sleep_ms(uint ms);
void nas_rand128(uint8 *rand128);
void nas_reset_board(void);
static int nas_get_wlrand(char *ifname, uint16 *val);
int nas_init(int argc, char *argv[]);
void nasStart(char *type);


/* Might be better to put this somewhere else, this doesn't have to be NAS
 * specific - it's implementing POSIX-like functionality
 */
time_t
time(time_t *t)
{
	if (!t)
		return 0;

	*t = hndrte_time();
	return *t;
}

/* Might be better to put this somewhere else, this doesn't have to be NAS
 * specific - it's implementing POSIX-like functionality
 */
int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
	uint32 milliseconds = hndrte_time();

	memset(tz, 0, sizeof(struct timezone));
	memset(tv, 0, sizeof(struct timeval));

	tv->tv_sec = milliseconds / 1000;

	return 0;
}


int
naswl_sendpkt(struct lbuf *lbf)
{
	return wldev->dev_funcs->xmit(NULL, wldev, lbf);
}


void
nas_sleep_ms(uint ms)
{
	hndrte_delay(ms*1000);
}

static int
nas_get_wlrand(char *ifname, uint16 *val)
{
	char buf[WLC_IOCTL_SMLEN];
	int ret;

	strcpy(buf, "rand");
	if ((ret = wl_ioctl(ifname, WLC_GET_VAR, buf, sizeof(buf))))
		return ret;

	*val = *(uint16 *)buf;
	return 0;
}

void
nas_rand128(uint8 *rand128)
{
	struct timeval tv;
	struct timezone tz;
	uint16 rand_val;
	MD5_CTX md5;

	gettimeofday(&tv, &tz);

	if (nas_get_wlrand("wl0", &rand_val) == 0)
		tv.tv_sec ^= rand_val;
	else
		NASDBG("NAS - nas_rad128 couldn't get random number from WL!!!\n");
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char *) &tv, sizeof(tv));
	MD5Update(&md5, (unsigned char *) &tz, sizeof(tz));
	MD5Final(rand128, &md5);
}

/* establish connection to receive wpa message */
int
nas_wksp_open_wpa(nas_wksp_t *nwksp)
{
	/* set the context for this handler to use */
	wpa_handler.ctx = nwksp;

	/* Register with the relay to receive  for packets */
	bcmrelay.dev_funcs->ioctl(&bcmrelay, RELAY_PROTOREGISTER,
	                          &wpa_handler,
	                          sizeof(ethtype_handler_t *), NULL, NULL, 0);

	NASDBG("%s: registered with relay for wpa messages\n", nwksp->lan);
	return 0;
}

static bool
wpa_recv(hndrte_dev_t *dev, void *ctx, struct lbuf *lb, uint16 ethtype)
{
	uint8 *pkt = lb->data;
	int len = lb->len;
	nas_wksp_t *nwksp = (nas_wksp_t *)ctx;
	nas_wpa_cb_t *nwcb;
	bcm_event_t *pvt_data = (bcm_event_t *)pkt;

	/* recv wpa message from relay */
	/* validate the recv packet */
	if (nas_validate_wlpvt_message(len, pkt) != 0) {
		goto error0;
	}
	pvt_data  = (bcm_event_t *)pkt;

	nwcb = nas_wksp_find_nwcb_by_mac(nwksp, pvt_data->eth.ether_dhost,
	                                 pvt_data->event.ifname);

	if (nwcb && !(nwcb->flags & NAS_WPA_CB_FLAG_ERROR))
		nas_handle_wlpvt_messages(nwcb, (void *)pvt_data, len, 1);

	lb_free(lb);
	return TRUE;

error0:
	/* way to say not our message */
	NASDBG("Not a WPA NAS packet, returning pkt to relay 0x%p\n", lb);
	return FALSE;
}

/* transmit preauth message thru the relay */
int
nas_preauth_send_packet(nas_t *nas, struct iovec *frags, int nfrags)
{
	printf("\nUnsupported - nas_preauth_send_packet\n\n");
	return 1;
}


/* open connections to network to receive/send eapol packets */
int
nas_wksp_open_eapol(nas_wksp_t *nwksp)
{
	/* not needed for HNDRTE - all EAPOL messages are
	 * encapsulated in BRCM ethertype from wl
	 */
	return 0;
}

/* send eapol packet out to the net */
int
nas_eapol_send_packet(nas_t *nas, struct iovec *frags, int nfrags)
{
	struct lbuf *lbf;
	int i, count;
	uchar *buf;

	/* Convert iov to mbuf chain */
	if (nfrags > 1)
	{
		for (i = 0, count = 0; i < nfrags; i++)
			count += frags[i].iov_len;
		if (!(lbf = lb_alloc(count, __FILE__, __LINE__)))
		{
			NASDBG("%s: lb_alloc error\n", nas->interface);
			return 1;
		}
		buf = lbf->data;
		for (i = 0, count = 0; i < nfrags; i++)
		{
			memcpy(&buf[count], frags[i].iov_base, frags[i].iov_len);
			count += frags[i].iov_len;
		}
	}
	else if (nfrags == 1)
	{
		lbf = lb_alloc(frags[0].iov_len, __FILE__, __LINE__);
		memcpy(lbf->data, frags[0].iov_base, frags[0].iov_len);
		NASDBG("%s: nfrags = 1 and Length of Message %d\n",
			nas->interface,  frags[0].iov_len);
	}
	else
	{
		NASDBG("%s: nfrags == 0 error\n", nas->interface);
		return 1;
	}
	if (!lbf)
	{
		NASDBG("%s: failed to allocate mblk\n", nas->interface);
		return 1;
	}

	/* send packet to network thru the interface */
	return naswl_sendpkt(lbf);
}

void
nas_reset_board(void)
{
	bsp_reboot();
	return;
}

/* nas task entry */
int
nas_init(int argc, char *argv[])
{
	nas_wksp_t *nwksp = NULL;

	/* alloc nas/wpa work space */
	if (!(nwksp = nas_wksp_alloc_workspace())) {
		NASMSG("Unable to allocate memory. Quitting...\n");
		goto exit0;
	}

	/* init nas/wpa work space */
	if (nas_wksp_parse_cmd_line(argc, argv, nwksp)) {
		NASMSG("Command line parsing error. Quitting...\n");
		goto exit1;
	}
	if (!nwksp->nwcbs) {
		NASMSG("No interface specified. Quitting...\n");
		goto exit1;
	}

	/* init nas */
	if (nas_wksp_init(nwksp)) {
		NASMSG("Unable to initialize NAS. Quitting...\n");
		goto exit1;
	}
	goto exit0;

	/* error handling */
exit1:
	nas_wksp_free_workspace(nwksp);
exit0:
	return 0;
}

void
nasStart(char *type)
{
	static char *nas_argv[NAS_WKSP_MAX_CMD_LINE_ARGS];
	int nas_argc = NAS_WKSP_MAX_CMD_LINE_ARGS;

	wldev = hndrte_get_dev("wl0");

	if (nas_wksp_build_cmd_line(type, NULL, &nas_argc, nas_argv))
		NASDBG("NAS error - couldn't build command line.\n");
	else if (nas_init(nas_argc, nas_argv) != 0)
		NASDBG("NAS error - init failed.\n");

	return;
}
