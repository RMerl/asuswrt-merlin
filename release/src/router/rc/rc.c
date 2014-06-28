/*

	Tomato Firmware
	Copyright (C) 2006-2009 Jonathan Zarate

*/

#include "rc.h"
#include "interface.h"
#include <sys/time.h>
#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

#ifdef DEBUG_RCTEST
// used for various testing
static int rctest_main(int argc, char *argv[])
{
	int on;

	if (argc < 2) {
		_dprintf("test what?\n");
	}
	else if (strcmp(argv[1], "rc_service")==0) {
		notify_rc(argv[2]);
	}
	else if(strcmp(argv[1], "get_phy_status")==0) {
		int mask;
		mask = atoi(argv[2]);
		TRACE_PT("debug for phy_status %x\n", get_phy_status(mask));
	}
	else if(strcmp(argv[1], "get_phy_speed")==0) {
		int mask;
		mask = atoi(argv[2]);
		TRACE_PT("debug for phy_speed %x\n", get_phy_speed(mask));
	}
	else if(strcmp(argv[1], "set_phy_ctrl")==0) {
		int mask, ctrl;
		mask = atoi(argv[2]);
		ctrl = atoi(argv[3]);
		TRACE_PT("debug for phy_speed %x\n", set_phy_ctrl(mask, ctrl));
	}
	else if(strcmp(argv[1], "handle_notifications")==0) {
		handle_notifications();
	}
	else if(strcmp(argv[1], "check_action")==0) {
		_dprintf("check: %d\n", check_action());
	}
	else if(strcmp(argv[1], "nvramhex")==0) {
		int i;
		char *nv;

		nv = nvram_safe_get(argv[2]);

		_dprintf("nvram %s(%d): ", nv, strlen(nv));
		for(i=0;i<strlen(nv);i++) {
			_dprintf(" %x", (unsigned char)*(nv+i));
		}
		_dprintf("\n");
	}
	else {
		on = atoi(argv[2]);
		_dprintf("%s %d\n", argv[1], on);

		if (strcmp(argv[1], "vlan") == 0)
		{	
			if(on) start_vlan();
			else stop_vlan();
		}
		else if (strcmp(argv[1], "lan") == 0) {
			if(on) start_lan();
			else stop_lan();
		}
		else if (strcmp(argv[1], "wl") == 0) {
			if(on) 
			{
				start_wl();
				lanaccess_wl();
			}
		}
		else if (strcmp(argv[1], "wan") == 0) {
			if(on) start_wan();
			else stop_wan();
		}
		else if (strcmp(argv[1], "firewall") == 0) {
			//if(on) start_firewall();
			//else stop_firewall();
		}
		else if (strcmp(argv[1], "watchdog") == 0) {
			if(on) start_watchdog();
			else stop_watchdog();
		}
#ifdef RTCONFIG_FANCTRL
		else if (strcmp(argv[1], "phy_tempsense") == 0) {
			if(on) start_phy_tempsense();
			else stop_phy_tempsense();
		}
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
		else if (strcmp(argv[1], "psta_monitor") == 0) {
			if(on) start_psta_monitor();
			else stop_psta_monitor();
		}
#endif
#endif
		else if (strcmp(argv[1], "qos") == 0) {//qos test
			if(on){
#ifdef RTCONFIG_RALINK
				if (module_loaded("hw_nat"))
				{
					modprobe_r("hw_nat");
					sleep(1);
#if 0
					system("echo 0 > /proc/sys/net/ipv4/conf/default/force_igmp_version");
					system("echo 0 > /proc/sys/net/ipv4/conf/all/force_igmp_version");
#endif
				}
#endif
				add_iQosRules(get_wan_ifname(0));
				start_iQos();
			}
			else 
			{
#ifdef RTCONFIG_RALINK
				if (nvram_get_int("hwnat") &&
					/* TODO: consider RTCONFIG_DUALWAN case */
//					!nvram_match("wan0_proto", "l2tp") &&
//					!nvram_match("wan0_proto", "pptp") &&
//					!(nvram_get_int("fw_pt_l2tp") || nvram_get_int("fw_pt_ipsec") &&
//					(nvram_match("wl0_radio", "0") || nvram_get_int("wl0_mrate_x")) &&
//					(nvram_match("wl1_radio", "0") || nvram_get_int("wl1_mrate_x")) &&
					!module_loaded("hw_nat"))
				{
#if 0
					system("echo 2 > /proc/sys/net/ipv4/conf/default/force_igmp_version");
					system("echo 2 > /proc/sys/net/ipv4/conf/all/force_igmp_version");
#endif

#if defined(RTN14U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P)
					if (!(!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")))
#endif
					{
						modprobe("hw_nat");
						sleep(1);
					}	
				}
#endif
				stop_iQos();
				del_iQosRules();
			}
		}
#ifdef RTCONFIG_WEBDAV
		else if (strcmp(argv[1], "webdav") == 0) {
			if(on)
				start_webdav();
		}
#endif
		else if (strcmp(argv[1], "gpiow") == 0) {
			if(argc>=4) set_gpio(atoi(argv[2]), atoi(argv[3]));
		}
		else if (strcmp(argv[1], "gpior") == 0) {
			_dprintf("%d\n", get_gpio(atoi(argv[2])));
		}
		else if (strcmp(argv[1], "gpiod") == 0) {
			if(argc>=4) gpio_dir(atoi(argv[2]), atoi(argv[3]));
		}
		else if (strcmp(argv[1], "init_switch") == 0) {
			init_switch(on);
		}
		else if (strcmp(argv[1], "set_action") == 0) {
			set_action(on);
		}
		else if (strcmp(argv[1], "pwr_usb") == 0) {
			set_pwr_usb(atoi(argv[2]));
			_dprintf("done.\n");
		}
#ifdef RTCONFIG_BCMFA
		else if (strcmp(argv[1], "fa_rev") == 0) {
			_dprintf("(%d) done.\n", get_fa_rev());
		}
		else if (strcmp(argv[1], "fa_dump") == 0) {
			_dprintf("(%d) done.\n", get_fa_dump());
		}
#endif
		else {
			printf("what?\n");
		}
	}
	return 0;
}
#endif


static int hotplug_main(int argc, char *argv[])
{
	if (argc >= 2) {
		if (strcmp(argv[1], "net") == 0) {
			hotplug_net();
		}
#ifdef RTCONFIG_USB
		// !!TB - USB Support
		else if (strcmp(argv[1], "usb") == 0) {
			hotplug_usb();
		}
#ifdef LINUX26
		else if (strcmp(argv[1], "block") == 0) {
			hotplug_usb();
		}
#endif
#endif
	}
	return 0;
}

typedef struct {
	const char *name;
	int (*main)(int argc, char *argv[]);
} applets_t;

static const applets_t applets[] = {
        { "preinit",                    init_main                               },
	{ "init",			init_main				},
	{ "console",			console_main			},
#ifdef DEBUG_RCTEST
	{ "rc",				rctest_main			},
#endif
	{ "ip-up",			ipup_main				},
	{ "ip-down",			ipdown_main				},
	{ "ip-pre-up",			ippreup_main				},
#ifdef RTCONFIG_IPV6
	{ "ipv6-up",			ip6up_main				},
	{ "ipv6-down",			ip6down_main				},
#endif
	{ "auth-fail",			authfail_main				},
#ifdef RTCONFIG_VPNC
	{ "vpnc-ip-up",			vpnc_ipup_main				},
	{ "vpnc-ip-down",		vpnc_ipdown_main				},
	{ "vpnc-ip-pre-up",		vpnc_ippreup_main				},
	{ "vpnc-auth-fail",		vpnc_authfail_main				},
#endif
#ifdef RTCONFIG_EAPOL
	{ "wpa_cli",			wpacli_main			},
#endif
	{ "hotplug",			hotplug_main			},
#ifdef RTCONFIG_BCMARM
        { "mtd-write",                  mtd_write_main_old                      },
        { "mtd-erase",                  mtd_unlock_erase_main_old               },
        { "mtd-unlock",                 mtd_unlock_erase_main_old               },
#else
	{ "mtd-write",			mtd_write_main			},
	{ "mtd-erase",			mtd_unlock_erase_main		},
	{ "mtd-unlock",			mtd_unlock_erase_main		},
#endif
	{ "watchdog",			watchdog_main			},
#ifdef RTCONFIG_FANCTRL
	{ "phy_tempsense",		phy_tempsense_main		},
#endif
#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
	{ "psta_monitor",		psta_monitor_main		},
#endif
#endif
#ifdef RTCONFIG_USB
	{ "usbled",			usbled_main			},
#endif
	{ "ddns_updated", 		ddns_updated_main		},
	{ "radio",			radio_main			},
	{ "ots",			ots_main			},
	{ "udhcpc",			udhcpc_wan			},
	{ "udhcpc_lan",			udhcpc_lan			},
	{ "zcip",			zcip_wan			},
#ifdef RTCONFIG_IPV6
	{ "dhcp6c-state",		dhcp6c_state_main		},
#endif
#ifdef RTCONFIG_WPS
	{ "wpsaide",			wpsaide_main			},
#endif
	{ "halt",			reboothalt_main			},
	{ "reboot",			reboothalt_main			},
	{ "ntp", 			ntp_main			},
#ifdef RTCONFIG_RALINK
	{ "rtkswitch",			config_rtkswitch		},
#endif
	{ "wanduck",                    wanduck_main                    },
	{ "tcpcheck",                   tcpcheck_main                   },
	{ "autodet", 			autodet_main			},
#ifdef RTCONFIG_CIFS
	{ "mount-cifs",			mount_cifs_main			},
#endif
#ifdef RTCONFIG_USB
	{ "ejusb",			ejusb_main			},
#ifdef RTCONFIG_DISK_MONITOR
	{ "disk_monitor",		diskmon_main			},
#endif
	{ "disk_remove",		diskremove_main			},
#endif
	{ "firmware_check",		firmware_check_main             },
	{ "service",			service_main			},
#ifdef RTCONFIG_BWDPI
	{ "bwdpi",			bwdpi_main			},
#endif
	{NULL, NULL}
};

int main(int argc, char **argv)
{
	char *base;
	int f;

	/*
		Make sure std* are valid since several functions attempt to close these
		handles. If nvram_*() runs first, nvram=0, nvram gets closed. - zzz
	*/

	if ((f = open("/dev/null", O_RDWR)) < 0) {
	}
	else if(f < 3) {
		dup(f);
		dup(f);
	}
	else {
		close(f);
	}

	base = strrchr(argv[0], '/');
	base = base ? base + 1 : argv[0];

#if 0
	if (strcmp(base, "rc") == 0) {
		if (argc < 2) return 1;
		if (strcmp(argv[1], "start") == 0) return kill(1, SIGUSR2);
		if (strcmp(argv[1], "stop") == 0) return kill(1, SIGINT);
		if (strcmp(argv[1], "restart") == 0) return kill(1, SIGHUP);
		++argv;
		--argc;
		base = argv[0];
	}
#endif

#ifdef RTCONFIG_RALINK
    if(getpid() != 1)
    {
#endif

#if defined(DEBUG_NOISY)
	if (nvram_match("debug_logrc", "1")) {
		int i;

		cprintf("[rc %d] ", getpid());
		for (i = 0; i < argc; ++i) {
			cprintf("%s ", argv[i]);
		}
		cprintf("\n");

	}
#endif

#if defined(DEBUG_NOISY)
	if (nvram_match("debug_ovrc", "1")) {
		char tmp[256];
		char *a[32];

		realpath(argv[0], tmp);
		if ((strncmp(tmp, "/tmp/", 5) != 0) && (argc < 32)) {
			sprintf(tmp, "%s%s", "/tmp/", base);
			if (f_exists(tmp)) {
				cprintf("[rc] override: %s\n", tmp);
				memcpy(a, argv, argc * sizeof(a[0]));
				a[argc] = 0;
				a[0] = tmp;
				execvp(tmp, a);
				exit(0);
			}
		}
	}
#endif
#ifdef RTCONFIG_RALINK
    }
#endif
	const applets_t *a;
	for (a = applets; a->name; ++a) {
		if (strcmp(base, a->name) == 0) {
			openlog(base, LOG_PID, LOG_USER);
			return a->main(argc, argv);
		}
	}


	if(!strcmp(base, "restart_wireless")){
		printf("restart wireless...\n");
		restart_wireless();
		return 0;
	}
        else if(!strcmp(base, "nvram_erase")){
                erase_nvram();
                return 0;
        }
#ifdef RTCONFIG_USB
	else if(!strcmp(base, "get_apps_name")){
		if(argc != 2){
			printf("Usage: get_apps_name [File name]\n");
			return 0;
		}

		return get_apps_name(argv[1]);
	}
	else if(!strcmp(base, "asus_sd")){
		if(argc != 3){
			printf("Usage: asus_sd [device_name] [action]\n");
			return 0;
		}

		return asus_sd(argv[1], argv[2]);
	}
	else if(!strcmp(base, "asus_lp")){
		if(argc != 3){
			printf("Usage: asus_lp [device_name] [action]\n");
			return 0;
		}

		return asus_lp(argv[1], argv[2]);
	}
	else if(!strcmp(base, "asus_sg")){
		if(argc != 3){
			printf("Usage: asus_sg [device_name] [action]\n");
			return 0;
		}

		return asus_sg(argv[1], argv[2]);
	}
	else if(!strcmp(base, "asus_sr")){
		if(argc != 3){
			printf("Usage: asus_sr [device_name] [action]\n");
			return 0;
		}

		return asus_sr(argv[1], argv[2]);
	}
	else if(!strcmp(base, "asus_tty")){
		if(argc != 3){
			printf("Usage: asus_tty [device_name] [action]\n");
			return 0;
		}

		return asus_tty(argv[1], argv[2]);
	}
	else if(!strcmp(base, "asus_usbbcm")){
		if(argc != 3){
			printf("Usage: asus_usbbcm [device_name] [action]\n");
			return 0;
		}

		return asus_usbbcm(argv[1], argv[2]);
	}
	else if(!strcmp(base, "asus_usb_interface")){
		if(argc != 3){
			printf("Usage: asus_usb_interface [device_name] [action]\n");
			return 0;
		}

		return asus_usb_interface(argv[1], argv[2]);
	}
	else if (!strcmp(base, "usb_notify")) {
#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
		usb_notify();
#endif

		return 0;
	}
#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
	else if(!strcmp(base, "run_app_script")){
		if(argc != 3){
			printf("Usage: run_app_script [Package name | allpkg] [APP action]\n");
			return 0;
		}

		if(!strcmp(argv[1], "allpkg"))
			return run_app_script(NULL, argv[2]);
		else
			return run_app_script(argv[1], argv[2]);
	}
	else if (!strcmp(base, "chk_app_state")) {
#define PID_FILE "/var/run/chk_app_state.pid"
		FILE *fp;
		char chk_value[4];

		if(f_read_string(PID_FILE, chk_value, 4) > 0
				&& atoi(chk_value) != getpid()){
			_dprintf("Already running!\n");
			return 0;
		}

		if((fp = fopen(PID_FILE, "w")) == NULL){
			_dprintf("Can't open the pid file!\n");
			return 0;
		}

		fprintf(fp, "%d", getpid());
		fclose(fp);

		memset(chk_value, 0, 4);
		strncpy(chk_value, nvram_safe_get("apps_state_switch"), 4);
		if(strcmp(chk_value, "")){
			if(atoi(chk_value) != APPS_SWITCH_FINISHED && !pids("app_switch.sh")){
				_dprintf("Don't have the switch script.\n");
				nvram_set("apps_state_switch", "");
			}

			unlink(PID_FILE);
			return 0;
		}

		memset(chk_value, 0, 4);
		strncpy(chk_value, nvram_safe_get("apps_state_install"), 4);
		if(strcmp(chk_value, "")){
			if(atoi(chk_value) != APPS_INSTALL_FINISHED && !pids("app_install.sh")){
				_dprintf("Don't have the install script.\n");
				nvram_set("apps_state_install", "");
			}

			unlink(PID_FILE);
			return 0;
		}

		memset(chk_value, 0, 4);
		strncpy(chk_value, nvram_safe_get("apps_state_upgrade"), 4);
		if(strcmp(chk_value, "")){
			if(atoi(chk_value) != APPS_UPGRADE_FINISHED && !pids("app_upgrade.sh")){
				_dprintf("Don't have the upgrade script.\n");
				nvram_set("apps_state_upgrade", "");
			}

			unlink(PID_FILE);
			return 0;
		}

		memset(chk_value, 0, 4);
		strncpy(chk_value, nvram_safe_get("apps_state_enable"), 4);
		if(strcmp(chk_value, "")){
			if(atoi(chk_value) != APPS_ENABLE_FINISHED && !pids("app_set_enabled.sh")){
				_dprintf("Don't have the enable script.\n");
				nvram_set("apps_state_enable", "");
			}

			unlink(PID_FILE);
			return 0;
		}

		unlink(PID_FILE);
		return 0;
	}
#endif
#endif
	else if(!strcmp(base, "ATE")) {
		if( argc == 2 || argc == 3 || argc == 4) {
			asus_ate_command(argv[1], argv[2], argv[3]);
		}
		else
			printf("ATE_ERROR\n");
                return 0;
	}
#if defined(RTCONFIG_RALINK)
	else if (!strcmp(base, "FWRITE")) {
		if (argc == 3)
			return FWRITE(argv[1], argv[2]);
		else
		return 0;
	}
	else if (!strcmp(base, "FREAD")) {
		if (argc == 3)
		{
			unsigned int addr;
			int len;
			addr = strtoul(argv[1], NULL, 16);
			if(argv[2][0] == '0' && argv[2][1] == 'x')
				len  = (int) strtoul(argv[2], NULL, 16);
			else
				len  = (int) strtoul(argv[2], NULL, 10);

			if(len > 0)
				return FREAD(addr, len);
		}
		printf("ATE_ERROR\n");
		return 0;
	}
	else if (!strcmp(base, "asuscfe_5g")) {
		if (argc == 2)
			return asuscfe(argv[1], WIF_5G);
		else
			return EINVAL;
	}
	else if (!strcmp(base, "asuscfe_2g")) {
		if (argc == 2)
			return asuscfe(argv[1], WIF_2G);
		else
			return EINVAL;
	}
	else if (!strcmp(base, "stainfo_2g")) {
		return stainfo(0);
	}
	else if (!strcmp(base, "stainfo_5g")) {
		return stainfo(1);
	}
#ifdef RTCONFIG_DSL
	else if(!strcmp(base, "gen_ralink_config")){
		if(argc != 3){
			printf("Usage: gen_ralink_config [band] [is_iNIC]\n");
			return 0;
		}
		return gen_ralink_config(atoi(argv[1]), atoi(argv[2]));
	}
#endif
#endif
	else if(!strcmp(base, "run_telnetd")) {
		run_telnetd();
		return 0;
	}
#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
	else if(!strcmp(base, "run_pptpd")) {
		start_pptpd();
		return 0;
	}
#endif
#ifdef RTCONFIG_PARENTALCTRL
	else if(!strcmp(base, "pc")) {
		pc_main(argc, argv);
		return 0;
	}
#endif
#if defined(CONFIG_BCMWL5) || (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER))
	else if (!strcmp(base, "wlcscan")) {
		return wlcscan_main();
	}
#endif
#ifdef RTCONFIG_WIRELESSREPEATER
	else if (!strcmp(base, "wlcconnect")) {
		return wlcconnect_main();
	}
	else if (!strcmp(base, "setup_dnsmq")) {
		if(argc != 2)
			return 0;

		return setup_dnsmq(atoi(argv[1]));
	}
#endif
	else if (!strcmp(base, "add_multi_routes")) {
		return add_multi_routes();
	}
	else if (!strcmp(base, "led_ctrl")) {
		if (argc != 3)
			return 0;

		return(led_control(atoi(argv[1]), atoi(argv[2])));
	}

#ifdef RTCONFIG_BCMARM
	/* mtd-erase2 [device] */
	else if (!strcmp(base, "mtd-erase2")) {
		if (argv[1] && ((!strcmp(argv[1], "boot")) ||
				(!strcmp(argv[1], "linux")) ||
				(!strcmp(argv[1], "linux2")) ||
				(!strcmp(argv[1], "rootfs")) ||
				(!strcmp(argv[1], "rootfs2")) ||
				(!strcmp(argv[1], "nvram")))) {
			return mtd_erase(argv[1]);
		} else {
			fprintf(stderr, "usage: mtd-erase2 [device]\n");
			return EINVAL;
		}
	}
	/* mtd-write2 [path] [device] */
	else if (!strcmp(base, "mtd-write2")) {
		if (argc >= 3)
			return mtd_write(argv[1], argv[2]);
		else {
			fprintf(stderr, "usage: mtd-write2 [path] [device]\n");
			return EINVAL;
		}
	}
#endif
	else if(!strcmp(base, "test_endian")){
		int num = 0x04030201;
		char c = *(char *)(&num);

		if(c == 0x04 || c == 0x01){
			if(c == 0x04)
				printf("Big.\n");
			else
				printf("Little.\n");
		}
		else
			printf("test error!\n");

		return 0;
	}
	else if (!strcmp(base, "free_caches")) {
		int c;
		unsigned int test_num;
		char *set_value = NULL;
		int clean_time = 1;
		int threshold = 0;

		if(argc){
			while((c = getopt(argc, argv, "c:w:t:")) != -1){
				switch(c){
					case 'c': // set the clean-cache mode: 0~3.
						test_num = strtol(optarg, NULL, 10);
        		if(test_num == LONG_MIN || test_num == LONG_MAX){
        			_dprintf("ERROR: unknown value %s...\n", optarg);
							return 0;
						}

						if(test_num < 0 || test_num > 3){
							_dprintf("ERROR: the value %s was over the range...\n", optarg);
							return 0;
						}

						set_value = optarg;

						break;
					case 'w': // set the waited time for cleaning.
						test_num = strtol(optarg, NULL, 10);
        		if(test_num < 0 || test_num == LONG_MIN || test_num == LONG_MAX){
        			_dprintf("ERROR: unknown value %s...\n", optarg);
							return 0;
						}

						clean_time = test_num;

						break;
					case 't': // set the waited time for cleaning.
						test_num = strtol(optarg, NULL, 10);
        		if(test_num < 0 || test_num == LONG_MIN || test_num == LONG_MAX){
        			_dprintf("ERROR: unknown value %s...\n", optarg);
							return 0;
						}

						threshold = test_num;

						break;
					default:
						fprintf(stderr, "Usage: free_caches [ -c clean_mode ] [ -w clean_time ] [ -t threshold ]\n");
						break;
				}
			}
		}

		if(!set_value)
			set_value = FREE_MEM_PAGE;

		free_caches(set_value, clean_time, threshold);

		return 0;
	}
#ifdef RTCONFIG_USB_MODEM
	else if(!strcmp(base, "write_3g_ppp_conf")){
		write_3g_ppp_conf();

		return 0;
	}
#endif
	printf("Unknown applet: %s\n", base);
	return 0;
}
