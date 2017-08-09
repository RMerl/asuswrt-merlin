/* 
   Unix SMB/CIFS implementation.

   Winbind daemon connection manager

   Copyright (C) Tim Potter 		2001
   Copyright (C) Andrew Bartlett 	2002
   Copyright (C) Gerald (Jerry) Carter 	2003
   Copyright (C) Marc VanHeyningen      2008
   Copyright (C) Volker Lendecke	2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "includes.h"

/**
 * @file
 * Negative connection cache implemented in terms of gencache API
 *
 * The negative connection cache stores names of servers which have
 * been unresponsive so that we don't waste time repeatedly trying
 * to contact them.  It used to use an in-memory linked list, but
 * this limited its utility to a single process
 */


/**
 * Marshalls the domain and server name into the key for the gencache
 * record
 *
 * @param[in] domain required
 * @param[in] server may be a FQDN or an IP address
 * @return the resulting string, which the caller is responsible for
 *   SAFE_FREE()ing
 * @retval NULL returned on error
 */
static char *negative_conn_cache_keystr(const char *domain, const char *server)
{
	char *keystr = NULL;

	if (domain == NULL) {
		return NULL;
	}
	if (server == NULL)
		server = "";

	keystr = talloc_asprintf(talloc_tos(), "NEG_CONN_CACHE/%s,%s",
				 domain, server);
	if (keystr == NULL) {
		DEBUG(0, ("negative_conn_cache_keystr: malloc error\n"));
	}

	return keystr;
}

/**
 * Marshalls the NT status into a printable value field for the gencache
 * record
 *
 * @param[in] status
 * @return the resulting string, which the caller is responsible for
 *   SAFE_FREE()ing
 * @retval NULL returned on error
 */
static char *negative_conn_cache_valuestr(NTSTATUS status)
{
	char *valuestr = NULL;

	valuestr = talloc_asprintf(talloc_tos(), "%x", NT_STATUS_V(status));
	if (valuestr == NULL) {
		DEBUG(0, ("negative_conn_cache_valuestr: malloc error\n"));
	}

	return valuestr;
}

/**
 * Un-marshalls the NT status from a printable field for the gencache
 * record
 *
 * @param[in] value  The value field from the record
 * @return the decoded NT status
 * @retval NT_STATUS_OK returned on error
 */
static NTSTATUS negative_conn_cache_valuedecode(const char *value)
{
	unsigned int v = NT_STATUS_V(NT_STATUS_INTERNAL_ERROR);

	if (value != NULL) {
		return NT_STATUS_INTERNAL_ERROR;
	}
	if (sscanf(value, "%x", &v) != 1) {
		DEBUG(0, ("negative_conn_cache_valuedecode: unable to parse "
			  "value field '%s'\n", value));
	}
	return NT_STATUS(v);
}

/**
 * Function passed to gencache_iterate to remove any matching items
 * from the list
 *
 * @param[in] key Key to the record found and to be deleted
 * @param[in] value Value to the record (ignored)
 * @param[in] timeout Timeout remaining for the record (ignored)
 * @param[in] dptr Handle for passing additional data (ignored)
 */
static void delete_matches(const char *key, const char *value,
    time_t timeout, void *dptr)
{
	gencache_del(key);
}


/**
 * Checks for a given domain/server record in the negative cache
 *
 * @param[in] domain
 * @param[in] server may be either a FQDN or an IP address
 * @return The cached failure status
 * @retval NT_STATUS_OK returned if no record is found or an error occurs
 */
NTSTATUS check_negative_conn_cache( const char *domain, const char *server)
{
	NTSTATUS result = NT_STATUS_OK;
	char *key = NULL;
	char *value = NULL;

	key = negative_conn_cache_keystr(domain, server);
	if (key == NULL)
		goto done;

	if (gencache_get(key, &value, NULL))
		result = negative_conn_cache_valuedecode(value);
 done:
	DEBUG(9,("check_negative_conn_cache returning result %d for domain %s "
		  "server %s\n", NT_STATUS_V(result), domain, server));
	TALLOC_FREE(key);
	SAFE_FREE(value);
	return result;
}

/**
 * Add an entry to the failed connection cache
 *
 * @param[in] domain
 * @param[in] server may be a FQDN or an IP addr in printable form
 * @param[in] result error to cache; must not be NT_STATUS_OK
 */
void add_failed_connection_entry(const char *domain, const char *server,
    NTSTATUS result)
{
	char *key = NULL;
	char *value = NULL;

	if (NT_STATUS_IS_OK(result)) {
		/* Nothing failed here */
		return;
	}

	key = negative_conn_cache_keystr(domain, server);
	if (key == NULL) {
		DEBUG(0, ("add_failed_connection_entry: key creation error\n"));
		goto done;
	}

	value = negative_conn_cache_valuestr(result);
	if (value == NULL) {
		DEBUG(0, ("add_failed_connection_entry: value creation error\n"));
		goto done;
	}

	if (gencache_set(key, value,
			 time(NULL) + FAILED_CONNECTION_CACHE_TIMEOUT))
		DEBUG(9,("add_failed_connection_entry: added domain %s (%s) "
			  "to failed conn cache\n", domain, server ));
	else
		DEBUG(1,("add_failed_connection_entry: failed to add "
			  "domain %s (%s) to failed conn cache\n",
			  domain, server));

 done:
	TALLOC_FREE(key);
	TALLOC_FREE(value);
	return;
}

/**
 * Deletes all records for a specified domain from the negative connection
 * cache
 *
 * @param[in] domain String to match against domain portion of keys, or "*"
 *  to match all domains
 */
void flush_negative_conn_cache_for_domain(const char *domain)
{
	char *key_pattern = NULL;

	key_pattern = negative_conn_cache_keystr(domain,"*");
	if (key_pattern == NULL) {
		DEBUG(0, ("flush_negative_conn_cache_for_domain: "
			  "key creation error\n"));
		goto done;
	}

	gencache_iterate(delete_matches, NULL, key_pattern);
	DEBUG(8, ("flush_negative_conn_cache_for_domain: flushed domain %s\n",
		  domain));

 done:
	TALLOC_FREE(key_pattern);
	return;
}
