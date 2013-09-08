/*

	Tomato Firmware
	Copyright (C) 2006-2009 Jonathan Zarate

*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <dirent.h>
#include <bcmnvram.h>
#include <syslog.h>
#include "shutils.h"
#include "shared.h"


int f_exists(const char *path)	// note: anything but a directory
{
	struct stat st;
	return (stat(path, &st) == 0) && (!S_ISDIR(st.st_mode));
}

int d_exists(const char *path)	//  directory only
{
	struct stat st;
	return (stat(path, &st) == 0) && (S_ISDIR(st.st_mode));
}

unsigned long f_size(const char *path)	// 4GB-1	-1 = error
{
	struct stat st;
	if (stat(path, &st) == 0) return st.st_size;
	return (unsigned long)-1;
}

int f_read(const char *path, void *buffer, int max)
{
	int f;
	int n;

	if ((f = open(path, O_RDONLY)) < 0) return -1;
	n = read(f, buffer, max);
	close(f);
	return n;
}

int f_write(const char *path, const void *buffer, int len, unsigned flags, unsigned cmode)
{
	static const char nl = '\n';
	int f;
	int r = -1;
	mode_t m;

	m = umask(0);
	if (cmode == 0) cmode = 0666;
	if ((f = open(path, (flags & FW_APPEND) ? (O_WRONLY|O_CREAT|O_APPEND) : (O_WRONLY|O_CREAT|O_TRUNC), cmode)) >= 0) {
		if ((buffer == NULL) || ((r = write(f, buffer, len)) == len)) {
			if (flags & FW_NEWLINE) {
				if (write(f, &nl, 1) == 1) ++r;
			}
		}
		close(f);
	}
	umask(m);
	return r;
}

int f_read_string(const char *path, char *buffer, int max)
{
	if (max <= 0) return -1;
	int n = f_read(path, buffer, max - 1);
	buffer[(n > 0) ? n : 0] = 0;
	return n;
}

int f_write_string(const char *path, const char *buffer, unsigned flags, unsigned cmode)
{
	return f_write(path, buffer, strlen(buffer), flags, cmode);
}

static int _f_read_alloc(const char *path, char **buffer, int max, int z)
{
	unsigned long n;

	*buffer = NULL;
	if (max >= 0) {
		if ((n = f_size(path)) != (unsigned long)-1) {
			if (n < max) max = n;
			if ((!z) && (max == 0)) return 0;
			if ((*buffer = malloc(max + z)) != NULL) {
				if ((max = f_read(path, *buffer, max)) >= 0) {
					if (z) *(*buffer + max) = 0;
					return max;
				}
				free(buffer);
			}
		}
	}
	return -1;
}

int f_read_alloc(const char *path, char **buffer, int max)
{
	return _f_read_alloc(path, buffer, max, 0);
}

int f_read_alloc_string(const char *path, char **buffer, int max)
{
	return _f_read_alloc(path, buffer, max, 1);
}

static int _f_wait_exists(const char *name, int max, int invert)
{
	while (max-- > 0) {
		if (f_exists(name) ^ invert) return 1;
		sleep(1);
	}
	return 0;
}

int f_wait_exists(const char *name, int max)
{
	return _f_wait_exists(name, max, 0);
}

int f_wait_notexists(const char *name, int max)
{
	return _f_wait_exists(name, max, 1);
}

int
check_if_dir_exist(const char *dirpath)
{
	return d_exists(dirpath);
}

int 
check_if_dir_empty(const char *dirpath)
{
	DIR *dir;
	struct dirent *dirent;
	int found=0;

	if((dir=opendir(dirpath))!=NULL) {
		while ((dirent=readdir(dir))!=NULL) {
			if(strcmp(dirent->d_name, ".") && strcmp(dirent->d_name, "..")) { 
				found=1;
				break;
			}
			
		}
		closedir(dir);
	}
	return found;
}

int
check_if_file_exist(const char *filepath)
{
/* Note: f_exists() checks not S_ISREG, but !S_ISDIR
	struct stat st;
	return (stat(path, &st) == 0) && (S_ISREG(st.st_mode));
*/
	return f_exists(filepath);
}

/* Test whether we can write to a directory.
 * @return:
 * 0		not writable
 * -1		invalid parameter
 * otherwise	writable
 */
int check_if_dir_writable(const char *dir)
{
	char tmp[PATH_MAX];
	FILE *fp;
	int ret = 0;

	if (!dir || *dir == '\0')
		return -1;

	sprintf(tmp, "%s/.test_dir_writable", dir);
	if ((fp = fopen(tmp, "w")) != NULL) {
		fclose(fp);
		unlink(tmp);
		ret = 1;
	}

	return ret;
}


/* Serialize using fcntl() calls 
 */

int file_lock(char *tag)
{
        char fn[64];
        struct flock lock;
        int lockfd = -1;
        pid_t lockpid;

        sprintf(fn, "/var/lock/%s.lock", tag);
        if ((lockfd = open(fn, O_CREAT | O_RDWR, 0666)) < 0)
                goto lock_error;

        pid_t pid = getpid();
        if (read(lockfd, &lockpid, sizeof(pid_t))) {
                // check if we already hold a lock
                if (pid == lockpid) {
                        // don't close the file here as that will release all locks
                        return -1;
                }
        }

        memset(&lock, 0, sizeof(lock));
        lock.l_type = F_WRLCK;
        lock.l_pid = pid;

        if (fcntl(lockfd, F_SETLKW, &lock) < 0) {
                close(lockfd);
                goto lock_error;
        }

        lseek(lockfd, 0, SEEK_SET);
        write(lockfd, &pid, sizeof(pid_t));
        return lockfd;
lock_error:
        // No proper error processing
        syslog(LOG_DEBUG, "Error %d locking %s, proceeding anyway", errno, fn);
        return -1;
}

void file_unlock(int lockfd)
{
        if (lockfd >= 0) {
                ftruncate(lockfd, 0);
                close(lockfd);
        }
}
