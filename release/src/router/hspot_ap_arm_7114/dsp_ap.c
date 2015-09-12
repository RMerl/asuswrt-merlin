/*
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id:$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "bcmendian.h"
#include "trace.h"
#include "dsp.h"
#include <signal.h>
#include "tcp_srv.h"

/* request handler */
typedef void (*requestHandlerT)(void *context,
	int reqLength, uint8 *reqData, uint8 *rspData);

#define MAX_WLAN_HANDLER	3

struct dspStruct {
	struct {
		dspWlanHandlerT handler;
		void *context;
	} wlanHandler[MAX_WLAN_HANDLER];
	pthread_t pThread;
	int pipeFd[2];
	int socketFd;
	bcmseclib_timer_mgr_t *timer_mgr;
};

#define MAX_EVENT_SIZE	(2 * 1024)
#define MAX_TIMERS	512

typedef enum {
	EVENT_SUBSCRIBE,
	EVENT_UNSUBSCRIBE,
	EVENT_STOP,
	EVENT_TIMEOUT,
	EVENT_REQUEST
} eventTypeE;

typedef struct {
	void *context;
	eventTypeE event;
	int isSync;
	int reqLength;
	uint8 *rspData;
	/* variable length reqData follows struct */
} dspEventT;

/* single instance of dispatcher */
static dspT *gDsp;


static void dsp_hup_hdlr(int sig)
{
	(void)sig;
	if (gDsp) {
		dspStop(gDsp);
	}
}

static void wlanEventHandler(dspT *dsp, ssize_t length, uint8 *rxbuf)
{
	bcm_event_t *bcmEvent;
	wl_event_msg_t *wlEvent;
	int i;

	if (length == 0 || rxbuf == 0)
		return;

	bcmEvent = (bcm_event_t *)rxbuf;

	wlEvent = &bcmEvent->event;
	wlEvent->flags = ntoh16(wlEvent->flags);
	wlEvent->version = ntoh16(wlEvent->version);
	wlEvent->event_type = ntoh32(wlEvent->event_type);
	wlEvent->status = ntoh32(wlEvent->status);
	wlEvent->reason = ntoh32(wlEvent->reason);
	wlEvent->auth_type = ntoh32(wlEvent->auth_type);
	wlEvent->datalen = ntoh32(wlEvent->datalen);

	if (wlEvent->datalen > length - sizeof(bcm_event_t)) {
		TRACE(TRACE_ERROR, "invalid data length %d > %d\n",
			wlEvent->datalen, length - sizeof(bcm_event_t));
	}

	for (i = 0; i < MAX_WLAN_HANDLER; i++) {
		if (dsp->wlanHandler[i].handler != 0)
			(dsp->wlanHandler[i].handler)(
				dsp->wlanHandler[i].context,
				wlEvent->event_type, wlEvent,
				(uint8 *)&bcmEvent[1], wlEvent->datalen);
	}
}

static void *dspMonitor(void *arg)
{
	int ret;
	dspT *dsp = arg;

	if ((ret = pipe(dsp->pipeFd)) == -1) {
		TRACE(TRACE_ERROR, "pipe failed %d\n", ret);
		return 0;
	}

	if ((dsp->socketFd = socket(PF_PACKET, SOCK_RAW, htons(ETHER_TYPE_BRCM))) < 0) {
		if (errno == EPERM) {
			TRACE(TRACE_ERROR, "requires root privilege\n");
		}
		TRACE(TRACE_ERROR, "socket failed errno=%d\n", errno);
		return 0;
	}

	bcmseclib_init_timer_utilities_ex(MAX_TIMERS, &dsp->timer_mgr);

	TRACE(TRACE_VERBOSE, "dispatcher starting...\n");

	while (1) {
		fd_set rfds;
		int	last_fd;
		int is_timeout;
		exp_time_t timeout_setting;
		struct timeval tv;
		int fd;

		FD_ZERO(&rfds);
		FD_SET(dsp->pipeFd[0], &rfds);
		FD_SET(dsp->socketFd, &rfds);
		last_fd = dsp->pipeFd[0] > dsp->socketFd ? dsp->pipeFd[0] : dsp->socketFd;
		/* returns -1 if there is no server */
		fd = tcpSetSelectFds(&rfds);
		last_fd = last_fd > fd ? last_fd:fd;

		memset(&tv, 0, sizeof(tv));
		is_timeout = bcmseclib_get_timeout_ex(dsp->timer_mgr, &timeout_setting);
		if (is_timeout) {
			tv.tv_sec = timeout_setting.sec;
			tv.tv_usec = timeout_setting.usec;
		}

		ret = select(last_fd + 1, &rfds, NULL, NULL, is_timeout ? &tv : NULL);
		if (ret == -1) {
			TRACE(TRACE_ERROR, "select failed %d\n", ret);
			continue;
		}

		/* process timers */
		bcmseclib_process_timer_expiry_ex(dsp->timer_mgr);

		if (FD_ISSET(dsp->pipeFd[0], &rfds)) {
			dspEventT evt;
			ssize_t count;

			/* uint8 req[MAX_EVENT_SIZE]; */
			static uint8 req[MAX_EVENT_SIZE];
			/* retrieve the event */
			count = read(dsp->pipeFd[0], &evt, sizeof(evt));
			if (count != sizeof(evt)) {
				TRACE(TRACE_ERROR, "failed to read event %d != %d\n",
					count, sizeof(evt));
			}
			/* retrieve the request data if any */
			if (evt.reqLength) {
				count = read(dsp->pipeFd[0], req, evt.reqLength);
				if (count != evt.reqLength) {
					TRACE(TRACE_ERROR, "failed to read request data %d != %d\n",
						count, evt.reqLength);
			}
			}

			switch (evt.event) {
			case EVENT_STOP:
				bcmseclib_deinit_timer_utilities_ex(dsp->timer_mgr);
				close(dsp->socketFd);
				close(dsp->pipeFd[0]);
				close(dsp->pipeFd[1]);
				break;
			case EVENT_REQUEST:
			{
				requestHandlerT *handler = (requestHandlerT *)req;
				if (handler != 0 && *handler != 0)
					(*handler)(evt.context, evt.reqLength, req, evt.rspData);
			}
				break;
			default:
				TRACE(TRACE_ERROR, "unknown event %d\n", evt.event);
				break;
			}

			if (evt.event == EVENT_STOP)
				break;
		}

		if (FD_ISSET(dsp->socketFd, &rfds)) {
			/* uint8 rxbuf[MAX_EVENT_SIZE]; */
			static uint8 rxbuf[MAX_EVENT_SIZE];
			ssize_t count;

			count = recv(dsp->socketFd, rxbuf, sizeof(rxbuf), 0);
			if (count == -1) {
				TRACE(TRACE_ERROR, "recv failed\n");
				continue;
			}

			wlanEventHandler(dsp, count, rxbuf);
		}
		tcpProcessSelect(&rfds);
	}

	TRACE(TRACE_VERBOSE, "dispatcher stopped\n");
	return 0;
}

static int dspSend(dspT *dsp, void *context,
	eventTypeE event, int isSync,
	int reqLength, uint8 *reqData,
	uint8 *rspData)
{
	/* char buf[MAX_EVENT_SIZE]; */
	static char buf[MAX_EVENT_SIZE];
	int total;
	dspEventT *evt;
	uint8 *d;
	int count;

	total = sizeof(*evt) + reqLength;
	if (total > MAX_EVENT_SIZE) {
		TRACE(TRACE_ERROR, "event data too big %d > %d\n", total, MAX_EVENT_SIZE);
		return 0;
	}
	evt = (dspEventT *)buf;
	evt->context = context;
	evt->event = event;
	evt->isSync = isSync;
	evt->reqLength = reqLength;
	evt->rspData = rspData;
	d = (uint8 *)evt + sizeof(*evt);
	memcpy(d, reqData, reqLength);
	count = write(dsp->pipeFd[1], evt, total);
	if (count != total) {
		TRACE(TRACE_ERROR, "write failed %d != %d\n", count, total);
		count = 0;
	}
	return count;
}

/* get dispatcher instance */
dspT *dsp(void)
{
	if (gDsp == 0)
		gDsp = dspCreate();

	return gDsp;
}

/* free dispatcher instance */
void dspFree(void)
{
	if (gDsp != 0) {
		dspDestroy(gDsp);
		gDsp = 0;
	}
}

/* create dispatcher */
dspT *dspCreate(void)
{
	dspT *dsp;

	dsp = malloc(sizeof(*dsp));
	if (dsp == 0)
		goto fail;

	memset(dsp, 0, sizeof(*dsp));
	return dsp;

fail:
	if (dsp != 0)
		free(dsp);

	return 0;
}

/* destroy dispatcher */
int dspDestroy(dspT *dsp)
{
	int ret = 1;
	free(dsp);
	TRACE(TRACE_VERBOSE, "dspDestroy done\n");
	return ret;
}

/* dispatcher subscribe */
int dspSubscribe(dspT *dsp, void *context, dspWlanHandlerT wlan)
{
	int i;
	for (i = 0; i < MAX_WLAN_HANDLER; i++) {
		if (dsp->wlanHandler[i].handler == 0) {
			dsp->wlanHandler[i].handler = wlan;
			dsp->wlanHandler[i].context = context;
			break;
		}
	}
	return TRUE;
}

/* dispatcher unsubscribe */
int dspUnsubscribe(dspT *dsp, dspWlanHandlerT wlan)
{
	int i;
	for (i = 0; i < MAX_WLAN_HANDLER; i++) {
		if (dsp->wlanHandler[i].handler == wlan) {
			memset(&dsp->wlanHandler[i], 0,
				sizeof(dsp->wlanHandler[i]));
			break;
		}
	}
	return TRUE;
}

/* start dispatcher processing */
int dspStart(dspT *dsp)
{
	int ret = 1;
	signal(SIGTERM, dsp_hup_hdlr);
	/* signal(SIGINT, dsp_hup_hdlr); */
	dspMonitor(dsp);
	return ret;
}

/* stop dispatcher processing */
int dspStop(dspT *dsp)
{
	return dspSend(dsp, dsp, EVENT_STOP, FALSE, 0, 0, 0);
}

/* send request to dispatcher */
int dspRequest(dspT *dsp, void *context, int reqLength, uint8 *reqData)
{
	/* asynchronous request */
	return dspSend(dsp, context, EVENT_REQUEST, FALSE, reqLength, reqData, 0);
}

/* send request to dispatcher and wait for response */
int dspRequestSynch(dspT *dsp, void *context,
	int reqLength, uint8 *reqData, uint8 *rspData)
{
	/* don't support synchronous request */
	return dspSend(dsp, context, EVENT_REQUEST, TRUE, reqLength, reqData, rspData);
}

/* get timer manager */
bcmseclib_timer_mgr_t *dspGetTimerMgr(dspT *dsp)
{
	return dsp->timer_mgr;
}
