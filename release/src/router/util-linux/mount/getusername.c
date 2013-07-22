#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include "getusername.h"

char *
getusername() {
	char *user = 0;
	struct passwd *pw = getpwuid(getuid());

	if (pw)
		user = pw->pw_name;
	return user;
}
