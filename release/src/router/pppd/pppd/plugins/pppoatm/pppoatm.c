/* pppoatm.c - pppd plugin to implement PPPoATM protocol.
 *
 * Copyright 2000 Mitchell Blank Jr.
 * Based in part on work from Jens Axboe and Paul Mackerras.
 * Updated to ppp-2.4.1 by Bernhard Kaindl
 *
 * Updated to ppp-2.4.2 by David Woodhouse 2004.
 *  - disconnect method added
 *  - remove_options() abuse removed.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "pppd.h"
#include "pathnames.h"
#include "fsm.h" /* Needed for lcp.h to include cleanly */
#include "lcp.h"
#include <atm.h>
#include <linux/atmdev.h>
#include <linux/atmppp.h>
#include <sys/stat.h>
#include <net/if.h>
#include <sys/ioctl.h>

const char pppd_version[] = VERSION;

static struct sockaddr_atmpvc pvcaddr;
static char *qosstr = NULL;
static bool llc_encaps = 0;
static bool vc_encaps = 0;
static int device_got_set = 0;
static int pppoatm_max_mtu, pppoatm_max_mru;
static int setdevname_pppoatm(const char *cp, const char **argv, int doit);
struct channel pppoa_channel;
static int pppoa_fd = -1;

static option_t pppoa_options[] = {
	{ "device name", o_wild, (void *) &setdevname_pppoatm,
	  "ATM service provider IDs: VPI.VCI",
	  OPT_DEVNAM | OPT_PRIVFIX | OPT_NOARG  | OPT_A2STRVAL | OPT_STATIC,
	  devnam},
	{ "llc-encaps", o_bool, &llc_encaps,
	  "use LLC encapsulation for PPPoATM", 1},
	{ "vc-encaps", o_bool, &vc_encaps,
	  "use VC multiplexing for PPPoATM (default)", 1},
	{ "qos", o_string, &qosstr,
	  "set QoS for PPPoATM connection", 1},
	{ NULL }
};

/* returns:
 *  -1 if there's a problem with setting the device
 *   0 if we can't parse "cp" as a valid name of a device
 *   1 if "cp" is a reasonable thing to name a device
 * Note that we don't actually open the device at this point
 * We do need to fill in:
 *   devnam: a string representation of the device
 *   devstat: a stat structure of the device.  In this case
 *     we're not opening a device, so we just make sure
 *     to set up S_ISCHR(devstat.st_mode) != 1, so we
 *     don't get confused that we're on stdin.
 */
int (*old_setdevname_hook)(const char* cp) = NULL;
static int setdevname_pppoatm(const char *cp, const char **argv, int doit)
{
	struct sockaddr_atmpvc addr;
	extern struct stat devstat;
	if (device_got_set)
		return 0;
	//info("PPPoATM setdevname_pppoatm: '%s'", cp);
	memset(&addr, 0, sizeof addr);
	if (text2atm(cp, (struct sockaddr *) &addr, sizeof(addr),
	    T2A_PVC | T2A_NAME) < 0) {
               if(doit)
                   info("atm does not recognize: %s", cp);
		return 0;
           }
	if (!doit) return 1;
	//if (!dev_set_ok()) return -1;
	memcpy(&pvcaddr, &addr, sizeof pvcaddr);
	strlcpy(devnam, cp, sizeof devnam);
	devstat.st_mode = S_IFSOCK;
	if (the_channel != &pppoa_channel) {
		the_channel = &pppoa_channel;
		lcp_wantoptions[0].neg_accompression = 0;
		lcp_allowoptions[0].neg_accompression = 0;
		lcp_wantoptions[0].neg_asyncmap = 0;
		lcp_allowoptions[0].neg_asyncmap = 0;
		lcp_wantoptions[0].neg_pcompression = 0;
	}
	info("PPPoATM setdevname_pppoatm - SUCCESS:%s", cp);
	device_got_set = 1;
	return 1;
}

#define pppoatm_overhead() (llc_encaps ? 6 : 2)

static void no_device_given_pppoatm(void)
{
	fatal("No vpi.vci specified");
}

static void set_line_discipline_pppoatm(int fd)
{
	struct atm_backend_ppp be;
	be.backend_num = ATM_BACKEND_PPP;
	if (!llc_encaps)
		be.encaps = PPPOATM_ENCAPS_VC;
	else if (!vc_encaps)
		be.encaps = PPPOATM_ENCAPS_LLC;
	else
		be.encaps = PPPOATM_ENCAPS_AUTODETECT;
	if (ioctl(fd, ATM_SETBACKEND, &be) < 0)
		fatal("ioctl(ATM_SETBACKEND): %m");
}

#if 0
static void reset_line_discipline_pppoatm(int fd)
{
	atm_backend_t be = ATM_BACKEND_RAW;
	/* 2.4 doesn't support this yet */
	(void) ioctl(fd, ATM_SETBACKEND, &be);
}
#endif

static int connect_pppoatm(void)
{
	int fd;
	struct atm_qos qos;

	if (!device_got_set)
		no_device_given_pppoatm();
	fd = socket(AF_ATMPVC, SOCK_DGRAM, 0);
	if (fd < 0)
		fatal("failed to create socket: %m");
	memset(&qos, 0, sizeof qos);
	qos.txtp.traffic_class = qos.rxtp.traffic_class = ATM_UBR;
	/* TODO: support simplified QoS setting */
	if (qosstr != NULL)
		if (text2qos(qosstr, &qos, 0))
			fatal("Can't parse QoS: \"%s\"");
	qos.txtp.max_sdu = lcp_allowoptions[0].mru + pppoatm_overhead();
	qos.rxtp.max_sdu = lcp_wantoptions[0].mru + pppoatm_overhead();
	qos.aal = ATM_AAL5;
	if (setsockopt(fd, SOL_ATM, SO_ATMQOS, &qos, sizeof(qos)) < 0)
		fatal("setsockopt(SO_ATMQOS): %m");
	/* TODO: accept on SVCs... */
	if (connect(fd, (struct sockaddr *) &pvcaddr,
	    sizeof(struct sockaddr_atmpvc)))
		fatal("connect(%s): %m", devnam);
	pppoatm_max_mtu = lcp_allowoptions[0].mru;
	pppoatm_max_mru = lcp_wantoptions[0].mru;
	set_line_discipline_pppoatm(fd);
	strlcpy(ppp_devnam, devnam, sizeof(ppp_devnam));
	pppoa_fd = fd;
	return fd;
}

static void disconnect_pppoatm(void)
{
	close(pppoa_fd);
}

void plugin_init(void)
{
#if defined(__linux__)
	extern int new_style_driver;	/* From sys-linux.c */
	if (!ppp_available() && !new_style_driver)
		fatal("Kernel doesn't support ppp_generic - "
		    "needed for PPPoATM");
#else
	fatal("No PPPoATM support on this OS");
#endif
	info("PPPoATM plugin_init");
	add_options(pppoa_options);
}
struct channel pppoa_channel = {
    options: pppoa_options,
    process_extra_options: NULL,
    check_options: NULL,
    connect: &connect_pppoatm,
    disconnect: &disconnect_pppoatm,
    establish_ppp: &generic_establish_ppp,
    disestablish_ppp: &generic_disestablish_ppp,
    send_config: NULL,
    recv_config: NULL,
    close: NULL,
    cleanup: NULL
};
