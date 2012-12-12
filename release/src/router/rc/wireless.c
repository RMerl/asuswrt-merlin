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
#ifdef RTCONFIG_WIRELESSREPEATER
#include <semaphore_mfp.h>
#endif


//	ref: http://wiki.openwrt.org/OpenWrtDocs/nas

//	#define DEBUG_TIMING

static int security_on(int idx, int unit, int subunit, void *param)
{
	return nvram_get_int(wl_nvname("radio", unit, 0)) && (!nvram_match(wl_nvname("security_mode", unit, subunit), "disabled"));
}
#ifdef REMOVE
static int is_wds(int idx, int unit, int subunit, void *param)
{
	return nvram_get_int(wl_nvname("radio", unit, 0)) && nvram_get_int(wl_nvname("wds_enable", unit, subunit));
}
#endif
#ifndef CONFIG_BCMWL5
static int is_sta(int idx, int unit, int subunit, void *param)
{
	return nvram_match(wl_nvname("mode", unit, subunit), "sta");
}
#endif
#ifdef REMOVE
int wds_enable(void)
{
	return foreach_wif(1, NULL, is_wds);
}
#endif
#ifdef CONFIG_BCMWL5
int
start_nas(void)
{
	int ret = eval("nas");

#ifdef __CONFIG_MEDIA_IPTV__

	pid_t pid;
	pid_t prev_pid = -1;
	int found = 0;

	/* Iterate a few times to allow nas to finish daemonizing itself. */
	do {
		sleep(1);
		if ((pid = get_pid_by_name("nas")) <= 0)
			break;
		found = (pid == prev_pid) ? found + 1 : 1;
		prev_pid = pid;
	} while (found < 3);

	if (found == 3)
		pmon_register(pid, &start_nas);

#endif /* __CONFIG_MEDIA_IPTV__ */

	return ret;
}

int
stop_nas(void)
{
	int ret;

#ifdef __CONFIG_MEDIA_IPTV__

	pid_t pid;

	if ((pid = pmon_get_pid(&start_nas)) > 0) 
		pmon_unregister(pid);

#endif /* __CONFIG_MEDIA_IPTV__ */

	ret = eval("killall", "nas");

	return ret;
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
#endif /* CONFIG_BCMWL5 */


#ifdef RTCONFIG_WIRELESSREPEATER
#define APSCAN_INFO "/tmp/apscan_info.txt"

static void wlcscan_safeleave(int signo) {
	signal(SIGTERM, SIG_IGN);
	
	nvram_set_int("wlc_scan_state", WLCSCAN_STATE_STOPPED);
	spinlock_unlock(SPINLOCK_SiteSurvey);
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
	spinlock_lock(SPINLOCK_SiteSurvey);
	if ((fp = fopen(APSCAN_INFO, "w")) != NULL){
		fclose(fp);
	}
	spinlock_unlock(SPINLOCK_SiteSurvey);
	
	nvram_set_int("wlc_scan_state", WLCSCAN_STATE_INITIALIZING);
	/* Starting scanning */
	i = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		wlcscan_core(APSCAN_INFO, word);
		// suppose only two or less interface handled
		nvram_set_int("wlc_scan_state", WLCSCAN_STATE_2G+i);
		i++;
	}
	nvram_set_int("wlc_scan_state", WLCSCAN_STATE_FINISHED);
	return 1;
}

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
	int ret;
	int old_ret = -1;

	signal(SIGTERM, wlcconnect_safeleave);

	nvram_set_int("wlc_state", WLC_STATE_INITIALIZING);
	nvram_set_int("wlc_sbstate", WLC_STOPPED_REASON_NONE);
	// prepare nvram
	// wl_mode_x = 3 ==> wet
	// wl_mode_x = 4 ==> ure
	
	nvram_set_int("wlc_state", WLC_STATE_CONNECTING);

	int link_setup = 0, wlc_count = 0;
#define NOTIFY_IDLE 0
#define NOTIFY_CONN 1
#define NOTIFY_DISCONN -1
	int wanduck_notify = NOTIFY_IDLE;
	while(1) {
		ret = wlcconnect_core();

		if(ret == WLC_STATE_CONNECTED) nvram_set_int("wlc_state", WLC_STATE_CONNECTED);
		else if(ret == WLC_STATE_CONNECTING) {
			nvram_set_int("wlc_state", WLC_STATE_STOPPED);
			nvram_set_int("wlc_sbstate", WLC_STOPPED_REASON_AUTH_FAIL);
		}
		else if(ret == WLC_STATE_INITIALIZING) {
			nvram_set_int("wlc_state", WLC_STATE_STOPPED);
			nvram_set_int("wlc_sbstate", WLC_STOPPED_REASON_NO_SIGNAL);
		}

		// let ret be two value: connected, disconnected.
		if(ret != WLC_STATE_CONNECTED)
			ret = WLC_STATE_CONNECTING;
		else if(nvram_match("lan_proto", "dhcp") && nvram_get_int("lan_state_t") != LAN_STATE_CONNECTED) 
			ret = WLC_STATE_CONNECTING;

		if(link_setup == 1){
			if(ret != WLC_STATE_CONNECTED){
				if(wlc_count < 3){
					wlc_count++;
_dprintf("Ready to disconnect...%d.\n", wlc_count);
					sleep(5);
					continue;
				}
			}
			else
				wlc_count = 0;
		}

		if(ret != old_ret){
			if(link_setup == 0){
				if(ret == WLC_STATE_CONNECTED)
					link_setup = 1;
				/*else{
					sleep(5);
					continue;
				}//*/
			}

			if(link_setup){
				if(ret == WLC_STATE_CONNECTED)
					wanduck_notify = NOTIFY_CONN;
				else
					wanduck_notify = NOTIFY_DISCONN;

				// notify the change to init.
				if(ret == WLC_STATE_CONNECTED)
					notify_rc_and_wait("restart_wlcmode 1");
				else
					notify_rc_and_wait("restart_wlcmode 0");
			}

#ifdef WEB_REDIRECT
			if(wanduck_notify == NOTIFY_DISCONN){
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
}

void repeater_pap_disable(void)
{
	char word[256], *next;
	int i;

	i = 0;

	foreach(word, nvram_safe_get("wl_ifnames"), next) {
		if(nvram_get_int("wlc_band")==i) {
			eval("ebtables", "-t", "filter", "-I", "FORWARD", "-i", word, "-j", "DROP");
			break;
		}
		i++;
	}
}

#endif
