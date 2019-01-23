/***********************************************************************
*
* plugin.c
*
* pppd plugin for kernel-mode PPPoE on Linux
*
* Copyright (C) 2001-2012 by Roaring Penguin Software Inc.
* Portions copyright 2000 Michal Ostrowski and Jamal Hadi Salim.
*
* Much code and many ideas derived from pppoe plugin by Michal
* Ostrowski and Jamal Hadi Salim, which carries this copyright:
*
* Copyright 2000 Michal Ostrowski <mostrows@styx.uwaterloo.ca>,
*                Jamal Hadi Salim <hadi@cyberus.ca>
* Borrows heavily from the PPPoATM plugin by Mitchell Blank Jr.,
* which is based in part on work from Jens Axboe and Paul Mackerras.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version
* 2 of the License, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id$";

#define _GNU_SOURCE 1
#include "pppoe.h"

#include "pppd/pppd.h"
#include "pppd/fsm.h"
#include "pppd/lcp.h"
#include "pppd/ipcp.h"
#include "pppd/ccp.h"
/* #include "pppd/pathnames.h" */

#include <linux/types.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <linux/ppp_defs.h>
#include <linux/if_pppox.h>

#ifndef _ROOT_PATH
#define _ROOT_PATH ""
#endif

#define _PATH_ETHOPT         _ROOT_PATH "/ppp/options."

char pppd_version[] = VERSION;

static int seen_devnam[2] = {0, 0};
static char *pppoe_reqd_mac = NULL;
static char *host_uniq = NULL;

/* From sys-linux.c in pppd -- MUST FIX THIS! */
extern int new_style_driver;

char *pppd_pppoe_service = NULL;
static char *acName = NULL;
static char *existingSession = NULL;
static int printACNames = 0;

static int PPPoEDevnameHook(char *cmd, char **argv, int doit);
static option_t Options[] = {
    { "device name", o_wild, (void *) &PPPoEDevnameHook,
      "PPPoE device name",
      OPT_DEVNAM | OPT_PRIVFIX | OPT_NOARG  | OPT_A2STRVAL | OPT_STATIC,
      devnam},
    { "rp_pppoe_service", o_string, &pppd_pppoe_service,
      "Desired PPPoE service name" },
    { "rp_pppoe_ac",      o_string, &acName,
      "Desired PPPoE access concentrator name" },
    { "rp_pppoe_sess",    o_string, &existingSession,
      "Attach to existing session (sessid:macaddr)" },
    { "rp_pppoe_verbose", o_int, &printACNames,
      "Be verbose about discovered access concentrators"},
    { "rp_pppoe_mac", o_string, &pppoe_reqd_mac,
      "Only connect to specified MAC address" },
    { "host-uniq", o_string, &host_uniq,
      "Specify custom Host-Uniq" },
    { NULL }
};
int (*OldDevnameHook)(char *cmd, char **argv, int doit) = NULL;
static PPPoEConnection *conn = NULL;

/**********************************************************************
 * %FUNCTION: PPPOEInitDevice
 * %ARGUMENTS:
 * None
 * %RETURNS:
 *
 * %DESCRIPTION:
 * Initializes PPPoE device.
 ***********************************************************************/
static int
PPPOEInitDevice(void)
{
    conn = malloc(sizeof(PPPoEConnection));
    if (!conn) {
	fatal("Could not allocate memory for PPPoE session");
    }
    memset(conn, 0, sizeof(PPPoEConnection));
    if (acName) {
	SET_STRING(conn->acName, acName);
    }
    if (pppd_pppoe_service) {
	SET_STRING(conn->serviceName, pppd_pppoe_service);
    }
    SET_STRING(conn->ifName, devnam);
    conn->discoverySocket = -1;
    conn->sessionSocket = -1;
    conn->printACNames = printACNames;
    conn->discoveryTimeout = PADI_TIMEOUT;
    return 1;
}

/**********************************************************************
 * %FUNCTION: PPPOEConnectDevice
 * %ARGUMENTS:
 * None
 * %RETURNS:
 * Non-negative if all goes well; -1 otherwise
 * %DESCRIPTION:
 * Connects PPPoE device.
 ***********************************************************************/
static int
PPPOEConnectDevice(void)
{
    struct sockaddr_pppox sp;
    struct ifreq ifr;
    int s;

    /* Restore configuration */
    lcp_allowoptions[0].mru = conn->mtu;
    lcp_wantoptions[0].mru = conn->mru;

    /* Update maximum MRU */
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	error("Can't get MTU for %s: %m", conn->ifName);
	return -1;
    }
    strncpy(ifr.ifr_name, conn->ifName, sizeof(ifr.ifr_name));
    if (ioctl(s, SIOCGIFMTU, &ifr) < 0) {
	error("Can't get MTU for %s: %m", conn->ifName);
	close(s);
	return -1;
    }
    close(s);

    if (lcp_allowoptions[0].mru > ifr.ifr_mtu - TOTAL_OVERHEAD) {
	lcp_allowoptions[0].mru = ifr.ifr_mtu - TOTAL_OVERHEAD;
    }
    if (lcp_wantoptions[0].mru > ifr.ifr_mtu - TOTAL_OVERHEAD) {
	lcp_wantoptions[0].mru = ifr.ifr_mtu - TOTAL_OVERHEAD;
    }

    /* Open session socket before discovery phase, to avoid losing session */
    /* packets sent by peer just after PADS packet (noted on some Cisco    */
    /* server equipment).                                                  */
    /* Opening this socket just before waitForPADS in the discovery()      */
    /* function would be more appropriate, but it would mess-up the code   */
    conn->sessionSocket = socket(AF_PPPOX, SOCK_STREAM, PX_PROTO_OE);
    if (conn->sessionSocket < 0) {
	error("Failed to create PPPoE socket: %m");
	return -1;
    }

    if (host_uniq && !parseHostUniq(host_uniq, &conn->hostUniq))
	fatal("Illegal value for host-uniq option");

    if (acName) {
	SET_STRING(conn->acName, acName);
    }
    if (pppd_pppoe_service) {
	SET_STRING(conn->serviceName, pppd_pppoe_service);
    }

    strlcpy(ppp_devnam, devnam, sizeof(ppp_devnam));
    if (existingSession) {
	unsigned int mac[ETH_ALEN];
	int i, ses;
	if (sscanf(existingSession, "%d:%x:%x:%x:%x:%x:%x",
		   &ses, &mac[0], &mac[1], &mac[2],
		   &mac[3], &mac[4], &mac[5]) != 7) {
	    fatal("Illegal value for rp_pppoe_sess option");
	}
	conn->session = htons(ses);
	for (i=0; i<ETH_ALEN; i++) {
	    conn->peerEth[i] = (unsigned char) mac[i];
	}
    } else {
        conn->discoverySocket =
            openInterface(conn->ifName, Eth_PPPOE_Discovery, conn->myEth, NULL);
        discovery(conn);
	if (conn->discoveryState != STATE_SESSION) {
	    error("Unable to complete PPPoE Discovery");
	    goto ERROR;
	}
    }

    /* Set PPPoE session-number for further consumption */
    ppp_session_number = ntohs(conn->session);

    sp.sa_family = AF_PPPOX;
    sp.sa_protocol = PX_PROTO_OE;
    sp.sa_addr.pppoe.sid = conn->session;
    memcpy(sp.sa_addr.pppoe.dev, conn->ifName, IFNAMSIZ);
    memcpy(sp.sa_addr.pppoe.remote, conn->peerEth, ETH_ALEN);

    /* Set remote_number for ServPoET */
    sprintf(remote_number, "%02X:%02X:%02X:%02X:%02X:%02X",
	    (unsigned) conn->peerEth[0],
	    (unsigned) conn->peerEth[1],
	    (unsigned) conn->peerEth[2],
	    (unsigned) conn->peerEth[3],
	    (unsigned) conn->peerEth[4],
	    (unsigned) conn->peerEth[5]);

    warn("Connected to %02X:%02X:%02X:%02X:%02X:%02X via interface %s",
	 (unsigned) conn->peerEth[0],
	 (unsigned) conn->peerEth[1],
	 (unsigned) conn->peerEth[2],
	 (unsigned) conn->peerEth[3],
	 (unsigned) conn->peerEth[4],
	 (unsigned) conn->peerEth[5],
	 conn->ifName);

    script_setenv("MACREMOTE", remote_number, 0);

    if (connect(conn->sessionSocket, (struct sockaddr *) &sp,
		sizeof(struct sockaddr_pppox)) < 0) {
	error("Failed to connect PPPoE socket: %d %m", errno);
	goto ERROR;
    }

    return conn->sessionSocket;

 ERROR:
    close(conn->sessionSocket);
    conn->sessionSocket = -1;
    /* Send PADT to reset the session unresponsive at buggy nas */
    sendPADT(conn, NULL);
    if (!existingSession) {
	close(conn->discoverySocket);
	conn->discoverySocket = -1;
    }
    return -1;
}

static void
PPPOESendConfig(int mtu,
		u_int32_t asyncmap,
		int pcomp,
		int accomp)
{
    int sock;
    struct ifreq ifr;

    if (mtu > MAX_PPPOE_MTU) {
	if (debug) warn("Couldn't increase MTU to %d", mtu);
	mtu = MAX_PPPOE_MTU;
    }
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
	warn("Couldn't create IP socket: %m");
	return;
    }
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    ifr.ifr_mtu = mtu;
    if (ioctl(sock, SIOCSIFMTU, &ifr) < 0) {
	warn("ioctl(SIOCSIFMTU): %m");
	return;
    }
    (void) close (sock);
}


static void
PPPOERecvConfig(int mru,
		u_int32_t asyncmap,
		int pcomp,
		int accomp)
{
    if (mru > MAX_PPPOE_MTU && debug) {
	warn("Couldn't increase MRU to %d", mru);
    }
}

/**********************************************************************
 * %FUNCTION: PPPOEDisconnectDevice
 * %ARGUMENTS:
 * None
 * %RETURNS:
 * Nothing
 * %DESCRIPTION:
 * Disconnects PPPoE device
 ***********************************************************************/
static void
PPPOEDisconnectDevice(void)
{
    struct sockaddr_pppox sp;

    if (conn->sessionSocket < 0)
	goto ERROR;

    sp.sa_family = AF_PPPOX;
    sp.sa_protocol = PX_PROTO_OE;
    sp.sa_addr.pppoe.sid = 0;
    memcpy(sp.sa_addr.pppoe.dev, conn->ifName, IFNAMSIZ);
    memcpy(sp.sa_addr.pppoe.remote, conn->peerEth, ETH_ALEN);
    if (connect(conn->sessionSocket, (struct sockaddr *) &sp,
		sizeof(struct sockaddr_pppox)) < 0) {
	warn("Failed to disconnect PPPoE socket: %d %m", errno);
    }
    close(conn->sessionSocket);
    conn->sessionSocket = -1;

ERROR:
    /* Send PADT to reset the session unresponsive at buggy nas */
    sendPADT(conn, NULL);
    if (!existingSession) {
	close(conn->discoverySocket);
	conn->discoverySocket = -1;
    }
}

static void
PPPOEDeviceOptions(void)
{
    char buf[256];
    snprintf(buf, 256, _PATH_ETHOPT "%s",devnam);
    if(!options_from_file(buf, 0, 0, 1))
	exit(EXIT_OPTION_ERROR);

}

struct channel pppoe_channel;

/**********************************************************************
 * %FUNCTION: PPPoEDevnameHook
 * %ARGUMENTS:
 * cmd -- the command (actually, the device name
 * argv -- argument vector
 * doit -- if non-zero, set device name.  Otherwise, just check if possible
 * %RETURNS:
 * 1 if we will handle this device; 0 otherwise.
 * %DESCRIPTION:
 * Checks if name is a valid interface name; if so, returns 1.  Also
 * sets up devnam (string representation of device).
 ***********************************************************************/
static int
PPPoEDevnameHook(char *cmd, char **argv, int doit)
{
    int r = 1;
    int fd;
    struct ifreq ifr;
    int seen_idx = doit ? 1 : 0;

    /* If "devnam" has already been set, ignore.
       This prevents kernel from doing modprobes against random
       pppd arguments that happen to begin with "nic-", "eth" or "br"

       Ideally, "nix-ethXXX" should be supplied immediately after
       "plugin rp-pppoe.so"

       Patch based on suggestion from Mike Ireton.
    */
    if (seen_devnam[seen_idx]) {
	if (OldDevnameHook) return OldDevnameHook(cmd, argv, doit);
	return 0;
    }

    /* Only do it if name is "ethXXX" or "brXXX" or what was specified
       by rp_pppoe_dev option (ugh). */
    /* Can also specify nic-XXXX in which case the nic- is stripped off. */
    if (!strncmp(cmd, "nic-", 4)) {
	cmd += 4;
    } else {
	if (strncmp(cmd, "eth", 3) &&
	    strncmp(cmd, "br", 2)) {
	    if (OldDevnameHook) return OldDevnameHook(cmd, argv, doit);
	    return 0;
	}
    }

    /* Open a socket */
    if ((fd = socket(PF_PACKET, SOCK_RAW, 0)) < 0) {
	r = 0;
    }

    /* Try getting interface index */
    if (r) {
	strncpy(ifr.ifr_name, cmd, IFNAMSIZ);
	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
	    r = 0;
	} else {
	    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		r = 0;
	    } else {
		if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		    error("Interface %s not Ethernet", cmd);
		    r=0;
		}
	    }
	}
    }

    /* Close socket */
    close(fd);
    if (r) {
	seen_devnam[seen_idx] = 1;
	if (doit) {
	    strncpy(devnam, cmd, sizeof(devnam));
	    if (the_channel != &pppoe_channel) {

		the_channel = &pppoe_channel;
		modem = 0;

		lcp_allowoptions[0].neg_accompression = 0;
		lcp_wantoptions[0].neg_accompression = 0;

		lcp_allowoptions[0].neg_asyncmap = 0;
		lcp_wantoptions[0].neg_asyncmap = 0;

		lcp_allowoptions[0].neg_pcompression = 0;
		lcp_wantoptions[0].neg_pcompression = 0;

		ipcp_allowoptions[0].neg_vj=0;
		ipcp_wantoptions[0].neg_vj=0;

		ccp_allowoptions[0].deflate = 0 ;
		ccp_wantoptions[0].deflate = 0 ;

		ccp_allowoptions[0].bsd_compress = 0;
		ccp_wantoptions[0].bsd_compress = 0;


		PPPOEInitDevice();
	    }
	}
	return 1;
    }

    if (OldDevnameHook) r = OldDevnameHook(cmd, argv, doit);
    return r;
}

/**********************************************************************
 * %FUNCTION: plugin_init
 * %ARGUMENTS:
 * None
 * %RETURNS:
 * Nothing
 * %DESCRIPTION:
 * Initializes hooks for pppd plugin
 ***********************************************************************/
void
plugin_init(void)
{
    if (!ppp_available() && !new_style_driver) {
	fatal("Linux kernel does not support PPPoE -- are you running 2.4.x?");
    }

    add_options(Options);

    info("RP-PPPoE plugin version %s compiled against pppd %s",
	 RP_VERSION, VERSION);
}

/**********************************************************************
*%FUNCTION: fatalSys
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message plus the errno value to stderr and syslog and exits.
*
***********************************************************************/
void
fatalSys(char const *str)
{
    char buf[1024];
    int i = errno;
    sprintf(buf, "%.256s: %.256s", str, strerror(i));
    printErr(buf);
    sprintf(buf, "RP-PPPoE: %.256s: %.256s", str, strerror(i));
    sendPADT(conn, buf);
    exit(1);
}

/**********************************************************************
*%FUNCTION: rp_fatal
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message to stderr and syslog and exits.
***********************************************************************/
void
rp_fatal(char const *str)
{
    printErr(str);
    sendPADTf(conn, "RP-PPPoE: %.256s", str);
    exit(1);
}

/**********************************************************************
*%FUNCTION: sysErr
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message plus the errno value to syslog.
***********************************************************************/
void
sysErr(char const *str)
{
    char buf[1024];
    sprintf(buf, "%.256s: %.256s", str, strerror(errno));
    printErr(buf);
}

void pppoe_check_options(void)
{
    unsigned int mac[ETH_ALEN];
    int i;

    if (pppoe_reqd_mac != NULL) {
        if (sscanf(pppoe_reqd_mac, "%x:%x:%x:%x:%x:%x",
                   &mac[0], &mac[1], &mac[2], &mac[3],
                   &mac[4], &mac[5]) != ETH_ALEN) {
            option_error("cannot parse pppoe-mac option value");
            exit(EXIT_OPTION_ERROR);
        }
        for (i = 0; i < 6; ++i)
            conn->req_peer_mac[i] = mac[i];
        conn->req_peer = 1;
    }

    lcp_allowoptions[0].neg_accompression = 0;
    lcp_wantoptions[0].neg_accompression = 0;

    lcp_allowoptions[0].neg_asyncmap = 0;
    lcp_wantoptions[0].neg_asyncmap = 0;

    lcp_allowoptions[0].neg_pcompression = 0;
    lcp_wantoptions[0].neg_pcompression = 0;

    if (lcp_allowoptions[0].mru > MAX_PPPOE_MTU) {
        lcp_allowoptions[0].mru = MAX_PPPOE_MTU;
    }
    if (lcp_wantoptions[0].mru > MAX_PPPOE_MTU) {
        lcp_wantoptions[0].mru = MAX_PPPOE_MTU;
    }

    /* Save configuration */
    conn->mtu = lcp_allowoptions[0].mru;
    conn->mru = lcp_wantoptions[0].mru;

    ccp_allowoptions[0].deflate = 0;
    ccp_wantoptions[0].deflate = 0;

    ipcp_allowoptions[0].neg_vj = 0;
    ipcp_wantoptions[0].neg_vj = 0;

    ccp_allowoptions[0].bsd_compress = 0;
    ccp_wantoptions[0].bsd_compress = 0;
}

struct channel pppoe_channel = {
    .options = Options,
    .process_extra_options = &PPPOEDeviceOptions,
    .check_options = &pppoe_check_options,
    .connect = &PPPOEConnectDevice,
    .disconnect = &PPPOEDisconnectDevice,
    .establish_ppp = &generic_establish_ppp,
    .disestablish_ppp = &generic_disestablish_ppp,
    .send_config = &PPPOESendConfig,
    .recv_config = &PPPOERecvConfig,
    .close = NULL,
    .cleanup = NULL
};
