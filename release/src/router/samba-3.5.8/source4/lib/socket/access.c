/* 
   Unix SMB/CIFS implementation.

   check access rules for socket connections

   Copyright (C) Andrew Tridgell 2004
   
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


/* 
   This module is an adaption of code from the tcpd-1.4 package written
   by Wietse Venema, Eindhoven University of Technology, The Netherlands.

   The code is used here with permission.

   The code has been considerably changed from the original. Bug reports
   should be sent to samba@samba.org
*/

#include "includes.h"
#include "system/network.h"
#include "lib/socket/socket.h"
#include "system/locale.h"

#define	FAIL		(-1)
#define ALLONES  ((uint32_t)0xFFFFFFFF)

/* masked_match - match address against netnumber/netmask */
static bool masked_match(TALLOC_CTX *mem_ctx, const char *tok, const char *slash, const char *s)
{
	uint32_t net;
	uint32_t mask;
	uint32_t addr;
	char *tok_cpy;

	if ((addr = interpret_addr(s)) == INADDR_NONE)
		return false;

	tok_cpy = talloc_strdup(mem_ctx, tok);
	tok_cpy[PTR_DIFF(slash,tok)] = '\0';
	net = interpret_addr(tok_cpy);
	talloc_free(tok_cpy);

        if (strlen(slash + 1) > 2) {
                mask = interpret_addr(slash + 1);
        } else {
		mask = (uint32_t)((ALLONES >> atoi(slash + 1)) ^ ALLONES);
		/* convert to network byte order */
		mask = htonl(mask);
        }

	if (net == INADDR_NONE || mask == INADDR_NONE) {
		DEBUG(0,("access: bad net/mask access control: %s\n", tok));
		return false;
	}
	
	return (addr & mask) == (net & mask);
}

/* string_match - match string against token */
static bool string_match(TALLOC_CTX *mem_ctx, const char *tok,const char *s, char *invalid_char)
{
	size_t     tok_len;
	size_t     str_len;
	const char   *cut;

	*invalid_char = '\0';

	/* Return true if a token has the magic value "ALL". Return
	 * FAIL if the token is "FAIL". If the token starts with a "."
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
		    && strcasecmp(tok, s + str_len - tok_len)==0) {
			return true;
		}
	} else if (tok[0] == '@') { /* netgroup: look it up */
		DEBUG(0,("access: netgroup support is not available\n"));
		return false;
	} else if (strcmp(tok, "ALL")==0) {	/* all: match any */
		return true;
	} else if (strcmp(tok, "FAIL")==0) {	/* fail: match any */
		return FAIL;
	} else if (strcmp(tok, "LOCAL")==0) {	/* local: no dots */
		if (strchr(s, '.') == 0 && strcasecmp(s, "unknown") != 0) {
			return true;
		}
	} else if (strcasecmp(tok, s)==0) {   /* match host name or address */
		return true;
	} else if (tok[(tok_len = strlen(tok)) - 1] == '.') {	/* network */
		if (strncmp(tok, s, tok_len) == 0)
			return true;
	} else if ((cut = strchr(tok, '/')) != 0) {	/* netnumber/netmask */
		if (isdigit((int)s[0]) && masked_match(mem_ctx, tok, cut, s))
			return true;
	} else if (strchr(tok, '*') != 0) {
		*invalid_char = '*';
	} else if (strchr(tok, '?') != 0) {
		*invalid_char = '?';
	}
	return false;
}

struct client_addr {
	const char *cname;
	const char *caddr;
};

/* client_match - match host name and address against token */
static bool client_match(TALLOC_CTX *mem_ctx, const char *tok, struct client_addr *client)
{
	bool match;
	char invalid_char = '\0';

	/*
	 * Try to match the address first. If that fails, try to match the host
	 * name if available.
	 */

	if ((match = string_match(mem_ctx, tok, client->caddr, &invalid_char)) == 0) {
		if(invalid_char)
			DEBUG(0,("client_match: address match failing due to invalid character '%c' found in \
token '%s' in an allow/deny hosts line.\n", invalid_char, tok ));

		if (client->cname[0] != 0)
			match = string_match(mem_ctx, tok, client->cname, &invalid_char);

		if(invalid_char)
			DEBUG(0,("client_match: address match failing due to invalid character '%c' found in \
token '%s' in an allow/deny hosts line.\n", invalid_char, tok ));
	}

	return (match);
}

/* list_match - match an item against a list of tokens with exceptions */
static bool list_match(TALLOC_CTX *mem_ctx, const char **list, struct client_addr *client)
{
	bool match = false;

	if (!list)
		return false;

	/*
	 * Process tokens one at a time. We have exhausted all possible matches
	 * when we reach an "EXCEPT" token or the end of the list. If we do find
	 * a match, look for an "EXCEPT" list and recurse to determine whether
	 * the match is affected by any exceptions.
	 */

	for (; *list ; list++) {
		if (strcmp(*list, "EXCEPT")==0)	/* EXCEPT: give up */
			break;
		if ((match = client_match(mem_ctx, *list, client)))	/* true or FAIL */
			break;
	}

	/* Process exceptions to true or FAIL matches. */
	if (match != false) {
		while (*list  && strcmp(*list, "EXCEPT")!=0)
			list++;

		for (; *list; list++) {
			if (client_match(mem_ctx, *list, client)) /* Exception Found */
				return false;
		}
	}

	return match;
}

/* return true if access should be allowed */
static bool allow_access_internal(TALLOC_CTX *mem_ctx,
				  const char **deny_list,const char **allow_list,
				  const char *cname, const char *caddr)
{
	struct client_addr client;

	client.cname = cname;
	client.caddr = caddr;

	/* if it is loopback then always allow unless specifically denied */
	if (strcmp(caddr, "127.0.0.1") == 0) {
		/*
		 * If 127.0.0.1 matches both allow and deny then allow.
		 * Patch from Steve Langasek vorlon@netexpress.net.
		 */
		if (deny_list && 
			list_match(mem_ctx, deny_list, &client) &&
				(!allow_list ||
				!list_match(mem_ctx, allow_list, &client))) {
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
	if (!deny_list || *deny_list == 0)
		return list_match(mem_ctx, allow_list, &client);

	/* if theres a deny list but no allow list then allow
	   all hosts not on the deny list */
	if (!allow_list || *allow_list == 0)
		return !list_match(mem_ctx, deny_list, &client);

	/* if there are both types of list then allow all hosts on the
           allow list */
	if (list_match(mem_ctx, allow_list, &client))
		return true;

	/* if there are both types of list and it's not on the allow then
	   allow it if its not on the deny */
	if (list_match(mem_ctx, deny_list, &client))
		return false;
	
	return true;
}

/* return true if access should be allowed */
bool allow_access(TALLOC_CTX *mem_ctx,
		  const char **deny_list, const char **allow_list,
		  const char *cname, const char *caddr)
{
	bool ret;
	char *nc_cname = talloc_strdup(mem_ctx, cname);
	char *nc_caddr = talloc_strdup(mem_ctx, caddr);

	if (!nc_cname || !nc_caddr) {
		return false;
	}
	
	ret = allow_access_internal(mem_ctx, deny_list, allow_list, nc_cname, nc_caddr);

	talloc_free(nc_cname);
	talloc_free(nc_caddr);

	return ret;
}

/* return true if the char* contains ip addrs only.  Used to avoid 
gethostbyaddr() calls */

static bool only_ipaddrs_in_list(const char** list)
{
	bool only_ip = true;
	
	if (!list)
		return true;
			
	for (; *list ; list++) {
		/* factor out the special strings */
		if (strcmp(*list, "ALL")==0 || 
		    strcmp(*list, "FAIL")==0 || 
		    strcmp(*list, "EXCEPT")==0) {
			continue;
		}
		
		if (!is_ipaddress(*list)) {
			/* 
			 * if we failed, make sure that it was not because the token
			 * was a network/netmask pair.  Only network/netmask pairs
			 * have a '/' in them
			 */
			if ((strchr(*list, '/')) == NULL) {
				only_ip = false;
				DEBUG(3,("only_ipaddrs_in_list: list has non-ip address (%s)\n", *list));
				break;
			}
		}
	}
	
	return only_ip;
}

/* return true if access should be allowed to a service for a socket */
bool socket_check_access(struct socket_context *sock, 
			 const char *service_name,
			 const char **allow_list, const char **deny_list)
{
	bool ret;
	const char *name="";
	struct socket_address *addr;
	TALLOC_CTX *mem_ctx;

	if ((!deny_list  || *deny_list==0) && 
	    (!allow_list || *allow_list==0)) {
		return true;
	}

	mem_ctx = talloc_init("socket_check_access");
	if (!mem_ctx) {
		return false;
	}

	addr = socket_get_peer_addr(sock, mem_ctx);
	if (!addr) {
		DEBUG(0,("socket_check_access: Denied connection from unknown host: could not get peer address from kernel\n"));
		talloc_free(mem_ctx);
		return false;
	}

	/* bypass gethostbyaddr() calls if the lists only contain IP addrs */
	if (!only_ipaddrs_in_list(allow_list) || 
	    !only_ipaddrs_in_list(deny_list)) {
		name = socket_get_peer_name(sock, mem_ctx);
		if (!name) {
			name = addr->addr;
		}
	}

	if (!addr) {
		DEBUG(0,("socket_check_access: Denied connection from unknown host\n"));
		talloc_free(mem_ctx);
		return false;
	}

	ret = allow_access(mem_ctx, deny_list, allow_list, name, addr->addr);
	
	if (ret) {
		DEBUG(2,("socket_check_access: Allowed connection to '%s' from %s (%s)\n", 
			 service_name, name, addr->addr));
	} else {
		DEBUG(0,("socket_check_access: Denied connection to '%s' from %s (%s)\n", 
			 service_name, name, addr->addr));
	}

	talloc_free(mem_ctx);

	return ret;
}
