#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "utils.h"

static u_int32_t hexget(char c)
{
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= '0' && c <= '9')
		return c - '0';

	return 0xf0;
}

static int ipx_getnet(u_int32_t *net, const char *str)
{
	int i;
	u_int32_t tmp;

	for(i = 0; *str && (i < 8); i++) {

		if ((tmp = hexget(*str)) & 0xf0) {
			if (*str == '.')
				return 0;
			else
				return -1;
		}

		str++;
		(*net) <<= 4;
		(*net) |= tmp;
	}

	if (*str == 0)
		return 0;

	return -1;
}

static int ipx_getnode(u_int8_t *node, const char *str)
{
	int i;
	u_int32_t tmp;

	for(i = 0; i < 6; i++) {
		if ((tmp = hexget(*str++)) & 0xf0)
			return -1;
		node[i] = (u_int8_t)tmp;
		node[i] <<= 4;
		if ((tmp = hexget(*str++)) & 0xf0)
			return -1;
		node[i] |= (u_int8_t)tmp;
		if (*str == ':')
			str++;
	}

	return 0;
}

static int ipx_pton1(const char *src, struct ipx_addr *addr)
{
	char *sep = (char *)src;
	int no_node = 0;

	memset(addr, 0, sizeof(struct ipx_addr));

	while(*sep && (*sep != '.'))
		sep++;

	if (*sep != '.')
		no_node = 1;

	if (ipx_getnet(&addr->ipx_net, src))
		return 0;

	addr->ipx_net = htonl(addr->ipx_net);

	if (no_node)
		return 1;

	if (ipx_getnode(addr->ipx_node, sep + 1))
		return 0;

	return 1;
}

int ipx_pton(int af, const char *src, void *addr)
{
	int err;

	switch (af) {
	case AF_IPX:
		errno = 0;
		err = ipx_pton1(src, (struct ipx_addr *)addr);
		break;
	default:
		errno = EAFNOSUPPORT;
		err = -1;
	}

	return err;
}
