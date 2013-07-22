/*
 * Terminology:
 *
 *	cpuset	- (libc) cpu_set_t data structure represents set of CPUs
 *	cpumask	- string with hex mask (e.g. "0x00000001")
 *	cpulist - string with CPU ranges (e.g. "0-3,5,7,8")
 *
 * Based on code from taskset.c and Linux kernel.
 *
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/syscall.h>

#include "cpuset.h"
#include "c.h"

static inline int val_to_char(int v)
{
	if (v >= 0 && v < 10)
		return '0' + v;
	else if (v >= 10 && v < 16)
		return ('a' - 10) + v;
	else
		return -1;
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

static const char *nexttoken(const char *q,  int sep)
{
	if (q)
		q = strchr(q, sep);
	if (q)
		q++;
	return q;
}

/*
 * Number of bits in a CPU bitmask on current system
 */
int get_max_number_of_cpus(void)
{
	int n, cpus = 2048;
	size_t setsize;
	cpu_set_t *set = cpuset_alloc(cpus, &setsize, NULL);

	if (!set)
		return -1;	/* error */

	for (;;) {
		CPU_ZERO_S(setsize, set);

		/* the library version does not return size of cpumask_t */
		n = syscall(SYS_sched_getaffinity, 0, setsize, set);

		if (n < 0 && errno == EINVAL && cpus < 1024 * 1024) {
			cpuset_free(set);
			cpus *= 2;
			set = cpuset_alloc(cpus, &setsize, NULL);
			if (!set)
				return -1;	/* error */
			continue;
		}
		cpuset_free(set);
		return n * 8;
	}
	return -1;
}

/*
 * Allocates a new set for ncpus and returns size in bytes and size in bits
 */
cpu_set_t *cpuset_alloc(int ncpus, size_t *setsize, size_t *nbits)
{
	cpu_set_t *set = CPU_ALLOC(ncpus);

	if (!set)
		return NULL;
	if (setsize)
		*setsize = CPU_ALLOC_SIZE(ncpus);
	if (nbits)
		*nbits = cpuset_nbits(CPU_ALLOC_SIZE(ncpus));
	return set;
}

void cpuset_free(cpu_set_t *set)
{
	CPU_FREE(set);
}

#if !HAVE_DECL_CPU_ALLOC
/* Please, use CPU_COUNT_S() macro. This is fallback */
int __cpuset_count_s(size_t setsize, const cpu_set_t *set)
{
	int s = 0;
	const __cpu_mask *p = set->__bits;
	const __cpu_mask *end = &set->__bits[setsize / sizeof (__cpu_mask)];

	while (p < end) {
		__cpu_mask l = *p++;

		if (l == 0)
			continue;
# if LONG_BIT > 32
		l = (l & 0x5555555555555555ul) + ((l >> 1) & 0x5555555555555555ul);
		l = (l & 0x3333333333333333ul) + ((l >> 2) & 0x3333333333333333ul);
		l = (l & 0x0f0f0f0f0f0f0f0ful) + ((l >> 4) & 0x0f0f0f0f0f0f0f0ful);
		l = (l & 0x00ff00ff00ff00fful) + ((l >> 8) & 0x00ff00ff00ff00fful);
		l = (l & 0x0000ffff0000fffful) + ((l >> 16) & 0x0000ffff0000fffful);
		l = (l & 0x00000000fffffffful) + ((l >> 32) & 0x00000000fffffffful);
# else
		l = (l & 0x55555555ul) + ((l >> 1) & 0x55555555ul);
		l = (l & 0x33333333ul) + ((l >> 2) & 0x33333333ul);
		l = (l & 0x0f0f0f0ful) + ((l >> 4) & 0x0f0f0f0ful);
		l = (l & 0x00ff00fful) + ((l >> 8) & 0x00ff00fful);
		l = (l & 0x0000fffful) + ((l >> 16) & 0x0000fffful);
# endif
		s += l;
	}
	return s;
}
#endif

/*
 * Returns human readable representation of the cpuset. The output format is
 * a list of CPUs with ranges (for example, "0,1,3-9").
 */
char *cpulist_create(char *str, size_t len,
			cpu_set_t *set, size_t setsize)
{
	size_t i;
	char *ptr = str;
	int entry_made = 0;
	size_t max = cpuset_nbits(setsize);

	for (i = 0; i < max; i++) {
		if (CPU_ISSET_S(i, setsize, set)) {
			int rlen;
			size_t j, run = 0;
			entry_made = 1;
			for (j = i + 1; j < max; j++) {
				if (CPU_ISSET_S(j, setsize, set))
					run++;
				else
					break;
			}
			if (!run)
				rlen = snprintf(ptr, len, "%zd,", i);
			else if (run == 1) {
				rlen = snprintf(ptr, len, "%zd,%zd,", i, i + 1);
				i++;
			} else {
				rlen = snprintf(ptr, len, "%zd-%zd,", i, i + run);
				i += run;
			}
			if (rlen < 0 || (size_t) rlen + 1 > len)
				return NULL;
			ptr += rlen;
			if (rlen > 0 && len > (size_t) rlen)
				len -= rlen;
			else
				len = 0;
		}
	}
	ptr -= entry_made;
	*ptr = '\0';

	return str;
}

/*
 * Returns string with CPU mask.
 */
char *cpumask_create(char *str, size_t len,
			cpu_set_t *set, size_t setsize)
{
	char *ptr = str;
	char *ret = NULL;
	int cpu;

	for (cpu = cpuset_nbits(setsize) - 4; cpu >= 0; cpu -= 4) {
		char val = 0;

		if (len == (size_t) (ptr - str))
			break;

		if (CPU_ISSET_S(cpu, setsize, set))
			val |= 1;
		if (CPU_ISSET_S(cpu + 1, setsize, set))
			val |= 2;
		if (CPU_ISSET_S(cpu + 2, setsize, set))
			val |= 4;
		if (CPU_ISSET_S(cpu + 3, setsize, set))
			val |= 8;

		if (!ret && val)
			ret = ptr;
		*ptr++ = val_to_char(val);
	}
	*ptr = '\0';
	return ret ? ret : ptr - 1;
}

/*
 * Parses string with list of CPU ranges.
 */
int cpumask_parse(const char *str, cpu_set_t *set, size_t setsize)
{
	int len = strlen(str);
	const char *ptr = str + len - 1;
	int cpu = 0;

	/* skip 0x, it's all hex anyway */
	if (len > 1 && !memcmp(str, "0x", 2L))
		str += 2;

	CPU_ZERO_S(setsize, set);

	while (ptr >= str) {
		char val;

		/* cpu masks in /sys uses comma as a separator */
		if (*ptr == ',')
			ptr--;

		val = char_to_val(*ptr);
		if (val == (char) -1)
			return -1;
		if (val & 1)
			CPU_SET_S(cpu, setsize, set);
		if (val & 2)
			CPU_SET_S(cpu + 1, setsize, set);
		if (val & 4)
			CPU_SET_S(cpu + 2, setsize, set);
		if (val & 8)
			CPU_SET_S(cpu + 3, setsize, set);
		len--;
		ptr--;
		cpu += 4;
	}

	return 0;
}

/*
 * Parses string with CPUs mask.
 */
int cpulist_parse(const char *str, cpu_set_t *set, size_t setsize)
{
	const char *p, *q;
	q = str;

	CPU_ZERO_S(setsize, set);

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
			CPU_SET_S(a, setsize, set);
			a += s;
		}
	}

	return 0;
}

#ifdef TEST_PROGRAM

#include <getopt.h>

int main(int argc, char *argv[])
{
	cpu_set_t *set;
	size_t setsize, buflen, nbits;
	char *buf, *mask = NULL, *range = NULL;
	int ncpus = 2048, rc, c;

	static const struct option longopts[] = {
	    { "ncpus", 1, 0, 'n' },
	    { "mask",  1, 0, 'm' },
	    { "range", 1, 0, 'r' },
	    { NULL,    0, 0, 0 }
	};

	while ((c = getopt_long(argc, argv, "n:m:r:", longopts, NULL)) != -1) {
		switch(c) {
		case 'n':
			ncpus = atoi(optarg);
			break;
		case 'm':
			mask = strdup(optarg);
			break;
		case 'r':
			range = strdup(optarg);
			break;
		default:
			goto usage_err;
		}
	}

	if (!mask && !range)
		goto usage_err;

	set = cpuset_alloc(ncpus, &setsize, &nbits);
	if (!set)
		err(EXIT_FAILURE, "failed to allocate cpu set");

	/*
	fprintf(stderr, "ncpus: %d, cpuset bits: %zd, cpuset bytes: %zd\n",
			ncpus, nbits, setsize);
	*/

	buflen = 7 * nbits;
	buf = malloc(buflen);
	if (!buf)
		err(EXIT_FAILURE, "failed to allocate cpu set buffer");

	if (mask)
		rc = cpumask_parse(mask, set, setsize);
	else
		rc = cpulist_parse(range, set, setsize);

	if (rc)
		errx(EXIT_FAILURE, "failed to parse string: %s", mask ? : range);

	printf("%-15s = %15s ", mask ? : range,
				cpumask_create(buf, buflen, set, setsize));
	printf("[%s]\n", cpulist_create(buf, buflen, set, setsize));

	free(buf);
	free(range);
	cpuset_free(set);

	return EXIT_SUCCESS;

usage_err:
	fprintf(stderr,
		"usage: %s [--ncpus <num>] --mask <mask> | --range <list>",
		program_invocation_short_name);
	exit(EXIT_FAILURE);
}
#endif
