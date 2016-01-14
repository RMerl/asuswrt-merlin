/*

	Copyright 2005, Broadcom Corporation
	All Rights Reserved.

	THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
	KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
	SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.

*/

#include "rc.h"

#include <sys/sysinfo.h>
#include <sys/ioctl.h>
#include <bcmutils.h>
#include <wlutils.h>

void stop_nas(void);

//	ref: http://wiki.openwrt.org/OpenWrtDocs/nas

//	#define DEBUG_TIMING

#ifdef REMOVE
static int security_on(int idx, int unit, int subunit, void *param)
{
	return nvram_get_int(wl_nvname("radio", unit, 0)) && (!nvram_match(wl_nvname("security_mode", unit, subunit), "disabled"));
}
static int is_wds(int idx, int unit, int subunit, void *param)
{
	return nvram_get_int(wl_nvname("radio", unit, 0)) && nvram_get_int(wl_nvname("wds_enable", unit, subunit));
}
#ifndef CONFIG_BCMWL5
static int is_sta(int idx, int unit, int subunit, void *param)
{
	return nvram_match(wl_nvname("mode", unit, subunit), "sta");
}
#endif
int wds_enable(void)
{
	return foreach_wif(1, NULL, is_wds);
}
#endif

#ifdef CONFIG_BCMWL5
int
start_nas(void)
{
	char *nas_argv[] = { "nas", NULL };
	pid_t pid;

	stop_nas();
	return _eval(nas_argv, NULL, 0, &pid);
}

void
stop_nas(void)
{
	killall_tk("nas");
}
#ifdef REMOVE
void notify_nas(const char *ifname)
{
#ifdef DEBUG_TIMING
	struct sysinfo si;
	sysinfo(&si);
	_dprintf("%s: ifname=%s uptime=%ld\n", __FUNCTION__, ifname, si.uptime);
#else
	_dprintf("%s: ifname=%s\n", __FUNCTION__, ifname);
#endif

#ifdef CONFIG_BCMWL5

	/* Inform driver to send up new WDS link event */
	if (wl_iovar_setint((char *)ifname, "wds_enable", 1)) {
		_dprintf("%s: set wds_enable failed\n", ifname);
	}

#else	/* !CONFIG_BCMWL5 */

	if (!foreach_wif(1, NULL, security_on)) return;

	int i;
	int unit;

	i = 10;
	while (pidof("nas") == -1) {
		_dprintf("waiting for nas i=%d\n", i);
		if (--i < 0) {
			syslog(LOG_ERR, "Unable to find nas");
			break;
		}
		sleep(1);
	}
	sleep(5);

	/* the wireless interface must be configured to run NAS */
	wl_ioctl((char *)ifname, WLC_GET_INSTANCE, &unit, sizeof(unit));

	xstart("nas4not", "lan", ifname, "up", "auto",
		nvram_safe_get(wl_nvname("crypto", unit, 0)),	// aes, tkip (aes+tkip ok?)
		nvram_safe_get(wl_nvname("akm", unit, 0)),	// psk (only?)
		nvram_safe_get(wl_nvname("wpa_psk", unit, 0)),	// shared key
		nvram_safe_get(wl_nvname("ssid", unit, 0))	// ssid
	);

#endif /* CONFIG_BCMWL5 */
}
#endif
#endif

#if defined(CONFIG_BCMWL5) || (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) || defined(RTCONFIG_QCA)
#define APSCAN_INFO "/tmp/apscan_info.txt"

static int lock = -1;

static void wlcscan_safeleave(int signo) {
	signal(SIGTERM, SIG_IGN);

	nvram_set_int("wlc_scan_state", WLCSCAN_STATE_STOPPED);
	file_unlock(lock);
	exit(0);
}

// TODO: wlcscan_main
// 	- scan and save result into files for httpd hook
//	- handlling stop signal

int wlcscan_main(void)
{
	FILE *fp;
	char word[256], *next;
	int i;

	signal(SIGTERM, wlcscan_safeleave);

	/* clean APSCAN_INFO */
	lock = file_lock("sitesurvey");
	if ((fp = fopen(APSCAN_INFO, "w")) != NULL) {
		fclose(fp);
	}
	file_unlock(lock);

	nvram_set_int("wlc_scan_state", WLCSCAN_STATE_INITIALIZING);
	/* Starting scanning */
	i = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
#ifdef RTCONFIG_BCM_7114
		wlcscan_core_escan(APSCAN_INFO, word);
#else
		wlcscan_core(APSCAN_INFO, word);
#endif
		// suppose only two or less interface handled
		nvram_set_int("wlc_scan_state", WLCSCAN_STATE_2G+i);
		i++;
	}
#ifdef RTCONFIG_QTN
	wlcscan_core_qtn(APSCAN_INFO, "wifi0");
#endif
	nvram_set_int("wlc_scan_state", WLCSCAN_STATE_FINISHED);
	return 1;
}

#endif /* CONFIG_BCMWL5 || (RTCONFIG_RALINK && RTCONFIG_WIRELESSREPEATER) || defined(RTCONFIG_QCA) */

#ifdef RTCONFIG_WIRELESSREPEATER
#define NOTIFY_IDLE 0
#define NOTIFY_CONN 1
#define NOTIFY_DISCONN -1

static void wlcconnect_safeleave(int signo) {
	signal(SIGTERM, SIG_IGN);

	nvram_set_int("wlc_state", WLC_STATE_STOPPED);
	nvram_set_int("wlc_sbstate", WLC_STOPPED_REASON_MANUAL);

	exit(0);
}

// wlcconnect_main
//	wireless ap monitor to connect to ap
//	when wlc_list, then connect to it according to priority
int wlcconnect_main(void)
{
_dprintf("%s: Start to run...\n", __FUNCTION__);
	int ret, old_ret = -1;
	int link_setup = 0, wlc_count = 0;
	int wanduck_notify = NOTIFY_IDLE;
#if defined(RTCONFIG_BLINK_LED)
	int unit = nvram_get_int("wlc_band");
	char *led_gpio = NULL, ifname[IFNAMSIZ];
#endif

	signal(SIGTERM, wlcconnect_safeleave);

	nvram_set_int("wlc_state", WLC_STATE_INITIALIZING);
	nvram_set_int("wlc_sbstate", WLC_STOPPED_REASON_NONE);
	nvram_set_int("wlc_state", WLC_STATE_CONNECTING);

	while (1) {
		ret = wlcconnect_core();
		if (ret == WLC_STATE_CONNECTED) nvram_set_int("wlc_state", WLC_STATE_CONNECTED);
		else if (ret == WLC_STATE_CONNECTING) {
			nvram_set_int("wlc_state", WLC_STATE_STOPPED);
			nvram_set_int("wlc_sbstate", WLC_STOPPED_REASON_AUTH_FAIL);
		}
		else if (ret == WLC_STATE_INITIALIZING) {
			nvram_set_int("wlc_state", WLC_STATE_STOPPED);
			nvram_set_int("wlc_sbstate", WLC_STOPPED_REASON_NO_SIGNAL);
		}

		// let ret be two value: connected, disconnected.
		if (ret != WLC_STATE_CONNECTED)
			ret = WLC_STATE_CONNECTING;
		else if (nvram_match("lan_proto", "dhcp") && nvram_get_int("lan_state_t") != LAN_STATE_CONNECTED)
			ret = WLC_STATE_CONNECTING;

		if (link_setup == 1) {
			if (ret != WLC_STATE_CONNECTED) {
				if (wlc_count < 3) {
					wlc_count++;
_dprintf("Ready to disconnect...%d.\n", wlc_count);
#ifdef RTCONFIG_RALINK
					sleep(1);
#else
#ifdef RTCONFIG_QCA
#ifdef RTCONFIG_PROXYSTA
					if (mediabridge_mode())
						sleep(10);
					else
#endif
#endif
					sleep(5);
#endif
					continue;
				}
			}
			else
				wlc_count = 0;
		}

		if (ret != old_ret) {
			if (link_setup == 0) {
				if (ret == WLC_STATE_CONNECTED)
					link_setup = 1;
				/*else {
					sleep(5);
					continue;
				}//*/
			}

			if (link_setup) {
				if (ret == WLC_STATE_CONNECTED)
					wanduck_notify = NOTIFY_CONN;
				else
					wanduck_notify = NOTIFY_DISCONN;

				// notify the change to init.
				if (ret == WLC_STATE_CONNECTED)
					notify_rc_and_wait("restart_wlcmode 1");
				else
				{
					notify_rc_and_wait("restart_wlcmode 0");
#ifdef CONFIG_BCMWL5
					notify_rc("restart_wireless");
#endif
				}

#if defined(RTCONFIG_BLINK_LED)
#if defined(RTCONFIG_QCA)
				if (unit == 0)
					led_gpio = "led_2g_gpio";
				else if (unit == 1)
					led_gpio = "led_5g_gpio";

				if (led_gpio) {
					sprintf(ifname, "sta%d", unit);
					if (wanduck_notify == NOTIFY_CONN)
						append_netdev_bled_if(led_gpio, ifname);
					else
						remove_netdev_bled_if(led_gpio, ifname);
				}
#endif
#endif
			}

#ifdef WEB_REDIRECT
			if (wanduck_notify == NOTIFY_DISCONN) {
				wanduck_notify = NOTIFY_IDLE;

				logmessage("notify wanduck", "wlc_state change!");
				_dprintf("%s: notify wanduck: wlc_state=%d.\n", __FUNCTION__, ret);
				// notify the change to wanduck.
				kill_pidfile_s("/var/run/wanduck.pid", SIGUSR1);
			}
#endif

			old_ret = ret;
		}

		sleep(5);
	}

	return 0;
}

void repeater_pap_disable(void)
{
	char word[256], *next;
	int i;

	i = 0;

	foreach(word, nvram_safe_get("wl_ifnames"), next) {
		if (nvram_get_int("wlc_band") == i) {
			eval("ebtables", "-t", "filter", "-I", "FORWARD", "-i", word, "-j", "DROP");
			break;
		}
		i++;
	}
}

#endif
