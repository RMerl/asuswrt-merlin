#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "utils.h"

static __inline__ u_int16_t dn_ntohs(u_int16_t addr)
{
	union {
		u_int8_t byte[2];
		u_int16_t word;
	} u;

	u.word = addr;
	return ((u_int16_t)u.byte[0]) | (((u_int16_t)u.byte[1]) << 8);
}

static __inline__ int do_digit(char *str, u_int16_t *addr, u_int16_t scale, size_t *pos, size_t len, int *started)
{
	u_int16_t tmp = *addr / scale;

	if (*pos == len)
		return 1;

	if (((tmp) > 0) || *started || (scale == 1)) {
		*str = tmp + '0';
		*started = 1;
		(*pos)++;
		*addr -= (tmp * scale);
	}

	return 0;
}


static const char *dnet_ntop1(const struct dn_naddr *dna, char *str, size_t len)
{
	u_int16_t addr, area;
	size_t pos = 0;
	int started = 0;

	memcpy(&addr, dna->a_addr, sizeof(addr));
	addr = dn_ntohs(addr);
	area = addr >> 10;

	if (dna->a_len != 2)
		return NULL;

	addr &= 0x03ff;

	if (len == 0)
		return str;

	if (do_digit(str + pos, &area, 10, &pos, len, &started))
		return str;

	if (do_digit(str + pos, &area, 1, &pos, len, &started))
		return str;

	if (pos == len)
		return str;

	*(str + pos) = '.';
	pos++;
	started = 0;

	if (do_digit(str + pos, &addr, 1000, &pos, len, &started))
		return str;

	if (do_digit(str + pos, &addr, 100, &pos, len, &started))
		return str;

	if (do_digit(str + pos, &addr, 10, &pos, len, &started))
		return str;

	if (do_digit(str + pos, &addr, 1, &pos, len, &started))
		return str;

	if (pos == len)
		return str;

	*(str + pos) = 0;

	return str;
}


const char *dnet_ntop(int af, const void *addr, char *str, size_t len)
{
	switch(af) {
		case AF_DECnet:
			errno = 0;
			return dnet_ntop1((struct dn_naddr *)addr, str, len);
		default:
			errno = EAFNOSUPPORT;
	}

	return NULL;
}


