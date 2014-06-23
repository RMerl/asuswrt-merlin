/*
 * ACS deamon (Linux)
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: acsd_main.c 427289 2013-10-03 01:36:21Z $
 */

#include <proto/ethernet.h>
#include <proto/bcmeth.h>
#include <proto/bcmevent.h>
#include <proto/802.11.h>

#include "acsd_svr.h"

acsd_wksp_t *d_info;

/* open a UDP packet to event dispatcher for receiving/sending data */
static int
acsd_open_eventfd()
{
	int reuse = 1;
	struct sockaddr_in sockaddr;
	int fd = ACSD_DFLT_FD;

	/* open loopback socket to communicate with event dispatcher */
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sockaddr.sin_port = htons(EAPD_WKSP_DCS_UDP_SPORT);

	if ((fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		ACSD_ERROR("Unable to create loopback socket\n");
		goto exit1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
		ACSD_ERROR("Unable to setsockopt to loopback socket %d.\n", fd);
		goto exit1;
	}

	if (bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
		ACSD_ERROR("Unable to bind to loopback socket %d\n", fd);
		goto exit1;
	}

	ACSD_INFO("opened loopback socket %d\n", fd);
	d_info->event_fd = fd;

	return ACSD_OK;

	/* error handling */
exit1:
	if (fd != ACSD_DFLT_FD) {
		close(fd);
	}
	return errno;
}

static int
acsd_svr_socket_init(unsigned int port)
{
	return ACSD_OK;

	struct sockaddr_in s_sock;

	d_info->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (d_info->listen_fd < 0) {
		ACSD_ERROR("Socket open failed: %s\n", strerror(errno));
		return ACSD_FAIL;
	}

	memset(&s_sock, 0, sizeof(struct sockaddr_in));
	s_sock.sin_family = AF_INET;
	s_sock.sin_addr.s_addr = htonl(INADDR_ANY);
	s_sock.sin_port = htons(port);

	if (bind(d_info->listen_fd, (struct sockaddr *)&s_sock,
		sizeof(struct sockaddr_in)) < 0) {
		ACSD_ERROR("Socket bind failed: %s\n", strerror(errno));
		return ACSD_FAIL;
	}

	if (listen(d_info->listen_fd, 5) < 0) {
		ACSD_ERROR("Socket listen failed: %s\n", strerror(errno));
		return ACSD_FAIL;
	}

	return ACSD_OK;
}

static void
acsd_close_listenfd()
{
	/* close event dispatcher socket */
	if (d_info->listen_fd != ACSD_DFLT_FD) {
		ACSD_INFO("listenfd: close  socket %d\n", d_info->listen_fd);
		close(d_info->listen_fd);
		d_info->event_fd = ACSD_DFLT_FD;
	}
	return;
}

static void
acsd_close_eventfd()
{
	/* close event dispatcher socket */
	if (d_info->event_fd != ACSD_DFLT_FD) {
		ACSD_INFO("eventfd: close loopback socket %d\n", d_info->event_fd);
		close(d_info->event_fd);
		d_info->event_fd = ACSD_DFLT_FD;
	}
	return;
}

static int
acsd_validate_wlpvt_message(int bytes, uint8 *dpkt)
{
	bcm_event_t *pvt_data;

	/* the message should be at least the header to even look at it */
	if (bytes < sizeof(bcm_event_t) + 2) {
		ACSD_ERROR("Invalid length of message\n");
		goto error_exit;
	}
	pvt_data  = (bcm_event_t *)dpkt;
	if (ntohs(pvt_data->bcm_hdr.subtype) != BCMILCP_SUBTYPE_VENDOR_LONG) {
		ACSD_ERROR("%s: not vendor specifictype\n",
		       pvt_data->event.ifname);
		goto error_exit;
	}
	if (pvt_data->bcm_hdr.version != BCMILCP_BCM_SUBTYPEHDR_VERSION) {
		ACSD_ERROR("%s: subtype header version mismatch\n",
			pvt_data->event.ifname);
		goto error_exit;
	}
	if (ntohs(pvt_data->bcm_hdr.length) < BCMILCP_BCM_SUBTYPEHDR_MINLENGTH) {
		ACSD_ERROR("%s: subtype hdr length not even minimum\n",
			pvt_data->event.ifname);
		goto error_exit;
	}
	if (bcmp(&pvt_data->bcm_hdr.oui[0], BRCM_OUI, DOT11_OUI_LEN) != 0) {
		ACSD_ERROR("%s: acsd_validate_wlpvt_message: not BRCM OUI\n",
			pvt_data->event.ifname);
		goto error_exit;
	}
	/* check for wl dcs message types */
	switch (ntohs(pvt_data->bcm_hdr.usr_subtype)) {
		case BCMILCP_BCM_SUBTYPE_EVENT:

			ACSD_INFO("subtype: event\n");
			break;
		default:
			goto error_exit;
			break;
	}
	return ACSD_OK; /* good packet may be this is destined to us */
error_exit:
	return ACSD_FAIL;
}

/*
 * Receives and processes the commands from client
 * o Wait for connection from client
 * o Process the command and respond back to client
 * o close connection with client
 */
static int
acsd_proc_client_req(void)
{
	uint resp_size = 0;
	int rcount = 0;
	int fd = -1;
	struct sockaddr_in cliaddr;
	uint len = 0; /* need initialize here to avoid EINVAL */
	char* buf;

	fd = accept(d_info->listen_fd, (struct sockaddr*)&cliaddr,
		&len);
	if (fd < 0) {
		if (errno == EINTR) return 0;
		else {
			ACSD_ERROR("accept failed: errno: %d - %s\n", errno, strerror(errno));
			return -1;
		}
	}
	d_info->conn_fd = fd;

	if (!d_info->cmd_buf)
		d_info->cmd_buf = acsd_malloc(ACSD_BUFSIZE_4K);

	buf = d_info->cmd_buf;

	/* get command from client */
	if ((rcount = sread(d_info->conn_fd, buf, ACSD_BUFSIZE_4K)) < 0) {
		ACSD_ERROR("Failed reading message from client: %s\n", strerror(errno));
		return -1;
	}

	acsd_proc_cmd(d_info, buf, rcount, &resp_size);

	if (swrite(d_info->conn_fd, buf, resp_size) < 0) {
		ACSD_ERROR("Failed sending message: %s\n", strerror(errno));
		return -1;
	}

	close(d_info->conn_fd);
	d_info->conn_fd = -1;
	return 0;
}

/* Check if stay in current channel long enough */
static bool
chanim_record_chan_dwell(acs_chaninfo_t *c_info, chanim_info_t *ch_info)
{
	acs_fcs_t *dcs_info = &c_info->acs_fcs;
	uint8 cur_idx = chanim_mark(ch_info).record_idx;
	uint8 start_idx;
	chanim_acs_record_t *start_record;
	time_t now = uptime();

	start_idx = MODSUB(cur_idx, 1, CHANIM_ACS_RECORD);
	start_record = &ch_info->record[start_idx];
	if (now - start_record->timestamp > dcs_info->acs_chan_dwell_time)
		return TRUE;
	return FALSE;
}

/* listen to sockets and call handlers to process packets */
static void
acsd_main_loop(struct timeval *tv)
{
	fd_set fdset;
	int width, status = 0, bytes, len;
	uint8 *pkt;
	bcm_event_t *pvt_data;

	/* init file descriptor set */
	FD_ZERO(&d_info->fdset);
	d_info->fdmax = -1;

	/* build file descriptor set now to save time later */
	if (d_info->event_fd != ACSD_DFLT_FD) {
		FD_SET(d_info->event_fd, &d_info->fdset);
		d_info->fdmax = d_info->event_fd;
	}

	if (d_info->listen_fd != ACSD_DFLT_FD) {
		FD_SET(d_info->listen_fd, &d_info->fdset);
		if (d_info->listen_fd > d_info->fdmax)
			d_info->fdmax = d_info->listen_fd;
	}

	pkt = d_info->packet;
	len = sizeof(d_info->packet);
	width = d_info->fdmax + 1;
	fdset = d_info->fdset;

	/* listen to data availible on all sockets */
	status = select(width, &fdset, NULL, NULL, tv);

	if ((status == -1 && errno == EINTR) || (status == 0))
		return;

	if (status <= 0) {
		ACSD_ERROR("err from select: %s", strerror(errno));
		return;
	}

	if (d_info->listen_fd != ACSD_DFLT_FD && FD_ISSET(d_info->listen_fd, &fdset)) {
		ACSD_INFO("recved command from a client\n");

		d_info->stats.num_cmds++;
		acsd_proc_client_req();
	}

	/* handle brcm event */
	if (d_info->event_fd !=  ACSD_DFLT_FD && FD_ISSET(d_info->event_fd, &fdset)) {
		char *ifname = (char *)pkt;
		struct ether_header *eth_hdr = (struct ether_header *)(ifname + IFNAMSIZ);
		uint16 ether_type = 0;
		uint32 evt_type;
		int err;

		ACSD_INFO("recved event from eventfd\n");

		d_info->stats.num_events++;

		if ((bytes = recv(d_info->event_fd, pkt, len, 0)) <= 0)
			return;

		ACSD_INFO("recved %d bytes from eventfd, ifname: %s\n",
			bytes, ifname);
		bytes -= IFNAMSIZ;

		if ((ether_type = ntohs(eth_hdr->ether_type) != ETHER_TYPE_BRCM)) {
			ACSD_INFO("recved ether type %x\n", ether_type);
			return;
		}

		if ((err = acsd_validate_wlpvt_message(bytes, (uint8 *)eth_hdr)))
			return;

		pvt_data = (bcm_event_t *)(ifname + IFNAMSIZ);
		evt_type = ntoh32(pvt_data->event.event_type);
		ACSD_INFO("recved brcm event, event_type: %d\n", evt_type);

		d_info->stats.valid_events++;

		switch (evt_type) {

		case WLC_E_DCS_REQUEST: {
			dot11_action_wifi_vendor_specific_t * actfrm;
			actfrm = (dot11_action_wifi_vendor_specific_t *)(pvt_data + 1);
			wl_bcmdcs_data_t dcs_data;

			if ((err = dcs_parse_actframe(actfrm, &dcs_data))) {
				ACSD_ERROR("err from dcs_parse_request: %d\n", err);
				break;
			}

			if ((err = dcs_handle_request(ifname, &dcs_data, DOT11_CSA_MODE_NO_TX,
				DCS_CSA_COUNT, CSA_BROADCAST_ACTION_FRAME)))
				ACSD_ERROR("err from dcs_handle_request: %d\n", err);

			break;
		}
		case WLC_E_SCAN_COMPLETE: {
			ACSD_INFO("recved brcm event: scan complete\n");
			break;
		}
		case WLC_E_PKTDELAY_IND: {
			txdelay_event_t pktdelay;
			int index = 0;
			acs_chaninfo_t *c_info;
			bool chan_least_dwell = FALSE;
			wl_bcmdcs_data_t dcs_data;

			if ((index = acs_idx_from_map(ifname)) < 0) {
				ACSD_ERROR("cannot find the mapped entry for ifname: %s\n", ifname);
				break;
			}

			c_info = d_info->acs_info->chan_info[index];
			chan_least_dwell = chanim_record_chan_dwell(c_info, c_info->chanim_info);

			memcpy(&pktdelay, (txdelay_event_t *)(pvt_data + 1),
				sizeof(txdelay_event_t));

			/* stay in current channel more than acs_chan_dwell_time */
			if (chan_least_dwell &&
				(pktdelay.chanim_stats.chan_idle <
					c_info->acs_fcs.acs_fcs_chanim_stats)) {
				acs_select_chspec(c_info);
				dcs_data.reason = 0;
				dcs_data.chspec = c_info->selected_chspec;

				if ((err = dcs_handle_request(ifname, &dcs_data,
					DOT11_CSA_MODE_NO_TX, FCS_CSA_COUNT,
					c_info->acs_fcs.acs_dcs_csa)))
					ACSD_ERROR("err from dcs_handle_request: %d\n", err);
				else
					chanim_upd_acs_record(c_info->chanim_info,
						c_info->selected_chspec, APCS_TXDLY);
			}
			break;
		}
		case WLC_E_TXFAIL_THRESH: {
			wl_intfer_event_t *event;
			int idx = 0;
			acs_chaninfo_t *c_info;
			wl_bcmdcs_data_t dcs_data;
			unsigned char *addr;
			bool chan_least_dwell = FALSE;

			if ((idx = acs_idx_from_map(ifname)) < 0) {
				ACSD_ERROR("cannot find the mapped entry for ifname: %s\n", ifname);
				break;
			}
			c_info = d_info->acs_info->chan_info[idx];
			event = (wl_intfer_event_t *)(pvt_data + 1);
			chan_least_dwell = chanim_record_chan_dwell(c_info, c_info->chanim_info);
			addr = (unsigned char *)(&(pvt_data->event.addr));

			ACSD_FCS("Intfer:%s Mac:%02x:%02x:%02x:%02x:%02x:%02x status=0x%x\n",
				ifname, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
				event->status);

			for (idx = 0; idx < WLINTFER_STATS_NSMPLS; idx++) {
				ACSD_FCS("0x%x\t", event->txfail_histo[idx]);
			}
			ACSD_FCS("\n");

			if (chan_least_dwell) {
				acs_select_chspec(c_info);
				dcs_data.reason = 0;
				dcs_data.chspec = c_info->selected_chspec;

				if ((err = dcs_handle_request(ifname, &dcs_data,
					DOT11_CSA_MODE_NO_TX, FCS_CSA_COUNT,
					c_info->acs_fcs.acs_dcs_csa))) {
					ACSD_ERROR("err from dcs_handle_request: %d\n", err);
				}
				else {
					acs_intfer_config(c_info);
					chanim_upd_acs_record(c_info->chanim_info,
						c_info->selected_chspec, APCS_TXFAIL);
				}
			}
			break;
		}
		default:
			ACSD_INFO("recved event type %x\n", evt_type);
			break;
		}
	}
}

static int
acsd_init(void)
{
	int err = ACSD_OK;
	uint  port = ACSD_DFLT_CLI_PORT;

	d_info = (acsd_wksp_t*)acsd_malloc(sizeof(acsd_wksp_t));

	d_info->version = ACSD_VERSION;
	d_info->fdmax = ACSD_DFLT_FD;
	d_info->event_fd = ACSD_DFLT_FD;
	d_info->listen_fd = ACSD_DFLT_FD;
	d_info->conn_fd = ACSD_DFLT_FD;
	d_info->poll_interval = ACSD_DFLT_POLL_INTERVAL;
	d_info->ticks = 0;
	d_info->cmd_buf = NULL;

	err = acsd_open_eventfd();
	err = acsd_svr_socket_init(port);

	return err;
}

static void
acsd_cleanup(void)
{
	if (d_info) {
		acs_cleanup(&d_info->acs_info);
		ACS_FREE(d_info->cmd_buf);
		free(d_info);
	}
}

static void
acsd_watchdog(uint ticks)
{
	int i;
	acs_chaninfo_t* c_info;

	for (i = 0; i < ACS_MAX_IF_NUM; i++) {
		c_info = d_info->acs_info->chan_info[i];

		if ((!c_info) || (c_info->mode == ACS_MODE_DISABLE))
			continue;

		if (ACS_FCS_MODE(c_info)) {	/* Update channel idle times */
			acs_dfsr_activity_update(ACS_DFSR_CTX(c_info), c_info->name);
		}

		if (ticks % ACS_STATUS_POLL == 0)
			acs_update_status(c_info);

		acsd_chanim_check(ticks, c_info);

		acs_scan_timer_or_dfsr_check(c_info); /* AUTOCHANNEL/DFSR is checked in called fn */

		if (AUTOCHANNEL(c_info) &&
			(acs_fcs_ci_scan_check(c_info) || CI_SCAN(c_info)))
			acs_do_ci_update(ticks, c_info);

	}
}

/* service main entry */
int
main(int argc, char *argv[])
{
	int err = ACSD_OK;
	struct timeval tv;
	char *val;

	val = nvram_safe_get("acsd_debug_level");
	if (strcmp(val, ""))
		acsd_debug_level = strtoul(val, NULL, 0);

	ACSD_INFO("acsd start...\n");

	val = nvram_safe_get("acs_ifnames");
	if (!strcmp(val, "")) {
		ACSD_ERROR("No interface specified, exiting...");
		return err;
	}

	if ((err = acsd_init()))
		goto cleanup;

	acs_init_run(&d_info->acs_info);

#if !defined(DEBUG)
	if (daemon(1, 1) == -1) {
		ACSD_ERROR("err from daemonize.\n");
		goto cleanup;
	}
#endif
	tv.tv_sec = d_info->poll_interval;
	tv.tv_usec = 0;

	while (1) {
		/* Don't change channel when WPS is in the processing,
		 * to avoid WPS fails
		 */
		if (ACS_WPS_RUNNING) {
			sleep_ms(1000);
			continue;
		}

		if (tv.tv_sec == 0 && tv.tv_usec == 0) {
			d_info->ticks ++;
			tv.tv_sec = d_info->poll_interval;
			tv.tv_usec = 0;
			ACSD_DEBUG("ticks: %d\n", d_info->ticks);
			acsd_watchdog(d_info->ticks);
		}
		acsd_main_loop(&tv);
	}
cleanup:
	acsd_close_eventfd();
	acsd_close_listenfd();
	acsd_cleanup();
	return err;
}
