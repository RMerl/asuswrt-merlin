/*
   Unix SMB/CIFS implementation.
   multiple interface handling
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "interfaces.h"

static struct iface_struct *probed_ifaces;
static int total_probed;

static struct interface *local_interfaces;

/****************************************************************************
 Check if an IP is one of mine.
**************************************************************************/

bool ismyaddr(const struct sockaddr *ip)
{
	struct interface *i;
	for (i=local_interfaces;i;i=i->next) {
		if (sockaddr_equal((struct sockaddr *)&i->ip,ip)) {
			return true;
		}
	}
	return false;
}

bool ismyip_v4(struct in_addr ip)
{
	struct sockaddr_storage ss;
	in_addr_to_sockaddr_storage(&ss, ip);
	return ismyaddr((struct sockaddr *)&ss);
}

/****************************************************************************
 Try and find an interface that matches an ip. If we cannot, return NULL.
**************************************************************************/

static struct interface *iface_find(const struct sockaddr *ip,
				bool check_mask)
{
	struct interface *i;

	if (is_address_any(ip)) {
		return local_interfaces;
	}

	for (i=local_interfaces;i;i=i->next) {
		if (check_mask) {
			if (same_net(ip, (struct sockaddr *)&i->ip, (struct sockaddr *)&i->netmask)) {
				return i;
			}
		} else if (sockaddr_equal((struct sockaddr *)&i->ip, ip)) {
			return i;
		}
	}

	return NULL;
}

/****************************************************************************
 Check if a packet is from a local (known) net.
**************************************************************************/

bool is_local_net(const struct sockaddr *from)
{
	struct interface *i;
	for (i=local_interfaces;i;i=i->next) {
		if (same_net(from, (struct sockaddr *)&i->ip, (struct sockaddr *)&i->netmask)) {
			return true;
		}
	}
	return false;
}

#if defined(HAVE_IPV6)
void setup_linklocal_scope_id(struct sockaddr *pss)
{
	struct interface *i;
	for (i=local_interfaces;i;i=i->next) {
		if (sockaddr_equal((struct sockaddr *)&i->ip,pss)) {
			struct sockaddr_in6 *psa6 =
				(struct sockaddr_in6 *)pss;
			psa6->sin6_scope_id = if_nametoindex(i->name);
			return;
		}
	}
}
#endif

/****************************************************************************
 Check if a packet is from a local (known) net.
**************************************************************************/

bool is_local_net_v4(struct in_addr from)
{
	struct sockaddr_storage ss;

	in_addr_to_sockaddr_storage(&ss, from);
	return is_local_net((struct sockaddr *)&ss);
}

/****************************************************************************
 How many interfaces do we have ?
**************************************************************************/

int iface_count(void)
{
	int ret = 0;
	struct interface *i;

	for (i=local_interfaces;i;i=i->next) {
		ret++;
	}
	return ret;
}

/****************************************************************************
 How many non-loopback IPv4 interfaces do we have ?
**************************************************************************/

int iface_count_v4_nl(void)
{
	int ret = 0;
	struct interface *i;

	for (i=local_interfaces;i;i=i->next) {
		if (is_loopback_addr((struct sockaddr *)&i->ip)) {
			continue;
		}
		if (i->ip.ss_family == AF_INET) {
			ret++;
		}
	}
	return ret;
}

/****************************************************************************
 Return a pointer to the in_addr of the first IPv4 interface that's
 not 0.0.0.0.
**************************************************************************/

const struct in_addr *first_ipv4_iface(void)
{
	struct interface *i;

	for (i=local_interfaces;i ;i=i->next) {
		if ((i->ip.ss_family == AF_INET) &&
		    (!is_zero_ip_v4(((struct sockaddr_in *)&i->ip)->sin_addr)))
		{
			break;
		}
	}

	if (!i) {
		return NULL;
	}
	return &((const struct sockaddr_in *)&i->ip)->sin_addr;
}

/****************************************************************************
 Return the Nth interface.
**************************************************************************/

struct interface *get_interface(int n)
{
	struct interface *i;

	for (i=local_interfaces;i && n;i=i->next) {
		n--;
	}

	if (i) {
		return i;
	}
	return NULL;
}

/****************************************************************************
 Return IP sockaddr_storage of the Nth interface.
**************************************************************************/

const struct sockaddr_storage *iface_n_sockaddr_storage(int n)
{
	struct interface *i;

	for (i=local_interfaces;i && n;i=i->next) {
		n--;
	}

	if (i) {
		return &i->ip;
	}
	return NULL;
}

/****************************************************************************
 Return IPv4 of the Nth interface (if a v4 address). NULL otherwise.
**************************************************************************/

const struct in_addr *iface_n_ip_v4(int n)
{
	struct interface *i;

	for (i=local_interfaces;i && n;i=i->next) {
		n--;
	}

	if (i && i->ip.ss_family == AF_INET) {
		return &((const struct sockaddr_in *)&i->ip)->sin_addr;
	}
	return NULL;
}

/****************************************************************************
 Return IPv4 bcast of the Nth interface (if a v4 address). NULL otherwise.
**************************************************************************/

const struct in_addr *iface_n_bcast_v4(int n)
{
	struct interface *i;

	for (i=local_interfaces;i && n;i=i->next) {
		n--;
	}

	if (i && i->ip.ss_family == AF_INET) {
		return &((const struct sockaddr_in *)&i->bcast)->sin_addr;
	}
	return NULL;
}

/****************************************************************************
 Return bcast of the Nth interface.
**************************************************************************/

const struct sockaddr_storage *iface_n_bcast(int n)
{
	struct interface *i;

	for (i=local_interfaces;i && n;i=i->next) {
		n--;
	}

	if (i) {
		return &i->bcast;
	}
	return NULL;
}

/* these 3 functions return the ip/bcast/nmask for the interface
   most appropriate for the given ip address. If they can't find
   an appropriate interface they return the requested field of the
   first known interface. */

const struct sockaddr_storage *iface_ip(const struct sockaddr *ip)
{
	struct interface *i = iface_find(ip, true);
	if (i) {
		return &i->ip;
	}

	/* Search for the first interface with
	 * matching address family. */

	for (i=local_interfaces;i;i=i->next) {
		if (i->ip.ss_family == ip->sa_family) {
			return &i->ip;
		}
	}
	return NULL;
}

/*
  return True if a IP is directly reachable on one of our interfaces
*/

bool iface_local(const struct sockaddr *ip)
{
	return iface_find(ip, true) ? true : false;
}

/****************************************************************************
 Add an interface to the linked list of interfaces.
****************************************************************************/

static void add_interface(const struct iface_struct *ifs)
{
	char addr[INET6_ADDRSTRLEN];
	struct interface *iface;

	if (iface_find((struct sockaddr *)&ifs->ip, False)) {
		DEBUG(3,("add_interface: not adding duplicate interface %s\n",
			print_sockaddr(addr, sizeof(addr), &ifs->ip) ));
		return;
	}

	if (!(ifs->flags & (IFF_BROADCAST|IFF_LOOPBACK))) {
		DEBUG(3,("not adding non-broadcast interface %s\n",
					ifs->name ));
		return;
	}

	iface = SMB_MALLOC_P(struct interface);
	if (!iface) {
		return;
	}

	ZERO_STRUCTPN(iface);

	iface->name = SMB_STRDUP(ifs->name);
	if (!iface->name) {
		SAFE_FREE(iface);
		return;
	}
	iface->flags = ifs->flags;
	iface->ip = ifs->ip;
	iface->netmask = ifs->netmask;
	iface->bcast = ifs->bcast;

	DLIST_ADD(local_interfaces, iface);

	DEBUG(2,("added interface %s ip=%s ",
		iface->name,
		print_sockaddr(addr, sizeof(addr), &iface->ip) ));
	DEBUG(2,("bcast=%s ",
		print_sockaddr(addr, sizeof(addr),
			&iface->bcast) ));
	DEBUG(2,("netmask=%s\n",
		print_sockaddr(addr, sizeof(addr),
			&iface->netmask) ));
}

/****************************************************************************
 Interpret a single element from a interfaces= config line.

 This handles the following different forms:

 1) wildcard interface name
 2) DNS name
 3) IP/masklen
 4) ip/mask
 5) bcast/mask
****************************************************************************/

static void interpret_interface(char *token)
{
	struct sockaddr_storage ss;
	struct sockaddr_storage ss_mask;
	struct sockaddr_storage ss_net;
	struct sockaddr_storage ss_bcast;
	struct iface_struct ifs;
	char *p;
	int i;
	bool added=false;
	bool goodaddr = false;

	/* first check if it is an interface name */
	for (i=0;i<total_probed;i++) {
		if (gen_fnmatch(token, probed_ifaces[i].name) == 0) {
			add_interface(&probed_ifaces[i]);
			added = true;
		}
	}
	if (added) {
		return;
	}

	/* maybe it is a DNS name */
	p = strchr_m(token,'/');
	if (p == NULL) {
		if (!interpret_string_addr(&ss, token, 0)) {
			DEBUG(2, ("interpret_interface: Can't find address "
				  "for %s\n", token));
			return;
		}

		for (i=0;i<total_probed;i++) {
			if (sockaddr_equal((struct sockaddr *)&ss, (struct sockaddr *)&probed_ifaces[i].ip)) {
				add_interface(&probed_ifaces[i]);
				return;
			}
		}
		DEBUG(2,("interpret_interface: "
			"can't determine interface for %s\n",
			token));
		return;
	}

	/* parse it into an IP address/netmasklength pair */
	*p = 0;
	goodaddr = interpret_string_addr(&ss, token, 0);
	*p++ = '/';

	if (!goodaddr) {
		DEBUG(2,("interpret_interface: "
			"can't determine interface for %s\n",
			token));
		return;
	}

	if (strlen(p) > 2) {
		goodaddr = interpret_string_addr(&ss_mask, p, 0);
		if (!goodaddr) {
			DEBUG(2,("interpret_interface: "
				"can't determine netmask from %s\n",
				p));
			return;
		}
	} else {
		char *endp = NULL;
		unsigned long val = strtoul(p, &endp, 0);
		if (p == endp || (endp && *endp != '\0')) {
			DEBUG(2,("interpret_interface: "
				"can't determine netmask value from %s\n",
				p));
			return;
		}
		if (!make_netmask(&ss_mask, &ss, val)) {
			DEBUG(2,("interpret_interface: "
				"can't apply netmask value %lu from %s\n",
				val,
				p));
			return;
		}
	}

	make_bcast(&ss_bcast, &ss, &ss_mask);
	make_net(&ss_net, &ss, &ss_mask);

	/* Maybe the first component was a broadcast address. */
	if (sockaddr_equal((struct sockaddr *)&ss_bcast, (struct sockaddr *)&ss) ||
		sockaddr_equal((struct sockaddr *)&ss_net, (struct sockaddr *)&ss)) {
		for (i=0;i<total_probed;i++) {
			if (same_net((struct sockaddr *)&ss, 
						 (struct sockaddr *)&probed_ifaces[i].ip, 
						 (struct sockaddr *)&ss_mask)) {
				/* Temporarily replace netmask on
				 * the detected interface - user knows
				 * best.... */
				struct sockaddr_storage saved_mask =
					probed_ifaces[i].netmask;
				probed_ifaces[i].netmask = ss_mask;
				DEBUG(2,("interpret_interface: "
					"using netmask value %s from "
					"config file on interface %s\n",
					p,
					probed_ifaces[i].name));
				add_interface(&probed_ifaces[i]);
				probed_ifaces[i].netmask = saved_mask;
				return;
			}
		}
		DEBUG(2,("interpret_interface: Can't determine ip for "
			"broadcast address %s\n",
			token));
		return;
	}

	/* Just fake up the interface definition. User knows best. */

	DEBUG(2,("interpret_interface: Adding interface %s\n",
		token));

	ZERO_STRUCT(ifs);
	(void)strlcpy(ifs.name, token, sizeof(ifs.name));
	ifs.flags = IFF_BROADCAST;
	ifs.ip = ss;
	ifs.netmask = ss_mask;
	ifs.bcast = ss_bcast;
	add_interface(&ifs);
}

/****************************************************************************
 Load the list of network interfaces.
****************************************************************************/

void load_interfaces(void)
{
	struct iface_struct *ifaces = NULL;
	const char **ptr = lp_interfaces();
	int i;

	gfree_interfaces();

	/* Probe the kernel for interfaces */
	total_probed = get_interfaces(talloc_tos(), &ifaces);

	if (total_probed > 0) {
		probed_ifaces = (struct iface_struct *)memdup(ifaces,
				sizeof(ifaces[0])*total_probed);
		if (!probed_ifaces) {
			DEBUG(0,("ERROR: memdup failed\n"));
			exit(1);
		}
	}
	TALLOC_FREE(ifaces);

	/* if we don't have a interfaces line then use all broadcast capable
	   interfaces except loopback */
	if (!ptr || !*ptr || !**ptr) {
		if (total_probed <= 0) {
			DEBUG(0,("ERROR: Could not determine network "
			"interfaces, you must use a interfaces config line\n"));
			exit(1);
		}
		for (i=0;i<total_probed;i++) {
			if (probed_ifaces[i].flags & IFF_BROADCAST) {
				add_interface(&probed_ifaces[i]);
			}
		}
		return;
	}

	if (ptr) {
		while (*ptr) {
			char *ptr_cpy = SMB_STRDUP(*ptr);
			if (ptr_cpy) {
				interpret_interface(ptr_cpy);
				free(ptr_cpy);
			}
			ptr++;
		}
	}

	if (!local_interfaces) {
		DEBUG(0,("WARNING: no network interfaces found\n"));
	}
}


void gfree_interfaces(void)
{
	while (local_interfaces) {
		struct interface *iface = local_interfaces;
		DLIST_REMOVE(local_interfaces, local_interfaces);
		SAFE_FREE(iface->name);
		SAFE_FREE(iface);
	}

	SAFE_FREE(probed_ifaces);
}

/****************************************************************************
 Return True if the list of probed interfaces has changed.
****************************************************************************/

bool interfaces_changed(void)
{
	bool ret = false;
	int n;
	struct iface_struct *ifaces = NULL;

	n = get_interfaces(talloc_tos(), &ifaces);

	if ((n > 0 )&& (n != total_probed ||
			memcmp(ifaces, probed_ifaces, sizeof(ifaces[0])*n))) {
		ret = true;
	}

	TALLOC_FREE(ifaces);
	return ret;
}
