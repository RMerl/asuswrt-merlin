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

static void safe_leave(int signo){
	csprintf("\n## wanduck.safeexit ##\n");
	signal(SIGTERM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	FD_ZERO(&allset);
	close(http_sock);
	close(dns_sock);

	int i, ret;
	for(i = 0; i < maxfd; ++i){
		ret = close(i);
		csprintf("## close %d: result=%d.\n", i, ret);
	}

	sleep(1);

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER){
		eval("ebtables", "-t", "broute", "-F");
		eval("ebtables", "-t", "filter", "-F");
		f_write_string("/proc/net/dnsmqctrl", "", 0, 0);
	}
#endif

	if(rule_setup == 1){
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

			_dprintf("%s: apply the nat_rules(%s): %s!\n", __FUNCTION__, fn, buf);
			logmessage("wanduck exit", "apply the nat_rules(%s)!", fn);

			setup_ct_timeout(TRUE);
			setup_udp_timeout(TRUE);

			eval("iptables-restore", NAT_RULES);
		}
		else{
			sprintf(buf, "nat_state=%d", NAT_STATE_INITIALIZING);
			eval("nvram", "set", buf);

			_dprintf("%s: initial the nat_rules: %s!\n", __FUNCTION__, buf);
			logmessage("wanduck exit", "initial the nat_rules!");
		}
#endif
	}

	remove(WANDUCK_PID_FILE);

csprintf("\n# return(exit wanduck)\n");
	exit(0);
}

void get_related_nvram(){
	sw_mode = nvram_get_int("sw_mode");

	if(nvram_match("x_Setting", "1"))
		isFirstUse = 0;
	else
		isFirstUse = 1;

#ifdef RTCONFIG_WIRELESSREPEATER
	if(strlen(nvram_safe_get("wlc_ssid")) > 0)
		setAP = 1;
	else
		setAP = 0;
#endif

#ifdef RTCONFIG_DUALWAN
	memset(dualwan_mode, 0, 8);
	strcpy(dualwan_mode, nvram_safe_get("wans_mode"));
#endif
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

#ifdef RTCONFIG_USB_MODEM
static void notify_nvram_changed(int signo){
	int unit;

#ifdef RTCONFIG_DUALWAN
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
		if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB){
			link_wan[unit] = is_usb_modem_ready();
			break;
		}
	}

	if(unit == WAN_UNIT_MAX)
		csprintf("# wanduck: nvram changed: Don't enable the USB line!\n");
	else
#else
	unit = WAN_UNIT_SECOND;

	link_wan[unit] = is_usb_modem_ready();
#endif

	csprintf("# wanduck: nvram changed: x_Setting=%d, link_modem=%d.\n", !isFirstUse, link_wan[unit]);
}
#endif

int get_packets_of_net_dev(const char *net_dev, unsigned long *rx_packets, unsigned long *tx_packets){
	FILE *fp;
	char buf[256];
	char *ifname;
	char *ptr;
	int i, got_packets = 0;

	if((fp = fopen(PROC_NET_DEV, "r")) == NULL){
csprintf("get_packets_of_net_dev: Can't open the file: %s.\n", PROC_NET_DEV);
		return got_packets;
	}

	// headers.
	for(i = 0; i < 2; ++i){
		if(fgets(buf, sizeof(buf), fp) == NULL){
csprintf("get_packets_of_net_dev: Can't read out the headers of %s.\n", PROC_NET_DEV);
			fclose(fp);
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
csprintf("get_packets_of_net_dev: Can't read the packet's number in %s.\n", PROC_NET_DEV);
			fclose(fp);
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

int do_ping_detect(int wan_unit){
	char cmd[256], buf[16], *next;
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

	return 0;
}

int do_dns_detect(){
	const char *test_url = "www.asus.com";

	if(gethostbyname(test_url) != NULL)
		return 1;

	return 0;
}

int do_tcp_dns_detect(int wan_unit){
	FILE *fp = NULL;
	char line[80], cmd[PATH_MAX];
	char prefix_wan[8], nvram_name[16], wan_dns[256];

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);

	memset(wan_dns, 0, 256);
	strcpy(wan_dns, nvram_safe_get(strcat_r(prefix_wan, "dns", nvram_name)));

	remove(DETECT_FILE);

	if(organize_tcpcheck_cmd(wan_dns, cmd, PATH_MAX) == NULL){
csprintf("wanduck: No tcpcheck cmd.\n");
		return 0;
	}
	system(cmd);

	if((fp = fopen(DETECT_FILE, "r")) == NULL){
csprintf("wanduck: No file: %s.\n", DETECT_FILE);
		return 0;
	}

	while(fgets(line, sizeof(line), fp) != NULL){
		if(strstr(line, "alive")){
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);

	return 0;
}

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
	else
		link_internet = CONNED;

	if(link_internet == DISCONN){
		nvram_set_int("link_internet", 0);
		record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NO_INTERNET_ACTIVITY);

		if(!(nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOINTERNET)) {
			nvram_set_int("link_internet", 1);
			link_internet = CONNED;
		}
	}
	else{
		nvram_set_int("link_internet", 1);
		record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NONE);
	}

	return link_internet;
}

int trigger_ppp_connection(int wan_unit){
#ifdef RTCONFIG_DSL
	const char *test_url = "203.69.138.19";
#else
	const char *test_url = "www.asus.com";
#endif
	char cmd[256];
	char prefix_wan[8], nvram_name[16];

	memset(prefix_wan, 0, 8);
	sprintf(prefix_wan, "wan%d_", wan_unit);

	if(nvram_match(strcat_r(prefix_wan, "proto", nvram_name), "pppoe")
			&& nvram_get_int(strcat_r(prefix_wan, "pppoe_demand", nvram_name))
			&& (nvram_match(strcat_r(prefix_wan, "ipaddr", nvram_name), "") || nvram_match(strcat_r(prefix_wan, "ipaddr", nvram_name), "10.64.64.64"))
			){
csprintf("# wanduck trigger the PPP connection!\n");
		sprintf(cmd, "ping -c 1 -W %d %s", TCPCHECK_TIMEOUT, test_url);
logmessage("wanduck", cmd);
		system(cmd);
	}

	return 0;
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
			if((fd = open(task_file, O_RDONLY)) > 0){
				memset(cmdline, 0, 64);
				read(fd, cmdline, 64);
				close(fd);

				if(strstr(cmdline, "pppd")
						|| strstr(cmdline, "l2tpd")
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
#ifdef RTCONFIG_DUALWAN
	if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_USB)
#else
	if(wan_unit == WAN_UNIT_SECOND)
#endif
	{
		ppp_fail_state = pppstatus();

		if(wan_state == WAN_STATE_INITIALIZING){
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
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
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
				record_wan_state_nvram(wan_unit, -1, -1, WAN_STOPPED_REASON_PPP_LACK_ACTIVITY);
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

int if_wan_phyconnected(int wan_unit, int wan_state){
	//int unit;
	char *wired_link_nvram;
#ifdef RTCONFIG_WIRELESSREPEATER
	int link_ap;
#endif
	int link_changed = 0;
	char prefix_wan[8], nvram_name[16], wan_proto[16];

	if(wan_unit)
		wired_link_nvram = "link_wan1";
	else
		wired_link_nvram = "link_wan";

#ifdef RTCONFIG_USB_MODEM
#ifdef RTCONFIG_DUALWAN
	if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_USB)
#else
	if(wan_unit == WAN_UNIT_SECOND)
#endif
	{
		if(link_wan[wan_unit] != nvram_get_int(wired_link_nvram)){
			nvram_set_int(wired_link_nvram, link_wan[wan_unit]);
		}
	}
	else
#endif
#ifdef RTCONFIG_DUALWAN
	if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_WAN
			|| get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_DSL
			|| get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_LAN
			)
#else
	if(wan_unit == WAN_UNIT_FIRST)
#endif
	{
		memset(prefix_wan, 0, 8);
		sprintf(prefix_wan, "wan%d_", wan_unit);

		memset(wan_proto, 0, 16);
		strcpy(wan_proto, nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

		// check wan port.
		link_wan[wan_unit] = get_wanports_status(wan_unit);

#ifdef RTCONFIG_LANWAN_LED
		if(link_wan[wan_unit]) led_control(LED_WAN, LED_ON);
		else led_control(LED_WAN, LED_OFF);

		if(get_lanports_status()) led_control(LED_LAN, LED_ON);
		else led_control(LED_LAN, LED_OFF);
#endif

		if(link_wan[wan_unit] != nvram_get_int(wired_link_nvram)){
			if(link_wan[wan_unit])
				nvram_set_int(wired_link_nvram, CONNED);
			else
				nvram_set_int(wired_link_nvram, DISCONN);

			link_changed = 1;
		}
	}

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
		if (link_changed) {
			// WAN port was disconnected, arm reconnect
			if(!link_setup[wan_unit] && !link_wan[wan_unit]){
				link_setup[wan_unit] = 1;
			} else
			// WAN port was connected, fire reconnect if armed
			if (link_setup[wan_unit]) {
				link_setup[wan_unit] = 0;
				// Only handle this case when WAN proto is DHCP or Static
				if (strcmp(wan_proto, "static") == 0) {
					disconn_case[wan_unit] = CASE_DISWAN;
					return PHY_RECONN;
				} else
				if (strcmp(wan_proto, "dhcp") == 0) {
					disconn_case[wan_unit] = CASE_DHCPFAIL;
					return PHY_RECONN;
				}
			}
		}
		else if(!link_wan[wan_unit]){
#ifdef RTCONFIG_DUALWAN
			if(strcmp(dualwan_mode, "lb"))
#endif
				nvram_set_int("link_internet", 0);

			record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);

			if((nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)){
				disconn_case[wan_unit] = CASE_DISWAN;
				return DISCONN;
			}
		}
	}
	else if(sw_mode == SW_MODE_AP){
#if 0
		if(!link_wan[wan_unit]){
			// ?: type error?
			nvram_set_int("link_internet", 0);

			record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);

			if((nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)){
				disconn_case[wan_unit] = CASE_DISWAN;
				return DISCONN;
			}
		}
#else
		int orig_internet = nvram_get_int("link_internet");
		if(orig_internet != 1)
			nvram_set_int("link_internet", 1);

		return CONNED;
#endif
	}
#ifdef RTCONFIG_WIRELESSREPEATER
	else{ // sw_mode == SW_MODE_REPEATER, SW_MODE_HOTSPOT.
		if(!link_ap){
			if(sw_mode == SW_MODE_HOTSPOT)
				record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);

			disconn_case[wan_unit] = CASE_AP_DISCONN;
			return DISCONN;
		}
		else if(sw_mode == SW_MODE_REPEATER){
			if(nvram_match("lan_proto", "dhcp") && nvram_get_int("lan_state_t") != LAN_STATE_CONNECTED)
				return DISCONN;
			else
				return CONNED;
		}
	}
#endif

	if(chk_proto(wan_unit, wan_state) == DISCONN){
		return DISCONN;
	}
	else if(sw_mode == SW_MODE_ROUTER) // TODO: temparily let detect_internet() service in SW_MODE_ROUTER.
		return detect_internet(wan_unit);

	return CONNED;
}

void handle_wan_line(int wan_unit, int action){
	char cmd[32];
	char prefix_wan[8], nvram_name[16], wan_proto[16];

	// Redirect rules.
	if(action){
		stop_nat_rules();
	}
	/*
	 * When C2C and remove the redirect rules,
	 * it means dissolve the default state.
	 */
	else if(conn_changed_state[wan_unit] == D2C || conn_changed_state[wan_unit] == CONNED){
		start_nat_rules();

		memset(prefix_wan, 0, 8);
		sprintf(prefix_wan, "wan%d_", wan_unit);

		memset(wan_proto, 0, 16);
		strcpy(wan_proto, nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

		if(!strcmp(wan_proto, "static")){
			/* Sync time */
			refresh_ntpc();
		}

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
		if(check_if_dir_exist("/opt/lib/ipkg")){
_dprintf("wanduck: update the APP's lists...\n");
			notify_rc("start_apps_update");
		}
#endif
	}
	else{ // conn_changed_state[wan_unit] == PHY_RECONN
csprintf("\n# wanduck(%d): Try to restart_wan_if.\n", action);
		memset(cmd, 0, 32);
		sprintf(cmd, "restart_wan_if %d", wan_unit);
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
	now = uptime();
	(void)strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

#ifdef NO_IOS_DETECT_INTERNET
	// disable iOS popup window. Jerry5 2012.11.27
	if (!strcmp(url,"www.apple.com/library/test/success.html")){
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

	if((conn_changed_state[wan_unit] == C2D || conn_changed_state[wan_unit] == DISCONN) && disconn_case[wan_unit] == CASE_THESAMESUBNET)
		sprintf(buf, "%s%s%s%s%s%d%s%s" ,buf , "Connection: close\r\n", "Location:http://", dut_addr, "/error_page.htm?flag=", disconn_case[wan_unit], "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
	else if(isFirstUse){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT)
			sprintf(buf, "%s%s%s%s%s%s%s" ,buf , "Connection: close\r\n", "Location:http://", dut_addr, "/QIS_wizard.htm?flag=sitesurvey", "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
		else
#endif
			sprintf(buf, "%s%s%s%s%s%s%s" ,buf , "Connection: close\r\n", "Location:http://", dut_addr, "/QIS_wizard.htm?flag=welcome", "\r\nContent-Type: text/plain\r\n", "\r\n<html></html>\r\n");
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

		// TODO: Only send pages for the wan(0)'s state.
		send_page(0, sfd, NULL, dst_url);
	}
	else
		close_socket(sfd, T_HTTP);
}

void handle_dns_req(int sfd, char *line, int maxlen, struct sockaddr *pcliaddr, int clen){
	dns_query_packet d_req;
	dns_response_packet d_reply;
	int reply_size;
	char reply_content[MAXLINE];
	
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
	
	if(!upper_strcmp(query_name, router_name)){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_match("lan_proto", "dhcp") && nvram_get_int("lan_state_t") != LAN_STATE_CONNECTED)
			d_reply.answers.addr = inet_addr_(nvram_default_get("lan_ipaddr"));
		else
#endif
			d_reply.answers.addr = inet_addr_(nvram_safe_get("lan_ipaddr"));	// router's ip
	}
	else
		d_reply.answers.addr = htonl(0x0a000001);	// 10.0.0.1
	
	memcpy(reply_content+reply_size, &d_reply.answers, sizeof(d_reply.answers));
	reply_size += sizeof(d_reply.answers);
	
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
		perror("wanduck read");
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
	
	if((n = recvfrom(sockfd, line, MAXLINE, 0, (struct sockaddr *)&cliaddr, &clilen)) == 0)	// client close
		return;
	else if(n < 0){
		perror("wanduck read");
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
	if(!strcmp(dualwan_mode, "lb"))
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
			if(disconn_case_old[wan_unit] == CASE_MISROUTE)
				return;
			disconn_case_old[wan_unit] = CASE_MISROUTE;

			logmessage(log_title, "The LAN's subnet may be the same with the WAN's subnet.");
		}
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

int switch_wan_line(const int wan_unit){
#ifdef RTCONFIG_USB_MODEM
	int retry, lock;
#endif
	char cmd[32];
	int unit;

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
		if(unit == wan_unit)
			break;
	if(unit == WAN_UNIT_MAX)
		return 0;

	if(wan_primary_ifunit() == wan_unit) // Already have no running modem.
		return 0;
#ifdef RTCONFIG_USB_MODEM
#ifdef RTCONFIG_DUALWAN
	else if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_USB)
#else
	else if(wan_unit == WAN_UNIT_SECOND)
#endif
	{
		//if(!is_usb_modem_ready())
		if(!link_wan[wan_unit])
			return 0; // No modem in USB ports.
	}
#endif

	csprintf("%s: wan(%d) Starting...\n", __FUNCTION__, wan_unit);
	// Set the modem to be running.
	set_wan_primary_ifunit(wan_unit);

#ifdef RTCONFIG_USB_MODEM
	if(nvram_invmatch("modem_enable", "4")
#ifdef RTCONFIG_DUALWAN
			&& get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_USB
#else
			&& wan_unit == WAN_UNIT_SECOND
#endif
			){
		// Wait the PPP config file to be done.
		retry = 0;
		while((lock = file_lock("3g")) == -1 && retry < MAX_WAIT_FILE)
			sleep(1);

		if(lock == -1){
			csprintf("(%d): No pppd conf file and turn off the state of USB Modem.\n", wan_unit);
			set_wan_primary_ifunit(0);
			return 0;
		}
		else
			file_unlock(lock);
	}
#endif

	// TODO: don't know if it's necessary?
	// clean or restart the other line.
#ifdef RTCONFIG_DUALWAN
	if(!strcmp(dualwan_mode, "fo"))
#endif
	{
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
			if(unit == wan_unit)
				continue;

			memset(cmd, 0, 32);
			sprintf(cmd, "restart_wan_if %d", !wan_unit);
TRACE_PT("%s.\n", cmd);
			notify_rc_and_wait(cmd);
			sleep(1);
		}
	}

	// restart the primary line.
	memset(cmd, 0, 32);
	if(get_wan_state(wan_unit) == WAN_STATE_CONNECTED)
		sprintf(cmd, "restart_wan_line %d", wan_unit);
	else
		sprintf(cmd, "restart_wan_if %d", wan_unit);
TRACE_PT("%s.\n", cmd);
	notify_rc_and_wait(cmd);

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
#ifdef RTCONFIG_WIRELESSREPEATER
	char domain_mapping[64];
#endif

	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, safe_leave);
	signal(SIGUSR1, get_network_nvram);
#ifdef RTCONFIG_USB_MODEM
	signal(SIGUSR2, notify_nvram_changed);
#endif

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

	if(build_socket(http_servport, dns_servport, &http_sock, &dns_sock) < 0){
		csprintf("\n*** Fail to build socket! ***\n");
		exit(0);
	}

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
	clilen = sizeof(cliaddr);

	sprintf(router_name, "%s", DUT_DOMAIN_NAME);

	max_wait_time = nvram_get_int("wans_disconn_time");
	if(max_wait_time < SCAN_INTERVAL)
		max_wait_time = MAX_WAIT_TIME;
	max_disconn_count = max_wait_time/SCAN_INTERVAL;

	nvram_set_int("link_wan", 0);
	nvram_set_int("link_wan1", 0);
	nvram_set_int("link_internet", 2);

	for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
		link_setup[wan_unit] = 0;
		link_wan[wan_unit] = DISCONN;

		changed_count[wan_unit] = S_IDLE;
		disconn_case[wan_unit] = CASE_NONE;

		memset(prefix_wan, 0, 8);
		sprintf(prefix_wan, "wan%d_", wan_unit);

		strcat_r(prefix_wan, "state_t", nvram_state[wan_unit]);
		strcat_r(prefix_wan, "sbstate_t", nvram_sbstate[wan_unit]);
		strcat_r(prefix_wan, "auxstate_t", nvram_auxstate[wan_unit]);
	}

#ifdef RTCONFIG_USB_MODEM
	notify_nvram_changed(-1);
#endif
	get_related_nvram(); // initial the System's variables.
	get_lan_nvram(); // initial the LAN's variables.

#ifdef RTCONFIG_DUALWAN
	if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "lb")){
		cross_state = DISCONN;
		for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
			conn_state[wan_unit] = if_wan_phyconnected(wan_unit, nvram_get_int(nvram_state[wan_unit]));

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
	else
#endif
	if(sw_mode == SW_MODE_ROUTER
#ifdef RTCONFIG_WIRELESSREPEATER
			|| sw_mode == SW_MODE_HOTSPOT
#endif
			){
		current_wan_unit = wan_primary_ifunit();
		other_wan_unit = !current_wan_unit;

		conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit, nvram_get_int(nvram_state[current_wan_unit]));

		conn_changed_state[current_wan_unit] = conn_state[current_wan_unit];

		cross_state = conn_state[current_wan_unit];

		conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

		record_conn_status(current_wan_unit);
	}
	else{ // sw_mode == SW_MODE_AP, SW_MODE_REPEATER.
		current_wan_unit = WAN_UNIT_FIRST;

		conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit, nvram_get_int(nvram_state[current_wan_unit]));

		cross_state = conn_state[current_wan_unit];

		conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER)
			record_conn_status(current_wan_unit);
#endif
	}

	/*
	 * Because start_wanduck() is run early than start_wan()
	 * and the redirect rules is already set before running wanduck,
	 * handle_wan_line() must not be run at the first detect.
	 */
	if(cross_state == DISCONN){
csprintf("\n# Enable direct rule\n");
		rule_setup = 1;
	}
	else if(cross_state == CONNED && isFirstUse){
csprintf("\n#CONNED : Enable direct rule\n");
		rule_setup = 1;
	}

	for(;;){
		rset = allset;
		tval.tv_sec = SCAN_INTERVAL;
		tval.tv_usec = 0;

		get_related_nvram();

#ifdef RTCONFIG_DUALWAN // RTCONFIG_DUALWAN
		if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "lb")){
			for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit){
#ifdef RTCONFIG_USB_MODEM
				//if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_USB && !link_modem)
				if(get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_USB && !link_wan[wan_unit])
					continue;
#endif

				current_state[wan_unit] = nvram_get_int(nvram_state[wan_unit]);

				if(current_state[wan_unit] == WAN_STATE_DISABLED){
					//record_wan_state_nvram(wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL, -1);

					disconn_case[wan_unit] = CASE_OTHERS;
					conn_state[wan_unit] = DISCONN;
				}
				else{
					conn_state[wan_unit] = if_wan_phyconnected(wan_unit, current_state[wan_unit]);
				}

				if(conn_state[wan_unit] != conn_state_old[wan_unit]){
					if(conn_state[wan_unit] == PHY_RECONN){
						conn_changed_state[wan_unit] = PHY_RECONN;
					}
					else if(conn_state[wan_unit] == DISCONN){
						conn_changed_state[wan_unit] = C2D;

						set_disconn_count(wan_unit, S_COUNT);
					}
					else if(conn_state[wan_unit] == CONNED){
						if(rule_setup == 1 && !isFirstUse){
csprintf("\n# DualWAN: Disable direct rule(D2C)\n");
							rule_setup = 0;
						}

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
						set_disconn_count(wan_unit, max_disconn_count);

					if(get_disconn_count(wan_unit) >= max_disconn_count){
						set_disconn_count(wan_unit, S_IDLE);

						memset(cmd, 0, 32);
						sprintf(cmd, "restart_wan_if %d", wan_unit);
						notify_rc_and_wait(cmd);

						memset(cmd, 0, 32);
						sprintf(cmd, "restart_wan_line %d", !wan_unit);
						notify_rc(cmd);
					}
					else
						set_disconn_count(wan_unit, get_disconn_count(wan_unit)+1);

					csprintf("%s: wan(%d) disconn count = %d/%d ...\n", __FUNCTION__, wan_unit, get_disconn_count(wan_unit), max_disconn_count);
				}
			}
		}
		else
#endif // RTCONFIG_DUALWAN
		if(sw_mode == SW_MODE_ROUTER
#ifdef RTCONFIG_WIRELESSREPEATER
				|| sw_mode == SW_MODE_HOTSPOT
#endif
				){
			current_wan_unit = wan_primary_ifunit();
			other_wan_unit = !current_wan_unit;

			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);

			if(current_state[current_wan_unit] == WAN_STATE_DISABLED){
				//record_wan_state_nvram(current_wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL, -1);

				disconn_case[current_wan_unit] = CASE_OTHERS;
				conn_state[current_wan_unit] = DISCONN;

				set_disconn_count(current_wan_unit, S_IDLE);
			}
			else
				conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit, current_state[current_wan_unit]);

			if(conn_state[current_wan_unit] == PHY_RECONN){
				conn_changed_state[current_wan_unit] = PHY_RECONN;

				conn_state_old[current_wan_unit] = DISCONN;

				// When the current line is re-plugged and the other line has plugged, the count has to be reset.
				if(link_wan[other_wan_unit]){
csprintf("# wanduck: set S_COUNT: PHY_RECONN.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}
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
				else if(!link_wan[other_wan_unit]
#ifdef RTCONFIG_DUALWAN
						&& get_dualwan_by_unit(other_wan_unit) == WANS_DUALWAN_IF_USB
#else
						&& other_wan_unit == WAN_UNIT_SECOND
#endif
						){
					set_disconn_count(current_wan_unit, S_IDLE);
				}
#endif
#if defined(RTCONFIG_USB_MODEM) || defined(RTCONFIG_DUALWAN)
				else if(get_disconn_count(current_wan_unit) == S_IDLE && current_state[current_wan_unit] != WAN_STATE_DISABLED
#ifdef RTCONFIG_DUALWAN
						&& (!strcmp(dualwan_mode, "off")
								|| (!strcmp(dualwan_mode, "fo") && get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE)
								)
#endif
						)
					set_disconn_count(current_wan_unit, S_COUNT);
#endif
			}

			if(get_disconn_count(current_wan_unit) != S_IDLE){
				if(get_disconn_count(current_wan_unit) < max_disconn_count){
					set_disconn_count(current_wan_unit, get_disconn_count(current_wan_unit)+1);
csprintf("# wanduck: wait time for switching: %d/%d.\n", get_disconn_count(current_wan_unit)*SCAN_INTERVAL, max_wait_time);
				}
				else{
csprintf("# wanduck: set S_COUNT: changed_count[] >= max_disconn_count.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}

			record_conn_status(current_wan_unit);
		}
		else{ // sw_mode == SW_MODE_AP, SW_MODE_REPEATER.
			current_wan_unit = WAN_UNIT_FIRST;
			conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit, current_state[current_wan_unit]);

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

#ifdef RTCONFIG_DUALWAN
		if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "lb")){
			;
		}
		else
#endif
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER){
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
					eval("ebtables", "-t", "filter", "-I", "FORWARD", "-i", nvram_safe_get(wlc_nvname("ifname")), "-j", "DROP");
					f_write_string("/proc/net/dnsmqctrl", "", 0, 0);

					stop_nat_rules();
				//}
				got_notify = 0;
			}
			else{
				//if(rule_setup == 1 && !isFirstUse){
				if(!isFirstUse){
csprintf("\n# mode(%d): Disable direct rule(CONNED)\n", sw_mode);
					rule_setup = 0;

					eval("ebtables", "-t", "broute", "-F");
					eval("ebtables", "-t", "filter", "-F");
					eval("ebtables", "-t", "broute", "-I", "BROUTING", "-p", "ipv4", "-d", "00:E0:11:22:33:44", "-j", "redirect", "--redirect-target", "DROP");
					sprintf(domain_mapping, "%x %s", inet_addr(nvram_safe_get("lan_ipaddr")), DUT_DOMAIN_NAME);
					f_write_string("/proc/net/dnsmqctrl", domain_mapping, 0, 0);

					start_nat_rules();
				}

				got_notify = 0;
			}
		}
		else
#endif
		if(sw_mode == SW_MODE_AP){
			; // do nothing.
		}
		else if(conn_changed_state[current_wan_unit] == C2D || (conn_changed_state[current_wan_unit] == CONNED && isFirstUse)){
			if(rule_setup == 0){
if(conn_changed_state[current_wan_unit] == C2D)
{
#ifdef RTCONFIG_DSL /* Paul add 2012/10/18 */
	led_control(LED_WAN, LED_OFF);
#endif
	csprintf("\n# Enable direct rule(C2D)\n");
}
else
	csprintf("\n# Enable direct rule(isFirstUse)\n");
				rule_setup = 1;

				handle_wan_line(current_wan_unit, rule_setup);

				if(conn_changed_state[current_wan_unit] == C2D
#ifdef RTCONFIG_DUALWAN
						&& strcmp(dualwan_mode, "off")
#endif
						){
#ifdef RTCONFIG_USB_MODEM
					// the current line is USB and be plugged off.
					if(!link_wan[current_wan_unit]
#ifdef RTCONFIG_DUALWAN
							&& get_dualwan_by_unit(current_wan_unit) == WANS_DUALWAN_IF_USB
#else
							&& current_wan_unit == WAN_UNIT_SECOND
#endif
							){
csprintf("\n# wanduck(C2D): Modem was plugged off and try to Switch the other line.\n");
						switch_wan_line(other_wan_unit);
					}
					else
#endif
					// C2D: Try to prepare the backup line.
					if(link_wan[other_wan_unit]){
						if(get_wan_state(other_wan_unit) != WAN_STATE_CONNECTED){
csprintf("\n# wanduck(C2D): Try to prepare the backup line.\n");
							memset(cmd, 0, 32);
							sprintf(cmd, "restart_wan_if %d", other_wan_unit);
							notify_rc_and_wait(cmd);
						}
					}
				}
			}
		}
		else if(conn_changed_state[current_wan_unit] == D2C || conn_changed_state[current_wan_unit] == CONNED){
			if(rule_setup == 1 && !isFirstUse){
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
			//if((link_modem && get_disconn_count(current_wan_unit) >= max_disconn_count)
			if(get_disconn_count(current_wan_unit) >= max_disconn_count
#ifdef RTCONFIG_USB_MODEM
					|| (!link_wan[current_wan_unit]
#ifdef RTCONFIG_DUALWAN
							&& get_dualwan_by_unit(current_wan_unit) == WANS_DUALWAN_IF_USB
#else
							&& current_wan_unit == WAN_UNIT_SECOND
#endif
							)
#endif
					)
			{
				if(current_wan_unit)
					csprintf("# Switching the connect to the first WAN line...\n");
				else
					csprintf("# Switching the connect to the second WAN line...\n");
				switch_wan_line(other_wan_unit);
			}
		}
		// phy connected -> disconnected -> connected
		else if(conn_changed_state[current_wan_unit] == PHY_RECONN){
			handle_wan_line(current_wan_unit, 0);
		}

		trigger_ppp_connection(current_wan_unit);

		if((nready = select(maxfd+1, &rset, NULL, NULL, &tval)) <= 0)
			continue;

		if(FD_ISSET(dns_sock, &rset)){
			run_dns_serv(dns_sock);
			if(--nready <= 0)
				continue;
		}
		else if(FD_ISSET(http_sock, &rset)){
			if((cur_sockfd = accept(http_sock, (struct sockaddr *)&cliaddr, &clilen)) <= 0){
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
