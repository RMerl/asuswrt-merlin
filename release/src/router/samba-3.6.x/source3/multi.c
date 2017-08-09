#include <stdio.h>
#include <string.h>

extern int smbd_main(int argc, char **argv);
extern int nmbd_main(int argc, char **argv);
extern int smbpasswd_main(int argc, char **argv);

static struct {
	const char *name;
	int (*func)(int argc, char **argv);
} multicall[] = {
	{ "smbd", smbd_main },
	{ "nmbd", nmbd_main },
	{ "smbpasswd", smbpasswd_main },
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

int main(int argc, char **argv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(multicall); i++) {
		if (strstr(argv[0], multicall[i].name))
			return multicall[i].func(argc, argv);
	}

	fprintf(stderr, "Invalid multicall command, available commands:");
	for (i = 0; i < ARRAY_SIZE(multicall); i++)
		fprintf(stderr, " %s", multicall[i].name);

	fprintf(stderr, "\n");

	return 1;
}
