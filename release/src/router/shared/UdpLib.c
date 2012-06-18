/*
 * hostapd / Wi-Fi Simple Configuration
 * Code copied from Intel SDK
 * Copyright (C) 2008, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 * $Id: UdpLib.c,v 1.7 2008/05/15 09:10:57 Exp $
 * Copyright (c) 2005 Intel Corporation. All rights reserved.
 * Contact Information: Harsha Hegde  <harsha.hegde@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README, README_WSC and COPYING for more details.
 */

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>


/* Linux specific headers */
#if defined(linux) || defined(__ECOS)
#include <sys/time.h>
#endif /* linux */

#include <netinet/in.h>
#include <UdpLib.h>

/* Comment out the next line if debug strings are not needed */
/* #define U_DEBUG */

void DEBUGF(char *, ...);


/*
 * udp_open()
 *
 * This function opens a UDP socket and returns the socket to the calling
 * application. The application will use this socket in any subsequent calls.
 *
 * Returns socket handle on success or -1 if there is an error.
 */
int udp_open()
{
	int        new_sock;     /* temporary socket handle */
#ifdef _WIN32_REAL
	int           retval;
	WSADATA       wsa_data;
#endif // _WIN32_REAL

	DEBUGF("Entered udp_open\n");

#ifdef _WIN32_REAL
	retval = WSAStartup(MAKEWORD(2, 0), &wsa_data);
	if (retval != 0)
	{
		DEBUGF("WSAStartup call failed.\n");
		return -1;
	}
#endif /*  _WIN32_REAL */

	/* create INTERNET, udp datagram socket */
	new_sock = (int) socket(AF_INET, SOCK_DGRAM, 0);

	if (new_sock < 0) {
		DEBUGF("socket call failed.\n");
		return -1;
	}

	DEBUGF("Socket open successful, sd: %d\n", new_sock);

	return new_sock;
}


/*
 * udp_bind(int fd, int portno)
 *
 * This call is used typically by the server application to establish a
 * well known port.
 *
 * Returns 1 if succeeds and returns -1 in case of an error.
*/
int udp_bind(int fd, int portno)
{
	struct sockaddr_in binder;

	DEBUGF("Entered udp_bind\n");

	binder.sin_family = AF_INET;
	binder.sin_addr.s_addr = INADDR_ANY;
	binder.sin_port = htons(portno);

	/* bind protocol to socket */
	if (bind(fd, (struct sockaddr *)&binder, sizeof(binder)))
	{
		DEBUGF("bind call for socket [%d] failed.\n", fd);
		return -1;
	}

	DEBUGF("Binding successful for socket [%d]\n", fd);

	return 1;
}


/*
 * udp_close(int fd)
 *
 * Closes a UDP session. Closes the socket and returns.
 */
void udp_close(int fd)
{
	DEBUGF("Entered udp_close\n");

#ifdef _WIN32_REAL
	WSACleanup();
	closesocket(fd);
#endif // _WIN32_REAL

#if defined(__linux__) || defined(__ECOS)
	close(fd);
#endif 

	return;
}


/*
 * udp_write(int fd, char * buf, int len, struct sockaddr_in * to)
 *
 * This function is called by the application to send data.
 * fd - socket handle
 * buf - data buffer
 * len - byte count of data in buf
 * to - socket address structure that contains remote ip address and port
 *      where the data is to be sent
 *
 * Returns bytesSent if succeeded. If there is an error -1 is returned
 */
int udp_write(int fd, char * buf, int len, struct sockaddr_in * to)
{
	int            bytes_sent;

	DEBUGF("Entered udp_write: len %d\n", len);
	bytes_sent = sendto(fd, buf, len, 0,
	                    (struct sockaddr *) to,
	                    sizeof(struct sockaddr_in));
	if (bytes_sent < 0)
	{
		DEBUGF("sendto on socket %d failed\n", fd);
		return -1;
	}
	return bytes_sent;
}


/*
 * udp_read(int fd, char * buf, int len, struct sockaddr_in * from)
 *
 * This function is called by the application to receive data.
 * fd - socket handle
 * buf - data buffer in which the received data will be put
 * len - size of buf in bytes
 * from - socket address structure that contains remote ip address and port
 *        from where the data is received
 *
 * Returns bytes received if succeeded. If there is an error -1 is returned
 */
int udp_read(int fd, char * buf, int len, struct sockaddr_in * from)
{
	int bytes_recd = 0;
	unsigned int fromlen = 0;
	fd_set        fdvar;
	int            sel_ret;

	DEBUGF("Entered udp_read\n");

	FD_ZERO(&fdvar);
	/* we have to wait for only one descriptor */
	FD_SET(fd, &fdvar);

	sel_ret = select(fd + 1, &fdvar, (fd_set *) 0, (fd_set *) 0, NULL);

	/* if select returns negative value, system error */
	if (sel_ret < 0)
	{
		DEBUGF("select call failed; system error\n");
		return -1;
	}

	/* Otherwise Read notification might has come, since we are
	   waiting for only one fd we need not check the bit. Go ahead
	   and read the packet
	*/
	fromlen = sizeof(struct sockaddr_in);
	bytes_recd = recvfrom(fd, buf, len, 0,
	                      (struct sockaddr *)from, &fromlen);
	if (bytes_recd < 0)
	{
		DEBUGF("recvfrom on socket %d failed\n", fd);
		return -1;
	}

	DEBUGF("Read %d bytes\n", bytes_recd);

	return bytes_recd;
}

/*
 * udp_read_timed(int fd, char * buf, int len,
 *                 struct sockaddr_in * from, int timeout)
 *
 * This function is called by the application to receive data.
 * fd - socket handle
 * buf - data buffer in which the received data will be put
 * len - size of buf in bytes
 * from - socket address structure that contains remote ip address and port
 *        from where the data is received
 * timeout - wait time in seconds
 *
 * Returns bytes received if succeeded. If there is an error -1 is returned
 */

int udp_read_timed(int fd, char * buf, int len,
                   struct sockaddr_in * from, int timeout)
{
	int bytes_recd = 0;
	unsigned int fromlen = 0;
	fd_set        fdvar;
	int            sel_ret;
	struct timeval tv;

	DEBUGF("Entered udp_read\n");

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	FD_ZERO(&fdvar);
	/* we have to wait for only one descriptor */
	FD_SET(fd, &fdvar);
	sel_ret = select(fd + 1, &fdvar, (fd_set *) 0, (fd_set *) 0, &tv);
	/* if select returns negative value, system error */
	if (sel_ret < 0)
	{
		DEBUGF("select call failed; system error\n");
		return -1;
	}
	else if (sel_ret == 0)
	{
		DEBUGF("select call timed out\n");
		return -1;
	}

	/* Otherwise Read notification might has come, since we are
	   waiting for only one fd we need not check the bit. Go ahead
	   and read the packet
	*/
	fromlen = sizeof(struct sockaddr_in);
	bytes_recd = recvfrom(fd, buf, len, 0,
	                      (struct sockaddr *)from, &fromlen);
	if (bytes_recd < 0)
	{
		DEBUGF("recvfrom on socket %d failed\n", fd);
		return -1;
	}

	DEBUGF("Read %d bytes\n", bytes_recd);

	return bytes_recd;
}

void DEBUGF(char * strFormat, ...)
{
#ifdef U_DEBUG
	char     szTraceMsg[1000];
	va_list  lpArgv;

	va_start(lpArgv, strFormat);
	vsprintf(szTraceMsg, strFormat, lpArgv);
	va_end(lpArgv);

	fprintf(stdout, "UdpLib: %s", szTraceMsg);
#endif
}
