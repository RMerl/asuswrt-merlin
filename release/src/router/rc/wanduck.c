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

#include <wanduck.h>


//#define DETECT_INTERNET_MORE
#define NO_IOS_DETECT_INTERNET
#ifdef RTCONFIG_LAN4WAN_LED
int LanWanLedCtrl(void);
#endif

static int wdbg = 0;

#define _wdbg(fmt, args...) do { if (wdbg) { dbg(fmt, ## args); }; } while (0)

#if defined(RTCONFIG_WANRED_LED)
static int update_wan_led_and_wanred_led(int wan_unit)
{
#if defined(RTCONFIG_WANPORT2)
	/* e.g. BRT-AC828: WAN WHITE/RED LED x 2, RTCONFIG_DUALWAN must be enabled. */
	int mode = sw_mode, l = link_wan[wan_unit], state;
	int wan_led, wanred_led;
	char s[] = "wanX_state_tXXX";

	if (wan_unit < 0 || wan_unit >= WAN_UNIT_MAX)
		return -1;

	if (mode < SW_MODE_ROUTER || mode > SW_MODE_HOTSPOT)
		mode = nvram_get_int("sw_mode");
	/* Turn on/off WAN WHITE/RED LED in accordance with wan status. */
	switch (mode) {
	case SW_MODE_ROUTER:
		switch (wan_unit) {
		case 0:
			wan_led = LED_WAN;
			wanred_led = LED_WAN_RED;
			break;
		case 1:
			wan_led = LED_WAN2;
			wanred_led = LED_WAN2_RED;
			break;
		default:
			return 0;
		}

		if (!nvram_match("wans_mode", "lb") &&
		    wan_primary_ifunit() != wan_unit)
		{
			led_control(wanred_led, LED_OFF);
			led_control(wan_led, LED_OFF);
			return 0;
		}

		snprintf(s, sizeof(s), "wan%d_state_t", wan_unit);
		state = nvram_get_int(s);
		l = get_wanports_status(wan_unit);
		if (l == CONNED && state == WAN_STATE_CONNECTED) {
			led_control(wanred_led, LED_OFF);
			led_control(wan_led, LED_ON);
		} else {
			led_control(wanred_led, LED_ON);
			led_control(wan_led, LED_OFF);
		}
		break;
	case SW_MODE_REPEATER:	/* fallthrough */
	case SW_MODE_AP:
		wan_red_led_control(LED_OFF);
		wan2_red_led_control(LED_OFF);
		break;
	}
#else	/* !RTCONFIG_WANPORT2 */
	/* e.g. RT-AC55U: WAN BLUE/RED LED */
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
		if (nvram_match("wans_mode", "lb")) {
			int u, onoff = 0;

			if (wan_unit)
				return 0;

			/* Turn on WAN BLUE LED if any WAN unit is connected in load-balanced mode. */
			for (u = WAN_UNIT_FIRST; !onoff && u < WAN_UNIT_MAX; ++u) {
				snprintf(s, sizeof(s), "wan%d_state_t", u);
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
		} else
#endif
		{
			if (wan_primary_ifunit() != wan_unit)
				return 0;

			snprintf(s, sizeof(s), "wan%d_state_t", wan_unit);
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
		}
		break;
	case SW_MODE_REPEATER:	/* fallthrough */
	case SW_MODE_AP:
		wan_red_led_control(LED_OFF);
		break;
	}
#endif	/* RTCONFIG_WANPORT2 */

	return 0;
}
#else	/* !RTCONFIG_WANRED_LED */
static inline int update_wan_led_and_wanred_led(int wan_unit) { return 0; }
#endif	/* RTCONFIG_WANRED_LED */

#if defined(RTCONFIG_FAILOVER_LED)
int update_failover_led(void)
{
	enum led_fan_mode_id v = LED_OFF;

	/* If dual-wan is not enabled or is not fail-over mode,
	 * always turn off fail-over led.
	 */
	if (get_dualwan_by_unit(1) == WANS_DUALWAN_IF_NONE ||
	    (!nvram_match("wans_mode", "fo") && !nvram_match("wans_mode", "fb")))
	{
		failover_led_control(LED_OFF);
		return 0;
	}

	if (wan_primary_ifunit() == 1)
		v = LED_ON;

	failover_led_control(v);

	return 0;
}
#else
static inline int update_failover_led(void);
#endif

#if defined(RTCONFIG_LANWAN_LED)
int update_wan_leds(int wan_unit)
{
#if defined(RTCONFIG_WANRED_LED)
#ifdef RTCONFIG_WPS_ALLLED_BTN
	if (nvram_match("AllLED", "1"))
#endif
	update_wan_led_and_wanred_led(wan_unit);
#else	/* !RTCONFIG_WANRED_LED */
	/* Turn on/off WAN LED in accordance with link status of WAN port */
	if (link_wan[wan_unit]
#ifdef RTCONFIG_WPS_ALLLED_BTN
			&& nvram_match("AllLED", "1")
#endif
	) {
		led_control(LED_WAN, LED_ON);
	} else {
		if(nvram_get_int("link_internet") != 2)
			led_control(LED_WAN, LED_OFF);
	}
#endif	/* RTCONFIG_WANRED_LED */

	update_failover_led();

	return 0;
}
#endif	/* RTCONFIG_LANWAN_LED */

static void safe_leave(int signo){
	int i, ret;
	char tmp[100] = "";

	_dprintf("\n## wanduck.safeexit ##\n");

	for(i = WAN_UNIT_FIRST; i < WAN_UNIT_MAX; ++i){
		link_wan_nvname(i, tmp, sizeof(tmp));
		nvram_unset(tmp);
	}

	signal(SIGTERM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	FD_ZERO(&allset);
	close(http_sock);
	close(dns_sock);

	for(i = 0; i < MAX_USER && client[i].sfd < maxfd; ++i){
		if(client[i].sfd != -1){
			ret = close(client[i].sfd);
			_dprintf("## close %d: result=%d.\n", client[i].sfd, ret);
		}
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

	_dprintf("\n# Disable direct rule(exit wanduck)\n");

	rule_setup = 0;
	conn_changed_state[current_wan_unit] = CONNED; // for cleaning the redirect rules.

	nvram_set_int("nat_state", NAT_STATE_INITIALIZING);
	nat_state = start_nat_rules();

	remove(WANDUCK_PID_FILE);

	_dprintf("\n# return(exit wanduck)\n");
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

	nat_redirect_enable = nvram_get_int("nat_redirect_enable");

#ifdef RTCONFIG_WIRELESSREPEATER
	if(strlen(nvram_safe_get("wlc_ssid")) > 0)
		setAP = 1;
	else
		setAP = 0;
#endif

#ifdef RTCONFIG_DUALWAN
	snprintf(dualwan_mode, sizeof(dualwan_mode), "%s", nvram_safe_get("wans_mode"));
	snprintf(dualwan_wans, sizeof(dualwan_wans), "%s", nvram_safe_get("wans_dualwan"));

	memset(wandog_target, 0, sizeof(wandog_target));
	if(sw_mode == SW_MODE_ROUTER){
		wandog_enable = nvram_get_int("wandog_enable");
		scan_interval = nvram_get_int("wandog_interval");
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
			max_disconn_count[unit] = nvram_get_int("wandog_maxfail");
		wandog_delay = nvram_get_int("wandog_delay");

		if((!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb"))
				&& wandog_enable == 1
				){
			snprintf(wandog_target, sizeof(wandog_target), "%s", nvram_safe_get("wandog_target"));
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

	if(current_lan_unit < 0)
		snprintf(prefix_lan, sizeof(prefix_lan), "lan_");
	else
		snprintf(prefix_lan, sizeof(prefix_lan), "lan%d_", current_lan_unit);

	snprintf(current_lan_ifname, sizeof(current_lan_ifname), "%s", nvram_safe_get(strcat_r(prefix_lan, "ifname", nvram_name)));
	snprintf(current_lan_proto, sizeof(current_lan_proto), "%s", nvram_safe_get(strcat_r(prefix_lan, "proto", nvram_name)));
	snprintf(current_lan_ipaddr, sizeof(current_lan_ipaddr), "%s", nvram_safe_get(strcat_r(prefix_lan, "ipaddr", nvram_name)));
	snprintf(current_lan_netmask, sizeof(current_lan_netmask), "%s", nvram_safe_get(strcat_r(prefix_lan, "netmask", nvram_name)));
	snprintf(current_lan_gateway, sizeof(current_lan_gateway), "%s", nvram_safe_get(strcat_r(prefix_lan, "gateway", nvram_name)));
	snprintf(current_lan_dns, sizeof(current_lan_dns), "%s", nvram_safe_get(strcat_r(prefix_lan, "dns", nvram_name)));
	snprintf(current_lan_subnet, sizeof(current_lan_subnet), "%s", nvram_safe_get(strcat_r(prefix_lan, "subnet", nvram_name)));

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER)
	{
		wlc_state = nvram_get_int("wlc_state");
		got_notify = 1;
	}
#endif

	_dprintf("# wanduck: Got LAN(%d) information:\n", current_lan_unit);
	if(test_log){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER)
		{
			_dprintf("# wanduck:   ipaddr=%s.\n", current_lan_ipaddr);
			_dprintf("# wanduck:wlc_state=%d.\n", wlc_state);
		}
		else
#endif
		{
			_dprintf("# wanduck:   ifname=%s.\n", current_lan_ifname);
			_dprintf("# wanduck:    proto=%s.\n", current_lan_proto);
			_dprintf("# wanduck:   ipaddr=%s.\n", current_lan_ipaddr);
			_dprintf("# wanduck:  netmask=%s.\n", current_lan_netmask);
			_dprintf("# wanduck:  gateway=%s.\n", current_lan_gateway);
			_dprintf("# wanduck:      dns=%s.\n", current_lan_dns);
			_dprintf("# wanduck:   subnet=%s.\n", current_lan_subnet);
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

/* 67u,87u,3200: have each led on every port.
 * 88u,3100,5300: have one led to hint wan port but this led is the union of all ports
 * force led_on on usb modem case */

void enable_wan_led()
{
	int usb_wan = get_dualwan_by_unit(wan_primary_ifunit()) == WANS_DUALWAN_IF_USB ? 1:0;

	if(usb_wan) {
		switch (get_model()) {
#ifdef RTAC68U
			case MODEL_RTAC68U:
				if (!is_ac66u_v2_series())
					break;
#endif
			case MODEL_RTAC3200:
			case MODEL_RTAC87U:
				eval("et", "-i", "eth0", "robowr", "0", "0x18", "0x01ff");
				eval("et", "-i", "eth0", "robowr", "0", "0x1a", "0x01fe");
				break;

			case MODEL_RTAC5300:
			case MODEL_RTAC5300R:
			case MODEL_RTAC88U:
			case MODEL_RTAC3100:
				eval("et", "-i", "eth0", "robowr", "0", "0x18", "0x01ff");
				eval("et", "-i", "eth0", "robowr", "0", "0x1a", "0");
				break;
		}
	}

	eval("et", "-i", "eth0", "robowr", "0", "0x18", "0x01ff");
	eval("et", "-i", "eth0", "robowr", "0", "0x1a", "0x01ff");
}

void disable_wan_led()
{
	eval("et", "-i", "eth0", "robowr", "0", "0x18", "0x01fe");
	eval("et", "-i", "eth0", "robowr", "0", "0x1a", "0x01fe");
}

static void wan_led_control(int sig) {
#if 0
#if defined(RTAC68U) || defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300) || defined(RTAC5300R)
#ifdef RTAC68U
	if (!is_ac66u_v2_series())
		return;
#endif
	char buf[16];
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get("wans_dualwan"));
	if (strcmp(buf, "wan none") != 0){
		logmessage("DualWAN", "skip single wan wan_led_control()");
		return;
	}
#endif
#endif
#if defined(RTAC68U) ||  defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300) || defined(RTAC5300R) || defined(DSL_AC68U)
	if(nvram_match("AllLED", "1")
#ifdef RTAC68U
		&& is_ac66u_v2_series()
#endif
	) {
#if defined(RTAC68U) ||  defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300) || defined(RTAC5300R)
		if (rule_setup) {
			led_control(LED_WAN, LED_ON);
			disable_wan_led();
		} else {
			led_control(LED_WAN, LED_OFF);
			enable_wan_led();
		}
#elif defined(DSL_AC68U)
		if (rule_setup) {
			led_control(LED_WAN, LED_OFF);
		} else {
			led_control(LED_WAN, LED_ON);
		}
#endif
	}
#endif
}

/* Return values:
    -1: ping target is disabled
     0: ping target has failed
     1: ping target ok
*/
int do_ping_detect(int wan_unit)
{
#ifdef RTCONFIG_DUALWAN
	FILE *fp;
	char cmd[512];
	int count;
	int debug = nvram_get_int("ping_debug");

	/* Check for valid domain to avoid shell escaping */
	if (!is_valid_domainname(wandog_target))
		return -1;
	if (debug)
		_dprintf("%s: %s %s\n", __FUNCTION__, "check", wandog_target);

	snprintf(cmd, sizeof(cmd), "ping -c1 -w2 -s32 %s -Mdont '%s'",
		 nvram_get_int("ttl_spoof_enable") ? "" : "-t128", wandog_target);
	if ((fp = popen(cmd, "r")) != NULL) {
		while (fgets(cmd, sizeof(cmd), fp) != NULL) {
			if (sscanf(cmd, "%*s %*s transmitted, %d %*s received", &count) == 1 && count) {
				fclose(fp);
				if (debug)
					_dprintf("%s: %s %s\n", __FUNCTION__, wandog_target, "ok");
				return 1;
			}
		}
		fclose(fp);
	}

	if (debug)
		_dprintf("%s: %s %s\n", __FUNCTION__, wandog_target, "fail");
	//logmessage("WAN Connection", "ping target failed");
	return 0;
#else /* RTCONFIG_DUALWAN */
	return -1;
#endif
}

#ifdef DETECT_INTERNET_MORE
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

	snprintf(cmd, sizeof(cmd), "/sbin/tcpcheck %d", TCPCHECK_TIMEOUT);

	foreach(buf, dns_list, next){
		snprintf(cmd+strlen(cmd), sizeof(cmd)-strlen(cmd), " %s:53", buf);
	}

	snprintf(cmd, sizeof(cmd), "%s >>%s", cmd, DETECT_FILE);

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

	snprintf(prefix_wan, sizeof(prefix_wan), "wan%d_", wan_unit);

	snprintf(wan_dns, sizeof(wan_dns), "%s", nvram_safe_get(strcat_r(prefix_wan, "dns", nvram_name)));

	if(organize_tcpcheck_cmd(wan_dns, cmd, PATH_MAX) == NULL){
		_dprintf("wanduck: No tcpcheck cmd.\n");
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
#endif // DETECT_INTERNET_MORE

#include <sys/wait.h>
#include <resolv.h>

/* Return values:
    -1: dns probe is disabled
     0: dns probe has failed
     1: dns probe ok
*/
int do_dns_detect(int wan_unit)
{
	struct addrinfo hints, *res, *ai;
	union {
		struct in_addr in;
		struct in6_addr in6;
	} *addr, target;
	char word[64], *next, *host, *content;
	int timeout, size, ret, status, pipefd[2];
	int debug = nvram_get_int("dns_probe_debug");

	host = nvram_safe_get("dns_probe_host");
	content = nvram_safe_get("dns_probe_content");
	if (debug)
		_dprintf("%s: %s %s %s\n", __FUNCTION__, "check", host, content);

	/* Check for valid domain to avoid shell escaping */
	if (!is_valid_domainname(host) || *content == '\0')
		return -1;

	memset(&hints, 0, sizeof(hints));
#ifdef RTCONFIG_IPV6
	hints.ai_family = ipv6_enabled() ? AF_UNSPEC : AF_INET;
#else
	hints.ai_family = AF_INET;
#endif
	hints.ai_socktype = SOCK_STREAM;
	timeout = nvram_get_int("dns_probe_timeout") ? : 2;

	ret = -1;
	if (pipe(pipefd) < 0)
		goto error;

	switch (fork()) {
	case -1:
		close(pipefd[1]);
		goto error;
	case 0:
		/* child */
		close(pipefd[0]);
		break;
	default:
		/* parent */
		close(pipefd[1]);
		do {
			ret = read(pipefd[0], &status, sizeof(status));
		} while (ret < 0 && errno == EINTR);
		ret = (ret == sizeof(status)) ? status : -1;
	error:
		close(pipefd[0]);

		if (debug)
			_dprintf("%s: %s ret %d\n", __FUNCTION__, host, ret);

		//if (ret == 0)
		//	logmessage("WAN Connection", "DNS probe failed");
		return ret;
	}

	/* Restore signals */
	signal(SIGTERM, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGUSR1, SIG_DFL);
	signal(SIGUSR2, SIG_DFL);
	signal(SIGALRM, SIG_DFL);
	alarm(timeout);

	status = 0;

	/* keep using libc's resolver state
	res_init();
	*/
	if (getaddrinfo(host, NULL, &hints, &res) == 0) {
		for (ai = res; ai; ai = ai->ai_next) {
			if (ai->ai_family == AF_INET) {
				addr = (void *)&((struct sockaddr_in *)ai->ai_addr)->sin_addr;
				size = sizeof(addr->in);
#ifdef RTCONFIG_IPV6
			} else if (ai->ai_family == AF_INET6) {
				addr = (void *)&((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
				size = sizeof(addr->in6);
#endif
			} else
				continue;

			if (debug) {
				inet_ntop(ai->ai_family, addr, word, sizeof(word));
				_dprintf("%s: %s %s %s\n", __FUNCTION__, "resolve", host, word);
			}

			foreach(word, content, next) {
				if ((strcmp(word, "*") == 0) ||
				    (inet_pton(ai->ai_family, word, &target) > 0 && memcmp(addr, &target, size) == 0)) {
					status = 1;
					goto done;
				}
			}
		}
	done:
		freeaddrinfo(res);
	}

	do {
		ret = write(pipefd[1], &status, sizeof(status));
	} while (ret < 0 && errno == EINTR);

	_exit(ret != sizeof(status));
}

int delay_dns_response(int wan_unit)
{
	static int last = -1;
	static int fail = 0;
	int ret = do_dns_detect(wan_unit);
	int delay_round = nvram_get_int("dns_delay_round");
	int debug = nvram_get_int("dns_probe_debug");

	if (debug && ret != last && ret >= 0) {
		logmessage("WAN Connection", "DNS probe %s (%d/%d)",
			   ret ? "succeeded" : "failed", fail, delay_round);
	}

	if (ret > 0) {
		last = ret;
		fail = 0;
	} else if (ret < 0) {
		/* do nothing */
	} else if (fail++ < delay_round && last >= 0) {
		ret = last;
	} else
		last = ret;

	return ret;
}

int detect_internet(int wan_unit)
{
#ifdef DETECT_INTERNET_MORE
	char wan_ifname[16];
	unsigned long rx_packets, tx_packets;
#endif
	int link_internet;
	int dns_ret;
	char tmp[100], prefix[16];
	char wan_proto[16];

#ifdef DETECT_INTERNET_MORE
	snprintf(wan_ifname, sizeof(wan_ifname), "%s", get_wan_ifname(wan_unit));
#endif

	dns_ret = delay_dns_response(wan_unit);

	snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);
	snprintf(wan_proto, sizeof(wan_proto), "%s", nvram_safe_get(strcat_r(prefix, "proto", tmp)));

	if(!(dualwan_unit__nonusbif(wan_unit) &&
			(!strcmp(wan_proto, "pppoe") ||
					!strcmp(wan_proto, "pptp") ||
					!strcmp(wan_proto, "l2tp")) &&
			nvram_get_int(strcat_r(prefix, "ppp_echo", tmp)) == 2))
		dns_ret = 1;

	if(
#ifdef RTCONFIG_DUALWAN
			strcmp(dualwan_mode, "lb") &&
#endif
			!found_default_route(wan_unit)){
		link_internet = DISCONN;

		// fix the missed gateway sometimes.
		if(is_wan_connect(wan_unit))
			add_multi_routes();
	}
#ifdef DETECT_INTERNET_MORE
	else if(!get_packets_of_net_dev(wan_ifname, &rx_packets, &tx_packets) || rx_packets <= RX_THRESHOLD)
		link_internet = DISCONN;
	else if(!isFirstUse && (!dns_ret && !do_tcp_dns_detect(wan_unit) && !do_ping_detect(wan_unit)))
		link_internet = DISCONN;
#endif
#ifdef RTCONFIG_DUALWAN
	else if((!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb"))
			&& wandog_enable == 1 && !isFirstUse && !do_ping_detect(wan_unit)){
		link_internet = DISCONN;

		// avoid the nat rules had be applied by wan_up before wanduck.
		if(nvram_get_int("nat_state") == NAT_STATE_NORMAL)
			stop_nat_rules();
	}
#endif
	else if(!dns_ret)
		link_internet = DISCONN;
	else
		link_internet = CONNED;

	if(link_internet == DISCONN){
		if(nvram_get_int("web_redirect") & WEBREDIRECT_FLAG_NOINTERNET)
			nvram_set_int("link_internet", 1);
		else{
			nvram_set_int("link_internet", 2);
			link_internet = CONNED;
		}
		record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NO_INTERNET_ACTIVITY);
	}
	else{
		nvram_set_int("link_internet", 2);
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
			snprintf(task_file, sizeof(task_file), "/proc/%d/cmdline", pid);
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

unsigned long long get_wan_flow(int wan_unit){
	unsigned long long rx, tx, total;

	if(dualwan_unit__usbif(wan_unit)){
		rx = strtoull(nvram_safe_get("modem_bytes_rx"), NULL, 10);
		tx = strtoull(nvram_safe_get("modem_bytes_tx"), NULL, 10);

		total = rx+tx;
	}
	else{
		// TODO: Data limit for the ethernet connection.
#ifdef RTCONFIG_TRAFFIC_LIMITER
		total = (unsigned long long)traffic_limiter_get_realtime(wan_unit) * 1024 * 1024 * 1024;
		if(test_log) _dprintf("[TRAFFIC LIMITER] /tmp/tl%d_realtime = %lld\n", wan_unit, total);
#else
		total = 0;
#endif
	}

	return total;
}

int chk_proto(int wan_unit){
	int wan_sbstate = nvram_get_int(nvram_sbstate[wan_unit]);
	char prefix_wan[8], nvram_name[16], wan_proto[16];
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	char buff[128];
#ifdef RTCONFIG_USB_MODEM
	unsigned long long total = get_wan_flow(wan_unit);
#endif
#endif
#ifdef RTCONFIG_TRAFFIC_LIMITER
	char cmd[32];
#endif

	snprintf(prefix_wan, sizeof(prefix_wan), "wan%d_", wan_unit);

	snprintf(wan_proto, sizeof(wan_proto), "%s", nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	// had detected the DATA limit before.
	if(wan_sbstate == WAN_STOPPED_REASON_DATALIMIT){
		if(nvram_get_int("modem_sms_limit") == 1 && nvram_get_int("modem_sms_limit_send") == 0){
			snprintf(buff, sizeof(buff), "start_sendSMS limit");
			nvram_set("freeze_duck", "5");
			notify_rc(buff);
			nvram_set("modem_sms_limit_send", "1");
			// if send the limit SMS, the alert SMS is not needed.
			nvram_set("modem_sms_alert_send", "1");
		}

		disconn_case[wan_unit] = CASE_DATALIMIT;
		return DISCONN;
	}
	else
#endif
#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_HOTSPOT){
		if(current_state[wan_unit] == WAN_STATE_STOPPED) {
			if(wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR)
				disconn_case[wan_unit] = CASE_THESAMESUBNET;
			else disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_CONNECTING){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
	}
	else
#endif
	// Start chk_proto() in SW_MODE_ROUTER.
#ifdef RTCONFIG_USB_MODEM
	if (dualwan_unit__usbif(wan_unit)) {
		int case_fail = (strcmp(wan_proto, "dhcp") == 0) ? CASE_DHCPFAIL : CASE_PPPFAIL;
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		unsigned long long alert, limit;

		eval("/usr/sbin/modem_status.sh", "bytes");

		alert = strtoull(nvram_safe_get("modem_bytes_data_warning"), NULL, 10);
		limit = strtoull(nvram_safe_get("modem_bytes_data_limit"), NULL, 10);

		if(limit > 0 && total >= limit){
			if(current_state[wan_unit] != WAN_STATE_STOPPED || wan_sbstate != WAN_STOPPED_REASON_DATALIMIT){
				_dprintf("wanduck(%d): chk_proto() detect the data limit.\n", wan_unit);
				record_wan_state_nvram(wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_DATALIMIT, -1);
				current_state[wan_unit] = WAN_STATE_STOPPED;
				wan_sbstate = WAN_STOPPED_REASON_DATALIMIT;
			}

			if(nvram_get_int("modem_sms_limit") == 1 && nvram_get_int("modem_sms_limit_send") == 0){
				snprintf(buff, sizeof(buff), "start_sendSMS limit");
				nvram_set("freeze_duck", "5");
				notify_rc(buff);
				nvram_set("modem_sms_limit_send", "1");
				// if send the limit SMS, the alert SMS is not needed.
				nvram_set("modem_sms_alert_send", "1");
			}
		}

		if(alert > 0 && total >= alert){
			if(nvram_get_int("modem_sms_limit") == 1 && nvram_get_int("modem_sms_alert_send") == 0){
				snprintf(buff, sizeof(buff), "start_sendSMS alert");
				nvram_set("freeze_duck", "5");
				notify_rc(buff);
				nvram_set("modem_sms_alert_send", "1");
			}
		}
#endif

		if(current_state[wan_unit] == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = case_fail;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_CONNECTING){
			ppp_fail_state = pppstatus(wan_unit);

			if(ppp_fail_state != WAN_STOPPED_REASON_NONE)
				record_wan_state_nvram(wan_unit, -1, ppp_fail_state, -1);

			disconn_case[wan_unit] = case_fail;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = case_fail;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_STOPPED){
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			if(wan_sbstate == WAN_STOPPED_REASON_DATALIMIT)
				disconn_case[wan_unit] = CASE_DATALIMIT;
			else
#endif
			if(wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR)
				disconn_case[wan_unit] = CASE_THESAMESUBNET;
			else
				disconn_case[wan_unit] = case_fail;

			return DISCONN;
		}
	}
	else
#endif // RTCONFIG_USB_MODEM
	if(!strcmp(wan_proto, "dhcp")
			|| !strcmp(wan_proto, "static")){
#ifdef RTCONFIG_TRAFFIC_LIMITER
		traffic_limiter_limitdata_check();

		// TODO: Data limit for the ethernet connection.
		if(traffic_limiter_wanduck_check(wan_unit)){
			if(current_state[wan_unit] != WAN_STATE_STOPPED || wan_sbstate != WAN_STOPPED_REASON_DATALIMIT){
				_dprintf("wanduck(%d): chk_proto() detect the data limit - dhcp / static.\n", wan_unit);
				record_wan_state_nvram(wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_DATALIMIT, -1);
				current_state[wan_unit] = WAN_STATE_STOPPED;
				wan_sbstate = WAN_STOPPED_REASON_DATALIMIT;

				/* stop_wan_if() */
				snprintf(cmd, sizeof(cmd), "stop_wan_if %d", wan_unit);
				notify_rc(cmd);
			}
		}
#endif

		if(current_state[wan_unit] == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_CONNECTING){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = CASE_DHCPFAIL;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_STOPPED) {
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			if(wan_sbstate == WAN_STOPPED_REASON_DATALIMIT)
				disconn_case[wan_unit] = CASE_DATALIMIT;
			else
#endif
			if(wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR)
				disconn_case[wan_unit] = CASE_THESAMESUBNET;
			else
				disconn_case[wan_unit] = CASE_DHCPFAIL;

			return DISCONN;
		}
	}
	else if(!strcmp(wan_proto, "pppoe")
			|| !strcmp(wan_proto, "pptp")
			|| !strcmp(wan_proto, "l2tp")
			){
		ppp_fail_state = pppstatus(wan_unit);
#ifdef RTCONFIG_TRAFFIC_LIMITER
		traffic_limiter_limitdata_check();

		// TODO: Data limit for the ethernet connection.
		if(traffic_limiter_wanduck_check(wan_unit)){
			if(current_state[wan_unit] != WAN_STATE_STOPPED || wan_sbstate != WAN_STOPPED_REASON_DATALIMIT){
				_dprintf("wanduck(%d): chk_proto() detect the data limit - pppoe / pptp / l2tp.\n", wan_unit);
				record_wan_state_nvram(wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_DATALIMIT, -1);
				current_state[wan_unit] = WAN_STATE_STOPPED;
				wan_sbstate = WAN_STOPPED_REASON_DATALIMIT;

				/* stop_wan_if() */
				snprintf(cmd, sizeof(cmd), "stop_wan_if %d", wan_unit);
				notify_rc(cmd);
			}
		}
#endif

		if(current_state[wan_unit] != WAN_STATE_CONNECTED
				&& ppp_fail_state == WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
			// PPP is into the idle mode.
			if(current_state[wan_unit] == WAN_STATE_STOPPED) // Sometimes ip_down() didn't set it.
				record_wan_state_nvram(wan_unit, -1, WAN_STOPPED_REASON_PPP_LACK_ACTIVITY, -1);
		}
		else if(current_state[wan_unit] == WAN_STATE_INITIALIZING){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_CONNECTING){
			if(ppp_fail_state != WAN_STOPPED_REASON_NONE)
				record_wan_state_nvram(wan_unit, -1, ppp_fail_state, -1);

			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_DISCONNECTED){
			disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
		else if(current_state[wan_unit] == WAN_STATE_STOPPED){
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			if(wan_sbstate == WAN_STOPPED_REASON_DATALIMIT)
				disconn_case[wan_unit] = CASE_DATALIMIT;
			else
#endif
				disconn_case[wan_unit] = CASE_PPPFAIL;
			return DISCONN;
		}
	}

	return CONNED;
}

int if_wan_phyconnected(int wan_unit){
	char *ptr, wired_link_nvram[16];
#ifdef RTCONFIG_WIRELESSREPEATER
	int link_ap = 0;
#endif
	int link_changed = 0;
	int link_internet = -1;
	char tmp[100], prefix[32];
	char wan_proto[16];
#ifdef RTCONFIG_USB_MODEM
	int find_modem_node = 0;
	int wan_state = 0;
	int sim_state = 0;
	char modem_type[32];
#ifdef RTCONFIG_INTERNAL_GOBI
	char usb_if[16];
	static int got_modem_info = 0;
	char buf[32];
#endif

#ifdef RTCONFIG_DYN_MODEM
#ifdef RTCONFIG_DUALWAN
	if(wan_unit == WAN_UNIT_FIRST && dualwan_unit__nonusbif(wan_unit) && get_dualwan_by_unit(other_wan_unit) == WANS_DUALWAN_IF_NONE){
		snprintf(prefix, sizeof(prefix), "wan%d_", other_wan_unit);

		find_modem_node = 0;
		wan_state = nvram_get_int(nvram_state[other_wan_unit]);

#ifdef RTCONFIG_INTERNAL_GOBI
		if(strlen(usb_if) <= 0 && nvram_get_int("usb_gobi") && !strcmp(nvram_safe_get(strcat_r(prefix2, "act_type", tmp2)), "gobi")){
			snprintf(usb_if, sizeof(usb_if), "%s", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
_dprintf("wanduck: try to get usb_if=%s.\n", usb_if);
		}

		if(strlen(usb_if) > 0){
			if(strlen(nvram_safe_get("usb_modem_act_imei")) <= 0)
				eval("/usr/sbin/modem_status.sh", "imei");
			if(got_modem_info < 3
#ifdef CONFIG_BCMWL5
					&& !factory_debug()
#else
					&& !IS_ATE_FACTORY_MODE()
#endif
					&& strlen(nvram_safe_get("usb_modem_act_hwver")) <= 0){
				++got_modem_info;
				eval("/usr/sbin/modem_status.sh", "hwver");
			}
			if(strlen(nvram_safe_get("usb_modem_act_swver")) <= 0)
				eval("/usr/sbin/modem_status.sh", "swver");
		}
#endif

		link_wan[other_wan_unit] = is_usb_modem_ready();
		if(link_wan[other_wan_unit]){
			snprintf(modem_type, sizeof(modem_type), "%s", nvram_safe_get("usb_modem_act_type"));
			if(strlen(modem_type) <= 0){
				eval("/usr/sbin/find_modem_type.sh");
				snprintf(modem_type, sizeof(modem_type), "%s", nvram_safe_get("usb_modem_act_type"));
			}
		}

		if(!nvram_get("usb_modem_act_sim"))
			sim_state = 100; // 100: didn't detect the SIM status yet.
		else
			sim_state = nvram_get_int("usb_modem_act_sim");

		if(!strcmp(modem_type, "tty") || !strcmp(modem_type, "mbim") || !strcmp(modem_type, "qmi") || !strcmp(modem_type, "gobi")){
			if(link_wan[other_wan_unit] == 1){
				if(!strcmp(nvram_safe_get("usb_modem_act_int"), "")){
					if(!strcmp(modem_type, "qmi")){	// e.q. Huawei E398.
						_dprintf("wanduck(%d): Sleep 3 seconds to wait modem nodes.\n", other_wan_unit);
						sleep(3);
					}
#if 0
					else{
						_dprintf("wanduck(%d): Sleep 2 seconds to wait modem nodes.\n", other_wan_unit);
						sleep(2);
					}
#endif

					wan_state = nvram_get_int(nvram_state[other_wan_unit]); // after sleep(), wan_state is changed.

					if(!strcmp(nvram_safe_get("usb_modem_act_int"), ""))
						find_modem_node = 1;
				}

#if 0
				if((!strcmp(modem_type, "tty") || !strcmp(modem_type, "mbim"))
						&& !strcmp(nvram_safe_get("usb_modem_act_bulk"), "")
						&& strcmp(nvram_safe_get("usb_modem_act_vid"), "6610") // e.q. ZTE MF637U
						){
					_dprintf("wanduck(%d): finding the second tty node...\n", other_wan_unit);
					find_modem_node = 1;
				}
#endif

				if(find_modem_node)
					eval("/usr/sbin/find_modem_node.sh");

#if 0
				if(!strcmp(modem_type, "tty") && !strcmp(nvram_safe_get("usb_modem_act_vid"), "6610")){ // e.q. ZTE MF637U
					if(wan_state == WAN_STATE_INITIALIZING && sim_state <= 0)
						eval("/usr/sbin/modem_status.sh", "sim");
				}
				else
#endif
				if(wan_state != WAN_STATE_CONNECTING || sim_state == 100){
					if(!strcmp(modem_type, "gobi"))
						eval("/usr/sbin/modem_status.sh", "sim");
					else if(sim_state == 100 || sim_state == -2) // QMI
						eval("/usr/sbin/modem_status.sh", "sim");
				}

				sim_state = nvram_get_int("usb_modem_act_sim");
				if(sim_state == 2 || sim_state == 3){
					sim_lock = 1;

					if(sim_state == 3 || !strcmp(nvram_safe_get("modem_pincode"), "") || nvram_get_int(nvram_auxstate[other_wan_unit]) == WAN_STOPPED_REASON_PINCODE_ERR)
						link_wan[other_wan_unit] = 3;
				}
				else if(sim_state != 1)
					link_wan[other_wan_unit] = 0;

#ifdef RTCONFIG_INTERNAL_GOBI
				if(sim_state > 1 && sim_state <= 6 && !strcmp(nvram_safe_get("usb_modem_act_auth"), ""))
					eval("/usr/sbin/modem_status.sh", "simauth");
#endif
			}
		}

		nvram_set_int(strcat_r(prefix, "is_usb_modem_ready", tmp), link_wan[other_wan_unit]);
if(test_log)
_dprintf("# wanduck: if_wan_phyconnected: x_Setting=%d, link_modem=%d, sim_state=%d.\n", !isFirstUse, link_wan[other_wan_unit], sim_state);

		link_wan_nvname(other_wan_unit, wired_link_nvram, sizeof(wired_link_nvram));
		if((ptr = nvram_get(wired_link_nvram)) == NULL || strlen(ptr) <= 0 || link_wan[other_wan_unit] != atoi(ptr)){
			nvram_set_int(wired_link_nvram, link_wan[other_wan_unit]);
			if(link_wan[other_wan_unit] != 0)
				record_wan_state_nvram(other_wan_unit, -1, -1, WAN_AUXSTATE_NONE);
		}

		disconn_case[other_wan_unit] = CASE_DYN_MODEM;
	}
#endif	/* RTCONFIG_DUALWAN */
#endif	/* RTCONFIG_DYN_MODEM */

	if(dualwan_unit__usbif(wan_unit)){
		snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);

		find_modem_node = 0;
		wan_state = nvram_get_int(nvram_state[wan_unit]);

		// need to check before detecting SIM. If not, the detect of wanduck will be blocked.
		if(nvram_get_int("usb_modem_act_scanning") != 0){
			_dprintf("wanduck(%d): detect the USB scan.\n", wan_unit);
			link_wan[wan_unit] = 4;
			sim_lock = 1;
			modem_type[0] = '\0';
		}
		else{
			modem_act_reset = nvram_get_int("usb_modem_act_reset");
			if(modem_act_reset == 1 || modem_act_reset == 2)
				return CONNED;

			// need to see the link status of modem anyway.
			link_wan[wan_unit] = is_usb_modem_ready();

			if(wan_state == WAN_STATE_CONNECTING)
				return CONNED;

			if(link_wan[wan_unit]){
				snprintf(modem_type, sizeof(modem_type), "%s", nvram_safe_get("usb_modem_act_type"));
				if(strlen(modem_type) <= 0){
					eval("/usr/sbin/find_modem_type.sh");
					snprintf(modem_type, sizeof(modem_type), "%s", nvram_safe_get("usb_modem_act_type"));
				}
			}
		}

#ifdef RTCONFIG_INTERNAL_GOBI
		if(link_wan[wan_unit] && !strcmp(modem_type, "gobi") && nvram_get_int("usb_gobi"))
			snprintf(usb_if, sizeof(usb_if), "%s", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
		else
			usb_if[0] = '\0';

		if(strlen(usb_if) > 0 && !got_modem_info){
			if(strlen(nvram_safe_get("usb_modem_act_imei")) <= 0)
				eval("/usr/sbin/modem_status.sh", "imei");
			if(got_modem_info < 3
#ifdef CONFIG_BCMWL5
					&& !factory_debug()
#else
					&& !IS_ATE_FACTORY_MODE()
#endif
					&& strlen(nvram_safe_get("usb_modem_act_hwver")) <= 0){
				++got_modem_info;
				eval("/usr/sbin/modem_status.sh", "hwver");
			}
			if(strlen(nvram_safe_get("usb_modem_act_swver")) <= 0)
				eval("/usr/sbin/modem_status.sh", "swver");
#ifdef RTCONFIG_USB_SMS_MODEM
			if(strlen(nvram_safe_get("usb_modem_act_smsc")) <= 0)
				eval("/usr/sbin/modem_status.sh", "smsc");
#endif
		}
#endif // RTCONFIG_INTERNAL_GOBI

		if(!nvram_get("usb_modem_act_sim"))
			sim_state = 100; // 100: didn't detect the SIM status yet.
		else
			sim_state = nvram_get_int("usb_modem_act_sim");

		if(!strcmp(modem_type, "tty") || !strcmp(modem_type, "mbim") || !strcmp(modem_type, "qmi") || !strcmp(modem_type, "gobi")){
			if(link_wan[wan_unit] == 1){
				if(!strcmp(nvram_safe_get("usb_modem_act_int"), "")){
					if(!strcmp(modem_type, "qmi")){	// e.q. Huawei E398.
						_dprintf("wanduck(%d): Sleep 3 seconds to wait modem nodes.\n", wan_unit);
						sleep(3);
					}
#if 0
					else{
						_dprintf("wanduck(%d): Sleep 2 seconds to wait modem nodes.\n", wan_unit);
						sleep(2);
					}
#endif

					wan_state = nvram_get_int(nvram_state[wan_unit]); // after sleep(), wan_state is changed.

					if(!strcmp(nvram_safe_get("usb_modem_act_int"), ""))
						find_modem_node = 1;
				}

#if 0
				if((!strcmp(modem_type, "tty") || !strcmp(modem_type, "mbim"))
						&& !strcmp(nvram_safe_get("usb_modem_act_bulk"), "")
						&& strcmp(nvram_safe_get("usb_modem_act_vid"), "6610") // e.q. ZTE MF637U
						){
					_dprintf("wanduck(%d): finding the second tty node...\n", wan_unit);
					find_modem_node = 1;
				}
#endif

				if(find_modem_node)
					eval("/usr/sbin/find_modem_node.sh");

#if 0
				if(!strcmp(modem_type, "tty") && !strcmp(nvram_safe_get("usb_modem_act_vid"), "6610")){ // e.q. ZTE MF637U
					if(wan_state == WAN_STATE_INITIALIZING && sim_state <= 0)
						eval("/usr/sbin/modem_status.sh", "sim");
				}
				else
#endif
				if(wan_state != WAN_STATE_CONNECTING || sim_state == 100){
					if(!strcmp(modem_type, "gobi"))
						eval("/usr/sbin/modem_status.sh", "sim");
					else if(sim_state == 100 || sim_state == -2)
						eval("/usr/sbin/modem_status.sh", "sim");
				}

				sim_state = nvram_get_int("usb_modem_act_sim");
				if(sim_state == 2 || sim_state == 3){
					sim_lock = 1;

					if(sim_state == 3 || !strcmp(nvram_safe_get("modem_pincode"), "") || nvram_get_int(nvram_auxstate[wan_unit]) == WAN_STOPPED_REASON_PINCODE_ERR)
						link_wan[wan_unit] = 3;
				}
				else if(sim_state != 1)
					link_wan[wan_unit] = 0;

#ifdef RTCONFIG_INTERNAL_GOBI
				if(sim_state > 1 && sim_state <= 6 && !strcmp(nvram_safe_get("usb_modem_act_auth"), ""))
					eval("/usr/sbin/modem_status.sh", "simauth");

				if(sim_state == 1){
					char act_ip[32];

					eval("/usr/sbin/modem_status.sh", "ip");
					snprintf(act_ip, sizeof(act_ip), "%s", nvram_safe_get("usb_modem_act_ip"));

					snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)));
					if(strcmp(buf, "") && strcmp(buf, "0.0.0.0")
							&& strcmp(act_ip, "") && strcmp(act_ip, "0.0.0.0")
							&& strcmp(buf, act_ip)){
						_dprintf("wanduck: renew IP...(%s)\n", act_ip);
						logmessage("wanduck", "renew IP...(%s)\n", act_ip);
						nvram_set(tmp, "0.0.0.0");
						snprintf(tmp, sizeof(tmp), "/var/run/udhcpc%d.pid", wan_unit);
						kill_pidfile_s(tmp, SIGUSR1);
					}

					if(is_wan_connect(wan_unit))
#if 1 // +CGCELLI seems to cause the Input/Output errors of ttyACM.
						eval("/usr/sbin/modem_status.sh", "fullsignal");
#else
						eval("/usr/sbin/modem_status.sh", "signal");
#endif
#if 0
					// from the newest GUI, the signal don't be shown without SIM.
					else
						eval("/usr/sbin/modem_status.sh", "signal");
#endif
				}
#endif
			}
		}

		nvram_set_int(strcat_r(prefix, "is_usb_modem_ready", tmp), link_wan[wan_unit]);
if(test_log)
_dprintf("# wanduck(%d): if_wan_phyconnected: x_Setting=%d, link_modem=%d, sim_state=%d.\n", wan_unit, !isFirstUse, link_wan[wan_unit], sim_state);

		update_wan_leds(wan_unit);

		link_wan_nvname(wan_unit, wired_link_nvram, sizeof(wired_link_nvram));
		if((ptr = nvram_get(wired_link_nvram)) == NULL || strlen(ptr) <= 0 || link_wan[wan_unit] != atoi(ptr)){
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
		snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);
		snprintf(wan_proto, sizeof(wan_proto), "%s", nvram_safe_get(strcat_r(prefix, "proto", tmp)));

		// check wan port.
		link_wan[wan_unit] = get_wanports_status(wan_unit);

		if (get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_WAN
#if defined(RTCONFIG_WANPORT2)
		    || get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_WAN2
#endif
#if defined(RTCONFIG_WANRED_LED)
		    || get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_LAN
#endif
		   )
		{
			update_wan_leds(wan_unit);
		}

		link_wan_nvname(wan_unit, wired_link_nvram, sizeof(wired_link_nvram));
		if((ptr = nvram_get(wired_link_nvram)) == NULL || strlen(ptr) <= 0 || link_wan[wan_unit] != atoi(ptr)){
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
		if(!link_wan[wan_unit]){
			record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);
		}
	}

#ifdef RTCONFIG_LANWAN_LED
	if(get_lanports_status()
#ifdef RTCONFIG_WPS_ALLLED_BTN
			&& nvram_match("AllLED", "1")
#endif
	)
		led_control(LED_LAN, LED_ON);
	else led_control(LED_LAN, LED_OFF);
#endif

#ifdef RTCONFIG_LAN4WAN_LED
	LanWanLedCtrl();
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
	// check if set AP.
	if(sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT)
	{
		link_ap = (wlc_state == WLC_STATE_CONNECTED);
		if(link_ap != nvram_get_int("link_ap"))
			nvram_set_int("link_ap", link_ap);
	}
#endif

	link_internet = nvram_get_int("link_internet");

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

					_dprintf("wanduck(%d): modem PHY_RECONN.\n", wan_unit);
					return PHY_RECONN;
				}
				else{
					record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);
					_dprintf("wanduck(%d): SIM or modem is pulled off.\n", wan_unit);
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
					_dprintf("wanduck(%d): static PHY_RECONN.\n", wan_unit);
					return PHY_RECONN;
				}
				else if(!strcmp(wan_proto, "dhcp")){
					disconn_case[wan_unit] = CASE_DHCPFAIL;
					_dprintf("wanduck(%d): dhcp PHY_RECONN.\n", wan_unit);
					return PHY_RECONN;
				}
			}
		}
		else if(!link_wan[wan_unit]){
#ifdef RTCONFIG_DUALWAN
			if(strcmp(dualwan_mode, "lb"))
#endif
			{
				nvram_set_int("link_internet", 1);
				link_internet = 1;
			}

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
	else if(sw_mode == SW_MODE_AP)
	{
#if 0
		if(!link_wan[wan_unit]){
			// ?: type error?
			nvram_set_int("link_internet", 1);
			link_internet = 1;

			record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);

			if((nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)){
				disconn_case[wan_unit] = CASE_DISWAN;
				return DISCONN;
			}
		}
#else
		if(link_internet != 2){
			nvram_set_int("link_internet", 2);
			link_internet = 2;
		}

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
			if(link_internet != 1){
				nvram_set_int("link_internet", 1);
				link_internet = 1;
			}

			if(sw_mode == SW_MODE_HOTSPOT)
				record_wan_state_nvram(wan_unit, -1, -1, WAN_AUXSTATE_NOPHY);

			disconn_case[wan_unit] = CASE_AP_DISCONN;
			return DISCONN;
		}
		else if(sw_mode == SW_MODE_REPEATER)
		{
			if(nvram_match("lan_proto", "dhcp") && nvram_get_int("lan_state_t") != LAN_STATE_CONNECTED){
				if(link_internet != 1){
					nvram_set_int("link_internet", 1);
					link_internet = 1;
				}

				return DISCONN;
			}
			else{
				if(link_internet != 2){
					nvram_set_int("link_internet", 2);
					link_internet = 2;
				}

				return CONNED;
			}
		}
	}
#endif

	return CONNED;
}

int if_wan_connected(int wan_unit){
	if(chk_proto(wan_unit) != CONNED)
		return DISCONN;
	else if(wan_unit == wan_primary_ifunit()
#ifdef RTCONFIG_DUALWAN
			|| !strcmp(dualwan_mode, "lb")
#endif
			)
		return detect_internet(wan_unit);

	return CONNED;
}

void handle_wan_line(int wan_unit, int action){
	char cmd[32];
	char prefix_wan[8], nvram_name[16], wan_proto[16];

	// Redirect rules.
	if(action){
_dprintf("nat_rule: stop_nat_rules 3.\n");
		nat_state = stop_nat_rules();
	}
	/*
	 * When C2C and remove the redirect rules,
	 * it means dissolve the default state.
	 */
	else if(conn_changed_state[wan_unit] == D2C || conn_changed_state[wan_unit] == CONNED){
_dprintf("nat_rule: start_nat_rules 3.\n");
		nat_state = start_nat_rules();

		snprintf(prefix_wan, sizeof(prefix_wan), "wan%d_", wan_unit);

		snprintf(wan_proto, sizeof(wan_proto), "%s", nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

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
		nat_state = stop_nat_rules();

		_dprintf("\n# wanduck(%d): Try to restart_wan_if.\n", action);
		snprintf(cmd, sizeof(cmd), "restart_wan_if %d", wan_unit);
		notify_rc_and_wait(cmd);
#if RTCONFIG_MULTICAST_IPTV
		if (nvram_get_int("switch_stb_x") > 6) {
			int unit;
			for (unit = WAN_UNIT_IPTV; unit < WAN_UNIT_MULTICAST_IPTV_MAX; unit++) {
				snprintf(cmd, sizeof(cmd), "restart_wan_if %d", unit);
				notify_rc_and_wait(cmd);
			}
		}
#endif
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
		_dprintf("Fail to socket for httpd port: %s.\n", http_port);
		return -1;
	}

	if((*dd = passivesock(dns_port, IPPROTO_UDP, 10)) < 0){
		_dprintf("Fail to socket for DNS port: %s.\n", dns_port);
		return -1;
	}

	return 0;
}

void send_page(int wan_unit, int sfd, char *file_dest, char *url){
	char buf[2*MAXLINE];
	time_t now;
	char timebuf[100];
	char dut_addr[64];
	char dut_proto[16];
	char dut_port[5];
	char redirection[100];

	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

#ifdef NO_IOS_DETECT_INTERNET
	// disable iOS popup window. Jerry5 2012.11.27
	if (!strcmp(url,"www.apple.com/library/test/success.html") && nvram_get_int("disiosdet") == 1){
		snprintf(buf, sizeof(buf), "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", "HTTP/1.0 200 OK\r\n", "Server: Apache/2.2.3 (Oracle)\r\n", "Content-Type: text/html; charset=UTF-8\r\n", "Cache-Control: max-age=557\r\n", "Expires: ", timebuf, "\r\n", "Date: ", timebuf, "\r\n", "Content-Length: 127\r\n", "Connection: close\r\n\r\n", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n", "<HTML>\n", "<HEAD>\n\t", "<TITLE>Success</TITLE>\n", "</HEAD>\n", "<BODY>\n", "Success\n", "</BODY>\n", "</HTML>\n");
	}
	else
#endif
	{
	snprintf(buf, sizeof(buf), "%s%s%s%s%s", "HTTP/1.0 302 Moved Temporarily\r\n", "Server: wanduck\r\n", "Date: ", timebuf, "\r\n");

#ifdef RTCONFIG_WIRELESSREPEATER
	if(sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT)
		snprintf(dut_addr, sizeof(dut_addr), "%s", DUT_DOMAIN_NAME);
	else
#endif
	{
		if ((isFirstUse) || (nvram_get_int("http_dut_redir") == 1))
			snprintf(dut_addr, sizeof(dut_addr), "%s", DUT_DOMAIN_NAME);
		else
			snprintf(dut_addr, sizeof(dut_addr), "%s", nvram_safe_get("lan_ipaddr"));
	}
#ifdef RTCONFIG_HTTPS
	if (nvram_get_int("http_enable") == 1) {
		snprintf(dut_proto, sizeof(dut_proto), "https://");
		snprintf(dut_port, sizeof(dut_port), "%d", nvram_get_int("https_lanport") ? : 443);
	} else
#endif
	{
		snprintf(dut_proto, sizeof(dut_proto), "http://");
		snprintf(dut_port, sizeof(dut_port), "%d", /*nvram_get_int("http_lanport") ? :*/ 80);
	}

	if(strstr(url, "hotspot-detect") || strstr(url, "generate_204")){
		snprintf(redirection, sizeof(redirection), "%s%s%s", "Location:", dut_proto, dut_addr);
	}
	else{
		//snprintf(redirection, sizeof(redirection), "%s%s%s%s%s", "refresh:1.0001; url=", dut_proto, dut_addr, ":", dut_port);
		snprintf(redirection, sizeof(redirection), "%s%s%s%s%s", "Location:", dut_proto, dut_addr, ":", dut_port);
	}

	// TODO: Only send pages for the wan(0)'s state.
	if(isFirstUse){
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT)
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s%s%s%s", "Connection: close\r\n", redirection, "/QIS_default.cgi?flag=sitesurvey", "\r\nContent-Type: text/html\r\n");
		else
#endif
#ifdef RTCONFIG_TMOBILE
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s%s%s%s", "Connection: close\r\n", redirection, "/MobileQIS_Login.asp", "\r\nContent-Type: text/html\r\n");
#else
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s%s%s%s", "Connection: close\r\n", redirection, "/QIS_default.cgi?flag=welcome", "\r\nContent-Type: text/html\r\n");
#endif
	}
	else if(conn_changed_state[wan_unit] == C2D || conn_changed_state[wan_unit] == DISCONN){
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		if(disconn_case[wan_unit] == CASE_DATALIMIT)
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s%s%s%d%s", "Connection: close\r\n", redirection, "/blocking.asp?flag=", disconn_case[wan_unit], "\r\nContent-Type: text/html\r\n");
		else
#endif
#ifdef RTCONFIG_USB_MODEM
#ifdef RTCONFIG_DYN_MODEM
		if(wan_unit == WAN_UNIT_FIRST && dualwan_unit__nonusbif(wan_unit) && get_dualwan_by_unit(other_wan_unit) == WANS_DUALWAN_IF_NONE && link_wan[other_wan_unit])
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s%s%s%d%s", "Connection: close\r\n", redirection, "/error_page.htm?flag=", disconn_case[other_wan_unit], "\r\nContent-Type: text/html\r\n");
		else
#endif
#endif
		if(disconn_case[wan_unit] == CASE_THESAMESUBNET)
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s%s%s%d%s", "Connection: close\r\n", redirection, "/error_page.htm?flag=", disconn_case[wan_unit], "\r\nContent-Type: text/html\r\n");
		else
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s%s%s%d%s", "Connection: close\r\n", redirection, "/error_page.htm?flag=", disconn_case[wan_unit], "\r\nContent-Type: text/html\r\n");
	}
#ifdef RTCONFIG_WIRELESSREPEATER
	else
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s%s%s/%s", "Connection: close\r\n", redirection, "index.asp", "\r\nContent-Type: text/html\r\n");
#endif

	}

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

	snprintf(dst_url, sizeof(dst_url), "%s/%s", host, dest);
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

#ifdef RTCONFIG_DUALWAN
	if(!strcmp(dualwan_mode, "lb") || !strcmp(dualwan_mode, "fb"))
		snprintf(log_title, sizeof(log_title), "WAN(%d) Connection", wan_unit);
	else
#endif
		snprintf(log_title, sizeof(log_title), "WAN Connection");

	snprintf(prefix_wan, sizeof(prefix_wan), "wan%d_", wan_unit);

	snprintf(wan_proto, sizeof(wan_proto), "%s", nvram_safe_get(strcat_r(prefix_wan, "proto", nvram_name)));

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

			if(ppp_fail_state == WAN_STOPPED_REASON_PPP_AUTH_FAIL){
				logmessage(log_title, "Failed to authenticate ourselves to peer.");
			}
			else if(ppp_fail_state == WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
				logmessage(log_title, "Terminating connection due to lack of activity.");
			}
			else if(ppp_fail_state == WAN_STOPPED_REASON_PPP_NO_RESPONSE){
				logmessage(log_title, "No response from ISP.");
			}
			else{
				logmessage(log_title, "Fail to connect with some issues.");
			}
		}
		else if(disconn_case[wan_unit] == CASE_DHCPFAIL){
			if(disconn_case_old[wan_unit] == CASE_DHCPFAIL)
				return;
			disconn_case_old[wan_unit] = CASE_DHCPFAIL;

			if(!strcmp(wan_proto, "dhcp")
#ifdef RTCONFIG_WIRELESSREPEATER
					|| sw_mode == SW_MODE_HOTSPOT
#endif
					){
				logmessage(log_title, "ISP's DHCP did not function properly.");
			}
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

#ifdef RTCONFIG_TMOBILE
			logmessage(log_title, "reset LAN subnet and dhcp pool.");
			if(!strncmp(nvram_safe_get("lan_ipaddr"), "192.168.29.", 11))
			{
				nvram_set("lan_ipaddr"   , "192.168.24.1");
				nvram_set("lan_ipaddr_rt", "192.168.24.1");
				nvram_set("dhcp_start"   , "192.168.24.100");
				nvram_set("dhcp_end"     , "192.168.24.254");
			} else {
				nvram_set("lan_ipaddr"   , "192.168.29.1");
				nvram_set("lan_ipaddr_rt", "192.168.29.1");
				nvram_set("dhcp_start"   , "192.168.29.100");
				nvram_set("dhcp_end"     , "192.168.29.254");
			}
			nvram_commit();
			reboot(RB_AUTOBOOT);
#endif
		}
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		else if(disconn_case[wan_unit] == CASE_DATALIMIT){
			if(disconn_case_old[wan_unit] == CASE_DATALIMIT)
				return;
			disconn_case_old[wan_unit] = CASE_DATALIMIT;

			logmessage(log_title, "The Data is at limit.");
		}
#endif
		else{	// disconn_case[wan_unit] == CASE_OTHERS
			if(disconn_case_old[wan_unit] == CASE_OTHERS)
				return;
			disconn_case_old[wan_unit] = CASE_OTHERS;

			logmessage(log_title, "WAN was exceptionally disconnected.");
		}
	}
	else if(conn_changed_state[wan_unit] == D2C){
		if(disconn_case_old[wan_unit] == -1)
			return;
		disconn_case_old[wan_unit] = -1;

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
	char prefix[] = "wanXXXXXX_", tmp[100] = "";
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
	else if (dualwan_unit__usbif(wan_unit)) {
		if(!link_wan[wan_unit]) {
			snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);

			nvram_set_int(strcat_r(prefix, "is_usb_modem_ready", tmp), link_wan[wan_unit]);
			return 0; // No modem in USB ports.
		}
	}
#endif

	_dprintf("%s: wan(%d) Starting...\n", __FUNCTION__, wan_unit);
	// Set the modem to be running.
	set_wan_primary_ifunit(wan_unit);

#ifdef RTCONFIG_USB_MODEM
	if (nvram_invmatch("modem_enable", "4") && dualwan_unit__usbif(wan_unit)) {
		// Wait the PPP config file to be done.
		retry = 0;
		while((lock = file_lock("3g")) == -1 && retry < MAX_WAIT_FILE)
			sleep(1);

		if(lock == -1){
			_dprintf("(%d): No pppd conf file and turn off the state of USB Modem.\n", wan_unit);
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

			_dprintf("wanduck1: restart_wan_if %d.\n", unit);
			snprintf(cmd, sizeof(cmd), "restart_wan_if %d", unit);
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

			_dprintf("wanduck1: stop_wan_if %d.\n", unit);
			snprintf(cmd, sizeof(cmd), "stop_wan_if %d", unit);
			notify_rc_and_wait(cmd);
			sleep(1);
		}
	}
#endif

#if defined(RTCONFIG_IPV6) && defined(RTCONFIG_DUALWAN)
	//int ipv6_service = 0;
	/* Start each configured and enabled wan connection and its undelying i/f */
	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
		if (get_ipv6_service_by_unit(unit) != IPV6_DISABLED) {
			//ipv6_service = 1;
			break;
		}
#endif

	current_state[wan_unit] = get_wan_state(wan_unit);
	if(current_state[wan_unit] != WAN_STATE_CONNECTING && current_state[wan_unit] != WAN_STATE_CONNECTED && current_state[wan_unit] != WAN_STATE_DISABLED){
		snprintf(cmd, sizeof(cmd), "restart_wan_if %d", wan_unit);
		_dprintf("wanduck2: %s.\n", cmd);
		notify_rc_and_wait(cmd);
	}
	else if(strcmp(get_wan_ifname(wan_unit), "")){
		snprintf(cmd, sizeof(cmd), "restart_wan_line %d", wan_unit);
		_dprintf("wanduck2: %s.\n", cmd);
		notify_rc(cmd);
	}

#ifdef RTCONFIG_DUALWAN
	if(sw_mode == SW_MODE_ROUTER
			&& (!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb"))
			){
		delay_detect = 1;
	}
#endif

	_dprintf("%s: wan(%d) End.\n", __FUNCTION__, wan_unit);
	return 1;
}

int wanduck_main(int argc, char *argv[]){
	char *http_servport, *dns_servport;
	int clilen;
	struct sockaddr_in cliaddr;
	struct timeval  tval;
	int nready, maxi, sockfd;
	int wan_unit, wan_sbstate;
	char prefix_wan[8];
#ifdef RTCONFIG_WIRELESSREPEATER
	char domain_mapping[64];
#endif
#ifdef RTCONFIG_DSL
	int usb_switched_back_dsl = 0;
#endif
	unsigned int now;
#if defined(RTCONFIG_DUALWAN) || defined(RTCONFIG_USB_MODEM)
	char cmd[32];
#endif
#ifdef RTCONFIG_DUALWAN
#if defined(RTAC68U) ||  defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)|| defined(RTAC5300R)
	int wanred_led_status = 0;	/* 1 is no internet, 2 is internet ok */
	int u, link_status;
#endif
#endif
#ifdef RTCONFIG_QTN
	time_t qtn_now;
	struct tm *qtn_tm;
	char *time_string[14] = {0};
#endif
#ifdef RTCONFIG_USB_MODEM
	char tmp2[100];
	char modem_type[32];
#endif

	wdbg = nvram_get_int("wdbg");
	test_log = nvram_get_int("wanduck_debug");

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

	if(build_socket(http_servport, dns_servport, &http_sock, &dns_sock) < 0){
		_dprintf("\n*** Fail to build socket! ***\n");
		exit(0);
	}
	if(fcntl(dns_sock, F_SETFL, fcntl(dns_sock, F_GETFL, 0) | O_NONBLOCK) < 0){
		_dprintf("wanduck set dnssock [%d] nonblock fail !\n", dns_sock);
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
	modem_act_reset = 0;
	clilen = sizeof(cliaddr);

	snprintf(router_name, sizeof(router_name), "%s", DUT_DOMAIN_NAME);

	nvram_set_int("link_internet", 0);

#ifdef RTCONFIG_WIRELESSREPEATER
	nvram_set_int("link_ap", 2);
#endif

#if defined(RTCONFIG_DUALWAN) || defined(RTCONFIG_USB_MODEM)
	for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit)
#else
	wan_unit = WAN_UNIT_FIRST;
#endif
	{
		link_setup[wan_unit] = 0;
		link_wan[wan_unit] = 0;

		changed_count[wan_unit] = S_IDLE;
		disconn_case[wan_unit] = CASE_NONE;

		snprintf(prefix_wan, sizeof(prefix_wan), "wan%d_", wan_unit);

		strcat_r(prefix_wan, "state_t", nvram_state[wan_unit]);
		strcat_r(prefix_wan, "sbstate_t", nvram_sbstate[wan_unit]);
		strcat_r(prefix_wan, "auxstate_t", nvram_auxstate[wan_unit]);

		set_disconn_count(wan_unit, S_IDLE);
#ifdef RTCONFIG_USB_MODEM
		nvram_set_int(strcat_r(prefix_wan, "is_usb_modem_ready", tmp2), link_wan[wan_unit]);
#endif
	}

	loop_sec = uptime();

	get_related_nvram(); // initial the System's variables.
	get_lan_nvram(); // initial the LAN's variables.

	nat_redirect_enable_old = nat_redirect_enable;
	nat_state = nvram_get_int("nat_state");

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
				current_state[wan_unit] = nvram_get_int(nvram_state[wan_unit]);
#ifdef RTCONFIG_USB_MODEM
				if(dualwan_unit__usbif(wan_unit) && current_state[wan_unit] == WAN_STATE_INITIALIZING)
					conn_state[wan_unit] = DISCONN;
				else
#endif
					conn_state[wan_unit] = if_wan_connected(wan_unit);
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
			_dprintf("wanduck: delay %d seconds...\n", wandog_delay);
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
			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);
#ifdef RTCONFIG_USB_MODEM
			if(!(dualwan_unit__usbif(current_wan_unit) && current_state[current_wan_unit] == WAN_STATE_INITIALIZING))
#endif
				conn_state[current_wan_unit] = if_wan_connected(current_wan_unit);

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
#ifdef RTCONFIG_USB_MODEM
		other_wan_unit = get_next_unit(current_wan_unit);
#endif

		conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit);
		if(conn_state[current_wan_unit] == CONNED){
			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);
#ifdef RTCONFIG_USB_MODEM
			if(!(dualwan_unit__usbif(current_wan_unit) && current_state[current_wan_unit] == WAN_STATE_INITIALIZING))
#endif
				conn_state[current_wan_unit] = if_wan_connected(current_wan_unit);

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

		if(sw_mode == SW_MODE_AP)
		{
#ifdef RTCONFIG_REDIRECT_DNAME
			if(cross_state == DISCONN){
				_dprintf("\n# AP mode: Enable direct rule(DISCONN)\n");
				eval("ebtables", "-t", "broute", "-F");
				eval("ebtables", "-t", "filter", "-F");
				redirect_setting();
				eval("iptables-restore", "/tmp/redirect_rules");
				// nat_rules = NAT_STATE_REDIRECT;
			}
			else if(cross_state == CONNED){
				_dprintf("\n# AP mode: Disable direct rule(CONNED)\n");
				eval("ebtables", "-t", "broute", "-F");
				eval("ebtables", "-t", "filter", "-F");
				eval("ebtables", "-t", "broute", "-I", "BROUTING", "-p", "ipv4", "--ip-proto", "udp", "--ip-dport", "53", "-j", "redirect", "--redirect-target", "DROP");
				redirect_nat_setting();
				eval("iptables-restore", NAT_RULES);
				// nat_rules = NAT_STATE_NORMAL;
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
		_dprintf("\n# Enable direct rule\n");
		rule_setup = 1;
	}
	else if(cross_state == CONNED && isFirstUse){
		_dprintf("\n#CONNED : Enable direct rule\n");
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
			if(!nat_redirect_enable_old && nat_redirect_enable)	//need redirect
			{
_dprintf("nat_rule: stop_nat_rules 4.\n");
				nat_state = stop_nat_rules();
			}
			else if(nat_redirect_enable_old && !nat_redirect_enable)	//don't redirect
			{
_dprintf("nat_rule: start_nat_rules 4.\n");
				nat_state = start_nat_rules();
			}
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
					goto WANDUCK_SELECT;
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
if(test_log){
_dprintf("wanduck(%d)(PHY state): PHY_RECONN=%d...\n", wan_unit, PHY_RECONN);
_dprintf("wanduck(%d)(PHY state): %d...\n", wan_unit, conn_state[wan_unit]);
}

					if(conn_state[wan_unit] == CONNED){
#ifdef RTCONFIG_USB_MODEM
						if(dualwan_unit__usbif(wan_unit) && current_state[wan_unit] == WAN_STATE_INITIALIZING)
							conn_state[wan_unit] = DISCONN;
						else
#endif
							conn_state[wan_unit] = if_wan_connected(wan_unit);
if(test_log)
_dprintf("wanduck(%d)(conn     ): %d...\n", wan_unit, conn_state[wan_unit]);
					}
				}

				wan_sbstate = nvram_get_int(nvram_sbstate[wan_unit]);
if(test_log)
_dprintf("wanduck(%d)(sbstate  ): %d...\n", wan_unit, wan_sbstate);

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
				if(disconn_case_old[wan_unit] != CASE_DATALIMIT && wan_sbstate == WAN_STOPPED_REASON_DATALIMIT){
					_dprintf("wanduck(%d)(lb): detect the data limit.\n", wan_unit);
					conn_state[wan_unit] = DISCONN;
				}
#endif

				if(conn_state[wan_unit] == CONNED && cross_state != CONNED)
					cross_state = CONNED;

if(test_log)
_dprintf("wanduck(%d)(link_wan ): %d...\n", wan_unit, link_wan[wan_unit]);
#ifdef RTCONFIG_USB_MODEM
				if(wan_sbstate != WAN_STOPPED_REASON_DATALIMIT && dualwan_unit__usbif(wan_unit)){
if(test_log)
_dprintf("wanduck(%d): decide start_wan_if or stop_wan_if...\n", wan_unit);
					if(link_wan[wan_unit] == 1 && current_state[wan_unit] == WAN_STATE_INITIALIZING && boot_end == 1){
						set_disconn_count(wan_unit, S_COUNT); // reset count for the new start_wan_if.
						_dprintf("wanduck: start_wan_if %d.\n", wan_unit);
						snprintf(cmd, sizeof(cmd), "start_wan_if %d", wan_unit);
						notify_rc(cmd);
						goto WANDUCK_SELECT;
					}
					else if(!link_wan[wan_unit] && current_state[wan_unit] != WAN_STATE_INITIALIZING
							//&& current_state[wan_unit] != WAN_STATE_STOPPED
							){
						_dprintf("wanduck: stop_wan_if %d.\n", wan_unit);
						snprintf(cmd, sizeof(cmd), "stop_wan_if %d", wan_unit);
						notify_rc(cmd);
						goto WANDUCK_SELECT;
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
#endif
				if(conn_state[wan_unit] != conn_state_old[wan_unit]){
					conn_state_old[wan_unit] = conn_state[wan_unit];

					if(conn_state[wan_unit] == PHY_RECONN){
						conn_changed_state[wan_unit] = PHY_RECONN;

						set_disconn_count(wan_unit, max_disconn_count[wan_unit]);
					}
					else if(conn_state[wan_unit] == DISCONN){
						conn_changed_state[wan_unit] = C2D;

#ifdef RTCONFIG_USB_MODEM
						if (dualwan_unit__usbif(wan_unit))
							set_disconn_count(wan_unit, max_disconn_count[wan_unit]);
						else
#endif
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
						if(disconn_case[wan_unit] == CASE_DATALIMIT)
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

					record_conn_status(wan_unit);
				}

				if(get_disconn_count(wan_unit) != S_IDLE){
					if(get_disconn_count(wan_unit) >= max_disconn_count[wan_unit]){
						set_disconn_count(wan_unit, S_IDLE);

#ifdef RTCONFIG_USB_MODEM
						if(dualwan_unit__usbif(wan_unit)){
							snprintf(modem_type, sizeof(modem_type), "%s", nvram_safe_get("usb_modem_act_type"));
							_dprintf("\n# wanduck(%d): modem_type=%s.\n", wan_unit, modem_type);
						}

						if(dualwan_unit__usbif(wan_unit) &&
								(!strcmp(modem_type, "") || (strcmp(modem_type, "tty") && strcmp(modem_type, "mbim")))
								){
							// connection be activated by hotplug or PHY_RECONN.
							_dprintf("\n# wanduck(%d): skip to run restart_wan_if.\n", wan_unit);
						}
						else
#endif
						{
							// connection be activated by wanduck.
							_dprintf("\n# wanduck(%d): run restart_wan_if.\n", wan_unit);
							snprintf(cmd, sizeof(cmd), "restart_wan_if %d", wan_unit);
							notify_rc(cmd);
						}

						if(is_wan_connect(get_next_unit(wan_unit))){
							_dprintf("\n# wanduck(%d): run restart_wan_line %d.\n", wan_unit, get_next_unit(wan_unit));
							snprintf(cmd, sizeof(cmd), "restart_wan_line %d", get_next_unit(wan_unit));
							notify_rc(cmd);
						}
					}
					else
						set_disconn_count(wan_unit, get_disconn_count(wan_unit)+1);

					_dprintf("%s: wan(%d) disconn count = %d/%d ...\n", __FUNCTION__, wan_unit, get_disconn_count(wan_unit), max_disconn_count[wan_unit]);
				}
			}
if(test_log)
_dprintf("wanduck(%d)(lb change): state %d, state_old %d, changed %d, cross_state=%d, wan_state %d.\n"
		, current_wan_unit, conn_state[current_wan_unit], conn_state_old[current_wan_unit], conn_changed_state[current_wan_unit], cross_state, current_state[current_wan_unit]);
		}
		else if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "fo")){
			if(delay_detect == 1 && wandog_delay > 0){
				_dprintf("wanduck: delay %d seconds...\n", wandog_delay);
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
						conn_state[current_wan_unit] = if_wan_connected(current_wan_unit);
if(test_log)
_dprintf("wanduck(%d)(fo   conn): state %d, state_old %d, changed %d, wan_state %d.\n"
		, current_wan_unit, conn_state[current_wan_unit], conn_state_old[current_wan_unit], conn_changed_state[current_wan_unit], current_state[current_wan_unit]);
				}
			}

			wan_sbstate = nvram_get_int(nvram_sbstate[current_wan_unit]);

			if(conn_state[current_wan_unit] == PHY_RECONN){
				conn_changed_state[current_wan_unit] = PHY_RECONN;

				conn_state_old[current_wan_unit] = DISCONN;

				// When the current line is re-plugged and the other line has plugged, the count has to be reset.
				if(get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE && link_wan[other_wan_unit]){
					_dprintf("# wanduck: set S_COUNT: PHY_RECONN.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			else if(wan_sbstate == WAN_STOPPED_REASON_DATALIMIT){
				if(conn_state_old[current_wan_unit] == CONNED){
					_dprintf("wanduck(%d)(fo): detect the data limit.\n", current_wan_unit);
					conn_changed_state[current_wan_unit] = C2D;
				}
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = DISCONN;

				if(get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE
						&& link_wan[other_wan_unit]
						&& nvram_get_int(nvram_sbstate[other_wan_unit]) != WAN_STOPPED_REASON_DATALIMIT)
					set_disconn_count(current_wan_unit, max_disconn_count[current_wan_unit]);
				else
					set_disconn_count(current_wan_unit, S_IDLE);
			}
#endif
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
#if 0
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			else if(dualwan_unit__usbif(current_wan_unit) && wan_sbstate == WAN_STOPPED_REASON_DATALIMIT){
				if(conn_state_old[current_wan_unit] == CONNED){
					_dprintf("wanduck(%d)(fo): detect the data limit.\n", current_wan_unit);
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
					_dprintf("# wanduck: set S_IDLE: CASE_THESAMESUBNET.\n");
					set_disconn_count(current_wan_unit, S_IDLE);
				}
#ifdef RTCONFIG_USB_MODEM
				// when the other line is modem and not plugged, the current disconnected line would not count.
				else if(!link_wan[other_wan_unit] && dualwan_unit__usbif(other_wan_unit))
					set_disconn_count(current_wan_unit, S_IDLE);
#endif
				else if(current_state[current_wan_unit] != WAN_STATE_DISABLED
						&& get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE) {
					if (get_disconn_count(current_wan_unit) == S_IDLE)
						set_disconn_count(current_wan_unit, S_COUNT);
				}
				// when auth failed, the single disconnected line would not count.
				else if(disconn_case[current_wan_unit] == CASE_PPPFAIL && wan_sbstate == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
					set_disconn_count(current_wan_unit, S_IDLE);
			}

			if(get_disconn_count(current_wan_unit) != S_IDLE){
				if(get_disconn_count(current_wan_unit) < max_disconn_count[current_wan_unit]){
					set_disconn_count(current_wan_unit, get_disconn_count(current_wan_unit)+1);
					_dprintf("# wanduck(%d): wait time for switching: %d/%d.\n", current_wan_unit, get_disconn_count(current_wan_unit)*scan_interval, max_wait_time[current_wan_unit]);
				}
				else{
					_dprintf("# wanduck(%d): set S_COUNT: changed_count[] >= max_disconn_count.\n", current_wan_unit);
				}
			}

			record_conn_status(current_wan_unit);
if(test_log)
_dprintf("wanduck(%d)(fo change): state %d, state_old %d, changed %d, wan_state %d.\n"
		, current_wan_unit, conn_state[current_wan_unit], conn_state_old[current_wan_unit], conn_changed_state[current_wan_unit], current_state[current_wan_unit]);
		}
		else if(sw_mode == SW_MODE_ROUTER && !strcmp(dualwan_mode, "fb")){
			if(delay_detect == 1 && wandog_delay > 0){
				_dprintf("wanduck: delay %d seconds...\n", wandog_delay);
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
						conn_state[current_wan_unit] = if_wan_connected(current_wan_unit);
				}

				if(other_wan_unit == WAN_FB_UNIT && conn_state[other_wan_unit] == CONNED){
					current_state[other_wan_unit] = nvram_get_int(nvram_state[other_wan_unit]);
#ifdef RTCONFIG_USB_MODEM
					if(!(dualwan_unit__usbif(other_wan_unit) && current_state[other_wan_unit] == WAN_STATE_INITIALIZING))
#endif
						conn_state[other_wan_unit] = if_wan_connected(other_wan_unit);
					_dprintf("wanduck: detect the fail-back line(%d)...\n", other_wan_unit);
if(test_log)
_dprintf("wanduck(%d) fail-back: state %d, state_old %d, changed %d, wan_state %d.\n"
		, other_wan_unit, conn_state[other_wan_unit], conn_state_old[other_wan_unit], conn_changed_state[other_wan_unit], current_state[other_wan_unit]);
				}
			}

			wan_sbstate = nvram_get_int(nvram_sbstate[current_wan_unit]);

			if(conn_state[current_wan_unit] == PHY_RECONN){
				conn_changed_state[current_wan_unit] = PHY_RECONN;

				conn_state_old[current_wan_unit] = DISCONN;

				// When the current line is re-plugged and the other line has plugged, the count has to be reset.
				if(get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE && link_wan[other_wan_unit]){
					_dprintf("# wanduck: set S_COUNT: PHY_RECONN.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
			}
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			else if(wan_sbstate == WAN_STOPPED_REASON_DATALIMIT){
				if(conn_state_old[current_wan_unit] == CONNED){
					_dprintf("wanduck(%d)(fb): detect the data limit.\n", current_wan_unit);
					conn_changed_state[current_wan_unit] = C2D;
				}
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = DISCONN;

				if(get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE
						&& link_wan[other_wan_unit]
						&& nvram_get_int(nvram_sbstate[other_wan_unit]) != WAN_STOPPED_REASON_DATALIMIT)
					set_disconn_count(current_wan_unit, max_disconn_count[current_wan_unit]);
				else
					set_disconn_count(current_wan_unit, S_IDLE);
			}
#endif
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
#if 0
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			else if(dualwan_unit__usbif(current_wan_unit) && wan_sbstate == WAN_STOPPED_REASON_DATALIMIT){
				if(conn_state_old[current_wan_unit] == CONNED){
					_dprintf("wanduck(%d)(fb): detect the data limit.\n", current_wan_unit);
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
					_dprintf("# wanduck: set S_IDLE: CASE_THESAMESUBNET.\n");
					set_disconn_count(current_wan_unit, S_IDLE);
				}
#ifdef RTCONFIG_USB_MODEM
				// when the other line is modem and not plugged, the current disconnected line would not count.
				else if(!link_wan[other_wan_unit] && dualwan_unit__usbif(other_wan_unit))
					set_disconn_count(current_wan_unit, S_IDLE);
#endif
				else if(current_state[current_wan_unit] != WAN_STATE_DISABLED
						&& get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE){
					if (get_disconn_count(current_wan_unit) == S_IDLE)
						set_disconn_count(current_wan_unit, S_COUNT);
				}
				// when auth failed, the single disconnected line would not count.
				else if(disconn_case[current_wan_unit] == CASE_PPPFAIL && wan_sbstate == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
					set_disconn_count(current_wan_unit, S_IDLE);
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
					_dprintf("# wanduck(%d): wait time for switching: %d/%d.\n", current_wan_unit, get_disconn_count(current_wan_unit)*scan_interval, max_wait_time[current_wan_unit]);
				}
				else{
					_dprintf("# wanduck(%d): set S_COUNT: changed_count[] >= max_disconn_count.\n", current_wan_unit);
				}
			}

			if(get_disconn_count(other_wan_unit) != S_IDLE){
				if(get_disconn_count(other_wan_unit) < max_fb_count){
					set_disconn_count(other_wan_unit, get_disconn_count(other_wan_unit)+1);
					_dprintf("# wanduck: wait time for returning: %d/%d.\n", get_disconn_count(other_wan_unit)*scan_interval, max_fb_wait_time);
				}
				else{
					_dprintf("# wanduck: set S_COUNT: changed_count[] >= max_fb_count.\n");
					set_disconn_count(other_wan_unit, S_COUNT);
				}
			}

			record_conn_status(current_wan_unit);
		}
		else
#else // RTCONFIG_DUALWAN
		if(sw_mode == SW_MODE_ROUTER
#ifdef RTCONFIG_WIRELESSREPEATER
				|| sw_mode == SW_MODE_HOTSPOT
#endif
				){
			// To check the phy connection of the standby line.
#ifdef RTCONFIG_USB_MODEM
			for(wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit)
#else
			wan_unit = WAN_UNIT_FIRST;
#endif
			{
				if(get_dualwan_by_unit(wan_unit) != WANS_DUALWAN_IF_NONE)
					conn_state[wan_unit] = if_wan_phyconnected(wan_unit);
			}

			current_wan_unit = wan_primary_ifunit();
#ifdef RTCONFIG_USB_MODEM
			other_wan_unit = get_next_unit(current_wan_unit);
#endif

			current_state[current_wan_unit] = nvram_get_int(nvram_state[current_wan_unit]);

			if(current_state[current_wan_unit] == WAN_STATE_DISABLED){
				//record_wan_state_nvram(current_wan_unit, WAN_STATE_STOPPED, WAN_STOPPED_REASON_MANUAL, -1);
				// mark here, because had executed update_wan_leds() at if_wan_phyconnected().
				//update_wan_leds(current_wan_unit);

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
// mark here, because had executed update_wan_leds() at if_wan_phyconnected().
//#if defined(RTAC58U)
//				update_wan_leds(current_wan_unit);
// mark here, because had executed if_wan_phyconnected() before.
//#elif !defined(RTN14U) && !defined(RTAC1200GP) && !defined(RTAC1200) && !defined(RTAC82U) && !defined(HIVEDOT) && !defined(HIVESPOT)
//				conn_state[current_wan_unit] = if_wan_phyconnected(current_wan_unit);
//#endif
				if(conn_state[current_wan_unit] == CONNED){
#ifdef RTCONFIG_USB_MODEM
					if(!(dualwan_unit__usbif(current_wan_unit) && current_state[current_wan_unit] == WAN_STATE_INITIALIZING))
#endif
						conn_state[current_wan_unit] = if_wan_connected(current_wan_unit);
				}
			}

			wan_sbstate = nvram_get_int(nvram_sbstate[current_wan_unit]);

			if(conn_state[current_wan_unit] == PHY_RECONN){
				conn_changed_state[current_wan_unit] = PHY_RECONN;

				conn_state_old[current_wan_unit] = DISCONN;

#ifdef RTCONFIG_USB_MODEM
				// When the current line is re-plugged and the other line has plugged, the count has to be reset.
				if(link_wan[other_wan_unit]){
					_dprintf("# wanduck: set S_COUNT: PHY_RECONN.\n");
					set_disconn_count(current_wan_unit, S_COUNT);
				}
#endif
			}
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			else if(wan_sbstate == WAN_STOPPED_REASON_DATALIMIT){
				if(conn_state_old[current_wan_unit] == CONNED){
					_dprintf("wanduck(usb): detect the data limit.\n");
					conn_changed_state[current_wan_unit] = C2D;
				}
				else
					conn_changed_state[current_wan_unit] = DISCONN;

				conn_state_old[current_wan_unit] = DISCONN;

#ifdef RTCONFIG_USB_MODEM
				if(link_wan[other_wan_unit]
						&& nvram_get_int(nvram_sbstate[other_wan_unit]) != WAN_STOPPED_REASON_DATALIMIT)
					set_disconn_count(current_wan_unit, max_disconn_count[current_wan_unit]);
				else
#endif
					set_disconn_count(current_wan_unit, S_IDLE);
			}
#endif
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
#if 0
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			else if(dualwan_unit__usbif(current_wan_unit) && wan_sbstate == WAN_STOPPED_REASON_DATALIMIT){
				if(conn_state_old[current_wan_unit] == CONNED){
					_dprintf("wanduck(usb): detect the data limit.\n");
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
#endif
			else if(conn_state[current_wan_unit] == CONNED){
				if(conn_state_old[current_wan_unit] == DISCONN)
					conn_changed_state[current_wan_unit] = D2C;
				else
					conn_changed_state[current_wan_unit] = CONNED;

				conn_state_old[current_wan_unit] = conn_state[current_wan_unit];

#ifdef RTCONFIG_DSL /* Paul add 2013/7/29, for Non-DualWAN 3G/4G WAN -> DSL WAN, auto Fail-Back feature */
#ifdef RTCONFIG_USB_MODEM
				if (nvram_match("dsltmp_adslsyncsts","up") && current_wan_unit == WAN_UNIT_SECOND){
					_dprintf("\n# wanduck: adslsync up.\n");
					set_disconn_count(current_wan_unit, S_IDLE);
					link_wan[current_wan_unit] = 0;
					conn_state[current_wan_unit] = DISCONN;
					usb_switched_back_dsl = 1;
					max_disconn_count[current_wan_unit] = 1;
				}
				else
					set_disconn_count(current_wan_unit, S_IDLE);
#endif
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
					_dprintf("# wanduck: set S_IDLE: CASE_THESAMESUBNET.\n");
					set_disconn_count(current_wan_unit, S_IDLE);
				}
#ifdef RTCONFIG_USB_MODEM
				// when the other line is modem and not plugged, the current disconnected line would not count.
				else if(!link_wan[other_wan_unit] && dualwan_unit__usbif(other_wan_unit))
					set_disconn_count(current_wan_unit, S_IDLE);
				else if(get_disconn_count(current_wan_unit) == S_IDLE && current_state[current_wan_unit] != WAN_STATE_DISABLED)
					set_disconn_count(current_wan_unit, S_COUNT);
#else
				// when auth failed, the single disconnected line would not count.
				else if(disconn_case[current_wan_unit] == CASE_PPPFAIL && wan_sbstate == WAN_STOPPED_REASON_PPP_AUTH_FAIL)
					set_disconn_count(current_wan_unit, S_IDLE);
#endif
			}

			if(get_disconn_count(current_wan_unit) != S_IDLE){
				if(get_disconn_count(current_wan_unit) < max_disconn_count[current_wan_unit]){
					set_disconn_count(current_wan_unit, get_disconn_count(current_wan_unit)+1);
					_dprintf("# wanduck(%d): wait time for switching: %d/%d.\n", current_wan_unit, get_disconn_count(current_wan_unit)*scan_interval, max_wait_time[current_wan_unit]);
				}
				else{
					_dprintf("# wanduck(%d): set S_COUNT: changed_count[] >= max_disconn_count.\n", current_wan_unit);
				}
			}

			record_conn_status(current_wan_unit);
		}
		else
#endif // RTCONFIG_DUALWAN
		{ // sw_mode == SW_MODE_AP, SW_MODE_REPEATER.
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
			if(cross_state == DISCONN || isFirstUse){
				if(!rule_setup){
					if(isFirstUse)
						_dprintf("\n# LB: Enable direct rule(isFirstUse)\n");
					else
						_dprintf("\n# LB: Enable direct rule\n");
					rule_setup = 1;
_dprintf("nat_rule: stop_nat_rules 5.\n");
				}
				nat_state = stop_nat_rules();
			}
			//else if(cross_state == CONNED && !isFirstUse){
			else{
				if(rule_setup){
					_dprintf("\n# LB: Disable direct rule\n");
					rule_setup = 0;
_dprintf("nat_rule: start_nat_rules 5.\n");
				}
				nat_state = start_nat_rules();
			}
		}
		else
#endif // RTCONFIG_DUALWAN
#ifdef RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_REPEATER)
		{
#ifdef RTCONFIG_RESTRICT_GUI
			char word[PATH_MAX], *next_word;
#endif

			if(!got_notify)
				; // do nothing.
			else if(conn_changed_state[current_wan_unit] == DISCONN || conn_changed_state[current_wan_unit] == C2D || isFirstUse){
				//if(!rule_setup){
					if(conn_changed_state[current_wan_unit] == DISCONN)
						_dprintf("\n# mode(%d): Enable direct rule(DISCONN)\n", sw_mode);
					else if(conn_changed_state[current_wan_unit] == C2D)
						_dprintf("\n# mode(%d): Enable direct rule(C2D)\n", sw_mode);
					else
						_dprintf("\n# mode(%d): Enable direct rule(isFirstUse)\n", sw_mode);
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
								goto WANDUCK_SELECT;

							eval("ebtables", "-t", "broute", "-A", "BROUTING", "-i", word, "-j", "mark", "--mark-set", BIT_RES_GUI, "--mark-target", "ACCEPT");
						}

						repeater_filter_setting(0);
					}
#endif
_dprintf("nat_rule: stop_nat_rules 6.\n");
					nat_state = stop_nat_rules();
				//}
				got_notify = 0;
			}
			else{
				//if(rule_setup && !isFirstUse)
				if(!isFirstUse)
				{
					_dprintf("\n# mode(%d): Disable direct rule(CONNED)\n", sw_mode);
					rule_setup = 0;
					eval("ebtables", "-t", "broute", "-F");
					eval("ebtables", "-t", "filter", "-F");

#ifdef RTCONFIG_RESTRICT_GUI
					if(nvram_get_int("fw_restrict_gui")){
						foreach(word, nvram_safe_get("lan_ifnames"), next_word){
							if(!strncmp(word, "vlan", 4))
								goto WANDUCK_SELECT;

							eval("ebtables", "-t", "broute", "-A", "BROUTING", "-i", word, "-j", "mark", "--mark-set", BIT_RES_GUI, "--mark-target", "CONTINUE");
						}

						repeater_filter_setting(1);
					}
#endif

					eval("ebtables", "-t", "broute", "-A", "BROUTING", "-d", "00:E0:11:22:33:44", "-j", "redirect", "--redirect-target", "DROP");
					snprintf(domain_mapping, sizeof(domain_mapping), "%x %s", inet_addr(nvram_safe_get("lan_ipaddr")), DUT_DOMAIN_NAME);
					f_write_string("/proc/net/dnsmqctrl", domain_mapping, 0, 0);
#ifdef RTCONFIG_REDIRECT_DNAME
					eval("ebtables", "-t", "broute", "-A", "BROUTING", "-p", "ipv4", "--ip-proto", "udp", "--ip-dport", "53", "-j", "redirect", "--redirect-target", "DROP");
#endif
_dprintf("nat_rule: start_nat_rules 6.\n");
					nat_state = start_nat_rules();
				}

				got_notify = 0;
			}
		}
		else
#endif // RTCONFIG_WIRELESSREPEATER
		if(sw_mode == SW_MODE_AP)
		{
#ifdef RTCONFIG_REDIRECT_DNAME
			if (conn_changed_state[current_wan_unit] == C2D) {
				_dprintf("\n# AP mode: Enable direct rule(C2D)\n");
				eval("ebtables", "-t", "broute", "-F");
				eval("ebtables", "-t", "filter", "-F");
				redirect_setting();
				eval("iptables-restore", "/tmp/redirect_rules");
				// nat_rules = NAT_STATE_REDIRECT;
			}
			else if (conn_changed_state[current_wan_unit] == D2C) {
				_dprintf("\n# AP mode: Disable direct rule(D2C)\n");
				eval("ebtables", "-t", "broute", "-F");
				eval("ebtables", "-t", "filter", "-F");
				eval("ebtables", "-t", "broute", "-I", "BROUTING", "-p", "ipv4", "--ip-proto", "udp", "--ip-dport", "53", "-j", "redirect", "--redirect-target", "DROP");
				redirect_nat_setting();
				eval("iptables-restore", NAT_RULES);
				// nat_rules = NAT_STATE_NORMAL;
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
							goto WANDUCK_SELECT;

						eval("ebtables", "-t", "broute", "-I", "BROUTING", "-i", word, "-j", "mark", "--mark-set", BIT_RES_GUI, "--mark-target", "CONTINUE");
					}

					repeater_filter_setting(0);
				}
			}
#endif
		}
		else if(conn_changed_state[current_wan_unit] == C2D || (conn_changed_state[current_wan_unit] == CONNED && isFirstUse)){
			if(!rule_setup){
				if(conn_changed_state[current_wan_unit] == C2D){
					if (nvram_match("led_disable", "0")) {
#if defined(RTCONFIG_WPS_ALLLED_BTN) || defined(RTCONFIG_DSL)
						led_control(LED_WAN, LED_OFF);
#elif defined(RTAC68U) || defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)|| defined(RTAC5300R)
						if(
#ifdef RTAC68U
							is_ac66u_v2_series()
#else
							1
#endif // RTAC68U
#ifdef RTCONFIG_DUALWAN
							&& (strcmp(dualwan_mode, "lb") == 0 ||
								 strcmp(dualwan_mode, "fb") == 0 ||
								 strcmp(dualwan_mode, "fo") == 0)
#endif // RTCONFIG_DUALWAN
						){
							logmessage("DualWAN", "skip single wan wan_led_control - WANRED off\n");
							if(nvram_match("AllLED", "1")){
								led_control(LED_WAN, LED_ON);
								disable_wan_led();
							}
						}
#endif
					}
					_dprintf("\n# Enable direct rule(C2D)......\n");

				}
				else
					_dprintf("\n# Enable direct rule(isFirstUse)\n");

				rule_setup = 1;

				if(conn_changed_state[current_wan_unit] == C2D){
#ifdef RTCONFIG_USB_MODEM
					// the current line is USB and have been plugged off.
					if(!link_wan[current_wan_unit] && dualwan_unit__usbif(current_wan_unit)){
						_dprintf("%s: wanduck clean_modem_state!\n", __FUNCTION__);
						clean_modem_state(2);
						if(get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE){
							_dprintf("\n# wanduck(C2D): Modem was plugged off and try to Switch the other line.\n");
							switch_wan_line(other_wan_unit, 0);
						}
						else if(current_state[current_wan_unit] != WAN_STATE_INITIALIZING){
							_dprintf("wanduck3: stop_wan_if %d.\n", current_wan_unit);
							snprintf(cmd, sizeof(cmd), "stop_wan_if %d", current_wan_unit);
							notify_rc(cmd);
						}

#ifdef RTCONFIG_DSL /* Paul add 2013/7/29, for Non-DualWAN 3G/4G WAN -> DSL WAN, auto Fail-Back feature */
#ifndef RTCONFIG_DUALWAN
						if(nvram_match("dsltmp_adslsyncsts","up") && usb_switched_back_dsl == 1){
							_dprintf("\n# wanduck: usb_switched_back_dsl: 1.\n");
							link_wan[WAN_UNIT_SECOND] = 1;
							max_disconn_count[WAN_UNIT_SECOND] = max_wait_time[WAN_UNIT_SECOND]/scan_interval;
							usb_switched_back_dsl = 0;
						}
#endif
#endif
					}
//					else
#endif // RTCONFIG_USB_MODEM
#if 0
					// C2D: Try to prepare the backup line.
					if(link_wan[other_wan_unit] == 1 && !is_wan_connect(other_wan_unit)){
						_dprintf("\n# wanduck(C2D): Try to prepare the backup line.\n");
						snprintf(cmd, sizeof(cmd), "restart_wan_if %d", other_wan_unit);
						notify_rc(cmd);
					}
#endif
				}
			}

			handle_wan_line(current_wan_unit, rule_setup);
		}
		else if(conn_changed_state[current_wan_unit] == D2C || conn_changed_state[current_wan_unit] == CONNED){
			if(rule_setup && !isFirstUse){
				if (nvram_match("led_disable", "0")) {
#if defined(RTCONFIG_WPS_ALLLED_BTN)
					if(nvram_match("AllLED", "1"))
						led_control(LED_WAN, LED_ON);
					else
						led_control(LED_WAN, LED_OFF);
#elif defined(DSL_N55U) || defined(DSL_N55U_B)
					led_control(LED_WAN, LED_ON);
#elif defined(RTAC68U) || defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)|| defined(RTAC5300R)
				if(nvram_match("AllLED", "1")
#ifdef RTAC68U
					&& is_ac66u_v2_series()
#endif
				){
						led_control(LED_WAN, LED_OFF);
						enable_wan_led();
					}
#endif
				}
				_dprintf("\n# Disable direct rule(D2C)......\n");
				rule_setup = 0;

				handle_wan_line(current_wan_unit, rule_setup);
			}
		}
		/*
		 * when all lines are plugged in and the currect line is disconnected over max_wait_time seconds,
		 * switch the connect to the other line.
		 */
		else if(conn_changed_state[current_wan_unit] == DISCONN){
#if defined(RTCONFIG_DUALWAN) || defined(RTCONFIG_USB_MODEM)
			if(get_dualwan_by_unit(other_wan_unit) != WANS_DUALWAN_IF_NONE
					&& (get_disconn_count(current_wan_unit) >= max_disconn_count[current_wan_unit]
#ifdef RTCONFIG_USB_MODEM
							|| (dualwan_unit__usbif(current_wan_unit) && !link_wan[current_wan_unit])
#endif
							)
					)
			{
				_dprintf("# wanduck(%d): Switching the connect to the %d WAN line...\n", current_wan_unit, get_next_unit(current_wan_unit));
				set_disconn_count(current_wan_unit, S_IDLE);;
				if(!link_wan[current_wan_unit] && dualwan_unit__usbif(current_wan_unit))
					switch_wan_line(other_wan_unit, 0);
				else
					switch_wan_line(other_wan_unit, 1);
			}
			else
#ifdef RTCONFIG_AUTOCOVER_SIP
			if(disconn_case[current_wan_unit] == CASE_THESAMESUBNET && isFirstUse && nvram_get_int("atcover_sip") == 1){
#if 1
				struct in_addr addr;
				in_addr_t new_addr;

				if (inet_deconflict(current_lan_ipaddr, current_lan_netmask,
						    current_lan_ipaddr, current_lan_netmask, &addr)) {
					nvram_set("lan_ipaddr", inet_ntoa(addr));
					nvram_set("lan_ipaddr_rt", inet_ntoa(addr));

					new_addr = ntohl(addr.s_addr);
					addr.s_addr = htonl(new_addr + 1);
					nvram_set("dhcp_start", inet_ntoa(addr));
					addr.s_addr = htonl((new_addr | ~inet_network(current_lan_netmask)) & 0xfffffffe);
					nvram_set("dhcp_end", inet_ntoa(addr));

					notify_rc_and_wait("restart_net_and_phy");
				}
#else
				/* nb: it does the commit, code above - not */
				notify_rc_and_wait("restart_subnet");
#endif
			}
			else
#endif
#endif
			/* recover redirect rules if overwritten by ppp+dhcp/zeroconf/etc */
			//if(disconn_case[current_wan_unit] == CASE_PPPFAIL && nat_state == NAT_STATE_REDIRECT &&
			//	nvram_get_int("nat_state") == NAT_STATE_NORMAL) {
			if(disconn_case[current_wan_unit] == CASE_PPPFAIL){
_dprintf("nat_rule: stop_nat_rules 7.\n");
				nat_state = stop_nat_rules();
			}
		}
		// phy connected -> disconnected -> connected
		else if(conn_changed_state[current_wan_unit] == PHY_RECONN){
#ifdef RTCONFIG_USB_MODEM
			if(dualwan_unit__usbif(current_wan_unit)){
				if(current_state[current_wan_unit] == WAN_STATE_INITIALIZING){
					snprintf(modem_type, sizeof(modem_type), "%s", nvram_safe_get("usb_modem_act_type"));

					handle_wan_line(current_wan_unit, 0);
				}
				else
					_dprintf("wanduck(%d): the modem had been run...\n", current_wan_unit);
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
			_dprintf("# wanduck: returning to the primary WAN line(%d)...\n", other_wan_unit);
			rule_setup = 1;

			handle_wan_line(other_wan_unit, rule_setup);
			switch_wan_line(other_wan_unit, 0);
		}
		// hot-standby: Try to prepare the backup line.
		else if(!strcmp(dualwan_mode, "fo") || !strcmp(dualwan_mode, "fb")){
			if(nvram_get_int("wans_standby") == 1 && link_wan[other_wan_unit] == 1 && get_wan_state(other_wan_unit) == WAN_STATE_INITIALIZING){
				_dprintf("\n# wanduck(hot-standby): Try to prepare the backup line.\n");
				snprintf(cmd, sizeof(cmd), "restart_wan_if %d", other_wan_unit);
				notify_rc(cmd);
			}
		}

#if defined(RTAC68U) || defined(RTAC87U) || defined(RTAC3200) || defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)|| defined(RTAC5300R)
		if (strcmp(dualwan_wans, "wan none")) {
			if(nvram_match("AllLED", "1")
#ifdef RTAC68U
				&& is_ac66u_v2_series()
#endif
			){
				link_status = 0;
				for (u = WAN_UNIT_FIRST; !link_status && u < WAN_UNIT_MAX; ++u) {
					if(conn_state[u] == CONNED)
						link_status++;
				}

				if(link_status) {
					if(wanred_led_status != 2 ){
						led_control(LED_WAN, LED_OFF);
						enable_wan_led();
						wanred_led_status = 2;
					}
				}else{
					if(wanred_led_status != 1 ){
						led_control(LED_WAN, LED_ON);
						disable_wan_led();
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
				_dprintf("# wanduck servs full\n");
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

	_dprintf("# wanduck exit error\n");
	exit(1);
}
