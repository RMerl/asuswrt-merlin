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
 */
#include <rtconfig.h>
#ifdef RTCONFIG_USB_MODEM
#include <usb_info.h>
#endif

#include "wanduck.h"

#define NO_DETECT_INTERNET
#define NO_IOS_DETECT_INTERNET
#ifdef RTCONFIG_LAN4WAN_LED
int LanWanLedCtrl(void);
#endif

static int wdbg = 0;

#define _wdbg(fmt, args...) do { if (wdbg) { dbg(fmt, ## args); }; } while (0)

int update_wan_leds(int wan_unit)
{
#if defined(RTCONFIG_LANWAN_LED)
#if defined(RTCONFIG_WANRED_LED)
	int mode = sw_mode, l = link_wan[wan_unit], state;
	char s[] = "wanX_state_tXXX";

	if (wan_unit < 0 || wan_unit >= WAN_UNIT_MAX)
		return -1;

	if (mode < SW_MODE_ROUTER || mode > SW_MODE_HOTSPOT)
		mode = nvram_get_int("sw_mode");
	/* Turn on/off WAN BLUE/RED LED in accordance with wan status. */
	switch (mode) {
	case SW_MODE_ROUTER:
#if defined(RTCONFIG_DUALWAN)
		if (!strcmp(dualwan_mode, "lb")) {
			int u, onoff = 0;

			if (wan_unit)
				return 0;

			/* Turn on WAN BLUE LED if any WAN unit is connected in load-balanced mode. */
			for (u = WAN_UNIT_FIRST; !onoff && u < WAN_UNIT_MAX; ++u) {
				sprintf(s, "wan%d_state_t", u);
				state = nvram_get_int(s);
				l = link_wan[u];
				if (dualwan_unit__nonusbif(u))
					l = get_wanports_status(u);

				if (l == CONNED && state == WAN_STATE_CONNECTED)
					onoff++;
			}

			if (onoff) {
				wan_red_led_control(LED_OFF);
				led_control(LED_WAN, LED_ON);
			} else {
				wan_red_led_control(LED_ON);
				led_control(LED_WAN, LED_OFF);
			}
		} else {
#endif
			if (wan_primary_ifunit() != wan_unit)
				return 0;

			sprintf(s, "wan%d_state_t", wan_unit);
			state = nvram_get_int(s);
			if (dualwan_unit__nonusbif(wan_unit))
				l = get_wanports_status(wan_unit);

			if (l == CONNED && state == WAN_STATE_CONNECTED) {
				wan_red_led_control(LED_OFF);
				led_control(LED_WAN, LED_ON);
			} else {
				wan_red_led_control(LED_ON);
				led_control(LED_WAN, LED_OFF);
			}
#if defined(RTCONFIG_DUALWAN)
		}
#endif
		break;
	case SW_MODE_REPEATER:	/* fallthrough */
	case SW_MODE_AP:
		wan_red_led_control(LED_OFF);
		break;
	}
#else	/* !RTCONFIG_WANRED_LED */
	/* Turn on/off WAN LED in accordance with link status of WAN port */
	if (link_wan[wan_unit]) {
		led_control(LED_WAN, LED_ON);
	} else {
		led_control(LED_WAN, LED_OFF);
	}
#endif	/* RTCONFIG_WANRED_LED */
#endif	/* RTCONFIG_LANWAN_LED */

	return 0;
}

static void safe_leave(int signo){
	csprintf("\n## wanduck.safeexit ##\n");
	signal(SIGTERM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	FD_ZERO(&allset);
	close(http_sock);
	close(dns_sock);

	int i, ret;
	for(i = 0; i < maxfd; ++i){
		ret = close(i);
		csprintf("## close %d: result=%d.\n", i, ret);
	}

#ifndef RTCONFIG_BCMARM
	sleep(1);
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER){
		eval("ebtables", "-t", "broute", "-F");
		eval("ebtables", "-t", "filter", "-F");
		f_write_string("/proc/net/dnsmqctrl", "", 0, 0);
	}
#endif

		int len;
		char *fn = NAT_RULES, ln[PATH_MAX];
		struct stat s;

		csprintf("\n# Disable direct rule(exit wanduck)\n");

		rule_setup = 0;
		conn_changed_state[current_wan_unit] = CONNED; // for cleaning the redirect rules.

#if 0
		// in the function safe_leave(), couldn't set any nvram using nvram_set().
		// So the notify mechanism would be invalid.
#if 0
		handle_wan_line(current_wan_unit, rule_setup);
#else // or
		start_nat_rules();
#endif
#else
		// Couldn't directly use nvram_set().
		// Must use the command: nvram, and set the nat_state.
		char buf[16];
		FILE *fp = fopen(NAT_RULES, "r");

		memset(buf, 0, 16);
		if(fp != NULL){
			fclose(fp);

			if (!lstat(NAT_RULES, &s) && S_ISLNK(s.st_mode) &&
			    (len = readlink(NAT_RULES, ln, sizeof(ln))) > 0) {
				ln[len] = '\0';
				fn = ln;
			}

			sprintf(buf, "nat_state=%d", NAT_STATE_NORMAL);
			eval("nvram", "set", buf);

			csprintf("%s: apply the nat_rules(%s): %s!\n", __FUNCTION__, fn, buf);
			logmessage("wanduck exit", "apply the nat_rules(%s)!", fn);

			setup_ct_timeout(TRUE);
			setup_udp_timeout(TRUE);

			eval("iptables-restore", NAT_RULES);
		}
		else{
			sprintf(buf, "nat_state=%d", NAT_STATE_INITIALIZING);
			eval("nvram", "set", buf);

			csprintf("%s: initial the nat_rules: %s!\n", __FUNCTION__, buf);
			logmessage("wanduck exit", "initial the nat_rules!");
		}
#endif

	remove(WANDUCK_PID_FILE);

	csprintf("\n# return(exit wanduck)\n");
	exit(0);
}

void get_related_nvram(){
	int unit;

	sw_mode = nvram_get_int("sw_mode");

	boot_end = nvram_get_int("success_start_service");

	if(nvram_match("x_Setting", "1"))
		isFirstUse = 0;
	else
		isFirstUse = 1;

	if(nvram_match("nat_redirect_enable", "0"))
		nat_redirect_enable = 0;
	else
		nat_redirect_enable = 1;

#ifdef RTCONFIG_WIRELESSREPEATER
	if(strlen(nvram_safe_get("wlc_ssid")) > 0)
		setAP = 1;
	else
		setAP = 0;
#endif

#ifdef RTCONFIG_DUALWAN
	memset(dualwan_mode, 0, 8);
	strcpy(dualwan_mode, nvram_safe_get("wans_mode"));
	memset(dualwan_wans, 0, 16);
	strcpy(dualwan_wans, nvram_safe_get("wans_dualwan"));

	memset(wandog_target, 0, PATH_MAX);
	if(sw_mode == SW_MODE_ROUTER){
		wandog_enable = nvram_get_int("wandog_enable");
		scan_interval = nvram_get_int("wandog_interval");
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
			max_disconn_count[unit] = nvram_get_int("wandog_maxfail");
		wandog_delay = nvram_get_int("wandog_delay");

		if((!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb"))
				&& wandog_enable == 1
				){
			strcpy(wandog_target, nvram_safe_get("wandog_target"));
		}

		if(!strcmp(dualwan_mode, "fb")){
			max_fb_count = nvram_get_int("wandog_fb_count");
			max_fb_wait_time = scan_interval*max_fb_count;
		}
	}
	else{
		wandog_enable = 0;
		scan_interval = DEFAULT_SCAN_INTERVAL;
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
			max_disconn_count[unit] = DEFAULT_MAX_DISCONN_COUNT;
		wandog_delay = -1;
	}
#else
	wandog_enable = 0;
	scan_interval = DEFAULT_SCAN_INTERVAL;
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
		max_disconn_count[unit] = DEFAULT_MAX_DISCONN_COUNT;
#endif
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
		max_wait_time[unit] = scan_interval*max_disconn_count[unit];
}

void get_lan_nvram(){
	char nvram_name[16];

	current_lan_unit = nvram_get_int("lan_unit");

	memset(prefix_lan, 0, 8);
	if(current_lan_unit < 0)
		strcpy(prefix_lan, "lan_");
	else
		sprintf(prefix_lan, "lan%d_", current_lan_unit);

	memset(current_lan_ifname, 0, 16);
	strcpy(current_lan_ifname, nvram_safe_get(strcat_r(prefix_lan, "ifname", nvram_name)));

	memset(current_lan_proto, 0, 16);
	strcpy(current_lan_proto, nvram_safe_get(strcat_r(prefix_lan, "proto", nvram_name)));

	memset(current_lan_ipaddr, 0, 16);
	strcpy(current_lan_ipaddr, nvram_safe_get(strcat_r(prefix_lan, "ipaddr", nvram_name)));

	memset(current_lan_netmask, 0, 16);
	strcpy(current_lan_netmask, nvram_safe_get(strcat_r(prefix_lan, "netmask", nvram_name)));

	memset(current_lan_gateway, 0, 16);
	strcpy(current_lan_gateway, nvram_safe_get(strcat_r(prefix_lan, "gateway", nvram_name)));

	memset(current_lan_dns, 0, 256);
	strcpy(current_lan_dns, nvram_safe_get(strcat_r(prefix_lan, "dns", nvram_name)));

	memset(current_lan_subnet, 0, 11);
	strcpy(current_lan_subnet, nvram_safe_get(strcat_r(prefix_lan, "subnet", nvram_name)));

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER){
		wlc_state = nvram_get_int("wlc_state");
		got_notify = 1;
	}
#endif

	csprintf("# wanduck: Got LAN(%d) information:\n", current_lan_unit);
	if(nvram_match("wanduck_debug", "1")){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER){
			csprintf("# wanduck:   ipaddr=%s.\n", current_lan_ipaddr);
			csprintf("# wanduck:wlc_state=%d.\n", wlc_state);
		}
		else
#endif
		{
			csprintf("# wanduck:   ifname=%s.\n", current_lan_ifname);
			csprintf("# wanduck:    proto=%s.\n", current_lan_proto);
			csprintf("# wanduck:   ipaddr=%s.\n", current_lan_ipaddr);
			csprintf("# wanduck:  netmask=%s.\n", current_lan_netmask);
			csprintf("# wanduck:  gateway=%s.\n", current_lan_gateway);
			csprintf("# wanduck:      dns=%s.\n", current_lan_dns);
			csprintf("# wanduck:   subnet=%s.\n", current_lan_subnet);
		}
	}
}

static void get_network_nvram(int signo){
	if(sw_mode == SW_MODE_AP)
		get_lan_nvram();
#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER)
		get_lan_nvram();
	else if(sw_mode == SW_MODE_HOTSPOT)
		wlc_state = nvram_get_int("wlc_state");
#endif
}

/* 87u,3200: 	  have each led on every port. 
 * 88u,3100,5300: have one led to hint wan port but this led is the union of all ports 
 * force led_on on usb modem case */

void enable_wan_wled()
{
	int usb_wan = get_dualwan_by_unit(wan_primary_ifunit()) == WANS_DUALWAN_IF_USB ? 1:0;

	if(usb_wan) {
        	switch (get_model()) {
                	case MODEL_RTAC3200:
                	case MODEL_RTAC87U:
				eval("et", "robowr", "0", "0x18", "0x01ff");
				eval("et", "robowr", "0", "0x1a", "0x01fe");
                        return;

                	case MODEL_RTAC5300:
                	case MODEL_RTAC88U:
                	case MODEL_RTAC3100:
				eval("et", "robowr", "0", "0x18", "0x01ff");
				eval("et", "robowr", "0", "0x1a", "0");
                        return;
        	}
	}

	eval("et", "robowr", "0", "0x18", "0x01ff");
	eval("et", "robowr", "0", "0x1a", "0x01ff");
}

void disable_wan_wled()
{
	eval("et", "robowr", "0", "0x18", "0x01fe");
	eval("et", "robowr", "0", "0x1a", "0x01fe");
}

static void wan_led_control(int sig) {
#if 0
#if defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300) 
	char buf[16];
        snprintf(buf, 16, "%s", nvram_safe_get("wans_dualwan"));
	if (strcmp(buf, "wan none") != 0){
		logmessage("DualWAN", "skip single wan wan_led_control()");
		return;
	}
#endif
#endif
	if(nvram_match("AllLED", "1")){
#if defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300) 
		if (rule_setup) {
			led_control(LED_WAN, LED_ON);
			disable_wan_wled();
		} else {
			led_control(LED_WAN, LED_OFF);
			enable_wan_wled();
		}
#elif defined(DSL_AC68U)
		if (rule_setup) {
			led_control(LED_WAN, LED_OFF);
		} else {
			led_control(LED_WAN, LED_ON);
		}
#endif
	}
}

int do_ping_detect(int wan_unit){
#ifdef RTCONFIG_DUALWAN
	char cmd[256];
#endif
#if 0
	char buf[16], *next;
	char prefix_wan[8], nvram_name[16], wan_dns[256];

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);

	memset(wan_dns, 0, 256);
	strcpy(wan_dns, nvram_safe_get(strcat_r(prefix_wan, "dns", nvram_name)));

	foreach(buf, wan_dns, next){
		sprintf(cmd, "ping -c 1 -W %d %s && touch %s", TCPCHECK_TIMEOUT, buf, PING_RESULT_FILE);
		system(cmd);

		if(check_if_file_exist(PING_RESULT_FILE)){
			unlink(PING_RESULT_FILE);
			return 1;
		}
	}
#elif defined(RTCONFIG_DUALWAN)
	csprintf("wanduck: ping %s to %s...", wandog_target, PING_RESULT_FILE);
	sprintf(cmd, "ping -c 1 %s >/dev/null && touch %s", wandog_target, PING_RESULT_FILE);
	system(cmd);

	if(check_if_file_exist(PING_RESULT_FILE)){
		csprintf("ok.\n");
		unlink(PING_RESULT_FILE);
		return 1;
	}
#else
	return 1;
#endif
	_dprintf("\n ping failed.\n");
	return 0;
}

#ifndef NO_DETECT_INTERNET
int get_packets_of_net_dev(const char *net_dev, unsigned long *rx_packets, unsigned long *tx_packets){
	FILE *fp;
	char buf[256];
	char *ifname;
	char *ptr;
	int i, got_packets = 0;

	if((fp = fopen(PROC_NET_DEV, "r")) == NULL){
		_dprintf("%s: Can't open the file: %s.\n", __FUNCTION__, PROC_NET_DEV);
		return got_packets;
	}

	fcntl(fileno(fp), F_SETFL, fcntl(fileno(fp), F_GETFL) | O_NONBLOCK);

	// headers.
	for(i = 0; i < 2; ++i){
		if(fgets(buf, sizeof(buf), fp) == NULL){
			fclose(fp);
			logmessage("wanduck", "%s: Can't read out the headers of %s.\n", __FUNCTION__, PROC_NET_DEV);
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				got_packets = 1;

			return got_packets;
		}
	}

	while(fgets(buf, sizeof(buf), fp) != NULL){
		if((ptr = strchr(buf, ':')) == NULL)
			continue;

		*ptr = 0;
		if((ifname = strrchr(buf, ' ')) == NULL)
			ifname = buf;
		else
			++ifname;

		if(strcmp(ifname, net_dev))
			continue;

		// <rx bytes, packets, errors, dropped, fifo errors, frame errors, compressed, multicast><tx ...>
		if(sscanf(ptr+1, "%*u%lu%*u%*u%*u%*u%*u%*u%*u%lu", rx_packets, tx_packets) != 2){
			fclose(fp);
			logmessage("wanduck", "%s: Can't read the packet's number in %s.\n", __FUNCTION__, PROC_NET_DEV);
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				got_packets = 1;
		
			return got_packets;
		}

		got_packets = 1;
		break;
	}
	fclose(fp);

	return got_packets;
}

char *organize_tcpcheck_cmd(char *dns_list, char *cmd, int size){
	char buf[256], *next;

	if(cmd == NULL || size <= 0)
		return NULL;

	memset(cmd, 0, size);

	sprintf(cmd, "/sbin/tcpcheck %d", TCPCHECK_TIMEOUT);

	foreach(buf, dns_list, next){
		sprintf(cmd, "%s %s:53", cmd, buf);
	}

	sprintf(cmd, "%s >>%s", cmd, DETECT_FILE);

	return cmd;
}

int do_tcp_dns_detect(int wan_unit){
	FILE *fp = NULL;
	char line[80], cmd[PATH_MAX];
	char prefix_wan[8], nvram_name[16], wan_dns[256];

	if(remove(DETECT_FILE) < 0)
	{
		logmessage("%s: cannot remove DETECT_FILE.\n", __FUNCTION__);
		return 0;
	}

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);

	memset(wan_dns, 0, 256);
	strcpy(wan_dns, nvram_safe_get(strcat_r(prefix_wan, "dns", nvram_name)));

	if(organize_tcpcheck_cmd(wan_dns, cmd, PATH_MAX) == NULL){
		csprintf("wanduck: No tcpcheck cmd.\n");
		return 0;
	}
	system(cmd);

	if((fp = fopen(DETECT_FILE, "r")) == NULL){
		_dprintf("wanduck: No file: %s.\n", DETECT_FILE);
		return 0;
	}
	
	fcntl(fileno(fp), F_SETFL, fcntl(fileno(fp), F_GETFL) | O_NONBLOCK);

	while(fgets(line, sizeof(line), fp) != NULL){
		if(strstr(line, "alive")){
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);

	return 0;
}
#endif

int detect_internet(int wan_unit){
	int link_internet;
#ifndef NO_DETECT_INTERNET
	unsigned long rx_packets, tx_packets;
#endif
	char prefix_wan[8], wan_ifname[16];

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);

	memset(wan_ifname, 0, 16);
	strcpy(wan_ifname, get_wan_ifname(wan_unit));

	if(
#ifdef RTCONFIG_DUALWAN
			strcmp(dualwan_mode, "lb") &&
#endif
			!found_default_route(wan_unit)
			)
		link_internet = DISCONN;
#ifndef NO_DETECT_INTERNET
	else if(!get_packets_of_net_dev(wan_ifname, &rx_packets, &tx_packets) || rx_packets <= RX_THRESHOLD)
		link_internet = DISCONN;
	else if(!isFirstUse && (!do_dns_detect() && !do_tcp_dns_detect(wan_unit) && !do_ping_detect(wan_unit)))
		link_internet = DISCONN;
#endif
#ifdef RTCONFIG_DUALWAN
	else if((!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb"))
			&& wandog_enable == 1 && !isFirstUse && !do_ping_detect(wan_unit))
		link_internet = DISCONN;
#endif
	else
		link_internet = CONNED;

	if(link_internet == DISCONN){
#ifndef NO_DETECT_INTERNET
		nvram_set_int("link_internet", 1);
#endif
		record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NO_INTERNET_ACTIVITY);

		if(!(nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOINTERNET)) {
#ifndef NO_DETECT_INTERNET
			nvram_set_int("link_internet", 2);
#endif
			link_internet = CONNED;
		}
	}
	else{
#ifndef NO_DETECT_INTERNET
		nvram_set_int("link_internet", 2);
#endif
		record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NONE);
	}

	return link_internet;
}

int passivesock(char *service, int protocol_num, int qlen){
	//struct servent *pse;
	struct sockaddr_in sin;
	int s, type, on;
	
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	
	// map service name to port number
	if((sin.sin_port = htons((u_short)atoi(service))) == 0){
		perror("cannot get service entry");
		
		return -1;
	}
	
	if(protocol_num == IPPROTO_UDP)
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;
	
	s = socket(PF_INET, type, protocol_num);
	if(s < 0){
		perror("cannot create socket");
		
		return -1;
	}
	
	on = 1;
	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0){
		perror("cannot set socket's option: SO_REUSEADDR");
		close(s);
		
		return -1;
	}
	
	if(bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		perror("cannot bind port");
		close(s);
		
		return -1;
	}
	
	if(type == SOCK_STREAM && listen(s, qlen) < 0){
		perror("cannot listen to port");
		close(s);
		
		return -1;
	}
	
	return s;
}

#if 0 
int check_ppp_exist(){
	DIR *dir;
	struct dirent *dent;
	char task_file[64], cmdline[64];
	int pid, fd;

	if((dir = opendir("/proc")) == NULL){
		perror("open proc");
		return 0;
	}

	while((dent = readdir(dir)) != NULL){
		if((pid = atoi(dent->d_name)) > 1){
			memset(task_file, 0, 64);
			sprintf(task_file, "/proc/%d/cmdline", pid);
			if((fd = open(task_file, O_RDONLY | O_NONBLOCK)) > 0){
				memset(cmdline, 0, 64);
				read(fd, cmdline, 64);
				close(fd);

				if(strstr(cmdline, "pppd")
						|| strstr(cmdline, "l2tpd")
						|| (errno==EAGAIN || errno==EWOULDBLOCK)
						){
					closedir(dir);
					return 1;
				}
			}
			else
				printf("cannot open %s\n", task_file);
		}
	}
	closedir(dir);
	
	return 0;
}
#endif

int chk_proto(int wan_unit, int wan_state){
	int wan_sbstate = nvram_get_int(nvram_sbstate[wan_unit]);
	char prefix_wan[8], nvram_name[16], wan_proto[16];

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);

	memset(wan_proto, 0, 16);
	strcpy(wan_proto, nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_HOTSPOT){
		if(wan_state == WAN_STATE_STOPPED) {
			if(wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR)
				disconn_case[wan_unit] = CASE_THESAMESUBNET;
			else disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
	}
	else
#endif
	// Start chk_proto() in SW_MODE_ROUTER.
#ifdef RTCONFIG_USB_MODEM
	if (dualwan_unit__usbif(wan_unit)) {
#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
		unsigned long long rx, tx;
		unsigned long long total, alert, limit;
		char buff[128];

		eval("modem_status.sh", "bytes");

		rx = strtoull(nvram_safe_get("modem_bytes_rx"), NULL, 10);
		tx = strtoull(nvram_safe_get("modem_bytes_tx"), NULL, 10);
		alert = strtoull(nvram_safe_get("modem_bytes_data_warning"), NULL, 10);
		limit = strtoull(nvram_safe_get("modem_bytes_data_limit"), NULL, 10);

		total = rx+tx;

		if(limit > 0 && total >= limit){
			if(wan_state != WAN_STATE_STOPPED || nvram_get_int(nvram_sbstate[wan_unit]) != WAN_STOPPED_REASON_DATALIMIT){
				csprintf("wanduck(%d): chk_proto() detect the data limit.\n", wan_unit);
				record_wan_state_nvram(wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_DATALIMIT, -1);
			}

			if(nvram_get_int("modem_sms_limit") == 1 && nvram_get_int("modem_sms_limit_send") == 0){
				snprintf(buff, 128, "start_sendSMS limit");
				nvram_set("freeze_duck", "5");
				notify_rc(buff);
				nvram_set("modem_sms_limit_send", "1");
				// if send the limit SMS, the alert SMS is not needed.
				nvram_set("modem_sms_alert_send", "1");
			}

			return DISCONN;
		}

		if(alert > 0 && total >= alert){
			if(nvram_get_int("modem_sms_limit") == 1 && nvram_get_int("modem_sms_alert_send") == 0){
				snprintf(buff, 128, "start_sendSMS alert");
				nvram_set("freeze_duck", "5");
				notify_rc(buff);
				nvram_set("modem_sms_alert_send", "1");
			}
		}
#endif

		if(wan_state == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			ppp_fail_state = pppstatus();

			if(ppp_fail_state == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
				record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_PPP_AUTH_FAIL);

			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_STOPPED){
			if(wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR)
				disconn_case[wan_unit] = CASE_THESAMESUBNET;
			else if(!strcmp(wan_proto, "dhcp"))
				disconn_case[wan_unit] = CASE_DHCPFAIL;
			else
				disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
		else if(wan_sbstate == WAN_STOPPED_REASON_DATALIMIT){
			disconn_case[wan_unit] = CASE_DATALIMIT;
			return DISCONN;
		}
#endif
	}
	else
#endif
	if(!strcmp(wan_proto, "dhcp")
			|| !strcmp(wan_proto, "static")){
		if(wan_state == WAN_STATE_STOPPED) {
			if(wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR)
				disconn_case[wan_unit] = CASE_THESAMESUBNET;
			else disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
#if defined(RTCONFIG_WANRED_LED)
			int r = CASE_DHCPFAIL, v = DISCONN;

			if (!strcmp(wan_proto, "static") && link_wan[wan_unit]) {
				r = CASE_NONE;
				v = CONNED;
			}

			disconn_case[wan_unit] = r;
			return v;
#else
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
#endif
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
	}
	else if(!strcmp(wan_proto, "pppoe")
			|| !strcmp(wan_proto, "pptp")
			|| !strcmp(wan_proto, "l2tp")
			){
		ppp_fail_state = pppstatus();

		if(wan_state != WAN_STATE_CONNECTED
				&& ppp_fail_state == WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
			// PPP is into the idle mode.
			if(wan_state == WAN_STATE_STOPPED) // Sometimes ip_down() didn't set it.
				record_wan_state_nvram(wan_unit, -1, WAN_STOPPED_REASON_PPP_LACK_ACTIVITY, -1);
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			if(ppp_fail_state == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
				record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_PPP_AUTH_FAIL);

			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(wan_state == WAN_STATE_STOPPED){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
	}

	return CONNED;
}

int if_wan_phyconnected(int wan_unit){
	char wired_link_nvram[16];
#ifdef RTCONFIG_WIRELESSREPEATER
	int link_ap = 0;
#endif
	int link_changed = 0;
	char prefix[] = "wanXXXXXX_", tmp[100] = "";
	char wan_proto[16];
#ifdef RTCONFIG_USB_MODEM
	int sim_state = 0;
#endif

	if(wan_unit == WAN_UNIT_FIRST)
		snprintf(wired_link_nvram, 16, "link_wan");
	else
		snprintf(wired_link_nvram, 16, "link_wan%d", wan_unit);

	snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);

#ifdef RTCONFIG_USB_MODEM
	if(dualwan_unit__usbif(wan_unit)){
		int find_modem_node = 0;
		int wan_state = nvram_get_int(nvram_state[wan_unit]);

#ifdef RT4GAC55U
		if(strlen(usb_if) <= 0 && nvram_get_int("usb_gobi") == 1){
			snprintf(usb_if, 16, "%s", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
csprintf("wanduck: try to get usb_if=%s.\n", usb_if);
		}

		if(strlen(usb_if) > 0){
			if(strlen(nvram_safe_get("usb_modem_act_imei")) <= 0)
				eval("modem_status.sh", "imei");
			if(strlen(nvram_safe_get("usb_modem_act_hwver")) <= 0)
				eval("modem_status.sh", "hwver");
			if(strlen(nvram_safe_get("usb_modem_act_swver")) <= 0)
				eval("modem_status.sh", "swver");
		}
#endif

		// need to check before detecting SIM. If not, the detect of wanduck will be blocked.
		if(nvram_get_int("usb_modem_act_scanning") != 0){
			csprintf("wanduck(%d): detect the USB scan.\n", wan_unit);
			link_wan[wan_unit] = 4;
			sim_lock = 1;
		}
		else{
			modem_act_reset = nvram_get_int("usb_modem_act_reset");
			if(modem_act_reset == 1 || modem_act_reset == 2)
				return CONNED;

			if(wan_state == WAN_STATE_CONNECTING)
				return CONNED;

			link_wan[wan_unit] = is_usb_modem_ready(); // include find_modem_type.sh
			if(link_wan[wan_unit] && strlen(modem_type) <= 0)
				snprintf(modem_type, 32, "%s", nvram_safe_get("usb_modem_act_type"));
			sim_state = nvram_get_int("usb_modem_act_sim");
		}

		if(!strcmp(modem_type, "qmi") || !strcmp(modem_type, "gobi")){
			if(link_wan[wan_unit] == 1){
				if(!strcmp(nvram_safe_get("usb_modem_act_int"), "")){
					if(!strcmp(modem_type, "qmi")){	// e.q. Huawei E398.
						csprintf("wanduck(%d): Sleep 3 seconds to wait modem nodes.\n", wan_unit);
						sleep(3);
					}
					else{
						csprintf("wanduck(%d): Sleep 2 seconds to wait modem nodes.\n", wan_unit);
						sleep(2);
					}

					wan_state = nvram_get_int(nvram_state[wan_unit]); // after sleep(), wan_state is changed.

					if(!strcmp(nvram_safe_get("usb_modem_act_int"), ""))
						find_modem_node = 1;
				}

				if((!strcmp(modem_type, "tty") || !strcmp(modem_type, "mbim"))
						&& !strcmp(nvram_safe_get("usb_modem_act_bulk"), "")
						&& strcmp(nvram_safe_get("usb_modem_act_vid"), "6610") // e.q. ZTE MF637U
						){
					csprintf("wanduck(%d): finding the second tty node...\n", wan_unit);
					find_modem_node = 1;
				}

				if(find_modem_node)
					eval("find_modem_node.sh");

				if(!strcmp(modem_type, "tty") && !strcmp(nvram_safe_get("usb_modem_act_vid"), "6610")){ // e.q. ZTE MF637U
					if(wan_state == WAN_STATE_INITIALIZING)
						eval("modem_status.sh", "sim");
				}
				else if(wan_state != WAN_STATE_CONNECTING)
					eval("modem_status.sh", "sim");

#ifdef RT4GAC55U
				if(wan_state == WAN_STATE_CONNECTED)
					eval("modem_status.sh", "operation");
#endif

				sim_state = nvram_get_int("usb_modem_act_sim");
				if(sim_state == 2 || sim_state == 3){
					sim_lock = 1;

					if(sim_state == 3 || !strcmp(nvram_safe_get("modem_pincode"), "") || nvram_get_int(nvram_auxstate[wan_unit]) == WAN_STOPPED_REASON_PINCODE_ERR)
						link_wan[wan_unit] = 3;
				}
				else if(sim_state != 1)
					link_wan[wan_unit] = 0;

#ifdef RT4GAC55U
				if(sim_state >= 1 && sim_state <= 5 && !strcmp(nvram_safe_get("usb_modem_act_auth"), ""))
					eval("modem_status.sh", "simauth");
#endif
			}
		}

		nvram_set_int(strcat_r(prefix, "is_usb_modem_ready", tmp), link_wan[wan_unit]);
if(test_log)
_dprintf("# wanduck: if_wan_phyconnected: x_Setting=%d, link_modem=%d, sim_state=%d.\n", !isFirstUse, link_wan[wan_unit], sim_state);

#if defined(RTCONFIG_WANRED_LED)
		update_wan_leds(wan_unit);
#endif

		if(link_wan[wan_unit] != nvram_get_int(wired_link_nvram)){
			nvram_set_int(wired_link_nvram, link_wan[wan_unit]);
			if(link_wan[wan_unit] != 0)
				record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NONE);

			if(link_wan[wan_unit] == 3){
				if(sim_state == 3)
					logmessage("wanduck", "The modem need the PUK code to reset PIN.");
				else
					logmessage("wanduck", "The modem need the PIN code to unlock.");
			}
			else if(link_wan[wan_unit] == 4){
				logmessage("wanduck", "The modem is scanning the stations.");
			}
			//else if(strcmp(modem_type, "ncm"))
			else
				link_changed = 1;
		}

		if(link_wan[wan_unit] == 3)
			return SET_PIN;
		else if(link_wan[wan_unit] == 4)
			return SET_USBSCAN;
	}
	else
#endif // RTCONFIG_USB_MODEM
	{
		snprintf(wan_proto, 16, "%s", nvram_safe_get(strcat_r(prefix, "proto", tmp)));

		// check wan port.
		link_wan[wan_unit] = get_wanports_status(wan_unit);

		if (get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_WAN
#if defined(RTCONFIG_WANRED_LED)
		    || get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_LAN
#endif
		   )
		{
			update_wan_leds(wan_unit);
		}

		if(link_wan[wan_unit] != nvram_get_int(wired_link_nvram)){
			if(link_wan[wan_unit]){
				nvram_set_int(wired_link_nvram, CONNED);
				record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NONE);
			}
			else{
				nvram_set_int(wired_link_nvram, DISCONN);
				record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);
			}

			link_changed = 1;
		}

		// after update_wan_state(), auxstate will be cleaned.
		if(!link_wan[wan_unit])
			record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);
	}

#ifdef RTCONFIG_LANWAN_LED
	if(get_lanports_status()) led_control(LED_LAN, LED_ON);
	else led_control(LED_LAN, LED_OFF);
#endif

#ifdef RTCONFIG_LAN4WAN_LED
	LanWanLedCtrl();
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
	// check if set AP.
	if(sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT){
		link_ap = (wlc_state == WLC_STATE_CONNECTED);
		if(link_ap != nvram_get_int("link_ap"))
			nvram_set_int("link_ap", link_ap);
	}
#endif

	if(sw_mode == SW_MODE_ROUTER){
		// this means D2C because of reconnecting the WAN port.
		if(link_changed){
#ifdef RTCONFIG_USB_MODEM
			if(dualwan_unit__usbif(wan_unit)){
				if(link_wan[wan_unit]){
					if(sim_lock){
						sim_lock = 0;
						record_wan_state_nvram(wan_unit, WAN_STATE_INITIALIZING, WAN_STOPPED_REASON_NONE, WAN_AUXSTATE_NONE);
					}

					csprintf("wanduck(%d): PHY_RECONN.\n", wan_unit);
					return PHY_RECONN;
				}
				else{
					record_wan_state_nvram(wan_unit, WAN_STATE_STOPPED, -1, WAN_AUXSTATE_NOPHY);
					csprintf("wanduck(%d): SIM or modem is pulled off.\n", wan_unit);
					return DISCONN;
				}
			}
			else
#endif
			// WAN port was disconnected, arm reconnect
			if(!link_setup[wan_unit] && !link_wan[wan_unit]){
				link_setup[wan_unit] = 1;
			}
			// WAN port was connected, fire reconnect if armed
			else if(link_setup[wan_unit]){
				link_setup[wan_unit] = 0;

				// Only handle this case when WAN proto is DHCP or Static
				if(!strcmp(wan_proto, "static")){
					disconn_case[wan_unit] = CASE_DISWAN;
					return PHY_RECONN;
				}
				else if(!strcmp(wan_proto, "dhcp")){
					disconn_case[wan_unit] = CASE_DHCPFAIL;
					return PHY_RECONN;
				}
			}
		}
		else if(!link_wan[wan_unit]){
#ifndef NO_DETECT_INTERNET
#ifdef RTCONFIG_DUALWAN
			if(strcmp(dualwan_mode, "lb"))
#endif
				nvram_set_int("link_internet", 1);
#endif

			if((nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)){
				disconn_case[wan_unit] = CASE_DISWAN;
			}

			max_disconn_count[wan_unit] = max_disconn_count[wan_unit]/2;
			if(max_disconn_count[wan_unit] < 1)
				max_disconn_count[wan_unit] = 1;
			max_wait_time[wan_unit] = scan_interval*max_disconn_count[wan_unit];

			return DISCONN;
		}
	}
	else if(sw_mode == SW_MODE_AP){
#if 0
		if(!link_wan[wan_unit]){
			// ?: type error?
			nvram_set_int("link_internet", 1);

			record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);

			if((nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)){
				disconn_case[wan_unit] = CASE_DISWAN;
				return DISCONN;
			}
		}
#else
		if(nvram_get_int("link_internet") != 2)
			nvram_set_int("link_internet", 2);

#ifdef RTCONFIG_DHCP_OVERRIDE
		if (nvram_match("dnsqmode", "2")) {
			disconn_case[wan_unit] = CASE_DISWAN;
			return DISCONN;
		}
		else
#endif
		return CONNED;
#endif
	}
#ifdef RTCONFIG_WIRELESSREPEATER
	else{ // sw_mode == SW_MODE_REPEATER, SW_MODE_HOTSPOT.
		if(!link_ap){
			if(nvram_get_int("link_internet") != 1)
				nvram_set_int("link_internet", 1);

			if(sw_mode == SW_MODE_HOTSPOT)
				record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);

			disconn_case[wan_unit] = CASE_AP_DISCONN;
			return DISCONN;
		}
		else if(sw_mode == SW_MODE_REPEATER){
			if(nvram_match("lan_proto", "dhcp") && nvram_get_int("lan_state_t") != LAN_STATE_CONNECTED){
				if(nvram_get_int("link_internet") != 1)
					nvram_set_int("link_internet", 1);

				return DISCONN;
			}
			else{
				if(nvram_get_int("link_internet") != 2)
					nvram_set_int("link_internet", 2);

				return CONNED;
			}
		}
	}
#endif

	return CONNED;
}

int if_wan_connected(int wan_unit, int wan_state){
	if(chk_proto(wan_unit, wan_state) != CONNED)
		return DISCONN;
	else if(sw_mode == SW_MODE_ROUTER) // TODO: temparily let detect_internet() service in SW_MODE_ROUTER.
		return detect_internet(wan_unit);

	return CONNED;
}

void handle_wan_line(int wan_unit, int action){
	char cmd[32];
	char prefix_wan[8], nvram_name[16], wan_proto[16];

	// Redirect rules.
	if(action){
#ifdef RTCONFIG_USB_MODEM
#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
		if(nvram_get_int(nvram_sbstate[current_wan_unit]) == WAN_STOPPED_REASON_DATALIMIT){
			snprintf(cmd, 32, "restart_wan_if %d", wan_unit);
			notify_rc_and_wait(cmd);
		}
		else
#endif
#endif
			stop_nat_rules();
	}
	/*
	 * When C2C and remove the redirect rules,
	 * it means dissolve the default state.
	 */
	else if(conn_changed_state[wan_unit] == D2C || conn_changed_state[wan_unit] == CONNED){
		start_nat_rules();

		snprintf(prefix_wan, 8, "wan%d_", wan_unit);

		snprintf(wan_proto, 16, "%s", nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

		if(!strcmp(wan_proto, "static")){
#if defined(RTCONFIG_WANRED_LED)
			char tmp[100];
			char *gateway, *wan_ifname;

			wan_ifname = nvram_safe_get(strcat_r(prefix_wan, "ifname", tmp));
			gateway = nvram_safe_get(strcat_r(prefix_wan, "gateway", tmp));
			if (!test_gateway(gateway, wan_ifname)) {
				update_wan_state(prefix_wan, WAN_STATE_CONNECTED, 0);
			} else {
				update_wan_state(prefix_wan, WAN_STATE_CONNECTING, 0);
			}
#endif
			/* Sync time */
			refresh_ntpc();
		}

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
		if(check_if_dir_exist("/opt/lib/ipkg")){
			csprintf("wanduck: update the APP's lists...\n");
			notify_rc("start_apps_update");
		}
#endif
	}
	else{ // conn_changed_state[wan_unit] == PHY_RECONN
		_dprintf("\n# wanduck(%d): Try to restart_wan_if.\n", action);
		snprintf(cmd, 32, "restart_wan_if %d", wan_unit);
		notify_rc_and_wait(cmd);
	}
}

void close_socket(int sockfd, char type){
	close(sockfd);
	FD_CLR(sockfd, &allset);
	client[fd_i].sfd = -1;
	client[fd_i].type = 0;
}

int build_socket(char *http_port, char *dns_port, int *hd, int *dd){
	if((*hd = passivesock(http_port, IPPROTO_TCP, 10)) < 0){
		csprintf("Fail to socket for httpd port: %s.\n", http_port);
		return -1;
	}
	
	if((*dd = passivesock(dns_port, IPPROTO_UDP, 10)) < 0){
		csprintf("Fail to socket for DNS port: %s.\n", dns_port);
		return -1;
	}
	
	return 0;
}

void send_page(int wan_unit, int sfd, char *file_dest, char *url){
	char buf[2*MAXLINE];
	time_t now;
	char timebuf[100];
	char dut_addr[64];

	memset(buf, 0, sizeof(buf));
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

#ifdef NO_IOS_DETECT_INTERNET
	// disable iOS popup window. Jerry5 2012.11.27
	if (!strcmp(url,"www.apple.com/library/test/success.html") && nvram_get_int("disiosdet") == 1){
		sprintf(buf, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", buf, "HTTP/1.0 200 OK\r\n", "Server: Apache/2.2.3 (Oracle)\r\n", "Content-Type: text/html; charset=UTF-8\r\n", "Cache-Control: max-age=557\r\n","Expires: ", timebuf, "\r\n", "Date: ", timebuf, "\r\n", "Content-Length: 127\r\n", "Connection: close\r\n\r\n","<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n","<HTML>\n","<HEAD>\n\t","<TITLE>Success</TITLE>\n","</HEAD>\n","<BODY>\n","Success\n","</BODY>\n","</HTML>\n");
	}
	else{
#endif

	sprintf(buf, "%s%s%s%s%s%s", buf, "HTTP/1.0 302 Moved Temporarily\r\n", "Server: wanduck\r\n", "Date: ", timebuf, "\r\n");

	memset(dut_addr, 0, 64);

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT)
		strcpy(dut_addr, DUT_DOMAIN_NAME);
	else
#endif
	if(isFirstUse)
		strcpy(dut_addr, DUT_DOMAIN_NAME);
	else
		strcpy(dut_addr, nvram_safe_get("lan_ipaddr"));

	// TODO: Only send pages for the wan(0)'s state.
#ifdef RTCONFIG_USB_MODEM
#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
	if((conn_changed_state[wan_unit] == C2D || conn_changed_state[wan_unit] == DISCONN) && disconn_case[wan_unit] == CASE_DATALIMIT)
		sprintf(buf, "%s%s%s%s%s%d%s%s" ,buf , "Connection: close\r\n", "Location:http://", dut_addr, "/error_page.htm?flag=", disconn_case[wan_unit], "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
	else
#endif
#endif
	if((conn_changed_state[wan_unit] == C2D || conn_changed_state[wan_unit] == DISCONN) && disconn_case[wan_unit] == CASE_THESAMESUBNET)
		sprintf(buf, "%s%s%s%s%s%d%s%s" ,buf , "Connection: close\r\n", "Location:http://", dut_addr, "/error_page.htm?flag=", disconn_case[wan_unit], "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
	else if(isFirstUse){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT)
			sprintf(buf, "%s%s%s%s%s%s%s" ,buf , "Connection: close\r\n", "Location:http://", dut_addr, "/QIS_default.cgi?flag=sitesurvey", "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
		else
		
#endif
			sprintf(buf, "%s%s%s%s%s%s%s" ,buf , "Connection: close\r\n", "Location:http://", dut_addr, "/QIS_default.cgi?flag=welcome", "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
	}
	else if(conn_changed_state[wan_unit] == C2D || conn_changed_state[wan_unit] == DISCONN)
		sprintf(buf, "%s%s%s%s%s%d%s%s" ,buf , "Connection: close\r\n", "Location:http://", dut_addr, "/error_page.htm?flag=", disconn_case[wan_unit], "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
#ifdef RTCONFIG_WIRELESSREPEATER
	else
		sprintf(buf, "%s%s%s%s%s", buf, "Connection: close\r\n", "Location:http://", dut_addr, "/index.asp\r\nContent-Type: text/plain\r\n\r\n<html></html>\r\n"); 
#endif

#ifdef NO_IOS_DETECT_INTERNET
	}
#endif
	write(sfd, buf, strlen(buf));
	close_socket(sfd, T_HTTP);
}

void parse_dst_url(char *page_src){
	int i, j;
	char dest[PATHLEN], host[64];
	char host_strtitle[7], *hp;
	
	j = 0;
	memset(dest, 0, sizeof(dest));
	memset(host, 0, sizeof(host));
	memset(host_strtitle, 0, sizeof(host_strtitle));
	
	for(i = 0; i < strlen(page_src); ++i){
		if(i >= PATHLEN)
			break;
		
		if(page_src[i] == ' ' || page_src[i] == '?'){
			dest[j] = '\0';
			break;
		}
		
		dest[j++] = page_src[i];
	}
	
	host_strtitle[0] = '\n';
	host_strtitle[1] = 'H';
	host_strtitle[2] = 'o';
	host_strtitle[3] = 's';
	host_strtitle[4] = 't';
	host_strtitle[5] = ':';
	host_strtitle[6] = ' ';
	
	if((hp = strstr(page_src, host_strtitle)) != NULL){
		hp += 7;
		j = 0;
		for(i = 0; i < strlen(hp); ++i){
			if(i >= 64)
				break;
			
			if(hp[i] == '\r' || hp[i] == '\n'){
				host[j] = '\0';
				break;
			}
			
			host[j++] = hp[i];
		}
	}
	
	memset(dst_url, 0, sizeof(dst_url));
	sprintf(dst_url, "%s/%s", host, dest);
}

void parse_req_queries(char *content, char *lp, int len, int *reply_size){
	int i, rn;
	
	rn = *(reply_size);
	for(i = 0; i < len; ++i){
		content[rn+i] = lp[i];
		if(lp[i] == 0){
			++i;
			break;
		}
	}
	
	if(i >= len)
		return;
	
	content[rn+i] = lp[i];
	content[rn+i+1] = lp[i+1];
	content[rn+i+2] = lp[i+2];
	content[rn+i+3] = lp[i+3];
	i += 4;
	
	*reply_size += i;
}

void handle_http_req(int sfd, char *line){
	int len;
	
	if(!strncmp(line, "GET /", 5)){
		parse_dst_url(line+5);
		
		len = strlen(dst_url);
		if((dst_url[len-4] == '.') &&
				(dst_url[len-3] == 'i') &&
				(dst_url[len-2] == 'c') &&
				(dst_url[len-1] == 'o')){
			close_socket(sfd, T_HTTP);
			
			return;
		}

		send_page(current_wan_unit, sfd, NULL, dst_url);
	}
	else
		close_socket(sfd, T_HTTP);
}

void handle_dns_req(int sfd, char *line, int maxlen, struct sockaddr *pcliaddr, int clen){
	dns_query_packet d_req;
	dns_response_packet d_reply;
	int reply_size;
	char reply_content[MAXLINE];
	void *data = &d_reply.answers;
	size_t data_len = sizeof(d_reply.answers);
	in_addr_t lan_ipaddr = inet_addr_(nvram_safe_get("lan_ipaddr"));	// router's LAN IP
#if !defined(RTCONFIG_FINDASUS)
	unsigned char auth_name_srv[] = {
		0x00, 0x00, 0x06, 0x00, 0x01, 0x00,
		0x00, 0x2a, 0x30, 0x00, 0x40, 0x01, 0x61, 0x0c,
		0x72, 0x6f, 0x6f, 0x74, 0x2d, 0x73, 0x65, 0x72,
		0x76, 0x65, 0x72, 0x73, 0x03, 0x6e, 0x65, 0x74,
		0x00, 0x05, 0x6e, 0x73, 0x74, 0x6c, 0x64, 0x0c,
		0x76, 0x65, 0x72, 0x69, 0x73, 0x69, 0x67, 0x6e,
		0x2d, 0x67, 0x72, 0x73, 0x03, 0x63, 0x6f, 0x6d,
		0x00, 0x78, 0x1b, 0x1a, 0xfd, 0x00, 0x00, 0x07,
		0x08, 0x00, 0x00, 0x03, 0x84, 0x00, 0x09, 0x3a,
		0x80, 0x00, 0x01, 0x51, 0x80
	};
#endif
	
	reply_size = 0;
	memset(reply_content, 0, MAXLINE);
	memset(&d_req, 0, sizeof(d_req));
	memcpy(&d_req.header, line, sizeof(d_req.header));
	
	// header
	memcpy(&d_reply.header, &d_req.header, sizeof(dns_header));
	d_reply.header.flag_set.flag_num = htons(0x8580);
	//d_reply.header.flag_set.flag_num = htons(0x8180);
	d_reply.header.answer_rrs = htons(0x0001);
	memcpy(reply_content, &d_reply.header, sizeof(d_reply.header));
	reply_size += sizeof(d_reply.header);
	
	reply_content[5] = 1;	// Questions
	reply_content[7] = 1;	// Answer RRS
	reply_content[9] = 0;	// Authority RRS
	reply_content[11] = 0;	// Additional RRS
	
	// queries
	parse_req_queries(reply_content, line+sizeof(dns_header), maxlen-sizeof(dns_header), &reply_size);
	
	// answers
	d_reply.answers.name = htons(0xc00c);
	d_reply.answers.type = htons(0x0001);
	d_reply.answers.ip_class = htons(0x0001);
	//d_reply.answers.ttl = htonl(0x00000001);
	d_reply.answers.ttl = htonl(0x00000000);
	d_reply.answers.data_len = htons(0x0004);
	
	char query_name[PATHLEN];
	int len, i;
	
	strncpy(query_name, line+sizeof(dns_header)+1, PATHLEN);
	len = strlen(query_name);
	for(i = 0; i < len; ++i)
		if(query_name[i] < 32)
			query_name[i] = '.';

	if ((repeater_mode() || mediabridge_mode()) &&
	    nvram_match("lan_proto", "dhcp") && nvram_get_int("lan_state_t") != LAN_STATE_CONNECTED)
		lan_ipaddr = inet_addr_(nvram_default_get("lan_ipaddr"));
	
	if(!upper_strcmp(query_name, router_name)){
		d_reply.answers.addr = lan_ipaddr;
	}
	else if (!upper_strcmp(query_name, "findasus.local")) {
#ifdef RTCONFIG_FINDASUS
		d_reply.answers.addr = lan_ipaddr;
#else
		data = auth_name_srv;
		data_len = sizeof(auth_name_srv);
		d_reply.header.flag_set.flag_num = htons(0x8183);			/* No such name */
		memcpy(reply_content, &d_reply.header, sizeof(d_reply.header));
		reply_content[5] = 1;	// Questions
		reply_content[7] = 0;	// Answer RRS
		reply_content[9] = 1;	// Authority RRS
		reply_content[11] = 0;	// Additional RRS
#endif
	}
	else
		d_reply.answers.addr = htonl(0x0a000001);	// 10.0.0.1
	
	memcpy(reply_content+reply_size, data, data_len);
	reply_size += data_len;
	
	sendto(sfd, reply_content, reply_size, 0, pcliaddr, clen);
}

void run_http_serv(int sockfd){
	ssize_t n;
	char line[MAXLINE];
	
	memset(line, 0, sizeof(line));
	
	if((n = read(sockfd, line, MAXLINE)) == 0){	// client close
		close_socket(sockfd, T_HTTP);
		
		return;
	}
	else if(n < 0){
		perror("wanduck serv http");
		return;
	}
	else{
		if(client[fd_i].type == T_HTTP)
			handle_http_req(sockfd, line);
		else
			close_socket(sockfd, T_HTTP);
	}
}

void run_dns_serv(int sockfd){
	int n;
	char line[MAXLINE];
	struct sockaddr_in cliaddr;
	int clilen = sizeof(cliaddr);
	
	memset(line, 0, MAXLINE);
	memset(&cliaddr, 0, clilen);
	
	if((n = recvfrom(sockfd, line, MAXLINE, 0, (struct sockaddr *)&cliaddr, (socklen_t *)&clilen)) == 0)	// client close
		return;
	else if(n < 0){
		perror("wanduck serv dns");
		return;
	}
	else
		handle_dns_req(sockfd, line, n, (struct sockaddr *)&cliaddr, clilen);
}

void record_wan_state_nvram(int wan_unit, int state, int sbstate, int auxstate){
	if(state != -1 && state != nvram_get_int(nvram_state[wan_unit]))
		nvram_set_int(nvram_state[wan_unit], state);

	if(sbstate != -1 && sbstate != nvram_get_int(nvram_sbstate[wan_unit]))
		nvram_set_int(nvram_sbstate[wan_unit], sbstate);

	if(auxstate != -1 && auxstate != nvram_get_int(nvram_auxstate[wan_unit]))
		nvram_set_int(nvram_auxstate[wan_unit], auxstate);
}

void record_conn_status(int wan_unit){
	char prefix_wan[8], nvram_name[16], wan_proto[16];
	char log_title[32];

	memset(log_title, 0, 32);
#ifdef RTCONFIG_DUALWAN
	if(!strcmp(dualwan_mode, "lb") || !strcmp(dualwan_mode, "fb"))
		sprintf(log_title, "WAN(%d) Connection", wan_unit);
	else
#endif
		strcpy(log_title, "WAN Connection");

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);

	memset(wan_proto, 0, 16);
	strcpy(wan_proto, nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

	if(conn_changed_state[wan_unit] == DISCONN || conn_changed_state[wan_unit] == C2D){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(disconn_case[wan_unit] == CASE_AP_DISCONN){
			if(disconn_case_old[wan_unit] == CASE_AP_DISCONN)
				return;
			disconn_case_old[wan_unit] = CASE_AP_DISCONN;

			logmessage(log_title, "Don't connect the AP yet.");
		}
		else
#endif
		if(disconn_case[wan_unit] == CASE_DISWAN){
			if(disconn_case_old[wan_unit] == CASE_DISWAN)
				return;
			disconn_case_old[wan_unit] = CASE_DISWAN;

			logmessage(log_title, "Ethernet link down.");
		}
		else if(disconn_case[wan_unit] == CASE_PPPFAIL){
			if(disconn_case_old[wan_unit] == CASE_PPPFAIL)
				return;
			disconn_case_old[wan_unit] = CASE_PPPFAIL;

			if(ppp_fail_state == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
				logmessage(log_title, "VPN authentication failed.");
			else if(ppp_fail_state == WAN_STOPPED_REASON_PPP_NO_ACTIVITY)
				logmessage(log_title, "No response from ISP.");
			else
				logmessage(log_title, "Fail to connect with some issues.");
		}
		else if(disconn_case[wan_unit] == CASE_DHCPFAIL){
			if(disconn_case_old[wan_unit] == CASE_DHCPFAIL)
				return;
			disconn_case_old[wan_unit] = CASE_DHCPFAIL;

			if(!strcmp(wan_proto, "dhcp")
#ifdef RTCONFIG_WIRELESSREPEATER
					|| sw_mode == SW_MODE_HOTSPOT
#endif
					)
				logmessage(log_title, "ISP's DHCP did not function properly.");
			else
				logmessage(log_title, "Detected that the WAN Connection Type was PPPoE. But the PPPoE Setting was not complete.");
		}
		else if(disconn_case[wan_unit] == CASE_MISROUTE){
			if(disconn_case_old[wan_unit] == CASE_MISROUTE)
				return;
			disconn_case_old[wan_unit] = CASE_MISROUTE;

			logmessage(log_title, "The router's ip was the same as gateway's ip. It led to your packages couldn't dispatch to internet correctly.");
		}
		else if(disconn_case[wan_unit] == CASE_THESAMESUBNET){
			if(disconn_case_old[wan_unit] == CASE_THESAMESUBNET)
				return;
			disconn_case_old[wan_unit] = CASE_THESAMESUBNET;

			logmessage(log_title, "The LAN's subnet may be the same with the WAN's subnet.");
		}
#ifdef RTCONFIG_USB_MODEM
#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
		else if(disconn_case[wan_unit] == CASE_DATALIMIT){
			if(disconn_case_old[wan_unit] == CASE_DATALIMIT)
				return;
			disconn_case_old[wan_unit] = CASE_DATALIMIT;

			logmessage(log_title, "The Data is at limit.");
		}
#endif
#endif
		else{	// disconn_case[wan_unit] == CASE_OTHERS
			if(disconn_case_old[wan_unit] == CASE_OTHERS)
				return;
			disconn_case_old[wan_unit] = CASE_OTHERS;

			logmessage(log_title, "WAN was exceptionally disconnected.");
		}
	}
	else if(conn_changed_state[wan_unit] == D2C){
		if(disconn_case_old[wan_unit] == 10)
			return;
		disconn_case_old[wan_unit] = 10;

		logmessage(log_title, "WAN was restored.");
	}
	else if(conn_changed_state[wan_unit] == PHY_RECONN){
		logmessage(log_title, "Ethernet link up.");
	}
}

int get_disconn_count(int wan_unit){
	return changed_count[wan_unit];
}

void set_disconn_count(int wan_unit, int flag){
	changed_count[wan_unit] = flag;
}

int get_next_unit(int wan_unit){
	int next = (wan_unit+1)%WAN_UNIT_MAX;

	return next;
}

int get_last_unit(int wan_unit){
	int last = wan_unit-1;

	if(last < WAN_UNIT_FIRST)
		last = WAN_UNIT_MAX-1;

	return last;
}

int switch_wan_line(const int wan_unit, const int restart_other){
#ifdef RTCONFIG_USB_MODEM
	int retry, lock;
#endif
	char cmd[32];
	int unit;
	char prefix[] = "wanXXXXXX_", tmp[100] = "";

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
		if(unit == wan_unit)
			break;
	if(unit == WAN_UNIT_MAX)
		return 0;

	if(wan_primary_ifunit() == wan_unit) // Already have no running modem.
		return 0;
#ifdef RTCONFIG_USB_MODEM
	else if (dualwan_unit__usbif(wan_unit)) {
		if(!link_wan[wan_unit]) {
			snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);
			nvram_set_int(strcat_r(prefix, "is_usb_modem_ready", tmp), link_wan[wan_unit]);
			return 0; // No modem in USB ports.
		}
	}
#endif

	csprintf("%s: wan(%d) Starting...\n", __FUNCTION__, wan_unit);
	// Set the modem to be running.
	set_wan_primary_ifunit(wan_unit);

#ifdef RTCONFIG_USB_MODEM
	if (nvram_invmatch("modem_enable", "4") && dualwan_unit__usbif(wan_unit)) {
		// Wait the PPP config file to be done.
		retry = 0;
		while((lock = file_lock("3g")) == -1 && retry < MAX_WAIT_FILE)
			sleep(1);

		if(lock == -1){
			csprintf("(%d): No pppd conf file and turn off the state of USB Modem.\n", wan_unit);
			set_wan_primary_ifunit(get_last_unit(wan_unit));
			return 0;
		}
		else
			file_unlock(lock);
	}
#endif

	// TODO: don't know if it's necessary?
	// clean or restart the other line.
	if(restart_other)
	{
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
			if(unit == wan_unit)
				continue;

			csprintf("wanduck1: restart_wan_if %d.\n", unit);
			snprintf(cmd, 32, "restart_wan_if %d", unit);
			notify_rc_and_wait(cmd);
			sleep(1);
		}
	}
#ifdef RTCONFIG_USB_MODEM
	else{
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
			if(unit == wan_unit)
				continue;

			if(dualwan_unit__nonusbif(unit))
				continue;

			csprintf("wanduck1: stop_wan_if %d.\n", unit);
			snprintf(cmd, 32, "stop_wan_if %d", unit);
			notify_rc_and_wait(cmd);
			sleep(1);
		}
	}
#endif

	// restart the primary line.
	if(get_wan_state(wan_unit) == WAN_STATE_CONNECTED)
		snprintf(cmd, 32, "restart_wan_line %d", wan_unit);
	else
		snprintf(cmd, 32, "restart_wan_if %d", wan_unit);
	csprintf("wanduck2: %s.\n", cmd);
	notify_rc_and_wait(cmd);

#ifdef RTCONFIG_DUALWAN
	if(sw_mode == SW_MODE_ROUTER
			&& (!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb"))
			){
		delay_detect = 1;
	}
#endif

	csprintf("%s: wan(%d) End.\n", __FUNCTION__, wan_unit);
	return 1;
}

int wanduck_main(int argc, char *argv[]){
	char *http_servport, *dns_servport;
	int clilen;
	struct sockaddr_in cliaddr;
	struct timeval  tval;
	int nready, maxi, sockfd;
	int wan_unit;
	char prefix_wan[8];
	char cmd[32];
	char tmp[100]="";
#ifdef RTCONFIG_WIRELESSREPEATER
	char domain_mapping[64];
#endif
#ifdef RTCONFIG_DSL
	int usb_switched_back_dsl = 0;
#endif
	unsigned int now;
#ifdef RTCONFIG_DUALWAN
#if defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)
	int wanred_led_status = 0;	/* 1 is no internet, 2 is internet ok */
	int u, link_status;
#endif
#endif
#ifdef RTCONFIG_QTN
	time_t qtn_now;
	struct tm *qtn_tm;
	char *time_string[14] = {0};
#endif

	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, safe_leave);
	signal(SIGCHLD, chld_reap);
	signal(SIGUSR1, get_network_nvram);
	signal(SIGUSR2, wan_led_control);

	if(argc < 3){
		http_servport = DFL_HTTP_SERV_PORT;
		dns_servport = DFL_DNS_SERV_PORT;
	}
	else{
		if(atoi(argv[1]) <= 0)
			http_servport = DFL_HTTP_SERV_PORT;
		else
			http_servport = (char *)argv[1];

		if(atoi(argv[2]) <= 0)
			dns_servport = DFL_DNS_SERV_PORT;
		else
			dns_servport = (char *)argv[2];
	}

	wdbg = nvram_get_int("wdbg");

	if(build_socket(http_servport, dns_servport, &http_sock, &dns_sock) < 0){
		csprintf("\n*** Fail to build socket! ***\n");
		exit(0);
	}
	if(fcntl(dns_sock, F_SETFL, fcntl(dns_sock, F_GETFL, 0) | O_NONBLOCK) < 0){
		_dprintf("wanduck set dnssock [%d] nonblock fail !\n", dns_sock);
		exit(0);
	}

	test_log = nvram_get_int("wanduck_debug");

	FILE *fp = fopen(WANDUCK_PID_FILE, "w");

	if(fp != NULL){
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	maxfd = (http_sock > dns_sock)?http_sock:dns_sock;
	maxi = -1;

	FD_ZERO(&allset);
	FD_SET(http_sock, &allset);
	FD_SET(dns_sock, &allset);

	for(fd_i = 0; fd_i < MAX_USER; ++fd_i){
		client[fd_i].sfd = -1;
		client[fd_i].type = 0;
	}

	rule_setup = 0;
	got_notify = 0;
	modem_act_reset = 0;
	clilen = sizeof(cliaddr);

	sprintf(router_name, "%s", DUT_DOMAIN_NAME);

#ifndef NO_DETECT_INTERNET
	nvram_set_int("link_internet", 0);
#endif
#ifdef RTCONFIG_WIRELESSREPEATER
	nvram_set_int("link_ap", 2);
#endif

	for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
		if(wan_unit != WAN_UNIT_FIRST)
			snprintf(tmp, 100, "link_wan%d", wan_unit);
		else
			snprintf(tmp, 100, "link_wan");
		nvram_set_int(tmp, 0);

		link_setup[wan_unit] = 0;
		link_wan[wan_unit] = 0;

		changed_count[wan_unit] = S_IDLE;
		disconn_case[wan_unit] = CASE_NONE;

		memset(prefix_wan, 0, 8);
		sprintf(prefix_wan, "wan%d_", wan_unit);

		strcat_r(prefix_wan, "state_t", nvram_state[wan_unit]);
		strcat_r(prefix_wan, "sbstate_t", nvram_sbstate[wan_unit]);
		strcat_r(prefix_wan, "auxstate_t", nvram_auxstate[wan_unit]);

		set_disconn_count(wan_unit, S_IDLE);
#ifdef RTCONFIG_USB_MODEM
		nvram_set_int(strcat_r(prefix_wan, "is_usb_modem_ready", tmp), link_wan[wan_unit]);
#endif
	}

#ifdef RTCONFIG_USB_MODEM
	memset(modem_type, 0, 32);
#ifdef RT4GAC55U
	memset(usb_if, 0, 32);
#endif
#endif

	loop_sec = uptime();

	get_related_nvram(); // initial the System's variables.
	get_lan_nvram(); // initial the LAN's variables.

	nat_redirect_enable_old = nat_redirect_enable;

#ifdef RTCONFIG_DUALWAN
#if 1
	WAN_FB_UNIT = WAN_UNIT_FIRST;
#else
	WAN_FB_UNIT = WAN_UNIT_SECOND;
#endif

#ifdef WEB_REDIRECT
	if(nvram_get_int("freeze_duck"))
		_dprintf("\n<*>freeze the duck, %ds left!\n", nvram_get_int("freeze_duck"));	// don't check conn state during inner events period
	else
#endif
	if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "lb")){
		cross_state = DISCONN;
		for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
			conn_state[wan_unit] = if_wan_phyconnected(wan_unit);
			if(conn_state[wan_unit] == CONNED){
#ifdef RTCONFIG_USB_MODEM
				current_state[wan_unit] = nvram_get_int(nvram_state[wan_unit]);
				if(!(dualwan_unit__usbif(wan_unit) && current_state[wan_unit] == WAN_STATE_INITIALIZING))
#endif
					conn_state[wan_unit] = if_wan_connected(wan_unit, nvram_get_int(nvram_state[wan_unit]));
			}
			else
				conn_state[wan_unit] = DISCONN;

			conn_changed_state[wan_unit] = conn_state[wan_unit];

			if(conn_state[wan_unit] == CONNED && cross_state != CONNED)
				cross_state = CONNED;

			conn_state_old[wan_unit] = conn_state[wan_unit];

			record_conn_status(wan_unit);

			if(cross_state == CONNED)
				set_disconn_count(wan_unit, S_IDLE);
			else
				set_disconn_count(wan_unit, S_COUNT);
		}
	}
	else if(sw_mode == SW_MODE_ROUTER
			&& (!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb"))
			){
		if(wandog_delay > 0){
			csprintf("wanduck: delay %d seconds...\n", wandog_delay);
			sleep(wandog_delay);
			delay_detect = 0;
		}

		// To check the phy connection of the standby line. 
		for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
			if(get_dualwan_by_unit(wan_unit) != WANS_DUALWAN_IF_NONE)
				conn_state[wan_unit] = if_wan_phyconnected(wan_unit);
		}

		current_wan_unit = wan_primary_ifunit();
		other_wan_unit = get_next_unit(current_wan_unit);
if(test_log)
_dprintf("wanduck(%d)(first detect start): state %d, state_old %d, changed %d, wan_state %d.\n"
		, current_wan_unit, conn_state[current_wan_unit], conn_state_old[current_wan_unit], conn_changed_state[current_wan_unit], current_state[current_wan_unit]);

		if(conn_state[current_wan_unit] == CONNED){
#ifdef RTCONFIG_USB_MODEM
			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);
			if(!(dualwan_unit__usbif(current_wan_unit) && current_state[current_wan_unit] == WAN_STATE_INITIALIZING))
#endif
				conn_state[current_wan_unit] = if_wan_connected(current_wan_unit, nvram_get_int(nvram_state[current_wan_unit]));

			cross_state = conn_state[current_wan_unit];
		}
		else
			cross_state = DISCONN;

		conn_changed_state[current_wan_unit] = conn_state[current_wan_unit];

		conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

		record_conn_status(current_wan_unit);
	}
	else
#endif // RTCONFIG_DUALWAN
	if(sw_mode == SW_MODE_ROUTER
#ifdef RTCONFIG_WIRELESSREPEATER
			|| sw_mode == SW_MODE_HOTSPOT
#endif
			){
		current_wan_unit = wan_primary_ifunit();
		other_wan_unit = get_next_unit(current_wan_unit);

		conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit);
		if(conn_state[current_wan_unit] == CONNED){
#ifdef RTCONFIG_USB_MODEM
			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);
			if(!(dualwan_unit__usbif(current_wan_unit) && current_state[current_wan_unit] == WAN_STATE_INITIALIZING))
#endif
				conn_state[current_wan_unit] = if_wan_connected(current_wan_unit, nvram_get_int(nvram_state[current_wan_unit]));

			cross_state = conn_state[current_wan_unit];
		}
		else
			cross_state = DISCONN;

		conn_changed_state[current_wan_unit] = conn_state[current_wan_unit];

		conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

		record_conn_status(current_wan_unit);
	}
	else{ // sw_mode == SW_MODE_AP, SW_MODE_REPEATER.
		current_wan_unit = WAN_UNIT_FIRST;

		conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit);

		cross_state = conn_state[current_wan_unit];

		conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER)
			record_conn_status(current_wan_unit);
#endif

		if(sw_mode == SW_MODE_AP){
#ifdef RTCONFIG_REDIRECT_DNAME
			if(cross_state == DISCONN){
				csprintf("\n# AP mode: Enable direct rule(DISCONN)\n");
				eval("ebtables", "-t", "broute", "-F");
				eval("ebtables", "-t", "filter", "-F");
				redirect_setting();
				eval("iptables-restore", "/tmp/redirect_rules");
			}
			else if(cross_state == CONNED){
				csprintf("\n# AP mode: Disable direct rule(CONNED)\n");
				eval("ebtables", "-t", "broute", "-F");
				eval("ebtables", "-t", "filter", "-F");
				eval("ebtables", "-t", "broute", "-I", "BROUTING", "-p", "ipv4", "--ip-proto", "udp", "--ip-dport", "53", "-j", "redirect", "--redirect-target", "DROP");
				redirect_nat_setting();
				eval("iptables-restore", NAT_RULES);
			}

#endif

#ifdef RTCONFIG_RESTRICT_GUI
			if(cross_state == CONNED){
				if(nvram_get_int("fw_restrict_gui")){
					char word[PATH_MAX], *next_word;

					foreach(word, nvram_safe_get("lan_ifnames"), next_word){
						if(!strncmp(word, "vlan", 4))
							continue;

						eval("ebtables", "-t", "broute", "-I", "BROUTING", "-i", word, "-j", "mark", "--mark-set", BIT_RES_GUI, "--mark-target", "CONTINUE");
					}

					repeater_filter_setting(0);
				}
			}
#endif
		}
	}

	/*
	 * Because start_wanduck() is run early than start_wan()
	 * and the redirect rules is already set before running wanduck,
	 * handle_wan_line() must not be run at the first detect.
	 */
#ifdef WEB_REDIRECT
	if(nvram_get_int("freeze_duck"))
		_dprintf("\n<**>freeze the duck, %ds left!\n", nvram_get_int("freeze_duck"));	// don't check conn state during inner events period
	else
#endif
	if(cross_state == DISCONN){
		csprintf("\n# Enable direct rule\n");
		rule_setup = 1;
	}
	else if(cross_state == CONNED && isFirstUse){
		csprintf("\n#CONNED : Enable direct rule\n");
		rule_setup = 1;
	}
if(test_log)
_dprintf("wanduck(%d)(first detect   end): state %d, state_old %d, changed %d, wan_state %d.\n"
		, current_wan_unit, conn_state[current_wan_unit], conn_state_old[current_wan_unit], conn_changed_state[current_wan_unit], current_state[current_wan_unit]);

	int first_loop = 1;
	unsigned int diff;
	for(;;){
		if(!first_loop){
			now = uptime();
			diff = now-loop_sec;

			if(diff < scan_interval){
				rset = allset;
				tval.tv_sec = scan_interval-diff;
				tval.tv_usec = 0;

				goto WANDUCK_SELECT;
			}

			loop_sec = now;
		}
		else
			first_loop = 0;

		rset = allset;
		tval.tv_sec = scan_interval;
		tval.tv_usec = 0;

		get_related_nvram();

		// Sam 2014/10/14
		// rule_setup depend on wan status,
		// if nat_redirect_enable changed, rebuild nat rule.
		if(rule_setup) {
			if(nat_redirect_enable_old == 0 && nat_redirect_enable == 1)	//need redirect
				stop_nat_rules();
			else if(nat_redirect_enable_old == 1 && nat_redirect_enable == 0)	//don't redirect
				start_nat_rules();
		}

		if(nat_redirect_enable_old != nat_redirect_enable)
			nat_redirect_enable_old = nat_redirect_enable;
		//_dprintf("rule_setup: %d, nat_state: %s\n", rule_setup, nvram_safe_get("nat_state"));

#ifdef WEB_REDIRECT
		if(nvram_get_int("freeze_duck")){
			_dprintf("\n<****>freeze the duck, %ds left!\n", nvram_get_int("freeze_duck"));	// don't check conn state during inner events period
			goto WANDUCK_SELECT;
		}
		else
#endif
#ifdef RTCONFIG_DUALWAN
		if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "lb")){
			cross_state = DISCONN;
			for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
#if 0
#ifdef RTCONFIG_USB_MODEM
				if(dualwan_unit__usbif(wan_unit) && !link_wan[wan_unit]){
					if_wan_phyconnected(wan_unit);
					continue;
				}
#endif
#endif

				current_state[wan_unit] = nvram_get_int(nvram_state[wan_unit]);

				if(current_state[wan_unit] == WAN_STATE_DISABLED){
					//record_wan_state_nvram(wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL, -1);

					disconn_case[wan_unit] = CASE_OTHERS;
					conn_state[wan_unit] = DISCONN;
				}
#ifdef RTCONFIG_USB_MODEM
				else if(dualwan_unit__usbif(wan_unit)
						&& (modem_act_reset == 1 || modem_act_reset == 2)
						){
_dprintf("wanduck(%d): detect the modem to be reset...\n", wan_unit);
					disconn_case[wan_unit] = CASE_OTHERS;
					conn_state[wan_unit] = DISCONN;
					set_disconn_count(wan_unit, S_IDLE);
				}
#endif
				else{
					conn_state[wan_unit] = if_wan_phyconnected(wan_unit);
					if(conn_state[wan_unit] == CONNED){
#ifdef RTCONFIG_USB_MODEM
						if(!(dualwan_unit__usbif(wan_unit) && current_state[wan_unit] == WAN_STATE_INITIALIZING))
#endif
							conn_state[wan_unit] = if_wan_connected(wan_unit, current_state[wan_unit]);
					}
				}

				if(conn_state[wan_unit] == CONNED && cross_state != CONNED)
					cross_state = CONNED;

#ifdef RTCONFIG_USB_MODEM
				if(dualwan_unit__usbif(wan_unit)){
					if(link_wan[wan_unit] == 1 && current_state[wan_unit] == WAN_STATE_INITIALIZING && boot_end == 1){
						csprintf("wanduck: start_wan_if %d.\n", wan_unit);
						snprintf(cmd, 32, "start_wan_if %d", wan_unit);
						notify_rc(cmd);
						continue;
					}
					else if(!link_wan[wan_unit] && current_state[wan_unit] != WAN_STATE_INITIALIZING){
						csprintf("wanduck2: stop_wan_if %d.\n", wan_unit);
						snprintf(cmd, 32, "stop_wan_if %d", wan_unit);
						notify_rc(cmd);
						continue;
					}
				}

				if(conn_state[wan_unit] == SET_PIN){
					conn_changed_state[wan_unit] = SET_PIN;
					set_disconn_count(wan_unit, S_IDLE);
				}
				else if(conn_state[wan_unit] == SET_USBSCAN){
					conn_changed_state[wan_unit] = SET_USBSCAN;
					set_disconn_count(wan_unit, S_IDLE);
				}
				else
#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
				if(dualwan_unit__usbif(wan_unit) && nvram_get_int(nvram_sbstate[wan_unit]) == WAN_STOPPED_REASON_DATALIMIT){
					csprintf("wanduck(%d)(lb): detect the data limit.\n", wan_unit);
					conn_changed_state[wan_unit] = C2D;
					set_disconn_count(wan_unit, max_disconn_count[wan_unit]);
				}
				else
#endif
#endif
				if(conn_state[wan_unit] != conn_state_old[wan_unit]){
					if(conn_state[wan_unit] == PHY_RECONN){
						conn_changed_state[wan_unit] = PHY_RECONN;
					}
					else if(conn_state[wan_unit] == DISCONN){
						conn_changed_state[wan_unit] = C2D;

#ifdef RTCONFIG_USB_MODEM
						if (dualwan_unit__usbif(wan_unit))
							set_disconn_count(wan_unit, max_disconn_count[wan_unit]);
						else
#endif
							set_disconn_count(wan_unit, S_COUNT);
					}
					else if(conn_state[wan_unit] == CONNED){
						conn_changed_state[wan_unit] = D2C;
						set_disconn_count(wan_unit, S_IDLE);
					}
					else
						conn_changed_state[wan_unit] = CONNED;

					conn_state_old[wan_unit] = conn_state[wan_unit];

					record_conn_status(wan_unit);
				}

				if(get_disconn_count(wan_unit) != S_IDLE){
					if(conn_state[wan_unit] == PHY_RECONN)
						set_disconn_count(wan_unit, max_disconn_count[wan_unit]);

					if(get_disconn_count(wan_unit) >= max_disconn_count[wan_unit]){
						set_disconn_count(wan_unit, S_IDLE);

#ifdef RTCONFIG_USB_MODEM
						csprintf("\n# wanduck(usb): type=%s.\n", modem_type);
						if(dualwan_unit__usbif(wan_unit) &&
								(!strcmp(modem_type, "ecm") || !strcmp(modem_type, "ncm") || !strcmp(modem_type, "rndis") || !strcmp(modem_type, "asix") || !strcmp(modem_type, "qmi") || !strcmp(modem_type, "gobi"))
								){
							csprintf("\n# wanduck(usb): skip to run restart_wan_if %d.\n", wan_unit);
							if(!link_wan[wan_unit] && strlen(modem_type) > 0)
								memset(modem_type, 0, 32);		
						}
						else
#endif
						{
							csprintf("\n# wanduck(%d): run restart_wan_if.\n", wan_unit);
							memset(cmd, 0, 32);
							sprintf(cmd, "restart_wan_if %d", wan_unit);
							notify_rc_and_period_wait(cmd, 30);
						}

						if(get_wan_state(get_next_unit(wan_unit)) == WAN_STATE_CONNECTED){
							memset(cmd, 0, 32);
							sprintf(cmd, "restart_wan_line %d", get_next_unit(wan_unit));
							notify_rc(cmd);
						}
					}
					else
						set_disconn_count(wan_unit, get_disconn_count(wan_unit)+1);

					csprintf("%s: wan(%d) disconn count = %d/%d ...\n", __FUNCTION__, wan_unit, get_disconn_count(wan_unit), max_disconn_count[wan_unit]);
				}
			}
if(test_log)
_dprintf("wanduck(%d)(lb change): state %d, state_old %d, changed %d, cross_state=%d, wan_state %d.\n"
		, current_wan_unit, conn_state[current_wan_unit], conn_state_old[current_wan_unit], conn_changed_state[current_wan_unit], cross_state, current_state[current_wan_unit]);
		}
		else if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "fo")){
			if(delay_detect == 1 && wandog_delay > 0){
				csprintf("wanduck: delay %d seconds...\n", wandog_delay);
				sleep(wandog_delay);
				delay_detect = 0;
			}

			// To check the phy connection of the standby line. 
			for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
				if(get_dualwan_by_unit(wan_unit) != WANS_DUALWAN_IF_NONE)
					conn_state[wan_unit] = if_wan_phyconnected(wan_unit);
			}

			current_wan_unit = wan_primary_ifunit();
			other_wan_unit = get_next_unit(current_wan_unit);

			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);
if(test_log)
_dprintf("wanduck(%d)(fo    phy): state %d, state_old %d, changed %d, wan_state %d.\n"
		, current_wan_unit, conn_state[current_wan_unit], conn_state_old[current_wan_unit], conn_changed_state[current_wan_unit], current_state[current_wan_unit]);

			if(current_state[current_wan_unit] == WAN_STATE_DISABLED){
				//record_wan_state_nvram(current_wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL, -1);

				disconn_case[current_wan_unit] = CASE_OTHERS;
				conn_state[current_wan_unit] = DISCONN;
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#ifdef RTCONFIG_USB_MODEM
			else if(dualwan_unit__usbif(current_wan_unit)
					&& (modem_act_reset == 1 || modem_act_reset == 2)
					){
_dprintf("wanduck(%d): detect the modem to be reset...\n", current_wan_unit);
				disconn_case[current_wan_unit] = CASE_OTHERS;
				conn_state[current_wan_unit] = DISCONN;
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#endif
			else{
				if(conn_state[current_wan_unit] == CONNED){
#ifdef RTCONFIG_USB_MODEM
					if(!(dualwan_unit__usbif(current_wan_unit) && current_state[current_wan_unit] == WAN_STATE_INITIALIZING))
#endif
						conn_state[current_wan_unit] = if_wan_connected(current_wan_unit, current_state[current_wan_unit]);
if(test_log)
_dprintf("wanduck(%d)(fo   conn): state %d, state_old %d, changed %d, wan_state %d.\n"
		, current_wan_unit, conn_state[current_wan_unit], conn_state_old[current_wan_unit], conn_changed_state[current_wan_unit], current_state[current_wan_unit]);
				}
			}

			if(conn_state[current_wan_unit] == PHY_RECONN){
				conn_changed_state[current_wan_unit] = PHY_RECONN;

				conn_state_old[current_wan_unit] = DISCONN;

				// When the current line is re-plugged and the other line has plugged, the count has to be reset.
				if(link_wan[other_wan_unit]){
					csprintf("# wanduck: set S_COUNT: PHY_RECONN.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}
#ifdef RTCONFIG_USB_MODEM
			else if(conn_state[current_wan_unit] == SET_PIN){
				conn_changed_state[current_wan_unit] = SET_PIN;

				conn_state_old[current_wan_unit] = DISCONN;
				// The USB modem needs the PIN code to unlock.
				set_disconn_count(current_wan_unit, S_IDLE);
			}
			else if(conn_state[current_wan_unit] == SET_USBSCAN){
				conn_changed_state[current_wan_unit] = C2D;

				conn_state_old[current_wan_unit] = DISCONN;
				// The USB modem is scanning the stations.
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
			else if(dualwan_unit__usbif(current_wan_unit) && nvram_get_int(nvram_sbstate[current_wan_unit]) == WAN_STOPPED_REASON_DATALIMIT){
				if(conn_state_old[current_wan_unit] == CONNED){
					csprintf("wanduck(%d)(fo): detect the data limit.\n", current_wan_unit);
					conn_changed_state[current_wan_unit] = C2D;
				}
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = DISCONN;
				//set_disconn_count(current_wan_unit, max_disconn_count[current_wan_unit]);
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#endif
#endif
			else if(conn_state[current_wan_unit] == CONNED){
				if(conn_state_old[current_wan_unit] == DISCONN)
					conn_changed_state[current_wan_unit] = D2C;
				else
					conn_changed_state[current_wan_unit] = CONNED;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];
				set_disconn_count(current_wan_unit, S_IDLE);
			}
			else if(conn_state[current_wan_unit] == DISCONN){
				if(conn_state_old[current_wan_unit] == CONNED)
					conn_changed_state[current_wan_unit] = C2D;
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

				if(disconn_case[current_wan_unit] == CASE_THESAMESUBNET){
					csprintf("# wanduck: set S_IDLE: CASE_THESAMESUBNET.\n");
					set_disconn_count(current_wan_unit, S_IDLE);
				}
#ifdef RTCONFIG_USB_MODEM
				// when the other line is modem and not plugged, the current disconnected line would not count.
				else if(!link_wan[other_wan_unit] && dualwan_unit__usbif(other_wan_unit))
					set_disconn_count(current_wan_unit, S_IDLE);
#endif
				else if(get_disconn_count(current_wan_unit) == S_IDLE && current_state[current_wan_unit] != WAN_STATE_DISABLED
						&& get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE
						)
					set_disconn_count(current_wan_unit, S_COUNT);
			}

			if(get_disconn_count(current_wan_unit) != S_IDLE){
				if(get_disconn_count(current_wan_unit) < max_disconn_count[current_wan_unit]){
					set_disconn_count(current_wan_unit, get_disconn_count(current_wan_unit)+1);
					csprintf("# wanduck(%d): wait time for switching: %d/%d.\n", current_wan_unit, get_disconn_count(current_wan_unit)*scan_interval, max_wait_time[current_wan_unit]);
				}
				else{
					csprintf("# wanduck(%d): set S_COUNT: changed_count[] >= max_disconn_count.\n", current_wan_unit);
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}

			record_conn_status(current_wan_unit);
if(test_log)
_dprintf("wanduck(%d)(fo change): state %d, state_old %d, changed %d, wan_state %d.\n"
		, current_wan_unit, conn_state[current_wan_unit], conn_state_old[current_wan_unit], conn_changed_state[current_wan_unit], current_state[current_wan_unit]);
		}
		else if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "fb")){
			if(delay_detect == 1 && wandog_delay > 0){
				csprintf("wanduck: delay %d seconds...\n", wandog_delay);
				sleep(wandog_delay);
				delay_detect = 0;
			}

			// To check the phy connection of the standby line. 
			for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
				if(get_dualwan_by_unit(wan_unit) != WANS_DUALWAN_IF_NONE)
					conn_state[wan_unit] = if_wan_phyconnected(wan_unit);
			}

			current_wan_unit = wan_primary_ifunit();
			other_wan_unit = get_next_unit(current_wan_unit);

			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);

			if(current_state[current_wan_unit] == WAN_STATE_DISABLED){
				//record_wan_state_nvram(current_wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL, -1);

				disconn_case[current_wan_unit] = CASE_OTHERS;
				conn_state[current_wan_unit] = DISCONN;
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#ifdef RTCONFIG_USB_MODEM
			else if(dualwan_unit__usbif(current_wan_unit)
					&& (modem_act_reset == 1 || modem_act_reset == 2)
					){
_dprintf("wanduck(%d): detect the modem to be reset...\n", current_wan_unit);
				disconn_case[current_wan_unit] = CASE_OTHERS;
				conn_state[current_wan_unit] = DISCONN;
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#endif
			else{
				if(conn_state[current_wan_unit] == CONNED){
#ifdef RTCONFIG_USB_MODEM
					if(!(dualwan_unit__usbif(current_wan_unit) && current_state[current_wan_unit] == WAN_STATE_INITIALIZING))
#endif
						conn_state[current_wan_unit] = if_wan_connected(current_wan_unit, current_state[current_wan_unit]);
				}

				if(other_wan_unit == WAN_FB_UNIT && conn_state[other_wan_unit] == CONNED){
					current_state[other_wan_unit] = nvram_get_int(nvram_state[other_wan_unit]);
#ifdef RTCONFIG_USB_MODEM
					if(!(dualwan_unit__usbif(other_wan_unit) && current_state[other_wan_unit] == WAN_STATE_INITIALIZING))
#endif
						conn_state[other_wan_unit] = if_wan_connected(other_wan_unit, current_state[other_wan_unit]);
					csprintf("wanduck: detect the fail-back line(%d)...\n", other_wan_unit);
if(test_log)
_dprintf("wanduck(%d) fail-back: state %d, state_old %d, changed %d, wan_state %d.\n"
		, other_wan_unit, conn_state[other_wan_unit], conn_state_old[other_wan_unit], conn_changed_state[other_wan_unit], current_state[other_wan_unit]);
				}
			}

			if(conn_state[current_wan_unit] == PHY_RECONN){
				conn_changed_state[current_wan_unit] = PHY_RECONN;

				conn_state_old[current_wan_unit] = DISCONN;

				// When the current line is re-plugged and the other line has plugged, the count has to be reset.
				if(link_wan[other_wan_unit]){
					csprintf("# wanduck: set S_COUNT: PHY_RECONN.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}
#ifdef RTCONFIG_USB_MODEM
			else if(conn_state[current_wan_unit] == SET_PIN){
				conn_changed_state[current_wan_unit] = SET_PIN;

				conn_state_old[current_wan_unit] = DISCONN;
				// The USB modem needs the PIN code to unlock.
				set_disconn_count(current_wan_unit, S_IDLE);
			}
			else if(conn_state[current_wan_unit] == SET_USBSCAN){
				conn_changed_state[current_wan_unit] = C2D;

				conn_state_old[current_wan_unit] = DISCONN;
				// The USB modem is scanning the stations.
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
			else if(dualwan_unit__usbif(current_wan_unit) && nvram_get_int(nvram_sbstate[current_wan_unit]) == WAN_STOPPED_REASON_DATALIMIT){
				if(conn_state_old[current_wan_unit] == CONNED){
					csprintf("wanduck(%d)(fb): detect the data limit.\n", current_wan_unit);
					conn_changed_state[current_wan_unit] = C2D;
				}
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = DISCONN;
				set_disconn_count(current_wan_unit, max_disconn_count[current_wan_unit]);
			}
#endif
#endif
			else if(conn_state[current_wan_unit] == CONNED){
				if(conn_state_old[current_wan_unit] == DISCONN)
					conn_changed_state[current_wan_unit] = D2C;
				else
					conn_changed_state[current_wan_unit] = CONNED;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];
				set_disconn_count(current_wan_unit, S_IDLE);
			}
			else if(conn_state[current_wan_unit] == DISCONN){
				if(conn_state_old[current_wan_unit] == CONNED)
					conn_changed_state[current_wan_unit] = C2D;
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

				if(disconn_case[current_wan_unit] == CASE_THESAMESUBNET){
					csprintf("# wanduck: set S_IDLE: CASE_THESAMESUBNET.\n");
					set_disconn_count(current_wan_unit, S_IDLE);
				}
#ifdef RTCONFIG_USB_MODEM
				// when the other line is modem and not plugged, the current disconnected line would not count.
				else if(!link_wan[other_wan_unit] && dualwan_unit__usbif(other_wan_unit))
					set_disconn_count(current_wan_unit, S_IDLE);
#endif
				else if(get_disconn_count(current_wan_unit) == S_IDLE && current_state[current_wan_unit] != WAN_STATE_DISABLED
						&& get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE
						)
					set_disconn_count(current_wan_unit, S_COUNT);
			}

			if(other_wan_unit == WAN_FB_UNIT){
				if(conn_state[other_wan_unit] == CONNED
						&& get_disconn_count(other_wan_unit) == S_IDLE
						)
					set_disconn_count(other_wan_unit, S_COUNT);
				else if(conn_state[other_wan_unit] == DISCONN)
					set_disconn_count(other_wan_unit, S_IDLE);
			}
			else
				set_disconn_count(other_wan_unit, S_IDLE);

			if(get_disconn_count(current_wan_unit) != S_IDLE){
				if(get_disconn_count(current_wan_unit) < max_disconn_count[current_wan_unit]){
					set_disconn_count(current_wan_unit, get_disconn_count(current_wan_unit)+1);
					csprintf("# wanduck(%d): wait time for switching: %d/%d.\n", current_wan_unit, get_disconn_count(current_wan_unit)*scan_interval, max_wait_time[current_wan_unit]);
				}
				else{
					csprintf("# wanduck(%d): set S_COUNT: changed_count[] >= max_disconn_count.\n", current_wan_unit);
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}

			if(get_disconn_count(other_wan_unit) != S_IDLE){
				if(get_disconn_count(other_wan_unit) < max_fb_count){
					set_disconn_count(other_wan_unit, get_disconn_count(other_wan_unit)+1);
					csprintf("# wanduck: wait time for returning: %d/%d.\n", get_disconn_count(other_wan_unit)*scan_interval, max_fb_wait_time);
				}
				else{
					csprintf("# wanduck: set S_COUNT: changed_count[] >= max_fb_count.\n");
					set_disconn_count(other_wan_unit, S_COUNT);
				}
			}

			record_conn_status(current_wan_unit);
		}
		else
#endif // RTCONFIG_DUALWAN
		if(sw_mode == SW_MODE_ROUTER
#ifdef RTCONFIG_WIRELESSREPEATER
				|| sw_mode == SW_MODE_HOTSPOT
#endif
				){
			// To check the phy connection of the standby line. 
			for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit)
				conn_state[wan_unit] = if_wan_phyconnected(wan_unit);

			current_wan_unit = wan_primary_ifunit();
			other_wan_unit = get_next_unit(current_wan_unit);

			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);

			if(current_state[current_wan_unit] == WAN_STATE_DISABLED){
				//record_wan_state_nvram(current_wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL, -1);
				update_wan_leds(current_wan_unit);

				disconn_case[current_wan_unit] = CASE_OTHERS;
				conn_state[current_wan_unit] = DISCONN;
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#ifdef RTCONFIG_USB_MODEM
			else if(dualwan_unit__usbif(current_wan_unit)
					&& (modem_act_reset == 1 || modem_act_reset == 2)
					){
_dprintf("wanduck(%d): detect the modem to be reset...\n", current_wan_unit);
				disconn_case[current_wan_unit] = CASE_OTHERS;
				conn_state[current_wan_unit] = DISCONN;
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#endif
			else{
#if !defined(RTN14U) && !defined(RTAC1200GP)			  
				conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit);
#endif				
				if(conn_state[current_wan_unit] == CONNED){
#ifdef RTCONFIG_USB_MODEM
					if(!(dualwan_unit__usbif(current_wan_unit) && current_state[current_wan_unit] == WAN_STATE_INITIALIZING))
#endif
						conn_state[current_wan_unit] = if_wan_connected(current_wan_unit, current_state[current_wan_unit]);
				}
			}

			if(conn_state[current_wan_unit] == PHY_RECONN){
				conn_changed_state[current_wan_unit] = PHY_RECONN;

				conn_state_old[current_wan_unit] = DISCONN;

				// When the current line is re-plugged and the other line has plugged, the count has to be reset.
				if(link_wan[other_wan_unit]){
					csprintf("# wanduck: set S_COUNT: PHY_RECONN.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}
#ifdef RTCONFIG_USB_MODEM
			else if(conn_state[current_wan_unit] == SET_PIN){
				conn_changed_state[current_wan_unit] = SET_PIN;

				conn_state_old[current_wan_unit] = DISCONN;
				// The USB modem needs the PIN code to unlock.
				set_disconn_count(current_wan_unit, S_IDLE);
			}
			else if(conn_state[current_wan_unit] == SET_USBSCAN){
				conn_changed_state[current_wan_unit] = C2D;

				conn_state_old[current_wan_unit] = DISCONN;
				// The USB modem is scanning the stations.
				set_disconn_count(current_wan_unit, S_IDLE);
			}
#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
			else if(dualwan_unit__usbif(current_wan_unit) && nvram_get_int(nvram_sbstate[current_wan_unit]) == WAN_STOPPED_REASON_DATALIMIT){
				if(conn_state_old[current_wan_unit] == CONNED){
					csprintf("wanduck(usb): detect the data limit.\n");
					conn_changed_state[current_wan_unit] = C2D;
				}
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = DISCONN;
				set_disconn_count(current_wan_unit, max_disconn_count[current_wan_unit]);
			}
#endif
#endif
			else if(conn_state[current_wan_unit] == CONNED){
				if(conn_state_old[current_wan_unit] == DISCONN)
					conn_changed_state[current_wan_unit] = D2C;
				else
					conn_changed_state[current_wan_unit] = CONNED;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

#ifdef RTCONFIG_DSL /* Paul add 2013/7/29, for Non-DualWAN 3G/4G WAN -> DSL WAN, auto Fail-Back feature */
				if (nvram_match("dsltmp_adslsyncsts","up") && current_wan_unit == WAN_UNIT_SECOND){
					csprintf("\n# wanduck: adslsync up.\n");
					set_disconn_count(current_wan_unit, S_IDLE);
					link_wan[current_wan_unit] = DISCONN;
					conn_state[current_wan_unit] = DISCONN;
					usb_switched_back_dsl = 1;
					max_disconn_count[current_wan_unit] = 1;
				}
				else
					set_disconn_count(current_wan_unit, S_IDLE);
#else
				set_disconn_count(current_wan_unit, S_IDLE);
#endif
			}
			else if(conn_state[current_wan_unit] == DISCONN){
				if(conn_state_old[current_wan_unit] == CONNED)
					conn_changed_state[current_wan_unit] = C2D;
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

				if(disconn_case[current_wan_unit] == CASE_THESAMESUBNET){
					csprintf("# wanduck: set S_IDLE: CASE_THESAMESUBNET.\n");
					set_disconn_count(current_wan_unit, S_IDLE);
				}
#ifdef RTCONFIG_USB_MODEM
				// when the other line is modem and not plugged, the current disconnected line would not count.
				else if(!link_wan[other_wan_unit] && dualwan_unit__usbif(other_wan_unit))
					set_disconn_count(current_wan_unit, S_IDLE);
				else if(get_disconn_count(current_wan_unit) == S_IDLE && current_state[current_wan_unit] != WAN_STATE_DISABLED)
					set_disconn_count(current_wan_unit, S_COUNT);
#else
				else
					set_disconn_count(current_wan_unit, S_IDLE);
#endif
			}

			if(get_disconn_count(current_wan_unit) != S_IDLE){
				if(get_disconn_count(current_wan_unit) < max_disconn_count[current_wan_unit]){
					set_disconn_count(current_wan_unit, get_disconn_count(current_wan_unit)+1);
					csprintf("# wanduck(%d): wait time for switching: %d/%d.\n", current_wan_unit, get_disconn_count(current_wan_unit)*scan_interval, max_wait_time[current_wan_unit]);
				}
				else{
					csprintf("# wanduck(%d): set S_COUNT: changed_count[] >= max_disconn_count.\n", current_wan_unit);
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}

			record_conn_status(current_wan_unit);
		}
		else{ // sw_mode == SW_MODE_AP, SW_MODE_REPEATER.
			current_wan_unit = WAN_UNIT_FIRST;
			conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit);

			if(conn_state[current_wan_unit] == DISCONN){
				if(conn_state_old[current_wan_unit] == CONNED)
					conn_changed_state[current_wan_unit] = C2D;
				else
					conn_changed_state[current_wan_unit] = DISCONN;
			}
			else{
				if(conn_state_old[current_wan_unit] == DISCONN)
					conn_changed_state[current_wan_unit] = D2C;
				else
					conn_changed_state[current_wan_unit] = CONNED;
			}

			conn_state_old[current_wan_unit] = conn_state[current_wan_unit];
		}
if(test_log)
_dprintf("wanduck(%d)(all   end): state %d, state_old %d, changed %d, wan_state %d.\n"
		, current_wan_unit, conn_state[current_wan_unit], conn_state_old[current_wan_unit], conn_changed_state[current_wan_unit], current_state[current_wan_unit]);

#ifdef RTCONFIG_DUALWAN
		if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "lb")){
#ifdef RTCONFIG_DSL	//TODO: general case
			int internet_led = 0;
			for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
				if(is_wan_connect(wan_unit))	//since not update current_state[wan_unit] in USB modem case
					internet_led = 1;
			}
			if(internet_led) {
#if defined(RTCONFIG_WPS_ALLLED_BTN)
				if(nvram_match("AllLED", "1"))
					led_control(LED_WAN, LED_ON);
				else
					led_control(LED_WAN, LED_OFF);
#else
				led_control(LED_WAN, LED_ON);
#endif
			}
			else {
				led_control(LED_WAN, LED_OFF);
			}
#endif
			if(!rule_setup && (cross_state == DISCONN || isFirstUse)){
				if(isFirstUse)
					csprintf("\n# LB: Enable direct rule(isFirstUse)\n");
				else
					csprintf("\n# LB: Enable direct rule\n");
				rule_setup = 1;
				stop_nat_rules();
			}
			else if(rule_setup && cross_state == CONNED && !isFirstUse){
				csprintf("\n# LB: Disable direct rule\n");
				rule_setup = 0;
				start_nat_rules();
			}
		}
		else
#endif
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER){
#ifdef RTCONFIG_RESTRICT_GUI
			char word[PATH_MAX], *next_word;
#endif

			if(!got_notify)
				; // do nothing.
			else if(conn_changed_state[current_wan_unit] == DISCONN || conn_changed_state[current_wan_unit] == C2D || isFirstUse){
				//if(rule_setup == 0){
					if(conn_changed_state[current_wan_unit] == DISCONN)
						csprintf("\n# mode(%d): Enable direct rule(DISCONN)\n", sw_mode);
					else if(conn_changed_state[current_wan_unit] == C2D)
						csprintf("\n# mode(%d): Enable direct rule(C2D)\n", sw_mode);
					else
						csprintf("\n# mode(%d): Enable direct rule(isFirstUse)\n", sw_mode);
					rule_setup = 1;

					eval("ebtables", "-t", "broute", "-F");
					eval("ebtables", "-t", "filter", "-F");
					// Drop the DHCP server from PAP.
					eval("ebtables", "-t", "filter", "-A", "FORWARD", "-i", nvram_safe_get(wlc_nvname("ifname")), "-j", "DROP");
					f_write_string("/proc/net/dnsmqctrl", "", 0, 0);

#ifdef RTCONFIG_RESTRICT_GUI
					if(nvram_get_int("fw_restrict_gui")){
						foreach(word, nvram_safe_get("lan_ifnames"), next_word){
							if(!strncmp(word, "vlan", 4))
								continue;

							eval("ebtables", "-t", "broute", "-A", "BROUTING", "-i", word, "-j", "mark", "--mark-set", BIT_RES_GUI, "--mark-target", "ACCEPT");
						}

						repeater_filter_setting(0);
					}
#endif

					stop_nat_rules();
				//}
				got_notify = 0;
			}
			else{
				//if(rule_setup == 1 && !isFirstUse)
				if(!isFirstUse)
				{
					csprintf("\n# mode(%d): Disable direct rule(CONNED)\n", sw_mode);
					rule_setup = 0;
					eval("ebtables", "-t", "broute", "-F");
					eval("ebtables", "-t", "filter", "-F");

#ifdef RTCONFIG_RESTRICT_GUI
					if(nvram_get_int("fw_restrict_gui")){
						foreach(word, nvram_safe_get("lan_ifnames"), next_word){
							if(!strncmp(word, "vlan", 4))
								continue;

							eval("ebtables", "-t", "broute", "-A", "BROUTING", "-i", word, "-j", "mark", "--mark-set", BIT_RES_GUI, "--mark-target", "CONTINUE");
						}

						repeater_filter_setting(1);
					}
#endif

					eval("ebtables", "-t", "broute", "-A", "BROUTING", "-d", "00:E0:11:22:33:44", "-j", "redirect", "--redirect-target", "DROP");
					sprintf(domain_mapping, "%x %s", inet_addr(nvram_safe_get("lan_ipaddr")), DUT_DOMAIN_NAME);
					f_write_string("/proc/net/dnsmqctrl", domain_mapping, 0, 0);
#ifdef RTCONFIG_REDIRECT_DNAME
					eval("ebtables", "-t", "broute", "-A", "BROUTING", "-p", "ipv4", "--ip-proto", "udp", "--ip-dport", "53", "-j", "redirect", "--redirect-target", "DROP");
#endif		
					start_nat_rules();
				}

				got_notify = 0;
			}
		}
		else
#endif
		if(sw_mode == SW_MODE_AP){
#ifdef RTCONFIG_REDIRECT_DNAME
			if (conn_changed_state[current_wan_unit] == C2D) {
				csprintf("\n# AP mode: Enable direct rule(C2D)\n");
				eval("ebtables", "-t", "broute", "-F");
				eval("ebtables", "-t", "filter", "-F");
				redirect_setting();
				eval("iptables-restore", "/tmp/redirect_rules");
			}
			else if (conn_changed_state[current_wan_unit] == D2C) {
				csprintf("\n# AP mode: Disable direct rule(D2C)\n");
				eval("ebtables", "-t", "broute", "-F");
				eval("ebtables", "-t", "filter", "-F");
				eval("ebtables", "-t", "broute", "-I", "BROUTING", "-p", "ipv4", "--ip-proto", "udp", "--ip-dport", "53", "-j", "redirect", "--redirect-target", "DROP");
				redirect_nat_setting();
				eval("iptables-restore", NAT_RULES);
			}
#else
			; // do nothing.
#endif

#ifdef RTCONFIG_RESTRICT_GUI
			if(conn_changed_state[current_wan_unit] == D2C){
				if(nvram_get_int("fw_restrict_gui")){
					char word[PATH_MAX], *next_word;

					foreach(word, nvram_safe_get("lan_ifnames"), next_word){
						if(!strncmp(word, "vlan", 4))
							continue;

						eval("ebtables", "-t", "broute", "-I", "BROUTING", "-i", word, "-j", "mark", "--mark-set", BIT_RES_GUI, "--mark-target", "CONTINUE");
					}

					repeater_filter_setting(0);
				}
			}
#endif
		}
		else if(conn_changed_state[current_wan_unit] == C2D || (conn_changed_state[current_wan_unit] == CONNED && isFirstUse)){
			if(rule_setup == 0){
				if(conn_changed_state[current_wan_unit] == C2D){
					if (nvram_match("led_disable", "0")) {
#if defined(RTCONFIG_WPS_ALLLED_BTN)
						led_control(LED_WAN, LED_OFF);
#elif defined(RTCONFIG_DSL)
						led_control(LED_WAN, LED_OFF);
#elif defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)
						if (strcmp(dualwan_mode, "lb") == 0 ||
								strcmp(dualwan_mode, "fb") == 0 ||
								strcmp(dualwan_mode, "fo") == 0) {
							logmessage("DualWAN", "skip single wan wan_led_control - WANRED off\n");

							if(nvram_match("AllLED", "1")){
								led_control(LED_WAN, LED_ON);
								disable_wan_wled();
							}
						}
#endif
					}
					csprintf("\n# Enable direct rule(C2D)\n");
				}
				else
					csprintf("\n# Enable direct rule(isFirstUse)\n");
				rule_setup = 1;

				handle_wan_line(current_wan_unit, rule_setup);

				if((conn_changed_state[current_wan_unit] == C2D)
#ifdef RTCONFIG_DUALWAN
						&& strcmp(dualwan_mode, "off")
#endif
						){
#ifdef RTCONFIG_USB_MODEM
					// the current line is USB and be plugged off.
					if(!link_wan[current_wan_unit] && dualwan_unit__usbif(current_wan_unit)){
						clean_modem_state(2);
						if(get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE){
							csprintf("\n# wanduck(C2D): Modem was plugged off and try to Switch the other line.\n");
							switch_wan_line(other_wan_unit, 0);
						}
						else if(current_state[current_wan_unit] != WAN_STATE_INITIALIZING){
							csprintf("wanduck3: stop_wan_if %d.\n", current_wan_unit);
							snprintf(cmd, 32, "stop_wan_if %d", current_wan_unit);
							notify_rc(cmd);
						}

#ifdef RTCONFIG_DSL /* Paul add 2013/7/29, for Non-DualWAN 3G/4G WAN -> DSL WAN, auto Fail-Back feature */
#ifndef RTCONFIG_DUALWAN
						if(nvram_match("dsltmp_adslsyncsts","up") && usb_switched_back_dsl == 1){
							csprintf("\n# wanduck: usb_switched_back_dsl: 1.\n");
							link_wan[WAN_UNIT_SECOND] = CONNED; 
							max_disconn_count[WAN_UNIT_SECOND] = max_wait_time[WAN_UNIT_SECOND]/scan_interval;
							usb_switched_back_dsl = 0;
						}
#endif
#endif
					}
					else
#endif
					// C2D: Try to prepare the backup line.
					if(link_wan[other_wan_unit] == 1 && get_wan_state(other_wan_unit) != WAN_STATE_CONNECTED){
						csprintf("\n# wanduck(C2D): Try to prepare the backup line.\n");
						memset(cmd, 0, 32);
						sprintf(cmd, "restart_wan_if %d", other_wan_unit);
						notify_rc_and_wait(cmd);
					}
				}
			}
		}
		else if(conn_changed_state[current_wan_unit] == D2C || conn_changed_state[current_wan_unit] == CONNED){
                        if(rule_setup == 1 && !isFirstUse){
				if (nvram_match("led_disable", "0")) {
#if defined(RTCONFIG_WPS_ALLLED_BTN)
					if(nvram_match("AllLED", "1"))
						led_control(LED_WAN, LED_ON);
					else
						led_control(LED_WAN, LED_OFF);
#elif defined(DSL_N55U) || defined(DSL_N55U_B)
					led_control(LED_WAN, LED_ON);
#elif defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)
					if(nvram_match("AllLED", "1")){
						led_control(LED_WAN, LED_OFF);
						enable_wan_wled();
					}
#endif
				}
				csprintf("\n# Disable direct rule(D2C)\n");
				rule_setup = 0;
				handle_wan_line(current_wan_unit, rule_setup);
			}
		}
		/*
		 * when all lines are plugged in and the currect line is disconnected over max_wait_time seconds,
		 * switch the connect to the other line.
		 */
		else if(conn_changed_state[current_wan_unit] == DISCONN){
			if(get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE
					&& (get_disconn_count(current_wan_unit) >= max_disconn_count[current_wan_unit]
#ifdef RTCONFIG_USB_MODEM
							|| (!link_wan[current_wan_unit] && dualwan_unit__usbif(current_wan_unit)
#ifdef RT4GAC55U
									&& strcmp(usb_if, "")
#endif
									)
#endif
							)
					)
			{
				csprintf("# wanduck(%d): Switching the connect to the %d WAN line...\n", current_wan_unit, get_next_unit(current_wan_unit));
				if(!link_wan[current_wan_unit] && dualwan_unit__usbif(current_wan_unit))
					switch_wan_line(other_wan_unit, 0);
				else
					switch_wan_line(other_wan_unit, 1);
			}
		}
		// phy connected -> disconnected -> connected
		else if(conn_changed_state[current_wan_unit] == PHY_RECONN){
#ifdef RTCONFIG_USB_MODEM
			if(dualwan_unit__usbif(current_wan_unit)){
				if(current_state[current_wan_unit] == WAN_STATE_INITIALIZING){
					if(!strcmp(modem_type, "qmi")){
						csprintf("wanduck(%d): Sleep 1 seconds to wait QMI modem nodes.\n", current_wan_unit);
						sleep(1);
					}
					handle_wan_line(current_wan_unit, 0);
				}
				else
					csprintf("wanduck(%d): the modem had been run...\n", current_wan_unit);
			}
			else
#endif
				handle_wan_line(current_wan_unit, 0);

			conn_changed_state[current_wan_unit] = conn_state[current_wan_unit];
		}

#ifdef RTCONFIG_DUALWAN
		if(!strcmp(dualwan_mode, "fb") && other_wan_unit == WAN_FB_UNIT && conn_state[other_wan_unit] == CONNED
				&& get_disconn_count(other_wan_unit) >= max_fb_count
				){
			csprintf("# wanduck: returning to the primary WAN line(%d)...\n", other_wan_unit);
			rule_setup = 1;
			handle_wan_line(other_wan_unit, rule_setup);
			switch_wan_line(other_wan_unit, 0);
		}
		// hot-standby: Try to prepare the backup line.
		else if(!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb")){
			if(nvram_get_int("wans_standby") == 1 && link_wan[other_wan_unit] == 1 && get_wan_state(other_wan_unit) == WAN_STATE_INITIALIZING){
				csprintf("\n# wanduck(hot-standby): Try to prepare the backup line.\n");
				memset(cmd, 0, 32);
				sprintf(cmd, "restart_wan_if %d", other_wan_unit);
				notify_rc_and_wait(cmd);
			}
		}
#if defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)

		if (strcmp(dualwan_wans, "wan none")) {
			if(nvram_match("AllLED", "1")){
				
				link_status = 0;
				for (u = WAN_UNIT_FIRST; !link_status && u < WAN_UNIT_MAX; ++u) {
					if(conn_state[u] == CONNED)
						link_status++;
				}

				if(link_status) {
					if(wanred_led_status != 2 ){
						led_control(LED_WAN, LED_OFF);
						enable_wan_wled();
						wanred_led_status = 2;
					}
				}else{
					if(wanred_led_status != 1 ){
						led_control(LED_WAN, LED_ON);
						disable_wan_wled();
						wanred_led_status = 1;
					}
				}
			}
		}
#endif
#endif
#ifdef RTCONFIG_QTN
		if (nvram_get_int("ntp_ready") == 1 && nvram_get_int("qtn_ready") == 1){
			if (nvram_get_int("qtn_ntp_ready") == 0){
				time(&qtn_now);
				qtn_tm = localtime(&qtn_now);
				snprintf(time_string, sizeof(time_string), "%02d%02d%02d%02d%04d",
						qtn_tm->tm_mon+1, qtn_tm->tm_mday, qtn_tm->tm_hour, qtn_tm->tm_min, qtn_tm->tm_year+1900);
				eval("qcsapi_sockrpc", "run_script", "router_command.sh", "sync_time", time_string);
				nvram_set_int("qtn_ntp_ready", 1);
			}
		}
#endif

		start_demand_ppp(current_wan_unit, 1);

WANDUCK_SELECT:
		if((nready = select(maxfd+1, &rset, NULL, NULL, &tval)) <= 0)
			continue;


		if(FD_ISSET(dns_sock, &rset)){
			run_dns_serv(dns_sock);
			if(--nready <= 0)
				continue;
		}
		else if(FD_ISSET(http_sock, &rset)){
			if((cur_sockfd = accept(http_sock, (struct sockaddr *)&cliaddr, (socklen_t *)&clilen)) <= 0){
				perror("http accept");
				continue;
			}

			for(fd_i = 0; fd_i < MAX_USER; ++fd_i){
				if(client[fd_i].sfd < 0){
					client[fd_i].sfd = cur_sockfd;
					client[fd_i].type = T_HTTP;
					break;
				}
			}

			if(fd_i == MAX_USER){
				csprintf("# wanduck servs full\n");
				close(cur_sockfd);
				continue;
			}

			FD_SET(cur_sockfd, &allset);
			if(cur_sockfd > maxfd)
				maxfd = cur_sockfd;
			if(fd_i > maxi)
				maxi = fd_i;
			
			if(--nready <= 0)
				continue;	// no more readable descriptors
		}

		// polling
		for(fd_i = 0; fd_i <= maxi; ++fd_i){
			if((sockfd = client[fd_i].sfd) < 0)
				continue;

			if(FD_ISSET(sockfd, &rset)){
				int nread;
				ioctl(sockfd, FIONREAD, &nread);
				if(nread == 0){
					close_socket(sockfd, T_HTTP);
					continue;
				}
				if(fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0)|O_NONBLOCK) < 0){
					_dprintf("wanduck set http req [%d] nonblock fail !\n", sockfd);
					continue;
				}

				cur_sockfd = sockfd;

				run_http_serv(sockfd);

				if(--nready <= 0)
					break;
			}
		}
	}

	csprintf("# wanduck exit error\n");
	exit(1);
}
