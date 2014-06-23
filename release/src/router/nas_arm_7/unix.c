/*
 * Unix support routines
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: unix.c 241182 2011-02-17 21:50:03Z $
 */

#include <typedefs.h>
#include <stdio.h>
#include <string.h>

void initopt(void);
int getopt(int argc, char **argv, char *ostr);

/* ****************************************** */
/* Some support functionality to match unix */

/* get option letter from argument vector  */
int opterr = 1;
int optind = 1;
int optopt;                     /* character checked for validity */
char *optarg;                /* argument associated with option */

#define EMSG	""
#define BADCH ('?')

static char *place = EMSG;        /* option letter processing */

void initopt(void)
{
	opterr = 1;
	optind = 1;
	place = EMSG;        /* option letter processing */
}

int getopt(int argc, char **argv, char *ostr)
{
	register char *oli;                        /* option letter list index */

	if (!*place) {
		/* update scanning pointer */
		if (optind >= argc || *(place = argv[optind]) != '-' || !*++place) {
			return EOF;
		}
		if (*place == '-') {
			/* found "--" */
			++optind;
			return EOF;
		}
	}

	/* option letter okay? */
	if ((optopt = (int)*place++) == (int)':' ||
	    !(oli = strchr(ostr, optopt))) {
		if (!*place) {
			++optind;
		}
		printf("illegal option");
		return BADCH;
	}
	if (*++oli != ':') {
		/* don't need argument */
		optarg = NULL;
		if (!*place)
			++optind;
	} else {
		/* need an argument */
		if (*place) {
			optarg = place;                /* no white space */
		} else  if (argc <= ++optind) {
			/* no arg */
			place = EMSG;
			printf("option requires an argument");
			return BADCH;
		} else {
			optarg = argv[optind];                /* white space */
		}
		place = EMSG;
		++optind;
	}
	return optopt; /* return option letter */
}
