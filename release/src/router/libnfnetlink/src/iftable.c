/* iftable - table of network interfaces
 *
 * (C) 2004 by Astaro AG, written by Harald Welte <hwelte@astaro.com>
 * (C) 2008 by Pablo Neira Ayuso <pablo@netfilter.org>
 *
 * This software is Free Software and licensed under GNU GPLv2. 
 */

/* IFINDEX handling */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>

#include <linux/netdevice.h>

#include <libnfnetlink/libnfnetlink.h>
#include "rtnl.h"
#include "linux_list.h"

struct ifindex_node {
	struct list_head head;

	u_int32_t	index;
	u_int32_t	type;
	u_int32_t	alen;
	u_int32_t	flags;
	char		addr[8];
	char		name[16];
};

struct nlif_handle {
	struct list_head ifindex_hash[16];
	struct rtnl_handle *rtnl_handle;
	struct rtnl_handler ifadd_handler;
	struct rtnl_handler ifdel_handler;
};

/* iftable_add - Add/Update an entry to/in the interface table
 * @n:		netlink message header of a RTM_NEWLINK message
 * @arg:	not used
 *
 * This function adds/updates an entry in the intrface table.
 * Returns -1 on error, 1 on success.
 */
static int iftable_add(struct nlmsghdr *n, void *arg)
{
	unsigned int hash, found = 0;
	struct ifinfomsg *ifi_msg = NLMSG_DATA(n);
	struct ifindex_node *this;
	struct rtattr *cb[IFLA_MAX+1];
	struct nlif_handle *h = arg;

	if (n->nlmsg_type != RTM_NEWLINK)
		return -1;

	if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifi_msg)))
		return -1;

	rtnl_parse_rtattr(cb, IFLA_MAX, IFLA_RTA(ifi_msg), IFLA_PAYLOAD(n));

	if (!cb[IFLA_IFNAME])
		return -1;

	hash = ifi_msg->ifi_index & 0xF;
	list_for_each_entry(this, &h->ifindex_hash[hash], head) {
		if (this->index == ifi_msg->ifi_index) {
			found = 1;
			break;
		}
	}

	if (!found) {
		this = malloc(sizeof(*this));
		if (!this)
			return -1;

		this->index = ifi_msg->ifi_index;
	}

	this->type = ifi_msg->ifi_type;
	this->flags = ifi_msg->ifi_flags;
	if (cb[IFLA_ADDRESS]) {
		unsigned int alen;
		this->alen = alen = RTA_PAYLOAD(cb[IFLA_ADDRESS]);
		if (alen > sizeof(this->addr))
			alen = sizeof(this->addr);
		memcpy(this->addr, RTA_DATA(cb[IFLA_ADDRESS]), alen);
	} else {
		this->alen = 0;
		memset(this->addr, 0, sizeof(this->addr));
	}
	strcpy(this->name, RTA_DATA(cb[IFLA_IFNAME]));

	if (!found)
		list_add(&this->head, &h->ifindex_hash[hash]);

	return 1;
}

/* iftable_del - Delete an entry from the interface table
 * @n:		netlink message header of a RTM_DELLINK nlmsg
 * @arg:	not used
 *
 * Delete an entry from the interface table.  
 * Returns -1 on error, 0 if no matching entry was found or 1 on success.
 */
static int iftable_del(struct nlmsghdr *n, void *arg)
{
	struct ifinfomsg *ifi_msg = NLMSG_DATA(n);
	struct rtattr *cb[IFLA_MAX+1];
	struct nlif_handle *h = arg;
	struct ifindex_node *this, *tmp;
	unsigned int hash;

	if (n->nlmsg_type != RTM_DELLINK)
		return -1;

	if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifi_msg)))
		return -1;

	rtnl_parse_rtattr(cb, IFLA_MAX, IFLA_RTA(ifi_msg), IFLA_PAYLOAD(n));

	hash = ifi_msg->ifi_index & 0xF;
	list_for_each_entry_safe(this, tmp, &h->ifindex_hash[hash], head) {
		if (this->index == ifi_msg->ifi_index) {
			list_del(&this->head);
			free(this);
			return 1;
		}
	}

	return 0;
}

/** Get the name for an ifindex
 *
 * \param nlif_handle A pointer to a ::nlif_handle created
 * \param index ifindex to be resolved
 * \param name interface name, pass a buffer of IFNAMSIZ size
 * \return -1 on error, 1 on success 
 */
int nlif_index2name(struct nlif_handle *h, 
		    unsigned int index,
		    char *name)
{
	unsigned int hash;
	struct ifindex_node *this;

	assert(h != NULL);
	assert(name != NULL);

	if (index == 0) {
		strcpy(name, "*");
		return 1;
	}

	hash = index & 0xF;
	list_for_each_entry(this, &h->ifindex_hash[hash], head) {
		if (this->index == index) {
			strcpy(name, this->name);
			return 1;
		}
	}

	errno = ENOENT;
	return -1;
}

/** Get the flags for an ifindex
 *
 * \param nlif_handle A pointer to a ::nlif_handle created
 * \param index ifindex to be resolved
 * \param flags pointer to variable used to store the interface flags
 * \return -1 on error, 1 on success 
 */
int nlif_get_ifflags(const struct nlif_handle *h,
		     unsigned int index,
		     unsigned int *flags)
{
	unsigned int hash;
	struct ifindex_node *this;

	assert(h != NULL);
	assert(flags != NULL);

	if (index == 0) {
		errno = ENOENT;
		return -1;
	}

	hash = index & 0xF;
	list_for_each_entry(this, &h->ifindex_hash[hash], head) {
		if (this->index == index) {
			*flags = this->flags;
			return 1;
		}
	}
	errno = ENOENT;
	return -1;
}

/** Initialize interface table
 *
 * Initialize rtnl interface and interface table
 * Call this before any nlif_* function
 *
 * \return file descriptor to netlink socket
 */
struct nlif_handle *nlif_open(void)
{
	int i;
	struct nlif_handle *h;

	h = calloc(1,  sizeof(struct nlif_handle));
	if (h == NULL)
		goto err;

	for (i=0; i<16; i++)
		INIT_LIST_HEAD(&h->ifindex_hash[i]);

	h->ifadd_handler.nlmsg_type = RTM_NEWLINK;
	h->ifadd_handler.handlefn = iftable_add;
	h->ifadd_handler.arg = h;
	h->ifdel_handler.nlmsg_type = RTM_DELLINK;
	h->ifdel_handler.handlefn = iftable_del;
	h->ifdel_handler.arg = h;

	h->rtnl_handle = rtnl_open();
	if (h->rtnl_handle == NULL)
		goto err;

	if (rtnl_handler_register(h->rtnl_handle, &h->ifadd_handler) < 0)
		goto err_close;

	if (rtnl_handler_register(h->rtnl_handle, &h->ifdel_handler) < 0)
		goto err_unregister;

	return h;

err_unregister:
	rtnl_handler_unregister(h->rtnl_handle, &h->ifadd_handler);
err_close:
	rtnl_close(h->rtnl_handle);
	free(h);
err:
	return NULL;
}

/** Destructor of interface table
 *
 * \param nlif_handle A pointer to a ::nlif_handle created 
 * via nlif_open()
 */
void nlif_close(struct nlif_handle *h)
{
	int i;
	struct ifindex_node *this, *tmp;

	assert(h != NULL);

	rtnl_handler_unregister(h->rtnl_handle, &h->ifadd_handler);
	rtnl_handler_unregister(h->rtnl_handle, &h->ifdel_handler);
	rtnl_close(h->rtnl_handle);

	for (i=0; i<16; i++) {
		list_for_each_entry_safe(this, tmp, &h->ifindex_hash[i], head) {
			list_del(&this->head);
			free(this);
		}
	}

	free(h);
	h = NULL; /* bugtrap */
}

/** Receive message from netlink and update interface table
 *
 * \param nlif_handle A pointer to a ::nlif_handle created
 * \return 0 if OK
 */
int nlif_catch(struct nlif_handle *h)
{
	assert(h != NULL);

	if (h->rtnl_handle)
		return rtnl_receive(h->rtnl_handle);

	return -1;
}

static int nlif_catch_multi(struct nlif_handle *h)
{
	assert(h != NULL);

	if (h->rtnl_handle)
		return rtnl_receive_multi(h->rtnl_handle);

	return -1;
}

/** 
 * nlif_query - request a dump of interfaces available in the system
 * @h: pointer to a valid nlif_handler
 */
int nlif_query(struct nlif_handle *h)
{
	assert(h != NULL);

	if (rtnl_dump_type(h->rtnl_handle, RTM_GETLINK) < 0)
		return -1;

	return nlif_catch_multi(h);
}

/** Returns socket descriptor for the netlink socket
 *
 * \param nlif_handle A pointer to a ::nlif_handle created
 * \return The fd or -1 if there's an error
 */
int nlif_fd(struct nlif_handle *h)
{
	assert(h != NULL);

	if (h->rtnl_handle)
		return h->rtnl_handle->rtnl_fd;

	return -1;
}
