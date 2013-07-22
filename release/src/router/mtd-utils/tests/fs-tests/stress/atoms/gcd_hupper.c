/*
 * Copyright (C) 2007 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * Author: Adrian Hunter
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <mntent.h>
#include <signal.h>

#include "tests.h"

#define MAX_NAME_SIZE 1024

struct gcd_pid
{
	struct gcd_pid *next;
	int pid;
	char *name;
	int mtd_index;
};

struct gcd_pid *gcd_pid_list = NULL;

int add_gcd_pid(const char *number)
{
	int pid;
	FILE *f;
	char file_name[MAX_NAME_SIZE];
	char program_name[MAX_NAME_SIZE];

	pid = atoi(number);
	if (pid <= 0)
		return 0;
	snprintf(file_name, MAX_NAME_SIZE, "/proc/%s/stat", number);
	f = fopen(file_name, "r");
	if (f == NULL)
		return 0;
	if (fscanf(f, "%d %s", &pid, program_name) != 2) {
		fclose(f);
		return 0;
	}
	if (strncmp(program_name, "(jffs2_gcd_mtd", 14) != 0)
		pid = 0;
	if (pid) {
		size_t sz;
		struct gcd_pid *g;

		sz = sizeof(struct gcd_pid);
		g = (struct gcd_pid *) malloc(sz);
		g->pid = pid;
		g->name = (char *) malloc(strlen(program_name) + 1);
		if (g->name)
			strcpy(g->name, program_name);
		else
			exit(1);
		g->mtd_index = atoi(program_name + 14);
		g->next = gcd_pid_list;
		gcd_pid_list = g;
	}
	fclose(f);
	return pid;
}

int get_pid_list(void)
{
	DIR *dir;
	struct dirent *entry;

	dir = opendir("/proc");
	if (dir == NULL)
		return 1;
	for (;;) {
		entry = readdir(dir);
		if (entry) {
			if (strcmp(".",entry->d_name) != 0 &&
					strcmp("..",entry->d_name) != 0)
				add_gcd_pid(entry->d_name);
		} else
			break;
	}
	closedir(dir);
	return 0;
}

int parse_index_number(const char *name)
{
	const char *p, *q;
	int all_zero;
	int index;

	p = name;
	while (*p && !isdigit(*p))
		++p;
	if (!*p)
		return -1;
	all_zero = 1;
	for (q = p; *q; ++q) {
		if (!isdigit(*q))
			return -1;
		if (*q != '0')
			all_zero = 0;
	}
	if (all_zero)
		return 0;
	index = atoi(p);
	if (index <= 0)
		return -1;
	return index;
}

int get_mtd_index(void)
{
	FILE *f;
	struct mntent *entry;
	struct stat f_info;
	struct stat curr_f_info;
	int found;
	int mtd_index = -1;

	if (stat(tests_file_system_mount_dir, &f_info) == -1)
		return -1;
	f = fopen("/proc/mounts", "rb");
	if (!f)
		f = fopen("/etc/mtab", "rb");
	if (f == NULL)
		return -1;
	found = 0;
	for (;;) {
		entry = getmntent(f);
		if (!entry)
			break;
		if (stat(entry->mnt_dir, &curr_f_info) == -1)
			continue;
		if (f_info.st_dev == curr_f_info.st_dev) {
			int i;

			i = parse_index_number(entry->mnt_fsname);
			if (i != -1) {
				if (found && i != mtd_index)
					return -1;
				found = 1;
				mtd_index = i;
			}
		}
	}
	fclose(f);
	return mtd_index;
}

int get_gcd_pid()
{
	struct gcd_pid *g;
	int mtd_index;

	if (get_pid_list())
		return 0;
	mtd_index = get_mtd_index();
	if (mtd_index == -1)
		return 0;
	for (g = gcd_pid_list; g; g = g->next)
		if (g->mtd_index == mtd_index)
			return g->pid;
	return 0;
}

void gcd_hupper(void)
{
	int64_t repeat;
	int pid;

	pid = get_gcd_pid();
	CHECK(pid != 0);
	repeat = tests_repeat_parameter;
	for (;;) {
		CHECK(kill(pid, SIGHUP) != -1);
		/* Break if repeat count exceeded */
		if (tests_repeat_parameter > 0 && --repeat <= 0)
			break;
		/* Sleep */
		if (tests_sleep_parameter > 0) {
			unsigned us = tests_sleep_parameter * 1000;
			unsigned rand_divisor = RAND_MAX / us;
			unsigned s = (us / 2) + (rand() / rand_divisor);
			usleep(s);
		}
	}
}

/* Title of this test */

const char *gcd_hupper_get_title(void)
{
	return "Send HUP signals to gcd";
}

/* Description of this test */

const char *gcd_hupper_get_description(void)
{
	return
		"Determine the PID of the gcd process. " \
		"Send it SIGHUP (may require root privileges). " \
		"If a sleep value is specified, the process sleeps. " \
		"If a repeat count is specified, then the task repeats " \
		"that number of times. " \
		"The repeat count is given by the -n or --repeat option, " \
		"otherwise it defaults to 1. " \
		"A repeat count of zero repeats forever. " \
		"The sleep value is given by the -p or --sleep option, " \
		"otherwise it defaults to 1. "
		"Sleep is specified in milliseconds.";
}

int main(int argc, char *argv[])
{
	int run_test;

	/* Set default test repetition */
	tests_repeat_parameter = 1;

	/* Set default test sleep */
	tests_sleep_parameter = 1;

	/* Handle common arguments */
	run_test = tests_get_args(argc, argv, gcd_hupper_get_title(),
			gcd_hupper_get_description(), "np");
	if (!run_test)
		return 1;
	/* Change directory to the file system and check it is ok for testing */
	tests_check_test_file_system();
	/* Do the actual test */
	gcd_hupper();
	return 0;
}
