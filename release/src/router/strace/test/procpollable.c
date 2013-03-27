#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/procfs.h>
#include <sys/stropts.h>
#include <poll.h>

int main(int argc, char *argv[])
{
	int pid;
	char proc[32];
	FILE *pfp;
	struct pollfd pfd;

	if ((pid = fork()) == 0) {
		pause();
		exit(0);
	}

	sprintf(proc, "/proc/%d", pid);

	if ((pfp = fopen(proc, "r+")) == NULL)
		goto fail;

	if (ioctl(fileno(pfp), PIOCSTOP, NULL) < 0)
		goto fail;

	pfd.fd = fileno(pfp);
	pfd.events = POLLPRI;

	if (poll(&pfd, 1, 0) < 0)
		goto fail;

	if (!(pfd.revents & POLLPRI))
		goto fail;

	kill(pid, SIGKILL);
	exit(0);
fail:
	kill(pid, SIGKILL);
	exit(1);
}
