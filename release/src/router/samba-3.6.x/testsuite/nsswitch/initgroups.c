#include <stdio.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>

int main(int argc, char **argv)
{
	int result, ngroups, i;
	gid_t *groups;
	struct passwd *pw;

	if (!(pw = getpwnam(argv[1]))) {
		printf("FAIL: no passwd entry for %s\n", argv[1]);
		return 1;
	}

	result = initgroups(argv[1], pw->pw_gid);

	if (result == -1) {
		printf("FAIL");
		return 1;
	}

	ngroups = getgroups(0, NULL);

	groups = (gid_t *)malloc(sizeof(gid_t) * ngroups);
	ngroups = getgroups(ngroups, groups);

	printf("%s is a member of groups:\n", argv[1]);

	for (i = 0; i < ngroups; i++) {
		struct group *grp;

		grp = getgrgid(groups[i]);

		printf("%d (%s)\n", groups[i], grp ? grp->gr_name : "?");
	}

	printf("PASS\n");
	return 0;
}
