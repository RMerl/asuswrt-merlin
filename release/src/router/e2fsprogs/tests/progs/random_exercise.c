/*
 * random_exercise.c --- Test program which exercises an ext2
 * 	filesystem.  It creates a lot of random files in the current
 * 	directory, while holding some files open while they are being
 * 	deleted.  This exercises the orphan list code, as well as
 * 	creating lots of fodder for the ext3 journal.
 *
 * Copyright (C) 2000 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "config.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAXFDS	128

struct state {
	char	name[16];
	int	state;
	int	isdir;
};

#define STATE_CLEAR	0
#define STATE_CREATED	1
#define STATE_DELETED	2

struct state state_array[MAXFDS];

#define DATA_SIZE 65536

char data_buffer[DATA_SIZE];

void clear_state_array()
{
	int	i;

	for (i = 0; i < MAXFDS; i++)
		state_array[i].state = STATE_CLEAR;
}

int get_random_fd()
{
	int	fd;

	while (1) {
		fd = ((int) random()) % MAXFDS;
		if (fd > 2)
			return fd;
	}
}

unsigned int get_inode_num(int fd)
{
	struct stat st;

	if (fstat(fd, &st) < 0) {
		perror("fstat");
		return 0;
	}
	return st.st_ino;
}


void create_random_file()
{
	char template[16] = "EX.XXXXXX";
	int	fd;
	int	isdir = 0;
	int	size;

	mktemp(template);
	isdir = random() & 1;
	if (isdir) {
		if (mkdir(template, 0700) < 0)
			return;
		fd = open(template, O_RDONLY, 0600);
		printf("Created temp directory %s, fd = %d\n",
		       template, fd);
	} else {
		size = random() & (DATA_SIZE-1);
		fd = open(template, O_CREAT|O_RDWR, 0600);
		write(fd, data_buffer, size);
		printf("Created temp file %s, fd = %d, size=%d\n",
		       template, fd, size);
	}
	state_array[fd].isdir = isdir;
	if (fd < 0)
		return;
	state_array[fd].isdir = isdir;
	state_array[fd].state = STATE_CREATED;
	strcpy(state_array[fd].name, template);
}

void truncate_file(int fd)
{
	int	size;

	size = random() & (DATA_SIZE-1);

	if (state_array[fd].isdir)
		return;

	ftruncate(fd, size);
	printf("Truncating temp file %s, fd = %d, ino=%u, size=%d\n",
	       state_array[fd].name, fd, get_inode_num(fd), size);
}


void unlink_file(int fd)
{
	char *filename = state_array[fd].name;

	printf("Deleting %s, fd = %d, ino = %u\n", filename, fd,
	       get_inode_num(fd));

	if (state_array[fd].isdir)
		rmdir(filename);
	else
		unlink(filename);
	state_array[fd].state = STATE_DELETED;
}

void close_file(int fd)
{
	char *filename = state_array[fd].name;

	printf("Closing %s, fd = %d, ino = %u\n", filename, fd,
	       get_inode_num(fd));

	close(fd);
	state_array[fd].state = STATE_CLEAR;
}


main(int argc, char **argv)
{
	int	i, fd;

	memset(data_buffer, 0, sizeof(data_buffer));
	sprintf(data_buffer, "This is a test file created by the "
		"random_exerciser program\n");

	for (i=0; i < 100000; i++) {
		fd = get_random_fd();
		switch (state_array[fd].state) {
		case STATE_CLEAR:
			create_random_file();
			break;
		case STATE_CREATED:
			if ((state_array[fd].isdir == 0) &&
			    (random() & 2))
				truncate_file(fd);
			else
				unlink_file(fd);
			break;
		case STATE_DELETED:
			close_file(fd);
			break;
		}
	}
}


