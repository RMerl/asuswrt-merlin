/***********************************************************************
*
* peer.c
*
* Manage lists of peers for L2TP
*
* Copyright (C) 2002 by Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: peer.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include "l2tp.h"
#include <stddef.h>
#include <string.h>

static hash_table all_peers;
static int peer_process_option(EventSelector *es,
			       char const *name,
			       char const *value);

static l2tp_peer prototype;

static option_handler peer_option_handler = {
    NULL, "peer", peer_process_option
};

static int port;

static int handle_secret_option(EventSelector *es, l2tp_opt_descriptor *desc, char const *value);
static int handle_hostname_option(EventSelector *es, l2tp_opt_descriptor *desc, char const *value);
static int handle_peername_option(EventSelector *es, l2tp_opt_descriptor *desc, char const *value);
static int set_lac_handler(EventSelector *es, l2tp_opt_descriptor *desc, char const *value);
static int handle_lac_option(EventSelector *es, l2tp_opt_descriptor *desc, char const *value);
static int set_lns_handler(EventSelector *es, l2tp_opt_descriptor *desc, char const *value);
static int handle_lns_option(EventSelector *es, l2tp_opt_descriptor *desc, char const *value);

/* Peer options */
static l2tp_opt_descriptor peer_opts[] = {
    /*  name               type                 addr */
    { "peer",              OPT_TYPE_IPADDR,   &prototype.addr.sin_addr.s_addr},
    { "mask",              OPT_TYPE_INT,      &prototype.mask_bits},
    { "secret",            OPT_TYPE_CALLFUNC, (void *) handle_secret_option},
    { "hostname",          OPT_TYPE_CALLFUNC, (void *) handle_hostname_option},
    { "peername",          OPT_TYPE_CALLFUNC, (void *) handle_peername_option},
    { "port",              OPT_TYPE_PORT,     &port },
    { "lac-handler",       OPT_TYPE_CALLFUNC, (void *) set_lac_handler},
    { "lac-opts",          OPT_TYPE_CALLFUNC, (void *) handle_lac_option},
    { "lns-handler",       OPT_TYPE_CALLFUNC, (void *) set_lns_handler},
    { "lns-opts",          OPT_TYPE_CALLFUNC, (void *) handle_lns_option},
    { "hide-avps",         OPT_TYPE_BOOL,     &prototype.hide_avps},
    { "retain-tunnel",     OPT_TYPE_BOOL,     &prototype.retain_tunnel},
    { "persist",           OPT_TYPE_BOOL,     &prototype.persist},
    { "holdoff",           OPT_TYPE_INT,      &prototype.holdoff},
    { "maxfail",           OPT_TYPE_INT,      &prototype.maxfail},
    { "strict-ip-check",   OPT_TYPE_BOOL,     &prototype.validate_peer_ip},
#ifdef RTCONFIG_VPNC
    { "vpnc",         OPT_TYPE_INT,      &vpnc},
#endif
    { NULL,                OPT_TYPE_BOOL,     NULL }
};

static int
set_lac_handler(EventSelector *es,
		l2tp_opt_descriptor *desc,
		char const *value)
{
    l2tp_lac_handler *handler = l2tp_session_find_lac_handler(value);
    if (!handler) {
	l2tp_set_errmsg("No LAC handler named '%s'", value);
	return -1;
    }
    prototype.lac_ops = handler->call_ops;
    return 0;
}

static int
set_lns_handler(EventSelector *es,
		l2tp_opt_descriptor *desc,
		char const *value)
{
    l2tp_lns_handler *handler = l2tp_session_find_lns_handler(value);
    if (!handler) {
	l2tp_set_errmsg("No LNS handler named '%s'", value);
	return -1;
    }
    prototype.lns_ops = handler->call_ops;
    return 0;
}

/**********************************************************************
* %FUNCTION: handle_secret_option
* %ARGUMENTS:
*  es -- event selector
*  desc -- descriptor
*  value -- the secret
* %RETURNS:
*  0
* %DESCRIPTION:
*  Copies secret to prototype
***********************************************************************/
static int
handle_secret_option(EventSelector *es,
		     l2tp_opt_descriptor *desc,
		     char const *value)
{
    strncpy(prototype.secret, value, MAX_SECRET_LEN);
    prototype.secret[MAX_SECRET_LEN-1] = 0;
    prototype.secret_len = strlen(prototype.secret);
    return 0;
}

/**********************************************************************
* %FUNCTION: handle_hostname_option
* %ARGUMENTS:
*  es -- event selector
*  desc -- descriptor
*  value -- the hostname
* %RETURNS:
*  0
* %DESCRIPTION:
*  Copies hostname to prototype
***********************************************************************/
static int
handle_hostname_option(EventSelector *es,
		     l2tp_opt_descriptor *desc,
		     char const *value)
{
    strncpy(prototype.hostname, value, MAX_HOSTNAME);
    prototype.hostname[MAX_HOSTNAME-1] = 0;
    prototype.hostname_len = strlen(prototype.hostname);
    return 0;
}

/**********************************************************************
* %FUNCTION: handle_peername_option
* %ARGUMENTS:
*  es -- event selector
*  desc -- descriptor
*  value -- the hostname
* %RETURNS:
*  0
* %DESCRIPTION:
*  Copies peername to prototype
***********************************************************************/
static int
handle_peername_option(EventSelector *es,
		     l2tp_opt_descriptor *desc,
		     char const *value)
{
    strncpy(prototype.peername, value, MAX_HOSTNAME);
    prototype.peername[MAX_HOSTNAME-1] = 0;
    prototype.peername_len = strlen(prototype.peername);
    return 0;
}

/**********************************************************************
* %FUNCTION: handle_lac_option
* %ARGUMENTS:
*  es -- event selector
*  desc -- descriptor
*  value -- the hostname
* %RETURNS:
*  0
* %DESCRIPTION:
*  Copies LAC options to prototype
***********************************************************************/
static int
handle_lac_option(EventSelector *es,
		     l2tp_opt_descriptor *desc,
		     char const *value)
{
    char word[512];
    while (value && *value) {
        value = l2tp_chomp_word(value, word);
        if (!word[0]) break;
        if (prototype.num_lac_options < MAX_OPTS) {
            char *x = strdup(word);
            if (x) prototype.lac_options[prototype.num_lac_options++] = x;
            prototype.lac_options[prototype.num_lac_options] = NULL;
        } else {
            break;
        }
    }
    return 0;
}

/**********************************************************************
* %FUNCTION: handle_lns_option
* %ARGUMENTS:
*  es -- event selector
*  desc -- descriptor
*  value -- the hostname
* %RETURNS:
*  0
* %DESCRIPTION:
*  Copies LNS options to prototype
***********************************************************************/
static int
handle_lns_option(EventSelector *es,
		     l2tp_opt_descriptor *desc,
		     char const *value)
{
    char word[512];
    while (value && *value) {
        value = l2tp_chomp_word(value, word);
        if (!word[0]) break;
        if (prototype.num_lns_options < MAX_OPTS) {
            char *x = strdup(word);
            if (x) prototype.lns_options[prototype.num_lns_options++] = x;
            prototype.lns_options[prototype.num_lns_options] = NULL;
        } else {
            break;
        }
    }
    return 0;
}

/**********************************************************************
* %FUNCTION: peer_process_option
* %ARGUMENTS:
*  es -- event selector
*  name, value -- name and value of option
* %RETURNS:
*  0 on success, -1 on failure
* %DESCRIPTION:
*  Processes an option in the "peer" section
***********************************************************************/
static int
peer_process_option(EventSelector *es,
		    char const *name,
		    char const *value)
{
    l2tp_peer *peer;

    /* Special cases: begin and end */
    if (!strcmp(name, "*begin*")) {
	/* Switching in to peer context */
	memset(&prototype, 0, sizeof(prototype));
	prototype.mask_bits = 32;
	prototype.validate_peer_ip = 1;
	port = 1701;
	return 0;
    }

    if (!strcmp(name, "*end*")) {
	/* Validate settings */
	uint16_t u16 = (uint16_t) port;
	prototype.addr.sin_port = htons(u16);
	prototype.addr.sin_family = AF_INET;

	/* Allow non-authenticated tunnels
	if (!prototype.secret_len) {
	    l2tp_set_errmsg("No secret supplied for peer");
	    return -1;
	}
	*/
	if (!prototype.lns_ops && !prototype.lac_ops) {
	    l2tp_set_errmsg("You must enable at least one of lns-handler or lac-handler");
	    return -1;
	}

	/* Add the peer */
	peer = l2tp_peer_insert(&prototype.addr);
	if (!peer) return -1;

	peer->mask_bits = prototype.mask_bits;
	memcpy(&peer->hostname,&prototype.hostname, sizeof(prototype.hostname));
	peer->hostname_len = prototype.hostname_len;
	memcpy(&peer->peername,&prototype.peername, sizeof(prototype.peername));
	peer->peername_len = prototype.peername_len;
	memcpy(&peer->secret, &prototype.secret, MAX_SECRET_LEN);
	peer->secret_len = prototype.secret_len;
	peer->lns_ops = prototype.lns_ops;
        memcpy(&peer->lns_options,&prototype.lns_options,sizeof(prototype.lns_options));
	peer->lac_ops = prototype.lac_ops;
        memcpy(&peer->lac_options,&prototype.lac_options,sizeof(prototype.lac_options));
	peer->hide_avps = prototype.hide_avps;
	peer->retain_tunnel = prototype.retain_tunnel;
	peer->persist = prototype.persist;
	peer->holdoff = prototype.holdoff;
	peer->maxfail = prototype.maxfail;
	peer->fail = 0;
	peer->validate_peer_ip = prototype.validate_peer_ip;
	return 0;
    }

    /* Process option */
    return l2tp_option_set(es, name, value, peer_opts);
}

/**********************************************************************
* %FUNCTION: peer_compute_hash
* %ARGUMENTS:
*  data -- a void pointer which is really a peer
* %RETURNS:
*  Inet address
***********************************************************************/
static unsigned int
peer_compute_hash(void *data)
{
    unsigned int hash = (unsigned int) (((l2tp_peer *) data)->addr.sin_addr.s_addr);
    return hash;
}

/**********************************************************************
* %FUNCTION: peer_compare
* %ARGUMENTS:
*  item1 -- first peer
*  item2 -- second peer
* %RETURNS:
*  0 if both peers have same ID, non-zero otherwise
***********************************************************************/
static int
peer_compare(void *item1, void *item2)
{
    return ((l2tp_peer *) item1)->addr.sin_addr.s_addr !=
	((l2tp_peer *) item2)->addr.sin_addr.s_addr;
}

/**********************************************************************
* %FUNCTION: peer_init
* %ARGUMENTS:
*  None
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Initializes peer hash table
***********************************************************************/
void
l2tp_peer_init(void)
{
    hash_init(&all_peers,
	      offsetof(l2tp_peer, hash),
	      peer_compute_hash,
	      peer_compare);
    l2tp_option_register_section(&peer_option_handler);
}

/**********************************************************************
* %FUNCTION: peer_find
* %ARGUMENTS:
*  addr -- IP address of peer
*  hostname -- AVP peer hostname
* %RETURNS:
*  A pointer to the peer with given IP address, or NULL if not found.
* %DESCRIPTION:
*  Searches peer hash table for specified peer.
***********************************************************************/
l2tp_peer *
l2tp_peer_find(struct sockaddr_in *addr, char const *peername)
{
    void *cursor;
    l2tp_peer *peer = NULL;
    l2tp_peer *candidate = NULL;
    char addr1_str[16], addr2_str[16];

    for (candidate = hash_start(&all_peers, &cursor);
	 candidate ;
	 candidate = hash_next(&all_peers, &cursor)) {

	unsigned long mask = candidate->mask_bits ?
	    htonl(0xFFFFFFFFUL << (32 - candidate->mask_bits)) : 0;

	strcpy(addr1_str, inet_ntoa(addr->sin_addr));
	strcpy(addr2_str, inet_ntoa(candidate->addr.sin_addr));
	DBG(l2tp_db(DBG_TUNNEL, "l2tp_peer_find(%s) examining peer %s/%d\n",
	       addr1_str, addr2_str,
	       candidate->mask_bits));

	if ((candidate->addr.sin_addr.s_addr & mask) ==
	    (addr->sin_addr.s_addr & mask)
	    && (!peername ||
		!(candidate->peername[0]) ||
		!strcmp(peername,candidate->peername))) {

	    if (peer == NULL) {
		peer = candidate;
	    } else {
		if (peer->mask_bits < candidate->mask_bits)
		    peer = candidate;
	    }
	}
    }

    strcpy(addr1_str, inet_ntoa(addr->sin_addr));
    if (peer != NULL)
	strcpy(addr2_str, inet_ntoa(peer->addr.sin_addr));
    DBG(l2tp_db(DBG_TUNNEL, "l2tp_peer_find(%s) found %s/%d\n",
	   addr1_str,
	   peer == NULL ? "NULL" : addr2_str,
	   peer == NULL ? -1 : peer->mask_bits));

    return peer;
}

/**********************************************************************
* %FUNCTION: peer_insert
* %ARGUMENTS:
*  addr -- IP address of peer
* %RETURNS:
*  NULL if insert failed, pointer to new peer structure otherwise
* %DESCRIPTION:
*  Inserts a new peer in the all_peers table
***********************************************************************/
l2tp_peer *
l2tp_peer_insert(struct sockaddr_in *addr)
{
    l2tp_peer *peer = malloc(sizeof(l2tp_peer));
    if (!peer) {
	l2tp_set_errmsg("peer_insert: Out of memory");
	return NULL;
    }
    memset(peer, 0, sizeof(*peer));

    peer->addr = *addr;
    hash_insert(&all_peers, peer);
    return peer;
}
