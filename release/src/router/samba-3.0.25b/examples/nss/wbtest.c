/* 
   nss sample code for extended winbindd functionality

   Copyright (C) Andrew Tridgell (tridge@samba.org)   

   you are free to use this code in any way you see fit, including
   without restriction, using this code in your own products. You do
   not need to give any attribution.
*/

/*
   compile like this:

      cc -o wbtest wbtest.c nss_winbind.c -ldl

   and run like this:

      ./wbtest /lib/libnss_winbind.so
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <nss.h>
#include <dlfcn.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "nss_winbind.h"

static int nss_test_users(struct nss_state *nss)
{
	struct passwd pwd;

	if (nss_setpwent(nss) != 0) {
		perror("setpwent");
		return -1;
	}

	/* loop over all users */
	while ((nss_getpwent(nss, &pwd) == 0)) {
		char *sid, **group_sids, *name2;
		int i;

		printf("User %s\n", pwd.pw_name);
		if (nss_nametosid(nss, pwd.pw_name, &sid) != 0) {
			perror("nametosid");
			return -1;
		}
		printf("\tSID %s\n", sid);

		if (nss_sidtoname(nss, sid, &name2) != 0) {
			perror("sidtoname");
			return -1;
		}
		printf("\tSID->name %s\n", name2);

		if (nss_getusersids(nss, sid, &group_sids) != 0) {
			perror("getusersids");
			return -1;
		}

		printf("\tGroups:\n");
		for (i=0; group_sids[i]; i++) {
			printf("\t\t%s\n", group_sids[i]);
			free(group_sids[i]);
		}

		free(sid);
		free(name2);
		free(group_sids);
	}


	if (nss_endpwent(nss) != 0) {
		perror("endpwent");
		return -1;
	}

	return 0;
}


/*
  main program. It lists all users, listing user SIDs for each user
 */
int main(int argc, char *argv[])
{	
	struct nss_state nss;
	const char *so_path = "/lib/libnss_winbind.so";
	int ret;

	if (argc > 1) {
		so_path = argv[1];
	}

	if (nss_open(&nss, so_path) != 0) {
		perror("nss_open");
		exit(1);
	}

	ret = nss_test_users(&nss);

	nss_close(&nss);

	return ret;
}
