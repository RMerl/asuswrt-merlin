#include <stdio.h>
#include <sys/utsname.h>

#include "linux_version.h"

int get_linux_version (void)
{
	static int kver = -1;
	struct utsname uts;
	int major = 0;
	int minor = 0;
	int teeny = 0;
	int n;

	if (kver != -1)
		return kver;
	if (uname (&uts))
		return kver = 0;

	n = sscanf(uts.release, "%d.%d.%d", &major, &minor, &teeny);
	if (n < 1 || n > 3)
		return kver = 0;

	return kver = KERNEL_VERSION(major, minor, teeny);
}
