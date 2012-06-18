/* test whether fcntl locking works between threads on this Linux system */

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <fcntl.h>

#include <sys/fcntl.h>

#include <sys/wait.h>

#include <errno.h>
#include <pthread.h>

static int sys_waitpid(pid_t pid,int *status,int options)
{
  return waitpid(pid,status,options);
}

#define DATA "conftest.fcntl"

#define SEEK_SET 0

static void *test_thread(void *thread_parm)
{
	int *status = thread_parm;
	int fd, ret;
	struct flock lock;
	
	sleep(2);
	fd = open(DATA, O_RDWR);

	if (fd == -1) {
		fprintf(stderr,"ERROR: failed to open %s (errno=%d)\n", 
			DATA, (int)errno);
		pthread_exit(thread_parm);
	}

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 4;
	lock.l_pid = 0;
		
	/* check if a lock applies */
	ret = fcntl(fd,F_SETLK,&lock);
	if ((ret != -1)) {
		fprintf(stderr,"ERROR: lock test failed (ret=%d errno=%d)\n", ret, (int)errno);
	} else {
		*status = 0;  /* SUCCESS! */
	}
	pthread_exit(thread_parm);
}

/* lock a byte range in a open file */
int main(int argc, char *argv[])
{
	struct flock lock;
	int fd, ret, status=1, rc;
	pid_t pid;
	char *testdir = NULL;
	pthread_t thread_id;
	pthread_attr_t thread_attr;

	testdir = getenv("TESTDIR");
	if (testdir) chdir(testdir);

	alarm(10);

	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&thread_id, &thread_attr, &test_thread, &status);
	pthread_attr_destroy(&thread_attr);
	if (rc == 0) {
		fprintf(stderr,"created thread_id=%lu\n", 
			(unsigned long int)thread_id);
	} else {
		fprintf(stderr,"ERROR: thread create failed, rc=%d\n", rc);
	}

	unlink(DATA);
	fd = open(DATA, O_RDWR|O_CREAT|O_RDWR, 0600);

	if (fd == -1) {
		fprintf(stderr,"ERROR: failed to open %s (errno=%d)\n", 
			DATA, (int)errno);
		exit(1);
	}

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 4;
	lock.l_pid = getpid();

	/* set a 4 byte write lock */
	fcntl(fd,F_SETLK,&lock);

	sleep(4);  /* allow thread to try getting lock */

	unlink(DATA);

#if defined(WIFEXITED) && defined(WEXITSTATUS)
    if(WIFEXITED(status)) {
        status = WEXITSTATUS(status);
    } else {
        status = 1;
    }
#else /* defined(WIFEXITED) && defined(WEXITSTATUS) */
	status = (status == 0) ? 0 : 1;
#endif /* defined(WIFEXITED) && defined(WEXITSTATUS) */

	if (status) {
		fprintf(stderr,"ERROR: lock test failed with status=%d\n", 
			status);
	}

	exit(status);
}
