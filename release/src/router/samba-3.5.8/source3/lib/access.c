/*
   This module is an adaption of code from the tcpd-1.4 package written
   by Wietse Venema, Eindhoven University of Technology, The Netherlands.

   The code is used here with permission.

   The code has been considerably changed from the original. Bug reports
   should be sent to samba@samba.org

   Updated for IPv6 by Jeremy Allison (C) 2007.
*/

#include "includes.h"

#define NAME_INDEX 0
#define ADDR_INDEX 1

/* masked_match - match address against netnumber/netmask */
static bool masked_match(const char *tok, const char *slash, const char *s)
{
	struct sockaddr_storage ss_mask;
	struct sockaddr_storage ss_tok;
	struct sockaddr_storage ss_host;
	char *tok_copy = NULL;

	if (!interpret_string_addr(&ss_host, s, 0)) {
		return false;
	}

	if (*tok == '[') {
		/* IPv6 address - remove braces. */
		tok_copy = SMB_STRDUP(tok+1);
		if (!tok_copy) {
			return false;
		}
		/* Remove the terminating ']' */
		tok_copy[PTR_DIFF(slash,tok)-1] = '\0';
	} else {
		tok_copy = SMB_STRDUP(tok);
		if (!tok_copy) {
			return false;
		}
		/* Remove the terminating '/' */
		tok_copy[PTR_DIFF(slash,tok)] = '\0';
	}

	if (!interpret_string_addr(&ss_tok, tok_copy, 0)) {
		SAFE_FREE(tok_copy);
		return false;
	}

	SAFE_FREE(tok_copy);

        if (strlen(slash + 1) > 2) {
		if (!interpret_string_addr(&ss_mask, slash+1, 0)) {
			return false;
		}
        } else {
		char *endp = NULL;
		unsigned long val = strtoul(slash+1, &endp, 0);
		if (slash+1 == endp || (endp && *endp != '\0')) {
			return false;
		}
		if (!make_netmask(&ss_mask, &ss_tok, val)) {
			return false;
		}
        }

	return same_net((struct sockaddr *)&ss_host, (struct sockaddr *)&ss_tok, (struct sockaddr *)&ss_mask);
}

/* string_match - match string s against token tok */
static bool string_match(const char *tok,const char *s)
{
	size_t     tok_len;
	size_t     str_len;
	const char   *cut;

	/* Return true if a token has the magic value "ALL". Return
	 * true if the token is "FAIL". If the token starts with a "."
	 * (domain name), return true if it matches the last fields of
	 * the string. If the token has the magic value "LOCAL",
	 * return true if the string does not contain a "."
	 * character. If the token ends on a "." (network number),
	 * return true if it matches the first fields of the
	 * string. If the token begins with a "@" (netgroup name),
	 * return true if the string is a (host) member of the
	 * netgroup. Return true if the token fully matches the
	 * string. If the token is a netnumber/netmask pair, return
	 * true if the address is a member of the specified subnet.
	 */

	if (tok[0] == '.') {			/* domain: match last fields */
		if ((str_len = strlen(s)) > (tok_len = strlen(tok))
		    && strequal(tok, s + str_len - tok_len)) {
			return true;
		}
	} else if (tok[0] == '@') { /* netgroup: look it up */
#ifdef	HAVE_NETGROUP
		DATA_BLOB tmp;
		char *mydomain = NULL;
		char *hostname = NULL;
		bool netgroup_ok = false;

		if (memcache_lookup(
			    NULL, SINGLETON_CACHE,
			    data_blob_string_const_null("yp_default_domain"),
			    &tmp)) {

			SMB_ASSERT(tmp.length > 0);
			mydomain = (tmp.data[0] == '\0')
				? NULL : (char *)tmp.data;
		}
		else {
			yp_get_default_domain(&mydomain);

			memcache_add(
				NULL, SINGLETON_CACHE,
				data_blob_string_const_null("yp_default_domain"),
				data_blob_string_const_null(mydomain?mydomain:""));
		}

		if (!mydomain) {
			DEBUG(0,("Unable to get default yp domain. "
				"Try without it.\n"));
		}
		if (!(hostname = SMB_STRDUP(s))) {
			DEBUG(1,("out of memory for strdup!\n"));
			return false;
		}

		netgroup_ok = innetgr(tok + 1, hostname, (char *) 0, mydomain);

		DEBUG(5,("looking for %s of domain %s in netgroup %s gave %s\n",
			 hostname,
			 mydomain?mydomain:"(ANY)",
			 tok+1,
			 BOOLSTR(netgroup_ok)));

		SAFE_FREE(hostname);

		if (netgroup_ok)
			return true;
#else
		DEBUG(0,("access: netgroup support is not configured\n"));
		return false;
#endif
	} else if (strequal(tok, "ALL")) {	/* all: match any */
		return true;
	} else if (strequal(tok, "FAIL")) {	/* fail: match any */
		return true;
	} else if (strequal(tok, "LOCAL")) {	/* local: no dots */
		if (strchr_m(s, '.') == 0 && !strequal(s, "unknown")) {
			return true;
		}
	} else if (strequal(tok, s)) {   /* match host name or address */
		return true;
	} else if (tok[(tok_len = strlen(tok)) - 1] == '.') {	/* network */
		if (strncmp(tok, s, tok_len) == 0) {
			return true;
		}
	} else if ((cut = strchr_m(tok, '/')) != 0) {	/* netnumber/netmask */
		if ((isdigit(s[0]) && strchr_m(tok, '.') != NULL) ||
			(tok[0] == '[' && cut > tok && cut[-1] == ']') ||
			((isxdigit(s[0]) || s[0] == ':') &&
				strchr_m(tok, ':') != NULL)) {
			/* IPv4/netmask or
			 * [IPv6:addr]/netmask or IPv6:addr/netmask */
			return masked_match(tok, cut, s);
		}
	} else if (strchr_m(tok, '*') != 0 || strchr_m(tok, '?')) {
		return unix_wild_match(tok, s);
	}
	return false;
}

/* client_match - match host name and address against token */
bool client_match(const char *tok, const void *item)
{
	const char **client = (const char **)item;

	/*
	 * Try to match the address first. If that fails, try to match the host
	 * name if available.
	 */

	if (string_match(tok, client[ADDR_INDEX])) {
		return true;
	}

	if (strnequal(client[ADDR_INDEX],"::ffff:",7) &&
			!strnequal(tok, "::ffff:",7)) {
		/* client[ADDR_INDEX] is an IPv4 mapped to IPv6, but
 		 * the list item is not. Try and match the IPv4 part of
 		 * address only. This will happen a lot on IPv6 enabled
 		 * systems with IPv4 allow/deny lists in smb.conf.
 		 * Bug #5311. JRA.
 		 */
		if (string_match(tok, (client[ADDR_INDEX])+7)) {
			return true;
		}
	}

	if (client[NAME_INDEX][0] != 0) {
		if (string_match(tok, client[NAME_INDEX])) {
			return true;
		}
	}

	return false;
}

/* list_match - match an item against a list of tokens with exceptions */
bool list_match(const char **list,const void *item,
		bool (*match_fn)(const char *, const void *))
{
	bool match = false;

	if (!list) {
		return false;
	}

	/*
	 * Process tokens one at a time. We have exhausted all possible matches
	 * when we reach an "EXCEPT" token or the end of the list. If we do find
	 * a match, look for an "EXCEPT" list and recurse to determine whether
	 * the match is affected by any exceptions.
	 */

	for (; *list ; list++) {
		if (strequal(*list, "EXCEPT")) {
			/* EXCEPT: give up */
			break;
		}
		if ((match = (*match_fn) (*list, item))) {
			/* true or FAIL */
			break;
		}
	}
	/* Process exceptions to true or FAIL matches. */

	if (match != false) {
		while (*list  && !strequal(*list, "EXCEPT")) {
			list++;
		}

		for (; *list; list++) {
			if ((*match_fn) (*list, item)) {
				/* Exception Found */
				return false;
			}
		}
	}

	return match;
}

/* return true if access should be allowed */
static bool allow_access_internal(const char **deny_list,
				const char **allow_list,
				const char *cname,
				const char *caddr)
{
	const char *client[2];

	client[NAME_INDEX] = cname;
	client[ADDR_INDEX] = caddr;

	/* if it is loopback then always allow unless specifically denied */
	if (strcmp(caddr, "127.0.0.1") == 0 || strcmp(caddr, "::1") == 0) {
		/*
		 * If 127.0.0.1 matches both allow and deny then allow.
		 * Patch from Steve Langasek vorlon@netexpress.net.
		 */
		if (deny_list &&
			list_match(deny_list,client,client_match) &&
				(!allow_list ||
				!list_match(allow_list,client, client_match))) {
			return false;
		}
		return true;
	}

	/* if theres no deny list and no allow list then allow access */
	if ((!deny_list || *deny_list == 0) &&
	    (!allow_list || *allow_list == 0)) {
		return true;
	}

	/* if there is an allow list but no deny list then allow only hosts
	   on the allow list */
	if (!deny_list || *deny_list == 0) {
		return(list_match(allow_list,client,client_match));
	}

	/* if theres a deny list but no allow list then allow
	   all hosts not on the deny list */
	if (!allow_list || *allow_list == 0) {
		return(!list_match(deny_list,client,client_match));
	}

	/* if there are both types of list then allow all hosts on the
           allow list */
	if (list_match(allow_list,(const char *)client,client_match)) {
		return true;
	}

	/* if there are both types of list and it's not on the allow then
	   allow it if its not on the deny */
	if (list_match(deny_list,(const char *)client,client_match)) {
		return false;
	}

	return true;
}

/* return true if access should be allowed */
bool allow_access(const char **deny_list,
		const char **allow_list,
		const char *cname,
		const char *caddr)
{
	bool ret;
	char *nc_cname = smb_xstrdup(cname);
	char *nc_caddr = smb_xstrdup(caddr);

	ret = allow_access_internal(deny_list, allow_list, nc_cname, nc_caddr);

	SAFE_FREE(nc_cname);
	SAFE_FREE(nc_caddr);
	return ret;
}

/* return true if the char* contains ip addrs only.  Used to avoid
name lookup calls */

static bool only_ipaddrs_in_list(const char **list)
{
	bool only_ip = true;

	if (!list) {
		return true;
	}

	for (; *list ; list++) {
		/* factor out the special strings */
		if (strequal(*list, "ALL") || strequal(*list, "FAIL") ||
		    strequal(*list, "EXCEPT")) {
			continue;
		}

		if (!is_ipaddress(*list)) {
			/*
			 * If we failed, make sure that it was not because
			 * the token was a network/netmask pair. Only
			 * network/netmask pairs have a '/' in them.
			 */
			if ((strchr_m(*list, '/')) == NULL) {
				only_ip = false;
				DEBUG(3,("only_ipaddrs_in_list: list has "
					"non-ip address (%s)\n",
					*list));
				break;
			}
		}
	}

	return only_ip;
}

/* return true if access should be allowed to a service for a socket */
bool check_access(int sock, const char **allow_list, const char **deny_list)
{
	bool ret = false;
	bool only_ip = false;

	if ((!deny_list || *deny_list==0) && (!allow_list || *allow_list==0))
		ret = true;

	if (!ret) {
		char addr[INET6_ADDRSTRLEN];

		/* Bypass name resolution calls if the lists
		 * only contain IP addrs */
		if (only_ipaddrs_in_list(allow_list) &&
				only_ipaddrs_in_list(deny_list)) {
			only_ip = true;
			DEBUG (3, ("check_access: no hostnames "
				"in host allow/deny list.\n"));
			ret = allow_access(deny_list,
					allow_list,
					"",
					get_peer_addr(sock,addr,sizeof(addr)));
		} else {
			DEBUG (3, ("check_access: hostnames in "
				"host allow/deny list.\n"));
			ret = allow_access(deny_list,
					allow_list,
					get_peer_name(sock,true),
					get_peer_addr(sock,addr,sizeof(addr)));
		}

		if (ret) {
			DEBUG(2,("Allowed connection from %s (%s)\n",
				 only_ip ? "" : get_peer_name(sock,true),
				 get_peer_addr(sock,addr,sizeof(addr))));
		} else {
			DEBUG(0,("Denied connection from %s (%s)\n",
				 only_ip ? "" : get_peer_name(sock,true),
				 get_peer_addr(sock,addr,sizeof(addr))));
		}
	}

	return(ret);
}
