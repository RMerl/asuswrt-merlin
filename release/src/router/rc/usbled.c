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
 * Copyright 2010, ASUSTeK Inc.
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

#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

#define USBLED_NORMAL_PERIOD		1		/* second */
#define USBLED_URGENT_PERIOD		100 * 1000	/* microsecond */	

static int usb_busy, count = 0;
static struct itimerval itv;
static char *usb_path1 = NULL;
static char *usb_path2 = NULL;
static int status_usb = 0;
static int status_usb_old = 0;

static void
alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec  = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

static int
usb_status()
{
	if (usb_busy)
		return 0;
	else if (nvram_invmatch("usb_path1", "") || nvram_invmatch("usb_path2", ""))
		return 1;
	else
		return 0;
}

static void no_blink(int sig)
{
//	dbG("\n\n\nreceive SIGUSR2 in usbled\n\n\n");

	alarmtimer(USBLED_NORMAL_PERIOD, 0);
	status_usb = -1;
	usb_busy = 0;
}
#if 0
static void wps_pbc(int sig)
{
	start_wps_pbc(0);
}
#endif
static void blink(int sig)
{
//	dbG("\n\n\nreceive SIGUSR1 to usbled\n\n\n");

	alarmtimer(0, USBLED_URGENT_PERIOD);
	usb_busy = 1;
}

static void usbled_exit(int sig)
{
	alarmtimer(0, 0);
	status_usb = 0;
        usb_busy = 0;
	led_control(LED_USB, LED_OFF);

        remove("/var/run/usbled.pid");
        exit(0);
}

static void usbled(int sig)
{
	usb_path1 = nvram_safe_get("usb_path1");
	usb_path2 = nvram_safe_get("usb_path2");
	status_usb_old = status_usb;
	status_usb = usb_status();

	if(nvram_match("asus_mfg", "1"))
		no_blink(sig);
	else if (!usb_busy)
	{
		if (status_usb != status_usb_old)
		{
			if (status_usb)
				led_control(LED_USB, LED_ON);
			else
				led_control(LED_USB, LED_OFF);
		}
	}
	else
	{
		if (strcmp(usb_path1, "storage") && strcmp(usb_path2, "storage"))
		{
			no_blink(sig);
		}
		else
		{
			count = (++count % 20);

			/* 0123456710 */
			/* 1010101010 */
			if (((count % 2) == 0) && (count > 8))
				led_control(LED_USB, LED_ON);
			else
				led_control(LED_USB, LED_OFF);
			alarmtimer(0, USBLED_URGENT_PERIOD);
		}
	}
}

int 
usbled_main(int argc, char *argv[])
{
	FILE *fp;
	sigset_t sigs_to_catch;

	/* write pid */
	if ((fp = fopen("/var/run/usbled.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	usb_busy = 0;

	/* set the signal handler */
	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGALRM);
	sigaddset(&sigs_to_catch, SIGTERM);
	sigaddset(&sigs_to_catch, SIGUSR1);
	sigaddset(&sigs_to_catch, SIGUSR2);
#if 0
	sigaddset(&sigs_to_catch, SIGTSTP);
#endif
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

	signal(SIGALRM, usbled);
	signal(SIGTERM, usbled_exit);
	signal(SIGUSR1, blink);
	signal(SIGUSR2, no_blink);
#if 0
	signal(SIGTSTP, wps_pbc);
#endif
	alarmtimer(USBLED_NORMAL_PERIOD, 0);

	/* Most of time it goes to sleep */
	while (1)
	{
		pause();
	}

	return 0;
}
