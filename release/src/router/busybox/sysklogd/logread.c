/* vi: set sw=4 ts=4: */
/*
 * circular buffer syslog implementation for busybox
 *
 * Copyright (C) 2000 by Gennady Feldman <gfeldman@gena01.com>
 *
 * Maintainer: Gennady Feldman <gfeldman@gena01.com> as of Mar 12, 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define logread_trivial_usage
//usage:       "[-f]"
//usage:#define logread_full_usage "\n\n"
//usage:       "Show messages in syslogd's circular buffer\n"
//usage:     "\n	-f	Output data as log grows"

#include "libbb.h"
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

#define DEBUG 0

/* our shared key (syslogd.c and logread.c must be in sync) */
enum { KEY_ID = 0x414e4547 }; /* "GENA" */

struct shbuf_ds {
	int32_t size;           // size of data - 1
	int32_t tail;           // end of message list
	char data[1];           // messages
};

static const struct sembuf init_sem[3] = {
	{0, -1, IPC_NOWAIT | SEM_UNDO},
	{1, 0}, {0, +1, SEM_UNDO}
};

struct globals {
	struct sembuf SMrup[1]; // {0, -1, IPC_NOWAIT | SEM_UNDO},
	struct sembuf SMrdn[2]; // {1, 0}, {0, +1, SEM_UNDO}
	struct shbuf_ds *shbuf;
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#define SMrup (G.SMrup)
#define SMrdn (G.SMrdn)
#define shbuf (G.shbuf)
#define INIT_G() do { \
	memcpy(SMrup, init_sem, sizeof(init_sem)); \
} while (0)

static void error_exit(const char *str) NORETURN;
static void error_exit(const char *str)
{
	//release all acquired resources
	shmdt(shbuf);
	bb_perror_msg_and_die(str);
}

/*
 * sem_up - up()'s a semaphore.
 */
static void sem_up(int semid)
{
	if (semop(semid, SMrup, 1) == -1)
		error_exit("semop[SMrup]");
}

static void interrupted(int sig UNUSED_PARAM)
{
	signal(SIGINT, SIG_IGN);
	shmdt(shbuf);
	exit(EXIT_SUCCESS);
}

int logread_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int logread_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned cur;
	int log_semid; /* ipc semaphore id */
	int log_shmid; /* ipc shared memory id */
	smallint follow = getopt32(argv, "f");

	INIT_G();

	log_shmid = shmget(KEY_ID, 0, 0);
	if (log_shmid == -1)
		bb_perror_msg_and_die("can't find syslogd buffer");

	/* Attach shared memory to our char* */
	shbuf = shmat(log_shmid, NULL, SHM_RDONLY);
	if (shbuf == NULL)
		bb_perror_msg_and_die("can't access syslogd buffer");

	log_semid = semget(KEY_ID, 0, 0);
	if (log_semid == -1)
		error_exit("can't get access to semaphores for syslogd buffer");

	signal(SIGINT, interrupted);

	/* Suppose atomic memory read */
	/* Max possible value for tail is shbuf->size - 1 */
	cur = shbuf->tail;

	/* Loop for logread -f, one pass if there was no -f */
	do {
		unsigned shbuf_size;
		unsigned shbuf_tail;
		const char *shbuf_data;
#if ENABLE_FEATURE_LOGREAD_REDUCED_LOCKING
		int i;
		int len_first_part;
		int len_total = len_total; /* for gcc */
		char *copy = copy; /* for gcc */
#endif
		if (semop(log_semid, SMrdn, 2) == -1)
			error_exit("semop[SMrdn]");

		/* Copy the info, helps gcc to realize that it doesn't change */
		shbuf_size = shbuf->size;
		shbuf_tail = shbuf->tail;
		shbuf_data = shbuf->data; /* pointer! */

		if (DEBUG)
			printf("cur:%d tail:%i size:%i\n",
					cur, shbuf_tail, shbuf_size);

		if (!follow) {
			/* advance to oldest complete message */
			/* find NUL */
			cur += strlen(shbuf_data + cur);
			if (cur >= shbuf_size) { /* last byte in buffer? */
				cur = strnlen(shbuf_data, shbuf_tail);
				if (cur == shbuf_tail)
					goto unlock; /* no complete messages */
			}
			/* advance to first byte of the message */
			cur++;
			if (cur >= shbuf_size) /* last byte in buffer? */
				cur = 0;
		} else { /* logread -f */
			if (cur == shbuf_tail) {
				sem_up(log_semid);
				fflush_all();
				sleep(1); /* TODO: replace me with a sleep_on */
				continue;
			}
		}

		/* Read from cur to tail */
#if ENABLE_FEATURE_LOGREAD_REDUCED_LOCKING
		len_first_part = len_total = shbuf_tail - cur;
		if (len_total < 0) {
			/* message wraps: */
			/* [SECOND PART.........FIRST PART] */
			/*  ^data      ^tail    ^cur      ^size */
			len_total += shbuf_size;
		}
		copy = xmalloc(len_total + 1);
		if (len_first_part < 0) {
			/* message wraps (see above) */
			len_first_part = shbuf_size - cur;
			memcpy(copy + len_first_part, shbuf_data, shbuf_tail);
		}
		memcpy(copy, shbuf_data + cur, len_first_part);
		copy[len_total] = '\0';
		cur = shbuf_tail;
#else
		while (cur != shbuf_tail) {
			fputs(shbuf_data + cur, stdout);
			cur += strlen(shbuf_data + cur) + 1;
			if (cur >= shbuf_size)
				cur = 0;
		}
#endif
 unlock:
		/* release the lock on the log chain */
		sem_up(log_semid);

#if ENABLE_FEATURE_LOGREAD_REDUCED_LOCKING
		for (i = 0; i < len_total; i += strlen(copy + i) + 1) {
			fputs(copy + i, stdout);
		}
		free(copy);
#endif
	} while (follow);

	shmdt(shbuf);

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
