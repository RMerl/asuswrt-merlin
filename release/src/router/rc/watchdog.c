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
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <shutils.h>
#include <stdarg.h>
#include <netdb.h>
#include <arpa/inet.h>
#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif
#ifdef RTCONFIG_QCA
#include <qca.h>
#endif
#include <shared.h>

#include <syslog.h>
#include <bcmnvram.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifndef RTCONFIG_BCMARM
#include <math.h>
#endif
#include <string.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/sysinfo.h>
#if 0
#ifdef RTCONFIG_PUSH_EMAIL
#include <push_log.h>
#endif
#endif
#ifdef RTCONFIG_USER_LOW_RSSI
#if defined(RTCONFIG_RALINK)
#include <typedefs.h>
#else
#include <wlioctl.h>
#endif
#endif



#define BCM47XX_SOFTWARE_RESET	0x40		/* GPIO 6 */
#define RESET_WAIT		2		/* seconds */
#define RESET_WAIT_COUNT	RESET_WAIT * 10 /* 10 times a second */

#define TEST_PERIOD		100		/* second */
#define NORMAL_PERIOD		1		/* second */
#define URGENT_PERIOD		100 * 1000	/* microsecond */
#define RUSHURGENT_PERIOD	50 * 1000	/* microsecond */
#define DAY_PERIOD		2 * 60 * 24	/* 1 day (in 30 sec periods) */

#define WPS_TIMEOUT_COUNT	121 * 20
#ifdef RTCONFIG_WPS_LED
#define WPS_SUCCESS_COUNT	3
#endif
#define WPS_WAIT		1		/* seconds */
#define WPS_WAIT_COUNT		WPS_WAIT * 20	/* 20 times a second */

#ifdef RTCONFIG_WPS_RST_BTN
#define WPS_RST_DO_WPS_COUNT		( 1*10)	/*  1 seconds */
#define WPS_RST_DO_RESTORE_COUNT	(10*10)	/* 10 seconds */
#endif	/* RTCONFIG_WPS_RST_BTN */

//#if defined(RTCONFIG_JFFS2LOG) && defined(RTCONFIG_JFFS2)
#if defined(RTCONFIG_JFFS2LOG) && (defined(RTCONFIG_JFFS2)||defined(RTCONFIG_BRCM_NAND_JFFS2))
#define LOG_COMMIT_PERIOD	2		/* 2 x 30 seconds */
static int log_commit_count = 0;
#endif
#if defined(RTCONFIG_USB_MODEM) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
#define MODEM_FLOW_PERIOD	1
unsigned int modem_flow_count = 0;
#endif
#if defined(RTCONFIG_TOR) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2))
#define TOR_CHECK_PERIOD	10		/* 10 x 30 seconds */
unsigned int tor_check_count = 0;
#endif


#if 0
static int cpu_timer = 0;
static int ddns_timer = 1;
static int media_timer = 0;
static int mem_timer = -1;
static int u2ec_timer = 0;
#endif
static struct itimerval itv;
static int watchdog_period = 0;
#ifdef RTCONFIG_BCMARM
static int chkusb3_period = 0;
static int u3_chk_life = 6;
#endif
static int btn_pressed = 0;
static int btn_count = 0;
#ifdef BTN_SETUP
static int btn_pressed_setup = 0;
static int btn_count_setup = 0;
static int wsc_timeout = 0;
static int btn_count_setup_second = 0;
static int btn_pressed_toggle_radio = 0;
#endif
static long ddns_update_timer = 0;

#if defined(RTCONFIG_WIRELESS_SWITCH) && defined(RTCONFIG_DSL)
// for WLAN sw init, only for slide switch
static int wlan_sw_init = 0;
#elif defined(RTCONFIG_WIRELESS_SWITCH) && defined(RTCONFIG_QCA)
static int wifi_sw_old = -1;
#endif
#ifdef RTCONFIG_LED_BTN
static int LED_status_old = -1;
static int LED_status = -1;
static int LED_status_changed = 0;
static int LED_status_first = 1;
static int LED_status_on = -1;
#ifdef RTAC87U
static int LED_switch_count = 0;
static int BTN_pressed_count = 0;
#endif
#endif
static int wlonunit = -1;

extern int g_wsc_configured;
extern int g_isEnrollee[MAX_NR_WL_IF];
int g_wl0_led_status=0xFF;
int g_wl1_led_status=0xFF;

#define REGULAR_DDNS_CHECK	10 //10x30 sec
static int ddns_check_count = 0;
static int freeze_duck_count = 0;

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
	itv.it_value.tv_sec = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

extern int no_need_to_start_wps();

void led_control_normal(void)
{
#ifdef RTCONFIG_WPS_LED
	int v = LED_OFF;
	// the behavior in normal when wps led != power led
	// wps led = off, power led = on

	if (nvram_match("wps_success", "1"))
		v = LED_ON;
	__wps_led_control(v);
#else
	led_control(LED_WPS, LED_ON);
#endif
	// in case LED_WPS != LED_POWER

#if defined(RTCONFIG_LED_BTN) && defined(RTAC87U)
	LED_switch_count = nvram_get_int("LED_switch_count");
	if(nvram_get_int("AllLED") == 0) return;
#endif

	led_control(LED_POWER, LED_ON);

#if defined(RTN11P)
	led_control(LED_WPS, LED_ON);	//wps led is also 2g led. and NO power led.
#else
	if (nvram_get_int("led_pwr_gpio") != nvram_get_int("led_wps_gpio"))
		led_control(LED_WPS, LED_OFF);
#endif

}

void erase_nvram(void)
{
	switch (get_model()) {
		case MODEL_RTAC56S:
		case MODEL_RTAC56U:
		case MODEL_RTAC3200:
		case MODEL_RPAC68U:
		case MODEL_RTAC68U:
		case MODEL_DSLAC68U:
		case MODEL_RTAC87U:
			eval("mtd-erase2", "nvram");
			break;
		default:
			eval("mtd-erase","-d","nvram");
	}
}

int init_toggle(void)
{
	switch (get_model()) {
		case MODEL_RTAC56S:
		case MODEL_RTAC56U:
		case MODEL_RTAC3200:
		case MODEL_RPAC68U:
		case MODEL_RTAC68U:
		case MODEL_DSLAC68U:
		case MODEL_RTAC87U:
			nvram_set("btn_ez_radiotoggle", "1");
			return BTN_WIFI_TOG;
		default:
			return BTN_WPS;
	}
}

void btn_check(void)
{
	if (nvram_match("asus_mfg", "1"))
	{
		//TRACE_PT("asus mfg btn check!!!\n");
#ifndef RTCONFIG_N56U_SR2
		if (button_pressed(BTN_RESET))
		{
			TRACE_PT("button RESET pressed\n");
			nvram_set("btn_rst", "1");
		}
#endif
		if (button_pressed(BTN_WPS))
		{
			TRACE_PT("button WPS pressed\n");
			nvram_set("btn_ez", "1");
		}
#ifdef RTCONFIG_WIRELESS_SWITCH
		if (button_pressed(BTN_WIFI_SW))
		{
			TRACE_PT("button WIFI_SW pressed\n");
			nvram_set("btn_wifi_sw", "1");
		}
		else
		{
			nvram_set("btn_wifi_sw", "0");
		}
#endif
#if defined(RTCONFIG_WIFI_TOG_BTN)
		if (button_pressed(BTN_WIFI_TOG))
		{
			TRACE_PT("button WIFI_TOG pressed\n");
			nvram_set("btn_wifi_toggle", "1");
		}
#endif
#ifdef RTCONFIG_TURBO
		if (button_pressed(BTN_TURBO))
		{
			TRACE_PT("button TURBO pressed\n");
			nvram_set("btn_turbo", "1");
		}
#endif
#ifdef RTCONFIG_LED_BTN /* currently for RT-AC68U only */
#if defined(RTAC3200)
		if (!button_pressed(BTN_LED))
#else
		if (button_pressed(BTN_LED))
#endif
		{
			TRACE_PT("button LED pressed\n");
			nvram_set("btn_led", "1");
		}
#ifdef RTAC68U
		else
		{
			TRACE_PT("button LED released\n");
			nvram_set("btn_led", "0");
		}
#endif
#endif
#ifdef RT4GAC55U
		if (button_pressed(BTN_LTE))
		{
			TRACE_PT("button LTE pressed\n");
			nvram_set("btn_lte", "1");
		}
#endif
		return;
	}

#ifdef BTN_SETUP
	if (btn_pressed_setup == BTNSETUP_NONE)
	{
#endif
#ifndef RTCONFIG_N56U_SR2
	if (button_pressed(BTN_RESET))
#else
	if (0)
#endif
	{
		TRACE_PT("button RESET pressed\n");

	/*--------------- Add BTN_RST MFG test ------------------------*/
#ifndef RTCONFIG_WPS_RST_BTN
#ifdef RTCONFIG_DSL /* Paul add 2013/4/2 */
			if((btn_count % 2)==0)
				led_control(0, 1);
			else
				led_control(0, 0);
#endif
#endif	/* ! RTCONFIG_WPS_RST_BTN */
			if (!btn_pressed)
			{
				btn_pressed = 1;
				btn_count = 0;
				alarmtimer(0, URGENT_PERIOD);
			}
			else
			{	/* Whenever it is pushed steady */
#ifdef RTCONFIG_WPS_RST_BTN
				btn_count++;
#else	/* ! RTCONFIG_WPS_RST_BTN */
				if (++btn_count > RESET_WAIT_COUNT)
				{
					fprintf(stderr, "You can release RESET button now!\n");
					btn_pressed = 2;
				}
				if (btn_pressed == 2)
				{
#ifdef RTCONFIG_DSL /* Paul add 2013/4/2 */
					led_control(0, 0);
					alarmtimer(0, 0);
					nvram_set("restore_defaults", "1");
					if (notify_rc_after_wait("resetdefault")) {
						/* Send resetdefault rc_service failed. */
						alarmtimer(NORMAL_PERIOD, 0);
					}
#else
				/* 0123456789 */
				/* 0011100111 */
					if ((btn_count % 10) < 2 || ((btn_count % 10) > 4 && (btn_count % 10) < 7))
						led_control(LED_POWER, LED_OFF);
					else
						led_control(LED_POWER, LED_ON);
#endif
				}
#endif	/* ! RTCONFIG_WPS_RST_BTN */
			}
	}
#if defined(RTCONFIG_WIRELESS_SWITCH) && defined(RTCONFIG_DSL)
	else if (button_pressed(BTN_WIFI_SW))
	{
		//TRACE_PT("button BTN_WIFI_SW pressed\n");
			if(wlan_sw_init == 0)
			{
				wlan_sw_init = 1;
/*
				eval("iwpriv", "ra0", "set", "RadioOn=1");
				eval("iwpriv", "rai0", "set", "RadioOn=1");
				TRACE_PT("Radio On\n");
				nvram_set("wl0_radio", "1");
				nvram_set("wl1_radio", "1");
				nvram_commit();
*/
			}
			else
			{
				// if wlan switch on , btn reset routine goes here
				if (btn_pressed==2)
				{
					// IT MUST BE SAME AS BELOW CODE
					led_control(LED_POWER, LED_OFF);
					alarmtimer(0, 0);
					nvram_set("restore_defaults", "1");
					if(notify_rc_after_wait("resetdefault")) {
						/* Send resetdefault rc_service failed. */
						alarmtimer(NORMAL_PERIOD, 0);
					}
				}

				if(nvram_match("wl0_HW_switch", "0") || nvram_match("wl1_HW_switch", "0")){
					//Ever apply the Wireless-Professional Web GU.
					//Not affect the status of WiFi interface, so do nothing
				}
				else{	//trun on WiFi by HW slash, make sure both WiFi interface enable.
					if(nvram_match("wl0_radio", "0") || nvram_match("wl1_radio", "0")){
						eval("iwpriv", "ra0", "set", "RadioOn=1");
						eval("iwpriv", "rai0", "set", "RadioOn=1");
						TRACE_PT("Radio On\n");
						nvram_set("wl0_radio", "1");
						nvram_set("wl1_radio", "1");

						nvram_set("wl0_HW_switch", "0");
						nvram_set("wl1_HW_switch", "0");
						nvram_commit();
					}
				}
			}
	}
#endif	/* RTCONFIG_WIRELESS_SWITCH && RTCONFIG_DSL */
	else
	{
#ifdef RTCONFIG_WPS_RST_BTN
		if (btn_pressed == 0)
			;
		else if (btn_count < WPS_RST_DO_RESTORE_COUNT)
		{
			if (btn_count < WPS_RST_DO_RESTORE_COUNT && btn_count >= WPS_RST_DO_WPS_COUNT && nvram_match("btn_ez_radiotoggle", "1"))
			{
				radio_switch(0);
			}

			if (btn_count < WPS_RST_DO_RESTORE_COUNT && btn_count >= WPS_RST_DO_WPS_COUNT
			   && !no_need_to_start_wps()
			   && !wps_band_radio_off(get_radio_band(0))
			   && !wps_band_ssid_broadcast_off(get_radio_band(0))
			   && nvram_match("btn_ez_radiotoggle", "0")
			   && !nvram_match("wps_ign_btn", "1")
			)
			{
				btn_pressed_setup = BTNSETUP_DETECT;
				btn_count_setup = WPS_WAIT_COUNT;	//to trigger WPS
				alarmtimer(0, RUSHURGENT_PERIOD);
			}
			else
			{
				btn_pressed_setup = BTNSETUP_NONE;
				btn_count_setup = 0;
				alarmtimer(NORMAL_PERIOD, 0);
			}

			btn_count = 0;
			btn_pressed = 0;
			led_control(LED_POWER, LED_ON);
		}
		else if (btn_count >= WPS_RST_DO_RESTORE_COUNT)	// to do restore
#else	/* ! RTCONFIG_WPS_RST_BTN */
		if (btn_pressed == 1)
		{
			btn_count = 0;
			btn_pressed = 0;
			led_control(LED_POWER, LED_ON);
			alarmtimer(NORMAL_PERIOD, 0);
		}
		else if (btn_pressed == 2)
#endif	/* ! RTCONFIG_WPS_RST_BTN */
		{
			led_control(LED_POWER, LED_OFF);
			alarmtimer(0, 0);
			nvram_set("restore_defaults", "1");
			if (notify_rc_after_wait("resetdefault")) {
				/* Send resetdefault rc_service failed. */
				alarmtimer(NORMAL_PERIOD, 0);
			}
		}
#if defined(RTCONFIG_WIRELESS_SWITCH) && defined(RTCONFIG_DSL)
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

				nvram_set("wl0_HW_switch", "1");
				nvram_set("wl1_HW_switch", "1");

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

					nvram_set("wl0_timesched", "0");
					nvram_set("wl1_timesched", "0");
				}

				//indicate use switch HW slash manually.
				if(nvram_match("wl0_HW_switch", "0") || nvram_match("wl1_HW_switch", "0")){
					nvram_set("wl0_HW_switch", "1");
					nvram_set("wl1_HW_switch", "1");
				}
			}
		}
#endif	/* RTCONFIG_WIRELESS_SWITCH && RTCONFIG_DSL */
	}

#ifdef BTN_SETUP
	}

	if (btn_pressed != 0) return;
#ifdef CONFIG_BCMWL5
	// wait until wl is ready
	if (!nvram_get_int("wlready")) return;
#endif

#if defined(RTCONFIG_WIRELESS_SWITCH) && defined(RTCONFIG_QCA)
	if (wifi_sw_old != button_pressed(BTN_WIFI_SW))
	{
		wifi_sw_old = button_pressed(BTN_WIFI_SW);
		if(wifi_sw_old != 0 && (!nvram_match("wl0_radio", "1") || !nvram_match("wl1_radio", "1")))
		{
			TRACE_PT("button BTN_WIFI_SW pressed: ON\n");
			nvram_set("wl0_radio", "1");
			nvram_set("wl1_radio", "1");
			nvram_set("wl0_HW_switch", "0");	// 0 to be ON
			nvram_set("wl1_HW_switch", "0");	// 0 to be ON
//			nvram_commit();
			eval("radio", "on");			// ON
		}
		else if(wifi_sw_old == 0 && (!nvram_match("wl0_radio", "0") || !nvram_match("wl1_radio", "0")))
		{
			TRACE_PT("button BTN_WIFI_SW released: OFF\n");
			eval("radio", "off");			// OFF
			nvram_set("wl0_radio", "0");
			nvram_set("wl1_radio", "0");
			nvram_set("wl0_HW_switch", "1");	// 1 to be OFF
			nvram_set("wl1_HW_switch", "1");	// 1 to be OFF
//			nvram_commit();
		}
	}
#endif	/* RTCONFIG_WIRELESS_SWITCH && RTCONFIG_QCA */


#ifndef RTCONFIG_WPS_RST_BTN
	// independent wifi-toggle btn or Added WPS button radio toggle option
#if defined(RTCONFIG_WIFI_TOG_BTN)
	if (button_pressed(BTN_WIFI_TOG))
#else
	if (button_pressed(BTN_WPS) && nvram_match("btn_ez_radiotoggle", "1")
		&& (nvram_get_int("sw_mode") != SW_MODE_REPEATER)) // repeater mode not support HW radio
#endif
	{
		TRACE_PT("button WIFI_TOG pressed\n");
		if (btn_pressed_toggle_radio == 0){
			radio_switch(0);
			btn_pressed_toggle_radio = 1;
			return;
		}
	}
	else{
		btn_pressed_toggle_radio = 0;
	}
#endif	/* RTCONFIG_WPS_RST_BTN */


#ifdef RTCONFIG_TURBO
	if (button_pressed(BTN_TURBO))
	{
		TRACE_PT("button BTN_TURBO pressed\n");
	}
#endif
#ifdef RTCONFIG_LED_BTN
	LED_status_old = LED_status;
	LED_status = button_pressed(BTN_LED);

#if defined(RTAC68U) || defined(RTAC3200)
#if defined(RTAC68U)
	LED_status_changed = 0;
	if (LED_status != LED_status_old)
	{
		if (LED_status_first)
		{
			LED_status_first = 0;
			LED_status_on = LED_status;
		}
		else
			LED_status_changed = 1;
	}
#elif defined(RTAC3200)
	if (!LED_status &&
	    (LED_status != LED_status_old))
	{
		LED_status_changed = 1;
		if (LED_status_first)
		{
			LED_status_first = 0;
			LED_status_on = 0;
		}
		else
			LED_status_on = 1- LED_status_on;
	}
	else
		LED_status_changed = 0;
#endif

	if (LED_status_changed)
	{
		TRACE_PT("button BTN_LED pressed\n");
#ifdef RTCONFIG_LED_BTN_MODE
#ifdef RTAC68U
		if (nvram_get_int("btn_led_mode"))
			reboot(RB_AUTOBOOT);
#endif
#endif

#if defined(RTAC68U)
		if (LED_status == LED_status_on)
#elif defined(RTAC3200)
		if (LED_status_on)
#endif
			nvram_set_int("AllLED", 1);
		else
			nvram_set_int("AllLED", 0);
#if defined(RTAC68U)
		if (LED_status == LED_status_on)
#elif defined(RTAC3200)
		if (LED_status_on)
#endif
		{
			led_control(LED_POWER, LED_ON);

#ifdef RTAC3200
			kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
#else
			eval("et", "robowr", "0", "0x18", "0x01ff");
			eval("et", "robowr", "0", "0x1a", "0x01ff");
#endif

			if (wlonunit == -1 || wlonunit == 0) {
#ifdef RTAC68U
#ifdef RTCONFIG_LEDARRAY
				eval("wl", "ledbh", "0", "7");
				eval("wl", "ledbh", "9", "7");
#endif
				eval("wl", "ledbh", "10", "7");
#elif RTAC3200
				eval("wl", "-i", "eth2", "ledbh", "10", "7");
#endif
			}
			if (wlonunit == -1 || wlonunit == 1) {
#ifdef RTAC68U
#ifdef RTCONFIG_LEDARRAY
				eval("wl", "-i", "eth2", "ledbh", "0", "7");
				eval("wl", "-i", "eth2", "ledbh", "9", "7");
#endif
				eval("wl", "-i", "eth2", "ledbh", "10", "7");
#elif RTAC3200
				eval("wl", "ledbh", "10", "7");
#endif
			}
#ifdef RTAC3200
			if (wlonunit == -1 || wlonunit == 2) {
				eval("wl", "-i", "eth3", "ledbh", "10", "7");
			}
#endif
#ifdef RTAC68U
			if (nvram_match("wl1_radio", "1") &&
			   (wlonunit == -1 || wlonunit == 1))
			{
				nvram_set("led_5g", "1");
				led_control(LED_5G, LED_ON);
			}
#endif
#ifdef RTCONFIG_TURBO
			if (nvram_match("wl0_radio", "1") || nvram_match("wl1_radio", "1")
#ifdef RTAC3200
				|| nvram_match("wl2_radio", "1")
#endif
			)
				led_control(LED_TURBO, LED_ON);
#endif
			kill_pidfile_s("/var/run/usbled.pid", SIGTSTP); // inform usbled to reset status
		}
		else
			setAllLedOff();
	}
#elif defined(RTAC87U)
	if(LED_status)
		++BTN_pressed_count;
	else{
		BTN_pressed_count = 0;
		LED_status_changed = 0;
	}

	if(BTN_pressed_count >= LED_switch_count && LED_status_changed == 0){
		LED_status_changed = 1;
		LED_status_on = nvram_get_int("AllLED");

		if(LED_status_on)
			nvram_set_int("AllLED", 0);
		else
			nvram_set_int("AllLED", 1);
		LED_status_on = !LED_status_on;

		if(LED_status_on){
			led_control(LED_POWER, LED_ON);

			eval("et", "robowr", "0", "0x18", "0x01ff");
			eval("et", "robowr", "0", "0x1a", "0x01ff");
			qcsapi_wifi_run_script("router_command.sh", "lan4_led_ctrl on");

			if(nvram_match("wl0_radio", "1"))
				eval("wl", "ledbh", "10", "7");
			if(nvram_match("wl1_radio", "1")){
				qcsapi_wifi_run_script("router_command.sh", "wifi_led_on");
				qcsapi_led_set(1, 1);
			}

#ifdef RTCONFIG_EXT_LED_WPS
			led_control(LED_WPS, LED_OFF);
#else
			led_control(LED_WPS, LED_ON);
#endif

			kill_pidfile_s("/var/run/usbled.pid", SIGTSTP); // inform usbled to reset status
		}
		else
			setAllLedOff();

		/* check LED_WAN status */
		kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
	}
#endif
#endif

#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
	if (psta_exist())
		return;
#endif
#endif
	if (btn_pressed_setup < BTNSETUP_START)
	{
#ifdef RTCONFIG_WPS_RST_BTN
		if (btn_pressed_setup == BTNSETUP_DETECT)
#else
		if (button_pressed(BTN_WPS) &&
		    !no_need_to_start_wps() &&
		    !wps_band_radio_off(get_radio_band(0)) &&
		    !wps_band_ssid_broadcast_off(get_radio_band(0)) &&
#ifndef RTCONFIG_WIFI_TOG_BTN
		    nvram_match("btn_ez_radiotoggle", "0") &&
#endif
		    !nvram_match("wps_ign_btn", "1"))
#endif	/* ! RTCONFIG_WPS_RST_BTN */
		{
			if (nvram_match("wps_enable", "1")) {
				TRACE_PT("button WPS pressed\n");

				if (btn_pressed_setup == BTNSETUP_NONE)
				{
					btn_pressed_setup = BTNSETUP_DETECT;
					btn_count_setup = 0;
					alarmtimer(0, RUSHURGENT_PERIOD);
				}
				else
				{	/* Whenever it is pushed steady */
					if (++btn_count_setup > WPS_WAIT_COUNT)
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
			} else {
				TRACE_PT("button WPS pressed, skip\n");
			}
		}
		else if (btn_pressed_setup == BTNSETUP_DETECT)
		{
			btn_pressed_setup = BTNSETUP_NONE;
			btn_count_setup = 0;
#ifndef RTCONFIG_WPS_LED
			wps_led_control(LED_ON);
#endif
			alarmtimer(NORMAL_PERIOD, 0);
		}
#ifdef RTCONFIG_WPS_LED
		else
		{
			if (nvram_match("wps_success", "1") && ++btn_count_setup_second > WPS_SUCCESS_COUNT) {
				btn_count_setup_second = 0;
				nvram_set("wps_success", "0");
				__wps_led_control(LED_OFF);
			}
		}
#endif
	}
	else
	{
		if (!nvram_match("wps_ign_btn", "1")) {
#ifndef RTCONFIG_WPS_RST_BTN
			if (button_pressed(BTN_WPS) &&
			    !no_need_to_start_wps() &&
			    !wps_band_radio_off(get_radio_band(0)) &&
			    !wps_band_ssid_broadcast_off(get_radio_band(0)))
			{
				/* Whenever it is pushed steady, again... */
				if (++btn_count_setup_second > WPS_WAIT_COUNT)
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
#endif	/* ! RTCONFIG_WPS_RST_BTN */

			if (is_wps_stopped() || --wsc_timeout == 0)
			{
				wsc_timeout = 0;

				btn_pressed_setup = BTNSETUP_NONE;
				btn_count_setup = 0;
				btn_count_setup_second = 0;

				led_control_normal();

				alarmtimer(NORMAL_PERIOD, 0);
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
				stop_wps_method();
#else
				restart_wps_monitor();
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
			wps_led_control(LED_ON);
		else
			wps_led_control(LED_OFF);
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


int svcStatus[12] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

/* Check for time-reated service 	*/
/* 1. Wireless Radio			*/
/* 2. Guess SSID expire time 		*/
//int timecheck(int argc, char *argv[])
void timecheck(void)
{
	int activeNow;
	char *svcDate, *svcTime, *svcTime2;
	char prefix[]="wlXXXXXX_", tmp[100], tmp2[100];
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

		//dbG("[watchdog] timecheck unit=%s radio=%s, timesched=%s\n", prefix, nvram_safe_get(strcat_r(prefix, "radio", tmp)), nvram_safe_get(strcat_r(prefix, "timesched", tmp2))); // radio toggle test
		if (nvram_match(strcat_r(prefix, "radio", tmp), "0") ||
			nvram_match(strcat_r(prefix, "timesched", tmp2), "0")){
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
	if ((nvram_get_int("sw_mode") != SW_MODE_REPEATER) &&
		(strlen(nvram_safe_get("wl0_vifs")) || strlen(nvram_safe_get("wl1_vifs")) ||
		 strlen(nvram_safe_get("wl2_vifs"))))
	{
		lan_ifname = nvram_safe_get("lan_ifname");
		sprintf(wl_vifs, "%s %s %s", nvram_safe_get("wl0_vifs"), nvram_safe_get("wl1_vifs"), nvram_safe_get("wl2_vifs"));

		foreach (word, wl_vifs, next) {
			snprintf(nv, sizeof(nv) - 1, "%s_expire_tmp", wif_to_vif(word));
			expire = nvram_get_int(nv);

			if (expire)
			{
				if (expire <= 30)
				{
					nvram_set(nv, "0");
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", wif_to_vif(word));
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

#ifdef RTCONFIG_RALINK
int need_restart_wsc = 0;
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
		need_restart_wsc = 1;
	}
#if 0
	else if (sig == SIGHUP)
	{
		switch (get_model()) {
		case MODEL_RTN56U:
#ifdef RTCONFIG_DSL
		case MODEL_DSLN55U:
#endif
			restart_wireless();
			break;
		}
	}
#endif
#endif
}

#if defined(RTCONFIG_WLAN_LED) || defined(RTN18U)
unsigned long get_2g_count()
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

		if(strcmp(ifname, "eth1")) continue;

		if(sscanf(p+1, "%lu%*u%*u%*u%*u%*u%*u%*u%*u%lu", &counter1, &counter2)!=2) continue;

	}
	fclose(f);

	return counter1;
}

void fake_wl_led_2g(void)
{
	static unsigned int blink_2g_check = 0;
	static unsigned int blink_2g = 0;
	static unsigned int data_2g = 0;
	unsigned long count_2g;
	int i;
	static int j;
	static int status = -1;
	static int status_old;

	// check data per 10 count
	if((blink_2g_check%10)==0) {
		count_2g = get_2g_count();
		if(count_2g && data_2g!=count_2g) {
			blink_2g = 1;
			data_2g = count_2g;
		}
		else blink_2g = 0;
		led_control(LED_2G, LED_ON);
	}

	if(blink_2g) {
		j = rand_seed_by_time() % 3;
		for(i=0;i<10;i++) {
			usleep(33*1000);

			status_old = status;
			if (((i%2)==0) && (i > (3 + 2*j)))
				status = 0;
			else
				status = 1;

			if (status != status_old)
			{
				if (status)
					led_control(LED_2G, LED_ON);
				else
					led_control(LED_2G, LED_OFF);
			}
		}
		led_control(LED_2G, LED_ON);
	}

	blink_2g_check++;
}
#endif	/* RTCONFIG_WLAN_LED */

#if defined(RTCONFIG_BRCM_USBAP) || defined(RTAC66U) || defined(BCM4352)
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
#if defined(RTAC66U) || defined(BCM4352)
		j = rand_seed_by_time() % 3;
#endif
		for(i=0;i<10;i++) {
#if defined(RTAC66U) || defined(BCM4352)
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
#ifdef RTCONFIG_WLAN_LED
	if (nvram_contains_word("rc_support", "led_2g"))
	{
#if defined(RTN53)
		if(nvram_get_int("wl0_radio") == 0)
			led_control(LED_2G, LED_OFF);
		else
#endif
		fake_wl_led_2g();
	}
#endif

#if defined(RTN18U)
	if (nvram_match("bl_version", "1.0.0.0"))
		fake_wl_led_2g();
#endif

#if defined(RTCONFIG_BRCM_USBAP) || defined(RTAC66U) || defined(BCM4352)
#if defined(RTAC66U) || defined(BCM4352)
	if (nvram_match("led_5g", "1") &&
	   (wlonunit == -1 || wlonunit == 1))
#endif
	{
#if defined(RTN53)
                if(nvram_get_int("wl1_radio") == 0)
                        led_control(LED_5G, LED_OFF);
                else
#endif
		fake_wl_led_5g();
	}
#endif
	

#if defined(RTN56UB1)
                if(nvram_get_int("wl1_ledon") == 1 && g_wl1_led_status!=1)
                        led_control(LED_5G, LED_ON);
		else if(nvram_get_int("wl1_ledon") == 0 && g_wl1_led_status!=0)
                        led_control(LED_5G, LED_OFF);
		g_wl1_led_status=nvram_get_int("wl1_ledon");

                if(nvram_get_int("wl0_ledon") == 1 && g_wl0_led_status!=1)
                        led_control(LED_2G, LED_ON);
		else if(nvram_get_int("wl0_ledon") == 0 && g_wl0_led_status!=0)
                        led_control(LED_2G, LED_OFF);
		g_wl0_led_status=nvram_get_int("wl0_ledon");
#endif		
// it is not really necessary, but if required, add internet led check here
// using wan_primary_ifunit() to get current working wan unit wan0 or wan1
// using wan0_state_t or wan1_state_t to get status of working wan,
//	WAN_STATE_CONNECTED means internet connected
//	else means internet disconnted

/* Paul add 2012/10/25 */
#ifdef RTCONFIG_DSL
#ifndef RTCONFIG_DUALWAN
if (nvram_match("dsltmp_adslsyncsts","up") && nvram_match("wan0_state_t","2"))
	led_DSLWAN();
#endif
#endif
}

/* Paul add 2012/10/25 */
#ifdef RTCONFIG_DSL
#ifndef RTCONFIG_DUALWAN
unsigned long get_dslwan_count()
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

		if (nvram_match("dsl0_proto","pppoe") || nvram_match("dsl0_proto","pppoa"))
		{
			if(strcmp(ifname, "ppp0")) continue;
		}
		else //Mer, IPoA
		{
			if(strcmp(ifname, "eth2.1.1")) continue;
		}

		if(sscanf(p+1, "%lu%*u%*u%*u%*u%*u%*u%*u%*u%lu", &counter1, &counter2)!=2) continue;
	}
	fclose(f);

	return counter1;
}

void led_DSLWAN(void)
{
	static unsigned int blink_dslwan_check = 0;
	static unsigned int blink_dslwan = 0;
	static unsigned int data_dslwan = 0;
	unsigned long count_dslwan;
	int i;
	static int j;
	static int status = -1;
	static int status_old;

	// check data per 10 count
	if((blink_dslwan_check%10)==0) {
		count_dslwan = get_dslwan_count();
		if(count_dslwan && data_dslwan!=count_dslwan) {
			blink_dslwan = 1;
			data_dslwan = count_dslwan;
}
		else blink_dslwan = 0;
		led_control(LED_WAN, LED_ON);
	}

	if(blink_dslwan) {
		j = rand_seed_by_time() % 3;
		for(i=0;i<10;i++) {
			usleep(33*1000);

			status_old = status;
			if (((i%2)==0) && (i > (3 + 2*j)))
				status = 0;
			else
				status = 1;

			if (status != status_old)
			{
				if (status)
					led_control(LED_WAN, LED_ON);
				else
					led_control(LED_WAN, LED_OFF);
			}
		}
		led_control(LED_WAN, LED_ON);
	}

	blink_dslwan_check++;
}
#endif
#endif

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
				/* sw mode changed: restore defaults */
				nvram_set("nvramver", "0");
				nvram_commit();
				reboot(RB_AUTOBOOT);
			}
		}
		else flag_sw_mode = 0;
	}
}
#endif
#ifdef WEB_REDIRECT
void wanduck_check(void)
{
	if((freeze_duck_count = nvram_get_int("freeze_duck")) > 0)
		nvram_set_int("freeze_duck", --freeze_duck_count);
}
#endif
void regular_ddns_check(void)
{
	struct in_addr ip_addr;
	struct hostent *hostinfo;

	//_dprintf("regular_ddns_check...\n");

	hostinfo = gethostbyname(nvram_get("ddns_hostname_x"));
	ddns_check_count = 0;

	if(hostinfo) {
		ip_addr.s_addr = *(unsigned long *)hostinfo -> h_addr_list[0];
		//_dprintf("  %s ?= %s\n", nvram_get("wan0_ipaddr"), inet_ntoa(ip_addr));
		if(strcmp(nvram_get("wan0_ipaddr"), inet_ntoa(ip_addr))) {
			//_dprintf("WAN IP change!\n");
			nvram_set("ddns_update_by_wdog", "1");
			//unlink("/tmp/ddns.cache");
			logmessage("watchdog", "Hostname/IP mapping error! Restart ddns.");

		}
	}
	return;
}

void ddns_check(void)
{
	//_dprintf("ddns_check... %d\n", ddns_check_count);
	if(nvram_match("ddns_enable_x", "1") &&
	  (nvram_match("wan0_state_t", "2") && nvram_match("wan0_auxstate_t", "0")) )
	{
		if (pids("ez-ipupdate")) //ez-ipupdate is running!
			return;

		if (nvram_match("ddns_regular_check", "1")&& !nvram_match("ddns_server_x", "WWW.ASUS.COM")) {
			int period = nvram_get_int("ddns_regular_period");
			if (period < 30) period = 60;
		    	if (ddns_check_count >= (period*2)) {
				regular_ddns_check();
				ddns_check_count = 0;
			return;
			}
			ddns_check_count++;
		}

		if( nvram_match("ddns_updated", "1") ) //already updated success
			return;

		if( nvram_match("ddns_server_x", "WWW.ASUS.COM") ){
			if( !(  !strcmp(nvram_safe_get("ddns_return_code_chk"),"Time-out") ||
				!strcmp(nvram_safe_get("ddns_return_code_chk"),"connect_fail") ||
				strstr(nvram_safe_get("ddns_return_code_chk"), "-1") ) )
				return;
		}
		else{ //non asusddns service
			if( !strcmp(nvram_safe_get("ddns_return_code_chk"),"auth_fail") )
				return;
		}
		nvram_set("ddns_update_by_wdog", "1");
		//unlink("/tmp/ddns.cache");
		logmessage("watchdog", "start ddns.");
		notify_rc("start_ddns");
		ddns_update_timer = 0;
	}

	return;
}

void networkmap_check()
{
	if (!pids("networkmap"))
		start_networkmap(0);
}
//#if defined(RTCONFIG_JFFS2LOG) && defined(RTCONFIG_JFFS2)
#if defined(RTCONFIG_JFFS2LOG) && (defined(RTCONFIG_JFFS2)||defined(RTCONFIG_BRCM_NAND_JFFS2))
void syslog_commit_check(void)
{
	struct stat tmp_log_stat, jffs_log_stat;
	int tmp_stat, jffs_stat;

	tmp_stat = stat("/tmp/syslog.log", &tmp_log_stat);
	if(tmp_stat == -1)
		return;

	if(++log_commit_count >= LOG_COMMIT_PERIOD) {

		jffs_stat = stat("/jffs/syslog.log", &jffs_log_stat);
		if( jffs_stat == -1) {
			eval("cp", "/tmp/syslog.log", "/tmp/syslog.log-1", "/jffs");
			return;
		}

		if(tmp_log_stat.st_size > jffs_log_stat.st_size)
			eval("cp", "/tmp/syslog.log", "/tmp/syslog.log-1", "/jffs");

		log_commit_count = 0;
	}
	return;
}
#endif

#if defined(RTCONFIG_USB_MODEM) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
void modem_flow_check(void){
	time_t now, start, diff_mon;
	struct tm tm_now, tm_start;
	char timebuf[32];
	int day_cycle;
	int reset;
	int unit;

	unit = get_wan_unit(nvram_safe_get("usb_modem_act_dev"));
	if(unit == -1)
		return;

	if(!is_wan_connect(unit))
		return;

	if(++modem_flow_count >= MODEM_FLOW_PERIOD){
		time(&now);
		memcpy(&tm_now, localtime(&now), sizeof(struct tm));
		if(nvram_get_int("modem_bytes_data_cycle_debug") == 1)
			_dprintf("modem_flow_check:   now. year %4d, month %2d, day %2d, hour, %2d, minute %2d.\n", tm_now.tm_year+1900, tm_now.tm_mon+1, tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min);

		start = strtol(nvram_safe_get("modem_bytes_data_start"), NULL, 10);
		memcpy(&tm_start, localtime(&start), sizeof(struct tm));
		if(nvram_get_int("modem_bytes_data_cycle_debug") == 1)
			_dprintf("modem_flow_check: start. year %4d, month %2d, day %2d, hour, %2d, minute %2d.\n", tm_start.tm_year+1900, tm_start.tm_mon+1, tm_start.tm_mday, tm_start.tm_hour, tm_start.tm_min);

		day_cycle = strtol(nvram_safe_get("modem_bytes_data_cycle"), NULL, 10);
		if(nvram_get_int("modem_bytes_data_cycle_debug") == 1)
			_dprintf("modem_flow_check: cycle=%d.\n", day_cycle);

		reset = 0;
		if(day_cycle >= 1 && day_cycle <= 31 && tm_now.tm_year+1900 >= 2014){
			if(!start || tm_start.tm_year+1900 < 2014){
				_dprintf("Start the cycle of the data count!\n");
				reset = 1;
			}
			else{
				if(tm_now.tm_mday == day_cycle){
					// ex: day_cycle=20, start=2015/2/15 or 2014/4/30, now=2015/3/20. it should be reset by 2015/3/20, but in fact didn't yet.
					if(tm_now.tm_mday != tm_start.tm_mday || tm_now.tm_mon != tm_start.tm_mon || tm_now.tm_year != tm_start.tm_year)
						reset = 1;
				}
				else if(tm_now.tm_year != tm_start.tm_year){
					reset = 1;
				}
				else if(tm_now.tm_mon < tm_start.tm_mon){
					diff_mon = tm_now.tm_mon+12-tm_start.tm_mon;
					// ex: over 2 months. it should be reset, but in fact didn't yet.
					if(diff_mon > 1)
						reset = 1;
					else if(diff_mon == 1){
						// ex: day_cycle=20, start=12/15, now=1/2. it should be reset by 12/20, but in fact didn't yet.
						// tm_start.tm_mday >= day_cycle: it had been reset.
						if(tm_start.tm_mday < day_cycle)
							reset = 1;
						// ex: day_cycle=20, start=12/25, now=1/21. it should be reset by 1/20, but in fact didn't yet.
						// tm_now.tm_mday < day_cycle: it didn't need to be reset.
						// tm_now.tm_mday == day_cycle: it had been reset by the above codes.
						else if(tm_now.tm_mday > day_cycle)
							reset = 1;
					}
				}
				else if(tm_now.tm_mon > tm_start.tm_mon){
					diff_mon = tm_now.tm_mon-tm_start.tm_mon;
					// ex: over 2 months. it should be reset, but in fact didn't yet.
					if(diff_mon > 1)
						reset = 1;
					else if(diff_mon == 1){
						// ex: day_cycle=31, start=2/28, now=3/1. it should be reset by 2/28(31?), but in fact didn't yet.
						// tm_start.tm_mday >= day_cycle: it had been reset.
						if(tm_start.tm_mday < day_cycle)
							reset = 1;
						// ex: day_cycle=20, start=2/25, now=3/21. it should be reset by 3/20, but in fact didn't yet.
						// tm_now.tm_mday < day_cycle: it didn't need to be reset.
						// tm_now.tm_mday == day_cycle: it had been reset by the above codes.
						else if(tm_now.tm_mday > day_cycle)
							reset = 1;
					}
				}
				else{ // tm_now.tm_mon == tm_start.tm_mon
					// ex: day_cycle=20, start=2/15, now=2/28. it should be reset by 2/20, but in fact didn't yet.
					// tm_now.tm_mday < day_cycle: it didn't need to be reset.
					// tm_now.tm_mday == day_cycle: it had been reset by the above codes.
					// tm_start.tm_mday >= day_cycle: it had been reset.
					if(tm_now.tm_mday > day_cycle && tm_start.tm_mday < day_cycle)
						reset = 1;
				}
			}
		}

		if(reset){
			eval("modem_status.sh", "bytes-");
			snprintf(timebuf, 32, "%d", (int)now);
			nvram_set("modem_bytes_data_start", timebuf);
			nvram_commit();
		}
		else
			eval("modem_status.sh", "bytes");

		modem_flow_count = 0;
	}
	return;
}
#endif

static void auto_firmware_check()
{
	static int period = 5757;
	static int bootup_check = 1;
	static int periodic_check = 0;
	int cycle_manual = nvram_get_int("fw_check_period");
	int cycle = (cycle_manual > 1) ? cycle_manual : 5760;
	time_t now;
	struct tm *tm;

	if (!nvram_get_int("ntp_ready"))
		return;

	if (!bootup_check && !periodic_check)
	{
		time(&now);
		tm = localtime(&now);

		if ((tm->tm_hour == 2))	// every 48 hours at 2 am
		{
			periodic_check = 1;
			period = -1;
		}
	}
#ifdef RTAC68U
	else if (nvram_match("bl_version", "2.1.2.2")) {
		periodic_check = 1;
		nvram_set_int("fw_check_period", 10);
	}
#endif

	if (bootup_check || periodic_check)
		period = (period + 1) % cycle;
	else
		return;

	if (!period)
	{
		if (bootup_check)
			bootup_check = 0;

		eval("/usr/sbin/webs_update.sh");
#ifdef RTCONFIG_DSL
		eval("/usr/sbin/notif_update.sh");
#endif

		if (nvram_get_int("webs_state_update") &&
		    !nvram_get_int("webs_state_error") &&
		    strlen(nvram_safe_get("webs_state_info")))
		{
			dbg("retrieve firmware information\n");
#ifdef RTAC68U
			if (!nvram_match("bl_version", "2.1.2.2"))
				return;

			if (!nvram_match("login_ip", ""))
				return;

			if (!nvram_get_int("webs_state_flag"))
			{
				dbg("no need to upgrade firmware\n");
				return;
			}

			nvram_set_int("auto_upgrade", 1);

			eval("/usr/sbin/webs_upgrade.sh");

			if (nvram_get_int("webs_state_error"))
			{
				dbg("error execute upgrade script\n");
				goto ERROR;
			}

			nvram_set("restore_defaults", "1");
			ResetDefault();

#ifdef RTCONFIG_DUAL_TRX
			int count = 80;
#else
			int count = 50;
#endif
			while ((count-- > 0) && (nvram_get_int("webs_state_upgrade") == 1))
			{
				dbg("reboot count down: %d\n", count);
				sleep(1);
			}

			reboot(RB_AUTOBOOT);
#endif
		}
		else
			dbg("could not retrieve firmware information!\n");
#ifdef RTAC68U
ERROR:
		nvram_set_int("auto_upgrade", 0);
#endif
	}
}

#if 0
#ifdef RTCONFIG_PUSH_EMAIL
#define PM_CONTENT "/tmp/push_mail"
#define PM_CONFIGURE "/etc/email/email.conf"
#define PM_WEEKLY 1
#define PM_MONTHLY 2
void SendOutMail(void){
	FILE *fp;
	char tmp[1024]={0}, buf[1024]={0};
	char tmp_sender[64]={0}, tmp_titile[64]={0}, tmp_attachment[64]={0};

#if 0
	fp = fopen(PM_CONTENT, "w");
	if(fp) {
		fputs(" Push Mail Service!!! ", fp);
		fclose(fp);
	}
#else
	getlogbyinterval(PM_CONTENT, 0, 0);

	if(f_size(PM_CONTENT) <= 0){
		cprintf("No notified log.\n");
		return;
	}
#endif

	/* write the configuration file.*/
	system("mkdir /etc/email");
	fp = fopen(PM_CONFIGURE,"w");
	if(fp == NULL){
		cprintf("create configuration file fail.\n");
		return;
	}

	/*SMTP_SERVER*/
	sprintf(tmp,"SMTP_SERVER = '%s'\n", nvram_safe_get("PM_SMTP_SERVER"));
	strcat(buf, tmp);
	/*SMTP_PORT*/
	sprintf(tmp,"SMTP_PORT = '%s'\n", nvram_safe_get("PM_SMTP_PORT"));
	strcat(buf, tmp);
	/*MY_NAME*/
	sprintf(tmp,"MY_NAME = '%s'\n", nvram_safe_get("PM_MY_NAME"));
	strcat(buf, tmp);
	/*MY_EMAIL*/
	sprintf(tmp,"MY_EMAIL = '%s'\n", nvram_safe_get("PM_MY_EMAIL"));
	strcat(buf, tmp);
	/*USE_TLS*/
	sprintf(tmp,"USE_TLS = '%s'\n", nvram_safe_get("PM_USE_TLS"));
	strcat(buf, tmp);
	/*SMTP_AUTH*/
	sprintf(tmp,"SMTP_AUTH = '%s'\n", nvram_safe_get("PM_SMTP_AUTH"));
	strcat(buf, tmp);
	/*SMTP_AUTH_USER*/
	sprintf(tmp,"SMTP_AUTH_USER = '%s'\n", nvram_safe_get("PM_SMTP_AUTH_USER"));
	strcat(buf, tmp);
	/*SMTP_AUTH_PASS*/
	sprintf(tmp,"SMTP_AUTH_PASS = '%s'\n", nvram_safe_get("PM_SMTP_AUTH_PASS"));
	strcat(buf, tmp);

	fputs(buf,fp);
	fclose(fp);

	/* issue command line to trigger eMail*/
	sprintf(tmp_sender, "Administrator");
	sprintf(tmp_titile, "RT-N16");
	sprintf(tmp_attachment, "/tmp/syslog.log");
	sprintf(tmp,"cat %s | email -V -n \"%s\" -s \"%s\" %s -a \"%s\"", PM_CONTENT, tmp_sender, tmp_titile, nvram_safe_get("PM_target"), tmp_attachment);
	system(tmp);
}

void save_next_time(struct tm *tm){
	nvram_set_int("PM_mon", tm->tm_mon);
	nvram_set_int("PM_day", tm->tm_mday);
	nvram_set_int("PM_hour", tm->tm_hour);
}

int nexthour = 0;
int nextday = 0;
int nextmonth = 0;
void schedule_mail(int interval, struct tm *tm){
	if(nextday == 0){
		cprintf("Schedule the next report.!!!\n");
		tm->tm_mday += interval;
		mktime(tm);
		nextmonth = tm->tm_mon;
		nextday = tm->tm_mday;
		nexthour = tm->tm_hour;

		save_next_time(tm);
	}
	else{
		if((tm->tm_mon == nextmonth) && (tm->tm_mday == nextday) && (tm->tm_hour == nexthour)){
			tm->tm_mday += interval;
			mktime(tm);
			nextmonth = tm->tm_mon;
			nextday = tm->tm_mday;
			nexthour = tm->tm_hour;
			save_next_time(tm);

			cprintf("Sending Push Mail Service out!!!\n");
			SendOutMail();
		}
	}
}

void push_mail(void)
{
	static int count =0;	//for debug
	time_t now;
	struct tm *tm;
	int tmpfreq = 0;
	//char tmp[32]={0};

	//tcapi_get("PushMail_Entry", "PM_enable", tmp);
	if(nvram_get_int("PM_enable") == 0){
		return;
	}

	/* reset the Push Mail Service */
	//tcapi_get("PushMail_Entry", "PM_restart", tmp);
	if(nvram_get_int("PM_restart") == 1){
		nexthour = 0;
		nextday = 0;
		nextmonth = 0;
		//tcapi_set("PushMail_Entry", "PM_restart", "0");
		nvram_set_int("PM_restart", 0);
	}

	//tcapi_get("PushMail_Entry", "PM_freq", tmp);
	tmpfreq=nvram_get_int("PM_freq");

	time(&now);
	tm = localtime(&now);

	if(tmpfreq == PM_MONTHLY){ /* Monthly report */
		schedule_mail(30, tm);
	}
	else if(tmpfreq == PM_WEEKLY){	/* Weekly report */
		schedule_mail(7, tm);
	}
	else{	/* Daily report */
		schedule_mail(1, tm);
	}

	//debug.javi
	//tcapi_get("PushMail_Entry", "PM_debug", tmp);
	if(nvram_get_int("PM_debug") == 1){
		if((count %10) == 0){
			cprintf("year=%d, month=%d, day=%d, wday=%d, hour=%d, min=%d, sec=%d\n",(tm->tm_year+1900), tm->tm_mon, tm->tm_mday, tm->tm_wday, tm->tm_hour, tm->tm_min, tm->tm_sec);
			cprintf("tmpfreq =%d, nextmonth =%d, nextday=%d nexthour=%d\n", tmpfreq, nextmonth , nextday, nexthour);
			cprintf("\n");
		}
		count ++;
	}
	//debug.javi
}
#endif
#endif

#ifdef RTCONFIG_USER_LOW_RSSI
#define ETHER_ADDR_STR_LEN	18

typedef struct wl_low_rssi_count{
	char	wlif[32];
	int	lowc;
}wl_lowr_count_t;

#define		WLLC_SIZE	2
static wl_lowr_count_t wllc[WLLC_SIZE];

void init_wllc()
{
	char wlif[128], *next;
	int idx=0;

	memset(wllc, 0, sizeof(wllc));

	foreach (wlif, nvram_safe_get("wl_ifnames"), next)
	{
		strncpy(wllc[idx].wlif, wlif, 32);
		wllc[idx].lowc = 0;

		idx++;
	}
}

#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)
#else
#define	MAX_STA_COUNT	128
void rssi_check_unit(int unit)
{
	int lrsi = 0, lrc = 0;

	scb_val_t scb_val;
	char ea[ETHER_ADDR_STR_LEN];
	int i, ii;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	int val = 0;
	char name_vif[] = "wlX.Y_XXXXXXXXXX";
	struct maclist *mac_list;
	int mac_list_size;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	lrc = nvram_get_int(strcat_r(prefix, "lrc", tmp));
	if(!lrc) lrc = 2;
	if (!(lrsi = nvram_get_int(strcat_r(prefix, "user_rssi", tmp))))
		return;

#ifdef RTCONFIG_PROXYSTA
	if (psta_exist_except(unit))
	{
		dbg("%s radio is disabled\n",
			nvram_match(strcat_r(prefix, "nband", tmp), "1") ? "5 GHz" : "2.4 GHz");
		return;
	}
	else if (is_psta(unit))
	{
		dbg("skip interface %s under psta mode\n", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
		return;
	}
#endif
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
	wl_ioctl(name, WLC_GET_RADIO, &val, sizeof(val));
	val &= WL_RADIO_SW_DISABLE | WL_RADIO_HW_DISABLE;

	if (val)
	{
		dbg("%s radio is disabled\n",
			nvram_match(strcat_r(prefix, "nband", tmp), "1") ? "5 GHz" : "2.4 GHz");
		return;
	}

#ifdef RTCONFIG_WIRELESSREPEATER
	if ((nvram_get_int("sw_mode") == SW_MODE_REPEATER)
		&& (nvram_get_int("wlc_band") == unit))
	{
		sprintf(name_vif, "wl%d.%d", unit, 1);
		name = name_vif;
	}
#endif

	/* buffers and length */
	mac_list_size = sizeof(mac_list->count) + MAX_STA_COUNT * sizeof(struct ether_addr);
	mac_list = malloc(mac_list_size);

	if (!mac_list)
		goto exit;

	memset(mac_list, 0, mac_list_size);

	/* query wl for authenticated sta list */
	strcpy((char*) mac_list, "authe_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, mac_list, mac_list_size))
		goto exit;

	for (i = 0; i < mac_list->count; i ++) {
		memcpy(&scb_val.ea, &mac_list->ea[i], ETHER_ADDR_LEN);

		if (wl_ioctl(name, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t)))
			continue;

		ether_etoa((void *)&mac_list->ea[i], ea);

		_dprintf("rssi chk.1. wlif(%s), chk ea=%s, rssi=%d(%d), lowr_cnt=%d, lrc=%d\n", name, ea, scb_val.val, lrsi, wllc[unit].lowc, lrc);

		if(scb_val.val < lrsi){
			_dprintf("rssi chk.2. low rssi: ea=%s, lowc=%d(%d)\n", ea, wllc[unit].lowc, lrc);
			if(++wllc[unit].lowc > lrc){
				_dprintf("rssi chk.3. deauth ea=%s\n", ea);

				scb_val.val = 8;	// reason code: Disassociated because sending STA is leaving BSS
				wllc[unit].lowc = 0;

				if (wl_ioctl(name, WLC_SCB_DEAUTHENTICATE_FOR_REASON, &scb_val, sizeof(scb_val_t)))
					continue;
			}
		} else
			wllc[unit].lowc = 0;
	}

	for (i = 1; i < 4; i++) {
#ifdef RTCONFIG_WIRELESSREPEATER
		if ((nvram_get_int("sw_mode") == SW_MODE_REPEATER)
			&& (unit == nvram_get_int("wlc_band")) && (i == 1))
			break;
#endif
		sprintf(prefix, "wl%d.%d_", unit, i);
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
		{
			sprintf(name_vif, "wl%d.%d", unit, i);

			memset(mac_list, 0, mac_list_size);

			/* query wl for authenticated sta list */
			strcpy((char*) mac_list, "authe_sta_list");
			if (wl_ioctl(name_vif, WLC_GET_VAR, mac_list, mac_list_size))
				goto exit;

			for (ii = 0; ii < mac_list->count; ii ++) {
				memcpy(&scb_val.ea, &mac_list->ea[ii], ETHER_ADDR_LEN);

				if (wl_ioctl(name_vif, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t)))
					continue;

				ether_etoa((void *)&mac_list->ea[ii], ea);

				_dprintf("rssi chk.1. wlif(%s), chk ea=%s, rssi=%d(%d), lowr_cnt=%d, lrc=%d\n", name_vif, ea, scb_val.val, lrsi, wllc[unit].lowc, lrc);

				if(scb_val.val < lrsi){
					_dprintf("rssi chk.2. low rssi: ea=%s, lowc=%d(%d)\n", ea, wllc[unit].lowc, lrc);
					if(++wllc[unit].lowc > lrc){
						_dprintf("rssi chk.3. deauth ea=%s\n", ea);

						scb_val.val = 8;	// reason code: Disassociated because sending STA is leaving BSS
						wllc[unit].lowc = 0;

						if (wl_ioctl(name_vif, WLC_SCB_DEAUTHENTICATE_FOR_REASON, &scb_val, sizeof(scb_val_t)))
							continue;
					}
				} else
					wllc[unit].lowc = 0;
			}
		}
	}

	/* error/exit */
exit:
	if (mac_list) free(mac_list);

	return;
}
#endif
void rssi_check()
{
	int ii = 0;
	char nv_param[NVRAM_MAX_PARAM_LEN];
	char *temp;

	if (!nvram_get_int("wlready"))
		return;

	for (ii = 0; ii < DEV_NUMIFS; ii++) {
		sprintf(nv_param, "wl%d_unit", ii);
		temp = nvram_get(nv_param);

		if (temp && strlen(temp) > 0)
			rssi_check_unit(ii);
	}
}
#endif

#ifdef RTCONFIG_BWDPI
#define CC_CONTENT "/tmp/push_mail"
#define EMAIL_CONF "/etc/email/email.conf"
static void send_wrslog_email(char *path)
{
	FILE *fp;
	char tmp[1024], buf[1024], title[64], attach[64], smtp_auth_pass[256];
	char date[30];
	time_t now;

	memset(tmp, 0, sizeof(tmp));
	memset(buf, 0, sizeof(buf));
	memset(title, 0, sizeof(title));
	memset(attach, 0, sizeof(attach));
	memset(date, 0, sizeof(date));
	memset(smtp_auth_pass, 0, sizeof(smtp_auth_pass));

	// get current date
	time(&now);
	StampToDate(now, date);

	//printf("send_mail : path = %s\n", path);
	// check path for protection
	if(!strcmp(path, "")) return;

	// email server conf setting
	mkdir_if_none("/etc/email");
	if((fp = fopen(EMAIL_CONF, "w")) == NULL){
		printf("fail to open %s\n", EMAIL_CONF);
		return;
	}

#ifdef RTCONFIG_HTTPS
	strncpy(smtp_auth_pass,pwdec(nvram_get("PM_SMTP_AUTH_PASS")),256);
#else
	strncpy(smtp_auth_pass,nvram_get("PM_SMTP_AUTH_PASS"),256);
#endif

	fprintf(fp,"SMTP_SERVER = '%s'\n", nvram_safe_get("PM_SMTP_SERVER"));
	fprintf(fp,"SMTP_PORT = '%s'\n", nvram_safe_get("PM_SMTP_PORT"));
	fprintf(fp,"MY_NAME = 'Administrator'\n");
	fprintf(fp,"MY_EMAIL = '%s'\n", nvram_safe_get("PM_MY_EMAIL"));
	fprintf(fp,"USE_TLS = 'true'\n");
	fprintf(fp,"SMTP_AUTH = 'LOGIN'\n");
	fprintf(fp,"SMTP_AUTH_USER = '%s'\n", nvram_safe_get("PM_SMTP_AUTH_USER"));
	fprintf(fp,"SMTP_AUTH_PASS = '%s'\n", smtp_auth_pass);
	fclose(fp);

	// email content
	if((fp = fopen(CC_CONTENT, "w")) == NULL){
		printf("fail to open %s\n", CC_CONTENT);
		return;
	}

	// extract mail log into seperated event information
	extract_data(path, fp);

	fprintf(fp, "Suggest action: Your client devices has been detected suspicious networking behavior and blocked connection with destination server to protect your sensitive information.\nBased on our recommendation, you can\n");
	fprintf(fp, "1. Remove app that access this site and don't visit this website to prevent any personal information leak.\n");
	fprintf(fp, "2. Check your router security setting.\n");
	fprintf(fp, "3. Update security patch for your client or new firmware for your router.\n");
	fprintf(fp, "Please refer to attached log file for detail information. You also can link to trend micro website to download security trial software for your client device protection.\n");
	fprintf(fp, "\n");
	fprintf(fp, "http://www.trendmicro.com/\n");
	fclose(fp);

	//system("cat /tmp/push_mail"); // debug

	// send email
	sprintf(title, "AiProtection alert! %s", date);
	sprintf(attach, path);
	sprintf(tmp,"cat %s | email -V -s \"%s\" %s -a \"%s\"", CC_CONTENT, title, nvram_safe_get("PM_MY_EMAIL"), attach);
	system(tmp);

	unlink(path);
}

static void capture_bwdpi_log()
{
	if(nvram_get_int("wrs_enable") == 0 && nvram_get_int("wrs_cc_enable") == 0 && nvram_get_int("wrs_vp_enable") == 0)
		return;

	char path[30];
	memset(path, 0, sizeof(path));

	int flag = merge_log(path);
	//printf("[capture log] flag = %d, path = %s\n", flag, path);
	if(flag) send_wrslog_email(path);
}

static void check_bwdpi_monitor()
{
	if(!pids("bwdpi_monitor"))
		start_bwdpi_monitor_service();
}

static void auto_sig_check()
{
	static int period = 5757;
	static int bootup_check = 1;
	static int periodic_check = 0;
	int cycle_manual = nvram_get_int("sig_check_period");
	int cycle = (cycle_manual > 1) ? cycle_manual : 5760;
	time_t now;
	struct tm *tm;

	if (!nvram_get_int("ntp_ready"))
		return;

	if (!bootup_check && !periodic_check)
	{
		time(&now);
		tm = localtime(&now);

		if ((tm->tm_hour == 2))	// every 48 hours at 2 am
		{
			periodic_check = 1;
			period = -1;
		}
	}

	if (bootup_check || periodic_check)
		period = (period + 1) % cycle;
	else
		return;

	if (!period)
	{
		if (bootup_check)
			bootup_check = 0;

		eval("/usr/sbin/sig_update.sh");

		if (nvram_get_int("sig_state_update") &&
		    !nvram_get_int("sig_state_error") &&
		    strlen(nvram_safe_get("sig_state_info")))
		{
			dbg("retrieve sig information\n");

			if (!nvram_get_int("sig_state_flag"))
			{
				dbg("no need to upgrade signature\n");
				return;
			}

			nvram_set_int("auto_sig_upgrade", 1);

			eval("/usr/sbin/sig_upgrade.sh");

			if (nvram_get_int("sig_state_error"))
			{
				dbg("error execute sig upgrade script\n");
				goto ERROR;
			}
		}
		else
			dbg("could not retrieve signature information!\n");
ERROR:
		nvram_set_int("auto_sig_upgrade", 0);
	}
}
#endif

#ifdef RTCONFIG_TOR
#if (defined(RTCONFIG_JFFS2)||defined(RTCONFIG_BRCM_NAND_JFFS2))
static void Tor_microdes_check(){

	FILE *f;
	char buf[256];
	char *ifname, *p;
	unsigned long counter1, counter2;
	struct stat tmp_db_stat, jffs_db_stat;
        int tmp_stat, jffs_stat;

	if(++tor_check_count >= TOR_CHECK_PERIOD) {
		tor_check_count = 0;

		jffs_stat = stat("/jffs/.tordb/cached-microdesc-consensus", &jffs_db_stat);
		if(jffs_stat != -1){
			return;
		}

		tmp_stat = stat("/tmp/.tordb/cached-microdesc-consensus", &tmp_db_stat);
        	if(tmp_stat == -1){
                	return;
		}

		if((f = fopen("/tmp/torlog", "r"))==NULL) return -1;

		while (fgets(buf, sizeof(buf), f)) {
                	if((p=strstr(buf, "now have enough directory"))==NULL) continue;
                	*p = 0;
			eval("cp", "-rf", "/tmp/.tordb", "/jffs/.tordb");
			break;
        	}
        	fclose(f);
	}

        return;
}
#endif
#endif

#if 0
static time_t	tt=0, tt_old=0;
static int 	bcnt=0;
void
period_chk_cnt()
{
	time(&tt);
	if(!tt_old)
		tt_old = tt;

	++bcnt;
	if(tt - tt_old > 9 && tt - tt_old < 15) {
		if(bcnt > 15) {
			char buf[5];
			_dprintf("\n\n\n!! >>> rush cpu count %d in 10 secs<<<\n\n\n", bcnt);
			sprintf(buf, "%d", bcnt);
			nvram_set("cpurush", buf);
		}
		tt_old = tt;
		bcnt = 0;
	} else if (tt - tt_old >= 15){
		tt = tt_old = bcnt = 0;
	}
}
#endif

/* wathchdog is runned in NORMAL_PERIOD, 1 seconds
 * check in each NORMAL_PERIOD
 *	1. button
 *
 * check in each NORAML_PERIOD*10
 *
 *	1. time-dependent service
 */

void watchdog(int sig)
{
	int period;
#ifdef RTCONFIG_PUSH_EMAIL
	//push_mail();
#endif
#if 0
	period_chk_cnt();
#endif
	/* handle button */
	btn_check();
	if(nvram_match("asus_mfg", "0")
#ifdef RTCONFIG_LED_BTN
		&& nvram_get_int("AllLED")
#endif
	)
	
	led_check();

#ifdef RTCONFIG_RALINK
	if(need_restart_wsc) {
		char word[256], *next, ifnames[128];

		strcpy(ifnames, nvram_safe_get("wl_ifnames"));
		foreach (word, ifnames, next) {
			eval("iwpriv", word, "set", "WscConfMode=0");
		}

		need_restart_wsc = 0;
		start_wsc_pin_enrollee();
	}
#endif
#ifdef RTCONFIG_SWMODE_SWITCH
	swmode_check();
#endif
#ifdef WEB_REDIRECT
	wanduck_check();
#endif
	/* if timer is set to less than 1 sec, then bypass the following */
	if (itv.it_value.tv_sec == 0) return;

	if (!nvram_match("asus_mfg", "0")) return;

	watchdog_period = (watchdog_period + 1) % 30;

#ifdef RTCONFIG_BCMARM
	if(u3_chk_life < 20) {
		chkusb3_period = (chkusb3_period + 1) % u3_chk_life;
		if(!chkusb3_period && nvram_match("usb_usb3", "1") && nvram_match("usb_path1_speed", "12")
				&& strcmp(nvram_safe_get("usb_path1"), "printer")
				&& strcmp(nvram_safe_get("usb_path1"), "modem")
				) {
			_dprintf("force reset usb pwr\n");
			stop_usb_program(1);
			sleep(1);
			set_pwr_usb(0);
			sleep(3);
			set_pwr_usb(1);
			u3_chk_life *= 2;
		}
	}
#endif
	if (watchdog_period) return;

#ifdef RTCONFIG_USER_LOW_RSSI
	rssi_check();
#endif
#ifdef BTN_SETUP
	if (btn_pressed_setup >= BTNSETUP_START) return;
#endif

	/* check for time-related services */
	timecheck();
#if 0
	cpu_usage_monitor();
#endif

	/* Force a DDNS update every "x" days - default is 21 days */
	period = nvram_get_int("ddns_refresh_x");
	if ((period) && (++ddns_update_timer >= (DAY_PERIOD * period))) {
		ddns_update_timer = 0;
		nvram_set("ddns_updated", "0");
	}

	ddns_check();
	networkmap_check();
//#if defined(RTCONFIG_JFFS2LOG) && defined(RTCONFIG_JFFS2)
#if defined(RTCONFIG_JFFS2LOG) && (defined(RTCONFIG_JFFS2)||defined(RTCONFIG_BRCM_NAND_JFFS2))
	syslog_commit_check();
#endif
#if defined(RTCONFIG_USB_MODEM) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
	modem_flow_check();
#endif
//	auto_firmware_check();

#ifdef RTCONFIG_BWDPI
	auto_sig_check();
	capture_bwdpi_log();
	check_bwdpi_monitor();
#endif

#if defined(RTCONFIG_TOR) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2))
	if(nvram_get_int("Tor_enable"))
		Tor_microdes_check();
#endif
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

	if ((nvram_get_int("sw_mode") == SW_MODE_AP) && (nvram_get_int("wlc_psta") == 1))
		wlonunit = nvram_get_int("wlc_band");

#ifdef RTCONFIG_RALINK
	doSystem("iwpriv %s set WatchdogPid=%d", WIF_2G, getpid());
#if defined(RTCONFIG_HAS_5G)
	doSystem("iwpriv %s set WatchdogPid=%d", WIF_5G, getpid());
#endif	/* RTCONFIG_HAS_5G */
#endif	/* RTCONFIG_RALINK */

	/* set the signal handler */
	signal(SIGCHLD, chld_reap);
	signal(SIGUSR1, catch_sig);
	signal(SIGUSR2, catch_sig);
	signal(SIGTSTP, catch_sig);
	signal(SIGALRM, watchdog);
#ifdef RTCONFIG_RALINK
	signal(SIGTTIN, catch_sig);
#if 0
	switch (get_model()) {
	case MODEL_RTN56U:
#ifdef RTCONFIG_DSL
	case MODEL_DSLN55U:
#endif
	signal(SIGHUP, catch_sig);
	break;
	}
#endif
#endif

#ifdef RTCONFIG_DSL //Paul add 2012/6/27
	nvram_set("btn_rst", "0");
	nvram_set("btn_ez", "0");
	nvram_set("dsltmp_syncloss", "0");
	nvram_set("dsltmp_syncloss_apply", "0");
#endif
#ifdef RTCONFIG_WIRELESS_SWITCH
	nvram_set("btn_wifi_sw", "0");
#endif
	nvram_unset("wps_ign_btn");
#ifdef RT4GAC55U
	nvram_set("btn_lte", "0");
#endif

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

