/*

	Tomato Firmware
	Copyright (C) 2006-2009 Jonathan Zarate

*/

#include "rc.h"

#include <sys/mount.h>
#include <sys/stat.h>


void start_cifs(void)
{
	xstart("mount-cifs", "-m");
}

void stop_cifs(void)
{
	killall("mount-cifs", SIGTERM);
	eval("mount-cifs", "-u");
}

int mount_cifs_main(int argc, char *argv[])
{
	char s[512];
	char opt[512];
	char mpath[32];
	int i, j;
	int try;
	int first;
	char *on, *unc, *user, *pass, *dom, *exec, *servern, *sec, *custom;
	int done[3];

	if (argc == 2) {
		if (strcmp(argv[1], "-m") == 0) {
			done[1] = 0;
			done[2] = 0;
			first = 1;
			try = 0;
			while (1) {
				for (i = 1; i <= 2; ++i) {
					if (done[i]) continue;

					done[i] = 2;

					sprintf(s, "cifs%d", i);
					strlcpy(s, nvram_safe_get(s), sizeof(s));
					if ((vstrsep(s, "<", &on, &unc, &user, &pass, &dom, &exec, &servern, &sec) != 8) || (*on != '1')) continue;
					custom = nvram_safe_get("cifs_opts");

					done[i] = 0;

					if (first) {
						notice_set("cifs", "Mounting...");
						modprobe("cifs");
						first = 0;
					}

					sprintf(mpath, "/cifs%d", i);
					umount(mpath);
# ifndef RTCONFIG_BCMARM
					j = sprintf(opt, "sep=<unc=%s", unc);
					if (*user) j += sprintf(opt + j, "<user=%s", user);
					if (*pass) j += sprintf(opt + j, "<pass=%s", pass);
					if (*dom) j += sprintf(opt + j, "<dom=%s", dom);
					if (*servern) j += sprintf(opt + j, "<servern=%s", servern);
					if (*sec) j += sprintf(opt + j, "<sec=%s", sec);
					if (*custom) j += sprintf(opt + j, "<%s", custom);

					if (mount("-", mpath, "cifs", MS_NOATIME|MS_NODIRATIME, opt) != 0) continue;
# else
					j = sprintf(opt, "nounix,noserverino");
					if (*user) j += sprintf(opt + j, ",username=%s", user);
					if (*pass) j += sprintf(opt + j, ",password=%s", pass);
	
					if (mount(unc, mpath, "cifs", MS_NOATIME|MS_NODIRATIME, opt) != 0) continue;
# endif
					done[i] = 1;
					if (try > 12) try = 12;	// 1 min

					if (*exec) {
						chdir(mpath);
						system(exec);
					}
					run_userfile(mpath, ".asusrouter", NULL, 3);
				}
				if ((done[1]) && (done[2])) {
					notice_set("cifs", "");
					return 0;
				}

				sleep(5 * ++try);
				if (try == 24) {	// 2 min
					sprintf(s, "Error mounting CIFS #%s. Still trying... ", (done[1] == done[2]) ? "1 and #2" : ((done[1] == 0) ? "1" : "2"));
					notice_set("cifs", s);
				}
				else if (try > 180) {	// 15 mins
					try = 180;
				}
			}

			return 1;
		}
		if (strcmp(argv[1], "-u") == 0) {
			for (i = 1; i <= 2; ++i) {
				sprintf(mpath, "/cifs%d", i);
				umount(mpath);
			}
			modprobe_r("cifs");
			notice_set("cifs", "");
			return 0;
		}
	}

	usage_exit(argv[0], "-m|-u");
	return 1;
}
