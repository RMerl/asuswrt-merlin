/* Network Authentication Service deamon (Ecos)
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: nas_ecos.c 241182 2011-02-17 21:50:03Z $
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <typedefs.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <bcmcrypto/md5.h>
#include <wlutils.h>

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <ecos_oslib.h>

#include <netconf.h>
#include <nvparse.h>

#include <proto/eap.h>
#include <nas_wksp.h>
#include <nas_radius.h>

#include <eapd.h>

extern int gettimeofday(struct timeval *tv, struct timezone *tz);
extern void sys_reboot(void);

static cyg_handle_t nas_main_hdl;
static char nas_main_stack[16*1024];
static cyg_thread nas_thread;
static int _nas_pid = 0, _nas_ready = 0;

static nas_wksp_t *nas_nwksp = NULL;

void
nas_sleep_ms(uint ms)
{
	cyg_tick_count_t ostick;

	ostick = ms / 10;
	cyg_thread_delay(ostick);
}

void
nas_rand128(uint8 *rand128)
{
	struct timeval tv;
	struct timezone tz;
	MD5_CTX md5;

	gettimeofday(&tv, &tz);
	tv.tv_sec ^= rand();
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char *) &tv, sizeof(tv));
	MD5Update(&md5, (unsigned char *) &tz, sizeof(tz));
	MD5Final(rand128, &md5);
}

static void
hup_hdlr(int sig)
{
	if (nas_nwksp)
		nas_nwksp->flags = NAS_WKSP_FLAG_SHUTDOWN;

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

void
nas_task_ready(void)
{
	_nas_ready = 1;
}

/* nas task entry */
int nas_main(void)
{
#ifdef BCMDBG
	char debug[8];
#endif
	/* clear _nas_ready */
	_nas_ready = 0;

	/* fill up NAS task pid */
	_nas_pid = oslib_pid();

	/* clear rootnwksp */
	nas_nwksp = NULL;

	/* alloc nas/wpa work space */
	if (!(nas_nwksp = nas_wksp_alloc_workspace())) {
		NASMSG("Unable to allocate work space memory. Quitting...\n");
		return -1;
	}

#ifdef BCMDBG
	/* verbose - 0:no | others:yes */
	/* for workspace */
	if (nas_safe_get_conf(debug, sizeof(debug), "nas_dbg") == 0)
		debug_nwksp = (int)atoi(debug);
#endif

	/* run main loop to dispatch message */
	nas_wksp_main_loop(nas_nwksp);

	return 0;
}

void
nas_reset_board()
{
	sys_reboot();
	return;
}

void
nasd_start(void)
{
	int wait_time = 1 * 100; /* 1 second */

	if (!_nas_pid ||
	     !oslib_waitpid(_nas_pid, NULL)) {
		cyg_thread_create(7,
		                     (cyg_thread_entry_t *)nas_main,
		                     (cyg_addrword_t)NULL,
		                     "NAS",
		                     (void *)nas_main_stack,
		                     sizeof(nas_main_stack),
		                     &nas_main_hdl,
		                     &nas_thread);
		cyg_thread_resume(nas_main_hdl);

		/* Make sure nas stared and initial completed. Otherwise,
		 * it may lost some wireless driver events.
		 */
		while (_nas_ready == 0 && wait_time > 0) {
			cyg_thread_delay(10);
			wait_time -= 10;
		}

		NASMSG("NAS task started\n");
	}
}

void
nasd_stop(void)
{
	if (_nas_pid) {
		hup_hdlr(15 /* SIGTERM */);

		/* wait until not running */
		while (oslib_waitpid(_nas_pid, NULL))
			cyg_thread_delay(10);
		_nas_pid = 0;
		NASMSG("NAS task stopped\n");
	}
}
