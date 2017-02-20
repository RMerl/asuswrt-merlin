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

#ifdef RTCONFIG_QCA
#include <qca.h>
#endif

#define USBLED_NORMAL_PERIOD		1		/* second */
#define USBLED_URGENT_PERIOD		100 * 1000	/* microsecond */	

static int model = MODEL_UNKNOWN;

static int usb_busy, count = 0;
static struct itimerval itv;
static char *usb_path1 = NULL;
static char *usb_path2 = NULL;
static int status_usb = 0;
static int status_usb_old = 0;

static int got_usb2 = 0;
static int got_usb3 = 0;
#ifdef RTCONFIG_USB_XHCI
static int got_usb2_old = 0;
static int got_usb3_old = 0;
#endif

#if defined(RTCONFIG_M2_SSD)
static char *usb_path3 = NULL;
static int got_m2ssd = 0, got_m2ssd_old = 0;
#endif

static void
alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec  = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

static int
usb_status(void)
{
	if (usb_busy)
		return 0;
	else if (nvram_invmatch("usb_led1", "") || nvram_invmatch("usb_led2", ""))
		return 1;
	else
		return 0;
}

#ifdef RTCONFIG_USB_XHCI
static int
check_usb2(void)
{
	if (usb_busy)
		return 0;
	else if (have_usb3_led(model) && *nvram_safe_get("usb_led2"))
		return 1;
	else
		return 0;
}

static int
check_usb3(void)
{
	if (usb_busy)
		return 0;
	else if (have_usb3_led(model) && nvram_invmatch("usb_led1", ""))
		return 1;
	else
		return 0;
}
#endif

#ifdef RTCONFIG_M2_SSD
static int check_m2_ssd(void)
{
	if (usb_busy)
		return 0;
	else if (nvram_invmatch("usb_led3", ""))
		return 1;
	else
		return 0;
}
#endif

static void no_blink(int sig)
{
	alarmtimer(USBLED_NORMAL_PERIOD, 0);
	status_usb = -1;
	if (have_usb3_led(model)) {
		got_usb2 = -1;
		got_usb3 = -1;
	}
#if defined(RTCONFIG_M2_SSD)
	if (have_sata_led(model)) {
		got_m2ssd = -1;
		sata_led_control(nvram_match("usb_led3", "1")? LED_ON : LED_OFF);
	}
#endif
	usb_busy = 0;
	if (have_usb3_led(model) || have_sata_led(model))
		nvram_unset("usb_led_id");
}

#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
static void reset_status(int sig)
{
	status_usb_old = -1;
#ifdef RTCONFIG_USB_XHCI
	got_usb2_old = -1;
	got_usb3_old = -1;
#endif
#if defined(RTCONFIG_M2_SSD)
	got_m2ssd_old = -1;
#endif
}
#endif

static void blink(int sig)
{
	alarmtimer(0, USBLED_URGENT_PERIOD);
	usb_busy = 1;
}

static void usbled_exit(int sig)
{
	alarmtimer(0, 0);
	status_usb = 0;
	if (have_usb3_led(model)) {
		got_usb2 = 0;
		got_usb3 = 0;
	}
	usb_busy = 0;

	led_control(LED_USB, LED_OFF);
	if (have_usb3_led(model))
		led_control(LED_USB3, LED_OFF);
	if (have_sata_led(model))
		led_control(LED_SATA, LED_OFF);

	remove("/var/run/usbled.pid");
	exit(0);
}

static void usbled(int sig)
{
	usb_path1 = nvram_safe_get("usb_path1");
	usb_path2 = nvram_safe_get("usb_path2");
#if defined(RTCONFIG_M2_SSD)
	usb_path3 = nvram_safe_get("usb_path3");
#endif
	status_usb_old = status_usb;
	status_usb = usb_status();

#ifdef RTCONFIG_USB_XHCI
	if (have_usb3_led(model)) {
		got_usb2_old = got_usb2;
		got_usb2 = check_usb2();
		got_usb3_old = got_usb3;
		got_usb3 = check_usb3();
	}
#endif
#if defined(RTCONFIG_M2_SSD)
	got_m2ssd_old = got_m2ssd;
	got_m2ssd = check_m2_ssd();
#endif

	if (nvram_match("asus_mfg", "1")
#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
			|| !nvram_get_int("AllLED")
#endif
			)
		no_blink(sig);
	else if (!usb_busy
#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
			&& nvram_get_int("AllLED")
#endif
			)
	{
#ifdef RTCONFIG_USB_XHCI
		if (have_usb3_led(model)) {
			if (model==MODEL_RTN18U && (nvram_match("bl_version", "3.0.0.7") || nvram_match("bl_version", "1.0.0.0")))
			{
				if ((got_usb2 != got_usb2_old) || (got_usb3 != got_usb3_old)) {
					if (!got_usb2 && !got_usb3)
						led_control(LED_USB, LED_OFF);
					else
						led_control(LED_USB, LED_ON);
				}
			}
			else {
				if (got_usb2 != got_usb2_old) {
					if (got_usb2)
						led_control(LED_USB, LED_ON);
					else
						led_control(LED_USB, LED_OFF);
				}
				if (got_usb3 != got_usb3_old) {
					if (got_usb3)
						led_control(LED_USB3, LED_ON);
					else
						led_control(LED_USB3, LED_OFF);
				}
			}
		}
		else
#endif
#if defined(RTCONFIG_M2_SSD)
		if (got_m2ssd != got_m2ssd_old)
		{
			sata_led_control(got_m2ssd? LED_ON : LED_OFF);
		}
		else
#endif
		if (status_usb != status_usb_old)
		{
			if (status_usb)
				led_control(LED_USB, LED_ON);
			else
				led_control(LED_USB, LED_OFF);
		}
	}
	else
#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
	if (nvram_get_int("AllLED"))
#endif
	{
		if (strcmp(usb_path1, "storage") && strcmp(usb_path2, "storage"))
		{
			no_blink(sig);
		}
		else
		{
			int led_id = nvram_get_int("usb_led_id");

			if (led_id != LED_USB && led_id != LED_USB3
#if defined(RTCONFIG_M2_SSD)
			    && led_id != LED_SATA
#endif
			   )
				led_id = LED_USB;

			count = (count+1) % 20;
			/* 0123456710 */
			/* 1010101010 */
			if (((count % 2) == 0) && (count > 8))
				led_control(led_id, LED_ON);
			else
				led_control(led_id, LED_OFF);
			alarmtimer(0, USBLED_URGENT_PERIOD);
		}
	}
}

int 
usbled_main(int argc, char *argv[])
{
	FILE *fp;
	sigset_t sigs_to_catch;
	model = get_model();

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
#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
	sigaddset(&sigs_to_catch, SIGTSTP);
#endif
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

	signal(SIGALRM, usbled);
	signal(SIGTERM, usbled_exit);
	signal(SIGUSR1, blink);
	signal(SIGUSR2, no_blink);
#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
	signal(SIGTSTP, reset_status);
#endif
	alarmtimer(USBLED_NORMAL_PERIOD, 0);

	/* Most of time it goes to sleep */
	while (1)
	{
		pause();
	}

	return 0;
}
