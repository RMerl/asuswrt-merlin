/* test whether ftruncte() can truncate a file as non-root */

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

#define DATA "conftest.truncroot"

static int sys_waitpid(pid_t pid,int *status,int options)
{
#ifdef HAVE_WAITPID
  return waitpid(pid,status,options);
#else /* USE_WAITPID */
  return wait4(pid, status, options, NULL);
#endif /* USE_WAITPID */
}

main()
{
	int fd;
	char buf[1024];
	pid_t pid;

    if (getuid() != 0) {
      fprintf(stderr,"ERROR: This test must be run as root - assuming \
ftruncate doesn't need root.\n");
      exit(1);
    }

	fd = open(DATA,O_RDWR|O_CREAT|O_TRUNC,0666);
	if(!fd)
		exit(1);

	if(write(fd, buf, 1024) != 1024)
		exit(1);

	if((pid = fork()) < 0)
		exit(1);

	if(pid) {
		/* Parent. */
		int status = 1;
		if(sys_waitpid(pid, &status, 0) != pid) {
			unlink(DATA);
			exit(1);
		}
		unlink(DATA);
		exit(WEXITSTATUS(status));
	}

	/* Child. */
	if(setuid(500) < 0)
		exit(1);

	if(ftruncate(fd, 0) < 0) {
		if(errno == EPERM || errno == EACCES)
			exit(0);
	}
	exit(1);
}
