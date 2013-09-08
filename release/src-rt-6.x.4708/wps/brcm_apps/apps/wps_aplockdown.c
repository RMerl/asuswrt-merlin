/*
 * WPS aplockdown
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_aplockdown.c 409569 2013-06-26 03:01:15Z $
 */

#include <stdio.h>
#include <time.h>
#include <tutrace.h>
#include <wps.h>
#include <wps_wl.h>
#include <wps_aplockdown.h>
#include <wps_ui.h>
#include <wps_ie.h>

typedef struct {
	unsigned int force_on;
	unsigned int locked;
	unsigned int time;
	unsigned int start_cnt;
	unsigned int forever_cnt;
	int failed_cnt;
} WPS_APLOCKDOWN;

#define WPS_APLOCKDOWN_START_CNT	3
#define	WPS_APLOCKDOWN_DEF_FOREVER_CNT	4
#define	WPS_APLOCKDOWN_FOREVER_CNT	10

static WPS_APLOCKDOWN wps_aplockdown;

int
wps_aplockdown_init()
{
	char *p;
	int start_cnt = 0;
	int forever_cnt = 0;

	memset(&wps_aplockdown, 0, sizeof(wps_aplockdown));

	if (!strcmp(wps_safe_get_conf("wps_aplockdown_forceon"), "1")) {
		wps_aplockdown.force_on = 1;

		wps_ui_set_env("wps_aplockdown", "1");
		wps_set_conf("wps_aplockdown", "1");
	}
	else {
		wps_ui_set_env("wps_aplockdown", "0");
		wps_set_conf("wps_aplockdown", "0");
	}

	/* check lock start count */
	if ((p = wps_get_conf("wps_lock_start_cnt")))
		start_cnt = atoi(p);

	if (start_cnt < WPS_APLOCKDOWN_START_CNT ||
	    start_cnt > WPS_APLOCKDOWN_FOREVER_CNT) {
		/* Default start count */
		start_cnt = WPS_APLOCKDOWN_START_CNT;
	}

	/* check lock forever count */
	if ((p = wps_get_conf("wps_lock_forever_cnt")))
		forever_cnt = atoi(p);

	if (forever_cnt < WPS_APLOCKDOWN_START_CNT ||
	    forever_cnt > WPS_APLOCKDOWN_FOREVER_CNT) {
		/* Default forever lock count */
		forever_cnt = WPS_APLOCKDOWN_DEF_FOREVER_CNT;
	}

	if (start_cnt > forever_cnt)
		start_cnt = forever_cnt;

	/* Save to structure */
	wps_aplockdown.start_cnt = start_cnt;
	wps_aplockdown.forever_cnt = forever_cnt;

	TUTRACE((TUTRACE_INFO,
		"WPS aplockdown init: force_on = %d, start_cnt = %d, forever_cnt = %d!\n",
		wps_aplockdown.force_on,
		wps_aplockdown.start_cnt,
		wps_aplockdown.forever_cnt));

	return 0;
}

int
wps_aplockdown_add(void)
{
	unsigned long now;

	time((time_t *)&now);

	TUTRACE((TUTRACE_INFO, "Error of config AP pin fail in %d!\n", now));

	/*
	 * Add PIN failed count
	 */
	if (wps_aplockdown.failed_cnt < wps_aplockdown.forever_cnt)
		wps_aplockdown.failed_cnt++;

	/*
	 * Lock it if reach start count.
	 */
	if (wps_aplockdown.failed_cnt >= wps_aplockdown.start_cnt) {
		wps_aplockdown.locked = 1;
		wps_aplockdown.time = now;

		wps_ui_set_env("wps_aplockdown", "1");
		wps_set_conf("wps_aplockdown", "1");

		/* reset the IE */
		wps_ie_set(NULL, NULL);

		TUTRACE((TUTRACE_ERR, "AP is lock down now\n"));
	}

	TUTRACE((TUTRACE_INFO, "Fail AP pin trial count = %d!\n", wps_aplockdown.failed_cnt));

	return wps_aplockdown.locked;
}

int
wps_aplockdown_check(void)
{
	unsigned long now;
	int ageout;

	if (wps_aplockdown.locked == 0)
		return 0;

	/* check lock forever */
	if (wps_aplockdown.force_on ||
	    wps_aplockdown.failed_cnt >= wps_aplockdown.forever_cnt)
		return 1;

	/* wps_aplockdown.failed_cnt will always >= wps_aplockdown.start_cnt,
	 * so, ageout start from 1 minutes.
	 */
	ageout = (1 << (wps_aplockdown.failed_cnt - wps_aplockdown.start_cnt)) * 60;

	time((time_t *)&now);

	/* Lock release check */
	if ((unsigned long)(now - wps_aplockdown.time) > ageout) {
		/* unset apLockDown indicator */
		wps_aplockdown.locked = 0;

		wps_ui_set_env("wps_aplockdown", "0");
		wps_set_conf("wps_aplockdown", "0");

		/* reset the IE */
		wps_ie_set(NULL, NULL);

		TUTRACE((TUTRACE_INFO, "Unlock AP lock down\n"));
	}

	return wps_aplockdown.locked;
}

int
wps_aplockdown_islocked()
{
	return wps_aplockdown.locked | wps_aplockdown.force_on;
}

int
wps_aplockdown_cleanup()
{
	/* Cleanup dynamic variables */
	wps_aplockdown.locked = 0;
	wps_aplockdown.time = 0;
	wps_aplockdown.failed_cnt = 0;

	return 0;
}
