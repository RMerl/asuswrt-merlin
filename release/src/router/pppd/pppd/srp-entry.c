/*
 * Utility program for generating entries in /etc/ppp/srp-secrets
 *
 * Copyright (c) 2001 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Non-exclusive rights to redistribute, modify, translate, and use
 * this software in source and binary forms, in whole or in part, is
 * hereby granted, provided that the above copyright notice is
 * duplicated in any source form, and that neither the name of the
 * copyright holder nor the author is used to endorse or promote
 * products derived from this software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Original version by James Carlson
 *
 * Usage:
 *	srp-entry [-i index] [clientname]
 *
 * Index, if supplied, is the modulus/generator index from
 * /etc/tpasswd.conf.  If not supplied, then the last (highest
 * numbered) entry from that file is used.  If the file doesn't exist,
 * then the default "well known" EAP SRP-SHA1 modulus/generator is
 * used.
 *
 * The default modulus/generator can be requested as index 0.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <t_pwd.h>

#ifndef	SOL2
#define	getpassphrase	getpass
#endif

#define	HAS_SPACE	1
#define	HAS_DQUOTE	2
#define	HAS_SQUOTE	4
#define	HAS_BACKSLASH	8

static const u_char wkmodulus[] = {
	0xAC, 0x6B, 0xDB, 0x41, 0x32, 0x4A, 0x9A, 0x9B,
	0xF1, 0x66, 0xDE, 0x5E, 0x13, 0x89, 0x58, 0x2F,
	0xAF, 0x72, 0xB6, 0x65, 0x19, 0x87, 0xEE, 0x07,
	0xFC, 0x31, 0x92, 0x94, 0x3D, 0xB5, 0x60, 0x50,
	0xA3, 0x73, 0x29, 0xCB, 0xB4, 0xA0, 0x99, 0xED,
	0x81, 0x93, 0xE0, 0x75, 0x77, 0x67, 0xA1, 0x3D,
	0xD5, 0x23, 0x12, 0xAB, 0x4B, 0x03, 0x31, 0x0D,
	0xCD, 0x7F, 0x48, 0xA9, 0xDA, 0x04, 0xFD, 0x50,
	0xE8, 0x08, 0x39, 0x69, 0xED, 0xB7, 0x67, 0xB0,
	0xCF, 0x60, 0x95, 0x17, 0x9A, 0x16, 0x3A, 0xB3,
	0x66, 0x1A, 0x05, 0xFB, 0xD5, 0xFA, 0xAA, 0xE8,
	0x29, 0x18, 0xA9, 0x96, 0x2F, 0x0B, 0x93, 0xB8,
	0x55, 0xF9, 0x79, 0x93, 0xEC, 0x97, 0x5E, 0xEA,
	0xA8, 0x0D, 0x74, 0x0A, 0xDB, 0xF4, 0xFF, 0x74,
	0x73, 0x59, 0xD0, 0x41, 0xD5, 0xC3, 0x3E, 0xA7,
	0x1D, 0x28, 0x1E, 0x44, 0x6B, 0x14, 0x77, 0x3B,
	0xCA, 0x97, 0xB4, 0x3A, 0x23, 0xFB, 0x80, 0x16,
	0x76, 0xBD, 0x20, 0x7A, 0x43, 0x6C, 0x64, 0x81,
	0xF1, 0xD2, 0xB9, 0x07, 0x87, 0x17, 0x46, 0x1A,
	0x5B, 0x9D, 0x32, 0xE6, 0x88, 0xF8, 0x77, 0x48,
	0x54, 0x45, 0x23, 0xB5, 0x24, 0xB0, 0xD5, 0x7D,
	0x5E, 0xA7, 0x7A, 0x27, 0x75, 0xD2, 0xEC, 0xFA,
	0x03, 0x2C, 0xFB, 0xDB, 0xF5, 0x2F, 0xB3, 0x78,
	0x61, 0x60, 0x27, 0x90, 0x04, 0xE5, 0x7A, 0xE6,
	0xAF, 0x87, 0x4E, 0x73, 0x03, 0xCE, 0x53, 0x29,
	0x9C, 0xCC, 0x04, 0x1C, 0x7B, 0xC3, 0x08, 0xD8,
	0x2A, 0x56, 0x98, 0xF3, 0xA8, 0xD0, 0xC3, 0x82,
	0x71, 0xAE, 0x35, 0xF8, 0xE9, 0xDB, 0xFB, 0xB6,
	0x94, 0xB5, 0xC8, 0x03, 0xD8, 0x9F, 0x7A, 0xE4,
	0x35, 0xDE, 0x23, 0x6D, 0x52, 0x5F, 0x54, 0x75,
	0x9B, 0x65, 0xE3, 0x72, 0xFC, 0xD6, 0x8E, 0xF2,
	0x0F, 0xA7, 0x11, 0x1F, 0x9E, 0x4A, 0xFF, 0x73
};

static const char *myname;

static void
usage(void)
{
	(void) fprintf(stderr, "Usage:\n\t%s [-i index] [clientname]\n",
	    myname);
	exit(1);
}

int
main(int argc, char **argv)
{
	struct t_conf *tc;
	struct t_confent *tcent, mytce;
	struct t_pw pwval;
	char *name;
	char pname[256];
	char *pass1, *pass2;
	int flags, idx;
	char *cp;
	char delimit;
	char strbuf[MAXB64PARAMLEN];
	char saltbuf[MAXB64SALTLEN];

	if ((myname = *argv) == NULL)
		myname = "srp-entry";
	else
		argv++;

	idx = -1;
	if (*argv != NULL && strcmp(*argv, "-i") == 0) {
		if (*++argv == NULL)
			usage();
		idx = atoi(*argv++);
	}

	tcent = NULL;
	if (idx != 0 && (tc = t_openconf(NULL)) != NULL) {
		if (idx == -1)
			tcent = t_getconflast(tc);
		else
			tcent = t_getconfbyindex(tc, idx);
	}
	if (idx <= 0 && tcent == NULL) {
		mytce.index = 0;
		mytce.modulus.data = (u_char *)wkmodulus;
		mytce.modulus.len = sizeof (wkmodulus);
		mytce.generator.data = (u_char *)"\002";
		mytce.generator.len = 1;
		tcent = &mytce;
	}
	if (tcent == NULL) {
		(void) fprintf(stderr, "SRP modulus/generator %d not found\n",
		    idx);
		exit(1);
	}

	if ((name = *argv) == NULL) {
		(void) printf("Client name: ");
		if (fgets(pname, sizeof (pname), stdin) == NULL)
			exit(1);
		if ((cp = strchr(pname, '\n')) != NULL)
			*cp = '\0';
		name = pname;
	}

	for (;;) {
		if ((pass1 = getpassphrase("Pass phrase: ")) == NULL)
			exit(1);
		pass1 = strdup(pass1);
		if ((pass2 = getpassphrase("Re-enter phrase: ")) == NULL)
			exit(1);
		if (strcmp(pass1, pass2) == 0)
			break;
		free(pass1);
		(void) printf("Phrases don't match; try again.\n");
	}

	memset(&pwval, 0, sizeof (pwval));
	t_makepwent(&pwval, name, pass1, NULL, tcent);
	flags = 0;
	for (cp = name; *cp != '\0'; cp++)
		if (isspace(*cp))
			flags |= HAS_SPACE;
		else if (*cp == '"')
			flags |= HAS_DQUOTE;
		else if (*cp == '\'')
			flags |= HAS_SQUOTE;
		else if (*cp == '\\')
			flags |= HAS_BACKSLASH;
	delimit = flags == 0 ? '\0' : (flags & HAS_DQUOTE) ? '\'' : '"';
	if (delimit != '\0')
		(void) putchar(delimit);
	for (cp = name; *cp != '\0'; cp++) {
		if (*cp == delimit || *cp == '\\')
			(void) putchar('\\');
		(void) putchar(*cp);
	}
	if (delimit != '\0')
		(void) putchar(delimit);
	(void) printf(" * %d:%s:%s *\n",
	    pwval.pebuf.index, t_tob64(strbuf,
		(char *)pwval.pebuf.password.data, pwval.pebuf.password.len),
	    t_tob64(saltbuf, (char *)pwval.pebuf.salt.data,
		pwval.pebuf.salt.len));
	return 0;
}
