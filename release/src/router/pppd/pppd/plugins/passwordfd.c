
/*
 *  Author: Arvin Schnell <arvin@suse.de>
 *
 *  This plugin let's you pass the password to the pppd via
 *  a file descriptor. That's easy and secure - no fiddling
 *  with pap- and chap-secrets files.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "pppd.h"

char pppd_version[] = VERSION;

static int passwdfd = -1;
static char save_passwd[MAXSECRETLEN];

static option_t options[] = {
    { "passwordfd", o_int, &passwdfd,
      "Receive password on this file descriptor" },
    { NULL }
};

static int pwfd_check (void)
{
    return 1;
}

static int pwfd_passwd (char *user, char *passwd)
{
    int readgood, red;

    if (passwdfd == -1)
	return -1;

    if (passwd == NULL)
	return 1;

    if (passwdfd == -2) {
	strcpy (passwd, save_passwd);
	return 1;
    }

    readgood = 0;
    do {
	red = read (passwdfd, passwd + readgood, MAXSECRETLEN - 1 - readgood);
	if (red == 0)
	    break;
	if (red < 0) {
	    error ("Can't read secret from fd\n");
	    readgood = -1;
	    break;
	}
	readgood += red;
    } while (readgood < MAXSECRETLEN - 1);

    close (passwdfd);

    if (readgood < 0)
	return 0;

    passwd[readgood] = 0;
    strcpy (save_passwd, passwd);
    passwdfd = -2;

    return 1;
}

void plugin_init (void)
{
    add_options (options);

    pap_check_hook = pwfd_check;
    pap_passwd_hook = pwfd_passwd;

    chap_check_hook = pwfd_check;
    chap_passwd_hook = pwfd_passwd;
}
