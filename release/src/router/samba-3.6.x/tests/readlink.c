/* test whether readlink returns a short buffer correctly. */

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DATA "readlink.test"
#define FNAME "rdlnk.file"

main()
{
	char buf[7];
	int ret;
	ssize_t rl_ret;

	unlink(FNAME);
	ret = symlink(DATA, FNAME);
	if (ret == -1) {
		exit(1);
	}

	rl_ret = readlink(FNAME, buf, sizeof(buf));
	if (rl_ret == -1) {
		unlink(FNAME);
		exit(1);
	}
	unlink(FNAME);
	exit(0);
}
