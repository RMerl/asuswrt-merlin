#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static void print_help()
{
	printf("ebtablesu v"PROGVERSION" ("PROGDATE")\n");
	printf(
"Usage:\n"
"ebtablesu open table         : copy the kernel table\n"
"ebtablesu fopen table file   : copy the table from the specified file\n"
"ebtablesu free table         : remove the table from memory\n"
"ebtablesu commit table       : commit the table to the kernel\n"
"ebtablesu fcommit table file : commit the table to the specified file\n\n"
"ebtablesu <ebtables options> : the ebtables specifications\n"
"                               use spaces only to separate options and commands\n"
"For the ebtables options, see\n# ebtables -h\nor\n# man ebtables\n"
	);
}
int main(int argc, char *argv[])
{
	char *arguments, *pos;
	int i, writefd, len = 0;

	if (argc > EBTD_ARGC_MAX) {
		fprintf(stderr, "ebtablesd accepts at most %d arguments, %d "
		        "arguments were specified. If you need this many "
		        "arguments, recompile this tool with a higher value for"
		        " EBTD_ARGC_MAX.\n", EBTD_ARGC_MAX - 1, argc - 1);
		return -1;
	} else if (argc == 1) {
		fprintf(stderr, "At least one argument is needed.\n");
		print_help();
		exit(0);
	}

	for (i = 0; i < argc; i++)
		len += strlen(argv[i]);
	/* Don't forget '\0' */
	len += argc;
	if (len > EBTD_CMDLINE_MAXLN) {
		fprintf(stderr, "ebtablesd has a maximum command line argument "
		       "length of %d, an argument length of %d was received. "
		       "If a smaller length is unfeasible, recompile this tool "
		       "with a higher value for EBTD_CMDLINE_MAXLN.\n",
		       EBTD_CMDLINE_MAXLN, len);
		return -1;
	}

	if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		if (argc != 2) {
			fprintf(stderr, "%s does not accept options.\n", argv[1]);
			return -1;
		}
		print_help();
		exit(0);
	}

	if (!(arguments = (char *)malloc(len))) {
		fprintf(stderr, "ebtablesu: out of memory.\n");
		return -1;
	}

	if ((writefd = open(EBTD_PIPE, O_WRONLY, 0)) == -1) {
		fprintf(stderr, "Could not open the pipe, perhaps ebtablesd is "
		        "not running or you don't have write permission (try "
		        "running as root).\n");
		return -1;
	}

	pos = arguments;
	for (i = 0; i < argc; i++) {
		strcpy(pos, argv[i]);
		pos += strlen(argv[i]);
		*(pos++) = ' ';
	}

	*(pos-1) = '\n';
	if (write(writefd, arguments, len) == -1) {
		perror("write");
		return -1;
	}
	return 0;
}
