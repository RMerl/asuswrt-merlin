/* vi: set sw=4 ts=4: */
/*
 * ipcrm.c - utility to allow removal of IPC objects and data structures.
 *
 * 01 Sept 2004 - Rodney Radford <rradford@mindspring.com>
 * Adapted for busybox from util-linux-2.12a.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* X/OPEN tells us to use <sys/{types,ipc,sem}.h> for semctl() */
/* X/OPEN tells us to use <sys/{types,ipc,msg}.h> for msgctl() */
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
#else
/* according to X/OPEN we have to define it ourselves */
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
	struct seminfo *__buf;
};
#endif

#define IPCRM_LEGACY 1


#if IPCRM_LEGACY

typedef enum type_id {
	SHM,
	SEM,
	MSG
} type_id;

static int remove_ids(type_id type, int argc, char **argv)
{
	unsigned long id;
	int ret = 0;		/* silence gcc */
	int nb_errors = 0;
	union semun arg;

	arg.val = 0;

	while (argc) {
		id = bb_strtoul(argv[0], NULL, 10);
		if (errno || id > INT_MAX) {
			bb_error_msg("invalid id: %s", argv[0]);
			nb_errors++;
		} else {
			if (type == SEM)
				ret = semctl(id, 0, IPC_RMID, arg);
			else if (type == MSG)
				ret = msgctl(id, IPC_RMID, NULL);
			else if (type ==  SHM)
				ret = shmctl(id, IPC_RMID, NULL);

			if (ret) {
				bb_perror_msg("can't remove id %s", argv[0]);
				nb_errors++;
			}
		}
		argc--;
		argv++;
	}

	return nb_errors;
}
#endif /* IPCRM_LEGACY */


int ipcrm_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ipcrm_main(int argc, char **argv)
{
	int c;
	int error = 0;

	/* if the command is executed without parameters, do nothing */
	if (argc == 1)
		return 0;
#if IPCRM_LEGACY
	/* check to see if the command is being invoked in the old way if so
	   then run the old code. Valid commands are msg, shm, sem. */
	{
		type_id what = 0; /* silence gcc */
		char w;

		w=argv[1][0];
		if ( ((w == 'm' && argv[1][1] == 's' && argv[1][2] == 'g')
		       || (argv[1][0] == 's'
		           && ((w=argv[1][1]) == 'h' || w == 'e')
		           && argv[1][2] == 'm')
		     ) && argv[1][3] == '\0'
		) {

			if (argc < 3)
				bb_show_usage();

			if (w == 'h')
				what = SHM;
			else if (w == 'm')
				what = MSG;
			else if (w == 'e')
				what = SEM;

			if (remove_ids(what, argc-2, &argv[2]))
				fflush_stdout_and_exit(EXIT_FAILURE);
			printf("resource(s) deleted\n");
			return 0;
		}
	}
#endif /* IPCRM_LEGACY */

	/* process new syntax to conform with SYSV ipcrm */
	while ((c = getopt(argc, argv, "q:m:s:Q:M:S:h?")) != -1) {
		int result;
		int id = 0;
		int iskey = isupper(c);

		/* needed to delete semaphores */
		union semun arg;

		arg.val = 0;

		if ((c == '?') || (c == 'h')) {
			bb_show_usage();
		}

		/* we don't need case information any more */
		c = tolower(c);

		/* make sure the option is in range: allowed are q, m, s */
		if (c != 'q' && c != 'm' && c != 's') {
			bb_show_usage();
		}

		if (iskey) {
			/* keys are in hex or decimal */
			key_t key = xstrtoul(optarg, 0);

			if (key == IPC_PRIVATE) {
				error++;
				bb_error_msg("illegal key (%s)", optarg);
				continue;
			}

			/* convert key to id */
			id = ((c == 'q') ? msgget(key, 0) :
				  (c == 'm') ? shmget(key, 0, 0) : semget(key, 0, 0));

			if (id < 0) {
				const char *errmsg;

				error++;
				switch (errno) {
				case EACCES:
					errmsg = "permission denied for";
					break;
				case EIDRM:
					errmsg = "already removed";
					break;
				case ENOENT:
					errmsg = "invalid";
					break;
				default:
					errmsg = "unknown error in";
					break;
				}
				bb_error_msg("%s %s (%s)", errmsg, "key", optarg);
				continue;
			}
		} else {
			/* ids are in decimal */
			id = xatoul(optarg);
		}

		result = ((c == 'q') ? msgctl(id, IPC_RMID, NULL) :
				  (c == 'm') ? shmctl(id, IPC_RMID, NULL) :
				  semctl(id, 0, IPC_RMID, arg));

		if (result) {
			const char *errmsg;
			const char *const what = iskey ? "key" : "id";

			error++;
			switch (errno) {
			case EACCES:
			case EPERM:
				errmsg = "permission denied for";
				break;
			case EINVAL:
				errmsg = "invalid";
				break;
			case EIDRM:
				errmsg = "already removed";
				break;
			default:
				errmsg = "unknown error in";
				break;
			}
			bb_error_msg("%s %s (%s)", errmsg, what, optarg);
			continue;
		}
	}

	/* print usage if we still have some arguments left over */
	if (optind != argc) {
		bb_show_usage();
	}

	/* exit value reflects the number of errors encountered */
	return error;
}
