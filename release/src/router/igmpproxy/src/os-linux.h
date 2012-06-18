#define _LINUX_IN_H
#include <linux/types.h>
#include <linux/mroute.h>
#include <netinet/ip.h>
#include <netinet/igmp.h>

#define IGMP_V3_MEMBERSHIP_REPORT 0x22

#define INADDR_ALLIGMPV3_GROUP ((in_addr_t) 0xe0000016)

static inline u_short ip_data_len(const struct ip *ip)
{
	return ntohs(ip->ip_len) - (ip->ip_hl << 2);
}

static inline void ip_set_len(struct ip *ip, u_short len)
{
	ip->ip_len = htons(len);
}
