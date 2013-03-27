#include <fcntl.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	int pid = atoi(argv[1]);
	int sfd;
	char sname[32];
	char buf[1024];
	char *s;
	int i;
	int signal, blocked, ignore, caught;

	sprintf(sname, "/proc/%d/stat", pid);

	if ((sfd = open(sname, O_RDONLY)) == -1) {
		perror(sname);
		return 1;
	}

	i = read(sfd, buf, 1024);
	buf[i] = '\0';

	for (i = 0, s = buf; i < 30; i++) {
		while (*++s != ' ') {
			if (!*s)
				break;
		}
	}

	if (sscanf(s, "%d%d%d%d", &signal, &blocked, &ignore, &caught) != 4) {
		fprintf(stderr, "/proc/pid/stat format error\n");
		return 1;
	}

	printf("%8x %8x %8x %8x\n", signal, blocked, ignore, caught);

	return 0;
}
