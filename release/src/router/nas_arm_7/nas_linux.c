/*
 * Network Authentication Service deamon (Linux)
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: nas_linux.c 375254 2012-12-18 05:07:38Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>

#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include <linux/filter.h>

#include <typedefs.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <bcmcrypto/md5.h>
#include <wlutils.h>

#include <nas_wksp.h>
#include <eapd.h>

static nas_wksp_t * nas_nwksp = NULL;

void
nas_sleep_ms(uint ms)
{
	usleep(1000*ms);
}

void
nas_rand128(uint8 *rand128)
{
	static int dev_random_fd = -1;
	struct timeval tv;
	struct timezone tz;
	MD5_CTX md5;
	if (dev_random_fd == -1)
		/* Use /dev/urandom because /dev/random, when it works at all,
		 * won't return anything when its entropy pool runs short and
		 * we can't control that.  /dev/urandom look good enough.
		 */
		dev_random_fd = open("/dev/urandom", O_RDONLY|O_NONBLOCK);
	if (dev_random_fd != -1)
		read(dev_random_fd, rand128, 16);
	else {
		gettimeofday(&tv, &tz);
		tv.tv_sec ^= rand();
		MD5Init(&md5);
		MD5Update(&md5, (unsigned char *) &tv, sizeof(tv));
		MD5Update(&md5, (unsigned char *) &tz, sizeof(tz));
		MD5Final(rand128, &md5);
	}
}

static void
hup_hdlr(int sig)
{
	if (nas_nwksp)
		nas_nwksp->flags = NAS_WKSP_FLAG_SHUTDOWN;
	return;
}

/* rekey event from console */
static void
rekey_hdlr(int sig)
{
	if (nas_nwksp) {
		nas_nwksp->flags = NAS_WKSP_FLAG_REKEY;
	}

	return;
}

/*
 * Configuration APIs
 */
int
nas_safe_get_conf(char *outval, int outval_size, char *name)
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

/* service main entry */
int
main(int argc, char *argv[])
{
	/* display usage if nothing is specified */
	if (argc == 2 &&
	    (!strncmp(argv[1], "-h", 2) ||
	     !strncmp(argv[1], "-H", 2))) {
		nas_wksp_display_usage();
		return 0;
	}

	/* alloc nas/wpa work space */
	if (!(nas_nwksp = nas_wksp_alloc_workspace())) {
		NASMSG("Unable to allocate work space memory. Quitting...\n");
		return 0;
	}

	if (argc > 1 && nas_wksp_parse_cmd(argc, argv, nas_nwksp)) {
		NASMSG("Command line parsing error. Quitting...\n");
		nas_wksp_free_workspace(nas_nwksp);
		return 0;
	}
	else {
#ifdef BCMDBG
		/* verbose - 0:no | others:yes */
		/* for workspace */
		char debug[8];
		if (nas_safe_get_conf(debug, sizeof(debug), "nas_dbg") == 0)
			debug_nwksp = (int)atoi(debug);
#endif
	}

	/* establish a handler to handle SIGTERM. */
	signal(SIGTERM, hup_hdlr);

	/* establish a handler to handle SIGUSR1. */
	signal(SIGUSR1, rekey_hdlr);

	/* run main loop to dispatch messages */
	nas_wksp_main_loop(nas_nwksp);

	return 0;
}

void
nas_reset_board()
{
	kill(1, SIGTERM);
	return;
}
