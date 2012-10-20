/**************************************
 ecmh - Easy Cast du Multi Hub
 by Jeroen Massar <jeroen@unfix.org>
***************************************
 $Author: fuzzel $
 $Id: interfaces.c,v 1.10 2005/02/09 17:58:06 fuzzel Exp $
 $Date: 2005/02/09 17:58:06 $
**************************************/

#include "ecmh.h"

#ifdef ECMH_BPF
bool int_create_bpf(struct intnode *intn, bool tunnel)
{
	unsigned int	i;
	char		devname[IFNAMSIZ];
	struct ifreq	ifr;

	/*
	 * When we are:
	 * - not in tunnel mode,
	 * or
	 * - it is not a tunnel
	 *
	 * Allocate BPF etc.
	 */
	if (!g_conf->tunnelmode || !tunnel)
	{
		/* Open a BPF for this interface */
		i = 0;
		while (intn->socket < 0)
		{
			snprintf(devname, sizeof(devname), "/dev/bpf%u", i++);
			intn->socket = open(devname, O_RDWR);
			if (intn->socket >= 0 || errno != EBUSY) break;
		}
		if (intn->socket < 0)
		{
			dolog(LOG_ERR, "Couldn't open a new BPF device for %s\n", intn->name);
			return false;
		}

		dolog(LOG_INFO, "Opened %s as a BPF device for %s\n", devname, intn->name);

		/* Bind it to the interface */
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, intn->name, sizeof(ifr.ifr_name));

		if (ioctl(intn->socket, BIOCSETIF, &ifr))
		{
			dolog(LOG_ERR, "Could not bind BPF to %s: %s (%d)\n", intn->name, strerror(errno), errno);
			return false;
		}
		dolog(LOG_INFO, "Bound BPF %s to %s\n", devname, intn->name);

		if (g_conf->promisc)
		{
			if (ioctl(intn->socket, BIOCPROMISC))
			{
			    	dolog(LOG_ERR, "Could not set %s to promisc: %s (%d)\n", intn->name, strerror(errno), errno);
				return false;
			}
			dolog(LOG_INFO, "BPF interface for %s is now promiscious\n", intn->name);
		}

		if (fcntl(intn->socket, F_SETFL, O_NONBLOCK) < 0)
		{
			dolog(LOG_ERR, "Could not set %s to non_blocking: %s (%d)\n", intn->name, strerror(errno), errno);
			return false;
		}

		i = 1;
		if (ioctl(intn->socket, BIOCIMMEDIATE, &i))
		{
			dolog(LOG_ERR, "Could not set %s to immediate: %s (%d)\n", intn->name, strerror(errno), errno);
			return false;
		}

		if (ioctl(intn->socket, BIOCGDLT, &intn->dlt))
		{
			dolog(LOG_ERR, "Could not get %s's DLT: %s (%d)\n", intn->name, strerror(errno), errno);
			return false;
		}
		if (intn->dlt != DLT_NULL && intn->dlt != DLT_EN10MB)
		{
		    	dolog(LOG_ERR, "Only NULL and EN10MB (Ethernet) as a DLT are supported, this DLT is %u\n", intn->dlt);
			return false;
		}
		dolog(LOG_INFO, "BPF's DLT is %s\n", intn->dlt == DLT_EN10MB ? "Ethernet" : (intn->dlt == DLT_NULL ? "Null" : "??"));

		if (ioctl(intn->socket, BIOCGBLEN, &intn->bufferlen))
		{
			dolog(LOG_ERR, "Could not get %s's BufferLen: %s (%d)\n", intn->name, strerror(errno), errno);
			return false;
		}
		dolog(LOG_INFO, "BPF's bufferLength is %u\n", intn->bufferlen);

		/* Is this buffer bigger than what we have allocated? -> Upgrade */
		if (intn->bufferlen > g_conf->bufferlen)
		{
			free(g_conf->buffer);
			g_conf->bufferlen = intn->bufferlen;
			g_conf->buffer = malloc(g_conf->bufferlen);
			if (!g_conf->buffer)
			{
				dolog(LOG_ERR, "Couldn't increase bufferlength to %u\n", g_conf->bufferlen);
				dolog(LOG_ERR, "Expecting a memory shortage, exiting\n");
				exit(-1);
			}
			memset(g_conf->buffer, 0, g_conf->bufferlen);
		}

		/* Add it to the select set */
		FD_SET(intn->socket, &g_conf->selectset);
		if (intn->socket > g_conf->hifd) g_conf->hifd = intn->socket;
	}
	/* It is a tunnel and we are in tunnel mode*/
	else
	{
		struct if_laddrreq	iflr;
		int			sock;

		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (sock < 0)
		{
			dolog(LOG_WARNING, "Couldn't allocate a AF_INET socket for getting interface info\n");
			return false;
		}

		memset(&iflr, 0, sizeof(iflr));
		strncpy(iflr.iflr_name, intn->name, sizeof(iflr.iflr_name));
 
		/*
		 * This a tunnel, find out based on it's src ip
		 * to which master it belongs
		 */
		if (ioctl(sock, SIOCGLIFPHYADDR, &iflr))
		{
			dolog(LOG_ERR, "Could not get GIF Addresses of tunnel %s: %s (%d)\n",
				intn->name, strerror(errno), errno);
			close(sock);
			return false;
		}
		close(sock);

		/* Find the master */
		intn->master = int_find_ipv4(true, &((struct sockaddr_in *)&iflr.addr)->sin_addr, false);
		if (!intn->master)
		{
			char buf[1024];
			inet_ntop(AF_INET, &((struct sockaddr_in *)&iflr.addr)->sin_addr, (char *)&buf, sizeof(buf));
			dolog(LOG_ERR, "Couldn't find the master device for %s (%s)\n", intn->name, buf);
			return false;
		}

		/* Store the addresses */
		memcpy(&intn->ipv4_local[0], &((struct sockaddr_in *)&iflr.addr)->sin_addr, sizeof(intn->ipv4_local[0]));
		memcpy(&intn->ipv4_remote, &((struct sockaddr_in *)&iflr.dstaddr)->sin_addr, sizeof(intn->ipv4_remote));
	}
	return true;
}
#endif /* ECMH_BPF */

#ifndef ECMH_BPF
struct intnode *int_create(unsigned int ifindex)
#else
struct intnode *int_create(unsigned int ifindex, bool tunnel)
#endif
{
	struct intnode	*intn = NULL;
	struct ifreq	ifreq;
	int		sock;

	/* Resize the interface array if needed */
	if ((ifindex+1) > g_conf->maxinterfaces)
	{
		g_conf->ints = (struct intnode *)realloc(g_conf->ints, sizeof(struct intnode)*(ifindex+1));

		if (!g_conf->ints)
		{
			dolog(LOG_ERR, "Couldn't init() - no memory for interface array.\n");
			exit(-1);
		}

		/* Clear out the new memory */
		memset(&g_conf->ints[g_conf->maxinterfaces], 0, sizeof(struct intnode)*((ifindex+1)-g_conf->maxinterfaces));

		/* Configure the new maximum */
		g_conf->maxinterfaces = (ifindex+1);
	}

	intn = &g_conf->ints[ifindex];

#ifndef ECMH_BPF
	dolog(LOG_DEBUG, "Creating new interface %u\n", ifindex);
#else
	dolog(LOG_DEBUG, "Creating new interface %u (%s)\n", ifindex, tunnel ? "Tunnel" : "Native");
#endif

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		dolog(LOG_ERR, "Couldn't create tempory socket for ioctl's\n");
		return NULL;
	}

	memset(intn, 0, sizeof(*intn));

	intn->ifindex = ifindex;

	/* Default to 0, we discover this after the queries has been sent */
	intn->mld_version = 0;

#ifdef ECMH_BPF
	intn->socket = -1;
#endif

	/* Get the interface name (eth0/sit0/...) */
	/* Will be used for reports etc */
	memset(&ifreq, 0, sizeof(ifreq));
#ifdef ECMH_BPF
	ifreq.ifr_ifindex = ifindex;
	if (ioctl(sock, SIOCGIFNAME, &ifreq) != 0)
#else
        if (if_indextoname(ifindex, intn->name) == NULL)
#endif
	{
		dolog(LOG_ERR, "Couldn't determine interfacename of link %d : %s\n", intn->ifindex, strerror(errno));
		int_destroy(intn);
		close(sock);
		return NULL;
	}
#ifdef ECMH_BPF
	memcpy(&intn->name, &ifreq.ifr_name, sizeof(intn->name));
#else
	memcpy(&ifreq.ifr_name, &intn->name, sizeof(ifreq.ifr_name));
#endif

	/* Get the MTU size of this interface */
	/* We will use that for fragmentation */
	if (ioctl(sock, SIOCGIFMTU, &ifreq) != 0)
	{
		dolog(LOG_ERR, "Couldn't determine MTU size for %s, link %d : %s\n", intn->name, intn->ifindex, strerror(errno));
		perror("Error");
		int_destroy(intn);
		close(sock);
		return NULL;
	}
	intn->mtu = ifreq.ifr_mtu;

#ifndef ECMH_BPF
	/* Get hardware address + type */
	if (ioctl(sock, SIOCGIFHWADDR, &ifreq) != 0)
	{
		dolog(LOG_ERR, "Couldn't determine hardware address for %s, link %d : %s\n", intn->name, intn->ifindex, strerror(errno));
		perror("Error");
		int_destroy(intn);
		close(sock);
		return NULL;
	}
	memcpy(&intn->hwaddr, &ifreq.ifr_hwaddr, sizeof(intn->hwaddr));
#endif

#ifndef ECMH_BPF
	/* Ignore Loopback devices */
	if (intn->hwaddr.sa_family == ARPHRD_LOOPBACK)
	{
		int_destroy(intn);
		close(sock);
		return NULL;
	}
#endif

#ifdef ECMH_BPF
	if (!int_create_bpf(intn, tunnel))
	{
		int_destroy(intn);
		close(sock);
		return NULL;
	}
#else
	/* Configure the interface to receive all multicast addresses */
	if (g_conf->promisc)
	{
		struct ifreq ifr;
		int err;

		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, intn->name, sizeof(ifr.ifr_name));
        	err = ioctl(sock, SIOCGIFFLAGS, &ifr);
		if (err)
		{
			dolog(LOG_WARNING, "Couldn't get interface flags of %u/%s: %s\n",
				intn->ifindex, intn->name, strerror(errno));
		}
		else
		{
			/* Should use IFF_ALLMULTI, but that is not supported... */
			ifr.ifr_flags |= IFF_PROMISC;
			err = ioctl(sock, SIOCSIFFLAGS, &ifr);
			if (err)
			{
				dolog(LOG_WARNING, "Couldn't get interface flags of %u/%s: %s\n",
					intn->ifindex, intn->name, strerror(errno));
			}
		}
	}
#endif

	/* Cleanup the socket */
	close(sock);

	if (	g_conf->upstream &&
		strcasecmp(intn->name, g_conf->upstream) == 0)
	{
		intn->upstream = true;
		g_conf->upstream_id = intn->ifindex;
	}
	else intn->upstream = false;

	/* All okay */
	return intn;
}

void int_destroy(struct intnode *intn)
{
D(	dolog(LOG_DEBUG, "Destroying interface %s\n", intn->name);)

#ifdef ECMH_BPF
	if (intn->socket != -1)
	{
		close(intn->socket);
		intn->socket = -1;
	}
#endif

	/* Resetting the MTU to zero disabled the interface */
	intn->mtu = 0;
}

struct intnode *int_find(unsigned int ifindex)
{
	if (ifindex >= g_conf->maxinterfaces || g_conf->ints[ifindex].mtu == 0) return NULL;
	return &g_conf->ints[ifindex];
}

#ifdef ECMH_BPF
struct intnode *int_find_ipv4(bool local, struct in_addr *ipv4)
{
	struct intnode	*intn;
	struct listnode	*ln;
	int		num = 0;
	unsigned int	i;

	for (i=0;i<g_conf->maxinterfaces;i++)
	{
		intn = &g_conf->ints[i];
		/* Skip uninitialized interfaces */
		if (intn->mtu == 0) continue;

		for (num=0;num<INTNODE_MAXIPV4;num++)
		{
			if (memcmp(local ? &intn->ipv4_local[num] : &intn->ipv4_remote, ipv4, sizeof(ipv4)) == 0)
			{
				return intn;
			}

			/* Only one remote IPv4 address */
			if (!local) break;
		}
	}
	return NULL;
}
#endif /* ECMH_BPF */

/*
 * Store the version of MLD if it is lower than the old one or the old one was 0 ;)
 * We only respond MLDv1's this way when there is a MLDv1 router on the link.
 * but we respond MLDv2's when there are only MLDv2 router on the link
 * Even if we build without MLDv2 support we detect the version which
 * could be used to determine if a router can't fallback to v1 for instance
 * later on in diagnostics. When built without MLDv2 support we always do MLDv1 though.
 * Everthing higher as MLDv2 is reported as MLDv2 as we don't know about it (yet :)
 */
void int_set_mld_version(struct intnode *intn, unsigned int newversion)
{
	if (newversion == 1)
	{
#ifdef ECMH_SUPPORT_MLD2
		if (g_conf->mld2only)
		{
			dolog(LOG_DEBUG, "Configured as MLDv2 Only Host, ignoring MLDv1 packet\n");
			return;
		}
#endif
		/*
		 * Only reset the version number
		 * if it wasn't set and not v1 yet
		 */
		if (	intn->mld_version == 0 &&
			intn->mld_version != 1)
		{
			if (intn->mld_version > 1) dolog(LOG_DEBUG, "MLDv1 Query detected on %s, downgrading from MLDv%u to MLDv1\n", intn->mld_version, intn->name);
			else dolog(LOG_DEBUG, "MLDv1 detected on %s, setting it to MLDv1\n", intn->name);
			intn->mld_version = 1;
			intn->mld_last_v1 = time(NULL);
		}
	}
#ifdef ECMH_SUPPORT_MLD2
	else
	{
		/*
		 * Current version = 1 and haven't seen a v1 for a while?
		 * Reset the counter
		 */
		if (	intn->mld_last_v1 != 0 &&
			(time(NULL) > (intn->mld_last_v1 + ECMH_SUBSCRIPTION_TIMEOUT)))
		{
			dolog(LOG_DEBUG, "MLDv1 has not been seen for %u seconds on %s, allowing upgrades\n", 
				(intn->mld_last_v1 + ECMH_SUBSCRIPTION_TIMEOUT), intn->name);
			/* Reset the version */
			intn->mld_version = 0;
			intn->mld_last_v1 = 0;
		}

		if (g_conf->mld1only)
		{
			dolog(LOG_DEBUG, "Configured as MLDv1 Only Host, ignoring possible upgrade to MLDv%u\n",
				newversion);
			return;
		}

		/*
		 * Only reset the version number
		 * if it wasn't set and not v1 yet and not v2 yet
		 */
		if (	intn->mld_version == 0 &&
			intn->mld_version != 1 &&
			intn->mld_version != 2)
		{
			dolog(LOG_DEBUG, "MLDv%u detected on %s/%u, setting it to MLDv%u\n",
				newversion, intn->name, intn->ifindex, newversion);
			intn->mld_version = newversion;
		}
	}
#else /* ECMH_SUPPORT_MLD2 */
	else
	{
		dolog(LOG_DEBUG, "MLDv%u detected on %s/%u while that version is not supported\n",
			newversion, intn->name);
	}
#endif /* ECMH_SUPPORT_MLD2 */
}

#ifdef ECMH_BPF

/* Add or update a local interface */
void local_update(struct intnode *intn)
{
	struct localnode	*localn;
	int			num=0;
	struct in_addr		any;

	/* Any IPv4 address */
	memset(&any, 0, sizeof(any));

	for (num=0;num<INTNODE_MAXIPV4;num++)
	{
		/* Empty ? */
		if (memcmp(&intn->ipv4_local[num], &any, sizeof(any)) == 0)
		{
			continue;
		}

		/* Try to find it first */
		localn = local_find(&intn->ipv4_local[num]);
		/* Already there?  */
		if (localn) continue;

		/* Allocate a piece of memory */
		localn = (struct localnode *)malloc(sizeof(*localn));
		if (!localn)
		{
			dolog(LOG_ERR, "Couldn't allocate memory for localnode\n");
			continue;
		}
		memset(localn, 0, sizeof(*localn));

		/* Fill it in */
		localn->intn = intn;

		dolog(LOG_DEBUG, "Adding %s to local tunnel-intercepting-interfaces\n", intn->name);

		/* Add it to the list */
		listnode_add(g_conf->locals, localn);
	}
}

struct localnode *local_find(struct in_addr *ipv4)
{
	struct localnode	*localn;
	struct listnode		*ln;
	int			num=0;

	LIST_LOOP(g_conf->locals, localn, ln)
	{
		for (num=0;num<INTNODE_MAXIPV4;num++)
		{
			if (memcmp(&localn->intn->ipv4_local[num], ipv4, sizeof(ipv4)) == 0)
			{
				return localn;
			}
		}
	}
	return NULL;
}

void local_remove(struct intnode *intn)
{
	struct localnode *localn;
	localn = local_find(&intn->ipv4_local);
	if (!localn) return;

	dolog(LOG_DEBUG, "Remove %s from local tunnel-intercepting-interfaces\n", intn->name);

	/* Remove it from the list */
	listnode_delete(g_conf->locals, localn);
}

void local_destroy(struct localnode *localn)
{
	/* Free the memory */
	free(localn);
}

#endif /* ECMH_BPF */

