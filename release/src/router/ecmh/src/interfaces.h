/**************************************
 ecmh - Easy Cast du Multi Hub
 by Jeroen Massar <jeroen@unfix.org>
***************************************
 $Author: fuzzel $
 $Id: interfaces.h,v 1.8 2005/02/09 17:58:06 fuzzel Exp $
 $Date: 2005/02/09 17:58:06 $
**************************************/

#define INTNODE_MAXIPV4 4			/* Maximum number of IPv4 aliases */

/*
 * The list of interfaces we do multicast on
 * These are discovered on the fly, very handy ;)
 */
struct intnode
{
	unsigned int	ifindex;		/* The ifindex */
	char		name[IFNAMSIZ];		/* Name of the interface */
	unsigned int	groupcount;		/* Number of groups this interface joined */
	unsigned int	mtu;			/* The MTU of this interface (mtu = 0 -> invalid interface) */

	unsigned int	mld_version;		/* The MLD version this interface supports */
	time_t		mld_last_v1;		/* The last v1 we have seen -> allows upgrade to v2 */

#ifndef ECMH_BPF
	struct sockaddr	hwaddr;			/* Hardware bytes */
#else
	int		socket;			/* (BPF|Raw)Socket, when this is an ethernet interface */
	unsigned int	dlt;			/* DLT of the interface (DLT_EN10MB or DLT_NULL)*/
	unsigned int	bufferlen;		/* The buffer length this interface expects */
	struct intnode	*master;		/* Master interface, when this is a proto-41 tunnel */
	struct in_addr	ipv4_local[INTNODE_MAXIPV4]; /* Local IPv4 address */
	struct in_addr	ipv4_remote;		/* Remote IPv4 address */
#endif

	struct in6_addr	linklocal;		/* Link local address */
	struct in6_addr	global;			/* Global unicast address */

	/* Per interface statistics */
	uint64_t	stat_packets_received;	/* Number of packets received */
	uint64_t	stat_packets_sent;	/* Number of packets sent */
	uint64_t	stat_bytes_received;	/* Number of bytes received */
	uint64_t	stat_bytes_sent;	/* Number of bytes sent */
	uint64_t	stat_icmp_received;	/* Number of ICMP's received */
	uint64_t	stat_icmp_sent;		/* Number of ICMP's sent */

	bool		upstream;		/* This interface is an upstream */
};

/* Node functions */
#ifndef ECMH_BPF
struct intnode *int_create(unsigned int ifindex);
#else
struct intnode *int_create(unsigned int ifindex, bool tunnel);
#endif
void int_destroy(struct intnode *intn);

/* List functions */
struct intnode *int_find(unsigned int ifindex);
#ifdef ECMH_BPF
struct intnode *int_find_ipv4(bool local, struct in_addr *ipv4);
#endif

/* Control function */
void int_set_mld_version(struct intnode *intn, unsigned int newversion);

#ifdef ECMH_BPF
/*
 * This node is used to quickly index local IPv4 addresses
 * and allow them to be found for the tunnels too when
 * sending packets
 */
struct localnode
{
	struct intnode *intn;		/* The interface */
};

void local_update(struct intnode *intn);
struct localnode *local_find(struct in_addr *ipv4);
void local_remove(struct intnode *intn);
void local_destroy(struct localnode *localn);

#endif

