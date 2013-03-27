#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
/* TESTS :
 * - open(const char *pathname, int flags, mode_t mode);
1) Attempt to create file that already exists - EEXIST
2) Attempt to open a directory for writing - EISDIR
3) Pathname does not exist - ENOENT
4) Open for write but no write permission - EACCES   

read(int fd, void *buf, size_t count);
1) Read using invalid file descriptor - EBADF

write(int fd, const void *buf, size_t count);
1) Write using invalid file descriptor - EBADF
2) Attempt to write to read-only file - EBADF

lseek(int fildes, off_t offset, int whence);
1) Seeking on an invalid file descriptor - EBADF
2) Invalid "whence" (3rd param) value -  EINVAL

close(int fd);
1) Attempt to close an invalid file descriptor - EBADF

stat(const char *file_name, struct stat *buf);
1) Pathname is a null string -  ENOENT
2) Pathname does not exist - ENOENT

fstat(int filedes, struct stat *buf);
1) Attempt to stat using an invalid file descriptor - EBADF

isatty (int desc);
Not applicable. We will test that it returns 1 when expected and a case
where it should return 0.

rename(const char *oldpath, const char *newpath);
1) newpath is an existing directory, but oldpath is not a directory. - EISDIR
2) newpath is a non-empty directory. - ENOTEMPTY or EEXIST
3) newpath is a subdirectory of old path. - EINVAL
4) oldpath does not exist. - ENOENT

unlink(const char *pathname);
1) pathname does not have write access. - EACCES
2) pathname does not exist. - ENOENT

time(time_t *t);
Not applicable.

system (const char * string);
1) Invalid string/command. -  returns 127.  */
static const char *strerrno (int err);

#define FILENAME    "foo.fileio.test"
#define RENAMED     "bar.fileio.test"
#define NONEXISTANT "nofoo.fileio.test"
#define NOWRITE     "nowrt.fileio.test"
#define TESTDIR1     "dir1.fileio.test"
#define TESTDIR2     "dir2.fileio.test"
#define TESTSUBDIR   "dir1.fileio.test/subdir.fileio.test"

#define STRING      "Hello World"

static void stop () {}

int
test_open ()
{
  int ret;

  /* Test opening */
  errno = 0;
  ret = open (FILENAME, O_CREAT | O_TRUNC | O_RDWR, S_IWUSR | S_IRUSR);
  printf ("open 1: ret = %d, errno = %d %s\n", ret, errno,
	  ret >= 0 ? "OK" : "");
  
  if (ret >= 0)
    close (ret);
  stop ();
  /* Creating an already existing file (created by fileio.exp) */
  errno = 0;
  ret = open (FILENAME, O_CREAT | O_EXCL | O_WRONLY, S_IWUSR | S_IRUSR);
  printf ("open 2: ret = %d, errno = %d %s\n", ret, errno,
	  strerrno (errno));
  if (ret >= 0)
    close (ret);
  stop ();
  /* Open directory (for writing) */
  errno = 0;
  ret = open (".", O_WRONLY);
  printf ("open 3: ret = %d, errno = %d %s\n", ret, errno,
	  strerrno (errno));
  if (ret >= 0)
    close (ret);
  stop ();
  /* Opening nonexistant file */
  errno = 0;
  ret = open (NONEXISTANT, O_RDONLY);
  printf ("open 4: ret = %d, errno = %d %s\n", ret, errno,
	  strerrno (errno));
  if (ret >= 0)
    close (ret);
  stop ();
  /* Open for write but no write permission */
  errno = 0;
  ret = open (NOWRITE, O_CREAT | O_RDONLY, S_IRUSR);
  if (ret >= 0)
    {
      close (ret);
      stop ();
      errno = 0;
      ret = open (NOWRITE, O_WRONLY);
      printf ("open 5: ret = %d, errno = %d %s\n", ret, errno,
	      strerrno (errno));
      if (ret >= 0)
	close (ret);
    }
  else
    {
      stop ();
      printf ("open 5: ret = %d, errno = %d\n", ret, errno);
    }
  stop ();
}

int
test_write ()
{
  int fd, ret;

  /* Test writing */
  errno = 0;
  fd = open (FILENAME, O_WRONLY);
  if (fd >= 0)
    {
      errno = 0;
      ret = write (fd, STRING, strlen (STRING));
      printf ("write 1: ret = %d, errno = %d %s\n", ret, errno,
              ret == strlen (STRING) ? "OK" : "");
      close (fd);
    }
  else
    printf ("write 1: ret = %d, errno = %d\n", ret, errno);
  stop ();
  /* Write using invalid file descriptor */
  errno = 0;
  ret = write (999, STRING, strlen (STRING));
  printf ("write 2: ret = %d, errno = %d, %s\n", ret, errno,
	  strerrno (errno));
  stop ();
  /* Write to a read-only file */
  errno = 0;
  fd = open (FILENAME, O_RDONLY);
  if (fd >= 0)
    {
      errno = 0;
      ret = write (fd, STRING, strlen (STRING));
      printf ("write 3: ret = %d, errno = %d %s\n", ret, errno,
	      strerrno (errno));
    }
  else
    printf ("write 3: ret = %d, errno = %d\n", ret, errno);
  stop ();
}

int
test_read ()
{
  int fd, ret;
  char buf[16];

  /* Test reading */
  errno = 0;
  fd = open (FILENAME, O_RDONLY);
  if (fd >= 0)
    {
      memset (buf, 0, 16);
      errno = 0;
      ret = read (fd, buf, 16);
      buf[15] = '\0'; /* Don't trust anybody... */
      if (ret == strlen (STRING))
        printf ("read 1: %s %s\n", buf, !strcmp (buf, STRING) ? "OK" : "");
      else
	printf ("read 1: ret = %d, errno = %d\n", ret, errno);
      close (fd);
    }
  else
    printf ("read 1: ret = %d, errno = %d\n", ret, errno);
  stop ();
  /* Read using invalid file descriptor */
  errno = 0;
  ret = read (999, buf, 16);
  printf ("read 2: ret = %d, errno = %d %s\n", ret, errno,
	  strerrno (errno));
  stop ();
}

int
test_lseek ()
{
  int fd;
  off_t ret = 0;

  /* Test seeking */
  errno = 0;
  fd = open (FILENAME, O_RDONLY);
  if (fd >= 0)
    {
      errno = 0;
      ret = lseek (fd, 0, SEEK_CUR);
      printf ("lseek 1: ret = %ld, errno = %d, %s\n", (long) ret, errno,
              ret == 0 ? "OK" : "");
      stop ();
      errno = 0;
      ret = lseek (fd, 0, SEEK_END);
      printf ("lseek 2: ret = %ld, errno = %d, %s\n", (long) ret, errno,
              ret == 11 ? "OK" : "");
      stop ();
      errno = 0;
      ret = lseek (fd, 3, SEEK_SET);
      printf ("lseek 3: ret = %ld, errno = %d, %s\n", (long) ret, errno,
              ret == 3 ? "OK" : "");
      close (fd);
    }
  else
    {
      printf ("lseek 1: ret = %d, errno = %d %s\n", ret, errno,
	      strerrno (errno));
      stop ();
      printf ("lseek 2: ret = %d, errno = %d %s\n", ret, errno,
	      strerrno (errno));
      stop ();
      printf ("lseek 3: ret = %d, errno = %d %s\n", ret, errno,
	      strerrno (errno));
    }
  /* Seeking on an invalid file descriptor */
  stop ();
}

int
test_close ()
{
  int fd, ret;

  /* Test close */
  errno = 0;
  fd = open (FILENAME, O_RDONLY);
  if (fd >= 0)
    {
      errno = 0;
      ret = close (fd);
      printf ("close 1: ret = %d, errno = %d, %s\n", ret, errno,
              ret == 0 ? "OK" : "");
    }
  else
    printf ("close 1: ret = %d, errno = %d\n", ret, errno);
  stop ();
  /* Close an invalid file descriptor */
  errno = 0;
  ret = close (999);
  printf ("close 2: ret = %d, errno = %d, %s\n", ret, errno,
  	  strerrno (errno));
  stop ();
}

int
test_stat ()
{
  int ret;
  struct stat st;

  /* Test stat */
  errno = 0;
  ret = stat (FILENAME, &st);
  if (!ret)
    printf ("stat 1: ret = %d, errno = %d %s\n", ret, errno,
	    st.st_size == 11 ? "OK" : "");
  else
    printf ("stat 1: ret = %d, errno = %d\n", ret, errno);
  stop ();
  /* NULL pathname */
  errno = 0;
  ret = stat (NULL, &st);
  printf ("stat 2: ret = %d, errno = %d %s\n", ret, errno,
  	  strerrno (errno));
  stop ();
  /* Empty pathname */
  errno = 0;
  ret = stat ("", &st);
  printf ("stat 3: ret = %d, errno = %d %s\n", ret, errno,
  	  strerrno (errno));
  stop ();
  /* Nonexistant file */
  errno = 0;
  ret = stat (NONEXISTANT, &st);
  printf ("stat 4: ret = %d, errno = %d %s\n", ret, errno,
  	  strerrno (errno));
  stop ();
}

int
test_fstat ()
{
  int fd, ret;
  struct stat st;

  /* Test fstat */
  errno = 0;
  fd = open (FILENAME, O_RDONLY);
  if (fd >= 0)
    {
      errno = 0;
      ret = fstat (fd, &st);
      if (!ret)
	printf ("fstat 1: ret = %d, errno = %d %s\n", ret, errno,
		st.st_size == 11 ? "OK" : "");
      else
	printf ("fstat 1: ret = %d, errno = %d\n", ret, errno);
      close (fd);
    }
  else
    printf ("fstat 1: ret = %d, errno = %d\n", ret, errno);
  stop ();
  /* Fstat using invalid file descriptor */
  errno = 0;
  ret = fstat (999, &st);
  printf ("fstat 2: ret = %d, errno = %d %s\n", ret, errno,
  	  strerrno (errno));
  stop ();
}

int
test_isatty ()
{
  int fd;

  /* Check std I/O */
  printf ("isatty 1: stdin %s\n", isatty (0) ? "yes OK" : "no");
  stop ();
  printf ("isatty 2: stdout %s\n", isatty (1) ? "yes OK" : "no");
  stop ();
  printf ("isatty 3: stderr %s\n", isatty (2) ? "yes OK" : "no");
  stop ();
  /* Check invalid fd */
  printf ("isatty 4: invalid %s\n", isatty (999) ? "yes" : "no OK");
  stop ();
  /* Check open file */
  fd = open (FILENAME, O_RDONLY);
  if (fd >= 0)
    {
      printf ("isatty 5: file %s\n", isatty (fd) ? "yes" : "no OK");
      close (fd);
    }
  else
    printf ("isatty 5: file couldn't open\n");
  stop ();
}


int
test_system ()
{
  /*
   * Requires test framework to switch on "set remote system-call-allowed 1"
   */
  int ret;
  char sys[512];

  /* Test for shell */
  ret = system (NULL);
  printf ("system 1: ret = %d %s\n", ret, ret != 0 ? "OK" : "");
  stop ();
  /* This test prepares the directory for test_rename() */
  sprintf (sys, "mkdir -p %s %s", TESTSUBDIR, TESTDIR2);
  ret = system (sys);
  if (ret == 127)
    printf ("system 2: ret = %d /bin/sh unavailable???\n", ret);
  else
    printf ("system 2: ret = %d %s\n", ret, ret == 0 ? "OK" : "");
  stop ();
  /* Invalid command (just guessing ;-) ) */
  ret = system ("wrtzlpfrmpft");
  printf ("system 3: ret = %d %s\n", ret, WEXITSTATUS (ret) == 127 ? "OK" : "");
  stop ();
}

int
test_rename ()
{
  int ret;
  struct stat st;

  /* Test rename */
  errno = 0;
  ret = rename (FILENAME, RENAMED);
  if (!ret)
    {
      errno = 0;
      ret = stat (FILENAME, &st);
      if (ret && errno == ENOENT)
        {
	  errno = 0;
	  ret = stat (RENAMED, &st);
	  printf ("rename 1: ret = %d, errno = %d %s\n", ret, errno,
		  strerrno (errno));
	  errno = 0;
	}
      else
	printf ("rename 1: ret = %d, errno = %d\n", ret, errno);
    }
  else
    printf ("rename 1: ret = %d, errno = %d\n", ret, errno);
  stop ();
  /* newpath is existing directory, oldpath is not a directory */
  errno = 0;
  ret = rename (RENAMED, TESTDIR2);
  printf ("rename 2: ret = %d, errno = %d %s\n", ret, errno,
	  strerrno (errno));
  stop ();
  /* newpath is a non-empty directory */
  errno = 0;
  ret = rename (TESTDIR2, TESTDIR1);
  printf ("rename 3: ret = %d, errno = %d %s\n", ret, errno,
          strerrno (errno));
  stop ();
  /* newpath is a subdirectory of old path */
  errno = 0;
  ret = rename (TESTDIR1, TESTSUBDIR);
  printf ("rename 4: ret = %d, errno = %d %s\n", ret, errno,
	  strerrno (errno));
  stop ();
  /* oldpath does not exist */
  errno = 0;
  ret = rename (NONEXISTANT, FILENAME);
  printf ("rename 5: ret = %d, errno = %d %s\n", ret, errno,
	  strerrno (errno));
  stop ();
}

int
test_unlink ()
{
  int ret;
  char name[256];
  char sys[512];

  /* Test unlink */
  errno = 0;
  ret = unlink (RENAMED);
  printf ("unlink 1: ret = %d, errno = %d %s\n", ret, errno,
	  strerrno (errno));
  stop ();
  /* No write access */
  sprintf (name, "%s/%s", TESTDIR2, FILENAME);
  errno = 0;
  ret = open (name, O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR);
  if (ret >= 0)
    {
      sprintf (sys, "chmod -w %s", TESTDIR2);
      ret = system (sys);
      if (!ret)
        {
	  errno = 0;
	  ret = unlink (name);
	  printf ("unlink 2: ret = %d, errno = %d %s\n", ret, errno,
		  strerrno (errno));
        }
      else
	printf ("unlink 2: ret = %d chmod failed\n", ret, errno);
    }
  else
    printf ("unlink 2: ret = %d, errno = %d\n", ret, errno);
  stop ();
  /* pathname doesn't exist */
  errno = 0;
  ret = unlink (NONEXISTANT);
  printf ("unlink 3: ret = %d, errno = %d %s\n", ret, errno,
          strerrno (errno));
  stop ();
}

int
test_time ()
{
  time_t ret, t;

  errno = 0;
  ret = time (&t);
  printf ("time 1: ret = %ld, errno = %d, t = %ld %s\n", (long) ret, errno, (long) t, ret == t ? "OK" : "");
  stop ();
  errno = 0;
  ret = time (NULL);
  printf ("time 2: ret = %ld, errno = %d, t = %ld %s\n",
	  (long) ret, errno, (long) t, ret >= t && ret < t + 10 ? "OK" : "");
  stop ();
}

static const char *
strerrno (int err)
{
  switch (err)
    {
    case 0: return "OK";
#ifdef EACCES
    case EACCES: return "EACCES";
#endif
#ifdef EBADF
    case EBADF: return "EBADF";
#endif
#ifdef EEXIST
    case EEXIST: return "EEXIST";
#endif
#ifdef EFAULT
    case EFAULT: return "EFAULT";
#endif
#ifdef EINVAL
    case EINVAL: return "EINVAL";
#endif
#ifdef EISDIR
    case EISDIR: return "EISDIR";
#endif
#ifdef ENOENT
    case ENOENT: return "ENOENT";
#endif
#ifdef ENOTEMPTY
    case ENOTEMPTY: return "ENOTEMPTY";
#endif
#ifdef EBUSY
    case EBUSY: return "EBUSY";
#endif
    default: return "E??";
    }
}

int
main ()
{
  /* Don't change the order of the calls.  They partly depend on each other */
  test_open ();
  test_write ();
  test_read ();
  test_lseek ();
  test_close ();
  test_stat ();
  test_fstat ();
  test_isatty ();
  test_system ();
  test_rename ();
  test_unlink ();
  test_time ();
  return 0;
}
