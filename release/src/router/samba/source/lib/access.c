/* 
   This module is an adaption of code from the tcpd-1.4 package written
   by Wietse Venema, Eindhoven University of Technology, The Netherlands.

   The code is used here with permission.

   The code has been considerably changed from the original. Bug reports
   should be sent to samba-bugs@samba.org
*/

#include "includes.h"

extern int DEBUGLEVEL;

/* Delimiters for lists of daemons or clients. */
static char *sep = ", \t";

#define	FAIL		(-1)

/* masked_match - match address against netnumber/netmask */
static int masked_match(char *tok, char *slash, char *s)
{
	uint32 net;
	uint32 mask;
	uint32 addr;

	if ((addr = interpret_addr(s)) == INADDR_NONE)
		return (False);
	*slash = 0;
	net = interpret_addr(tok);
	*slash = '/';
	if (net == INADDR_NONE || 
	    (mask = interpret_addr(slash + 1)) == INADDR_NONE) {
		DEBUG(0,("access: bad net/mask access control: %s\n", tok));
		return (False);
	}
	return ((addr & mask) == net);
}

/* string_match - match string against token */
static int string_match(char *tok,char *s, char *invalid_char)
{
	size_t     tok_len;
	size_t     str_len;
	char   *cut;

	*invalid_char = '\0';

	/* Return True if a token has the magic value "ALL". Return
	 * FAIL if the token is "FAIL". If the token starts with a "."
	 * (domain name), return True if it matches the last fields of
	 * the string. If the token has the magic value "LOCAL",
	 * return True if the string does not contain a "."
	 * character. If the token ends on a "." (network number),
	 * return True if it matches the first fields of the
	 * string. If the token begins with a "@" (netgroup name),
	 * return True if the string is a (host) member of the
	 * netgroup. Return True if the token fully matches the
	 * string. If the token is a netnumber/netmask pair, return
	 * True if the address is a member of the specified subnet.  */

	if (tok[0] == '.') {			/* domain: match last fields */
		if ((str_len = strlen(s)) > (tok_len = strlen(tok))
		    && strcasecmp(tok, s + str_len - tok_len) == 0)
			return (True);
	} else if (tok[0] == '@') { /* netgroup: look it up */
#ifdef	HAVE_NETGROUP
		static char *mydomain = NULL;
		char *hostname = NULL;
		BOOL netgroup_ok = False;

		if (!mydomain) yp_get_default_domain(&mydomain);

		if (!mydomain) {
			DEBUG(0,("Unable to get default yp domain.\n"));
			return False;
		}
		if (!(hostname = strdup(s))) {
			DEBUG(1,("out of memory for strdup!\n"));
			return False;
		}
		
		netgroup_ok = innetgr(tok + 1, hostname, (char *) 0, mydomain);
		
		DEBUG(5,("looking for %s of domain %s in netgroup %s gave %s\n", 
			 hostname,
			 mydomain, 
			 tok+1,
			 BOOLSTR(netgroup_ok)));

		free(hostname);
      
		if (netgroup_ok) return(True);
#else
		DEBUG(0,("access: netgroup support is not configured\n"));
		return (False);
#endif
	} else if (strcasecmp(tok, "ALL") == 0) {	/* all: match any */
		return (True);
	} else if (strcasecmp(tok, "FAIL") == 0) {	/* fail: match any */
		return (FAIL);
	} else if (strcasecmp(tok, "LOCAL") == 0) {	/* local: no dots */
		if (strchr(s, '.') == 0 && strcasecmp(s, "unknown") != 0)
			return (True);
	} else if (!strcasecmp(tok, s)) {   /* match host name or address */
		return (True);
	} else if (tok[(tok_len = strlen(tok)) - 1] == '.') {	/* network */
		if (strncmp(tok, s, tok_len) == 0)
			return (True);
	} else if ((cut = strchr(tok, '/')) != 0) {	/* netnumber/netmask */
		if (isdigit((int)s[0]) && masked_match(tok, cut, s))
			return (True);
	} else if (strchr(tok, '*') != 0) {
		*invalid_char = '*';
	} else if (strchr(tok, '?') != 0) {
		*invalid_char = '?';
	}
	return (False);
}


/* client_match - match host name and address against token */
static int client_match(char *tok,char *item)
{
    char **client = (char **)item;
    int     match;
	char invalid_char = '\0';

    /*
     * Try to match the address first. If that fails, try to match the host
     * name if available.
     */

    if ((match = string_match(tok, client[1], &invalid_char)) == 0) {
		if(invalid_char)
			DEBUG(0,("client_match: address match failing due to invalid character '%c' found in \
token '%s' in an allow/deny hosts line.\n", invalid_char, tok ));

		if (client[0][0] != 0)
			match = string_match(tok, client[0], &invalid_char);

		if(invalid_char)
			DEBUG(0,("client_match: address match failing due to invalid character '%c' found in \
token '%s' in an allow/deny hosts line.\n", invalid_char, tok ));
	}

    return (match);
}

/* list_match - match an item against a list of tokens with exceptions */
/* (All modifications are marked with the initials "jkf") */
static int list_match(char *list,char *item, int (*match_fn)(char *, char *))
{
    char   *tok;
    char   *listcopy;		/* jkf */
    int     match = False;

    /*
     * jkf@soton.ac.uk -- 31 August 1994 -- Stop list_match()
     * overwriting the list given as its first parameter.
     */

    /* jkf -- can get called recursively with NULL list */
    listcopy = (list == 0) ? (char *)0 : strdup(list);

    /*
     * Process tokens one at a time. We have exhausted all possible matches
     * when we reach an "EXCEPT" token or the end of the list. If we do find
     * a match, look for an "EXCEPT" list and recurse to determine whether
     * the match is affected by any exceptions.
     */

    for (tok = strtok(listcopy, sep); tok ; tok = strtok(NULL, sep)) {
	if (strcasecmp(tok, "EXCEPT") == 0)	/* EXCEPT: give up */
	    break;
	if ((match = (*match_fn) (tok, item)))	/* True or FAIL */
	    break;
    }
    /* Process exceptions to True or FAIL matches. */

    if (match != False) {
	while ((tok = strtok((char *) 0, sep)) && strcasecmp(tok, "EXCEPT"))
	     /* VOID */ ;
	if (tok == 0 || list_match((char *) 0, item, match_fn) == False) {
	    if (listcopy != 0) free(listcopy); /* jkf */
	    return (match);
	}
    }

    if (listcopy != 0) free(listcopy); /* jkf */
    return (False);
}


/* return true if access should be allowed */
BOOL allow_access(char *deny_list,char *allow_list,
		  char *cname,char *caddr)
{
	char *client[2];

	client[0] = cname;
	client[1] = caddr;  

	/* if it is loopback then always allow unless specifically denied */
	if (strcmp(caddr, "127.0.0.1") == 0) {
		if (deny_list && 
		    list_match(deny_list,(char *)client,client_match)) {
			return False;
		}
		return True;
	}

	/* if theres no deny list and no allow list then allow access */
	if ((!deny_list || *deny_list == 0) && 
	    (!allow_list || *allow_list == 0)) {
		return(True);  
	}

	/* if there is an allow list but no deny list then allow only hosts
	   on the allow list */
	if (!deny_list || *deny_list == 0)
		return(list_match(allow_list,(char *)client,client_match));

	/* if theres a deny list but no allow list then allow
	   all hosts not on the deny list */
	if (!allow_list || *allow_list == 0)
		return(!list_match(deny_list,(char *)client,client_match));

	/* if there are both type of list then allow all hosts on the
           allow list */
	if (list_match(allow_list,(char *)client,client_match))
		return (True);

	/* if there are both type of list and it's not on the allow then
	   allow it if its not on the deny */
	if (list_match(deny_list,(char *)client,client_match))
		return (False);
	
	return (True);
}

/* return true if access should be allowed to a service for a socket */
BOOL check_access(int sock, char *allow_list, char *deny_list)
{
	BOOL ret = False;
	
	if (deny_list) deny_list = strdup(deny_list);
	if (allow_list) allow_list = strdup(allow_list);

	if ((!deny_list || *deny_list==0) && (!allow_list || *allow_list==0)) {
		ret = True;
	}

	if (!ret) {
		if (allow_access(deny_list,allow_list,
				 client_name(sock),client_addr(sock))) {
			DEBUG(2,("Allowed connection from %s (%s)\n",
				 client_name(sock),client_addr(sock)));
			ret = True;
		} else {
			DEBUG(0,("Denied connection from %s (%s)\n",
				 client_name(sock),client_addr(sock)));
		}
	}

	if (deny_list) free(deny_list);
	if (allow_list) free(allow_list);
	return(ret);
}
