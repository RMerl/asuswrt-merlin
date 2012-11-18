#ifndef _LIBIPT_SET_H
#define _LIBIPT_SET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

static int get_set_getsockopt(void *data, size_t * size)
{
	int sockfd = -1;
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (sockfd < 0)
		exit_error(OTHER_PROBLEM,
			   "Can't open socket to ipset.\n");
	/* Send! */
	return getsockopt(sockfd, SOL_IP, SO_IP_SET, data, size);
}

static void
parse_bindings(const char *optarg, struct ipt_set_info *info)
{
	char *saved = strdup(optarg);
	char *ptr, *tmp = saved;
	int i = 0;
	
	while (i < IP_SET_MAX_BINDINGS && tmp != NULL) {
		ptr = strsep(&tmp, ",");
		if (strncmp(ptr, "src", 3) == 0)
			info->flags[i++] |= IPSET_SRC;
		else if (strncmp(ptr, "dst", 3) == 0)
			info->flags[i++] |= IPSET_DST;
		else
			exit_error(PARAMETER_PROBLEM,
				   "You must spefify (the comma separated list of) 'src' or 'dst'.");
	}

	if (tmp)
		exit_error(PARAMETER_PROBLEM,
			   "Can't follow bindings deeper than %d.", 
			   IP_SET_MAX_BINDINGS);

	free(saved);
}

#endif /*_LIBIPT_SET_H*/
