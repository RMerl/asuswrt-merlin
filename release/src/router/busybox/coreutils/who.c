/* vi: set sw=4 ts=4: */
/*----------------------------------------------------------------------
 * Mini who is used to display user name, login time,
 * idle time and host name.
 *
 * Author: Da Chen  <dchen@ayrnetworks.com>
 *
 * This is a free document; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation:
 *    http://www.gnu.org/copyleft/gpl.html
 *
 * Copyright (c) 2002 AYR Networks, Inc.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 *----------------------------------------------------------------------
 */
/* BB_AUDIT SUSv3 _NOT_ compliant -- missing options -b, -d, -l, -m, -p, -q, -r, -s, -t, -T, -u; Missing argument 'file'.  */

#include "libbb.h"
#include <utmp.h>

static void idle_string(char *str6, time_t t)
{
	t = time(NULL) - t;

	/*if (t < 60) {
		str6[0] = '.';
		str6[1] = '\0';
		return;
	}*/
	if (t >= 0 && t < (24 * 60 * 60)) {
		sprintf(str6, "%02d:%02d",
				(int) (t / (60 * 60)),
				(int) ((t % (60 * 60)) / 60));
		return;
	}
	strcpy(str6, "old");
}

int who_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int who_main(int argc UNUSED_PARAM, char **argv)
{
	struct utmp *ut;
	unsigned opt;

	opt_complementary = "=0";
	opt = getopt32(argv, "aH");
	if (opt & 2) // -H
		printf("USER\t\tTTY\t\tIDLE\tTIME\t\t HOST\n");

	setutent();
	while ((ut = getutent()) != NULL) {
		if (ut->ut_user[0]
		 && ((opt & 1) || ut->ut_type == USER_PROCESS)
		) {
			char str6[6];
			char name[sizeof("/dev/") + sizeof(ut->ut_line) + 1];
			struct stat st;
			time_t seconds;

			str6[0] = '?';
			str6[1] = '\0';
			strcpy(name, "/dev/");
			safe_strncpy(ut->ut_line[0] == '/' ? name : name + sizeof("/dev/")-1,
				ut->ut_line,
				sizeof(ut->ut_line)+1
			);
			if (stat(name, &st) == 0)
				idle_string(str6, st.st_atime);
			/* manpages say ut_tv.tv_sec *is* time_t,
			 * but some systems have it wrong */
			seconds = ut->ut_tv.tv_sec;
			/* How wide time field can be?
			 * "Nov 10 19:33:20": 15 chars
			 * "2010-11-10 19:33": 16 chars
			 */
			printf("%-15.*s %-15.*s %-7s %-16.16s %.*s\n",
					(int)sizeof(ut->ut_user), ut->ut_user,
					(int)sizeof(ut->ut_line), ut->ut_line,
					str6,
					ctime(&seconds) + 4,
					(int)sizeof(ut->ut_host), ut->ut_host
			);
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		endutent();
	return EXIT_SUCCESS;
}
