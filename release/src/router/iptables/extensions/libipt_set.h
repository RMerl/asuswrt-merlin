#ifndef _LIBIPT_SET_H
#define _LIBIPT_SET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#ifdef DEBUG
#define DEBUGP(x, args...) fprintf(stderr, x, ## args)
#else
#define DEBUGP(x, args...) 
#endif

static void
parse_bindings(const char *optarg, struct ipt_set_info *info)
{
	char *saved = strdup(optarg);
	char *ptr, *tmp = saved;
	int i = 0;
	
	while (i < (IP_SET_MAX_BINDINGS - 1) && tmp != NULL) {
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
			   "Can't follow bindings deeper than %i.", 
			   IP_SET_MAX_BINDINGS - 1);

	free(saved);
}

static int get_set_getsockopt(void *data, socklen_t * size)
{
	int sockfd = -1;
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (sockfd < 0)
		exit_error(OTHER_PROBLEM,
			   "Can't open socket to ipset.\n");
	/* Send! */
	return getsockopt(sockfd, SOL_IP, SO_IP_SET, data, size);
}

static void get_set_byname(const char *setname, struct ipt_set_info *info)
{
	struct ip_set_req_get_set req;
	socklen_t size = sizeof(struct ip_set_req_get_set);
	int res;

	req.op = IP_SET_OP_GET_BYNAME;
	req.version = IP_SET_PROTOCOL_VERSION;
	strncpy(req.set.name, setname, IP_SET_MAXNAMELEN);
	req.set.name[IP_SET_MAXNAMELEN - 1] = '\0';
	res = get_set_getsockopt(&req, &size);
	if (res != 0)
		exit_error(OTHER_PROBLEM,
			   "Problem when communicating with ipset, errno=%d.\n",
			   errno);
	if (size != sizeof(struct ip_set_req_get_set))
		exit_error(OTHER_PROBLEM,
			   "Incorrect return size from kernel during ipset lookup, "
			   "(want %ld, got %ld)\n",
			   sizeof(struct ip_set_req_get_set), size);
	if (req.set.index == IP_SET_INVALID_ID)
		exit_error(PARAMETER_PROBLEM,
			   "Set %s doesn't exist.\n", setname);

	info->index = req.set.index;
}

static void get_set_byid(char * setname, ip_set_id_t index)
{
	struct ip_set_req_get_set req;
	socklen_t size = sizeof(struct ip_set_req_get_set);
	int res;

	req.op = IP_SET_OP_GET_BYINDEX;
	req.version = IP_SET_PROTOCOL_VERSION;
	req.set.index = index;
	res = get_set_getsockopt(&req, &size);
	if (res != 0)
		exit_error(OTHER_PROBLEM,
			   "Problem when communicating with ipset, errno=%d.\n",
			   errno);
	if (size != sizeof(struct ip_set_req_get_set))
		exit_error(OTHER_PROBLEM,
			   "Incorrect return size from kernel during ipset lookup, "
			   "(want %ld, got %ld)\n",
			   sizeof(struct ip_set_req_get_set), size);
	if (req.set.name[0] == '\0')
		exit_error(PARAMETER_PROBLEM,
			   "Set id %i in kernel doesn't exist.\n", index);

	strncpy(setname, req.set.name, IP_SET_MAXNAMELEN);
}

#endif /*_LIBIPT_SET_H*/
