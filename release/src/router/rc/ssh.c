/*

	Tomato Firmware
	Copyright (C) 2006-2009 Jonathan Zarate

*/

#include "rc.h"
#include <shared.h>

static inline int check_host_key(const char *ktype, const char *nvname, const char *hkfn)
{
	unlink(hkfn);

	if (!nvram_get_file(nvname, hkfn, 2048)) {
		eval("dropbearkey", "-t", (char *)ktype, "-f", (char *)hkfn);
		if (nvram_set_file(nvname, hkfn, 2048)) {
			return 1;
		}
	}

	return 0;
}

char *get_parsed_key(const char *name, char *buf)
{
	char *value;
	int len, i;

	value = nvram_safe_get(name);

	len = strlen(value);
	if (len > 3500) len = 3500;

	for (i=0; (i < len); i++) {
		if (value[i] == '>')
			buf[i] = '\n';
		else
			buf[i] = value[i];
	}

	buf[i] = '\0';

	return buf;
}

int start_sshd(void)
{
	char buf[3500], *port;
	char *dropbear_argv[] = { "dropbear",
		"-p", buf,	/* -p [address:]port */
		"-a",
		NULL,		/* -s */
		NULL, NULL,	/* -W receive_window_buffer */
		NULL, NULL,	/* -a or -j -k */
		NULL };
	int index = 4;

	if (!nvram_get_int("sshd_enable"))
		return 0;

	if (getpid() != 1) {
		notify_rc("start_sshd");
		return 0;
	}

	mkdir("/etc/dropbear", 0700);
	mkdir("/root/.ssh", 0700);

	f_write_string("/root/.ssh/authorized_keys", get_parsed_key("sshd_authkeys", buf), 0, 0700);

	if (check_host_key("rsa", "sshd_hostkey", "/etc/dropbear/dropbear_rsa_host_key") |
	    check_host_key("dss", "sshd_dsskey",  "/etc/dropbear/dropbear_dss_host_key") |
	    check_host_key("ecdsa", "sshd_ecdsakey",  "/etc/dropbear/dropbear_ecdsa_host_key"))
		nvram_commit_x();

	port = buf;
	if (is_routing_enabled() && nvram_get_int("sshd_enable") != 1)
		port += snprintf(buf, sizeof(buf), "%s:", nvram_safe_get("lan_ipaddr"));
	snprintf(port, sizeof(buf) - (port - buf), "%d", nvram_get_int("sshd_port") ? : 22);

	if (!nvram_get_int("sshd_pass"))
		dropbear_argv[index++] = "-s";

	if (nvram_get_int("sshd_rwb")) {
		dropbear_argv[index++] = "-W";
		dropbear_argv[index++] = nvram_safe_get("sshd_rwb");
	}

	if (nvram_get_int("sshd_forwarding")) {
		dropbear_argv[index++] = "-a";
	} else {
		dropbear_argv[index++] = "-j";
		dropbear_argv[index++] = "-k";
	}

	return _eval(dropbear_argv, NULL, 0, NULL);
}

void stop_sshd(void)
{
	if (getpid() != 1) {
		notify_rc("stop_sshd");
		return;
	}

	if (pids("dropbear"))
		killall_tk("dropbear");
}
