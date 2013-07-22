/*
 * krishna balasubramanian 1993
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 1999-04-02 frank zago
 * - can now remove several id's in the same call
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include "nls.h"

/* for getopt */
#include <unistd.h>
/* for tolower and isupper */
#include <ctype.h>

#ifndef HAVE_UNION_SEMUN
/* according to X/OPEN we have to define it ourselves */
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};
#endif

static void usage(char *);

char *execname;

typedef enum type_id {
	SHM,
	SEM,
	MSG
} type_id;

static int
remove_ids(type_id type, int argc, char **argv) {
	int id;
	int ret = 0;		/* for gcc */
	char *end;
	int nb_errors = 0;
	union semun arg;

	arg.val = 0;

	while(argc) {

		id = strtoul(argv[0], &end, 10);

		if (*end != 0) {
			printf (_("invalid id: %s\n"), argv[0]);
			nb_errors ++;
		} else {
			switch(type) {
			case SEM:
				ret = semctl (id, 0, IPC_RMID, arg);
				break;

			case MSG:
				ret = msgctl (id, IPC_RMID, NULL);
				break;
				
			case SHM:
				ret = shmctl (id, IPC_RMID, NULL);
				break;
			}

			if (ret) {
				printf (_("cannot remove id %s (%s)\n"),
					argv[0], strerror(errno));
				nb_errors ++;
			}
		}
		argc--;
		argv++;
	}
	
	return(nb_errors);
}

static void deprecate_display_usage(void)
{
	usage(execname);
	printf (_("deprecated usage: %s {shm | msg | sem} id ...\n"),
		execname);
}

static int deprecated_main(int argc, char **argv)
{
	execname = argv[0];

	if (argc < 3) {
		deprecate_display_usage();
		exit(1);
	}
	
	if (!strcmp(argv[1], "shm")) {
		if (remove_ids(SHM, argc-2, &argv[2]))
			exit(1);
	}
	else if (!strcmp(argv[1], "msg")) {
		if (remove_ids(MSG, argc-2, &argv[2]))
			exit(1);
	} 
	else if (!strcmp(argv[1], "sem")) {
		if (remove_ids(SEM, argc-2, &argv[2]))
			exit(1);
	}
	else {
		deprecate_display_usage();
		printf (_("unknown resource type: %s\n"), argv[1]);
		exit(1);
	}

	printf (_("resource(s) deleted\n"));
	return 0;
}


/* print the new usage */
static void
usage(char *progname)
{
	fprintf(stderr,
		_("usage: %s [ [-q msqid] [-m shmid] [-s semid]\n"
		  "          [-Q msgkey] [-M shmkey] [-S semkey] ... ]\n"),
		progname);
}

int main(int argc, char **argv)
{
	int   c;
	int   error = 0;
	char *prog = argv[0];

	/* if the command is executed without parameters, do nothing */
	if (argc == 1)
		return 0;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/* check to see if the command is being invoked in the old way if so
	   then run the old code */
	if (strcmp(argv[1], "shm") == 0 ||
	    strcmp(argv[1], "msg") == 0 ||
	    strcmp(argv[1], "sem") == 0)
		return deprecated_main(argc, argv);

	/* process new syntax to conform with SYSV ipcrm */
	while ((c = getopt(argc, argv, "q:m:s:Q:M:S:")) != -1) {
		int result;
		int id = 0;
		int iskey = isupper(c);

		/* needed to delete semaphores */
		union semun arg;
		arg.val = 0;

		/* we don't need case information any more */
		c = tolower(c);

		/* make sure the option is in range */
		if (c != 'q' && c != 'm' && c != 's') {
			usage(prog);
			error++;
			return error;
		}

		if (iskey) {
			/* keys are in hex or decimal */
			key_t key = strtoul(optarg, NULL, 0);
			if (key == IPC_PRIVATE) {
				error++;
				fprintf(stderr, _("%s: illegal key (%s)\n"),
					prog, optarg);
				continue;
			}

			/* convert key to id */
			id = ((c == 'q') ? msgget(key, 0) :
			      (c == 'm') ? shmget(key, 0, 0) :
			      semget(key, 0, 0));

			if (id < 0) {
				char *errmsg;
				error++;
				switch(errno) {
				case EACCES:
					errmsg = _("permission denied for key");
					break;
				case EIDRM:
					errmsg = _("already removed key");
					break;
				case ENOENT:
					errmsg = _("invalid key");
					break;
				default:
					errmsg = _("unknown error in key");
					break;
				}
				fprintf(stderr, "%s: %s (%s)\n",
					prog, errmsg, optarg);
				continue;
			}
		} else {
			/* ids are in decimal */
			id = strtoul(optarg, NULL, 10);
		}

		result = ((c == 'q') ? msgctl(id, IPC_RMID, NULL) :
			  (c == 'm') ? shmctl(id, IPC_RMID, NULL) : 
			  semctl(id, 0, IPC_RMID, arg));

		if (result < 0) {
			char *errmsg;
			error++;
			switch(errno) {
			case EACCES:
			case EPERM:
				errmsg = iskey
					? _("permission denied for key")
					: _("permission denied for id");
				break;
			case EINVAL:
				errmsg = iskey
					? _("invalid key")
					: _("invalid id");
				break;
			case EIDRM:
				errmsg = iskey
					? _("already removed key")
					: _("already removed id");
				break;
			default:
				errmsg = iskey
					? _("unknown error in key")
					: _("unknown error in id");
				break;
			}
			fprintf(stderr, _("%s: %s (%s)\n"),
				prog, errmsg, optarg);
			continue;
		}
	}

	/* print usage if we still have some arguments left over */
	if (optind != argc) {
		fprintf(stderr, _("%s: unknown argument: %s\n"),
			prog, argv[optind]);
		usage(prog);
	}

	/* exit value reflects the number of errors encountered */
	return error;
}
