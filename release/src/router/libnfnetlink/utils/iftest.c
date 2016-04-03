/* simple test for index to interface name API */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <net/if.h>

#include <libnfnetlink/libnfnetlink.h>

int main(int argc, char *argv[])
{
	int idx;
	struct nlif_handle *h;
	char name[IFNAMSIZ];
	unsigned int flags;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <interface>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	h = nlif_open();
	if (h == NULL) {
		perror("nlif_open");
		exit(EXIT_FAILURE);
	}

	if (nlif_query(h) == -1) {
		fprintf(stderr, "failed query to retrieve interfaces: %s\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	idx = if_nametoindex(argv[1]);

	/* Superfluous: just to make sure nlif_index2name is working fine */
	if (nlif_index2name(h, idx, name) == -1)
		fprintf(stderr, "Cannot translate device idx=%u\n", idx);

	if (nlif_get_ifflags(h, idx, &flags) == -1) {
		fprintf(stderr, "Cannot get flags for device `%s'\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	printf("index (%d) is %s (%s) (%s)\n", idx, argv[1],
		flags & IFF_RUNNING ? "RUNNING" : "NOT RUNNING",
		flags & IFF_UP ? "UP" : "DOWN");

	nlif_close(h);
	return EXIT_SUCCESS;
}
