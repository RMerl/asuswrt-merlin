/*
 * Radius support for NAS workspace
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: nas_wksp_radius.c 437585 2013-11-19 10:49:52Z $
 */

#include <typedefs.h>
#include <assert.h>
#include <unistd.h>
#include <proto/eap.h>
#include <errno.h>

#include "nas.h"
#include "nas_wksp.h"
#include "nas_radius.h"
#include "nas_wksp_radius.h"

#ifdef NAS_WKSP_BUILD_NAS_AUTH
/*
* Common NAS communication routines that can be used under different
* OSs. These functions need to be re-implemented only when the socket
* layer interface provided for the specific operating system is not
* the similiar kind to that in this implementation.
*/
#define NAS_WKSP_RADIUS_TRIES	5
#define NAS_WKSP_RADIUS_SLEEP	5	/* in seconds */

/* establish connection to radius server */
int
nas_radius_open(nas_wksp_t *nwksp, nas_wpa_cb_t *nwcb)
{
	nas_t *nas = &nwcb->nas;
	int n, i;

	/*
	* Prevent a descriptor leak in case the connection was broken by
	* the server and some one tries to re-establish the connection
	* with the server.
	*/
	if (nas->wan != NAS_WKSP_UNK_FILE_DESC) {
		NASDBG("%s: close radius socket %d\n", nas->interface, nas->wan);
		close(nas->wan);
		nas->wan = NAS_WKSP_UNK_FILE_DESC;
	}

	/* Connect to server */
	if ((nas->wan = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		NASDBG("%s: Unable to create radius socket\n", nas->interface);
		goto exit0;
	}

	/* Give the RADIUS server a little time in case it's on
	 * the WAN.  It could be opened on demand later, but it's
	 * good to get an open descriptor now if possible.
	 */
	for (i = 0; i < NAS_WKSP_RADIUS_TRIES; i ++) {
		if (connect(nas->wan, (struct sockaddr *)&nas->server, sizeof(nas->server)) < 0) {
			NASDBG("%s: Unable to connect radius socket %d\n", nas->interface,
			       nas->wan);
			nas_sleep_ms(NAS_WKSP_RADIUS_SLEEP*1000);
			continue;
		}
		n = sizeof(nas->client);
		if (getsockname(nas->wan, (struct sockaddr *)&nas->client, (socklen_t *)&n) < 0)
			break;
		NASDBG("%s: opened radius socket %d\n", nas->interface, nas->wan);
		return 0;
	}

	/* It should never come to here - error! */
	close(nas->wan);
exit0:
	NASDBG("%s: failed to open radius socket\n", nas->interface);
	nas->wan = NAS_WKSP_UNK_FILE_DESC;
	return errno;
}

/* close the connection to radius server */
void
nas_radius_close(nas_wksp_t *nwksp, nas_wpa_cb_t *nwcb)
{
	nas_t *nas = &nwcb->nas;
	if (nas->wan != NAS_WKSP_UNK_FILE_DESC) {
		NASDBG("%s: close radius socket %d\n", nas->interface, nas->wan);
		close(nas->wan);
		nas->wan = NAS_WKSP_UNK_FILE_DESC;
	}
}

/* establish connection to radius server for each interface */
int
nas_wksp_open_radius(nas_wksp_t *nwksp)
{
	nas_wpa_cb_t *nwcb;
	int i;

	for (i = 0; i < nwksp->nwcbs; i ++) {
		nwcb = nwksp->nwcb[i];
		assert(nwcb);
		/* open connection to radius server */
		if (CHECK_RADIUS(nwcb->nas.mode)) {
				/* open connection to radius server */
				nas_radius_open(nwksp, nwcb);
		}
	}
	return 0;
}

/* close connection to radius server for each interface */
void
nas_wksp_close_radius(nas_wksp_t *nwksp)
{
	nas_wpa_cb_t *nwcb;
	int i;

	for (i = 0; i < nwksp->nwcbs; i ++) {
		nwcb = nwksp->nwcb[i];
		assert(nwcb);
		if (CHECK_RADIUS(nwcb->nas.mode)) {
			nas_radius_close(nwksp, nwcb);
		}
	}
}

/* send radius packet to radius server */
int
nas_radius_send_packet(nas_t *nas, radius_header_t *radius, int length)
{
	int ret;
	nas_wpa_cb_t *nwcb = (nas_wpa_cb_t *)nas->appl;
	NASDBG("%s: sending packet to radius socket %d\n", nas->interface, nas->wan);
	if ((ret = send(nas->wan, (char *)radius, length, 0)) < 0) {
		NASDBG("%s: send error %d to radius socket %d\n", nas->interface, errno, nas->wan);
		/* Try re-opening it once before giving up. */
		/* This could happen if the peer has been reset */
		if (errno == EBADF) {
			if (!nas_radius_open(nwcb->nwksp, nwcb)) {
				NASDBG("%s: resending packet to radius socket %d\n", nas->interface,
				       nas->wan);
				ret = send(nas->wan, (char *)radius, length, 0);
				if (ret < 0)
					NASDBG("%s: resend error %d to radius socket %d\n",
					       nas->interface, errno, nas->wan);
			}
		}
	}
	return ret;
}

#endif	/* #ifdef NAS_WKSP_BUILD_NAS_AUTH */
