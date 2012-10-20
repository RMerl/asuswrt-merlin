/**************************************
 ecmh - Easy Cast du Multi Hub
 by Jeroen Massar <jeroen@unfix.org>
***************************************
 $Author: fuzzel $
 $Id: groups.h,v 1.4 2005/02/09 17:58:06 fuzzel Exp $
 $Date: 2005/02/09 17:58:06 $
**************************************/

/* The node used to hold the groups we joined */
struct groupnode
{
	struct in6_addr	mca;		/* The Multicast IPv6 address (group) */
	struct list	*interfaces;	/* The list of grpint nodes (interfaces) that */
					/* are interrested in this node */
	time_t		lastforward;	/* The last time we forwarded a report for this group */
	uint64_t	bytes;		/* Number of received bytes */
	uint64_t	packets;	/* Number of received packets */
};

struct groupnode *group_create(const struct in6_addr *mca);
void group_destroy(struct groupnode *groupn);
struct groupnode *group_find(const struct in6_addr *mca);
struct grpintnode *groupint_get(const struct in6_addr *mca, struct intnode *interface, bool *isnew);
