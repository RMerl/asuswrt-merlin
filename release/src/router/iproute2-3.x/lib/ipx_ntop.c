#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "utils.h"

static __inline__ int do_digit(char *str, u_int32_t addr, u_int32_t scale, size_t *pos, size_t len)
{
	u_int32_t tmp = addr >> (scale * 4);

	if (*pos == len)
		return 1;

	tmp &= 0x0f;
	if (tmp > 9)
		*str = tmp + 'A' - 10;
	else
		*str = tmp + '0';
	(*pos)++;

	return 0;
}

static const char *ipx_ntop1(const struct ipx_addr *addr, char *str, size_t len)
{
	int i;
	size_t pos = 0;

	if (len == 0)
		return str;

	for(i = 7; i >= 0; i--)
		if (do_digit(str + pos, ntohl(addr->ipx_net), i, &pos, len))
			return str;

	if (pos == len)
		return str;

	*(str + pos) = '.';
	pos++;

	for(i = 0; i < 6; i++) {
		if (do_digit(str + pos, addr->ipx_node[i], 1, &pos, len))
			return str;
		if (do_digit(str + pos, addr->ipx_node[i], 0, &pos, len))
			return str;
	}

	if (pos == len)
		return str;

	*(str + pos) = 0;

	return str;
}


const char *ipx_ntop(int af, const void *addr, char *str, size_t len)
{
	switch(af) {
		case AF_IPX:
			errno = 0;
			return ipx_ntop1((struct ipx_addr *)addr, str, len);
		default:
			errno = EAFNOSUPPORT;
	}

	return NULL;
}


