/*
 * fsck.h
 */

#include <time.h>

#ifdef __STDC__
#define NOARGS void
#else
#define NOARGS
#endif

#ifdef __GNUC__
#define FSCK_ATTR(x) __attribute__(x)
#else
#define FSCK_ATTR(x)
#endif


#ifndef DEFAULT_FSTYPE
#define DEFAULT_FSTYPE	"ext2"
#endif

#define MAX_DEVICES 32
#define MAX_ARGS 32

#define EXIT_OK          0
#define EXIT_NONDESTRUCT 1
#define EXIT_DESTRUCT    2
#define EXIT_UNCORRECTED 4
#define EXIT_ERROR       8
#define EXIT_USAGE       16
#define EXIT_LIBRARY     128

/*
 * Internal structure for mount tabel entries.
 */

struct fs_info {
	char  *device;
	char  *mountpt;
	char  *type;
	char  *opts;
	int   freq;
	int   passno;
	int   flags;
	dev_t disk;
	int   stacked;
	struct fs_info *next;
};

#define FLAG_DONE 1
#define FLAG_PROGRESS 2

/*
 * Structure to allow exit codes to be stored
 */
struct fsck_instance {
	int	pid;
	int	flags;
	int	lock;		/* flock()ed whole disk file descriptor or -1 */
	int	exit_status;
	time_t	start_time;
	char *	prog;
	char *	type;
	struct fs_info *fs;
	struct fsck_instance *next;
};

extern char *base_device(const char *device);
extern const char *identify_fs(const char *fs_name, const char *fs_types);

/* ismounted.h */
extern int is_mounted(const char *file);
