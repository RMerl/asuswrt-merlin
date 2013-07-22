/*
 *  readprofile.c - used to read /proc/profile
 *
 *  Copyright (C) 1994,1996 Alessandro Rubini (rubini@ipvvis.unipv.it)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 * 1999-09-01 Stephane Eranian <eranian@cello.hpl.hp.com>
 * - 64bit clean patch
 * 3Feb2001 Andrew Morton <andrewm@uow.edu.au>
 * - -M option to write profile multiplier.
 * 2001-11-07 Werner Almesberger <wa@almesberger.net>
 * - byte order auto-detection and -n option
 * 2001-11-09 Werner Almesberger <wa@almesberger.net>
 * - skip step size (index 0)
 * 2002-03-09 John Levon <moz@compsoc.man.ac.uk>
 * - make maplineno do something
 * 2002-11-28 Mads Martin Joergensen +
 * - also try /boot/System.map-`uname -r`
 * 2003-04-09 Werner Almesberger <wa@almesberger.net>
 * - fixed off-by eight error and improved heuristics in byte order detection
 * 2003-08-12 Nikita Danilov <Nikita@Namesys.COM>
 * - added -s option; example of use:
 * "readprofile -s -m /boot/System.map-test | grep __d_lookup | sort -n -k3"
 */

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "xalloc.h"
#include "nls.h"

#define S_LEN 128

static char *prgname;

/* These are the defaults */
static char defaultmap[]="/boot/System.map";
static char defaultpro[]="/proc/profile";
static char optstring[]="M:m:np:itvarVbs";

static FILE *
myopen(char *name, char *mode, int *flag) {
	int len = strlen(name);

	if (!strcmp(name+len-3,".gz")) {
		FILE *res;
		char *cmdline = xmalloc(len+6);
		sprintf(cmdline, "zcat %s", name);
		res = popen(cmdline,mode);
		free(cmdline);
		*flag = 1;
		return res;
	}
	*flag = 0;
	return fopen(name,mode);
}

#ifndef BOOT_SYSTEM_MAP
#define BOOT_SYSTEM_MAP "/boot/System.map-"
#endif

static char *
boot_uname_r_str(void) {
	struct utsname uname_info;
	char *s;
	size_t len;

	if (uname(&uname_info))
		return "";
	len = strlen(BOOT_SYSTEM_MAP) + strlen(uname_info.release) + 1;
	s = xmalloc(len);
	strcpy(s, BOOT_SYSTEM_MAP);
	strcat(s, uname_info.release);
	return s;
}

static void
usage(void) {
	fprintf(stderr, _(
		"%s: Usage: \"%s [options]\n"
		"\t -m <mapfile>  (defaults: \"%s\" and\n\t\t\t\t  \"%s\")\n"
		"\t -p <pro-file> (default: \"%s\")\n"
		"\t -M <mult>     set the profiling multiplier to <mult>\n"
		"\t -i            print only info about the sampling step\n"
		"\t -v            print verbose data\n"
		"\t -a            print all symbols, even if count is 0\n"
		"\t -b            print individual histogram-bin counts\n"
		"\t -s            print individual counters within functions\n"
		"\t -r            reset all the counters (root only)\n"
		"\t -n            disable byte order auto-detection\n"
		"\t -V            print version and exit\n"),
		prgname, prgname, defaultmap, boot_uname_r_str(), defaultpro);
	exit(1);
}

int
main(int argc, char **argv) {
	FILE *map;
	int proFd;
	char *mapFile, *proFile, *mult=0;
	size_t len=0, indx=1;
	unsigned long long add0=0;
	unsigned int step;
	unsigned int *buf, total, fn_len;
	unsigned long long fn_add, next_add;          /* current and next address */
	char fn_name[S_LEN], next_name[S_LEN];   /* current and next name */
	char mode[8];
	int c;
	ssize_t rc;
	int optAll=0, optInfo=0, optReset=0, optVerbose=0, optNative=0;
	int optBins=0, optSub=0;
	char mapline[S_LEN];
	int maplineno=1;
	int popenMap;   /* flag to tell if popen() has been used */
	int header_printed;

#define next (current^1)

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	prgname = argv[0];
	proFile = defaultpro;
	mapFile = defaultmap;

	while ((c = getopt(argc, argv, optstring)) != -1) {
		switch(c) {
		case 'm':
			mapFile = optarg;
			break;
		case 'n':
			optNative++;
			break;
		case 'p':
			proFile = optarg;
			break;
		case 'a':
			optAll++;
			break;
		case 'b':
			optBins++;
			break;
		case 's':
			optSub++;
			break;
		case 'i':
			optInfo++;
			break;
		case 'M':
			mult = optarg;
			break;
		case 'r':
			optReset++;
			break;
		case 'v':
			optVerbose++;
			break;
		case 'V':
			printf(_("%s (%s)\n"), prgname,
			       PACKAGE_STRING);
			exit(0);
		default:
			usage();
		}
	}

	if (optReset || mult) {
		int multiplier, fd, to_write;

		/*
		 * When writing the multiplier, if the length of the write is
		 * not sizeof(int), the multiplier is not changed
		 */
		if (mult) {
			multiplier = strtoul(mult, 0, 10);
			to_write = sizeof(int);
		} else {
			multiplier = 0;
			to_write = 1;	/* sth different from sizeof(int) */
		}
		/* try to become root, just in case */
		setuid(0);
		fd = open(defaultpro,O_WRONLY);
		if (fd < 0) {
			perror(defaultpro);
			exit(1);
		}
		if (write(fd, &multiplier, to_write) != to_write) {
			fprintf(stderr, _("readprofile: error writing %s: %s\n"),
				defaultpro, strerror(errno));
			exit(1);
		}
		close(fd);
		exit(0);
	}

	/*
	 * Use an fd for the profiling buffer, to skip stdio overhead
	 */
	if (((proFd=open(proFile,O_RDONLY)) < 0)
	    || ((int)(len=lseek(proFd,0,SEEK_END)) < 0)
	    || (lseek(proFd,0,SEEK_SET) < 0)) {
		fprintf(stderr,"%s: %s: %s\n",prgname,proFile,strerror(errno));
		exit(1);
	}

	buf = xmalloc(len);

	rc = read(proFd,buf,len);
	if (rc < 0 || (size_t) rc != len) {
		fprintf(stderr,"%s: %s: %s\n",prgname,proFile,strerror(errno));
		exit(1);
	}
	close(proFd);

	if (!optNative) {
		int entries = len/sizeof(*buf);
		int big = 0,small = 0;
		unsigned *p;
		size_t i;

		for (p = buf+1; p < buf+entries; p++) {
			if (*p & ~0U << (sizeof(*buf)*4))
				big++;
			if (*p & ((1 << (sizeof(*buf)*4))-1))
				small++;
		}
		if (big > small) {
			fprintf(stderr,_("Assuming reversed byte order. "
				"Use -n to force native byte order.\n"));
			for (p = buf; p < buf+entries; p++)
				for (i = 0; i < sizeof(*buf)/2; i++) {
					unsigned char *b = (unsigned char *) p;
					unsigned char tmp;

					tmp = b[i];
					b[i] = b[sizeof(*buf)-i-1];
					b[sizeof(*buf)-i-1] = tmp;
				}
		}
	}

	step = buf[0];
	if (optInfo) {
		printf(_("Sampling_step: %i\n"), step);
		exit(0);
	} 

	total = 0;

	map = myopen(mapFile, "r", &popenMap);
	if (map == NULL && mapFile == defaultmap) {
		mapFile = boot_uname_r_str();
		map = myopen(mapFile, "r", &popenMap);
	}
	if (map == NULL) {
		int errsv = errno;
		fprintf(stderr, "%s: ", prgname);
		errno = errsv;
		perror(mapFile);
		exit(1);
	}

	while (fgets(mapline,S_LEN,map)) {
		if (sscanf(mapline,"%llx %s %s",&fn_add,mode,fn_name) != 3) {
			fprintf(stderr,_("%s: %s(%i): wrong map line\n"),
				prgname, mapFile, maplineno);
			exit(1);
		}
		/* only elf works like this */
		if (!strcmp(fn_name,"_stext") || !strcmp(fn_name,"__stext")) {
			add0 = fn_add;
			break;
		}
		maplineno++;
	}

	if (!add0) {
		fprintf(stderr,_("%s: can't find \"_stext\" in %s\n"),
			prgname, mapFile);
		exit(1);
	}

	/*
	 * Main loop.
	 */
	while (fgets(mapline,S_LEN,map)) {
		unsigned int this=0;
		int done = 0;

		if (sscanf(mapline,"%llx %s %s",&next_add,mode,next_name)!=3) {
			fprintf(stderr,_("%s: %s(%i): wrong map line\n"),
				prgname,mapFile, maplineno);
			exit(1);
		}
		header_printed = 0;

		/* the kernel only profiles up to _etext */
		if (!strcmp(next_name, "_etext") ||
		    !strcmp(next_name, "__etext"))
			done = 1;
		else {
			/* ignore any LEADING (before a '[tT]' symbol is found)
			   Absolute symbols and __init_end because some
			   architectures place it before .text section */
			if ((*mode == 'A' || *mode == '?')
			    && (total == 0 || !strcmp(next_name, "__init_end")))
				continue;
			if (*mode != 'T' && *mode != 't' &&
			    *mode != 'W' && *mode != 'w')
				break;	/* only text is profiled */
		}

		if (indx >= len / sizeof(*buf)) {
			fprintf(stderr, _("%s: profile address out of range. "
					  "Wrong map file?\n"), prgname);
			exit(1);
		}

		while (indx < (next_add-add0)/step) {
			if (optBins && (buf[indx] || optAll)) {
				if (!header_printed) {
					printf ("%s:\n", fn_name);
					header_printed = 1;
				}
				printf ("\t%llx\t%u\n", (indx - 1)*step + add0, buf[indx]);
			}
			this += buf[indx++];
		}
		total += this;

		if (optBins) {
			if (optVerbose || this > 0)
				printf ("  total\t\t\t\t%u\n", this);
		} else if ((this || optAll) &&
			   (fn_len = next_add-fn_add) != 0) {
			if (optVerbose)
				printf("%016llx %-40s %6i %8.4f\n", fn_add,
				       fn_name,this,this/(double)fn_len);
			else
				printf("%6i %-40s %8.4f\n",
				       this,fn_name,this/(double)fn_len);
			if (optSub) {
				unsigned long long scan;

				for (scan = (fn_add-add0)/step + 1;
				     scan < (next_add-add0)/step; scan++) {
					unsigned long long addr;

					addr = (scan - 1)*step + add0;
					printf("\t%#llx\t%s+%#llx\t%u\n",
					       addr, fn_name, addr - fn_add,
					       buf[scan]);
				}
			}
		}

		fn_add = next_add;
		strcpy(fn_name,next_name);

		maplineno++;
		if (done)
			break;
	}

	/* clock ticks, out of kernel text - probably modules */
	printf("%6i %s\n", buf[len/sizeof(*buf)-1], "*unknown*");

	/* trailer */
	if (optVerbose)
		printf("%016x %-40s %6i %8.4f\n",
		       0,"total",total,total/(double)(fn_add-add0));
	else
		printf("%6i %-40s %8.4f\n",
		       total,_("total"),total/(double)(fn_add-add0));
	
	popenMap ? pclose(map) : fclose(map);
	exit(0);
}
