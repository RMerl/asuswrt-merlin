/* lnstat - Unified linux network statistics
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

/* Maximum number of fields that can be displayed */
#define MAX_FIELDS		128

/* Maximum number of header lines */
#define HDR_LINES 		10

/* default field width if none specified */
#define FIELD_WIDTH_DEFAULT	8
#define FIELD_WIDTH_MAX		20

#define DEFAULT_INTERVAL	2

#define HDR_LINE_LENGTH		(MAX_FIELDS*FIELD_WIDTH_MAX)

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "lnstat.h"

static struct option opts[] = {
	{ "version", 0, NULL, 'V' },
	{ "count", 1, NULL, 'c' },
	{ "dump", 1, NULL, 'd' },
	{ "file", 1, NULL, 'f' },
	{ "help", 0, NULL, 'h' },
	{ "interval", 1, NULL, 'i' },
	{ "keys", 1, NULL, 'k' },
	{ "subject", 1, NULL, 's' },
	{ "width", 1, NULL, 'w' },
};

static int usage(char *name, int exit_code)
{
	fprintf(stderr, "%s Version %s\n", name, LNSTAT_VERSION);
	fprintf(stderr, "Copyright (C) 2004 by Harald Welte "
			"<laforge@gnumonks.org>\n");
	fprintf(stderr, "This program is free software licensed under GNU GPLv2"
			"\nwith ABSOLUTELY NO WARRANTY.\n\n");
	fprintf(stderr, "Parameters:\n");
	fprintf(stderr, "\t-V --version\t\tPrint Version of Program\n");
	fprintf(stderr, "\t-c --count <count>\t"
			"Print <count> number of intervals\n");
	fprintf(stderr, "\t-d --dump\t\t"
			"Dump list of available files/keys\n");
	fprintf(stderr, "\t-f --file <file>\tStatistics file to use\n");
	fprintf(stderr, "\t-h --help\t\tThis help message\n");
	fprintf(stderr, "\t-i --interval <intv>\t"
			"Set interval to 'intv' seconds\n");
	fprintf(stderr, "\t-k --keys k,k,k,...\tDisplay only keys specified\n");
	fprintf(stderr, "\t-s --subject [0-2]\t?\n");
	fprintf(stderr, "\t-w --width n,n,n,...\tWidth for each field\n");
	fprintf(stderr, "\n");

	exit(exit_code);
}

struct field_param {
	const char *name;
	struct lnstat_field *lf;
	struct {
		unsigned int width;
	} print;
};

struct field_params {
	unsigned int num;
	struct field_param params[MAX_FIELDS];
};

static void print_line(FILE *of, const struct lnstat_file *lnstat_files,
		       const struct field_params *fp)
{
	int i;

	for (i = 0; i < fp->num; i++) {
		struct lnstat_field *lf = fp->params[i].lf;
		char formatbuf[255];

		snprintf(formatbuf, sizeof(formatbuf)-1, "%%%ulu|",
			 fp->params[i].print.width);
		fprintf(of, formatbuf, lf->result);
	}
	fputc('\n', of);
}

/* find lnstat_field according to user specification */
static int map_field_params(struct lnstat_file *lnstat_files,
			    struct field_params *fps, int interval)
{
	int i, j = 0;
	struct lnstat_file *lf;

	/* no field specification on commandline, need to build default */
	if (!fps->num) {
		for (lf = lnstat_files; lf; lf = lf->next) {
			for (i = 0; i < lf->num_fields; i++) {
				fps->params[j].lf = &lf->fields[i];
				fps->params[j].lf->file->interval.tv_sec =
								interval;
				if (!fps->params[j].print.width)
					fps->params[j].print.width =
							FIELD_WIDTH_DEFAULT;
				
				if (++j >= MAX_FIELDS - 1) {
					fprintf(stderr,
						"WARN: MAX_FIELDS (%d) reached,"
						" truncating number of keys\n",
						MAX_FIELDS);
					goto full;
				}
			}
		}
	full:
		fps->num = j;
		return 1;
	}

	for (i = 0; i < fps->num; i++) {
		fps->params[i].lf = lnstat_find_field(lnstat_files,
						      fps->params[i].name);
		if (!fps->params[i].lf) {
			fprintf(stderr, "Field `%s' unknown\n",
				fps->params[i].name);
			return 0;
		}
		fps->params[i].lf->file->interval.tv_sec = interval;
		if (!fps->params[i].print.width)
			fps->params[i].print.width = FIELD_WIDTH_DEFAULT;
	}
	return 1;
}

struct table_hdr {
	int num_lines;
	char *hdr[HDR_LINES];
};

static struct table_hdr *build_hdr_string(struct lnstat_file *lnstat_files,
					  struct field_params *fps,
					  int linewidth)
{
	int h,i;
	static struct table_hdr th;
	int ofs = 0;

	for (i = 0; i < HDR_LINES; i++) {
		th.hdr[i] = malloc(HDR_LINE_LENGTH);
		memset(th.hdr[i], 0, sizeof(th.hdr[i]));
	}

	for (i = 0; i < fps->num; i++) {
		char *cname, *fname = fps->params[i].lf->name;
		char fmt[12];
		unsigned int width = fps->params[i].print.width;

		snprintf(fmt, sizeof(fmt)-1, "%%%u.%us|", width, width);

		snprintf(th.hdr[0]+ofs, width+2, fmt,
			 fps->params[i].lf->file->basename);

		cname = fname;
		for (h = 1; h < HDR_LINES; h++) {
			if (cname - fname >= strlen(fname))
				snprintf(th.hdr[h]+ofs, width+2, fmt, "");
			else {
				th.num_lines = h+1;
				snprintf(th.hdr[h]+ofs, width+2, fmt, cname);
			}
			cname += width;
		}
		ofs += width+1;
	}
	/* fill in spaces */
	for (h = 1; h <= th.num_lines; h++) {
		for (i = 0; i < ofs; i++) {
			if (th.hdr[h][i] == '\0')
				th.hdr[h][i] = ' ';
		}
	}

	return &th;
}

static int print_hdr(FILE *of, struct table_hdr *th)
{
	int i;

	for (i = 0; i < th->num_lines; i++) {
		fputs(th->hdr[i], of);
		fputc('\n', of);
	}
	return 0;
}


int main(int argc, char **argv)
{
	struct lnstat_file *lnstat_files;
	const char *basename;
	int c;
	int interval = DEFAULT_INTERVAL;
	int hdr = 2;
	enum {
		MODE_DUMP,
		MODE_NORMAL,
	} mode = MODE_NORMAL;

	unsigned long count = 1;
	static struct field_params fp;
	int num_req_files = 0;
	char *req_files[LNSTAT_MAX_FILES];

	/* backwards compatibility mode for old tools */
	basename = strrchr(argv[0], '/');
	if (basename)
		basename += 1;	  /* name after slash */
	else
		basename = argv[0]; /* no slash */

	if (!strcmp(basename, "rtstat")) {
		/* rtstat compatibility mode */
		req_files[0] = "rt_cache";
		num_req_files = 1;
	} else if (!strcmp(basename, "ctstat")) {
		/* ctstat compatibility mode */
		req_files[0] = "ip_conntrack";
		num_req_files = 1;
	}

	while ((c = getopt_long(argc, argv,"Vc:df:h?i:k:s:w:",
				opts, NULL)) != -1) {
		int i, len = 0;
		char *tmp, *tok;

		switch (c) {
			case 'c':
				count = strtoul(optarg, NULL, 0);
				break;
			case 'd':
				mode = MODE_DUMP;
				break;
			case 'f':
				req_files[num_req_files++] = strdup(optarg);
				break;
			case '?':
			case 'h':
				usage(argv[0], 0);
				break;
			case 'i':
				sscanf(optarg, "%u", &interval);
				break;
			case 'k':
				tmp = strdup(optarg);
				if (!tmp)
					break;
				for (tok = strtok(tmp, ",");
				     tok;
				     tok = strtok(NULL, ",")) {
					if (fp.num >= MAX_FIELDS) {
						fprintf(stderr, 
							"WARN: too many keys"
							" requested: (%d max)\n",
							MAX_FIELDS);
						break;
					}
					fp.params[fp.num++].name = tok;
				}
				break;
			case 's':
				sscanf(optarg, "%u", &hdr);
				break;
			case 'w':
				tmp = strdup(optarg);
				if (!tmp)
					break;
				i = 0;
				for (tok = strtok(tmp, ",");
				     tok;
				     tok = strtok(NULL, ",")) {
					len  = strtoul(tok, NULL, 0);
					if (len > FIELD_WIDTH_MAX)
						len = FIELD_WIDTH_MAX;
					fp.params[i].print.width = len;
					i++;
				}
				if (i == 1) {
					for (i = 0; i < MAX_FIELDS; i++)
						fp.params[i].print.width = len;
				}
				break;
			default:
				usage(argv[0], 1);
				break;
		}
	}

	lnstat_files = lnstat_scan_dir(PROC_NET_STAT, num_req_files,
				       (const char **) req_files);

	switch (mode) {
		int i;
		struct table_hdr *header;
	case MODE_DUMP:
		lnstat_dump(stderr, lnstat_files);
		break;
	case MODE_NORMAL:

		if (!map_field_params(lnstat_files, &fp, interval))
			exit(1);

		header = build_hdr_string(lnstat_files, &fp, 80);
		if (!header)
			exit(1);

		if (interval < 1 )
			interval=1;

		for (i = 0; i < count; i++) {
			if  ((hdr > 1 && (! (i % 20))) || (hdr == 1 && i == 0))
				print_hdr(stdout, header);
			lnstat_update(lnstat_files);
			print_line(stdout, lnstat_files, &fp);
			fflush(stdout);
			sleep(interval);
		}
	}

	return 1;
}

