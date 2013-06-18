/*
 * taskset.c - taskset
 * Command-line utility for setting and retrieving a task's CPU affinity
 *
 * Robert Love <rml@tech9.net>		25 April 2002
 *
 * Linux kernels as of 2.5.8 provide the needed syscalls for
 * working with a task's cpu affinity.  Currently 2.4 does not
 * support these syscalls, but patches are available at:
 *
 * 	http://www.kernel.org/pub/linux/kernel/people/rml/cpu-affinity/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, v2, as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Copyright (C) 2004 Robert Love
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sched.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

static void show_usage(const char *cmd)
{
	fprintf(stderr, "taskset version " "VERSION" "\n");
	fprintf(stderr, "usage: %s [options] [mask | cpu-list] [pid |"\
		" cmd [args...]]\n", cmd);
	fprintf(stderr, "set or get the affinity of a process\n\n");
	fprintf(stderr, "  -p, --pid                  "
		"operate on existing given pid\n");
        fprintf(stderr, "  -c, --cpu-list             "\
		"display and specify cpus in list format\n");
	fprintf(stderr, "  -h, --help                 display this help\n");
	fprintf(stderr, "  -v, --version              "\
		"output version information\n\n");
	fprintf(stderr, "The default behavior is to run a new command:\n");
	fprintf(stderr, "  %s 03 sshd -b 1024\n", cmd);
	fprintf(stderr, "You can retrieve the mask of an existing task:\n");
	fprintf(stderr, "  %s -p 700\n", cmd);
	fprintf(stderr, "Or set it:\n");
	fprintf(stderr, "  %s -p 03 700\n", cmd);
	fprintf(stderr, "List format uses a comma-separated list instead"\
			" of a mask:\n");
	fprintf(stderr, "  %s -pc 0,3,7-11 700\n", cmd);
	fprintf(stderr, "Ranges in list format can take a stride argument:\n");
	fprintf(stderr, "  e.g. 0-31:2 is equivalent to mask 0x55555555\n\n");
}

static inline int val_to_char(int v)
{
	if (v >= 0 && v < 10)
		return '0' + v;
	else if (v >= 10 && v < 16)
		return ('a' - 10) + v;
	else 
		return -1;
}

static char * cpuset_to_str(cpu_set_t *mask, char *str)
{
	int base;
	char *ptr = str;
	char *ret = 0;

	for (base = CPU_SETSIZE - 4; base >= 0; base -= 4) {
		char val = 0;
		if (CPU_ISSET(base, mask))
			val |= 1;
		if (CPU_ISSET(base + 1, mask))
			val |= 2;
		if (CPU_ISSET(base + 2, mask))
			val |= 4;
		if (CPU_ISSET(base + 3, mask))
			val |= 8;
		if (!ret && val)
			ret = ptr;
		*ptr++ = val_to_char(val);
	}
	*ptr = 0;
	return ret ? ret : ptr - 1;
}

static char * cpuset_to_cstr(cpu_set_t *mask, char *str)
{
	int i;
	char *ptr = str;
	int entry_made = 0;

	for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, mask)) {
			int j;
			int run = 0;
			entry_made = 1;
			for (j = i + 1; j < CPU_SETSIZE; j++) {
				if (CPU_ISSET(j, mask))
					run++;
				else
					break;
			}
			if (!run)
				sprintf(ptr, "%d,", i);
			else if (run == 1) {
				sprintf(ptr, "%d,%d,", i, i + 1);
				i++;
			} else {
				sprintf(ptr, "%d-%d,", i, i + run);
				i += run;
			}
			while (*ptr != 0)
				ptr++;
		}
	}
	ptr -= entry_made;
	*ptr = 0;

	return str;
}

static inline int char_to_val(int c)
{
	int cl;

	cl = tolower(c);
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (cl >= 'a' && cl <= 'f')
		return cl + (10 - 'a');
	else
		return -1;
}

static int str_to_cpuset(cpu_set_t *mask, const char* str)
{
	int len = strlen(str);
	const char *ptr = str + len - 1;
	int base = 0;

	/* skip 0x, it's all hex anyway */
	if (len > 1 && !memcmp(str, "0x", 2L))
		str += 2;

	CPU_ZERO(mask);
	while (ptr >= str) {
		char val = char_to_val(*ptr);
		if (val == (char) -1)
			return -1;
		if (val & 1)
			CPU_SET(base, mask);
		if (val & 2)
			CPU_SET(base + 1, mask);
		if (val & 4)
			CPU_SET(base + 2, mask);
		if (val & 8)
			CPU_SET(base + 3, mask);
		len--;
		ptr--;
		base += 4;
	}

	return 0;
}

static const char *nexttoken(const char *q,  int sep)
{
	if (q)
		q = strchr(q, sep);
	if (q)
		q++;
	return q;
}

static int cstr_to_cpuset(cpu_set_t *mask, const char* str)
{
	const char *p, *q;
	q = str;
	CPU_ZERO(mask);

	while (p = q, q = nexttoken(q, ','), p) {
		unsigned int a;	/* beginning of range */
		unsigned int b;	/* end of range */
		unsigned int s;	/* stride */
		const char *c1, *c2;

		if (sscanf(p, "%u", &a) < 1)
			return 1;
		b = a;
		s = 1;

		c1 = nexttoken(p, '-');
		c2 = nexttoken(p, ',');
		if (c1 != NULL && (c2 == NULL || c1 < c2)) {
			if (sscanf(c1, "%u", &b) < 1)
				return 1;
			c1 = nexttoken(c1, ':');
			if (c1 != NULL && (c2 == NULL || c1 < c2))
				if (sscanf(c1, "%u", &s) < 1) {
					return 1;
			}
		}

		if (!(a <= b))
			return 1;
		while (a <= b) {
			CPU_SET(a, mask);			
			a += s;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	cpu_set_t new_mask, cur_mask;
	pid_t pid = 0;
	int opt, err;
	char mstr[1 + CPU_SETSIZE / 4];
	char cstr[7 * CPU_SETSIZE];
	int c_opt = 0;

	struct option longopts[] = {
		{ "pid",	0, NULL, 'p' },
		{ "cpu-list",	0, NULL, 'c' },
		{ "help",	0, NULL, 'h' },
		{ "version",	0, NULL, 'V' },
		{ NULL,		0, NULL, 0 }
	};

	while ((opt = getopt_long(argc, argv, "+pchV", longopts, NULL)) != -1) {
		int ret = 1;

		switch (opt) {
		case 'p':
			pid = atoi(argv[argc - 1]);
			break;
		case 'c':
			c_opt = 1;
			break;
		case 'V':
			printf("taskset version " "VERSION" "\n");
			return 0;
		case 'h':
			ret = 0;
		default:
			show_usage(argv[0]);
			return ret;
		}
	}

	if ((!pid && argc - optind < 2)
			|| (pid && (argc - optind < 1 || argc - optind > 2))) {
		show_usage(argv[0]);
		return 1;
	}

	if (pid) {
		if (sched_getaffinity(pid, sizeof (cur_mask), &cur_mask) < 0) {
			perror("sched_getaffinity");
			fprintf(stderr, "failed to get pid %d's affinity\n",
				pid);
			return 1;
		}
		if (c_opt)
			printf("pid %d's current affinity list: %s\n", pid,
				cpuset_to_cstr(&cur_mask, cstr));
		else
			printf("pid %d's current affinity mask: %s\n", pid,
				cpuset_to_str(&cur_mask, mstr));

		if (argc - optind == 1)
			return 0;
	}

	if (c_opt)
		err = cstr_to_cpuset(&new_mask, argv[optind]);
	else
		err = str_to_cpuset(&new_mask, argv[optind]);

	if (err) {
		if (c_opt)
			fprintf(stderr, "failed to parse CPU list %s\n",
				argv[optind]);
		else
			fprintf(stderr, "failed to parse CPU mask %s\n",
				argv[optind]);
		return 1;
	}

	if (sched_setaffinity(pid, sizeof (new_mask), &new_mask)) {
		perror("sched_setaffinity");
		fprintf(stderr, "failed to set pid %d's affinity.\n", pid);
		return 1;
	}

	if (sched_getaffinity(pid, sizeof (cur_mask), &cur_mask) < 0) {
		perror("sched_getaffinity");
		fprintf(stderr, "failed to get pid %d's affinity.\n", pid);
		return 1;
	}

	if (pid) {
		if (c_opt)
			printf("pid %d's new affinity list: %s\n", pid, 
		               cpuset_to_cstr(&cur_mask, cstr));
		else
			printf("pid %d's new affinity mask: %s\n", pid, 
		               cpuset_to_str(&cur_mask, mstr));
	} else {
		argv += optind + 1;
		execvp(argv[0], argv);
		perror("execvp");
		fprintf(stderr, "failed to execute %s\n", argv[0]);
		return 1;
	}

	return 0;
}
