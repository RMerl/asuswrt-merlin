/*
   Unix SMB/CIFS implementation.
   name query routines
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Jeremy Allison 2007.

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

/* nmbd.c sets this to True. */
bool global_in_nmbd = False;

/****************************
 * SERVER AFFINITY ROUTINES *
 ****************************/

 /* Server affinity is the concept of preferring the last domain
    controller with whom you had a successful conversation */

/****************************************************************************
****************************************************************************/
#define SAFKEY_FMT	"SAF/DOMAIN/%s"
#define SAF_TTL		900
#define SAFJOINKEY_FMT	"SAFJOIN/DOMAIN/%s"
#define SAFJOIN_TTL	3600

static char *saf_key(const char *domain)
{
	char *keystr;

	asprintf_strupper_m(&keystr, SAFKEY_FMT, domain);

	return keystr;
}

static char *saf_join_key(const char *domain)
{
	char *keystr;

	asprintf_strupper_m(&keystr, SAFJOINKEY_FMT, domain);

	return keystr;
}

/****************************************************************************
****************************************************************************/

bool saf_store( const char *domain, const char *servername )
{
	char *key;
	time_t expire;
	bool ret = False;

	if ( !domain || !servername ) {
		DEBUG(2,("saf_store: "
			"Refusing to store empty domain or servername!\n"));
		return False;
	}

	if ( (strlen(domain) == 0) || (strlen(servername) == 0) ) {
		DEBUG(0,("saf_store: "
			"refusing to store 0 length domain or servername!\n"));
		return False;
	}

	key = saf_key( domain );
	expire = time( NULL ) + lp_parm_int(-1, "saf","ttl", SAF_TTL);

	DEBUG(10,("saf_store: domain = [%s], server = [%s], expire = [%u]\n",
		domain, servername, (unsigned int)expire ));

	ret = gencache_set( key, servername, expire );

	SAFE_FREE( key );

	return ret;
}

bool saf_join_store( const char *domain, const char *servername )
{
	char *key;
	time_t expire;
	bool ret = False;

	if ( !domain || !servername ) {
		DEBUG(2,("saf_join_store: Refusing to store empty domain or servername!\n"));
		return False;
	}

	if ( (strlen(domain) == 0) || (strlen(servername) == 0) ) {
		DEBUG(0,("saf_join_store: refusing to store 0 length domain or servername!\n"));
		return False;
	}

	key = saf_join_key( domain );
	expire = time( NULL ) + lp_parm_int(-1, "saf","join ttl", SAFJOIN_TTL);

	DEBUG(10,("saf_join_store: domain = [%s], server = [%s], expire = [%u]\n",
		domain, servername, (unsigned int)expire ));

	ret = gencache_set( key, servername, expire );

	SAFE_FREE( key );

	return ret;
}

bool saf_delete( const char *domain )
{
	char *key;
	bool ret = False;

	if ( !domain ) {
		DEBUG(2,("saf_delete: Refusing to delete empty domain\n"));
		return False;
	}

	key = saf_join_key(domain);
	ret = gencache_del(key);
	SAFE_FREE(key);

	if (ret) {
		DEBUG(10,("saf_delete[join]: domain = [%s]\n", domain ));
	}

	key = saf_key(domain);
	ret = gencache_del(key);
	SAFE_FREE(key);

	if (ret) {
		DEBUG(10,("saf_delete: domain = [%s]\n", domain ));
	}

	return ret;
}

/****************************************************************************
****************************************************************************/

char *saf_fetch( const char *domain )
{
	char *server = NULL;
	time_t timeout;
	bool ret = False;
	char *key = NULL;

	if ( !domain || strlen(domain) == 0) {
		DEBUG(2,("saf_fetch: Empty domain name!\n"));
		return NULL;
	}

	key = saf_join_key( domain );

	ret = gencache_get( key, &server, &timeout );

	SAFE_FREE( key );

	if ( ret ) {
		DEBUG(5,("saf_fetch[join]: Returning \"%s\" for \"%s\" domain\n",
			server, domain ));
		return server;
	}

	key = saf_key( domain );

	ret = gencache_get( key, &server, &timeout );

	SAFE_FREE( key );

	if ( !ret ) {
		DEBUG(5,("saf_fetch: failed to find server for \"%s\" domain\n",
					domain ));
	} else {
		DEBUG(5,("saf_fetch: Returning \"%s\" for \"%s\" domain\n",
			server, domain ));
	}

	return server;
}

/****************************************************************************
 Generate a random trn_id.
****************************************************************************/

static int generate_trn_id(void)
{
	uint16 id;

	generate_random_buffer((uint8 *)&id, sizeof(id));

	return id % (unsigned)0x7FFF;
}

/****************************************************************************
 Parse a node status response into an array of structures.
****************************************************************************/

static NODE_STATUS_STRUCT *parse_node_status(char *p,
				int *num_names,
				struct node_status_extra *extra)
{
	NODE_STATUS_STRUCT *ret;
	int i;

	*num_names = CVAL(p,0);

	if (*num_names == 0)
		return NULL;

	ret = SMB_MALLOC_ARRAY(NODE_STATUS_STRUCT,*num_names);
	if (!ret)
		return NULL;

	p++;
	for (i=0;i< *num_names;i++) {
		StrnCpy(ret[i].name,p,15);
		trim_char(ret[i].name,'\0',' ');
		ret[i].type = CVAL(p,15);
		ret[i].flags = p[16];
		p += 18;
		DEBUG(10, ("%s#%02x: flags = 0x%02x\n", ret[i].name,
			   ret[i].type, ret[i].flags));
	}
	/*
	 * Also, pick up the MAC address ...
	 */
	if (extra) {
		memcpy(&extra->mac_addr, p, 6); /* Fill in the mac addr */
	}
	return ret;
}


/****************************************************************************
 Do a NBT node status query on an open socket and return an array of
 structures holding the returned names or NULL if the query failed.
**************************************************************************/

NODE_STATUS_STRUCT *node_status_query(int fd,
					struct nmb_name *name,
					const struct sockaddr_storage *to_ss,
					int *num_names,
					struct node_status_extra *extra,
					int retry_time)
{
	bool found=False;
	int retries = 2;
	//int retry_time = 2000;	
	struct timeval tval;
	struct packet_struct p;
	struct packet_struct *p2;
	struct nmb_packet *nmb = &p.packet.nmb;
	NODE_STATUS_STRUCT *ret;

	ZERO_STRUCT(p);

	if (to_ss->ss_family != AF_INET) {
		/* Can't do node status to IPv6 */
		return NULL;
	}
	nmb->header.name_trn_id = generate_trn_id();
	nmb->header.opcode = 0;
	nmb->header.response = false;
	nmb->header.nm_flags.bcast = false;
	nmb->header.nm_flags.recursion_available = false;
	nmb->header.nm_flags.recursion_desired = false;
	nmb->header.nm_flags.trunc = false;
	nmb->header.nm_flags.authoritative = false;
	nmb->header.rcode = 0;
	nmb->header.qdcount = 1;
	nmb->header.ancount = 0;
	nmb->header.nscount = 0;
	nmb->header.arcount = 0;
	nmb->question.question_name = *name;
	nmb->question.question_type = 0x21;
	nmb->question.question_class = 0x1;

	p.ip = ((const struct sockaddr_in *)to_ss)->sin_addr;
	p.port = NMB_PORT;
	p.recv_fd = -1;
	p.send_fd = fd;
	p.timestamp = time(NULL);
	p.packet_type = NMB_PACKET;

	GetTimeOfDay(&tval);

	if (!send_packet(&p))
		return NULL;

	retries--;

	while (1) {
		
		struct timeval tval2;
		GetTimeOfDay(&tval2);

		//- 20120309 Jerry modify to check if diff time is lower 0
		if (TvalDiff(&tval,&tval2) > retry_time || TvalDiff(&tval,&tval2) < 0) {
		//if (TvalDiff(&tval,&tval2) > retry_time) {
			if (!retries){
				//fprintf(stderr, "break\n");
				break;
			}
			if (!found && !send_packet(&p))
				return NULL;
			GetTimeOfDay(&tval);
			retries--;
			//fprintf(stderr,"retry retries=[%d][%d]\n", fd, retries);			
		}
		//fprintf(stderr, "1, [%d]\n", fd);
		if ((p2=receive_nmb_packet(fd,90,nmb->header.name_trn_id))) {
			
			struct nmb_packet *nmb2 = &p2->packet.nmb;
			debug_nmb_packet(p2);
			
			if (nmb2->header.opcode != 0 ||
			    nmb2->header.nm_flags.bcast ||
			    nmb2->header.rcode ||
			    !nmb2->header.ancount ||
			    nmb2->answers->rr_type != 0x21) {
				/* XXXX what do we do with this? could be a
				   redirect, but we'll discard it for the
				   moment */
				free_packet(p2);
				continue;
			}

			ret = parse_node_status(&nmb2->answers->rdata[0],
					num_names, extra);
			free_packet(p2);
			
			return ret;
		}

	}
	//fprintf(stderr, "4\n");

	return NULL;
}

/****************************************************************************
 Find the first type XX name in a node status reply - used for finding
 a servers name given its IP. Return the matched name in *name.
**************************************************************************/

bool name_status_find(const char *q_name,
			int q_type,
			int type,
			const struct sockaddr_storage *to_ss,
			fstring name)
{
	char addr[INET6_ADDRSTRLEN];
	struct sockaddr_storage ss;
	NODE_STATUS_STRUCT *status = NULL;
	struct nmb_name nname;
	int count, i;
	int sock;
	bool result = false;

	if (lp_disable_netbios()) {
		DEBUG(5,("name_status_find(%s#%02x): netbios is disabled\n",
					q_name, q_type));
		return False;
	}

	print_sockaddr(addr, sizeof(addr), to_ss);

	DEBUG(10, ("name_status_find: looking up %s#%02x at %s\n", q_name,
		   q_type, addr));

	/* Check the cache first. */

	if (namecache_status_fetch(q_name, q_type, type, to_ss, name)) {
		return True;
	}

	if (to_ss->ss_family != AF_INET) {
		/* Can't do node status to IPv6 */
		return false;
	}

	if (!interpret_string_addr(&ss, lp_socket_address(),
				AI_NUMERICHOST|AI_PASSIVE)) {
		zero_sockaddr(&ss);
	}

	sock = open_socket_in(SOCK_DGRAM, 0, 3, &ss, True);
	if (sock == -1)
		goto done;

	/* W2K PDC's seem not to respond to '*'#0. JRA */
	make_nmb_name(&nname, q_name, q_type);
	status = node_status_query(sock, &nname, to_ss, &count, NULL, 2000);
	close(sock);
	if (!status)
		goto done;

	for (i=0;i<count;i++) {
                /* Find first one of the requested type that's not a GROUP. */
		if (status[i].type == type && ! (status[i].flags & 0x80))
			break;
	}
	if (i == count)
		goto done;

	pull_ascii_nstring(name, sizeof(fstring), status[i].name);

	/* Store the result in the cache. */
	/* but don't store an entry for 0x1c names here.  Here we have
	   a single host and DOMAIN<0x1c> names should be a list of hosts */

	if ( q_type != 0x1c ) {
		namecache_status_store(q_name, q_type, type, to_ss, name);
	}

	result = true;

 done:
	SAFE_FREE(status);

	DEBUG(10, ("name_status_find: name %sfound", result ? "" : "not "));

	if (result)
		DEBUGADD(10, (", name %s ip address is %s", name, addr));

	DEBUG(10, ("\n"));

	return result;
}

/*
  comparison function used by sort_addr_list
*/

static int addr_compare(const struct sockaddr *ss1,
		const struct sockaddr *ss2)
{
	int max_bits1=0, max_bits2=0;
	int num_interfaces = iface_count();
	int i;

	/* Sort IPv4 addresses first. */
	if (ss1->sa_family != ss2->sa_family) {
		if (ss2->sa_family == AF_INET) {
			return 1;
		} else {
			return -1;
		}
	}

	/* Here we know both addresses are of the same
	 * family. */

	for (i=0;i<num_interfaces;i++) {
		const struct sockaddr_storage *pss = iface_n_bcast(i);
		unsigned char *p_ss1 = NULL;
		unsigned char *p_ss2 = NULL;
		unsigned char *p_if = NULL;
		size_t len = 0;
		int bits1, bits2;

		if (pss->ss_family != ss1->sa_family) {
			/* Ignore interfaces of the wrong type. */
			continue;
		}
		if (pss->ss_family == AF_INET) {
			p_if = (unsigned char *)
				&((const struct sockaddr_in *)pss)->sin_addr;
			p_ss1 = (unsigned char *)
				&((const struct sockaddr_in *)ss1)->sin_addr;
			p_ss2 = (unsigned char *)
				&((const struct sockaddr_in *)ss2)->sin_addr;
			len = 4;
		}
#if defined(HAVE_IPV6)
		if (pss->ss_family == AF_INET6) {
			p_if = (unsigned char *)
				&((const struct sockaddr_in6 *)pss)->sin6_addr;
			p_ss1 = (unsigned char *)
				&((const struct sockaddr_in6 *)ss1)->sin6_addr;
			p_ss2 = (unsigned char *)
				&((const struct sockaddr_in6 *)ss2)->sin6_addr;
			len = 16;
		}
#endif
		if (!p_ss1 || !p_ss2 || !p_if || len == 0) {
			continue;
		}
		bits1 = matching_len_bits(p_ss1, p_if, len);
		bits2 = matching_len_bits(p_ss2, p_if, len);
		max_bits1 = MAX(bits1, max_bits1);
		max_bits2 = MAX(bits2, max_bits2);
	}

	/* Bias towards directly reachable IPs */
	if (iface_local(ss1)) {
		if (ss1->sa_family == AF_INET) {
			max_bits1 += 32;
		} else {
			max_bits1 += 128;
		}
	}
	if (iface_local(ss2)) {
		if (ss2->sa_family == AF_INET) {
			max_bits2 += 32;
		} else {
			max_bits2 += 128;
		}
	}
	return max_bits2 - max_bits1;
}

/*******************************************************************
 compare 2 ldap IPs by nearness to our interfaces - used in qsort
*******************************************************************/

int ip_service_compare(struct ip_service *ss1, struct ip_service *ss2)
{
	int result;

	if ((result = addr_compare((struct sockaddr *)&ss1->ss, (struct sockaddr *)&ss2->ss)) != 0) {
		return result;
	}

	if (ss1->port > ss2->port) {
		return 1;
	}

	if (ss1->port < ss2->port) {
		return -1;
	}

	return 0;
}

/*
  sort an IP list so that names that are close to one of our interfaces
  are at the top. This prevents the problem where a WINS server returns an IP
  that is not reachable from our subnet as the first match
*/

static void sort_addr_list(struct sockaddr_storage *sslist, int count)
{
	if (count <= 1) {
		return;
	}

	qsort(sslist, count, sizeof(struct sockaddr_storage),
			QSORT_CAST addr_compare);
}

static void sort_service_list(struct ip_service *servlist, int count)
{
	if (count <= 1) {
		return;
	}

	qsort(servlist, count, sizeof(struct ip_service),
			QSORT_CAST ip_service_compare);
}

/**********************************************************************
 Remove any duplicate address/port pairs in the list
 *********************************************************************/

static int remove_duplicate_addrs2(struct ip_service *iplist, int count )
{
	int i, j;

	DEBUG(10,("remove_duplicate_addrs2: "
			"looking for duplicate address/port pairs\n"));

	/* one loop to remove duplicates */
	for ( i=0; i<count; i++ ) {
		if ( is_zero_addr((struct sockaddr *)&iplist[i].ss)) {
			continue;
		}

		for ( j=i+1; j<count; j++ ) {
			if (sockaddr_equal((struct sockaddr *)&iplist[i].ss, (struct sockaddr *)&iplist[j].ss) &&
					iplist[i].port == iplist[j].port) {
				zero_sockaddr(&iplist[j].ss);
			}
		}
	}

	/* one loop to clean up any holes we left */
	/* first ip should never be a zero_ip() */
	for (i = 0; i<count; ) {
		if (is_zero_addr((struct sockaddr *)&iplist[i].ss) ) {
			if (i != count-1) {
				memmove(&iplist[i], &iplist[i+1],
					(count - i - 1)*sizeof(iplist[i]));
			}
			count--;
			continue;
		}
		i++;
	}

	return count;
}

static bool prioritize_ipv4_list(struct ip_service *iplist, int count)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct ip_service *iplist_new = TALLOC_ARRAY(frame, struct ip_service, count);
	int i, j;

	if (iplist_new == NULL) {
		TALLOC_FREE(frame);
		return false;
	}

	j = 0;

	/* Copy IPv4 first. */
	for (i = 0; i < count; i++) {
		if (iplist[i].ss.ss_family == AF_INET) {
			iplist_new[j++] = iplist[i];
		}
	}

	/* Copy IPv6. */
	for (i = 0; i < count; i++) {
		if (iplist[i].ss.ss_family != AF_INET) {
			iplist_new[j++] = iplist[i];
		}
	}

	memcpy(iplist, iplist_new, sizeof(struct ip_service)*count);
	TALLOC_FREE(frame);
	return true;
}

/****************************************************************************
 Do a netbios name query to find someones IP.
 Returns an array of IP addresses or NULL if none.
 *count will be set to the number of addresses returned.
 *timed_out is set if we failed by timing out
****************************************************************************/

struct sockaddr_storage *name_query(int fd,
			const char *name,
			int name_type,
			bool bcast,
			bool recurse,
			const struct sockaddr_storage *to_ss,
			int *count,
			int *flags,
			bool *timed_out)
{
	bool found=false;
	int i, retries = 3;
	int retry_time = bcast?250:2000;
	struct timeval tval;
	struct packet_struct p;
	struct packet_struct *p2;
	struct nmb_packet *nmb = &p.packet.nmb;
	struct sockaddr_storage *ss_list = NULL;

	if (lp_disable_netbios()) {
		DEBUG(5,("name_query(%s#%02x): netbios is disabled\n",
					name, name_type));
		return NULL;
	}

	if (to_ss->ss_family != AF_INET) {
		return NULL;
	}

	if (timed_out) {
		*timed_out = false;
	}

	memset((char *)&p,'\0',sizeof(p));
	(*count) = 0;
	(*flags) = 0;

	nmb->header.name_trn_id = generate_trn_id();
	nmb->header.opcode = 0;
	nmb->header.response = false;
	nmb->header.nm_flags.bcast = bcast;
	nmb->header.nm_flags.recursion_available = false;
	nmb->header.nm_flags.recursion_desired = recurse;
	nmb->header.nm_flags.trunc = false;
	nmb->header.nm_flags.authoritative = false;
	nmb->header.rcode = 0;
	nmb->header.qdcount = 1;
	nmb->header.ancount = 0;
	nmb->header.nscount = 0;
	nmb->header.arcount = 0;

	make_nmb_name(&nmb->question.question_name,name,name_type);

	nmb->question.question_type = 0x20;
	nmb->question.question_class = 0x1;

	p.ip = ((struct sockaddr_in *)to_ss)->sin_addr;
	p.port = NMB_PORT;
	p.recv_fd = -1;
	p.send_fd = fd;
	p.timestamp = time(NULL);
	p.packet_type = NMB_PACKET;

	GetTimeOfDay(&tval);

	if (!send_packet(&p))
		return NULL;

	retries--;

	while (1) {
		struct timeval tval2;

		GetTimeOfDay(&tval2);
		if (TvalDiff(&tval,&tval2) > retry_time) {
			if (!retries)
				break;
			if (!found && !send_packet(&p))
				return NULL;
			GetTimeOfDay(&tval);
			retries--;
		}

		if ((p2=receive_nmb_packet(fd,90,nmb->header.name_trn_id))) {
			struct nmb_packet *nmb2 = &p2->packet.nmb;
			debug_nmb_packet(p2);

			/* If we get a Negative Name Query Response from a WINS
			 * server, we should report it and give up.
			 */
			if( 0 == nmb2->header.opcode	/* A query response   */
			    && !(bcast)			/* from a WINS server */
			    && nmb2->header.rcode	/* Error returned     */
				) {

				if( DEBUGLVL( 3 ) ) {
					/* Only executed if DEBUGLEVEL >= 3 */
					dbgtext( "Negative name query "
						"response, rcode 0x%02x: ",
						nmb2->header.rcode );
					switch( nmb2->header.rcode ) {
					case 0x01:
						dbgtext( "Request "
						"was invalidly formatted.\n" );
						break;
					case 0x02:
						dbgtext( "Problem with NBNS, "
						"cannot process name.\n");
						break;
					case 0x03:
						dbgtext( "The name requested "
						"does not exist.\n" );
						break;
					case 0x04:
						dbgtext( "Unsupported request "
						"error.\n" );
						break;
					case 0x05:
						dbgtext( "Query refused "
						"error.\n" );
						break;
					default:
						dbgtext( "Unrecognized error "
						"code.\n" );
						break;
					}
				}
				free_packet(p2);
				return( NULL );
			}

			if (nmb2->header.opcode != 0 ||
			    nmb2->header.nm_flags.bcast ||
			    nmb2->header.rcode ||
			    !nmb2->header.ancount) {
				/*
				 * XXXX what do we do with this? Could be a
				 * redirect, but we'll discard it for the
				 * moment.
				 */
				free_packet(p2);
				continue;
			}

			ss_list = SMB_REALLOC_ARRAY(ss_list,
						struct sockaddr_storage,
						(*count) +
						nmb2->answers->rdlength/6);

			if (!ss_list) {
				DEBUG(0,("name_query: Realloc failed.\n"));
				free_packet(p2);
				return NULL;
			}

			DEBUG(2,("Got a positive name query response "
					"from %s ( ",
					inet_ntoa(p2->ip)));

			for (i=0;i<nmb2->answers->rdlength/6;i++) {
				struct in_addr ip;
				putip((char *)&ip,&nmb2->answers->rdata[2+i*6]);
				in_addr_to_sockaddr_storage(&ss_list[(*count)],
						ip);
				DEBUGADD(2,("%s ",inet_ntoa(ip)));
				(*count)++;
			}
			DEBUGADD(2,(")\n"));

			found=true;
			retries=0;
			/* We add the flags back ... */
			if (nmb2->header.response)
				(*flags) |= NM_FLAGS_RS;
			if (nmb2->header.nm_flags.authoritative)
				(*flags) |= NM_FLAGS_AA;
			if (nmb2->header.nm_flags.trunc)
				(*flags) |= NM_FLAGS_TC;
			if (nmb2->header.nm_flags.recursion_desired)
				(*flags) |= NM_FLAGS_RD;
			if (nmb2->header.nm_flags.recursion_available)
				(*flags) |= NM_FLAGS_RA;
			if (nmb2->header.nm_flags.bcast)
				(*flags) |= NM_FLAGS_B;
			free_packet(p2);
			/*
			 * If we're doing a unicast lookup we only
			 * expect one reply. Don't wait the full 2
			 * seconds if we got one. JRA.
			 */
			if(!bcast && found)
				break;
		}
	}

	/* only set timed_out if we didn't fund what we where looking for*/

	if ( !found && timed_out ) {
		*timed_out = true;
	}

	/* sort the ip list so we choose close servers first if possible */
	sort_addr_list(ss_list, *count);

	return ss_list;
}

/********************************************************
 convert an array if struct sockaddr_storage to struct ip_service
 return false on failure.  Port is set to PORT_NONE;
*********************************************************/

static bool convert_ss2service(struct ip_service **return_iplist,
		const struct sockaddr_storage *ss_list,
		int count)
{
	int i;

	if ( count==0 || !ss_list )
		return False;

	/* copy the ip address; port will be PORT_NONE */
	if ((*return_iplist = SMB_MALLOC_ARRAY(struct ip_service, count)) ==
			NULL) {
		DEBUG(0,("convert_ip2service: malloc failed "
			"for %d enetries!\n", count ));
		return False;
	}

	for ( i=0; i<count; i++ ) {
		(*return_iplist)[i].ss   = ss_list[i];
		(*return_iplist)[i].port = PORT_NONE;
	}

	return true;
}

/********************************************************
 Resolve via "bcast" method.
*********************************************************/

NTSTATUS name_resolve_bcast(const char *name,
			int name_type,
			struct ip_service **return_iplist,
			int *return_count)
{
	int sock, i;
	int num_interfaces = iface_count();
	struct sockaddr_storage *ss_list;
	struct sockaddr_storage ss;
	NTSTATUS status;

	if (lp_disable_netbios()) {
		DEBUG(5,("name_resolve_bcast(%s#%02x): netbios is disabled\n",
					name, name_type));

		return NT_STATUS_INVALID_PARAMETER;
	}

	*return_iplist = NULL;
	*return_count = 0;

	/*
	 * "bcast" means do a broadcast lookup on all the local interfaces.
	 */

	DEBUG(3,("name_resolve_bcast: Attempting broadcast lookup "
		"for name %s<0x%x>\n", name, name_type));


	fprintf(stderr, "JerryLin: namequery.c-->name_resolve_bcast: Attempting broadcast lookup "
		"for name %s<0x%x> num_interfaces=[%d]\n", name, name_type, num_interfaces);
	
	if (!interpret_string_addr(&ss, lp_socket_address(),
				AI_NUMERICHOST|AI_PASSIVE)) {
		zero_sockaddr(&ss);
	}

	sock = open_socket_in( SOCK_DGRAM, 0, 3, &ss, true );
	if (sock == -1) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	set_socket_options(sock,"SO_BROADCAST");
	/*
	 * Lookup the name on all the interfaces, return on
	 * the first successful match.
	 */
	for( i = num_interfaces-1; i >= 0; i--) {
		const struct sockaddr_storage *pss = iface_n_bcast(i);
		int flags;

		/* Done this way to fix compiler error on IRIX 5.x */
		if (!pss) {
			continue;
		}
		
		ss_list = name_query(sock, name, name_type, true,
				    true, pss, return_count, &flags, NULL);
		if (ss_list) {
			goto success;
		}
	}

	/* failed - no response */

	close(sock);
	
	return NT_STATUS_UNSUCCESSFUL;

success:

	status = NT_STATUS_OK;
	if (!convert_ss2service(return_iplist, ss_list, *return_count) )
		status = NT_STATUS_INVALID_PARAMETER;

	SAFE_FREE(ss_list);
	close(sock);
	return status;
}

/********************************************************
 Resolve via "wins" method.
*********************************************************/

NTSTATUS resolve_wins(const char *name,
		int name_type,
		struct ip_service **return_iplist,
		int *return_count)
{
	int sock, t, i;
	char **wins_tags;
	struct sockaddr_storage src_ss, *ss_list = NULL;
	struct in_addr src_ip;
	NTSTATUS status;

	if (lp_disable_netbios()) {
		DEBUG(5,("resolve_wins(%s#%02x): netbios is disabled\n",
					name, name_type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	*return_iplist = NULL;
	*return_count = 0;

	DEBUG(3,("resolve_wins: Attempting wins lookup for name %s<0x%x>\n",
				name, name_type));

	if (wins_srv_count() < 1) {
		DEBUG(3,("resolve_wins: WINS server resolution selected "
			"and no WINS servers listed.\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* we try a lookup on each of the WINS tags in turn */
	wins_tags = wins_srv_tags();

	if (!wins_tags) {
		/* huh? no tags?? give up in disgust */
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* the address we will be sending from */
	if (!interpret_string_addr(&src_ss, lp_socket_address(),
				AI_NUMERICHOST|AI_PASSIVE)) {
		zero_sockaddr(&src_ss);
	}

	if (src_ss.ss_family != AF_INET) {
		char addr[INET6_ADDRSTRLEN];
		print_sockaddr(addr, sizeof(addr), &src_ss);
		DEBUG(3,("resolve_wins: cannot receive WINS replies "
			"on IPv6 address %s\n",
			addr));
		wins_srv_tags_free(wins_tags);
		return NT_STATUS_INVALID_PARAMETER;
	}

	src_ip = ((struct sockaddr_in *)&src_ss)->sin_addr;

	/* in the worst case we will try every wins server with every
	   tag! */
	for (t=0; wins_tags && wins_tags[t]; t++) {
		int srv_count = wins_srv_count_tag(wins_tags[t]);
		for (i=0; i<srv_count; i++) {
			struct sockaddr_storage wins_ss;
			struct in_addr wins_ip;
			int flags;
			bool timed_out;

			wins_ip = wins_srv_ip_tag(wins_tags[t], src_ip);

			if (global_in_nmbd && ismyip_v4(wins_ip)) {
				/* yikes! we'll loop forever */
				continue;
			}

			/* skip any that have been unresponsive lately */
			if (wins_srv_is_dead(wins_ip, src_ip)) {
				continue;
			}

			DEBUG(3,("resolve_wins: using WINS server %s "
				"and tag '%s'\n",
				inet_ntoa(wins_ip), wins_tags[t]));

			sock = open_socket_in(SOCK_DGRAM, 0, 3, &src_ss, true);
			if (sock == -1) {
				continue;
			}

			in_addr_to_sockaddr_storage(&wins_ss, wins_ip);
			ss_list = name_query(sock,
						name,
						name_type,
						false,
						true,
						&wins_ss,
						return_count,
						&flags,
						&timed_out);

			/* exit loop if we got a list of addresses */

			if (ss_list)
				goto success;

			close(sock);

			if (timed_out) {
				/* Timed out wating for WINS server to respond.
				 * Mark it dead. */
				wins_srv_died(wins_ip, src_ip);
			} else {
				/* The name definately isn't in this
				   group of WINS servers.
				   goto the next group  */
				break;
			}
		}
	}

	wins_srv_tags_free(wins_tags);
	return NT_STATUS_NO_LOGON_SERVERS;

success:

	status = NT_STATUS_OK;
	if (!convert_ss2service(return_iplist, ss_list, *return_count))
		status = NT_STATUS_INVALID_PARAMETER;

	SAFE_FREE(ss_list);
	wins_srv_tags_free(wins_tags);
	close(sock);

	return status;
}

/********************************************************
 Resolve via "lmhosts" method.
*********************************************************/

static NTSTATUS resolve_lmhosts(const char *name, int name_type,
				struct ip_service **return_iplist,
				int *return_count)
{
	/*
	 * "lmhosts" means parse the local lmhosts file.
	 */

	XFILE *fp;
	char *lmhost_name = NULL;
	int name_type2;
	struct sockaddr_storage return_ss;
	NTSTATUS status = NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
	TALLOC_CTX *ctx = NULL;

	*return_iplist = NULL;
	*return_count = 0;

	DEBUG(3,("resolve_lmhosts: "
		"Attempting lmhosts lookup for name %s<0x%x>\n",
		name, name_type));

	fp = startlmhosts(get_dyn_LMHOSTSFILE());

	if ( fp == NULL )
		return NT_STATUS_NO_SUCH_FILE;

	ctx = talloc_init("resolve_lmhosts");
	if (!ctx) {
		endlmhosts(fp);
		return NT_STATUS_NO_MEMORY;
	}

	while (getlmhostsent(ctx, fp, &lmhost_name, &name_type2, &return_ss)) {

		if (!strequal(name, lmhost_name)) {
			TALLOC_FREE(lmhost_name);
			continue;
		}

		if ((name_type2 != -1) && (name_type != name_type2)) {
			TALLOC_FREE(lmhost_name);
			continue;
		}

		*return_iplist = SMB_REALLOC_ARRAY((*return_iplist),
					struct ip_service,
					(*return_count)+1);

		if ((*return_iplist) == NULL) {
			TALLOC_FREE(ctx);
			endlmhosts(fp);
			DEBUG(3,("resolve_lmhosts: malloc fail !\n"));
			return NT_STATUS_NO_MEMORY;
		}

		(*return_iplist)[*return_count].ss = return_ss;
		(*return_iplist)[*return_count].port = PORT_NONE;
		*return_count += 1;

		/* we found something */
		status = NT_STATUS_OK;

		/* Multiple names only for DC lookup */
		if (name_type != 0x1c)
			break;
	}

	TALLOC_FREE(ctx);
	endlmhosts(fp);
	return status;
}


/********************************************************
 Resolve via "hosts" method.
*********************************************************/

static NTSTATUS resolve_hosts(const char *name, int name_type,
			      struct ip_service **return_iplist,
			      int *return_count)
{
	/*
	 * "host" means do a localhost, or dns lookup.
	 */
	struct addrinfo hints;
	struct addrinfo *ailist = NULL;
	struct addrinfo *res = NULL;
	int ret = -1;
	int i = 0;

	if ( name_type != 0x20 && name_type != 0x0) {
		DEBUG(5, ("resolve_hosts: not appropriate "
			"for name type <0x%x>\n",
			name_type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	*return_iplist = NULL;
	*return_count = 0;

	DEBUG(3,("resolve_hosts: Attempting host lookup for name %s<0x%x>\n",
				name, name_type));

	ZERO_STRUCT(hints);
	/* By default make sure it supports TCP. */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG;

#if !defined(HAVE_IPV6)
	/* Unless we have IPv6, we really only want IPv4 addresses back. */
	hints.ai_family = AF_INET;
#endif

	ret = getaddrinfo(name,
			NULL,
			&hints,
			&ailist);
	if (ret) {
		DEBUG(3,("resolve_hosts: getaddrinfo failed for name %s [%s]\n",
			name,
			gai_strerror(ret) ));
	}

	for (res = ailist; res; res = res->ai_next) {
		struct sockaddr_storage ss;

		if (!res->ai_addr || res->ai_addrlen == 0) {
			continue;
		}

		ZERO_STRUCT(ss);
		memcpy(&ss, res->ai_addr, res->ai_addrlen);

		*return_count += 1;

		*return_iplist = SMB_REALLOC_ARRAY(*return_iplist,
						struct ip_service,
						*return_count);
		if (!*return_iplist) {
			DEBUG(3,("resolve_hosts: malloc fail !\n"));
			freeaddrinfo(ailist);
			return NT_STATUS_NO_MEMORY;
		}
		(*return_iplist)[i].ss = ss;
		(*return_iplist)[i].port = PORT_NONE;
		i++;
	}
	if (ailist) {
		freeaddrinfo(ailist);
	}
	if (*return_count) {
		return NT_STATUS_OK;
	}
	return NT_STATUS_UNSUCCESSFUL;
}

/********************************************************
 Resolve via "ADS" method.
*********************************************************/

static NTSTATUS resolve_ads(const char *name,
			    int name_type,
			    const char *sitename,
			    struct ip_service **return_iplist,
			    int *return_count)
{
	int 			i, j;
	NTSTATUS  		status;
	TALLOC_CTX		*ctx;
	struct dns_rr_srv	*dcs = NULL;
	int			numdcs = 0;
	int			numaddrs = 0;

	if ((name_type != 0x1c) && (name_type != KDC_NAME_TYPE) &&
	    (name_type != 0x1b)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if ( (ctx = talloc_init("resolve_ads")) == NULL ) {
		DEBUG(0,("resolve_ads: talloc_init() failed!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* The DNS code needs fixing to find IPv6 addresses... JRA. */

	switch (name_type) {
		case 0x1b:
			DEBUG(5,("resolve_ads: Attempting to resolve "
				 "PDC for %s using DNS\n", name));
			status = ads_dns_query_pdc(ctx, name, &dcs, &numdcs);
			break;

		case 0x1c:
			DEBUG(5,("resolve_ads: Attempting to resolve "
				 "DCs for %s using DNS\n", name));
			status = ads_dns_query_dcs(ctx, name, sitename, &dcs,
						   &numdcs);
			break;
		case KDC_NAME_TYPE:
			DEBUG(5,("resolve_ads: Attempting to resolve "
				 "KDCs for %s using DNS\n", name));
			status = ads_dns_query_kdcs(ctx, name, sitename, &dcs,
						    &numdcs);
			break;
		default:
			status = NT_STATUS_INVALID_PARAMETER;
			break;
	}

	if ( !NT_STATUS_IS_OK( status ) ) {
		talloc_destroy(ctx);
		return status;
	}

	for (i=0;i<numdcs;i++) {
		numaddrs += MAX(dcs[i].num_ips,1);
	}

	if ((*return_iplist = SMB_MALLOC_ARRAY(struct ip_service, numaddrs)) ==
			NULL ) {
		DEBUG(0,("resolve_ads: malloc failed for %d entries\n",
					numaddrs ));
		talloc_destroy(ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/* now unroll the list of IP addresses */

	*return_count = 0;
	i = 0;
	j = 0;
	while ( i < numdcs && (*return_count<numaddrs) ) {
		struct ip_service *r = &(*return_iplist)[*return_count];

		r->port = dcs[i].port;

		/* If we don't have an IP list for a name, lookup it up */

		if (!dcs[i].ss_s) {
			interpret_string_addr(&r->ss, dcs[i].hostname, 0);
			i++;
			j = 0;
		} else {
			/* use the IP addresses from the SRV sresponse */

			if ( j >= dcs[i].num_ips ) {
				i++;
				j = 0;
				continue;
			}

			r->ss = dcs[i].ss_s[j];
			j++;
		}

		/* make sure it is a valid IP.  I considered checking the
		 * negative connection cache, but this is the wrong place
		 * for it. Maybe only as a hack. After think about it, if
		 * all of the IP addresses returned from DNS are dead, what
		 * hope does a netbios name lookup have ? The standard reason
		 * for falling back to netbios lookups is that our DNS server
		 * doesn't know anything about the DC's   -- jerry */

		if (!is_zero_addr((struct sockaddr *)&r->ss)) {
			(*return_count)++;
		}
	}

	talloc_destroy(ctx);
	return NT_STATUS_OK;
}

/*******************************************************************
 Internal interface to resolve a name into an IP address.
 Use this function if the string is either an IP address, DNS
 or host name or NetBIOS name. This uses the name switch in the
 smb.conf to determine the order of name resolution.

 Added support for ip addr/port to support ADS ldap servers.
 the only place we currently care about the port is in the
 resolve_hosts() when looking up DC's via SRV RR entries in DNS
**********************************************************************/

NTSTATUS internal_resolve_name(const char *name,
			        int name_type,
				const char *sitename,
				struct ip_service **return_iplist,
				int *return_count,
				const char *resolve_order)
{
	char *tok;
	const char *ptr;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	int i;
	TALLOC_CTX *frame = NULL;

	*return_iplist = NULL;
	*return_count = 0;

	DEBUG(10, ("internal_resolve_name: looking up %s#%x (sitename %s)\n",
			name, name_type, sitename ? sitename : "(null)"));
	
	if (is_ipaddress(name)) {
		if ((*return_iplist = SMB_MALLOC_P(struct ip_service)) ==
				NULL) {
			DEBUG(0,("internal_resolve_name: malloc fail !\n"));
			return NT_STATUS_NO_MEMORY;
		}

		/* ignore the port here */
		(*return_iplist)->port = PORT_NONE;

		/* if it's in the form of an IP address then get the lib to interpret it */
		if (!interpret_string_addr(&(*return_iplist)->ss,
					name, AI_NUMERICHOST)) {
			DEBUG(1,("internal_resolve_name: interpret_string_addr "
				"failed on %s\n",
				name));
			SAFE_FREE(*return_iplist);
			return NT_STATUS_INVALID_PARAMETER;
		}
		*return_count = 1;
		return NT_STATUS_OK;
	}

	/* Check name cache */

	if (namecache_fetch(name, name_type, return_iplist, return_count)) {
		/* This could be a negative response */
		if (*return_count > 0) {
			return NT_STATUS_OK;
		} else {
			return NT_STATUS_UNSUCCESSFUL;
		}
	}

	/* set the name resolution order */

	if (strcmp( resolve_order, "NULL") == 0) {
		DEBUG(8,("internal_resolve_name: all lookups disabled\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!resolve_order[0]) {
		ptr = "host";
	} else {
		ptr = resolve_order;
	}

	/* iterate through the name resolution backends */

	frame = talloc_stackframe();
	while (next_token_talloc(frame, &ptr, &tok, LIST_SEP)) {		
		if((strequal(tok, "host") || strequal(tok, "hosts"))) {
			status = resolve_hosts(name, name_type, return_iplist,
					       return_count);
			if (NT_STATUS_IS_OK(status)) {
				goto done;
			}
		} else if(strequal( tok, "kdc")) {
			/* deal with KDC_NAME_TYPE names here.
			 * This will result in a SRV record lookup */
			status = resolve_ads(name, KDC_NAME_TYPE, sitename,
					     return_iplist, return_count);
			if (NT_STATUS_IS_OK(status)) {
				/* Ensure we don't namecache
				 * this with the KDC port. */
				name_type = KDC_NAME_TYPE;
				goto done;
			}
		} else if(strequal( tok, "ads")) {
			/* deal with 0x1c and 0x1b names here.
			 * This will result in a SRV record lookup */
			status = resolve_ads(name, name_type, sitename,
					     return_iplist, return_count);
			if (NT_STATUS_IS_OK(status)) {
				goto done;
			}
		} else if(strequal( tok, "lmhosts")) {
			status = resolve_lmhosts(name, name_type,
						 return_iplist, return_count);
			if (NT_STATUS_IS_OK(status)) {
				goto done;
			}
		} else if(strequal( tok, "wins")) {
			/* don't resolve 1D via WINS */
			if (name_type != 0x1D) {
				status = resolve_wins(name, name_type,
						      return_iplist,
						      return_count);
				if (NT_STATUS_IS_OK(status)) {
					goto done;
				}
			}
		} else if(strequal( tok, "bcast")) {			
			status = name_resolve_bcast(name, name_type,
						    return_iplist,
						    return_count);
			if (NT_STATUS_IS_OK(status)) {
				goto done;
			}
		} else {
			DEBUG(0,("resolve_name: unknown name switch type %s\n",
				tok));
		}
	}

	/* All of the resolve_* functions above have returned false. */

	TALLOC_FREE(frame);
	SAFE_FREE(*return_iplist);
	*return_count = 0;

	return NT_STATUS_UNSUCCESSFUL;

  done:

	/* Remove duplicate entries.  Some queries, notably #1c (domain
	controllers) return the PDC in iplist[0] and then all domain
	controllers including the PDC in iplist[1..n].  Iterating over
	the iplist when the PDC is down will cause two sets of timeouts. */

	if ( *return_count ) {
		*return_count = remove_duplicate_addrs2(*return_iplist,
					*return_count );
	}

	/* Save in name cache */
	if ( DEBUGLEVEL >= 100 ) {
		for (i = 0; i < *return_count && DEBUGLEVEL == 100; i++) {
			char addr[INET6_ADDRSTRLEN];
			print_sockaddr(addr, sizeof(addr),
					&(*return_iplist)[i].ss);
			DEBUG(100, ("Storing name %s of type %d (%s:%d)\n",
					name,
					name_type,
					addr,
					(*return_iplist)[i].port));
		}
	}

	namecache_store(name, name_type, *return_count, *return_iplist);

	/* Display some debugging info */

	if ( DEBUGLEVEL >= 10 ) {
		DEBUG(10, ("internal_resolve_name: returning %d addresses: ",
					*return_count));

		for (i = 0; i < *return_count; i++) {
			char addr[INET6_ADDRSTRLEN];
			print_sockaddr(addr, sizeof(addr),
					&(*return_iplist)[i].ss);
			DEBUGADD(10, ("%s:%d ",
					addr,
					(*return_iplist)[i].port));
		}
		DEBUG(10, ("\n"));
	}

	TALLOC_FREE(frame);
	return status;
}

/********************************************************
 Internal interface to resolve a name into one IP address.
 Use this function if the string is either an IP address, DNS
 or host name or NetBIOS name. This uses the name switch in the
 smb.conf to determine the order of name resolution.
*********************************************************/

bool resolve_name(const char *name,
		struct sockaddr_storage *return_ss,
		int name_type,
		bool prefer_ipv4)
{
	struct ip_service *ss_list = NULL;
	char *sitename = NULL;
	int count = 0;

	if (is_ipaddress(name)) {
		return interpret_string_addr(return_ss, name, AI_NUMERICHOST);
	}

	sitename = sitename_fetch(lp_realm()); /* wild guess */

	if (NT_STATUS_IS_OK(internal_resolve_name(name, name_type, sitename,
						  &ss_list, &count,
						  lp_name_resolve_order()))) {
		int i;

		if (prefer_ipv4) {
			for (i=0; i<count; i++) {
				if (!is_zero_addr((struct sockaddr *)&ss_list[i].ss) &&
						!is_broadcast_addr((struct sockaddr *)&ss_list[i].ss) &&
						(ss_list[i].ss.ss_family == AF_INET)) {
					*return_ss = ss_list[i].ss;
					SAFE_FREE(ss_list);
					SAFE_FREE(sitename);
					return True;
				}
			}
		}

		/* only return valid addresses for TCP connections */
		for (i=0; i<count; i++) {
			if (!is_zero_addr((struct sockaddr *)&ss_list[i].ss) &&
					!is_broadcast_addr((struct sockaddr *)&ss_list[i].ss)) {
				*return_ss = ss_list[i].ss;
				SAFE_FREE(ss_list);
				SAFE_FREE(sitename);
				return True;
			}
		}
	}

	SAFE_FREE(ss_list);
	SAFE_FREE(sitename);
	return False;
}

/********************************************************
 Internal interface to resolve a name into a list of IP addresses.
 Use this function if the string is either an IP address, DNS
 or host name or NetBIOS name. This uses the name switch in the
 smb.conf to determine the order of name resolution.
*********************************************************/

NTSTATUS resolve_name_list(TALLOC_CTX *ctx,
		const char *name,
		int name_type,
		struct sockaddr_storage **return_ss_arr,
		unsigned int *p_num_entries)
{
	struct ip_service *ss_list = NULL;
	char *sitename = NULL;
	int count = 0;
	int i;
	unsigned int num_entries;
	NTSTATUS status;

	*p_num_entries = 0;
	*return_ss_arr = NULL;

	if (is_ipaddress(name)) {
		*return_ss_arr = TALLOC_P(ctx, struct sockaddr_storage);
		if (!*return_ss_arr) {
			return NT_STATUS_NO_MEMORY;
		}
		if (!interpret_string_addr(*return_ss_arr, name, AI_NUMERICHOST)) {
			TALLOC_FREE(*return_ss_arr);
			return NT_STATUS_BAD_NETWORK_NAME;
		}
		*p_num_entries = 1;
		return NT_STATUS_OK;
	}

	sitename = sitename_fetch(lp_realm()); /* wild guess */

	status = internal_resolve_name(name, name_type, sitename,
						  &ss_list, &count,
						  lp_name_resolve_order());
	SAFE_FREE(sitename);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* only return valid addresses for TCP connections */
	for (i=0, num_entries = 0; i<count; i++) {
		if (!is_zero_addr((struct sockaddr *)&ss_list[i].ss) &&
				!is_broadcast_addr((struct sockaddr *)&ss_list[i].ss)) {
			num_entries++;
		}
	}
	if (num_entries == 0) {
		SAFE_FREE(ss_list);
		return NT_STATUS_BAD_NETWORK_NAME;
	}

	*return_ss_arr = TALLOC_ARRAY(ctx,
				struct sockaddr_storage,
				num_entries);
	if (!(*return_ss_arr)) {
		SAFE_FREE(ss_list);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0, num_entries = 0; i<count; i++) {
		if (!is_zero_addr((struct sockaddr *)&ss_list[i].ss) &&
				!is_broadcast_addr((struct sockaddr *)&ss_list[i].ss)) {
			(*return_ss_arr)[num_entries++] = ss_list[i].ss;
		}
	}

	status = NT_STATUS_OK;
	*p_num_entries = num_entries;

	SAFE_FREE(ss_list);
	return NT_STATUS_OK;
}

/********************************************************
 Find the IP address of the master browser or DMB for a workgroup.
*********************************************************/

bool find_master_ip(const char *group, struct sockaddr_storage *master_ss)
{
	struct ip_service *ip_list = NULL;
	int count = 0;
	NTSTATUS status;

	if (lp_disable_netbios()) {
		DEBUG(5,("find_master_ip(%s): netbios is disabled\n", group));
		return false;
	}

	status = internal_resolve_name(group, 0x1D, NULL, &ip_list, &count,
				       lp_name_resolve_order());
	if (NT_STATUS_IS_OK(status)) {
		*master_ss = ip_list[0].ss;
		SAFE_FREE(ip_list);
		return true;
	}

	status = internal_resolve_name(group, 0x1B, NULL, &ip_list, &count,
				       lp_name_resolve_order());
	if (NT_STATUS_IS_OK(status)) {
		*master_ss = ip_list[0].ss;
		SAFE_FREE(ip_list);
		return true;
	}

	SAFE_FREE(ip_list);
	return false;
}

/********************************************************
 Get the IP address list of the primary domain controller
 for a domain.
*********************************************************/

bool get_pdc_ip(const char *domain, struct sockaddr_storage *pss)
{
	struct ip_service *ip_list = NULL;
	int count = 0;
	NTSTATUS status = NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;

	/* Look up #1B name */

	if (lp_security() == SEC_ADS) {
		status = internal_resolve_name(domain, 0x1b, NULL, &ip_list,
					       &count, "ads");
	}

	if (!NT_STATUS_IS_OK(status) || count == 0) {
		status = internal_resolve_name(domain, 0x1b, NULL, &ip_list,
					       &count,
					       lp_name_resolve_order());
		if (!NT_STATUS_IS_OK(status)) {
			return false;
		}
	}

	/* if we get more than 1 IP back we have to assume it is a
	   multi-homed PDC and not a mess up */

	if ( count > 1 ) {
		DEBUG(6,("get_pdc_ip: PDC has %d IP addresses!\n", count));
		sort_service_list(ip_list, count);
	}

	*pss = ip_list[0].ss;
	SAFE_FREE(ip_list);
	return true;
}

/* Private enum type for lookups. */

enum dc_lookup_type { DC_NORMAL_LOOKUP, DC_ADS_ONLY, DC_KDC_ONLY };

/********************************************************
 Get the IP address list of the domain controllers for
 a domain.
*********************************************************/

static NTSTATUS get_dc_list(const char *domain,
			const char *sitename,
			struct ip_service **ip_list,
			int *count,
			enum dc_lookup_type lookup_type,
			bool *ordered)
{
	char *resolve_order = NULL;
	char *saf_servername = NULL;
	char *pserver = NULL;
	const char *p;
	char *port_str = NULL;
	int port;
	char *name;
	int num_addresses = 0;
	int  local_count, i, j;
	struct ip_service *return_iplist = NULL;
	struct ip_service *auto_ip_list = NULL;
	bool done_auto_lookup = false;
	int auto_count = 0;
	NTSTATUS status;
	TALLOC_CTX *ctx = talloc_init("get_dc_list");

	*ip_list = NULL;
	*count = 0;

	if (!ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	*ordered = False;

	/* if we are restricted to solely using DNS for looking
	   up a domain controller, make sure that host lookups
	   are enabled for the 'name resolve order'.  If host lookups
	   are disabled and ads_only is True, then set the string to
	   NULL. */

	resolve_order = talloc_strdup(ctx, lp_name_resolve_order());
	if (!resolve_order) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}
	strlower_m(resolve_order);
	if (lookup_type == DC_ADS_ONLY)  {
		if (strstr( resolve_order, "host")) {
			resolve_order = talloc_strdup(ctx, "ads");

			/* DNS SRV lookups used by the ads resolver
			   are already sorted by priority and weight */
			*ordered = true;
		} else {
                        resolve_order = talloc_strdup(ctx, "NULL");
		}
	} else if (lookup_type == DC_KDC_ONLY) {
		/* DNS SRV lookups used by the ads/kdc resolver
		   are already sorted by priority and weight */
		*ordered = true;
		resolve_order = talloc_strdup(ctx, "kdc");
	}
	if (!resolve_order) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	/* fetch the server we have affinity for.  Add the
	   'password server' list to a search for our domain controllers */

	saf_servername = saf_fetch( domain);

	if (strequal(domain, lp_workgroup()) || strequal(domain, lp_realm())) {
		pserver = talloc_asprintf(NULL, "%s, %s",
			saf_servername ? saf_servername : "",
			lp_passwordserver());
	} else {
		pserver = talloc_asprintf(NULL, "%s, *",
			saf_servername ? saf_servername : "");
	}

	SAFE_FREE(saf_servername);
	if (!pserver) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	/* if we are starting from scratch, just lookup DOMAIN<0x1c> */

	if (!*pserver ) {
		DEBUG(10,("get_dc_list: no preferred domain controllers.\n"));
		status = internal_resolve_name(domain, 0x1C, sitename, ip_list,
					     count, resolve_order);
		goto out;
	}

	DEBUG(3,("get_dc_list: preferred server list: \"%s\"\n", pserver ));

	/*
	 * if '*' appears in the "password server" list then add
	 * an auto lookup to the list of manually configured
	 * DC's.  If any DC is listed by name, then the list should be
	 * considered to be ordered
	 */

	p = pserver;
	while (next_token_talloc(ctx, &p, &name, LIST_SEP)) {
		if (!done_auto_lookup && strequal(name, "*")) {
			status = internal_resolve_name(domain, 0x1C, sitename,
						       &auto_ip_list,
						       &auto_count,
						       resolve_order);
			if (NT_STATUS_IS_OK(status)) {
				num_addresses += auto_count;
			}
			done_auto_lookup = true;
			DEBUG(8,("Adding %d DC's from auto lookup\n",
						auto_count));
		} else  {
			num_addresses++;
		}
	}

	/* if we have no addresses and haven't done the auto lookup, then
	   just return the list of DC's.  Or maybe we just failed. */

	if ((num_addresses == 0)) {
		if (done_auto_lookup) {
			DEBUG(4,("get_dc_list: no servers found\n"));
			status = NT_STATUS_NO_LOGON_SERVERS;
			goto out;
		}
		status = internal_resolve_name(domain, 0x1C, sitename, ip_list,
					     count, resolve_order);
		goto out;
	}

	if ((return_iplist = SMB_MALLOC_ARRAY(struct ip_service,
					num_addresses)) == NULL) {
		DEBUG(3,("get_dc_list: malloc fail !\n"));
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	p = pserver;
	local_count = 0;

	/* fill in the return list now with real IP's */

	while ((local_count<num_addresses) &&
			next_token_talloc(ctx, &p, &name, LIST_SEP)) {
		struct sockaddr_storage name_ss;

		/* copy any addersses from the auto lookup */

		if (strequal(name, "*")) {
			for (j=0; j<auto_count; j++) {
				char addr[INET6_ADDRSTRLEN];
				print_sockaddr(addr,
						sizeof(addr),
						&auto_ip_list[j].ss);
				/* Check for and don't copy any
				 * known bad DC IP's. */
				if(!NT_STATUS_IS_OK(check_negative_conn_cache(
						domain,
						addr))) {
					DEBUG(5,("get_dc_list: "
						"negative entry %s removed "
						"from DC list\n",
						addr));
					continue;
				}
				return_iplist[local_count].ss =
					auto_ip_list[j].ss;
				return_iplist[local_count].port =
					auto_ip_list[j].port;
				local_count++;
			}
			continue;
		}

		/* added support for address:port syntax for ads
		 * (not that I think anyone will ever run the LDAP
		 * server in an AD domain on something other than
		 * port 389 */

		port = (lp_security() == SEC_ADS) ? LDAP_PORT : PORT_NONE;
		if ((port_str=strchr(name, ':')) != NULL) {
			*port_str = '\0';
			port_str++;
			port = atoi(port_str);
		}

		/* explicit lookup; resolve_name() will
		 * handle names & IP addresses */
		if (resolve_name( name, &name_ss, 0x20, true )) {
			char addr[INET6_ADDRSTRLEN];
			print_sockaddr(addr,
					sizeof(addr),
					&name_ss);

			/* Check for and don't copy any known bad DC IP's. */
			if( !NT_STATUS_IS_OK(check_negative_conn_cache(domain,
							addr)) ) {
				DEBUG(5,("get_dc_list: negative entry %s "
					"removed from DC list\n",
					name ));
				continue;
			}

			return_iplist[local_count].ss = name_ss;
			return_iplist[local_count].port = port;
			local_count++;
			*ordered = true;
		}
	}

	/* need to remove duplicates in the list if we have any
	   explicit password servers */

	if (local_count) {
		local_count = remove_duplicate_addrs2(return_iplist,
				local_count );
	}

	/* For DC's we always prioritize IPv4 due to W2K3 not
	 * supporting LDAP, KRB5 or CLDAP over IPv6. */

	if (local_count && return_iplist) {
		prioritize_ipv4_list(return_iplist, local_count);
	}

	if ( DEBUGLEVEL >= 4 ) {
		DEBUG(4,("get_dc_list: returning %d ip addresses "
				"in an %sordered list\n",
				local_count,
				*ordered ? "":"un"));
		DEBUG(4,("get_dc_list: "));
		for ( i=0; i<local_count; i++ ) {
			char addr[INET6_ADDRSTRLEN];
			print_sockaddr(addr,
					sizeof(addr),
					&return_iplist[i].ss);
			DEBUGADD(4,("%s:%d ", addr, return_iplist[i].port ));
		}
		DEBUGADD(4,("\n"));
	}

	*ip_list = return_iplist;
	*count = local_count;

	status = ( *count != 0 ? NT_STATUS_OK : NT_STATUS_NO_LOGON_SERVERS );

  out:

	if (!NT_STATUS_IS_OK(status)) {
		SAFE_FREE(return_iplist);
		*ip_list = NULL;
		*count = 0;
	}

	SAFE_FREE(auto_ip_list);
	TALLOC_FREE(ctx);
	return status;
}

/*********************************************************************
 Small wrapper function to get the DC list and sort it if neccessary.
*********************************************************************/

NTSTATUS get_sorted_dc_list( const char *domain,
			const char *sitename,
			struct ip_service **ip_list,
			int *count,
			bool ads_only )
{
	bool ordered = false;
	NTSTATUS status;
	enum dc_lookup_type lookup_type = DC_NORMAL_LOOKUP;

	*ip_list = NULL;
	*count = 0;

	DEBUG(8,("get_sorted_dc_list: attempting lookup "
		"for name %s (sitename %s) using [%s]\n",
		domain,
		sitename ? sitename : "NULL",
		(ads_only ? "ads" : lp_name_resolve_order())));

	if (ads_only) {
		lookup_type = DC_ADS_ONLY;
	}

	status = get_dc_list(domain, sitename, ip_list,
			count, lookup_type, &ordered);
	if (NT_STATUS_EQUAL(status, NT_STATUS_NO_LOGON_SERVERS)
	    && sitename) {
		DEBUG(3,("get_sorted_dc_list: no server for name %s available"
			 " in site %s, fallback to all servers\n",
			 domain, sitename));
		status = get_dc_list(domain, NULL, ip_list,
				     count, lookup_type, &ordered);
	}

	if (!NT_STATUS_IS_OK(status)) {
		SAFE_FREE(*ip_list);
		*count = 0;
		return status;
	}

	/* only sort if we don't already have an ordered list */
	if (!ordered) {
		sort_service_list(*ip_list, *count);
	}

	return NT_STATUS_OK;
}

/*********************************************************************
 Get the KDC list - re-use all the logic in get_dc_list.
*********************************************************************/

NTSTATUS get_kdc_list( const char *realm,
			const char *sitename,
			struct ip_service **ip_list,
			int *count)
{
	bool ordered;
	NTSTATUS status;

	*count = 0;
	*ip_list = NULL;

	status = get_dc_list(realm, sitename, ip_list,
			count, DC_KDC_ONLY, &ordered);

	if (!NT_STATUS_IS_OK(status)) {
		SAFE_FREE(*ip_list);
		*count = 0;
		return status;
	}

	/* only sort if we don't already have an ordered list */
	if ( !ordered ) {
		sort_service_list(*ip_list, *count);
	}

	return NT_STATUS_OK;
}
