/* vi: set sw=4 ts=4: */
/*
 * taskset - retrieve or set a processes' CPU affinity
 * Copyright (c) 2006 Bernhard Reutner-Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//config:config TASKSET
//config:	bool "taskset"
//config:	default n  # doesn't build on some non-x86 targets (m68k)
//config:	help
//config:	  Retrieve or set a processes's CPU affinity.
//config:	  This requires sched_{g,s}etaffinity support in your libc.
//config:
//config:config FEATURE_TASKSET_FANCY
//config:	bool "Fancy output"
//config:	default y
//config:	depends on TASKSET
//config:	help
//config:	  Add code for fancy output. This merely silences a compiler-warning
//config:	  and adds about 135 Bytes. May be needed for machines with alot
//config:	  of CPUs.

//applet:IF_TASKSET(APPLET(taskset, BB_DIR_USR_BIN, BB_SUID_DROP))
//kbuild:lib-$(CONFIG_TASKSET) += taskset.o

//usage:#define taskset_trivial_usage
//usage:       "[-p] [MASK] [PID | PROG ARGS]"
//usage:#define taskset_full_usage "\n\n"
//usage:       "Set or get CPU affinity\n"
//usage:     "\n	-p	Operate on an existing PID"
//usage:
//usage:#define taskset_example_usage
//usage:       "$ taskset 0x7 ./dgemm_test&\n"
//usage:       "$ taskset -p 0x1 $!\n"
//usage:       "pid 4790's current affinity mask: 7\n"
//usage:       "pid 4790's new affinity mask: 1\n"
//usage:       "$ taskset 0x7 /bin/sh -c './taskset -p 0x1 $$'\n"
//usage:       "pid 6671's current affinity mask: 1\n"
//usage:       "pid 6671's new affinity mask: 1\n"
//usage:       "$ taskset -p 1\n"
//usage:       "pid 1's current affinity mask: 3\n"
/*
 Not yet implemented:
 * -a/--all-tasks (affect all threads)
 * -c/--cpu-list  (specify CPUs via "1,3,5-7")
 */

#include <sched.h>
#include "libbb.h"

#if ENABLE_FEATURE_TASKSET_FANCY
#define TASKSET_PRINTF_MASK "%s"
/* craft a string from the mask */
static char *from_cpuset(cpu_set_t *mask)
{
	int i;
	char *ret = NULL;
	char *str = xzalloc((CPU_SETSIZE / 4) + 1); /* we will leak it */

	for (i = CPU_SETSIZE - 4; i >= 0; i -= 4) {
		int val = 0;
		int off;
		for (off = 0; off <= 3; ++off)
			if (CPU_ISSET(i + off, mask))
				val |= 1 << off;
		if (!ret && val)
			ret = str;
		*str++ = bb_hexdigits_upcase[val] | 0x20;
	}
	return ret;
}
#else
#define TASKSET_PRINTF_MASK "%llx"
static unsigned long long from_cpuset(cpu_set_t *mask)
{
	BUILD_BUG_ON(CPU_SETSIZE < 8*sizeof(int));

	/* Take the least significant bits. Assume cpu_set_t is
	 * implemented as an array of unsigned long or unsigned
	 * int.
	 */
	if (CPU_SETSIZE < 8*sizeof(long))
		return *(unsigned*)mask;
	if (CPU_SETSIZE < 8*sizeof(long long))
		return *(unsigned long*)mask;
# if BB_BIG_ENDIAN
	if (sizeof(long long) > sizeof(long)) {
		/* We can put two long in the long long, but they have to
		 * be swapped: the least significant word comes first in the
		 * array */
		unsigned long *p = (void*)mask;
		return p[0] + ((unsigned long long)p[1] << (8*sizeof(long)));
	}
# endif
	return *(unsigned long long*)mask;
}
#endif


int taskset_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int taskset_main(int argc UNUSED_PARAM, char **argv)
{
	cpu_set_t mask;
	pid_t pid = 0;
	unsigned opt_p;
	const char *current_new;
	char *pid_str;
	char *aff = aff; /* for compiler */

	/* NB: we mimic util-linux's taskset: -p does not take
	 * an argument, i.e., "-pN" is NOT valid, only "-p N"!
	 * Indeed, util-linux-2.13-pre7 uses:
	 * getopt_long(argc, argv, "+pchV", ...), not "...p:..." */

	opt_complementary = "-1"; /* at least 1 arg */
	opt_p = getopt32(argv, "+p");
	argv += optind;

	if (opt_p) {
		pid_str = *argv++;
		if (*argv) { /* "-p <aff> <pid> ...rest.is.ignored..." */
			aff = pid_str;
			pid_str = *argv; /* NB: *argv != NULL in this case */
		}
		/* else it was just "-p <pid>", and *argv == NULL */
		pid = xatoul_range(pid_str, 1, ((unsigned)(pid_t)ULONG_MAX) >> 1);
	} else {
		aff = *argv++; /* <aff> <cmd...> */
		if (!*argv)
			bb_show_usage();
	}

	current_new = "current\0new";
	if (opt_p) {
 print_aff:
		if (sched_getaffinity(pid, sizeof(mask), &mask) < 0)
			bb_perror_msg_and_die("can't %cet pid %d's affinity", 'g', pid);
		printf("pid %d's %s affinity mask: "TASKSET_PRINTF_MASK"\n",
				pid, current_new, from_cpuset(&mask));
		if (!*argv) {
			/* Either it was just "-p <pid>",
			 * or it was "-p <aff> <pid>" and we came here
			 * for the second time (see goto below) */
			return EXIT_SUCCESS;
		}
		*argv = NULL;
		current_new += 8; /* "new" */
	}

	/* Affinity was specified, translate it into cpu_set_t */
	CPU_ZERO(&mask);
	if (!ENABLE_FEATURE_TASKSET_FANCY) {
		unsigned i;
		unsigned long long m;

		/* Do not allow zero mask: */
		m = xstrtoull_range(aff, 0, 1, ULLONG_MAX);
		i = 0;
		do {
			if (m & 1)
				CPU_SET(i, &mask);
			i++;
			m >>= 1;
		} while (m != 0);
	} else {
		unsigned i;
		char *last_byte;
		char *bin;
		uint8_t bit_in_byte;

		/* Cheap way to get "long enough" buffer */
		bin = xstrdup(aff);

		if (aff[0] != '0' || (aff[1]|0x20) != 'x') {
/* TODO: decimal/octal masks are still limited to 2^64 */
			unsigned long long m = xstrtoull_range(aff, 0, 1, ULLONG_MAX);
			bin += strlen(bin);
			last_byte = bin - 1;
			while (m) {
				*--bin = m & 0xff;
				m >>= 8;
			}
		} else {
			/* aff is "0x.....", we accept very long masks in this form */
			last_byte = hex2bin(bin, aff + 2, INT_MAX);
			if (!last_byte) {
 bad_aff:
				bb_error_msg_and_die("bad affinity '%s'", aff);
			}
			last_byte--; /* now points to the last byte */
		}

		i = 0;
		bit_in_byte = 1;
		while (last_byte >= bin) {
			if (bit_in_byte & *last_byte) {
				if (i >= CPU_SETSIZE)
					goto bad_aff;
				CPU_SET(i, &mask);
				//bb_error_msg("bit %d set", i);
			}
			i++;
			/* bit_in_byte is uint8_t! & 0xff is implied */
			bit_in_byte = (bit_in_byte << 1);
			if (!bit_in_byte) {
				bit_in_byte = 1;
				last_byte--;
			}
		}
	}

	/* Set pid's or our own (pid==0) affinity */
	if (sched_setaffinity(pid, sizeof(mask), &mask))
		bb_perror_msg_and_die("can't %cet pid %d's affinity", 's', pid);

	if (!argv[0]) /* "-p <aff> <pid> [...ignored...]" */
		goto print_aff; /* print new affinity and exit */

	BB_EXECVP_or_die(argv);
}
