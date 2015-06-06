#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include <bcmnvram.h>
#include <netconf.h>
#include <shutils.h>

#include <rc.h>
#include <shared.h>

#ifdef RTCONFIG_EAPOL
static int
start_wpa_supplicant(int unit, int restart)
{
	FILE *fp;
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char options[sizeof("/etc/wpa_supplicantXXXXXXXXXX.conf")];
	char pidfile[sizeof("/var/run/wpa_supplicantXXXXXXXXXX.pid")];
	char *wpa_argv[] = {"/usr/sbin/wpa_supplicant",
		"-B", "-W",
		"-i", NULL,	/* interface */
		"-D", "wired",
		"-c", options,
		"-P", pidfile,
		NULL
	};
	char *cli_argv[] = {"/usr/sbin/wpa_cli",
		"-B",
		"-i", NULL,	/* interface */
		"-a", "/tmp/wpa_cli",
		NULL};
	int ret;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(options, sizeof(options), "/etc/wpa_supplicant%d.conf", unit);
	snprintf(pidfile, sizeof(pidfile), "/var/run/wpa_supplicant%d.pid", unit);

	if (restart && kill_pidfile_s(pidfile, 0) == 0)
		return kill_pidfile_s(pidfile, SIGUSR2);

	/* Get interface */
	wpa_argv[4] = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
	cli_argv[3] = wpa_argv[4];

	/* Get driver, wired default */
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else	/* Both BCM 5.x and 6.x */
	if (get_switch() == SWITCH_BCM5325)
		wpa_argv[6] = "roboswitch";
#endif

	/* Generate options file */
	if ((fp = fopen(options, "w")) == NULL) {
		perror(options);
		return -1;
	}
	fprintf(fp,
		"ctrl_interface=/var/run/wpa_supplicant\n"
		"ap_scan=0\n"
		"fast_reauth=1\n"
		"network={\n"
		"	key_mgmt=IEEE8021X\n"
		"	eap=MD5\n"
		"	identity=\"%s\"\n"
		"	password=\"%s\"\n"
		"	eapol_flags=0\n"
		"}\n",
		nvram_safe_get(strcat_r(prefix, "pppoe_username", tmp)),
		nvram_safe_get(strcat_r(prefix, "pppoe_passwd", tmp)));
	fclose(fp);

	/* Start supplicant & monitor */
	ret = _eval(wpa_argv, NULL, 0, NULL);
	if (ret == 0)
		_eval(cli_argv, NULL, 0, NULL);

	return 0;
}

static int
stop_wpa_supplicant(int unit)
{
	char pidfile[sizeof("/var/run/wpa_supplicantXXXXXXXXXX.pid")];

	snprintf(pidfile, sizeof(pidfile), "/var/run/wpa_supplicant%d.pid", unit);
	kill_pidfile_tk(pidfile);

	/* It dies automagicaly after the supplicant is gone,
	if (pids("wpa_cli"))
		killall_tk("wpa_cli"); */

	return 0;
}

int
wpacli_main(int argc, char **argv)
{
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	int unit;

	if (argc < 3 || (unit = wan_ifunit(argv[1])) < 0)
		return EINVAL;
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	if (!nvram_match(strcat_r(prefix, "auth_x", tmp), "8021x-md5"))
		return 0;

	if (strncmp(argv[2], "CONNECTED", sizeof("CONNECTED")) == 0)
	{
		/* Called only once, no action */
	}
	if (strncmp(argv[2], "EAP-SUCCESS", sizeof("EAP-SUCCESS")) == 0)
	{
		/* Called multiple times on success (re)auth */
		logmessage("802.1x client", "authenticated");
		/* TODO: refresh dhcp? */
	} else
	if (strncmp(argv[2], "EAP-FAILURE", sizeof("EAP-FAILURE")) == 0)
	{
		/* Called every/multiple times on (re)auth fail and/or timeout */
		if (nvram_get_int(strcat_r(prefix, "state_t", tmp)) != WAN_STATE_STOPPED)
			logmessage("802.1x client", "authentication failed");

		/* Reuse auth-fail state */
		update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_PPP_AUTH_FAIL);
	}
	if (strncmp(argv[2], "DISCONNECTED", sizeof("DISCONNECTED")) == 0)
	{
		/* Called only once, no action */
	}

	return 0;
}
#endif

int
start_auth(int unit, int wan_up)
{
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char *wan_proto, *wan_auth;
	int ret = -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	if (strcmp(wan_proto, "static") != 0 &&
	    strcmp(wan_proto, "dhcp") != 0)
		return -1;

	wan_auth = nvram_safe_get(strcat_r(prefix, "auth_x", tmp));
#ifdef RTCONFIG_EAPOL
	if (strcmp(wan_auth, "8021x-md5") == 0 && !wan_up)
		ret = start_wpa_supplicant(unit, 0);
#endif

	_dprintf("%s:: done\n", __func__);
	return ret;
}

int
stop_auth(int unit, int wan_down)
{
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char *wan_proto;
	int ret = -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	if (strcmp(wan_proto, "static") != 0 &&
	    strcmp(wan_proto, "dhcp") != 0)
		return -1;

#ifdef RTCONFIG_EAPOL
	if (!wan_down)
		ret = stop_wpa_supplicant(unit);
#endif

	_dprintf("%s:: done\n", __func__);
	return ret;
}

int
restart_auth(int unit)
{
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char *wan_proto, *wan_auth;
	int ret = -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	if (strcmp(wan_proto, "static") != 0 &&
	    strcmp(wan_proto, "dhcp") != 0)
		return -1;

	wan_auth = nvram_safe_get(strcat_r(prefix, "auth_x", tmp));
#ifdef RTCONFIG_EAPOL
	if (strcmp(wan_auth, "8021x-md5") == 0)
		ret = start_wpa_supplicant(unit, 1);
#endif

	_dprintf("%s:: done\n", __func__);
	return ret;
}
