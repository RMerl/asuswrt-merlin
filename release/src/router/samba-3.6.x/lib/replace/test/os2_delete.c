/*
  test readdir/unlink pattern that OS/2 uses
  tridge@samba.org July 2005
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#define NUM_FILES 700
#define READDIR_SIZE 100
#define DELETE_SIZE 4

#define TESTDIR "test.dir"

static int test_readdir_os2_delete_ret;

#define FAILED(d) (printf("failure: readdir [\nFailed for %s - %d = %s\n]\n", d, errno, strerror(errno)), test_readdir_os2_delete_ret = 1, 1)

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifdef _WIN32
#define mkdir(d,m) _mkdir(d)
#endif

static void cleanup(void)
{
	/* I'm a lazy bastard */
	if (system("rm -rf " TESTDIR)) {
		FAILED("system");
	}
	mkdir(TESTDIR, 0700) == 0 || FAILED("mkdir");
}

static void create_files(void)
{
	int i;
	for (i=0;i<NUM_FILES;i++) {
		char fname[40];
		int fd;
		snprintf(fname, sizeof(fname), TESTDIR "/test%u.txt", i);
		fd = open(fname, O_CREAT|O_RDWR, 0600);
		if (fd < 0) {
			FAILED("open");
		}
		if (close(fd) != 0) {
			FAILED("close");
		}
	}
}

static int os2_delete(DIR *d)
{
	off_t offsets[READDIR_SIZE];
	int i, j;
	struct dirent *de;
	char names[READDIR_SIZE][30];

	/* scan, remembering offsets */
	for (i=0, de=readdir(d); 
	     de && i < READDIR_SIZE; 
	     de=readdir(d), i++) {
		offsets[i] = telldir(d);
		strcpy(names[i], de->d_name);
	}

	if (i == 0) {
		return 0;
	}

	/* delete the first few */
	for (j=0; j<MIN(i, DELETE_SIZE); j++) {
		char fname[40];
		snprintf(fname, sizeof(fname), TESTDIR "/%s", names[j]);
		unlink(fname) == 0 || FAILED("unlink");
	}

	/* seek to just after the deletion */
	seekdir(d, offsets[j-1]);

	/* return number deleted */
	return j;
}

int test_readdir_os2_delete(void)
{
	int total_deleted = 0;
	DIR *d;
	struct dirent *de;

	test_readdir_os2_delete_ret = 0;

	cleanup();
	create_files();

	d = opendir(TESTDIR "/test0.txt");
	if (d != NULL) FAILED("opendir() on file succeed");
	if (errno != ENOTDIR) FAILED("opendir() on file didn't give ENOTDIR");

	d = opendir(TESTDIR);

	/* skip past . and .. */
	de = readdir(d);
	strcmp(de->d_name, ".") == 0 || FAILED("match .");
	de = readdir(d);
	strcmp(de->d_name, "..") == 0 || FAILED("match ..");

	while (1) {
		int n = os2_delete(d);
		if (n == 0) break;
		total_deleted += n;
	}
	closedir(d);

	fprintf(stderr, "Deleted %d files of %d\n", total_deleted, NUM_FILES);

	rmdir(TESTDIR) == 0 || FAILED("rmdir");

	if (system("rm -rf " TESTDIR) == -1) {
		FAILED("system");
	}

	return test_readdir_os2_delete_ret;
}
