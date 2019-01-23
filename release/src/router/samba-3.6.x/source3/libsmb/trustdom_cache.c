/* 
   Unix SMB/CIFS implementation.

   Trusted domain names cache on top of gencache.

   Copyright (C) Rafal Szczesniak	2002

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
#include "../libcli/security/security.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_ALL	/* there's no proper class yet */

#define TDOMKEY_FMT  "TDOM/%s"
#define TDOMTSKEY    "TDOMCACHE/TIMESTAMP"


/**
 * @file trustdom_cache.c
 *
 * Implementation of trusted domain names cache useful when
 * samba acts as domain member server. In such case, caching
 * domain names currently trusted gives a performance gain
 * because there's no need to query PDC each time we need
 * list of trusted domains
 **/

/**
 * Initialise trustdom name caching system. Call gencache
 * initialisation routine to perform necessary activities.
 *
 * @return true upon successful cache initialisation or
 *         false if cache init failed
 **/

bool trustdom_cache_enable(void)
{
	return True;
}


/**
 * Shutdown trustdom name caching system. Calls gencache
 * shutdown function.
 *
 * @return true upon successful cache close or
 *         false if it failed
 **/

bool trustdom_cache_shutdown(void)
{
	return True;
}


/**
 * Form up trustdom name key. It is based only
 * on domain name now.
 *
 * @param name trusted domain name
 * @return cache key for use in gencache mechanism
 **/

static char* trustdom_cache_key(const char* name)
{
	char* keystr = NULL;
	asprintf_strupper_m(&keystr, TDOMKEY_FMT, name);

	return keystr;
}


/**
 * Store trusted domain in gencache as the domain name (key)
 * and trusted domain's SID (value)
 *
 * @param name trusted domain name
 * @param alt_name alternative trusted domain name (used in ADS domains)
 * @param sid trusted domain's SID
 * @param timeout cache entry expiration time
 * @return true upon successful value storing or
 *         false if store attempt failed
 **/
 
bool trustdom_cache_store(char* name, char* alt_name, const struct dom_sid *sid,
                          time_t timeout)
{
	char *key, *alt_key;
	fstring sid_string;
	bool ret;

	DEBUG(5, ("trustdom_store: storing SID %s of domain %s\n",
	          sid_string_dbg(sid), name));

	key = trustdom_cache_key(name);
	alt_key = alt_name ? trustdom_cache_key(alt_name) : NULL;

	/* Generate string representation domain SID */
	sid_to_fstring(sid_string, sid);

	/*
	 * try to put the names in the cache
	 */
	if (alt_key) {
		ret = gencache_set(alt_key, sid_string, timeout);
		if ( ret ) {
			ret = gencache_set(key, sid_string, timeout);
		}
		SAFE_FREE(alt_key);
		SAFE_FREE(key);
		return ret;
	}

	ret = gencache_set(key, sid_string, timeout);
	SAFE_FREE(key);
	return ret;
}


/**
 * Fetch trusted domain's SID from the gencache.
 * This routine can also be used to check whether given
 * domain is currently trusted one.
 *
 * @param name trusted domain name
 * @param sid trusted domain's SID to be returned
 * @return true if entry is found or
 *         false if has expired/doesn't exist
 **/

bool trustdom_cache_fetch(const char* name, struct dom_sid* sid)
{
	char *key = NULL, *value = NULL;
	time_t timeout;

	/* exit now if null pointers were passed as they're required further */
	if (!sid)
		return False;

	/* prepare a key and get the value */
	key = trustdom_cache_key(name);
	if (!key)
		return False;

	if (!gencache_get(key, &value, &timeout)) {
		DEBUG(5, ("no entry for trusted domain %s found.\n", name));
		SAFE_FREE(key);
		return False;
	} else {
		SAFE_FREE(key);
		DEBUG(5, ("trusted domain %s found (%s)\n", name, value));
	}

	/* convert sid string representation into struct dom_sid structure */
	if(! string_to_sid(sid, value)) {
		sid = NULL;
		SAFE_FREE(value);
		return False;
	}

	SAFE_FREE(value);
	return True;
}


/*******************************************************************
 fetch the timestamp from the last update 
*******************************************************************/

uint32 trustdom_cache_fetch_timestamp( void )
{
	char *value = NULL;
	time_t timeout;
	uint32 timestamp;

	if (!gencache_get(TDOMTSKEY, &value, &timeout)) {
		DEBUG(5, ("no timestamp for trusted domain cache located.\n"));
		SAFE_FREE(value);
		return 0;
	} 

	timestamp = atoi(value);

	SAFE_FREE(value);
	return timestamp;
}

/*******************************************************************
 store the timestamp from the last update 
*******************************************************************/

bool trustdom_cache_store_timestamp( uint32 t, time_t timeout )
{
	fstring value;

	fstr_sprintf(value, "%d", t );

	if (!gencache_set(TDOMTSKEY, value, timeout)) {
		DEBUG(5, ("failed to set timestamp for trustdom_cache\n"));
		return False;
	} 

	return True;
}


/**
 * Delete single trustdom entry. Look at the
 * gencache_iterate definition.
 *
 **/

static void flush_trustdom_name(const char* key, const char *value, time_t timeout, void* dptr)
{
	gencache_del(key);
	DEBUG(5, ("Deleting entry %s\n", key));
}


/**
 * Flush all the trusted domains entries from the cache.
 **/

void trustdom_cache_flush(void)
{
	/* 
	 * iterate through each TDOM cache's entry and flush it
	 * by flush_trustdom_name function
	 */
	gencache_iterate(flush_trustdom_name, NULL, trustdom_cache_key("*"));
	DEBUG(5, ("Trusted domains cache flushed\n"));
}

/********************************************************************
 update the trustdom_cache if needed 
********************************************************************/
#define TRUSTDOM_UPDATE_INTERVAL	600

void update_trustdom_cache( void )
{
	char **domain_names;
	struct dom_sid *dom_sids;
	uint32 num_domains;
	uint32 last_check;
	int time_diff;
	TALLOC_CTX *mem_ctx = NULL;
	time_t now = time(NULL);
	int i;

	/* get the timestamp.  We have to initialise it if the last timestamp == 0 */	
	if ( (last_check = trustdom_cache_fetch_timestamp()) == 0 ) 
		trustdom_cache_store_timestamp(0, now+TRUSTDOM_UPDATE_INTERVAL);

	time_diff = (int) (now - last_check);

	if ( (time_diff > 0) && (time_diff < TRUSTDOM_UPDATE_INTERVAL) ) {
		DEBUG(10,("update_trustdom_cache: not time to update trustdom_cache yet\n"));
		return;
	}

	/* note that we don't lock the timestamp. This prevents this
	   smbd from blocking all other smbd daemons while we
	   enumerate the trusted domains */
	trustdom_cache_store_timestamp(now, now+TRUSTDOM_UPDATE_INTERVAL);

	if ( !(mem_ctx = talloc_init("update_trustdom_cache")) ) {
		DEBUG(0,("update_trustdom_cache: talloc_init() failed!\n"));
		goto done;
	}

	/* get the domains and store them */

	if ( enumerate_domain_trusts(mem_ctx, lp_workgroup(), &domain_names, 
		&num_domains, &dom_sids)) {
		for ( i=0; i<num_domains; i++ ) {
			trustdom_cache_store( domain_names[i], NULL, &dom_sids[i], 
				now+TRUSTDOM_UPDATE_INTERVAL);
		}		
	} else {
		/* we failed to fetch the list of trusted domains - restore the old
		   timestamp */
		trustdom_cache_store_timestamp(last_check, 
					       last_check+TRUSTDOM_UPDATE_INTERVAL);
	}

done:	
	talloc_destroy( mem_ctx );

	return;
}
