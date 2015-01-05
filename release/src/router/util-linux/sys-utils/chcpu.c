/*
 * chcpu - CPU configuration tool
 *
 * Copyright IBM Corp. 2011
 * Author(s): Heiko Carstens <heiko.carstens@de.ibm.com>,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cpuset.h"
#include "nls.h"
#include "xalloc.h"
#include "c.h"
#include "strutils.h"
#include "bitops.h"
#include "path.h"

#define _PATH_SYS_CPU		"/sys/devices/system/cpu"
#define _PATH_SYS_CPU_ONLINE	_PATH_SYS_CPU "/online"
#define _PATH_SYS_CPU_RESCAN	_PATH_SYS_CPU "/rescan"
#define _PATH_SYS_CPU_DISPATCH	_PATH_SYS_CPU "/dispatching"

static cpu_set_t *onlinecpus;
static int maxcpus;

#define is_cpu_online(cpu) (CPU_ISSET_S((cpu), CPU_ALLOC_SIZE(maxcpus), onlinecpus))
#define num_online_cpus()  (CPU_COUNT_S(CPU_ALLOC_SIZE(maxcpus), onlinecpus))

enum {
	CMD_CPU_ENABLE	= 0,
	CMD_CPU_DISABLE,
	CMD_CPU_CONFIGURE,
	CMD_CPU_DECONFIGURE,
	CMD_CPU_RESCAN,
	CMD_CPU_DISPATCH_HORIZONTAL,
	CMD_CPU_DISPATCH_VERTICAL,
};

static int cpu_enable(cpu_set_t *cpu_set, size_t setsize, int enable)
{
	unsigned int cpu;
	int online, rc;
	int configured = -1;

	for (cpu = 0; cpu < setsize; cpu++) {
		if (!CPU_ISSET(cpu, cpu_set))
			continue;
		if (!path_exist(_PATH_SYS_CPU "/cpu%d", cpu)) {
			printf(_("CPU %d does not exist\n"), cpu);
			continue;
		}
		if (!path_exist(_PATH_SYS_CPU "/cpu%d/online", cpu)) {
			printf(_("CPU %d is not hot pluggable\n"), cpu);
			continue;
		}
		online = path_getnum(_PATH_SYS_CPU "/cpu%d/online", cpu);
		if ((online == 1) && (enable == 1)) {
			printf(_("CPU %d is already enabled\n"), cpu);
			continue;
		}
		if ((online == 0) && (enable == 0)) {
			printf(_("CPU %d is already disabled\n"), cpu);
			continue;
		}
		if (path_exist(_PATH_SYS_CPU "/cpu%d/configure", cpu))
			configured = path_getnum(_PATH_SYS_CPU "/cpu%d/configure", cpu);
		if (enable) {
			rc = path_writestr("1", _PATH_SYS_CPU "/cpu%d/online", cpu);
			if ((rc == -1) && (configured == 0))
				printf(_("CPU %d enable failed "
					 "(CPU is deconfigured)\n"), cpu);
			else if (rc == -1)
				printf(_("CPU %d enable failed (%m)\n"), cpu);
			else
				printf(_("CPU %d enabled\n"), cpu);
		} else {
			if (onlinecpus && num_online_cpus() == 1) {
				printf(_("CPU %d disable failed "
					 "(last enabled CPU)\n"), cpu);
				continue;
			}
			rc = path_writestr("0", _PATH_SYS_CPU "/cpu%d/online", cpu);
			if (rc == -1)
				printf(_("CPU %d disable failed (%m)\n"), cpu);
			else {
				printf(_("CPU %d disabled\n"), cpu);
				if (onlinecpus)
					CPU_CLR(cpu, onlinecpus);
			}
		}
	}
	return EXIT_SUCCESS;
}

static int cpu_rescan(void)
{
	if (!path_exist(_PATH_SYS_CPU_RESCAN))
		errx(EXIT_FAILURE, _("This system does not support rescanning of CPUs"));
	if (path_writestr("1", _PATH_SYS_CPU_RESCAN) == -1)
		err(EXIT_FAILURE, _("Failed to trigger rescan of CPUs"));
	printf(_("Triggered rescan of CPUs\n"));
	return EXIT_SUCCESS;
}

static int cpu_set_dispatch(int mode)
{
	if (!path_exist(_PATH_SYS_CPU_DISPATCH))
		errx(EXIT_FAILURE, _("This system does not support setting "
				     "the dispatching mode of CPUs"));
	if (mode == 0) {
		if (path_writestr("0", _PATH_SYS_CPU_DISPATCH) == -1)
			err(EXIT_FAILURE, _("Failed to set horizontal dispatch mode"));
		printf(_("Succesfully set horizontal dispatching mode\n"));
	} else {
		if (path_writestr("1", _PATH_SYS_CPU_DISPATCH) == -1)
			err(EXIT_FAILURE, _("Failed to set vertical dispatch mode"));
		printf(_("Succesfully set vertical dispatching mode\n"));
	}
	return EXIT_SUCCESS;
}

static int cpu_configure(cpu_set_t *cpu_set, size_t setsize, int configure)
{
	unsigned int cpu;
	int rc, current;

	for (cpu = 0; cpu < setsize; cpu++) {
		if (!CPU_ISSET(cpu, cpu_set))
			continue;
		if (!path_exist(_PATH_SYS_CPU "/cpu%d", cpu)) {
			printf(_("CPU %d does not exist\n"), cpu);
			continue;
		}
		if (!path_exist(_PATH_SYS_CPU "/cpu%d/configure", cpu)) {
			printf(_("CPU %d is not configurable\n"), cpu);
			continue;
		}
		current = path_getnum(_PATH_SYS_CPU "/cpu%d/configure", cpu);
		if ((current == 1) && (configure == 1)) {
			printf(_("CPU %d is already configured\n"), cpu);
			continue;
		}
		if ((current == 0) && (configure == 0)) {
			printf(_("CPU %d is already deconfigured\n"), cpu);
			continue;
		}
		if ((current == 1) && (configure == 0) && onlinecpus &&
		    is_cpu_online(cpu)) {
			printf(_("CPU %d deconfigure failed "
				 "(CPU is enabled)\n"), cpu);
			continue;
		}
		if (configure) {
			rc = path_writestr("1", _PATH_SYS_CPU "/cpu%d/configure", cpu);
			if (rc == -1)
				printf(_("CPU %d configure failed (%m)\n"), cpu);
			else
				printf(_("CPU %d configured\n"), cpu);
		} else {
			rc = path_writestr("0", _PATH_SYS_CPU "/cpu%d/configure", cpu);
			if (rc == -1)
				printf(_("CPU %d deconfigure failed (%m)\n"), cpu);
			else
				printf(_("CPU %d deconfigured\n"), cpu);
		}
	}
	return EXIT_SUCCESS;
}

static void cpu_parse(char *cpu_string, cpu_set_t *cpu_set, size_t setsize)
{
	int rc;

	rc = cpulist_parse(cpu_string, cpu_set, setsize, 1);
	if (rc == 0)
		return;
	if (rc == 2)
		errx(EXIT_FAILURE, _("invalid CPU number in CPU list: %s"), cpu_string);
	errx(EXIT_FAILURE, _("failed to parse CPU list: %s"), cpu_string);
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fprintf(out, _(
		"\nUsage:\n"
		" %s [options]\n"), program_invocation_short_name);

	puts(_(	"\nOptions:\n"
		"  -h, --help                    print this help\n"
		"  -e, --enable <cpu-list>       enable cpus\n"
		"  -d, --disable <cpu-list>      disable cpus\n"
		"  -c, --configure <cpu-list>    configure cpus\n"
		"  -g, --deconfigure <cpu-list>  deconfigure cpus\n"
		"  -p, --dispatch <mode>         set dispatching mode\n"
		"  -r, --rescan                  trigger rescan of cpus\n"
		"  -V, --version                 output version information and exit\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	cpu_set_t *cpu_set;
	size_t setsize;
	int cmd = -1;
	int c;

	static const struct option longopts[] = {
		{ "configure",	required_argument, 0, 'c' },
		{ "deconfigure",required_argument, 0, 'g' },
		{ "disable",	required_argument, 0, 'd' },
		{ "dispatch",	required_argument, 0, 'p' },
		{ "enable",	required_argument, 0, 'e' },
		{ "help",	no_argument,       0, 'h' },
		{ "rescan",	no_argument,       0, 'r' },
		{ "version",	no_argument,       0, 'V' },
		{ NULL,		0, 0, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	maxcpus = get_max_number_of_cpus();
	if (maxcpus < 1)
		errx(EXIT_FAILURE, _("cannot determine NR_CPUS; aborting"));
	if (path_exist(_PATH_SYS_CPU_ONLINE))
		onlinecpus = path_cpulist(maxcpus, _PATH_SYS_CPU_ONLINE);
	setsize = CPU_ALLOC_SIZE(maxcpus);
	cpu_set = CPU_ALLOC(maxcpus);
	if (!cpu_set)
		err(EXIT_FAILURE, _("cpuset_alloc failed"));

	while ((c = getopt_long(argc, argv, "c:d:e:g:hp:rV", longopts, NULL)) != -1) {
		if (cmd != -1 && strchr("cdegpr", c))
			errx(EXIT_FAILURE,
			     _("configure, deconfigure, disable, dispatch, enable "
			       "and rescan are mutually exclusive"));
		switch (c) {
		case 'c':
			cmd = CMD_CPU_CONFIGURE;
			cpu_parse(argv[optind - 1], cpu_set, setsize);
			break;
		case 'd':
			cmd = CMD_CPU_DISABLE;
			cpu_parse(argv[optind - 1], cpu_set, setsize);
			break;
		case 'e':
			cmd = CMD_CPU_ENABLE;
			cpu_parse(argv[optind - 1], cpu_set, setsize);
			break;
		case 'g':
			cmd = CMD_CPU_DECONFIGURE;
			cpu_parse(argv[optind - 1], cpu_set, setsize);
			break;
		case 'h':
			usage(stdout);
		case 'p':
			if (strcmp("horizontal", argv[optind - 1]) == 0)
				cmd = CMD_CPU_DISPATCH_HORIZONTAL;
			else if (strcmp("vertical", argv[optind - 1]) == 0)
				cmd = CMD_CPU_DISPATCH_VERTICAL;
			else
				errx(EXIT_FAILURE, _("unsupported argument: %s"),
				     argv[optind -1 ]);
			break;
		case 'r':
			cmd = CMD_CPU_RESCAN;
			break;
		case 'V':
			printf(_("%s from %s\n"), program_invocation_short_name,
			       PACKAGE_STRING);
			return EXIT_SUCCESS;
		default:
			usage(stderr);
		}
	}

	if ((argc == 1) || (argc != optind))
		usage(stderr);

	switch (cmd) {
	case CMD_CPU_ENABLE:
		return cpu_enable(cpu_set, maxcpus, 1);
	case CMD_CPU_DISABLE:
		return cpu_enable(cpu_set, maxcpus, 0);
	case CMD_CPU_CONFIGURE:
		return cpu_configure(cpu_set, maxcpus, 1);
	case CMD_CPU_DECONFIGURE:
		return cpu_configure(cpu_set, maxcpus, 0);
	case CMD_CPU_RESCAN:
		return cpu_rescan();
	case CMD_CPU_DISPATCH_HORIZONTAL:
		return cpu_set_dispatch(0);
	case CMD_CPU_DISPATCH_VERTICAL:
		return cpu_set_dispatch(1);
	}
	return EXIT_SUCCESS;
}
