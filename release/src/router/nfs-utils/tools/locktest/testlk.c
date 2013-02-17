#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#ifdef linux
#include <getopt.h>
#endif
#include <fcntl.h>

static void	usage(int exval);
static void	fatal(char *);

int
main(int argc, char **argv)
{
	unsigned long	start = 0, len = 0;
	struct flock	fl;
	int		c, fd, cmd, typ;
	char		*fname;

	typ = F_RDLCK;
	cmd = F_SETLK;

	while ((c = getopt(argc, argv, "bhrtw")) != EOF) {
		switch (c) {
		case 'h':
			usage(0);
		case 'r':
			cmd = F_SETLK;
			typ = F_RDLCK;
			break;
		case 'w':
			cmd = F_SETLK;
			typ = F_WRLCK;
			break;
		case 'b':
			cmd = F_SETLKW;
			typ = F_WRLCK;
			break;
		case 't':
			cmd = F_GETLK;
			break;
		case '?':
			usage(1);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc <= 0 || argc > 3)
		usage(1);

	fname = argv[0];
	/* printf("TP\n"); */
	if (argc > 1)
		start = atoi(argv[1]);
	/* printf("TP\n"); */
	if (argc > 2)
		len   = atoi(argv[2]);
	/* printf("TP\n"); */

	if ((fd = open(fname, O_RDWR, 0644)) < 0)
		fatal(fname);

	/* printf("TP1\n"); */
	fl.l_type = typ;
	fl.l_whence = 0;
	fl.l_start = start;
	fl.l_len = len;

	if (fcntl(fd, cmd, &fl) < 0)
		fatal("fcntl");
	printf("fcntl: ok\n");
	
	/* printf("TP2\n"); */
	if (cmd == F_GETLK) {
		if (fl.l_type == F_UNLCK) {
			printf("%s: no conflicting lock\n", fname);
		} else {
			printf("%s: conflicting lock by %d on (%lld;%lld)\n",
				fname, fl.l_pid, fl.l_start, fl.l_len);
		}
		return 0;
	}

	/* printf("TP3\n"); */
	pause();
	return 0;
}

static void
usage(int exval)
{
	fprintf(stderr, "usage: testlk filename [start [len]]\n");
	exit(exval);
}

static void
fatal(char *msg)
{
	perror(msg);
	exit(2);
}
