/*
 *  ipcmk.c - used to create ad-hoc IPC segments
 *
 *  Copyright (C) 2008 Hayden A. James (hayden.james@gmail.com)
 *  Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

#include "nls.h"
#include "c.h"

static const char *progname;

key_t createKey(void)
{
	srandom( time( NULL ) );
	return random();
}

int createShm(size_t size, int permission)
{
	int result = -1;
	int shmid;
	key_t key = createKey();

	if (-1 != (shmid = shmget(key, size, permission | IPC_CREAT)))
		result = shmid;

	return result;
}

int createMsg(int permission)
{
	int result = -1;
	int msgid;
	key_t key = createKey();

	if (-1 != (msgid = msgget(key, permission | IPC_CREAT)))
		result = msgid;

	return result;
}

int createSem(int nsems, int permission)
{
	int result = -1;
	int semid;
	key_t key = createKey();

	if (-1 != (semid = semget(key, nsems, permission | IPC_CREAT)))
		result = semid;

	return result;
}

void usage(int rc)
{
	FILE *out = rc == EXIT_FAILURE ? stderr : stdout;

	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	     _(" %s [options]\n"), progname);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -M <size>     create shared memory segment of size <size>\n"
		" -S <nsems>    create semaphore array with <nsems> elements\n"
		" -Q            create message queue\n"
		" -p <mode>     permission for the resource (default is 0644)\n"), out);

	fputs(_("\nFor more information see ipcmk(1).\n"), out);

	exit(rc);
}

int main(int argc, char **argv)
{
	int permission = 0644;
	int opt;
	size_t size = 0;
	int nsems = 0;
	int doShm = 0, doMsg = 0, doSem = 0;

	progname = program_invocation_short_name;
	if (!progname)
		progname = "ipcmk";

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while((opt = getopt(argc, argv, "hM:QS:p:")) != -1) {
		switch(opt) {
		case 'M':
			size = atoi(optarg);
			doShm = 1;
			break;
		case 'Q':
			doMsg = 1;
			break;
		case 'S':
			nsems = atoi(optarg);
			doSem = 1;
			break;
		case 'p':
			permission = strtoul(optarg, NULL, 8);
			break;
		case 'h':
			usage(EXIT_SUCCESS);
			break;
		default:
			doShm = doMsg = doSem = 0;
			break;
		}
	}

	if(!doShm && !doMsg && !doSem)
		usage(EXIT_FAILURE);

	if (doShm) {
		int shmid;
		if (-1 == (shmid = createShm(size, permission)))
			err(EXIT_FAILURE, _("create share memory failed"));
		else
			printf(_("Shared memory id: %d\n"), shmid);
	}

	if (doMsg) {
		int msgid;
		if (-1 == (msgid = createMsg(permission)))
			err(EXIT_FAILURE, _("create message queue failed"));
		else
			printf(_("Message queue id: %d\n"), msgid);
	}

	if (doSem) {
		int semid;
		if (-1 == (semid = createSem(nsems, permission)))
			err(EXIT_FAILURE, _("create semaphore failed"));
		else
			printf(_("Semaphore id: %d\n"), semid);
	}

	return EXIT_SUCCESS;
}
