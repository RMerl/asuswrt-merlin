/*
 * config.c - blkid.conf routines
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdint.h>
#include <stdarg.h>

#include "blkidP.h"
#include "env.h"

static int parse_evaluate(struct blkid_config *conf, char *s)
{
	while(s && *s) {
		char *sep;

		if (conf->nevals >= __BLKID_EVAL_LAST)
			goto err;
		sep = strchr(s, ',');
		if (sep)
			*sep = '\0';
		if (strcmp(s, "udev") == 0)
			conf->eval[conf->nevals] = BLKID_EVAL_UDEV;
		else if (strcmp(s, "scan") == 0)
			conf->eval[conf->nevals] = BLKID_EVAL_SCAN;
		else
			goto err;
		conf->nevals++;
		if (sep)
			s = sep + 1;
		else
			break;
	}
	return 0;
err:
	DBG(DEBUG_CONFIG, printf(
		"config file: unknown evaluation method '%s'.\n", s));
	return -1;
}

static int parse_next(FILE *fd, struct blkid_config *conf)
{
	char buf[BUFSIZ];
	char *s;

	/* read the next non-blank non-comment line */
	do {
		if (fgets (buf, sizeof(buf), fd) == NULL)
			return feof(fd) ? 0 : -1;
		s = strchr (buf, '\n');
		if (!s) {
			/* Missing final newline?  Otherwise extremely */
			/* long line - assume file was corrupted */
			if (feof(fd))
				s = strchr (buf, '\0');
			else {
				DBG(DEBUG_CONFIG, fprintf(stderr,
					"libblkid: config file: missing newline at line '%s'.\n",
					buf));
				return -1;
			}
		}
		*s = '\0';
		if (--s >= buf && *s == '\r')
			*s = '\0';

		s = buf;
		while (*s == ' ' || *s == '\t')		/* skip space */
			s++;

	} while (*s == '\0' || *s == '#');

	if (!strncmp(s, "SEND_UEVENT=", 12)) {
		s += 13;
		if (*s && !strcasecmp(s, "yes"))
			conf->uevent = TRUE;
		else if (*s)
			conf->uevent = FALSE;
	} else if (!strncmp(s, "CACHE_FILE=", 11)) {
		s += 11;
		if (*s)
			conf->cachefile = blkid_strdup(s);
	} else if (!strncmp(s, "EVALUATE=", 9)) {
		s += 9;
		if (*s && parse_evaluate(conf, s) == -1)
			return -1;
	} else {
		DBG(DEBUG_CONFIG, printf(
			"config file: unknown option '%s'.\n", s));
		return -1;
	}
	return 0;
}

/* return real config data or built-in default */
struct blkid_config *blkid_read_config(const char *filename)
{
	struct blkid_config *conf;
	FILE *f;

	if (!filename)
		filename = safe_getenv("BLKID_CONF");
	if (!filename)
		filename = BLKID_CONFIG_FILE;

	conf = (struct blkid_config *) calloc(1, sizeof(*conf));
	if (!conf)
		return NULL;
	conf->uevent = -1;

	DBG(DEBUG_CONFIG, fprintf(stderr,
		"reading config file: %s.\n", filename));

	f = fopen(filename, "r");
	if (!f) {
		DBG(DEBUG_CONFIG, fprintf(stderr,
			"%s: does not exist, using built-in default\n", filename));
		goto dflt;
	}
	while (!feof(f)) {
		if (parse_next(f, conf)) {
			DBG(DEBUG_CONFIG, fprintf(stderr,
				"%s: parse error\n", filename));
			goto err;
		}
	}
dflt:
	if (!conf->nevals) {
		conf->eval[0] = BLKID_EVAL_UDEV;
		conf->eval[1] = BLKID_EVAL_SCAN;
		conf->nevals = 2;
	}
	if (!conf->cachefile)
		conf->cachefile = blkid_strdup(BLKID_CACHE_FILE);
	if (conf->uevent == -1)
		conf->uevent = TRUE;
	if (f)
		fclose(f);
	return conf;
err:
	free(conf);
	fclose(f);
	return NULL;
}

void blkid_free_config(struct blkid_config *conf)
{
	if (!conf)
		return;
	free(conf->cachefile);
	free(conf);
}

#ifdef TEST_PROGRAM
/*
 * usage: tst_config [<filename>]
 */
int main(int argc, char *argv[])
{
	int i;
	struct blkid_config *conf;
	char *filename = NULL;

	blkid_init_debug(DEBUG_ALL);

	if (argc == 2)
		filename = argv[1];

	conf = blkid_read_config(filename);
	if (!conf)
		return EXIT_FAILURE;

	printf("EVALUATE:    ");
	for (i = 0; i < conf->nevals; i++)
		printf("%s ", conf->eval[i] == BLKID_EVAL_UDEV ? "udev" : "scan");
	printf("\n");

	printf("SEND UEVENT: %s\n", conf->uevent ? "TRUE" : "FALSE");
	printf("CACHE_FILE:  %s\n", conf->cachefile);

	blkid_free_config(conf);
	return EXIT_SUCCESS;
}
#endif
