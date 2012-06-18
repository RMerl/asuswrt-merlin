/* 
   Unix SMB/CIFS implementation.
   SMB client library implementation (server cache)
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Richard Sharpe 2000
   Copyright (C) John Terpstra 2000
   Copyright (C) Tom Jansen (Ninja ISD) 2002 
   
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
#include "libsmbclient.h"
#include "libsmb_internal.h"

/*
 * Structure we use if internal caching mechanism is used 
 * nothing fancy here.
 */
struct smbc_server_cache {
	char *server_name;
	char *share_name;
	char *workgroup;
	char *username;
	SMBCSRV *server;
        
	struct smbc_server_cache *next, *prev;
};



/*
 * Add a new connection to the server cache.
 * This function is only used if the external cache is not enabled 
 */
int
SMBC_add_cached_server(SMBCCTX * context,
                       SMBCSRV * newsrv,
                       const char * server,
                       const char * share, 
                       const char * workgroup,
                       const char * username)
{
	struct smbc_server_cache * srvcache = NULL;
        
	if (!(srvcache = SMB_MALLOC_P(struct smbc_server_cache))) {
		errno = ENOMEM;
		DEBUG(3, ("Not enough space for server cache allocation\n"));
		return 1;
	}
        
	ZERO_STRUCTP(srvcache);
        
	srvcache->server = newsrv;
        
	srvcache->server_name = SMB_STRDUP(server);
	if (!srvcache->server_name) {
		errno = ENOMEM;
		goto failed;
	}
        
	srvcache->share_name = SMB_STRDUP(share);
	if (!srvcache->share_name) {
		errno = ENOMEM;
		goto failed;
	}
        
	srvcache->workgroup = SMB_STRDUP(workgroup);
	if (!srvcache->workgroup) {
		errno = ENOMEM;
		goto failed;
	}
        
	srvcache->username = SMB_STRDUP(username);
	if (!srvcache->username) {
		errno = ENOMEM;
		goto failed;
	}
        
	DLIST_ADD(context->internal->server_cache, srvcache);
	return 0;
        
failed:
	SAFE_FREE(srvcache->server_name);
	SAFE_FREE(srvcache->share_name);
	SAFE_FREE(srvcache->workgroup);
	SAFE_FREE(srvcache->username);
	SAFE_FREE(srvcache);
        
	return 1;
}



/*
 * Search the server cache for a server 
 * returns server handle on success, NULL on error (not found)
 * This function is only used if the external cache is not enabled 
 */
SMBCSRV *
SMBC_get_cached_server(SMBCCTX * context,
                       const char * server, 
                       const char * share,
                       const char * workgroup,
                       const char * user)
{
	struct smbc_server_cache * srv = NULL;
        
	/* Search the cache lines */
	for (srv = context->internal->server_cache; srv; srv = srv->next) {
                
		if (strcmp(server,srv->server_name)  == 0 &&
		    strcmp(workgroup,srv->workgroup) == 0 &&
		    strcmp(user, srv->username)  == 0) {
                        
                        /* If the share name matches, we're cool */
                        if (strcmp(share, srv->share_name) == 0) {
                                return srv->server;
                        }
                        
                        /*
                         * We only return an empty share name or the attribute
                         * server on an exact match (which would have been
                         * caught above).
                         */
                        if (*share == '\0' || strcmp(share, "*IPC$") == 0)
                                continue;
                        
                        /*
                         * Never return an empty share name or the attribute
                         * server if it wasn't what was requested.
                         */
                        if (*srv->share_name == '\0' ||
                            strcmp(srv->share_name, "*IPC$") == 0)
                                continue;
                        
                        /*
                         * If we're only allowing one share per server, then
                         * a connection to the server (other than the
                         * attribute server connection) is cool.
                         */
                        if (smbc_getOptionOneSharePerServer(context)) {
                                /*
                                 * The currently connected share name
                                 * doesn't match the requested share, so
                                 * disconnect from the current share.
                                 */
                                if (! cli_tdis(srv->server->cli)) {
                                        /* Sigh. Couldn't disconnect. */
                                        cli_shutdown(srv->server->cli);
					srv->server->cli = NULL;
                                        smbc_getFunctionRemoveCachedServer(context)(context, srv->server);
                                        continue;
                                }
                                
                                /*
                                 * Save the new share name.  We've
                                 * disconnected from the old share, and are
                                 * about to connect to the new one.
                                 */
                                SAFE_FREE(srv->share_name);
                                srv->share_name = SMB_STRDUP(share);
                                if (!srv->share_name) {
                                        /* Out of memory. */
                                        cli_shutdown(srv->server->cli);
					srv->server->cli = NULL;
                                        smbc_getFunctionRemoveCachedServer(context)(context, srv->server);
                                        continue;
                                }
                                
                                
                                return srv->server;
                        }
                }
	}
        
	return NULL;
}


/* 
 * Search the server cache for a server and remove it
 * returns 0 on success
 * This function is only used if the external cache is not enabled 
 */
int
SMBC_remove_cached_server(SMBCCTX * context,
                          SMBCSRV * server)
{
	struct smbc_server_cache * srv = NULL;
        
	for (srv = context->internal->server_cache; srv; srv = srv->next) {
		if (server == srv->server) { 
                        
			/* remove this sucker */
			DLIST_REMOVE(context->internal->server_cache, srv);
			SAFE_FREE(srv->server_name);
			SAFE_FREE(srv->share_name);
			SAFE_FREE(srv->workgroup);
			SAFE_FREE(srv->username);
			SAFE_FREE(srv);
			return 0;
		}
	}
	/* server not found */
	return 1;
}


/*
 * Try to remove all the servers in cache
 * returns 1 on failure and 0 if all servers could be removed.
 */
int
SMBC_purge_cached_servers(SMBCCTX * context)
{
	struct smbc_server_cache * srv;
	struct smbc_server_cache * next;
	int could_not_purge_all = 0;
        
	for (srv = context->internal->server_cache,
                     next = (srv ? srv->next :NULL);
             srv;
             srv = next,
                     next = (srv ? srv->next : NULL)) {
                
		if (SMBC_remove_unused_server(context, srv->server)) {
			/* could not be removed */
			could_not_purge_all = 1;
		}
	}
	return could_not_purge_all;
}
