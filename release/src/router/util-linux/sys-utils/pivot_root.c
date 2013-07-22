/*
 * pivot_root.c - Change the root file system
 *
 * Copyright (C) 2000 Werner Almesberger
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#define pivot_root(new_root,put_old) syscall(SYS_pivot_root,new_root,put_old)

int main(int argc, const char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s new_root put_old\n", argv[0]);
		return 1;
	}
	if (pivot_root(argv[1], argv[2]) < 0) {
		perror("pivot_root");
		return 1;
	}
	return 0;
}
