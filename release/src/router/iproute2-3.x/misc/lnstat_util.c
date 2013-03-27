/* lnstat.c:  Unified linux network statistics
 *
 * Copyright (C) 2004 by Harald Welte <laforge@gnumonks.org>
 *
 * Development of this code was funded by Astaro AG, http://www.astaro.com/
 *
 * Based on original concept and ideas from predecessor rtstat.c:
 *
 * Copyright 2001 by Robert Olsson <robert.olsson@its.uu.se>
 *                                 Uppsala University, Sweden
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <time.h>

#include <sys/time.h>
#include <sys/types.h>

#include "lnstat.h"

/* size of temp buffer used to read lines from procfiles */
#define FGETS_BUF_SIZE 1024


#define RTSTAT_COMPAT_LINE "entries  in_hit in_slow_tot in_no_route in_brd in_martian_dst in_martian_src  out_hit out_slow_tot out_slow_mc  gc_total gc_ignored gc_goal_miss gc_dst_overflow in_hlist_search out_hlist_search\n"

/* Read (and summarize for SMP) the different stats vars. */
static int scan_lines(struct lnstat_file *lf, int i)
{
	int j, num_lines = 0;

	for (j = 0; j < lf->num_fields; j++)
		lf->fields[j].values[i] = 0;

	while(!feof(lf->fp)) {
		char buf[FGETS_BUF_SIZE];
		char *ptr = buf;

		num_lines++;

		fgets(buf, sizeof(buf)-1, lf->fp);
		gettimeofday(&lf->last_read, NULL);

		for (j = 0; j < lf->num_fields; j++) {
			unsigned long f = strtoul(ptr, &ptr, 16);
			if (j == 0)
				lf->fields[j].values[i] = f;
			else
				lf->fields[j].values[i] += f;
		}
	}
	return num_lines;
}

static int time_after(struct timeval *last,
		      struct timeval *tout,
		      struct timeval *now)
{
	if (now->tv_sec > last->tv_sec + tout->tv_sec)
		return 1;

	if (now->tv_sec == last->tv_sec + tout->tv_sec) {
		if (now->tv_usec > last->tv_usec + tout->tv_usec)
			return 1;
	}

	return 0;
}

int lnstat_update(struct lnstat_file *lnstat_files)
{
	struct lnstat_file *lf;
	char buf[FGETS_BUF_SIZE];
	struct timeval tv;

	gettimeofday(&tv, NULL);

	for (lf = lnstat_files; lf; lf = lf->next) {
		if (time_after(&lf->last_read, &lf->interval, &tv)) {
			int i;
			struct lnstat_field *lfi;

			rewind(lf->fp);
			if (!lf->compat) {
				/* skip first line */
				fgets(buf, sizeof(buf)-1, lf->fp);
			}
			scan_lines(lf, 1);

			for (i = 0, lfi = &lf->fields[i];
			     i < lf->num_fields; i++, lfi = &lf->fields[i]) {
				if (i == 0)
					lfi->result = lfi->values[1];
				else
					lfi->result = (lfi->values[1]-lfi->values[0])
				    			/ lf->interval.tv_sec;
			}

			rewind(lf->fp);
			fgets(buf, sizeof(buf)-1, lf->fp);
			scan_lines(lf, 0);
		}
	}

	return 0;
}

/* scan first template line and fill in per-field data structures */
static int __lnstat_scan_fields(struct lnstat_file *lf, char *buf)
{
	char *tok;
	int i;

	tok = strtok(buf, " \t\n");
	for (i = 0; i < LNSTAT_MAX_FIELDS_PER_LINE; i++) {
		lf->fields[i].file = lf;
		strncpy(lf->fields[i].name, tok, LNSTAT_MAX_FIELD_NAME_LEN);
		/* has to be null-terminate since we initialize to zero
		 * and field size is NAME_LEN + 1 */
		tok = strtok(NULL, " \t\n");
		if (!tok) {
			lf->num_fields = i+1;
			return 0;
		}
	}
	return 0;
}

static int lnstat_scan_fields(struct lnstat_file *lf)
{
	char buf[FGETS_BUF_SIZE];

	rewind(lf->fp);
	fgets(buf, sizeof(buf)-1, lf->fp);

	return __lnstat_scan_fields(lf, buf);
}

/* fake function emulating lnstat_scan_fields() for old kernels */
static int lnstat_scan_compat_rtstat_fields(struct lnstat_file *lf)
{
	char buf[FGETS_BUF_SIZE];

	strncpy(buf, RTSTAT_COMPAT_LINE, sizeof(buf)-1);

	return __lnstat_scan_fields(lf, buf);
}

/* find out whether string 'name; is in given string array */
static int name_in_array(const int num, const char **arr, const char *name)
{
	int i;
	for (i = 0; i < num; i++) {
		if (!strcmp(arr[i], name))
			return 1;
	}
	return 0;
}

/* allocate lnstat_file and open given file */
static struct lnstat_file *alloc_and_open(const char *path, const char *file)
{
	struct lnstat_file *lf;

	/* allocate */
	lf = malloc(sizeof(*lf));
	if (!lf)
		return NULL;

	/* initialize */
	memset(lf, 0, sizeof(*lf));

	/* de->d_name is guaranteed to be <= NAME_MAX */
	strcpy(lf->basename, file);
	strcpy(lf->path, path);
	strcat(lf->path, "/");
	strcat(lf->path, lf->basename);

	/* initialize to default */
	lf->interval.tv_sec = 1;

	/* open */
	lf->fp = fopen(lf->path, "r");
	if (!lf->fp) {
		free(lf);
		return NULL;
	}

	return lf;
}


/* lnstat_scan_dir - find and parse all available statistics files/fields */
struct lnstat_file *lnstat_scan_dir(const char *path, const int num_req_files,
				    const char **req_files)
{
	DIR *dir;
	struct lnstat_file *lnstat_files = NULL;
	struct dirent *de;

	if (!path)
		path = PROC_NET_STAT;

	dir = opendir(path);
	if (!dir) {
		struct lnstat_file *lf;
		/* Old kernel, before /proc/net/stat was introduced */
		fprintf(stderr, "Your kernel doesn't have lnstat support. ");

		/* we only support rtstat, not multiple files */
		if (num_req_files >= 2) {
			fputc('\n', stderr);
			return NULL;
		}

		/* we really only accept rt_cache */
		if (num_req_files && !name_in_array(num_req_files,
						    req_files, "rt_cache")) {
			fputc('\n', stderr);
			return NULL;
		}

		fprintf(stderr, "Fallback to old rtstat-only operation\n");

		lf = alloc_and_open("/proc/net", "rt_cache_stat");
		if (!lf)
			return NULL;
		lf->compat = 1;
		strncpy(lf->basename, "rt_cache", sizeof(lf->basename));

		/* FIXME: support for old files */
		if (lnstat_scan_compat_rtstat_fields(lf) < 0)
			return NULL;

		lf->next = lnstat_files;
		lnstat_files = lf;
		return lnstat_files;
	}

	while ((de = readdir(dir))) {
		struct lnstat_file *lf;

		if (de->d_type != DT_REG)
			continue;

		if (num_req_files && !name_in_array(num_req_files,
						    req_files, de->d_name))
			continue;

		lf = alloc_and_open(path, de->d_name);
		if (!lf)
			return NULL;

		/* fill in field structure */
		if (lnstat_scan_fields(lf) < 0)
			return NULL;

		/* prepend to global list */
		lf->next = lnstat_files;
		lnstat_files = lf;
	}
	closedir(dir);

	return lnstat_files;
}

int lnstat_dump(FILE *outfd, struct lnstat_file *lnstat_files)
{
	struct lnstat_file *lf;

	for (lf = lnstat_files; lf; lf = lf->next) {
		int i;

		fprintf(outfd, "%s:\n", lf->path);

		for (i = 0; i < lf->num_fields; i++)
			fprintf(outfd, "\t%2u: %s\n", i+1, lf->fields[i].name);

	}
	return 0;
}

struct lnstat_field *lnstat_find_field(struct lnstat_file *lnstat_files,
				       const char *name)
{
	struct lnstat_file *lf;
	struct lnstat_field *ret = NULL;
	const char *colon = strchr(name, ':');
	char *file;
	const char *field;

	if (colon) {
		file = strndup(name, colon-name);
		field = colon+1;
	} else {
		file = NULL;
		field = name;
	}

	for (lf = lnstat_files; lf; lf = lf->next) {
		int i;

		if (file && strcmp(file, lf->basename))
			continue;

		for (i = 0; i < lf->num_fields; i++) {
			if (!strcmp(field, lf->fields[i].name)) {
				ret = &lf->fields[i];
				goto out;
			}
		}
	}
out:
	if (file)
		free(file);

	return ret;
}
