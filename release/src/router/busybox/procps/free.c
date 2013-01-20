/* vi: set sw=4 ts=4: */
/*
 * Mini free implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/* getopt not needed */

//usage:#define free_trivial_usage
//usage:       "" IF_DESKTOP("[-b/k/m/g]")
//usage:#define free_full_usage "\n\n"
//usage:       "Display the amount of free and used system memory"
//usage:
//usage:#define free_example_usage
//usage:       "$ free\n"
//usage:       "              total         used         free       shared      buffers\n"
//usage:       "  Mem:       257628       248724         8904        59644        93124\n"
//usage:       " Swap:       128516         8404       120112\n"
//usage:       "Total:       386144       257128       129016\n"

#include "libbb.h"
#ifdef __linux__
# include <sys/sysinfo.h>
#endif

struct globals {
	unsigned mem_unit;
#if ENABLE_DESKTOP
	unsigned unit_steps;
# define G_unit_steps G.unit_steps
#else
# define G_unit_steps 10
#endif
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#define INIT_G() do { } while (0)


static unsigned long long scale(unsigned long d)
{
	return ((unsigned long long)d * G.mem_unit) >> G_unit_steps;
}


int free_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int free_main(int argc UNUSED_PARAM, char **argv IF_NOT_DESKTOP(UNUSED_PARAM))
{
	struct sysinfo info;

	INIT_G();

#if ENABLE_DESKTOP
	G.unit_steps = 10;
	if (argv[1] && argv[1][0] == '-') {
		switch (argv[1][1]) {
		case 'b':
			G.unit_steps = 0;
			break;
		case 'k': /* 2^10 */
			/* G.unit_steps = 10; - already is */
			break;
		case 'm': /* 2^(2*10) */
			G.unit_steps = 20;
			break;
		case 'g': /* 2^(3*10) */
			G.unit_steps = 30;
			break;
		default:
			bb_show_usage();
		}
	}
#endif

	sysinfo(&info);

	/* Kernels prior to 2.4.x will return info.mem_unit==0, so cope... */
	G.mem_unit = (info.mem_unit ? info.mem_unit : 1);

	printf("     %13s%13s%13s%13s%13s\n",
		"total",
		"used",
		"free",
		"shared", "buffers" /* swap and total don't have these columns */
		/* procps version 3.2.8 also shows "cached" column, but
		 * sysinfo() does not provide this value, need to parse
		 * /proc/meminfo instead and get "Cached: NNN kB" from there.
		 */
	);

#define FIELDS_5 "%13llu%13llu%13llu%13llu%13llu\n"
#define FIELDS_3 (FIELDS_5 + 2*6)
#define FIELDS_2 (FIELDS_5 + 3*6)

	printf("Mem: ");
	printf(FIELDS_5,
		scale(info.totalram),
		scale(info.totalram - info.freeram),
		scale(info.freeram),
		scale(info.sharedram),
		scale(info.bufferram)
	);
	/* Show alternate, more meaningful busy/free numbers by counting
	 * buffer cache as free memory (make it "-/+ buffers/cache"
	 * if/when we add support for "cached" column): */
	printf("-/+ buffers:      ");
	printf(FIELDS_2,
		scale(info.totalram - info.freeram - info.bufferram),
		scale(info.freeram + info.bufferram)
	);
#if BB_MMU
	printf("Swap:");
	printf(FIELDS_3,
		scale(info.totalswap),
		scale(info.totalswap - info.freeswap),
		scale(info.freeswap)
	);
#endif
	return EXIT_SUCCESS;
}
