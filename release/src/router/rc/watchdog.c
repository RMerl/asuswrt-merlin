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
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */
#include <rc.h>
 
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <shutils.h>
#include <stdarg.h>
#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

#include <syslog.h>
#include <bcmnvram.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#define BCM47XX_SOFTWARE_RESET  0x40		/* GPIO 6 */
#define RESET_WAIT		2		/* seconds */
#define RESET_WAIT_COUNT	RESET_WAIT * 10 /* 10 times a second */

#define TEST_PERIOD		100		/* second */
#define NORMAL_PERIOD		1		/* second */
#define URGENT_PERIOD		100 * 1000	/* microsecond */	
#define RUSHURGENT_PERIOD	50 * 1000	/* microsecond */

#ifdef BTN_SETUP
#define SETUP_WAIT		3		/* seconds */
#define SETUP_WAIT_COUNT	SETUP_WAIT * 10 /* 10 times a second */
#define SETUP_TIMEOUT		60 		/* seconds */
#define SETUP_TIMEOUT_COUNT	SETUP_TIMEOUT * 10 /* 60 times a second */
#endif // BTN_SETUP
#define WPS_TIMEOUT_COUNT	121 * 20

#if 0
static int cpu_timer = 0;
static int ddns_timer = 1;
static int media_timer = 0;
static int mem_timer = -1;
static int u2ec_timer = 0;
#endif
struct itimerval itv;
int watchdog_period = 0;
int watchdog_count = 0;
static int btn_pressed = 0;
static int btn_count = 0;
long sync_interval = 1;	// every 10 seconds a unit
int sync_flag = 0;
long timestamp_g = 0;
int stacheck_interval = -1;
#ifdef BTN_SETUP
int btn_pressed_setup = 0;
int btn_pressed_flag = 0;
int btn_count_setup = 0;
int btn_count_timeout = 0;
int wsc_timeout = 0;
int btn_count_setup_second = 0;
int btn_pressed_toggle_radio = 0;
#endif

#ifdef RTCONFIG_WIRELESS_SWITCH
// for WLAN sw init, only for slide switch
static int wlan_sw_init = 0;
#endif


extern int g_wsc_configured;
extern int g_isEnrollee;

void 
sys_exit()
{
	printf("[watchdog] sys_exit");
	set_action(ACT_REBOOT);
	kill(1, SIGTERM);
}

static void
alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec  = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

extern int no_need_to_start_wps();

void led_control_normal(void)
{
	// the behaviro in normal when wps led != power led
	// wps led = on, power led = on

	led_control(LED_WPS, LED_ON);
	// in case LED_WPS != LED_POWER
	led_control(LED_POWER, LED_ON);
}
 
void btn_check(void)
{
#ifdef BTN_SETUP
	if (btn_pressed_setup == BTNSETUP_NONE)
	{
#endif

	if (button_pressed(BTN_RESET))
	{
		TRACE_PT("button RESET pressed\n");

	/*--------------- Add BTN_RST MFG test ------------------------*/
		if (nvram_match("asus_mfg", "1"))
		{
			nvram_set("btn_rst", "1");
		}
		else
		{
			if (!btn_pressed)
			{
				btn_pressed = 1;
				btn_count = 0;
				alarmtimer(0, URGENT_PERIOD);
			}
			else
			{	/* Whenever it is pushed steady */
				if (++btn_count > RESET_WAIT_COUNT)
				{
					fprintf(stderr, "You can release RESET button now!\n");

					btn_pressed = 2;
				}
				if (btn_pressed == 2)
				{
#ifdef RTCONFIG_DSL
					eval("adslate", "sysdefault");
#endif
				/* 0123456789 */
				/* 0011100111 */
					if ((btn_count % 10) < 2 || ((btn_count % 10) > 4 && (btn_count % 10) < 7))
						led_control(LED_POWER, LED_OFF);
					else
						led_control(LED_POWER, LED_ON);
				}
			}
		} // end BTN_RST MFG test
	}
#ifdef RTCONFIG_WIRELESS_SWITCH
	else if (button_pressed(BTN_WIFI_SW))
	{
		//TRACE_PT("button BTN_WIFI_SW pressed\n");	
		if (nvram_match("asus_mfg", "1"))
		{
			nvram_set("btn_wifi_sw", "1");
		}
		else
		{	
			if(wlan_sw_init == 0)
			{
				wlan_sw_init = 1;							
				
				eval("iwpriv", "ra0", "set", "RadioOn=1");
				eval("iwpriv", "rai0", "set", "RadioOn=1");
				TRACE_PT("Radio On\n");	
				nvram_set("wl0_radio", "1");
				nvram_set("wl1_radio", "1");
				nvram_commit(); 			
			}
			else
			{
				// if wlan switch on , btn reset routine goes here
				if (btn_pressed==2)
				{
					// IT MUST BE SAME AS BELOW CODE
					led_control(LED_POWER, LED_OFF);
					alarmtimer(0, 0);
					eval("mtd-erase","-d","nvram");
					/* FIXME: all stop-wan, umount logic will not be called
					 * prevous sys_exit (kill(1, SIGTERM) was ok
					 * since nvram isn't valid stop_wan should just kill possible daemons,
					 * nothing else, maybe with flag */
					sync();
					reboot(RB_AUTOBOOT);					
				}
			
				if(nvram_match("wl0_radio", "0") || nvram_match("wl1_radio", "0")){
					eval("iwpriv", "ra0", "set", "RadioOn=1");
					eval("iwpriv", "rai0", "set", "RadioOn=1");
					TRACE_PT("Radio On\n"); 
					nvram_set("wl0_radio", "1");
					nvram_set("wl1_radio", "1");
					nvram_commit(); 
				}
			}	
		}
	}
#endif	
	else
	{
		if (btn_pressed == 1)
		{
			btn_count = 0;
			btn_pressed = 0;
			led_control(LED_POWER, LED_ON);
			alarmtimer(NORMAL_PERIOD, 0);
		}
		else if (btn_pressed == 2)
		{
			led_control(LED_POWER, LED_OFF);
			alarmtimer(0, 0);
			eval("mtd-erase","-d","nvram");
			/* FIXME: all stop-wan, umount logic will not be called
			 * prevous sys_exit (kill(1, SIGTERM) was ok
			 * since nvram isn't valid stop_wan should just kill possible daemons,
			 * nothing else, maybe with flag */
			sync();
			reboot(RB_AUTOBOOT);
		}
#ifdef RTCONFIG_WIRELESS_SWITCH
		else
		{
			// no button is pressed or released
			if(wlan_sw_init == 0)
			{
				wlan_sw_init = 1;
				eval("iwpriv", "ra0", "set", "RadioOn=0");
				eval("iwpriv", "rai0", "set", "RadioOn=0");
				TRACE_PT("Radio Off\n");	
				nvram_set("wl0_radio", "0");
				nvram_set("wl1_radio", "0");
				nvram_commit(); 	
			}
			else
			{
				if(nvram_match("wl0_radio", "1") || nvram_match("wl1_radio", "1")){
					eval("iwpriv", "ra0", "set", "RadioOn=0");
					eval("iwpriv", "rai0", "set", "RadioOn=0");
					TRACE_PT("Radio Off\n");
					nvram_set("wl0_radio", "0");
					nvram_set("wl1_radio", "0");
				}
			}		
		}
#endif
	}

#ifdef BTN_SETUP
	}

	if (btn_pressed != 0) return;

	// Added WPS button radio toggle option
	if (button_pressed(BTN_WPS) && nvram_match("btn_ez_radiotoggle", "1") 
		&& (nvram_get_int("sw_mode") != SW_MODE_REPEATER)) // repeater mode not support HW radio
	{
		if (btn_pressed_toggle_radio == 0){
			eval("radio","toggle");
			btn_pressed_toggle_radio = 1;
			return;
		}
	}
	else{
		btn_pressed_toggle_radio = 0;
	}

	if (btn_pressed_setup < BTNSETUP_START)
	{
		if (!no_need_to_start_wps() && nvram_match("btn_ez_radiotoggle", "0") &&
			!nvram_match("wps_ign_btn", "1") && button_pressed(BTN_WPS))
		{
			TRACE_PT("button WPS pressed\n");

			/* Add BTN_EZ MFG test */
			if (nvram_match("asus_mfg", "1"))
			{
				nvram_set("btn_ez", "1");
			}
			else
			{
				if (btn_pressed_setup == BTNSETUP_NONE)
				{
					btn_pressed_setup = BTNSETUP_DETECT;
					btn_count_setup = 0;
					alarmtimer(0, RUSHURGENT_PERIOD);
				}
				else
				{	/* Whenever it is pushed steady */
					if (++btn_count_setup > SETUP_WAIT_COUNT)
					{
						btn_pressed_setup = BTNSETUP_START;
						btn_count_setup = 0;
						btn_count_setup_second = 0;
						nvram_set("wps_ign_btn", "1");
#if 0
						start_wps_pbc(0);	// always 2.4G
#else
						kill_pidfile_s("/var/run/wpsaide.pid", SIGTSTP);
#endif
						wsc_timeout = WPS_TIMEOUT_COUNT;
					}
				}
			} // end BTN_EZ MFG test
		} 
		else if (btn_pressed_setup == BTNSETUP_DETECT)
		{
			btn_pressed_setup = BTNSETUP_NONE;
			btn_count_setup = 0;
			led_control(LED_POWER, LED_ON);
			alarmtimer(NORMAL_PERIOD, 0);
		}
	}
	else 
	{
		if (!nvram_match("wps_ign_btn", "1")) {
			if (!no_need_to_start_wps() && button_pressed(BTN_WPS))
			{
				/* Whenever it is pushed steady, again... */
				if (++btn_count_setup_second > SETUP_WAIT_COUNT)
				{
					btn_pressed_setup = BTNSETUP_START;
					btn_count_setup_second = 0;
					nvram_set("wps_ign_btn", "1");
#if 0
					start_wps_pbc(0);	// always 2.4G
#else
					kill_pidfile_s("/var/run/wpsaide.pid", SIGTSTP);
#endif
					wsc_timeout = WPS_TIMEOUT_COUNT;
				}
			}

			if (is_wps_stopped() || --wsc_timeout == 0)
			{
				wsc_timeout = 0;

				btn_pressed_setup = BTNSETUP_NONE;
				btn_count_setup = 0;
				btn_count_setup_second = 0;

				led_control_normal();

				alarmtimer(NORMAL_PERIOD, 0);
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
#ifdef RTCONFIG_RALINK
				stop_wps_method();
#else
#endif
#endif
				return;
			}
		}

		++btn_count_setup;
		btn_count_setup = (btn_count_setup % 20);

		/* 0123456789 */
		/* 1010101010 */
		if ((btn_count_setup % 2) == 0 && (btn_count_setup > 10))
			led_control(LED_WPS, LED_ON);
		else
			led_control(LED_WPS, LED_OFF);
	}
#endif
}


#define DAYSTART (0)
#define DAYEND (60*60*23 + 60*59 + 59) // 86399
static int in_sched(int now_mins, int now_dow, int sched_begin, int sched_end, int sched_begin2, int sched_end2, int sched_dow)
{
	//cprintf("%s: now_mins=%d sched_begin=%d sched_end=%d sched_begin2=%d sched_end2=%d now_dow=%d sched_dow=%d\n", __FUNCTION__, now_mins, sched_begin, sched_end, sched_begin2, sched_end2, now_dow, sched_dow);
	int restore_dow = now_dow; // orig now day of week

	// wday: 0
	if((now_dow & 0x40) != 0){
		// under Sunday's sched time
		if(((now_dow & sched_dow) != 0) && (now_mins >= sched_begin2) && (now_mins <= sched_end2) && (sched_begin2 < sched_end2))
			return 1;

		// under Sunday's sched time and cross-night
		if(((now_dow & sched_dow) != 0) && (now_mins >= sched_begin2) && (sched_begin2 >= sched_end2))
			return 1;
		
		 // under Saturday's sched time
		now_dow >>= 6; // Saturday
		if(((now_dow & sched_dow) != 0) && (now_mins <= sched_end2) && (sched_begin2 >= sched_end2))
			return 1;

		// reset now_dow, avoid to check now_day = 0000001 (Sat)
		now_dow = restore_dow;
	}

	// wday: 1
	if((now_dow & 0x20) != 0){ 
		// under Monday's sched time
		if(((now_dow & sched_dow) != 0) && (now_mins >= sched_begin) && (now_mins <= sched_end) && (sched_begin < sched_end))
			return 1;

		// under Monday's sched time and cross-night
		if(((now_dow & sched_dow) != 0) && (now_mins >= sched_begin) && (sched_begin >= sched_end))
			return 1;

		// under Sunday's sched time
		now_dow <<= 1; // Sunday
		if(((now_dow & sched_dow) != 0) && (now_mins <= sched_end2) && (sched_begin2 >= sched_end2)) 
			return 1;
	}

	// wday: 2-5
	if((now_dow & 0x1e) != 0){
		// under today's sched time
		if(((now_dow & sched_dow) != 0) && (now_mins >= sched_begin) && (now_mins <= sched_end) && (sched_begin < sched_end))
			return 1;

		// under today's sched time and cross-night
		if(((now_dow & sched_dow) != 0) && (now_mins >= sched_begin) && (sched_begin >= sched_end))
			return 1;

		// under yesterday's sched time
		now_dow <<= 1; // yesterday
		if(((now_dow & sched_dow) != 0) && (now_mins <= sched_end) && (sched_begin >= sched_end))
			return 1; 
	}
	
	// wday: 6
	if((now_dow & 0x01) != 0){
		// under Saturday's sched time
		if(((now_dow & sched_dow) != 0) && (now_mins >= sched_begin2) && (now_mins <= sched_end2) && (sched_begin2 < sched_end2))
			return 1;

		// under Saturday's sched time and cross-night
		if(((now_dow & sched_dow) != 0) && (now_mins >= sched_begin2) && (sched_begin2 >= sched_end2))
			return 1;
		
		// under Friday's sched time
		now_dow <<= 1; // Friday
		if(((now_dow & sched_dow) != 0) && (now_mins <= sched_end) && (sched_begin >= sched_end)) 
			return 1; 
	}
	
	return 0;
}

int timecheck_item(char *activeDate, char *activeTime, char *activeTime2)
{
	int current, active, activeTimeStart, activeTimeEnd;
	int activeTimeStart2, activeTimeEnd2;
	int now_dow, sched_dow=0;
	time_t now;
	struct tm *tm;
	int i;

	setenv("TZ", nvram_safe_get("time_zone_x"), 1);

	time(&now);
	tm = localtime(&now);
	current = tm->tm_hour * 60 + tm->tm_min;
	active = 0;

	// weekdays time
	activeTimeStart = ((activeTime[0]-'0')*10 + (activeTime[1]-'0'))*60 + (activeTime[2]-'0')*10 + (activeTime[3]-'0');
	activeTimeEnd = ((activeTime[4]-'0')*10 + (activeTime[5]-'0'))*60 + (activeTime[6]-'0')*10 + (activeTime[7]-'0');

	// weekend time
	activeTimeStart2 = ((activeTime2[0]-'0')*10 + (activeTime2[1]-'0'))*60 + (activeTime2[2]-'0')*10 + (activeTime2[3]-'0');
	activeTimeEnd2 = ((activeTime2[4]-'0')*10 + (activeTime2[5]-'0'))*60 + (activeTime2[6]-'0')*10 + (activeTime2[7]-'0');

	// now day of week
	now_dow = 1<< (6-tm->tm_wday);

	// schedule day of week
	sched_dow = 0;
	for(i=0;i<=6;i++){
		sched_dow += (activeDate[i]-'0') << (6-i);
	}
	
	active = in_sched(current, now_dow, activeTimeStart, activeTimeEnd, activeTimeStart2, activeTimeEnd2, sched_dow);

	//cprintf("[watchdoe] active: %d\n", active);

	return active;
}

#ifdef RTCONFIG_RALINK
extern char *wif_to_vif(char *wif);
#endif

int svcStatus[8] = { -1, -1, -1, -1, -1, -1, -1, -1};

/* Check for time-reated service 	*/
/* 1. Wireless Radio			*/
/* 2. Guess SSID expire time 		*/
//int timecheck(int argc, char *argv[])
void timecheck(void)
{
	int activeNow;
	char *svcDate, *svcTime, *svcTime2;
	char prefix[]="wlXXXXXX_", tmp[100];
	char word[256], *next;
	int unit, item;
	char *lan_ifname;
	char wl_vifs[256], nv[40];
	int expire, need_commit = 0;

	item = 0;
	unit = 0;

	if (nvram_match("reload_svc_radio", "1"))
	{
		nvram_set("reload_svc_radio", "0");

		foreach (word, nvram_safe_get("wl_ifnames"), next) {
			svcStatus[item] = -1;
			item++;
			unit++;
		}

		item = 0;
		unit = 0;
	}

	// radio on/off
	if (nvram_match("svc_ready", "1"))
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		//dbG("[watchdog] timecheck unit=%s radio=%s\n", prefix, nvram_safe_get(strcat_r(prefix, "radio", tmp))); // radio toggle test
		if (nvram_match(strcat_r(prefix, "radio", tmp), "0")){
			item++;
			unit++;
			continue;
		}

		svcDate = nvram_safe_get(strcat_r(prefix, "radio_date_x", tmp));
		svcTime = nvram_safe_get(strcat_r(prefix, "radio_time_x", tmp));
		svcTime2 = nvram_safe_get(strcat_r(prefix, "radio_time2_x", tmp));

		activeNow = timecheck_item(svcDate, svcTime, svcTime2);
		snprintf(tmp, sizeof(tmp), "%d", unit);

		if(svcStatus[item]!=activeNow) {
			svcStatus[item] = activeNow;
			if(activeNow) eval("radio", "on", tmp);
			else eval("radio", "off", tmp);
		}
		item++;
		unit++;
	}

	// guest ssid expire check
	if (strlen(nvram_safe_get("wl0_vifs")) || strlen(nvram_safe_get("wl1_vifs")))
	{
		lan_ifname = nvram_safe_get("lan_ifname");
		sprintf(wl_vifs, "%s %s", nvram_safe_get("wl0_vifs"), nvram_safe_get("wl1_vifs"));

		foreach (word, wl_vifs, next) {
#ifdef RTCONFIG_RALINK
			snprintf(nv, sizeof(nv) - 1, "%s_expire_tmp", wif_to_vif(word));
#else
			snprintf(nv, sizeof(nv) - 1, "%s_expire_tmp", word);
#endif
			expire = atoi(nvram_safe_get(nv));
			
			if (expire)
			{
				if (expire <= 30)
				{
					nvram_set(nv, "0");
#ifdef RTCONFIG_RALINK
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", wif_to_vif(word));
#else
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", word);
#endif
					nvram_set(nv, "0");
					if (!need_commit) need_commit = 1;
#ifdef CONFIG_BCMWL5
					eval("wl", "-i", word, "closed", "1");
					eval("wl", "-i", word, "bss_maxassoc", "1");
					eval("wl", "-i", word, "bss", "down");
#endif
					ifconfig(word, 0, NULL, NULL);
					eval("brctl", "delif", lan_ifname, word);
				}
				else
				{
					expire -= 30;
					sprintf(tmp, "%d", expire);
					nvram_set(nv, tmp);
				}
			}
		}

		if (need_commit)
		{
			need_commit = 0;
			nvram_commit();
		}
	}

	return;
}

#if 0
int high_cpu_usage_count = 0;

void
cpu_usage_monitor()
{
	cpu_timer = (cpu_timer + 1) % 2;
	if (cpu_timer) return;

	unsigned int usage = cpu_main(0, NULL, 0);

	if (usage >= 95)
		high_cpu_usage_count++;
	else
		high_cpu_usage_count = 0;

	if (high_cpu_usage_count >= 5)
		sys_exit();
}
#endif

static void catch_sig(int sig)
{
	if (sig == SIGUSR1)
	{
		dbG("[watchdog] Handle WPS LED for WPS Start\n");

		alarmtimer(NORMAL_PERIOD, 0);

		btn_pressed_setup = BTNSETUP_START;
		btn_count_setup = 0;
		btn_count_setup_second = 0;
		wsc_timeout = WPS_TIMEOUT_COUNT;
		alarmtimer(0, RUSHURGENT_PERIOD);
	}
	else if (sig == SIGUSR2)
	{
		if (nvram_match("wps_ign_btn", "1")) return;

//		strcmp(wan_proto, "bigpond") == 0 ||
		dbG("[watchdog] Handle WPS LED for WPS Stop\n");

		btn_pressed_setup = BTNSETUP_NONE;
		btn_count_setup = 0;
		btn_count_setup_second = 0;
		wsc_timeout = 1;
		alarmtimer(NORMAL_PERIOD, 0);

		led_control_normal();
	}
	else if (sig == SIGTSTP)
	{
		dbG("[watchdog] Reset WPS timeout count\n");

		wsc_timeout = WPS_TIMEOUT_COUNT;
	}
#ifdef RTCONFIG_RALINK
	else if (sig == SIGTTIN)
	{
		wsc_user_commit();
	}
	else if (sig == SIGHUP)
	{
		int model = get_model();
		switch (model) {
		case MODEL_RTN56U:
#ifdef RTCONFIG_DSL
		case MODEL_DSLN55U:
#endif
			restart_wireless();
			break;
		}
	}
#endif
}

#if defined(RTCONFIG_BRCM_USBAP) || defined(RTAC66U)
unsigned long get_5g_count()
{
	FILE *f;
	char buf[256];
	char *ifname, *p;
	unsigned long counter1, counter2;

	if((f = fopen("/proc/net/dev", "r"))==NULL) return -1;

	fgets(buf, sizeof(buf), f);
	fgets(buf, sizeof(buf), f);

	counter1=counter2=0;

	while (fgets(buf, sizeof(buf), f)) {
		if((p=strchr(buf, ':'))==NULL) continue;
		*p = 0;
		if((ifname = strrchr(buf, ' '))==NULL) ifname = buf;
		else ++ifname;

		if(strcmp(ifname, "eth2")) continue;

		if(sscanf(p+1, "%lu%*u%*u%*u%*u%*u%*u%*u%*u%lu", &counter1, &counter2)!=2) continue; 

	}
	fclose(f);

	return counter1;
}

void fake_wl_led_5g(void)
{
	static unsigned int blink_5g_check = 0;
	static unsigned int blink_5g = 0;	
	static unsigned int data_5g = 0;
	unsigned long count_5g;	
	int i;
	static int j;
	static int status = -1;
	static int status_old;

	// check data per 10 count
	if((blink_5g_check%10)==0) {
		count_5g = get_5g_count();
		if(count_5g && data_5g!=count_5g) {
			blink_5g = 1;
			data_5g = count_5g;
		}
		else blink_5g = 0;
		led_control(LED_5G, LED_ON);
	}

	if(blink_5g) {
#ifdef RTAC66U
		j = rand_seed_by_time() % 3;
#endif
		for(i=0;i<10;i++) {
#ifdef RTAC66U
			usleep(33*1000);

			status_old = status;
			if (((i%2)==0) && (i > (3 + 2*j)))
				status = 0;
			else
				status = 1;

			if (status != status_old)
			{
				if (status)
					led_control(LED_5G, LED_ON);
				else
					led_control(LED_5G, LED_OFF);
			}
#else
			usleep(50*1000);
			if(i%2==0) {
				led_control(LED_5G, LED_OFF);
			}
			else {
				led_control(LED_5G, LED_ON);
			}
#endif
		}
		led_control(LED_5G, LED_ON);
	}
 
	blink_5g_check++;
}
#endif

void led_check(void)
{
#if defined(RTCONFIG_BRCM_USBAP) || defined(RTAC66U)
#ifdef RTAC66U
	if (nvram_match("led_5g", "1"))
#endif
	fake_wl_led_5g();
#endif

// it is not really necessary, but if required, add internet led check here
// using wan_primary_ifunit() to get current working wan unit wan0 or wan1
// using wan0_state_t or wan1_state_t to get status of working wan, 
//	WAN_STATE_CONNECTED means internet connected
//	else means internet disconnted

}

#ifdef RTCONFIG_SWMODE_SWITCH
// copied from 2.x code
int pre_sw_mode=0, sw_mode=0;
int flag_sw_mode=0;
int tmp_sw_mode=0;
int count_stable=0;

void swmode_check()
{
	char tmp[10];

	if(!nvram_get_int("swmode_switch")) return;

	pre_sw_mode = sw_mode;

	if(button_pressed(BTN_SWMODE_SW_REPEATER))
		sw_mode=SW_MODE_REPEATER;
	else if(button_pressed(BTN_SWMODE_SW_AP))
		sw_mode=SW_MODE_AP;
	else sw_mode=SW_MODE_ROUTER;
	
	if(sw_mode!=pre_sw_mode) {
		if(nvram_get_int("sw_mode")!=sw_mode) {
			flag_sw_mode=1;
			count_stable=0;
			tmp_sw_mode=sw_mode;
		}
		else flag_sw_mode=0;
	}
	else if(flag_sw_mode==1 && nvram_invmatch("asus_mfg", "1")) {	
		if(tmp_sw_mode==sw_mode) {
			if(++count_stable>4) // stable for more than 5 second
			{
				fprintf(stderr, "Reboot to switch sw mode ..\n");
				flag_sw_mode=0;			
				sync();
				reboot(RB_AUTOBOOT);
			}
		}
		else flag_sw_mode = 0;
	}
}
#endif

void ddns_check(void)
{
	if( nvram_match("ddns_enable_x", "1") &&
	   (nvram_match("wan0_state_t", "2") && nvram_match("wan0_auxstate_t", "0")) )
	{
		if (pids("ez-ipupdate")) //ez-ipupdate is running!
			return;

		if( nvram_match("ddns_server_x", "WWW.ASUS.COM") ){
			if((strcmp(nvram_safe_get("ddns_return_code_chk"),"Time-out") != 0) &&
		     	   (strcmp(nvram_safe_get("ddns_return_code_chk"),"connect_fail") != 0) ) {
				return;
			}
		}
		else{ //non asusddns service
			 if( nvram_match("ddns_updated", "1") )
				return;
		}
		logmessage("watchdog", "start ddns.");
		unlink("/tmp/ddns.cache");
		notify_rc("start_ddns");
	}
	return;
}

/* wathchdog is runned in NORMAL_PERIOD, 1 seconds
 * check in each NORMAL_PERIOD
 *	1. button
 *
 * check in each NORAML_PERIOD*10
 *
 *      1. time-dependent service
 */
void watchdog(int sig)
{
	/* handle button */
	btn_check();
	if(nvram_match("asus_mfg", "0"))
		led_check();

#ifdef RTCONFIG_SWMODE_SWITCH
 	swmode_check();
#endif

	/* if timer is set to less than 1 sec, then bypass the following */
	if (itv.it_value.tv_sec == 0) return;

	if (!nvram_match("asus_mfg", "0")) return;

	watchdog_period = (watchdog_period + 1) % 30;

	if (watchdog_period) return;

#ifdef BTN_SETUP
	if (btn_pressed_setup >= BTNSETUP_START) return;
#endif

	/* check for time-related services */
	timecheck();
#if 0
	cpu_usage_monitor();
#endif
	ddns_check();

	return;
}

int 
watchdog_main(int argc, char *argv[])
{
	FILE *fp;

	/* write pid */
	if ((fp = fopen("/var/run/watchdog.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

#ifdef RTCONFIG_SWMODE_SWITCH
	pre_sw_mode=nvram_get_int("sw_mode");
#endif

#ifdef RTCONFIG_RALINK
	doSystem("iwpriv %s set WatchdogPid=%d", WIF_2G, getpid());
	doSystem("iwpriv %s set WatchdogPid=%d", WIF_5G, getpid());
#endif

	/* set the signal handler */
	signal(SIGUSR1, catch_sig);
	signal(SIGUSR2, catch_sig);
	signal(SIGTSTP, catch_sig);
	signal(SIGALRM, watchdog);
#ifdef RTCONFIG_RALINK
	signal(SIGTTIN, catch_sig);
	int model = get_model();
	switch (model) {
	case MODEL_RTN56U:
#ifdef RTCONFIG_DSL
	case MODEL_DSLN55U:
#endif
	signal(SIGHUP, catch_sig);
	break;
	}
#endif

#ifdef RTCONFIG_DSL //Paul add 2012/6/27
	nvram_set("btn_rst", "0");
	nvram_set("btn_ez", "0");
	nvram_set("btn_wifi_sw", "0");
#endif

	nvram_unset("wps_ign_btn");

	if (!pids("ots"))
		start_ots();

	setenv("TZ", nvram_safe_get("time_zone_x"), 1);

	_dprintf("TZ watchdog\n");
	/* set timer */
	alarmtimer(NORMAL_PERIOD, 0);

	led_control_normal();

	/* Most of time it goes to sleep */
	while (1)
	{	
		pause();
	}

	return 0;
}
