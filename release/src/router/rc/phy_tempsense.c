/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Copyright 2011, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <bcmnvram.h>
#include <shared.h>
#include <shutils.h>
#include <wlutils.h>

#define FAN_NORMAL_PERIOD	5 * 1000	/* microsecond */
#define TEMP_MAX		94.000
#define TEMP_3			88.000
#define TEMP_2			82.000
#define TEMP_1			76.000
#define TEMP_MIN		70.000

#define max(a,b)  (((a) > (b)) ? (a) : (b))
#define min(a,b)  (((a) < (b)) ? (a) : (b))

static int count = -2;
static int status = -1;
static int duty_cycle = 0;
static int status_old = 0;
static double tempavg_24 = 0.000;
#if defined(RTAC5300) || defined(RTAC5300R)
static double tempavg_5l = 0.000;
static double tempavg_5h = 0.000;
#else
static double tempavg_50 = 0.000;
#endif
static double tempavg_max = 0.000;
static struct itimerval itv;
static int count_timer = 0;
static int base = 0;

static void
alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec  = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

static int
fan_status()
{
	int idx;

	if (!base)
		return 1;
	else if (base == 1)
		return 0;
	else
		idx = count_timer % base;

	if (!idx)
		return 0;
	else
		return 1;
}

static void
phy_tempsense_exit(int sig)
{
	alarmtimer(0, 0);
	led_control(FAN, FAN_OFF);

        remove("/var/run/phy_tempsense.pid");
        exit(0);
}

static int
phy_tempsense_mon()
{
	char buf[WLC_IOCTL_SMLEN];
	char buf2[WLC_IOCTL_SMLEN];
#if defined(RTAC5300) || defined(RTAC5300R)
	char buf3[WLC_IOCTL_SMLEN];
#endif
	int ret;
	unsigned int *ret_int = NULL;
	unsigned int *ret_int2 = NULL;
#if defined(RTAC5300) || defined(RTAC5300R)
	unsigned int *ret_int3 = NULL;
#endif

	strcpy(buf, "phy_tempsense");
	strcpy(buf2, "phy_tempsense");
#if defined(RTAC5300) || defined(RTAC5300R)
	strcpy(buf3, "phy_tempsense");
#endif
	if ((ret = wl_ioctl("eth1", WLC_GET_VAR, buf, sizeof(buf))))
		return ret;

	if ((ret = wl_ioctl("eth2", WLC_GET_VAR, buf2, sizeof(buf2))))
		return ret;
#if defined(RTAC5300) || defined(RTAC5300R)
	if ((ret = wl_ioctl("eth3", WLC_GET_VAR, buf3, sizeof(buf3))))
		return ret;
#endif

	ret_int = (unsigned int *)buf;
	ret_int2 = (unsigned int *)buf2;
#if defined(RTAC5300) || defined(RTAC5300R)
	ret_int3 = (unsigned int *)buf3;	
#endif

	if (count == -2)
	{
		count++;
		tempavg_24 = *ret_int;
#if defined(RTAC5300) || defined(RTAC5300R)
		tempavg_5l = *ret_int2;
		tempavg_5h = *ret_int3;
#else
		tempavg_50 = *ret_int2;
#endif
	}
	else
	{
		tempavg_24 = (tempavg_24 * 4 + *ret_int) / 5;
#if defined(RTAC5300) || defined(RTAC5300R)
		tempavg_5l = (tempavg_5l * 4 + *ret_int2) / 5;
		tempavg_5h = (tempavg_5h * 4 + *ret_int2) / 5;
#else
		tempavg_50 = (tempavg_50 * 4 + *ret_int2) / 5;
#endif
	}
#if 0
	tempavg_max = (((tempavg_24) > (tempavg_50)) ? (tempavg_24) : (tempavg_50));
#else 
#if defined(RTAC5300) || defined(RTAC5300R)
	tempavg_max = (tempavg_24 + tempavg_5l + tempavg_5h) / 2;
#else
	tempavg_max = (tempavg_24 + tempavg_50) / 2;
#endif
#endif
//	dbG("phy_tempsense 2.4G: %d (%.3f), 5G: %d (%.3f), Max: %.3f\n", 
//		*ret_int, tempavg_24, *ret_int2, tempavg_50, tempavg_max);

	if (duty_cycle && (tempavg_max < TEMP_MAX))
		base = duty_cycle + 1;
	else if (tempavg_max < TEMP_MIN)
		base = 1;
	else if (tempavg_max < TEMP_1)
		base = 2;
	else if (tempavg_max < TEMP_2)
		base = 3;
	else if (tempavg_max < TEMP_3)
		base = 4;
	else if (tempavg_max < TEMP_MAX)
		base = 5;
	else
		base = 0;

	if (!base)
		nvram_set_int("fanctrl_dutycycle_ex", 5);
	else
		nvram_set_int("fanctrl_dutycycle_ex", base - 1);

	return 0;
}

static void
phy_tempsense(int sig)
{
	int count_local = count_timer % 30;

	if (!count_local)
		phy_tempsense_mon();

        status_old = status;
        status = fan_status();
//	dbG("tempavg: %.3f, fan status: %d\n", tempavg_max, status);

	if (status != status_old)
	{
		if (status)
			led_control(FAN, FAN_ON);
		else
			led_control(FAN, FAN_OFF);
	}

	count_timer = (count_timer + 1) % 60;

	alarmtimer(0, FAN_NORMAL_PERIOD);
}

static void
update_dutycycle(int sig)
{
	alarmtimer(0, 0);

	count = -1;
	status = -1;
	count_timer = 0;

	duty_cycle = nvram_get_int("fanctrl_dutycycle");
	if ((duty_cycle < 0) || (duty_cycle > 4))
		duty_cycle = 0;

	dbG("\nduty cycle: %d\n", duty_cycle);

	phy_tempsense(sig);
}

int 
phy_tempsense_main(int argc, char *argv[])
{
	FILE *fp;
	sigset_t sigs_to_catch;

	/* write pid */
	if ((fp = fopen("/var/run/phy_tempsense.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	/* set the signal handler */
	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGALRM);
	sigaddset(&sigs_to_catch, SIGTERM);
	sigaddset(&sigs_to_catch, SIGUSR1);
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

	signal(SIGALRM, phy_tempsense);
	signal(SIGTERM, phy_tempsense_exit);
	signal(SIGUSR1, update_dutycycle);

	nvram_set_int("fanctrl_dutycycle_ex", base);

	duty_cycle = nvram_get_int("fanctrl_dutycycle");
	if ((duty_cycle < 0) || (duty_cycle > 4))
		duty_cycle = 0;

	dbG("\nduty cycle: %d\n", duty_cycle);

	alarmtimer(0, FAN_NORMAL_PERIOD);

	/* Most of time it goes to sleep */
	while (1)
	{
		pause();
	}

	return 0;
}

void
restart_fanctrl()
{
	kill_pidfile_s("/var/run/phy_tempsense.pid", SIGUSR1);	
}

