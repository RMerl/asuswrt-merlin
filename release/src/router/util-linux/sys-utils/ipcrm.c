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

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include "c.h"
#include "nls.h"
#include "strutils.h"

#ifndef HAVE_UNION_SEMUN
/* according to X/OPEN we have to define it ourselves */
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};
#endif

typedef enum type_id {
	SHM,
	SEM,
	MSG,
	ALL
} type_id;

static int verbose = 0;

/* print the new usage */
static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fprintf(out, USAGE_HEADER);
	fprintf(out, " %s [options]\n", program_invocation_short_name);
	fprintf(out, " %s <shm|msg|sem> <id> [...]\n", program_invocation_short_name);
	fprintf(out, USAGE_OPTIONS);
	fputs(_(" -m, --shmem-id <id>        remove shared memory segment by shmid\n"), out);
	fputs(_(" -M, --shmem-key <key>      remove shared memory segment by key\n"), out);
	fputs(_(" -q, --queue-id <id>        remove message queue by id\n"), out);
	fputs(_(" -Q, --queue-key <key>      remove message queue by key\n"), out);
	fputs(_(" -s, --semaphore-id <id>    remove semaprhore by id\n"), out);
	fputs(_(" -S, --semaphore-key <key>  remove semaprhore by key\n"), out);
	fputs(_(" -a, --all[=<shm|msg|sem>]  remove all\n"), out);
	fputs(_(" -v, --verbose              explain what is being done\n"), out);
	fprintf(out, USAGE_SEPARATOR);
	fprintf(out, USAGE_HELP);
	fprintf(out, USAGE_VERSION);
	fprintf(out, USAGE_MAN_TAIL("ipcrm(1)"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int remove_id(int type, int iskey, int id)
{
        int ret;
	char *errmsg;
	/* needed to delete semaphores */
	union semun arg;
	arg.val = 0;

	/* do the removal */
	switch (type) {
	case SHM:
		if (verbose)
			printf(_("removing shared memory segment id `%d'\n"), id);
		ret = shmctl(id, IPC_RMID, NULL);
		break;
	case MSG:
		if (verbose)
			printf(_("removing message queue id `%d'\n"), id);
		ret = msgctl(id, IPC_RMID, NULL);
		break;
	case SEM:
		if (verbose)
			printf(_("removing semaphore id `%d'\n"), id);
		ret = semctl(id, 0, IPC_RMID, arg);
		break;
	default:
		errx(EXIT_FAILURE, "impossible occurred");
	}

	/* how did the removal go? */
	if (ret < 0) {
		switch (errno) {
		case EACCES:
		case EPERM:
			errmsg = iskey ? _("permission denied for key") : _("permission denied for id");
			break;
		case EINVAL:
			errmsg = iskey ? _("invalid key") : _("invalid id");
			break;
		case EIDRM:
			errmsg = iskey ? _("already removed key") : _("already removed id");
			break;
		default:
			err(EXIT_FAILURE, iskey ? _("key failed") : _("id failed"));
		}
		warnx("%s (%d)", errmsg, id);
		return 1;
	}
	return 0;
}

static int remove_arg_list(type_id type, int argc, char **argv)
{
	int id;
	char *end;
	int nb_errors = 0;

	do {
		id = strtoul(argv[0], &end, 10);
		if (*end != 0) {
			warnx(_("invalid id: %s"), argv[0]);
			nb_errors++;
		} else {
			if (remove_id(type, 0, id))
				nb_errors++;
		}
		argc--;
		argv++;
	} while (argc);
	return (nb_errors);
}

static int deprecated_main(int argc, char **argv)
{
	type_id type;

	if (!strcmp(argv[1], "shm"))
		type = SHM;
	else if (!strcmp(argv[1], "msg"))
		type = MSG;
	else if (!strcmp(argv[1], "sem"))
		type = SEM;
	else
		return 0;

	if (argc < 3) {
		warnx(_("not enough arguments"));
		usage(stderr);
	}

	if (remove_arg_list(type, argc - 2, &argv[2]))
		exit(EXIT_FAILURE);

	printf(_("resource(s) deleted\n"));
	return 1;
}

static unsigned long strtokey(const char *str, const char *errmesg)
{
	unsigned long num;
	char *end = NULL;

	if (str == NULL || *str == '\0')
		goto err;
	errno = 0;
	/* keys are in hex or decimal */
	num = strtoul(str, &end, 0);

	if (errno || str == end || (end && *end))
		goto err;

	return num;
 err:
	if (errno)
		err(EXIT_FAILURE, "%s: '%s'", errmesg, str);
	else
		errx(EXIT_FAILURE, "%s: '%s'", errmesg, str);
	return 0;
}

static int key_to_id(type_id type, char *optarg)
{
	int id;
	/* keys are in hex or decimal */
	key_t key = strtokey(optarg, "failed to parse argument");
	if (key == IPC_PRIVATE) {
		warnx(_("illegal key (%s)"), optarg);
		return -1;
	}
	switch (type) {
	case SHM:
		id = shmget(key, 0, 0);
		break;
	case MSG:
		id = msgget(key, 0);
		break;
	case SEM:
		id = semget(key, 0, 0);
		break;
	case ALL:
		abort();
	default:
		errx(EXIT_FAILURE, "impossible occurred");
	}
	if (id < 0) {
		char *errmsg;
		switch (errno) {
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
			err(EXIT_FAILURE, _("key failed"));
		}
		warnx("%s (%s)", errmsg, optarg);
	}
	return id;
}

static int remove_all(type_id type)
{
	int ret = 0;
	int id, rm_me, maxid;

	struct shmid_ds shmseg;
	struct shm_info shm_info;

	struct semid_ds semary;
	struct seminfo seminfo;
	union semun arg;

	struct msqid_ds msgque;
	struct msginfo msginfo;

	if (type == SHM || type == ALL) {
		maxid =
		    shmctl(0, SHM_INFO, (struct shmid_ds *)(void *)&shm_info);
		if (maxid < 0)
			errx(EXIT_FAILURE,
			     _("kernel not configured for shared memory"));
		for (id = 0; id <= maxid; id++) {
			rm_me = shmctl(id, SHM_STAT, &shmseg);
			if (rm_me < 0)
				continue;
			ret |= remove_id(SHM, 0, rm_me);
		}
	}
	if (type == SEM || type == ALL) {
		arg.array = (ushort *) (void *)&seminfo;
		maxid = semctl(0, 0, SEM_INFO, arg);
		if (maxid < 0)
			errx(EXIT_FAILURE,
			     _("kernel not configured for semaphores"));
		for (id = 0; id <= maxid; id++) {
			arg.buf = (struct semid_ds *)&semary;
			rm_me = semctl(id, 0, SEM_STAT, arg);
			if (rm_me < 0)
				continue;
			ret |= remove_id(SEM, 0, rm_me);
		}
	}
	if (type == MSG || type == ALL) {
		maxid =
		    msgctl(0, MSG_INFO, (struct msqid_ds *)(void *)&msginfo);
		if (maxid < 0)
			errx(EXIT_FAILURE,
			     _("kernel not configured for message queues"));
		for (id = 0; id <= maxid; id++) {
			rm_me = msgctl(id, MSG_STAT, &msgque);
			if (rm_me < 0)
				continue;
			ret |= remove_id(MSG, 0, rm_me);
		}
	}
	return ret;
}

int main(int argc, char **argv)
{
	int c;
	int ret = 0;
	int id = -1;
	int iskey;
	int rm_all = 0;
	type_id what_all;

	static const struct option longopts[] = {
		{"shmem-id", required_argument, NULL, 'm'},
		{"shmem-key", required_argument, NULL, 'M'},
		{"queue-id", required_argument, NULL, 'q'},
		{"queue-key", required_argument, NULL, 'Q'},
		{"semaphore-id", required_argument, NULL, 's'},
		{"semaphore-key", required_argument, NULL, 'S'},
		{"all", optional_argument, NULL, 'a'},
		{"verbose", no_argument, NULL, 'v'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	/* if the command is executed without parameters, do nothing */
	if (argc == 1)
		return 0;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/* check to see if the command is being invoked in the old way if so
	 * then remove argument list */
	if (deprecated_main(argc, argv))
		return EXIT_SUCCESS;

	/* process new syntax to conform with SYSV ipcrm */
	while((c = getopt_long(argc, argv, "q:m:s:Q:M:S:a::vhV", longopts, NULL)) != -1) {
		iskey = 0;
		switch (c) {
		case 'M':
			iskey = 1;
			id = key_to_id(SHM, optarg);
			if (id < 0) {
				ret++;
				break;
			}
		case 'm':
			if (!iskey)
				id = strtoll_or_err(optarg, _("failed to parse argument"));
			if (remove_id(SHM, iskey, id))
				ret++;
			break;
		case 'Q':
			iskey = 1;
			id = key_to_id(MSG, optarg);
			if (id < 0) {
				ret++;
				break;
			}
		case 'q':
			if (!iskey)
				id = strtoll_or_err(optarg, _("failed to parse argument"));
			if (remove_id(MSG, iskey, id))
				ret++;
			break;
		case 'S':
			iskey = 1;
			id = key_to_id(SEM, optarg);
			if (id < 0) {
				ret++;
				break;
			}
		case 's':
			if (!iskey)
				id = strtoll_or_err(optarg, _("failed to parse argument"));
			if (remove_id(SEM, iskey, id))
				ret++;
			break;
		case 'a':
			rm_all = 1;
			if (optarg) {
				if (!strcmp(optarg, "shm"))
					what_all = SHM;
				else if (!strcmp(optarg, "msg"))
					what_all = MSG;
				else if (!strcmp(optarg, "sem"))
					what_all = SEM;
				else
					errx(EXIT_FAILURE,
					     _("unknown argument: %s"), optarg);
			} else {
				what_all = ALL;
			}
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
			usage(stdout);
		case 'V':
			printf(UTIL_LINUX_VERSION);
			return EXIT_SUCCESS;
		default:
			usage(stderr);
		}
	}

	if (rm_all)
		if (remove_all(what_all))
			ret++;

	/* print usage if we still have some arguments left over */
	if (optind < argc) {
		warnx(_("unknown argument: %s"), argv[optind]);
		usage(stderr);
	}

	return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
