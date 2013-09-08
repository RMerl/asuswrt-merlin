/*
 * WPS eCos platform dependent portion
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_ecos_osl.c 279189 2011-08-23 10:14:21Z $
 */

#include <osl.h> /* OSL_DELAY */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <wlutils.h>
#include <tutrace.h>
#include <wpserror.h>
#include <wpscommon.h>
#include <wps_ap.h>

#include <shutils.h>
#include <wlif_utils.h>
#include <security_ipc.h>
#include <wps.h>
#include <wps_ui.h>
#include <netinet/if_ether.h>
#include <ecos_oslib.h>

#ifdef WPS_UPNP_DEVICE
#include <upnp.h>
#endif

#ifndef hz
#define hz 100
#endif

static char if_name[20] = "";

uint32
WpsHtonl(uint32 intlong)
{
	return htonl(intlong);
}

uint16
WpsHtons(uint16 intshort)
{
	return htons(intshort);
}

uint16
WpsHtonsPtr(uint8 * in, uint8 * out)
{
	uint16 v;
	uint8 *c;
	c = (uint8 *)&v;
	c[0] = in[0]; c[1] = in[1];
	v = htons(v);
	out[0] = c[0]; out[1] = c[1];
	return v;
}

uint32
WpsHtonlPtr(uint8 * in, uint8 * out)
{
	uint32 v;
	uint8 *c;
	c = (uint8 *)&v;
	c[0] = in[0]; c[1] = in[1]; c[2] = in[2]; c[3] = in[3];
	v = htonl(v);
	out[0] = c[0]; out[1] = c[1]; out[2] = c[2]; out[3] = c[3];
	return v;
}

uint32
WpsNtohl(uint8 *a)
{
	uint32 v;
	v = (a[0] << 24) + (a[1] << 16) + (a[2] << 8) + a[3];
	return v;
}

uint16
WpsNtohs(uint8 *a)
{
	uint16 v;
	v = (a[0]*256) + a[1];
	return v;
}

void
WpsSleep(uint32 seconds)
{
	oslib_ticks_sleep(seconds * hz);
}

void
WpsSleepMs(uint32 ms)
{
	/* udelay */
	OSL_DELAY(ms * 1000);
}

/* OSL arp function */
int
wps_osl_arp_get(char *client_ip, unsigned char mac[6])
{
	int s;
	struct ecos_arpentry req;

	memset(&req, 0, sizeof(req));
	if (inet_aton(client_ip, &req.addr) == 0)
		return -1;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		return -1;

	ioctl(s, SIOCGARPRT, &req);

	memcpy(mac, req.enaddr, 6);

	TUTRACE((TUTRACE_INFO,
		"client MAC = '%02x:%02x:%02x:%02x:%02x:%02x'\n",
		mac[0], mac[1], mac[2],
		mac[3], mac[4], mac[5]));

	close(s);
	return 0;
}

/* Network IPC part code */
static int
open_udp_socket(char *addr, uint16 port)
{
	int reuse = 1;
	int sock_fd;
	struct sockaddr_in sockaddr;

	/* open loopback socket to communicate with EAPD */
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = inet_addr(addr);
	sockaddr.sin_port = htons(port);

	if ((sock_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		TUTRACE((TUTRACE_ERR, "Unable to create loopback socket\n"));
		goto exit0;
	}

	if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, (char*)&reuse, sizeof(reuse)) < 0) {
		TUTRACE((TUTRACE_ERR, "Unable to setsockopt to loopback socket %d.\n", sock_fd));
		goto exit1;
	}

	if (bind(sock_fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
		TUTRACE((TUTRACE_ERR, "Unable to bind to loopback socket %d\n", sock_fd));
		goto exit1;
	}
	TUTRACE((TUTRACE_INFO, "opened loopback socket %d in port %d\n", sock_fd, port));
	return sock_fd;

	/* error handling */
exit1:
	close(sock_fd);

exit0:
	TUTRACE((TUTRACE_ERR, "failed to open loopback socket\n"));
	return -1;
}

static inline void
close_udp_socket(int fd)
{
	if (fd >= 0)
		close(fd);
}

/* Init eap socket */
int
wps_osl_eap_handle_init()
{
	int handle;

	/* open socket */
	handle = open_udp_socket(WPS_EAP_ADDR, EAPD_WKSP_WPS_UDP_MPORT);
	if (handle < 0) {
		TUTRACE((TUTRACE_ERR, "init eap handle error\n"));
		return -1;
	}

	return handle;
}

void
wps_osl_eap_handle_deinit(int handle)
{
	close_udp_socket(handle);
}


/* Init ui socket */
int
wps_osl_ui_handle_init()
{
	int handle;

	/* open socket */
	handle = open_udp_socket(WPS_UI_ADDR, WPS_UI_PORT);
	if (handle < 0) {
		TUTRACE((TUTRACE_ERR, "init ui handle error\n"));
		return -1;
	}

	return handle;
}

void
wps_osl_ui_handle_deinit(int handle)
{
	close_udp_socket(handle);
}

/* Receive packet */
static struct sockaddr_in dst;

wps_hndl_t *
wps_osl_wait_for_all_packets(char *dataBuffer, uint32 *dataLen, wps_hndl_t *hndl_list)
{
	wps_hndl_t *hndl;
	wps_hndl_t *ret_hndl = NULL;
	int n, max_fd = -1;
	fd_set fdvar;
	struct timeval timeout;

	struct iovec iov[2];
	struct msghdr msg;
	int bytes;
	int num;
	int handle_max_fd;

	if (dataBuffer && (! dataLen)) {
		TUTRACE((TUTRACE_ERR, "wpsm_readData: arg err!\n"));
		return NULL;
	}

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	FD_ZERO(&fdvar);

	/* Set FD */
	hndl = hndl_list;
	while (hndl) {
#ifdef WPS_UPNP_DEVICE
		/*
		 * hndl->handle is unused when hndl->type is WPS_RECEIVE_PKT_UPNP,
		 * and we only have one libupnp handle.
		 */
		if (hndl->type == WPS_RECEIVE_PKT_UPNP) {
			handle_max_fd = upnp_fdset(hndl->private, &fdvar);
		}
		else
#endif /* WPS_UPNP_DEVICE */
		{
			FD_SET(hndl->handle, &fdvar);
			handle_max_fd = hndl->handle;
		}

		if (max_fd < handle_max_fd)
			max_fd = handle_max_fd;

		hndl = hndl->next;
	}

	if (max_fd == -1) {
		TUTRACE((TUTRACE_ERR, "wpsm_readData: no fd set!\n"));
		return NULL;
	}

	/* Do select */
	n = select(max_fd + 1, &fdvar, NULL, NULL, &timeout);
	if (n <= 0) {
		/*
		 * to avoid the select operation interferenced by led lighting timer.
		 * this will be removed after led lighting timer is replaced by wireless driver
		 */
		if (n < 0 && errno != EINTR) {
			TUTRACE((TUTRACE_ERR, "wpsm_readData: select recv failed\n"));
		}
		goto out;
	}

	/* Poll which handle received data */
	hndl = hndl_list;
	while (hndl) {
#ifdef WPS_UPNP_DEVICE
		if (hndl->type == WPS_RECEIVE_PKT_UPNP) {
			/*
			 * We have to know the return code after upnp_dispatch invoke
			 * WFADevice functions, the libupnp_retcode variable will store it.
			 */
			if (upnp_fd_isset(hndl->private, &fdvar)) {
				upnp_dispatch(hndl->private, &fdvar);
				ret_hndl = hndl;
				break;
			}
		}
		else
#endif /* WPS_UPNP_DEVICE */
		if (FD_ISSET(hndl->handle, &fdvar)) {
			/*
			 * Specially hacking eap handle to
			 * retrieve eap wl ifname.
			 */
			num = 0;
			if (hndl->type == WPS_RECEIVE_PKT_EAP) {
				iov[num].iov_base = hndl->ifname;
				iov[num].iov_len = sizeof(hndl->ifname);
				num++;
			}
			iov[num].iov_base = (void *)dataBuffer;
			iov[num].iov_len = *dataLen;

			/* Setup message */
			memset(&dst, 0, sizeof(dst));
			memset(&msg, 0, sizeof(msg));
			msg.msg_name = (void *)&dst;
			msg.msg_namelen = sizeof(dst);
			msg.msg_iov = iov;
			msg.msg_iovlen = num+1;

			/* Read this message */
			bytes = recvmsg(hndl->handle, &msg, 0);
			bytes -= num * sizeof(hndl->ifname);
			if (bytes <= 0) {
				TUTRACE((TUTRACE_ERR,
					"recv failed, recvBytes = %d\n", bytes));
				break;
			}

			*dataLen = bytes;
			ret_hndl = hndl;
			break;
		}

		hndl = hndl->next;
	}

out:

#ifdef WPS_UPNP_DEVICE
	/*
	 * Update upnp_timeout.
	 * We only have one libupnp handle and return libupnp handle when
	 * no other handle selected.
	 * Return libupnp handle makes wps_process_msg() to call wps_upnp_process_msg()
	 * to retrieve wps_libupnp_ProcessMsg() return code.
	 */
	hndl = hndl_list;
	while (hndl) {
		if (hndl->type == WPS_RECEIVE_PKT_UPNP) {
			upnp_timeout(hndl->private);
			if (ret_hndl == NULL)
				ret_hndl = hndl;
			break; /* We only have one libupnp handle */
		}
		hndl = hndl->next;
	}
#endif /* WPS_UPNP_DEVICE */

	return ret_hndl;
}

/*
 * WPS EAP Packet send functions
 */
static uint32
Eapol_SendDataDown(char * dataBuffer, uint32 dataLen)
{
	struct msghdr mh;
	struct iovec iov[2];
	struct sockaddr_in to;
	int eap_fd = wps_eap_get_handle();

	TUTRACE((TUTRACE_INFO, "Send EAPOL... buffer Length = %d\n",
		dataLen));

	if (!dataBuffer || !dataLen) {
		TUTRACE((TUTRACE_ERR, "Invalid Parameters\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	/* send to EAPD */
	to.sin_addr.s_addr = inet_addr(EAPD_WKSP_UDP_ADDR);
	to.sin_family = AF_INET;
	to.sin_port = htons(EAPD_WKSP_WPS_UDP_RPORT);

	/* outgoing interface name */
	iov[0].iov_base = (void *)if_name;
	iov[0].iov_len = IFNAMSIZ;
	iov[1].iov_base = dataBuffer;
	iov[1].iov_len = dataLen;

	memset(&mh, 0, sizeof(mh));
	mh.msg_name = (void *)&to;
	mh.msg_namelen = sizeof(to);
	mh.msg_iov = iov;
	mh.msg_iovlen = 2;

	if (sendmsg(eap_fd, &mh, 0) < 0) {
		TUTRACE((TUTRACE_ERR, "L2 send failed\n"));
		return TREAP_ERR_SENDRECV;
	}

	return WPS_SUCCESS;
}

uint32
wps_eapol_send_data(char *dataBuffer, uint32 dataLen)
{
	WPS_HexDumpAscii("==> To EAPOL", -1, dataBuffer, dataLen);

	return (Eapol_SendDataDown(dataBuffer, dataLen));
}

char *
wps_eapol_parse_msg(char *msg, int msg_len, int *len)
{
	/* just return msg and msg_len, the msg is a eapol raw date */
	*len = msg_len;
	return msg;
}

/*
 * WPS UI Packet send functions
 */
int
wps_osl_send_uimsg(wps_hndl_t *hndl, char *buf, int len)
{
	int n;

	n = sendto(hndl->handle, buf, len, 0, (struct sockaddr *)&dst,
		sizeof(struct sockaddr_in));

	/* Sleep 100 ms to make sure
	 * WPS have received socket
	 */
	WpsSleepMs(100);
	return n;
}

int
wps_set_ifname(char *ifname)
{
	strcpy(if_name, ifname);
	return 0;
}

/*
 * Debug hex dump
 */
void
WPS_HexDumpAscii(const char *title, int upnp_type, const unsigned char *buf,
	unsigned int len)
{
#ifdef _TUDEBUGTRACE
	int i, llen;
	const unsigned char *pos = buf;
	const int line_len = 16;

	const char *upnp_str = "";

	if (upnp_type != -1) {
		switch (upnp_type)
		{
		case UPNP_WPS_TYPE_SSR:
			upnp_str = "UPNP_WPS_TYPE_SSR";
			break;

		case UPNP_WPS_TYPE_PMR:
			upnp_str = "UPNP_WPS_TYPE_PMR";
			break;

		case UPNP_WPS_TYPE_GDIR:
			upnp_str = "UPNP_WPS_TYPE_GDIR";
			break;

		case UPNP_WPS_TYPE_PWR:
			upnp_str = "UPNP_WPS_TYPE_PWR";
			break;

		case UPNP_WPS_TYPE_WE:
			upnp_str = "UPNP_WPS_TYPE_WE";
			break;

		case UPNP_WPS_TYPE_DISCONNECT:
			upnp_str = "UPNP_WPS_TYPE_DISCONNECT";
			break;

		default:
			upnp_str = "UNKNOW TYPE";
			break;
		}
	}

	printf("WPS %s - %s - hexdump_ascii(len=%lu):\n", title, upnp_str, (unsigned long) len);
	while (len) {
		llen = len > line_len ? line_len : len;
		printf("    ");
		for (i = 0; i < llen; i++)
			printf(" %02x", pos[i]);
		for (i = llen; i < line_len; i++)
			printf("   ");
		printf("   ");
		for (i = 0; i < llen; i++) {
			if (isprint(pos[i]))
				printf("%c", pos[i]);
			else
				printf("*");
		}
		for (i = llen; i < line_len; i++)
			printf(" ");
		printf("\n");
		pos += llen;
		len -= llen;
	}
#else
	return;

#endif /* _TUDEBUGTRACE */
}
