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

void start_sshd(void)
{
	int dirty = 0;
	char buf[3500];

        if (!nvram_match("sshd_enable", "1"))
                return;

	mkdir("/etc/dropbear", 0700);
	mkdir("/root/.ssh", 0700);

	f_write_string("/root/.ssh/authorized_keys", get_parsed_key("sshd_authkeys", buf), 0, 0700);

	dirty |= check_host_key("rsa", "sshd_hostkey", "/etc/dropbear/dropbear_rsa_host_key");
	dirty |= check_host_key("dss", "sshd_dsskey",  "/etc/dropbear/dropbear_dss_host_key");
	dirty |= check_host_key("ecdsa", "sshd_ecdsakey",  "/etc/dropbear/dropbear_ecdsa_host_key");
	if (dirty)
		nvram_commit_x();

/*
	xstart("dropbear", "-a", "-p", nvram_safe_get("sshd_port"), nvram_get_int("sshd_pass") ? "" : "-s");
*/

	char *argv[9];
	int argc;
	char *p;

	argv[0] = "dropbear";
	argv[1] = "-p";
	argv[2] = nvram_safe_get("sshd_port");
	argc = 3;

	if (!nvram_get_int("sshd_pass")) argv[argc++] = "-s";

	if (nvram_get_int("sshd_forwarding")) {
		argv[argc++] = "-a";
	} else {
		argv[argc++] = "-j";
		argv[argc++] = "-k";
	}

	if (((p = nvram_get("sshd_rwb")) != NULL) && (*p)) {
		argv[argc++] = "-W";
		argv[argc++] = p;
	}

	argv[argc] = NULL;
	_eval(argv, NULL, 0, NULL);

}

void stop_sshd(void)
{
	killall("dropbear", SIGTERM);
}
