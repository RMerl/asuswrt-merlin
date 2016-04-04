#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <libnt.h>
#include <stdarg.h>

#define READ_BUF_SIZE	1024

#define FW_CREATE	0
#define FW_APPEND	1
#define FW_NEWLINE	2

void Debug2Console(const char * format, ...)
{
	FILE *f;
	int nfd;
	va_list args;

	if (((nfd = open("/dev/console", O_WRONLY | O_NONBLOCK)) > 0) &&
	    (f = fdopen(nfd, "w")))
	{
		va_start(args, format);
		vfprintf(f, format, args);
		va_end(args);
		fclose(f);
	}
	else
	{
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
	}

	if (nfd != -1) close(nfd);
}

int isFileExist(char *fname)
{
	struct stat fstat;
	
	if (lstat(fname,&fstat)==-1)
		return 0;
	if (S_ISREG(fstat.st_mode))
		return 1;
	
	return 0;
}

static void remove_delimitor(char *s)
{
	char *p1, *p2;
	
	p1 = p2 = s;
	while(*p1 != '\0' || *(p1 + 1) != '\0') {
		if(*p1 != '\0') {
			*p2 = *p1;
			p2++;
		}
		p1++;
	}
	*p2 = '\0';
}

int get_pid_num_by_name(char *pidName)
{
	DIR *dir;
	struct dirent *next;
	int match_cnt = 0;
	FILE *cmdline;
	char filename[READ_BUF_SIZE];
	char buffer[READ_BUF_SIZE];
	
	dir = opendir("/proc");
	if(!dir) {
		printf("Cannot open /proc\n");
		return 0;
	}
	
	while((next = readdir(dir)) != NULL) {
		memset(filename, 0, sizeof(filename));
		memset(buffer, 0, sizeof(buffer));
		
		if(strcmp(next->d_name, "..") == 0) {
			continue;
		}
		
		if(!isdigit(*next->d_name)) {
			continue;
		}
		
		sprintf(filename, "/proc/%s/cmdline", next->d_name);
		if(!(cmdline = fopen(filename, "r"))) {
			continue;
		}
		if(fgets(buffer, READ_BUF_SIZE - 1, cmdline) == NULL) {
			fclose(cmdline);
			continue;
		}
		fclose(cmdline);
		
		remove_delimitor(buffer);
		
		if(strstr(buffer, pidName) != NULL) {
			match_cnt++;
		}
	}
	closedir(dir);
	
	return match_cnt;
}

void StampToDate(unsigned long timestamp, char *date)
{
	struct tm *local;
	time_t now;
	
	now = timestamp;
	local = localtime(&now);
	strftime(date, 30, "%Y-%m-%d %H:%M:%S", local);
}

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
        printf("Error %d locking %s, proceeding anyway", errno, fn);
        return -1;
}

void file_unlock(int lockfd)
{
        if (lockfd >= 0) {
                ftruncate(lockfd, 0);
                close(lockfd);
        }
}

static int f_read(const char *path, void *buffer, int max)
{
	int f;
	int n;
	
	if ((f = open(path, O_RDONLY)) < 0) return -1;
	n = read(f, buffer, max);
	close(f);
	return n;
}

int f_read_string(const char *path, char *buffer, int max)
{
	if (max <= 0) return -1;
	int n = f_read(path, buffer, max - 1);
	buffer[(n > 0) ? n : 0] = 0;
	return n;
}

static int f_write(const char *path, const void *buffer, int len, unsigned flags, unsigned cmode)
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

int f_write_string(const char *path, const char *buffer, unsigned flags, unsigned cmode)
{
	return f_write(path, buffer, strlen(buffer), flags, cmode);
}

