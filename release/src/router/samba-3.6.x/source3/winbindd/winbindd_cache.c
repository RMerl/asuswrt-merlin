/* 
   Unix SMB/CIFS implementation.

   Winbind cache backend functions

   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Gerald Carter   2003-2007
   Copyright (C) Volker Lendecke 2005
   Copyright (C) Guenther Deschner 2005
   Copyright (C) Michael Adam    2007

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
#include "system/filesys.h"
#include "winbindd.h"
#include "tdb_validate.h"
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/ndr_wbint.h"
#include "ads.h"
#include "nss_info.h"
#include "../libcli/security/security.h"
#include "passdb/machine_sid.h"
#include "util_tdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

#define WINBINDD_CACHE_VER1 1 /* initial db version */
#define WINBINDD_CACHE_VER2 2 /* second version with timeouts for NDR entries */

#define WINBINDD_CACHE_VERSION WINBINDD_CACHE_VER2
#define WINBINDD_CACHE_VERSION_KEYSTR "WINBINDD_CACHE_VERSION"

extern struct winbindd_methods reconnect_methods;
#ifdef HAVE_ADS
extern struct winbindd_methods ads_methods;
#endif
extern struct winbindd_methods builtin_passdb_methods;
extern struct winbindd_methods sam_passdb_methods;

/*
 * JRA. KEEP THIS LIST UP TO DATE IF YOU ADD CACHE ENTRIES.
 * Here are the list of entry types that are *not* stored
 * as form struct cache_entry in the cache.
 */

static const char *non_centry_keys[] = {
	"SEQNUM/",
	"DR/",
	"DE/",
	"WINBINDD_OFFLINE",
	WINBINDD_CACHE_VERSION_KEYSTR,
	NULL
};

/************************************************************************
 Is this key a non-centry type ?
************************************************************************/

static bool is_non_centry_key(TDB_DATA kbuf)
{
	int i;

	if (kbuf.dptr == NULL || kbuf.dsize == 0) {
		return false;
	}
	for (i = 0; non_centry_keys[i] != NULL; i++) {
		size_t namelen = strlen(non_centry_keys[i]);
		if (kbuf.dsize < namelen) {
			continue;
		}
		if (strncmp(non_centry_keys[i], (const char *)kbuf.dptr, namelen) == 0) {
			return true;
		}
	}
	return false;
}

/* Global online/offline state - False when online. winbindd starts up online
   and sets this to true if the first query fails and there's an entry in
   the cache tdb telling us to stay offline. */

static bool global_winbindd_offline_state;

struct winbind_cache {
	TDB_CONTEXT *tdb;
};

struct cache_entry {
	NTSTATUS status;
	uint32 sequence_number;
	uint64_t timeout;
	uint8 *data;
	uint32 len, ofs;
};

void (*smb_panic_fn)(const char *const why) = smb_panic;

#define WINBINDD_MAX_CACHE_SIZE (50*1024*1024)

static struct winbind_cache *wcache;

/* get the winbind_cache structure */
static struct winbind_cache *get_cache(struct winbindd_domain *domain)
{
	struct winbind_cache *ret = wcache;

	/* We have to know what type of domain we are dealing with first. */

	if (domain->internal) {
		domain->backend = &builtin_passdb_methods;
		domain->initialized = True;
	}

	if (strequal(domain->name, get_global_sam_name()) &&
	    sid_check_is_domain(&domain->sid)) {
		domain->backend = &sam_passdb_methods;
		domain->initialized = True;
	}

	if ( !domain->initialized ) {
		init_dc_connection( domain );
	}

	/* 
	   OK.  listen up becasue I'm only going to say this once.
	   We have the following scenarios to consider
	   (a) trusted AD domains on a Samba DC,
	   (b) trusted AD domains and we are joined to a non-kerberos domain
	   (c) trusted AD domains and we are joined to a kerberos (AD) domain

	   For (a) we can always contact the trusted domain using krb5 
	   since we have the domain trust account password

	   For (b) we can only use RPC since we have no way of 
	   getting a krb5 ticket in our own domain

	   For (c) we can always use krb5 since we have a kerberos trust

	   --jerry
	 */

	if (!domain->backend) {
#ifdef HAVE_ADS
		struct winbindd_domain *our_domain = domain;

		/* find our domain first so we can figure out if we 
		   are joined to a kerberized domain */

		if ( !domain->primary )
			our_domain = find_our_domain();

		if ((our_domain->active_directory || IS_DC)
		    && domain->active_directory
		    && !lp_winbind_rpc_only()) {
			DEBUG(5,("get_cache: Setting ADS methods for domain %s\n", domain->name));
			domain->backend = &ads_methods;
		} else {
#endif	/* HAVE_ADS */
			DEBUG(5,("get_cache: Setting MS-RPC methods for domain %s\n", domain->name));
			domain->backend = &reconnect_methods;
#ifdef HAVE_ADS
		}
#endif	/* HAVE_ADS */
	}

	if (ret)
		return ret;

	ret = SMB_XMALLOC_P(struct winbind_cache);
	ZERO_STRUCTP(ret);

	wcache = ret;
	wcache_flush_cache();

	return ret;
}

/*
  free a centry structure
*/
static void centry_free(struct cache_entry *centry)
{
	if (!centry)
		return;
	SAFE_FREE(centry->data);
	free(centry);
}

static bool centry_check_bytes(struct cache_entry *centry, size_t nbytes)
{
	if (centry->len - centry->ofs < nbytes) {
		DEBUG(0,("centry corruption? needed %u bytes, have %d\n", 
			 (unsigned int)nbytes,
			 centry->len - centry->ofs));
		return false;
	}
	return true;
}

/*
  pull a uint64_t from a cache entry
*/
static uint64_t centry_uint64_t(struct cache_entry *centry)
{
	uint64_t ret;

	if (!centry_check_bytes(centry, 8)) {
		smb_panic_fn("centry_uint64_t");
	}
	ret = BVAL(centry->data, centry->ofs);
	centry->ofs += 8;
	return ret;
}

/*
  pull a uint32 from a cache entry 
*/
static uint32 centry_uint32(struct cache_entry *centry)
{
	uint32 ret;

	if (!centry_check_bytes(centry, 4)) {
		smb_panic_fn("centry_uint32");
	}
	ret = IVAL(centry->data, centry->ofs);
	centry->ofs += 4;
	return ret;
}

/*
  pull a uint16 from a cache entry 
*/
static uint16 centry_uint16(struct cache_entry *centry)
{
	uint16 ret;
	if (!centry_check_bytes(centry, 2)) {
		smb_panic_fn("centry_uint16");
	}
	ret = SVAL(centry->data, centry->ofs);
	centry->ofs += 2;
	return ret;
}

/*
  pull a uint8 from a cache entry 
*/
static uint8 centry_uint8(struct cache_entry *centry)
{
	uint8 ret;
	if (!centry_check_bytes(centry, 1)) {
		smb_panic_fn("centry_uint8");
	}
	ret = CVAL(centry->data, centry->ofs);
	centry->ofs += 1;
	return ret;
}

/*
  pull a NTTIME from a cache entry 
*/
static NTTIME centry_nttime(struct cache_entry *centry)
{
	NTTIME ret;
	if (!centry_check_bytes(centry, 8)) {
		smb_panic_fn("centry_nttime");
	}
	ret = IVAL(centry->data, centry->ofs);
	centry->ofs += 4;
	ret += (uint64)IVAL(centry->data, centry->ofs) << 32;
	centry->ofs += 4;
	return ret;
}

/*
  pull a time_t from a cache entry. time_t stored portably as a 64-bit time.
*/
static time_t centry_time(struct cache_entry *centry)
{
	return (time_t)centry_nttime(centry);
}

/* pull a string from a cache entry, using the supplied
   talloc context 
*/
static char *centry_string(struct cache_entry *centry, TALLOC_CTX *mem_ctx)
{
	uint32 len;
	char *ret;

	len = centry_uint8(centry);

	if (len == 0xFF) {
		/* a deliberate NULL string */
		return NULL;
	}

	if (!centry_check_bytes(centry, (size_t)len)) {
		smb_panic_fn("centry_string");
	}

	ret = TALLOC_ARRAY(mem_ctx, char, len+1);
	if (!ret) {
		smb_panic_fn("centry_string out of memory\n");
	}
	memcpy(ret,centry->data + centry->ofs, len);
	ret[len] = 0;
	centry->ofs += len;
	return ret;
}

/* pull a hash16 from a cache entry, using the supplied
   talloc context 
*/
static char *centry_hash16(struct cache_entry *centry, TALLOC_CTX *mem_ctx)
{
	uint32 len;
	char *ret;

	len = centry_uint8(centry);

	if (len != 16) {
		DEBUG(0,("centry corruption? hash len (%u) != 16\n", 
			len ));
		return NULL;
	}

	if (!centry_check_bytes(centry, 16)) {
		return NULL;
	}

	ret = TALLOC_ARRAY(mem_ctx, char, 16);
	if (!ret) {
		smb_panic_fn("centry_hash out of memory\n");
	}
	memcpy(ret,centry->data + centry->ofs, 16);
	centry->ofs += 16;
	return ret;
}

/* pull a sid from a cache entry, using the supplied
   talloc context 
*/
static bool centry_sid(struct cache_entry *centry, struct dom_sid *sid)
{
	char *sid_string;
	bool ret;

	sid_string = centry_string(centry, talloc_tos());
	if (sid_string == NULL) {
		return false;
	}
	ret = string_to_sid(sid, sid_string);
	TALLOC_FREE(sid_string);
	return ret;
}


/*
  pull a NTSTATUS from a cache entry
*/
static NTSTATUS centry_ntstatus(struct cache_entry *centry)
{
	NTSTATUS status;

	status = NT_STATUS(centry_uint32(centry));
	return status;
}


/* the server is considered down if it can't give us a sequence number */
static bool wcache_server_down(struct winbindd_domain *domain)
{
	bool ret;

	if (!wcache->tdb)
		return false;

	ret = (domain->sequence_number == DOM_SEQUENCE_NONE);

	if (ret)
		DEBUG(10,("wcache_server_down: server for Domain %s down\n", 
			domain->name ));
	return ret;
}

static bool wcache_fetch_seqnum(const char *domain_name, uint32_t *seqnum,
				uint32_t *last_seq_check)
{
	char *key;
	TDB_DATA data;

	if (wcache->tdb == NULL) {
		DEBUG(10,("wcache_fetch_seqnum: tdb == NULL\n"));
		return false;
	}

	key = talloc_asprintf(talloc_tos(), "SEQNUM/%s", domain_name);
	if (key == NULL) {
		DEBUG(10, ("talloc failed\n"));
		return false;
	}

	data = tdb_fetch_bystring(wcache->tdb, key);
	TALLOC_FREE(key);

	if (data.dptr == NULL) {
		DEBUG(10, ("wcache_fetch_seqnum: %s not found\n",
			   domain_name));
		return false;
	}
	if (data.dsize != 8) {
		DEBUG(10, ("wcache_fetch_seqnum: invalid data size %d\n",
			   (int)data.dsize));
		SAFE_FREE(data.dptr);
		return false;
	}

	*seqnum = IVAL(data.dptr, 0);
	*last_seq_check = IVAL(data.dptr, 4);
	SAFE_FREE(data.dptr);

	return true;
}

static NTSTATUS fetch_cache_seqnum( struct winbindd_domain *domain, time_t now )
{
	uint32 last_check, time_diff;

	if (!wcache_fetch_seqnum(domain->name, &domain->sequence_number,
				 &last_check)) {
		return NT_STATUS_UNSUCCESSFUL;
	}
	domain->last_seq_check = last_check;

	/* have we expired? */

	time_diff = now - domain->last_seq_check;
	if ( time_diff > lp_winbind_cache_time() ) {
		DEBUG(10,("fetch_cache_seqnum: timeout [%s][%u @ %u]\n",
			domain->name, domain->sequence_number,
			(uint32)domain->last_seq_check));
		return NT_STATUS_UNSUCCESSFUL;
	}

	DEBUG(10,("fetch_cache_seqnum: success [%s][%u @ %u]\n", 
		domain->name, domain->sequence_number, 
		(uint32)domain->last_seq_check));

	return NT_STATUS_OK;
}

bool wcache_store_seqnum(const char *domain_name, uint32_t seqnum,
			 time_t last_seq_check)
{
	char *key_str;
	uint8_t buf[8];
	int ret;

	if (wcache->tdb == NULL) {
		DEBUG(10, ("wcache_store_seqnum: wcache->tdb == NULL\n"));
		return false;
	}

	key_str = talloc_asprintf(talloc_tos(), "SEQNUM/%s", domain_name);
	if (key_str == NULL) {
		DEBUG(10, ("talloc_asprintf failed\n"));
		return false;
	}

	SIVAL(buf, 0, seqnum);
	SIVAL(buf, 4, last_seq_check);

	ret = tdb_store_bystring(wcache->tdb, key_str,
				 make_tdb_data(buf, sizeof(buf)), TDB_REPLACE);
	TALLOC_FREE(key_str);
	if (ret == -1) {
		DEBUG(10, ("tdb_store_bystring failed: %s\n",
			   tdb_errorstr(wcache->tdb)));
		TALLOC_FREE(key_str);
		return false;
	}

	DEBUG(10, ("wcache_store_seqnum: success [%s][%u @ %u]\n",
		   domain_name, seqnum, (unsigned)last_seq_check));

	return true;
}

static bool store_cache_seqnum( struct winbindd_domain *domain )
{
	return wcache_store_seqnum(domain->name, domain->sequence_number,
				   domain->last_seq_check);
}

/*
  refresh the domain sequence number. If force is true
  then always refresh it, no matter how recently we fetched it
*/

static void refresh_sequence_number(struct winbindd_domain *domain, bool force)
{
	NTSTATUS status;
	unsigned time_diff;
	time_t t = time(NULL);
	unsigned cache_time = lp_winbind_cache_time();

	if (is_domain_offline(domain)) {
		return;
	}

	get_cache( domain );

#if 0	/* JERRY -- disable as the default cache time is now 5 minutes */
	/* trying to reconnect is expensive, don't do it too often */
	if (domain->sequence_number == DOM_SEQUENCE_NONE) {
		cache_time *= 8;
	}
#endif

	time_diff = t - domain->last_seq_check;

	/* see if we have to refetch the domain sequence number */
	if (!force && (time_diff < cache_time) &&
			(domain->sequence_number != DOM_SEQUENCE_NONE) &&
			NT_STATUS_IS_OK(domain->last_status)) {
		DEBUG(10, ("refresh_sequence_number: %s time ok\n", domain->name));
		goto done;
	}

	/* try to get the sequence number from the tdb cache first */
	/* this will update the timestamp as well */

	status = fetch_cache_seqnum( domain, t );
	if (NT_STATUS_IS_OK(status) &&
			(domain->sequence_number != DOM_SEQUENCE_NONE) &&
			NT_STATUS_IS_OK(domain->last_status)) {
		goto done;
	}

	/* important! make sure that we know if this is a native 
	   mode domain or not.  And that we can contact it. */

	if ( winbindd_can_contact_domain( domain ) ) {		
		status = domain->backend->sequence_number(domain, 
							  &domain->sequence_number);
	} else {
		/* just use the current time */
		status = NT_STATUS_OK;
		domain->sequence_number = time(NULL);
	}


	/* the above call could have set our domain->backend to NULL when
	 * coming from offline to online mode, make sure to reinitialize the
	 * backend - Guenther */
	get_cache( domain );

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("refresh_sequence_number: failed with %s\n", nt_errstr(status)));
		domain->sequence_number = DOM_SEQUENCE_NONE;
	}

	domain->last_status = status;
	domain->last_seq_check = time(NULL);

	/* save the new sequence number in the cache */
	store_cache_seqnum( domain );

done:
	DEBUG(10, ("refresh_sequence_number: %s seq number is now %d\n", 
		   domain->name, domain->sequence_number));

	return;
}

/*
  decide if a cache entry has expired
*/
static bool centry_expired(struct winbindd_domain *domain, const char *keystr, struct cache_entry *centry)
{
	/* If we've been told to be offline - stay in that state... */
	if (lp_winbind_offline_logon() && global_winbindd_offline_state) {
		DEBUG(10,("centry_expired: Key %s for domain %s valid as winbindd is globally offline.\n",
			keystr, domain->name ));
		return false;
	}

	/* when the domain is offline return the cached entry.
	 * This deals with transient offline states... */

	if (!domain->online) {
		DEBUG(10,("centry_expired: Key %s for domain %s valid as domain is offline.\n",
			keystr, domain->name ));
		return false;
	}

	/* if the server is OK and our cache entry came from when it was down then
	   the entry is invalid */
	if ((domain->sequence_number != DOM_SEQUENCE_NONE) &&  
	    (centry->sequence_number == DOM_SEQUENCE_NONE)) {
		DEBUG(10,("centry_expired: Key %s for domain %s invalid sequence.\n",
			keystr, domain->name ));
		return true;
	}

	/* if the server is down or the cache entry is not older than the
	   current sequence number or it did not timeout then it is OK */
	if (wcache_server_down(domain)
	    || ((centry->sequence_number == domain->sequence_number)
		&& (centry->timeout > time(NULL)))) {
		DEBUG(10,("centry_expired: Key %s for domain %s is good.\n",
			keystr, domain->name ));
		return false;
	}

	DEBUG(10,("centry_expired: Key %s for domain %s expired\n",
		keystr, domain->name ));

	/* it's expired */
	return true;
}

static struct cache_entry *wcache_fetch_raw(char *kstr)
{
	TDB_DATA data;
	struct cache_entry *centry;
	TDB_DATA key;

	key = string_tdb_data(kstr);
	data = tdb_fetch(wcache->tdb, key);
	if (!data.dptr) {
		/* a cache miss */
		return NULL;
	}

	centry = SMB_XMALLOC_P(struct cache_entry);
	centry->data = (unsigned char *)data.dptr;
	centry->len = data.dsize;
	centry->ofs = 0;

	if (centry->len < 16) {
		/* huh? corrupt cache? */
		DEBUG(10,("wcache_fetch_raw: Corrupt cache for key %s "
			  "(len < 16)?\n", kstr));
		centry_free(centry);
		return NULL;
	}

	centry->status = centry_ntstatus(centry);
	centry->sequence_number = centry_uint32(centry);
	centry->timeout = centry_uint64_t(centry);

	return centry;
}

static bool is_my_own_sam_domain(struct winbindd_domain *domain)
{
	if (strequal(domain->name, get_global_sam_name()) &&
	    sid_check_is_domain(&domain->sid)) {
		return true;
	}

	return false;
}

static bool is_builtin_domain(struct winbindd_domain *domain)
{
	if (strequal(domain->name, "BUILTIN") &&
	    sid_check_is_builtin(&domain->sid)) {
		return true;
	}

	return false;
}

/*
  fetch an entry from the cache, with a varargs key. auto-fetch the sequence
  number and return status
*/
static struct cache_entry *wcache_fetch(struct winbind_cache *cache, 
					struct winbindd_domain *domain,
					const char *format, ...) PRINTF_ATTRIBUTE(3,4);
static struct cache_entry *wcache_fetch(struct winbind_cache *cache, 
					struct winbindd_domain *domain,
					const char *format, ...)
{
	va_list ap;
	char *kstr;
	struct cache_entry *centry;

	if (!winbindd_use_cache() ||
	    is_my_own_sam_domain(domain) ||
	    is_builtin_domain(domain)) {
		return NULL;
	}

	refresh_sequence_number(domain, false);

	va_start(ap, format);
	smb_xvasprintf(&kstr, format, ap);
	va_end(ap);

	centry = wcache_fetch_raw(kstr);
	if (centry == NULL) {
		free(kstr);
		return NULL;
	}

	if (centry_expired(domain, kstr, centry)) {

		DEBUG(10,("wcache_fetch: entry %s expired for domain %s\n",
			 kstr, domain->name ));

		centry_free(centry);
		free(kstr);
		return NULL;
	}

	DEBUG(10,("wcache_fetch: returning entry %s for domain %s\n",
		 kstr, domain->name ));

	free(kstr);
	return centry;
}

static void wcache_delete(const char *format, ...) PRINTF_ATTRIBUTE(1,2);
static void wcache_delete(const char *format, ...)
{
	va_list ap;
	char *kstr;
	TDB_DATA key;

	va_start(ap, format);
	smb_xvasprintf(&kstr, format, ap);
	va_end(ap);

	key = string_tdb_data(kstr);

	tdb_delete(wcache->tdb, key);
	free(kstr);
}

/*
  make sure we have at least len bytes available in a centry 
*/
static void centry_expand(struct cache_entry *centry, uint32 len)
{
	if (centry->len - centry->ofs >= len)
		return;
	centry->len *= 2;
	centry->data = SMB_REALLOC_ARRAY(centry->data, unsigned char,
					 centry->len);
	if (!centry->data) {
		DEBUG(0,("out of memory: needed %d bytes in centry_expand\n", centry->len));
		smb_panic_fn("out of memory in centry_expand");
	}
}

/*
  push a uint64_t into a centry
*/
static void centry_put_uint64_t(struct cache_entry *centry, uint64_t v)
{
	centry_expand(centry, 8);
	SBVAL(centry->data, centry->ofs, v);
	centry->ofs += 8;
}

/*
  push a uint32 into a centry 
*/
static void centry_put_uint32(struct cache_entry *centry, uint32 v)
{
	centry_expand(centry, 4);
	SIVAL(centry->data, centry->ofs, v);
	centry->ofs += 4;
}

/*
  push a uint16 into a centry 
*/
static void centry_put_uint16(struct cache_entry *centry, uint16 v)
{
	centry_expand(centry, 2);
	SSVAL(centry->data, centry->ofs, v);
	centry->ofs += 2;
}

/*
  push a uint8 into a centry 
*/
static void centry_put_uint8(struct cache_entry *centry, uint8 v)
{
	centry_expand(centry, 1);
	SCVAL(centry->data, centry->ofs, v);
	centry->ofs += 1;
}

/* 
   push a string into a centry 
 */
static void centry_put_string(struct cache_entry *centry, const char *s)
{
	int len;

	if (!s) {
		/* null strings are marked as len 0xFFFF */
		centry_put_uint8(centry, 0xFF);
		return;
	}

	len = strlen(s);
	/* can't handle more than 254 char strings. Truncating is probably best */
	if (len > 254) {
		DEBUG(10,("centry_put_string: truncating len (%d) to: 254\n", len));
		len = 254;
	}
	centry_put_uint8(centry, len);
	centry_expand(centry, len);
	memcpy(centry->data + centry->ofs, s, len);
	centry->ofs += len;
}

/* 
   push a 16 byte hash into a centry - treat as 16 byte string.
 */
static void centry_put_hash16(struct cache_entry *centry, const uint8 val[16])
{
	centry_put_uint8(centry, 16);
	centry_expand(centry, 16);
	memcpy(centry->data + centry->ofs, val, 16);
	centry->ofs += 16;
}

static void centry_put_sid(struct cache_entry *centry, const struct dom_sid *sid)
{
	fstring sid_string;
	centry_put_string(centry, sid_to_fstring(sid_string, sid));
}


/*
  put NTSTATUS into a centry
*/
static void centry_put_ntstatus(struct cache_entry *centry, NTSTATUS status)
{
	uint32 status_value = NT_STATUS_V(status);
	centry_put_uint32(centry, status_value);
}


/*
  push a NTTIME into a centry 
*/
static void centry_put_nttime(struct cache_entry *centry, NTTIME nt)
{
	centry_expand(centry, 8);
	SIVAL(centry->data, centry->ofs, nt & 0xFFFFFFFF);
	centry->ofs += 4;
	SIVAL(centry->data, centry->ofs, nt >> 32);
	centry->ofs += 4;
}

/*
  push a time_t into a centry - use a 64 bit size.
  NTTIME here is being used as a convenient 64-bit size.
*/
static void centry_put_time(struct cache_entry *centry, time_t t)
{
	NTTIME nt = (NTTIME)t;
	centry_put_nttime(centry, nt);
}

/*
  start a centry for output. When finished, call centry_end()
*/
struct cache_entry *centry_start(struct winbindd_domain *domain, NTSTATUS status)
{
	struct cache_entry *centry;

	if (!wcache->tdb)
		return NULL;

	centry = SMB_XMALLOC_P(struct cache_entry);

	centry->len = 8192; /* reasonable default */
	centry->data = SMB_XMALLOC_ARRAY(uint8, centry->len);
	centry->ofs = 0;
	centry->sequence_number = domain->sequence_number;
	centry->timeout = lp_winbind_cache_time() + time(NULL);
	centry_put_ntstatus(centry, status);
	centry_put_uint32(centry, centry->sequence_number);
	centry_put_uint64_t(centry, centry->timeout);
	return centry;
}

/*
  finish a centry and write it to the tdb
*/
static void centry_end(struct cache_entry *centry, const char *format, ...) PRINTF_ATTRIBUTE(2,3);
static void centry_end(struct cache_entry *centry, const char *format, ...)
{
	va_list ap;
	char *kstr;
	TDB_DATA key, data;

	if (!winbindd_use_cache()) {
		return;
	}

	va_start(ap, format);
	smb_xvasprintf(&kstr, format, ap);
	va_end(ap);

	key = string_tdb_data(kstr);
	data.dptr = centry->data;
	data.dsize = centry->ofs;

	tdb_store(wcache->tdb, key, data, TDB_REPLACE);
	free(kstr);
}

static void wcache_save_name_to_sid(struct winbindd_domain *domain, 
				    NTSTATUS status, const char *domain_name,
				    const char *name, const struct dom_sid *sid,
				    enum lsa_SidType type)
{
	struct cache_entry *centry;
	fstring uname;

	centry = centry_start(domain, status);
	if (!centry)
		return;

	if ((domain_name == NULL) || (domain_name[0] == '\0')) {
		struct winbindd_domain *mydomain =
			find_domain_from_sid_noinit(sid);
		if (mydomain != NULL) {
			domain_name = mydomain->name;
		}
	}

	centry_put_uint32(centry, type);
	centry_put_sid(centry, sid);
	fstrcpy(uname, name);
	strupper_m(uname);
	centry_end(centry, "NS/%s/%s", domain_name, uname);
	DEBUG(10,("wcache_save_name_to_sid: %s\\%s -> %s (%s)\n", domain_name,
		  uname, sid_string_dbg(sid), nt_errstr(status)));
	centry_free(centry);
}

static void wcache_save_sid_to_name(struct winbindd_domain *domain, NTSTATUS status, 
				    const struct dom_sid *sid, const char *domain_name, const char *name, enum lsa_SidType type)
{
	struct cache_entry *centry;
	fstring sid_string;

	centry = centry_start(domain, status);
	if (!centry)
		return;

	if ((domain_name == NULL) || (domain_name[0] == '\0')) {
		struct winbindd_domain *mydomain =
			find_domain_from_sid_noinit(sid);
		if (mydomain != NULL) {
			domain_name = mydomain->name;
		}
	}

	if (NT_STATUS_IS_OK(status)) {
		centry_put_uint32(centry, type);
		centry_put_string(centry, domain_name);
		centry_put_string(centry, name);
	}

	centry_end(centry, "SN/%s", sid_to_fstring(sid_string, sid));
	DEBUG(10,("wcache_save_sid_to_name: %s -> %s (%s)\n", sid_string, 
		  name, nt_errstr(status)));
	centry_free(centry);
}


static void wcache_save_user(struct winbindd_domain *domain, NTSTATUS status,
			     struct wbint_userinfo *info)
{
	struct cache_entry *centry;
	fstring sid_string;

	if (is_null_sid(&info->user_sid)) {
		return;
	}

	centry = centry_start(domain, status);
	if (!centry)
		return;
	centry_put_string(centry, info->acct_name);
	centry_put_string(centry, info->full_name);
	centry_put_string(centry, info->homedir);
	centry_put_string(centry, info->shell);
	centry_put_uint32(centry, info->primary_gid);
	centry_put_sid(centry, &info->user_sid);
	centry_put_sid(centry, &info->group_sid);
	centry_end(centry, "U/%s", sid_to_fstring(sid_string,
						  &info->user_sid));
	DEBUG(10,("wcache_save_user: %s (acct_name %s)\n", sid_string, info->acct_name));
	centry_free(centry);
}

static void wcache_save_lockout_policy(struct winbindd_domain *domain,
				       NTSTATUS status,
				       struct samr_DomInfo12 *lockout_policy)
{
	struct cache_entry *centry;

	centry = centry_start(domain, status);
	if (!centry)
		return;

	centry_put_nttime(centry, lockout_policy->lockout_duration);
	centry_put_nttime(centry, lockout_policy->lockout_window);
	centry_put_uint16(centry, lockout_policy->lockout_threshold);

	centry_end(centry, "LOC_POL/%s", domain->name);

	DEBUG(10,("wcache_save_lockout_policy: %s\n", domain->name));

	centry_free(centry);
}



static void wcache_save_password_policy(struct winbindd_domain *domain,
					NTSTATUS status,
					struct samr_DomInfo1 *policy)
{
	struct cache_entry *centry;

	centry = centry_start(domain, status);
	if (!centry)
		return;

	centry_put_uint16(centry, policy->min_password_length);
	centry_put_uint16(centry, policy->password_history_length);
	centry_put_uint32(centry, policy->password_properties);
	centry_put_nttime(centry, policy->max_password_age);
	centry_put_nttime(centry, policy->min_password_age);

	centry_end(centry, "PWD_POL/%s", domain->name);

	DEBUG(10,("wcache_save_password_policy: %s\n", domain->name));

	centry_free(centry);
}

/***************************************************************************
 ***************************************************************************/

static void wcache_save_username_alias(struct winbindd_domain *domain,
				       NTSTATUS status,
				       const char *name, const char *alias)
{
	struct cache_entry *centry;
	fstring uname;

	if ( (centry = centry_start(domain, status)) == NULL )
		return;

	centry_put_string( centry, alias );

	fstrcpy(uname, name);
	strupper_m(uname);
	centry_end(centry, "NSS/NA/%s", uname);

	DEBUG(10,("wcache_save_username_alias: %s -> %s\n", name, alias ));

	centry_free(centry);
}

static void wcache_save_alias_username(struct winbindd_domain *domain,
				       NTSTATUS status,
				       const char *alias, const char *name)
{
	struct cache_entry *centry;
	fstring uname;

	if ( (centry = centry_start(domain, status)) == NULL )
		return;

	centry_put_string( centry, name );

	fstrcpy(uname, alias);
	strupper_m(uname);
	centry_end(centry, "NSS/AN/%s", uname);

	DEBUG(10,("wcache_save_alias_username: %s -> %s\n", alias, name ));

	centry_free(centry);
}

/***************************************************************************
 ***************************************************************************/

NTSTATUS resolve_username_to_alias( TALLOC_CTX *mem_ctx,
				    struct winbindd_domain *domain,
				    const char *name, char **alias )
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	char *upper_name;

	if ( domain->internal )
		return NT_STATUS_NOT_SUPPORTED;

	if (!cache->tdb)
		goto do_query;

	if ( (upper_name = SMB_STRDUP(name)) == NULL )
		return NT_STATUS_NO_MEMORY;
	strupper_m(upper_name);

	centry = wcache_fetch(cache, domain, "NSS/NA/%s", upper_name);

	SAFE_FREE( upper_name );

	if (!centry)
		goto do_query;

	status = centry->status;

	if (!NT_STATUS_IS_OK(status)) {
		centry_free(centry);
		return status;
	}

	*alias = centry_string( centry, mem_ctx );

	centry_free(centry);

	DEBUG(10,("resolve_username_to_alias: [Cached] - mapped %s to %s\n",
		  name, *alias ? *alias : "(none)"));

	return (*alias) ? NT_STATUS_OK : NT_STATUS_OBJECT_NAME_NOT_FOUND;

do_query:

	/* If its not in cache and we are offline, then fail */

	if ( get_global_winbindd_state_offline() || !domain->online ) {
		DEBUG(8,("resolve_username_to_alias: rejecting query "
			 "in offline mode\n"));
		return NT_STATUS_NOT_FOUND;
	}

	status = nss_map_to_alias( mem_ctx, domain->name, name, alias );

	if ( NT_STATUS_IS_OK( status ) ) {
		wcache_save_username_alias(domain, status, name, *alias);
	}

	if ( NT_STATUS_EQUAL( status, NT_STATUS_NONE_MAPPED ) ) {
		wcache_save_username_alias(domain, status, name, "(NULL)");
	}

	DEBUG(5,("resolve_username_to_alias: backend query returned %s\n",
		 nt_errstr(status)));

	if ( NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND) ) {
		set_domain_offline( domain );
	}

	return status;
}

/***************************************************************************
 ***************************************************************************/

NTSTATUS resolve_alias_to_username( TALLOC_CTX *mem_ctx,
				    struct winbindd_domain *domain,
				    const char *alias, char **name )
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	char *upper_name;

	if ( domain->internal )
		return  NT_STATUS_NOT_SUPPORTED;

	if (!cache->tdb)
		goto do_query;

	if ( (upper_name = SMB_STRDUP(alias)) == NULL )
		return NT_STATUS_NO_MEMORY;
	strupper_m(upper_name);

	centry = wcache_fetch(cache, domain, "NSS/AN/%s", upper_name);

	SAFE_FREE( upper_name );

	if (!centry)
		goto do_query;

	status = centry->status;

	if (!NT_STATUS_IS_OK(status)) {
		centry_free(centry);
		return status;
	}

	*name = centry_string( centry, mem_ctx );

	centry_free(centry);

	DEBUG(10,("resolve_alias_to_username: [Cached] - mapped %s to %s\n",
		  alias, *name ? *name : "(none)"));

	return (*name) ? NT_STATUS_OK : NT_STATUS_OBJECT_NAME_NOT_FOUND;

do_query:

	/* If its not in cache and we are offline, then fail */

	if ( get_global_winbindd_state_offline() || !domain->online ) {
		DEBUG(8,("resolve_alias_to_username: rejecting query "
			 "in offline mode\n"));
		return NT_STATUS_NOT_FOUND;
	}

	/* an alias cannot contain a domain prefix or '@' */

	if (strchr(alias, '\\') || strchr(alias, '@')) {
		DEBUG(10,("resolve_alias_to_username: skipping fully "
			  "qualified name %s\n", alias));
		return NT_STATUS_OBJECT_NAME_INVALID;
	}

	status = nss_map_from_alias( mem_ctx, domain->name, alias, name );

	if ( NT_STATUS_IS_OK( status ) ) {
		wcache_save_alias_username( domain, status, alias, *name );
	}

	if (NT_STATUS_EQUAL(status, NT_STATUS_NONE_MAPPED)) {
		wcache_save_alias_username(domain, status, alias, "(NULL)");
	}

	DEBUG(5,("resolve_alias_to_username: backend query returned %s\n",
		 nt_errstr(status)));

	if ( NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND) ) {
		set_domain_offline( domain );
	}

	return status;
}

NTSTATUS wcache_cached_creds_exist(struct winbindd_domain *domain, const struct dom_sid *sid)
{
	struct winbind_cache *cache = get_cache(domain);
	TDB_DATA data;
	fstring key_str, tmp;
	uint32 rid;

	if (!cache->tdb) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	if (is_null_sid(sid)) {
		return NT_STATUS_INVALID_SID;
	}

	if (!(sid_peek_rid(sid, &rid)) || (rid == 0)) {
		return NT_STATUS_INVALID_SID;
	}

	fstr_sprintf(key_str, "CRED/%s", sid_to_fstring(tmp, sid));

	data = tdb_fetch(cache->tdb, string_tdb_data(key_str));
	if (!data.dptr) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	SAFE_FREE(data.dptr);
	return NT_STATUS_OK;
}

/* Lookup creds for a SID - copes with old (unsalted) creds as well
   as new salted ones. */

NTSTATUS wcache_get_creds(struct winbindd_domain *domain, 
			  TALLOC_CTX *mem_ctx, 
			  const struct dom_sid *sid,
			  const uint8 **cached_nt_pass,
			  const uint8 **cached_salt)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	time_t t;
	uint32 rid;
	fstring tmp;

	if (!cache->tdb) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	if (is_null_sid(sid)) {
		return NT_STATUS_INVALID_SID;
	}

	if (!(sid_peek_rid(sid, &rid)) || (rid == 0)) {
		return NT_STATUS_INVALID_SID;
	}

	/* Try and get a salted cred first. If we can't
	   fall back to an unsalted cred. */

	centry = wcache_fetch(cache, domain, "CRED/%s",
			      sid_to_fstring(tmp, sid));
	if (!centry) {
		DEBUG(10,("wcache_get_creds: entry for [CRED/%s] not found\n", 
			  sid_string_dbg(sid)));
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	t = centry_time(centry);

	/* In the salted case this isn't actually the nt_hash itself,
	   but the MD5 of the salt + nt_hash. Let the caller
	   sort this out. It can tell as we only return the cached_salt
	   if we are returning a salted cred. */

	*cached_nt_pass = (const uint8 *)centry_hash16(centry, mem_ctx);
	if (*cached_nt_pass == NULL) {
		fstring sidstr;

		sid_to_fstring(sidstr, sid);

		/* Bad (old) cred cache. Delete and pretend we
		   don't have it. */
		DEBUG(0,("wcache_get_creds: bad entry for [CRED/%s] - deleting\n", 
				sidstr));
		wcache_delete("CRED/%s", sidstr);
		centry_free(centry);
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	/* We only have 17 bytes more data in the salted cred case. */
	if (centry->len - centry->ofs == 17) {
		*cached_salt = (const uint8 *)centry_hash16(centry, mem_ctx);
	} else {
		*cached_salt = NULL;
	}

	dump_data_pw("cached_nt_pass", *cached_nt_pass, NT_HASH_LEN);
	if (*cached_salt) {
		dump_data_pw("cached_salt", *cached_salt, NT_HASH_LEN);
	}

	status = centry->status;

	DEBUG(10,("wcache_get_creds: [Cached] - cached creds for user %s status: %s\n",
		  sid_string_dbg(sid), nt_errstr(status) ));

	centry_free(centry);
	return status;
}

/* Store creds for a SID - only writes out new salted ones. */

NTSTATUS wcache_save_creds(struct winbindd_domain *domain, 
			   const struct dom_sid *sid,
			   const uint8 nt_pass[NT_HASH_LEN])
{
	struct cache_entry *centry;
	fstring sid_string;
	uint32 rid;
	uint8 cred_salt[NT_HASH_LEN];
	uint8 salted_hash[NT_HASH_LEN];

	if (is_null_sid(sid)) {
		return NT_STATUS_INVALID_SID;
	}

	if (!(sid_peek_rid(sid, &rid)) || (rid == 0)) {
		return NT_STATUS_INVALID_SID;
	}

	centry = centry_start(domain, NT_STATUS_OK);
	if (!centry) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	dump_data_pw("nt_pass", nt_pass, NT_HASH_LEN);

	centry_put_time(centry, time(NULL));

	/* Create a salt and then salt the hash. */
	generate_random_buffer(cred_salt, NT_HASH_LEN);
	E_md5hash(cred_salt, nt_pass, salted_hash);

	centry_put_hash16(centry, salted_hash);
	centry_put_hash16(centry, cred_salt);
	centry_end(centry, "CRED/%s", sid_to_fstring(sid_string, sid));

	DEBUG(10,("wcache_save_creds: %s\n", sid_string));

	centry_free(centry);

	return NT_STATUS_OK;
}


/* Query display info. This is the basic user list fn */
static NTSTATUS query_user_list(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries, 
				struct wbint_userinfo **info)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	unsigned int i, retry;
	bool old_status = domain->online;

	if (!cache->tdb)
		goto do_query;

	centry = wcache_fetch(cache, domain, "UL/%s", domain->name);
	if (!centry)
		goto do_query;

do_fetch_cache:
	*num_entries = centry_uint32(centry);

	if (*num_entries == 0)
		goto do_cached;

	(*info) = TALLOC_ARRAY(mem_ctx, struct wbint_userinfo, *num_entries);
	if (! (*info)) {
		smb_panic_fn("query_user_list out of memory");
	}
	for (i=0; i<(*num_entries); i++) {
		(*info)[i].acct_name = centry_string(centry, mem_ctx);
		(*info)[i].full_name = centry_string(centry, mem_ctx);
		(*info)[i].homedir = centry_string(centry, mem_ctx);
		(*info)[i].shell = centry_string(centry, mem_ctx);
		centry_sid(centry, &(*info)[i].user_sid);
		centry_sid(centry, &(*info)[i].group_sid);
	}

do_cached:	
	status = centry->status;

	DEBUG(10,("query_user_list: [Cached] - cached list for domain %s status: %s\n",
		domain->name, nt_errstr(status) ));

	centry_free(centry);
	return status;

do_query:
	*num_entries = 0;
	*info = NULL;

	/* Return status value returned by seq number check */

	if (!NT_STATUS_IS_OK(domain->last_status))
		return domain->last_status;

	/* Put the query_user_list() in a retry loop.  There appears to be
	 * some bug either with Windows 2000 or Samba's handling of large
	 * rpc replies.  This manifests itself as sudden disconnection
	 * at a random point in the enumeration of a large (60k) user list.
	 * The retry loop simply tries the operation again. )-:  It's not
	 * pretty but an acceptable workaround until we work out what the
	 * real problem is. */

	retry = 0;
	do {

		DEBUG(10,("query_user_list: [Cached] - doing backend query for list for domain %s\n",
			domain->name ));

		status = domain->backend->query_user_list(domain, mem_ctx, num_entries, info);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(3, ("query_user_list: returned 0x%08x, "
				  "retrying\n", NT_STATUS_V(status)));
		}
		if (NT_STATUS_EQUAL(status, NT_STATUS_UNSUCCESSFUL)) {
			DEBUG(3, ("query_user_list: flushing "
				  "connection cache\n"));
			invalidate_cm_connection(&domain->conn);
		}
		if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
		    NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
			if (!domain->internal && old_status) {
				set_domain_offline(domain);
			}
			/* store partial response. */
			if (*num_entries > 0) {
				/*
				 * humm, what about the status used for cache?
				 * Should it be NT_STATUS_OK?
				 */
				break;
			}
			/*
			 * domain is offline now, and there is no user entries,
			 * try to fetch from cache again.
			 */
			if (cache->tdb && !domain->online && !domain->internal && old_status) {
				centry = wcache_fetch(cache, domain, "UL/%s", domain->name);
				/* partial response... */
				if (!centry) {
					goto skip_save;
				} else {
					goto do_fetch_cache;
				}
			} else {
				goto skip_save;
			}
		}

	} while (NT_STATUS_V(status) == NT_STATUS_V(NT_STATUS_UNSUCCESSFUL) && 
		 (retry++ < 5));

	/* and save it */
	refresh_sequence_number(domain, false);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	centry = centry_start(domain, status);
	if (!centry)
		goto skip_save;
	centry_put_uint32(centry, *num_entries);
	for (i=0; i<(*num_entries); i++) {
		centry_put_string(centry, (*info)[i].acct_name);
		centry_put_string(centry, (*info)[i].full_name);
		centry_put_string(centry, (*info)[i].homedir);
		centry_put_string(centry, (*info)[i].shell);
		centry_put_sid(centry, &(*info)[i].user_sid);
		centry_put_sid(centry, &(*info)[i].group_sid);
		if (domain->backend && domain->backend->consistent) {
			/* when the backend is consistent we can pre-prime some mappings */
			wcache_save_name_to_sid(domain, NT_STATUS_OK, 
						domain->name,
						(*info)[i].acct_name, 
						&(*info)[i].user_sid,
						SID_NAME_USER);
			wcache_save_sid_to_name(domain, NT_STATUS_OK, 
						&(*info)[i].user_sid,
						domain->name,
						(*info)[i].acct_name, 
						SID_NAME_USER);
			wcache_save_user(domain, NT_STATUS_OK, &(*info)[i]);
		}
	}	
	centry_end(centry, "UL/%s", domain->name);
	centry_free(centry);

skip_save:
	return status;
}

/* list all domain groups */
static NTSTATUS enum_dom_groups(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries, 
				struct wb_acct_info **info)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	unsigned int i;
	bool old_status;

	old_status = domain->online;
	if (!cache->tdb)
		goto do_query;

	centry = wcache_fetch(cache, domain, "GL/%s/domain", domain->name);
	if (!centry)
		goto do_query;

do_fetch_cache:
	*num_entries = centry_uint32(centry);

	if (*num_entries == 0)
		goto do_cached;

	(*info) = TALLOC_ARRAY(mem_ctx, struct wb_acct_info, *num_entries);
	if (! (*info)) {
		smb_panic_fn("enum_dom_groups out of memory");
	}
	for (i=0; i<(*num_entries); i++) {
		fstrcpy((*info)[i].acct_name, centry_string(centry, mem_ctx));
		fstrcpy((*info)[i].acct_desc, centry_string(centry, mem_ctx));
		(*info)[i].rid = centry_uint32(centry);
	}

do_cached:	
	status = centry->status;

	DEBUG(10,("enum_dom_groups: [Cached] - cached list for domain %s status: %s\n",
		domain->name, nt_errstr(status) ));

	centry_free(centry);
	return status;

do_query:
	*num_entries = 0;
	*info = NULL;

	/* Return status value returned by seq number check */

	if (!NT_STATUS_IS_OK(domain->last_status))
		return domain->last_status;

	DEBUG(10,("enum_dom_groups: [Cached] - doing backend query for list for domain %s\n",
		domain->name ));

	status = domain->backend->enum_dom_groups(domain, mem_ctx, num_entries, info);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (cache->tdb &&
			!domain->online &&
			!domain->internal &&
			old_status) {
			centry = wcache_fetch(cache, domain, "GL/%s/domain", domain->name);
			if (centry) {
				goto do_fetch_cache;
			}
		}
	}
	/* and save it */
	refresh_sequence_number(domain, false);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	centry = centry_start(domain, status);
	if (!centry)
		goto skip_save;
	centry_put_uint32(centry, *num_entries);
	for (i=0; i<(*num_entries); i++) {
		centry_put_string(centry, (*info)[i].acct_name);
		centry_put_string(centry, (*info)[i].acct_desc);
		centry_put_uint32(centry, (*info)[i].rid);
	}	
	centry_end(centry, "GL/%s/domain", domain->name);
	centry_free(centry);

skip_save:
	return status;
}

/* list all domain groups */
static NTSTATUS enum_local_groups(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				uint32 *num_entries, 
				struct wb_acct_info **info)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	unsigned int i;
	bool old_status;

	old_status = domain->online;
	if (!cache->tdb)
		goto do_query;

	centry = wcache_fetch(cache, domain, "GL/%s/local", domain->name);
	if (!centry)
		goto do_query;

do_fetch_cache:
	*num_entries = centry_uint32(centry);

	if (*num_entries == 0)
		goto do_cached;

	(*info) = TALLOC_ARRAY(mem_ctx, struct wb_acct_info, *num_entries);
	if (! (*info)) {
		smb_panic_fn("enum_dom_groups out of memory");
	}
	for (i=0; i<(*num_entries); i++) {
		fstrcpy((*info)[i].acct_name, centry_string(centry, mem_ctx));
		fstrcpy((*info)[i].acct_desc, centry_string(centry, mem_ctx));
		(*info)[i].rid = centry_uint32(centry);
	}

do_cached:	

	/* If we are returning cached data and the domain controller
	   is down then we don't know whether the data is up to date
	   or not.  Return NT_STATUS_MORE_PROCESSING_REQUIRED to
	   indicate this. */

	if (wcache_server_down(domain)) {
		DEBUG(10, ("enum_local_groups: returning cached user list and server was down\n"));
		status = NT_STATUS_MORE_PROCESSING_REQUIRED;
	} else
		status = centry->status;

	DEBUG(10,("enum_local_groups: [Cached] - cached list for domain %s status: %s\n",
		domain->name, nt_errstr(status) ));

	centry_free(centry);
	return status;

do_query:
	*num_entries = 0;
	*info = NULL;

	/* Return status value returned by seq number check */

	if (!NT_STATUS_IS_OK(domain->last_status))
		return domain->last_status;

	DEBUG(10,("enum_local_groups: [Cached] - doing backend query for list for domain %s\n",
		domain->name ));

	status = domain->backend->enum_local_groups(domain, mem_ctx, num_entries, info);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
		NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (cache->tdb &&
			!domain->internal &&
			!domain->online &&
			old_status) {
			centry = wcache_fetch(cache, domain, "GL/%s/local", domain->name);
			if (centry) {
				goto do_fetch_cache;
			}
		}
	}
	/* and save it */
	refresh_sequence_number(domain, false);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	centry = centry_start(domain, status);
	if (!centry)
		goto skip_save;
	centry_put_uint32(centry, *num_entries);
	for (i=0; i<(*num_entries); i++) {
		centry_put_string(centry, (*info)[i].acct_name);
		centry_put_string(centry, (*info)[i].acct_desc);
		centry_put_uint32(centry, (*info)[i].rid);
	}
	centry_end(centry, "GL/%s/local", domain->name);
	centry_free(centry);

skip_save:
	return status;
}

NTSTATUS wcache_name_to_sid(struct winbindd_domain *domain,
			    const char *domain_name,
			    const char *name,
			    struct dom_sid *sid,
			    enum lsa_SidType *type)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry;
	NTSTATUS status;
	char *uname;

	if (cache->tdb == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	uname = talloc_strdup_upper(talloc_tos(), name);
	if (uname == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if ((domain_name == NULL) || (domain_name[0] == '\0')) {
		domain_name = domain->name;
	}

	centry = wcache_fetch(cache, domain, "NS/%s/%s", domain_name, uname);
	TALLOC_FREE(uname);
	if (centry == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	status = centry->status;
	if (NT_STATUS_IS_OK(status)) {
		*type = (enum lsa_SidType)centry_uint32(centry);
		centry_sid(centry, sid);
	}

	DEBUG(10,("name_to_sid: [Cached] - cached name for domain %s status: "
		  "%s\n", domain->name, nt_errstr(status) ));

	centry_free(centry);
	return status;
}

/* convert a single name to a sid in a domain */
static NTSTATUS name_to_sid(struct winbindd_domain *domain,
			    TALLOC_CTX *mem_ctx,
			    const char *domain_name,
			    const char *name,
			    uint32_t flags,
			    struct dom_sid *sid,
			    enum lsa_SidType *type)
{
	NTSTATUS status;
	bool old_status;

	old_status = domain->online;

	status = wcache_name_to_sid(domain, domain_name, name, sid, type);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		return status;
	}

	ZERO_STRUCTP(sid);

	/* If the seq number check indicated that there is a problem
	 * with this DC, then return that status... except for
	 * access_denied.  This is special because the dc may be in
	 * "restrict anonymous = 1" mode, in which case it will deny
	 * most unauthenticated operations, but *will* allow the LSA
	 * name-to-sid that we try as a fallback. */

	if (!(NT_STATUS_IS_OK(domain->last_status)
	      || NT_STATUS_EQUAL(domain->last_status, NT_STATUS_ACCESS_DENIED)))
		return domain->last_status;

	DEBUG(10,("name_to_sid: [Cached] - doing backend query for name for domain %s\n",
		domain->name ));

	status = domain->backend->name_to_sid(domain, mem_ctx, domain_name,
					      name, flags, sid, type);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
		NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (!domain->internal &&
			!domain->online &&
			old_status) {
			NTSTATUS cache_status;
			cache_status = wcache_name_to_sid(domain, domain_name, name, sid, type);
			return cache_status;
		}
	}
	/* and save it */
	refresh_sequence_number(domain, false);

	if (domain->online &&
	    (NT_STATUS_IS_OK(status) || NT_STATUS_EQUAL(status, NT_STATUS_NONE_MAPPED))) {
		wcache_save_name_to_sid(domain, status, domain_name, name, sid, *type);

		/* Only save the reverse mapping if this was not a UPN */
		if (!strchr(name, '@')) {
			strupper_m(CONST_DISCARD(char *,domain_name));
			strlower_m(CONST_DISCARD(char *,name));
			wcache_save_sid_to_name(domain, status, sid, domain_name, name, *type);
		}
	}

	return status;
}

NTSTATUS wcache_sid_to_name(struct winbindd_domain *domain,
			    const struct dom_sid *sid,
			    TALLOC_CTX *mem_ctx,
			    char **domain_name,
			    char **name,
			    enum lsa_SidType *type)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry;
	char *sid_string;
	NTSTATUS status;

	if (cache->tdb == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	sid_string = sid_string_tos(sid);
	if (sid_string == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	centry = wcache_fetch(cache, domain, "SN/%s", sid_string);
	TALLOC_FREE(sid_string);
	if (centry == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	if (NT_STATUS_IS_OK(centry->status)) {
		*type = (enum lsa_SidType)centry_uint32(centry);
		*domain_name = centry_string(centry, mem_ctx);
		*name = centry_string(centry, mem_ctx);
	}

	status = centry->status;
	centry_free(centry);

	DEBUG(10,("sid_to_name: [Cached] - cached name for domain %s status: "
		  "%s\n", domain->name, nt_errstr(status) ));

	return status;
}

/* convert a sid to a user or group name. The sid is guaranteed to be in the domain
   given */
static NTSTATUS sid_to_name(struct winbindd_domain *domain,
			    TALLOC_CTX *mem_ctx,
			    const struct dom_sid *sid,
			    char **domain_name,
			    char **name,
			    enum lsa_SidType *type)
{
	NTSTATUS status;
	bool old_status;

	old_status = domain->online;
	status = wcache_sid_to_name(domain, sid, mem_ctx, domain_name, name,
				    type);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		return status;
	}

	*name = NULL;
	*domain_name = NULL;

	/* If the seq number check indicated that there is a problem
	 * with this DC, then return that status... except for
	 * access_denied.  This is special because the dc may be in
	 * "restrict anonymous = 1" mode, in which case it will deny
	 * most unauthenticated operations, but *will* allow the LSA
	 * sid-to-name that we try as a fallback. */

	if (!(NT_STATUS_IS_OK(domain->last_status)
	      || NT_STATUS_EQUAL(domain->last_status, NT_STATUS_ACCESS_DENIED)))
		return domain->last_status;

	DEBUG(10,("sid_to_name: [Cached] - doing backend query for name for domain %s\n",
		domain->name ));

	status = domain->backend->sid_to_name(domain, mem_ctx, sid, domain_name, name, type);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
		NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (!domain->internal &&
			!domain->online &&
			old_status) {
			NTSTATUS cache_status;
			cache_status = wcache_sid_to_name(domain, sid, mem_ctx,
							domain_name, name, type);
			return cache_status;
		}
	}
	/* and save it */
	refresh_sequence_number(domain, false);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	wcache_save_sid_to_name(domain, status, sid, *domain_name, *name, *type);

	/* We can't save the name to sid mapping here, as with sid history a
	 * later name2sid would give the wrong sid. */

	return status;
}

static NTSTATUS rids_to_names(struct winbindd_domain *domain,
			      TALLOC_CTX *mem_ctx,
			      const struct dom_sid *domain_sid,
			      uint32 *rids,
			      size_t num_rids,
			      char **domain_name,
			      char ***names,
			      enum lsa_SidType **types)
{
	struct winbind_cache *cache = get_cache(domain);
	size_t i;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	bool have_mapped;
	bool have_unmapped;
	bool old_status;

	old_status = domain->online;
	*domain_name = NULL;
	*names = NULL;
	*types = NULL;

	if (!cache->tdb) {
		goto do_query;
	}

	if (num_rids == 0) {
		return NT_STATUS_OK;
	}

	*names = TALLOC_ARRAY(mem_ctx, char *, num_rids);
	*types = TALLOC_ARRAY(mem_ctx, enum lsa_SidType, num_rids);

	if ((*names == NULL) || (*types == NULL)) {
		result = NT_STATUS_NO_MEMORY;
		goto error;
	}

	have_mapped = have_unmapped = false;

	for (i=0; i<num_rids; i++) {
		struct dom_sid sid;
		struct cache_entry *centry;
		fstring tmp;

		if (!sid_compose(&sid, domain_sid, rids[i])) {
			result = NT_STATUS_INTERNAL_ERROR;
			goto error;
		}

		centry = wcache_fetch(cache, domain, "SN/%s",
				      sid_to_fstring(tmp, &sid));
		if (!centry) {
			goto do_query;
		}

		(*types)[i] = SID_NAME_UNKNOWN;
		(*names)[i] = talloc_strdup(*names, "");

		if (NT_STATUS_IS_OK(centry->status)) {
			char *dom;
			have_mapped = true;
			(*types)[i] = (enum lsa_SidType)centry_uint32(centry);

			dom = centry_string(centry, mem_ctx);
			if (*domain_name == NULL) {
				*domain_name = dom;
			} else {
				talloc_free(dom);
			}

			(*names)[i] = centry_string(centry, *names);

		} else if (NT_STATUS_EQUAL(centry->status, NT_STATUS_NONE_MAPPED)
			   || NT_STATUS_EQUAL(centry->status, STATUS_SOME_UNMAPPED)) {
			have_unmapped = true;

		} else {
			/* something's definitely wrong */
			result = centry->status;
			goto error;
		}

		centry_free(centry);
	}

	if (!have_mapped) {
		return NT_STATUS_NONE_MAPPED;
	}
	if (!have_unmapped) {
		return NT_STATUS_OK;
	}
	return STATUS_SOME_UNMAPPED;

 do_query:

	TALLOC_FREE(*names);
	TALLOC_FREE(*types);

	result = domain->backend->rids_to_names(domain, mem_ctx, domain_sid,
						rids, num_rids, domain_name,
						names, types);

	if (NT_STATUS_EQUAL(result, NT_STATUS_IO_TIMEOUT) ||
		NT_STATUS_EQUAL(result, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (cache->tdb &&
			!domain->internal &&
			!domain->online &&
			old_status) {
			have_mapped = have_unmapped = false;

			for (i=0; i<num_rids; i++) {
				struct dom_sid sid;
				struct cache_entry *centry;
				fstring tmp;

				if (!sid_compose(&sid, domain_sid, rids[i])) {
					result = NT_STATUS_INTERNAL_ERROR;
					goto error;
				}

				centry = wcache_fetch(cache, domain, "SN/%s",
						      sid_to_fstring(tmp, &sid));
				if (!centry) {
					(*types)[i] = SID_NAME_UNKNOWN;
					(*names)[i] = talloc_strdup(*names, "");
					continue;
				}

				(*types)[i] = SID_NAME_UNKNOWN;
				(*names)[i] = talloc_strdup(*names, "");

				if (NT_STATUS_IS_OK(centry->status)) {
					char *dom;
					have_mapped = true;
					(*types)[i] = (enum lsa_SidType)centry_uint32(centry);

					dom = centry_string(centry, mem_ctx);
					if (*domain_name == NULL) {
						*domain_name = dom;
					} else {
						talloc_free(dom);
					}

					(*names)[i] = centry_string(centry, *names);

				} else if (NT_STATUS_EQUAL(centry->status, NT_STATUS_NONE_MAPPED)) {
					have_unmapped = true;

				} else {
					/* something's definitely wrong */
					result = centry->status;
					centry_free(centry);
					goto error;
				}

				centry_free(centry);
			}

			if (!have_mapped) {
				return NT_STATUS_NONE_MAPPED;
			}
			if (!have_unmapped) {
				return NT_STATUS_OK;
			}
			return STATUS_SOME_UNMAPPED;
		}
	}
	/*
	  None of the queried rids has been found so save all negative entries
	*/
	if (NT_STATUS_EQUAL(result, NT_STATUS_NONE_MAPPED)) {
		for (i = 0; i < num_rids; i++) {
			struct dom_sid sid;
			const char *name = "";
			const enum lsa_SidType type = SID_NAME_UNKNOWN;
			NTSTATUS status = NT_STATUS_NONE_MAPPED;

			if (!sid_compose(&sid, domain_sid, rids[i])) {
				return NT_STATUS_INTERNAL_ERROR;
			}

			wcache_save_sid_to_name(domain, status, &sid, *domain_name,
						name, type);
		}

		return result;
	}

	/*
	  Some or all of the queried rids have been found.
	*/
	if (!NT_STATUS_IS_OK(result) &&
	    !NT_STATUS_EQUAL(result, STATUS_SOME_UNMAPPED)) {
		return result;
	}

	refresh_sequence_number(domain, false);

	for (i=0; i<num_rids; i++) {
		struct dom_sid sid;
		NTSTATUS status;

		if (!sid_compose(&sid, domain_sid, rids[i])) {
			result = NT_STATUS_INTERNAL_ERROR;
			goto error;
		}

		status = (*types)[i] == SID_NAME_UNKNOWN ?
			NT_STATUS_NONE_MAPPED : NT_STATUS_OK;

		wcache_save_sid_to_name(domain, status, &sid, *domain_name,
					(*names)[i], (*types)[i]);
	}

	return result;

 error:
	TALLOC_FREE(*names);
	TALLOC_FREE(*types);
	return result;
}

NTSTATUS wcache_query_user(struct winbindd_domain *domain,
			   TALLOC_CTX *mem_ctx,
			   const struct dom_sid *user_sid,
			   struct wbint_userinfo *info)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	char *sid_string;

	if (cache->tdb == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	sid_string = sid_string_tos(user_sid);
	if (sid_string == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	centry = wcache_fetch(cache, domain, "U/%s", sid_string);
	TALLOC_FREE(sid_string);
	if (centry == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	/*
	 * If we have an access denied cache entry and a cached info3
	 * in the samlogon cache then do a query.  This will force the
	 * rpc back end to return the info3 data.
	 */

	if (NT_STATUS_EQUAL(domain->last_status, NT_STATUS_ACCESS_DENIED) &&
	    netsamlogon_cache_have(user_sid)) {
		DEBUG(10, ("query_user: cached access denied and have cached "
			   "info3\n"));
		domain->last_status = NT_STATUS_OK;
		centry_free(centry);
		return NT_STATUS_NOT_FOUND;
	}

	/* if status is not ok then this is a negative hit
	   and the rest of the data doesn't matter */
	status = centry->status;
	if (NT_STATUS_IS_OK(status)) {
		info->acct_name = centry_string(centry, mem_ctx);
		info->full_name = centry_string(centry, mem_ctx);
		info->homedir = centry_string(centry, mem_ctx);
		info->shell = centry_string(centry, mem_ctx);
		info->primary_gid = centry_uint32(centry);
		centry_sid(centry, &info->user_sid);
		centry_sid(centry, &info->group_sid);
	}

	DEBUG(10,("query_user: [Cached] - cached info for domain %s status: "
		  "%s\n", domain->name, nt_errstr(status) ));

	centry_free(centry);
	return status;
}

/* Lookup user information from a rid */
static NTSTATUS query_user(struct winbindd_domain *domain,
			   TALLOC_CTX *mem_ctx,
			   const struct dom_sid *user_sid,
			   struct wbint_userinfo *info)
{
	NTSTATUS status;
	bool old_status;

	old_status = domain->online;
	status = wcache_query_user(domain, mem_ctx, user_sid, info);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		return status;
	}

	ZERO_STRUCTP(info);

	/* Return status value returned by seq number check */

	if (!NT_STATUS_IS_OK(domain->last_status))
		return domain->last_status;

	DEBUG(10,("query_user: [Cached] - doing backend query for info for domain %s\n",
		domain->name ));

	status = domain->backend->query_user(domain, mem_ctx, user_sid, info);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
		NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (!domain->internal &&
			!domain->online &&
			old_status) {
			NTSTATUS cache_status;
			cache_status = wcache_query_user(domain, mem_ctx, user_sid, info);
			return cache_status;
		}
	}
	/* and save it */
	refresh_sequence_number(domain, false);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	wcache_save_user(domain, status, info);

	return status;
}

NTSTATUS wcache_lookup_usergroups(struct winbindd_domain *domain,
				  TALLOC_CTX *mem_ctx,
				  const struct dom_sid *user_sid,
				  uint32_t *pnum_sids,
				  struct dom_sid **psids)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	uint32_t i, num_sids;
	struct dom_sid *sids;
	fstring sid_string;

	if (cache->tdb == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	centry = wcache_fetch(cache, domain, "UG/%s",
			      sid_to_fstring(sid_string, user_sid));
	if (centry == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	/* If we have an access denied cache entry and a cached info3 in the
           samlogon cache then do a query.  This will force the rpc back end
           to return the info3 data. */

	if (NT_STATUS_EQUAL(domain->last_status, NT_STATUS_ACCESS_DENIED)
	    && netsamlogon_cache_have(user_sid)) {
		DEBUG(10, ("lookup_usergroups: cached access denied and have "
			   "cached info3\n"));
		domain->last_status = NT_STATUS_OK;
		centry_free(centry);
		return NT_STATUS_NOT_FOUND;
	}

	num_sids = centry_uint32(centry);
	sids = talloc_array(mem_ctx, struct dom_sid, num_sids);
	if (sids == NULL) {
		centry_free(centry);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<num_sids; i++) {
		centry_sid(centry, &sids[i]);
	}

	status = centry->status;

	DEBUG(10,("lookup_usergroups: [Cached] - cached info for domain %s "
		  "status: %s\n", domain->name, nt_errstr(status)));

	centry_free(centry);

	*pnum_sids = num_sids;
	*psids = sids;
	return status;
}

/* Lookup groups a user is a member of. */
static NTSTATUS lookup_usergroups(struct winbindd_domain *domain,
				  TALLOC_CTX *mem_ctx,
				  const struct dom_sid *user_sid,
				  uint32 *num_groups, struct dom_sid **user_gids)
{
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	unsigned int i;
	fstring sid_string;
	bool old_status;

	old_status = domain->online;
	status = wcache_lookup_usergroups(domain, mem_ctx, user_sid,
					  num_groups, user_gids);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		return status;
	}

	(*num_groups) = 0;
	(*user_gids) = NULL;

	/* Return status value returned by seq number check */

	if (!NT_STATUS_IS_OK(domain->last_status))
		return domain->last_status;

	DEBUG(10,("lookup_usergroups: [Cached] - doing backend query for info for domain %s\n",
		domain->name ));

	status = domain->backend->lookup_usergroups(domain, mem_ctx, user_sid, num_groups, user_gids);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
		NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (!domain->internal &&
			!domain->online &&
			old_status) {
			NTSTATUS cache_status;
			cache_status = wcache_lookup_usergroups(domain, mem_ctx, user_sid,
							  num_groups, user_gids);
			return cache_status;
		}
	}
	if ( NT_STATUS_EQUAL(status, NT_STATUS_SYNCHRONIZATION_REQUIRED) )
		goto skip_save;

	/* and save it */
	refresh_sequence_number(domain, false);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	centry = centry_start(domain, status);
	if (!centry)
		goto skip_save;

	centry_put_uint32(centry, *num_groups);
	for (i=0; i<(*num_groups); i++) {
		centry_put_sid(centry, &(*user_gids)[i]);
	}	

	centry_end(centry, "UG/%s", sid_to_fstring(sid_string, user_sid));
	centry_free(centry);

skip_save:
	return status;
}

static char *wcache_make_sidlist(TALLOC_CTX *mem_ctx, uint32_t num_sids,
				 const struct dom_sid *sids)
{
	uint32_t i;
	char *sidlist;

	sidlist = talloc_strdup(mem_ctx, "");
	if (sidlist == NULL) {
		return NULL;
	}
	for (i=0; i<num_sids; i++) {
		fstring tmp;
		sidlist = talloc_asprintf_append_buffer(
			sidlist, "/%s", sid_to_fstring(tmp, &sids[i]));
		if (sidlist == NULL) {
			return NULL;
		}
	}
	return sidlist;
}

NTSTATUS wcache_lookup_useraliases(struct winbindd_domain *domain,
				   TALLOC_CTX *mem_ctx, uint32_t num_sids,
				   const struct dom_sid *sids,
				   uint32_t *pnum_aliases, uint32_t **paliases)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	uint32_t num_aliases;
	uint32_t *aliases;
	NTSTATUS status;
	char *sidlist;
	int i;

	if (cache->tdb == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	if (num_sids == 0) {
		*pnum_aliases = 0;
		*paliases = NULL;
		return NT_STATUS_OK;
	}

	/* We need to cache indexed by the whole list of SIDs, the aliases
	 * resulting might come from any of the SIDs. */

	sidlist = wcache_make_sidlist(talloc_tos(), num_sids, sids);
	if (sidlist == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	centry = wcache_fetch(cache, domain, "UA%s", sidlist);
	TALLOC_FREE(sidlist);
	if (centry == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	num_aliases = centry_uint32(centry);
	aliases = talloc_array(mem_ctx, uint32_t, num_aliases);
	if (aliases == NULL) {
		centry_free(centry);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<num_aliases; i++) {
		aliases[i] = centry_uint32(centry);
	}

	status = centry->status;

	DEBUG(10,("lookup_useraliases: [Cached] - cached info for domain: %s "
		  "status %s\n", domain->name, nt_errstr(status)));

	centry_free(centry);

	*pnum_aliases = num_aliases;
	*paliases = aliases;

	return status;
}

static NTSTATUS lookup_useraliases(struct winbindd_domain *domain,
				   TALLOC_CTX *mem_ctx,
				   uint32 num_sids, const struct dom_sid *sids,
				   uint32 *num_aliases, uint32 **alias_rids)
{
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	char *sidlist;
	int i;
	bool old_status;

	old_status = domain->online;
	status = wcache_lookup_useraliases(domain, mem_ctx, num_sids, sids,
					   num_aliases, alias_rids);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		return status;
	}

	(*num_aliases) = 0;
	(*alias_rids) = NULL;

	if (!NT_STATUS_IS_OK(domain->last_status))
		return domain->last_status;

	DEBUG(10,("lookup_usergroups: [Cached] - doing backend query for info "
		  "for domain %s\n", domain->name ));

	sidlist = wcache_make_sidlist(talloc_tos(), num_sids, sids);
	if (sidlist == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = domain->backend->lookup_useraliases(domain, mem_ctx,
						     num_sids, sids,
						     num_aliases, alias_rids);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
		NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (!domain->internal &&
			!domain->online &&
			old_status) {
			NTSTATUS cache_status;
			cache_status = wcache_lookup_useraliases(domain, mem_ctx, num_sids,
								 sids, num_aliases, alias_rids);
			return cache_status;
		}
	}
	/* and save it */
	refresh_sequence_number(domain, false);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	centry = centry_start(domain, status);
	if (!centry)
		goto skip_save;
	centry_put_uint32(centry, *num_aliases);
	for (i=0; i<(*num_aliases); i++)
		centry_put_uint32(centry, (*alias_rids)[i]);
	centry_end(centry, "UA%s", sidlist);
	centry_free(centry);

 skip_save:
	return status;
}

NTSTATUS wcache_lookup_groupmem(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				const struct dom_sid *group_sid,
				uint32_t *num_names,
				struct dom_sid **sid_mem, char ***names,
				uint32_t **name_types)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	unsigned int i;
	char *sid_string;

	if (cache->tdb == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	sid_string = sid_string_tos(group_sid);
	if (sid_string == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	centry = wcache_fetch(cache, domain, "GM/%s", sid_string);
	TALLOC_FREE(sid_string);
	if (centry == NULL) {
		return NT_STATUS_NOT_FOUND;
	}

	*sid_mem = NULL;
	*names = NULL;
	*name_types = NULL;

	*num_names = centry_uint32(centry);
	if (*num_names == 0) {
		centry_free(centry);
		return NT_STATUS_OK;
	}

	*sid_mem = talloc_array(mem_ctx, struct dom_sid, *num_names);
	*names = talloc_array(mem_ctx, char *, *num_names);
	*name_types = talloc_array(mem_ctx, uint32, *num_names);

	if ((*sid_mem == NULL) || (*names == NULL) || (*name_types == NULL)) {
		TALLOC_FREE(*sid_mem);
		TALLOC_FREE(*names);
		TALLOC_FREE(*name_types);
		centry_free(centry);
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<(*num_names); i++) {
		centry_sid(centry, &(*sid_mem)[i]);
		(*names)[i] = centry_string(centry, mem_ctx);
		(*name_types)[i] = centry_uint32(centry);
	}

	status = centry->status;

	DEBUG(10,("lookup_groupmem: [Cached] - cached info for domain %s "
		  "status: %s\n", domain->name, nt_errstr(status)));

	centry_free(centry);
	return status;
}

static NTSTATUS lookup_groupmem(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				const struct dom_sid *group_sid,
				enum lsa_SidType type,
				uint32 *num_names,
				struct dom_sid **sid_mem, char ***names,
				uint32 **name_types)
{
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	unsigned int i;
	fstring sid_string;
	bool old_status;

	old_status = domain->online;
	status = wcache_lookup_groupmem(domain, mem_ctx, group_sid, num_names,
					sid_mem, names, name_types);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_NOT_FOUND)) {
		return status;
	}

	(*num_names) = 0;
	(*sid_mem) = NULL;
	(*names) = NULL;
	(*name_types) = NULL;

	/* Return status value returned by seq number check */

	if (!NT_STATUS_IS_OK(domain->last_status))
		return domain->last_status;

	DEBUG(10,("lookup_groupmem: [Cached] - doing backend query for info for domain %s\n",
		domain->name ));

	status = domain->backend->lookup_groupmem(domain, mem_ctx, group_sid,
						  type, num_names,
						  sid_mem, names, name_types);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
		NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (!domain->internal &&
			!domain->online &&
			old_status) {
			NTSTATUS cache_status;
			cache_status = wcache_lookup_groupmem(domain, mem_ctx, group_sid,
							      num_names, sid_mem, names,
							      name_types);
			return cache_status;
		}
	}
	/* and save it */
	refresh_sequence_number(domain, false);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	centry = centry_start(domain, status);
	if (!centry)
		goto skip_save;
	centry_put_uint32(centry, *num_names);
	for (i=0; i<(*num_names); i++) {
		centry_put_sid(centry, &(*sid_mem)[i]);
		centry_put_string(centry, (*names)[i]);
		centry_put_uint32(centry, (*name_types)[i]);
	}	
	centry_end(centry, "GM/%s", sid_to_fstring(sid_string, group_sid));
	centry_free(centry);

skip_save:
	return status;
}

/* find the sequence number for a domain */
static NTSTATUS sequence_number(struct winbindd_domain *domain, uint32 *seq)
{
	refresh_sequence_number(domain, false);

	*seq = domain->sequence_number;

	return NT_STATUS_OK;
}

/* enumerate trusted domains 
 * (we need to have the list of trustdoms in the cache when we go offline) -
 * Guenther */
static NTSTATUS trusted_domains(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				struct netr_DomainTrustList *trusts)
{
 	NTSTATUS status;
	struct winbind_cache *cache;
	struct winbindd_tdc_domain *dom_list = NULL;
	size_t num_domains = 0;
	bool retval = false;
	int i;
	bool old_status;

	old_status = domain->online;
	trusts->count = 0;
	trusts->array = NULL;

	cache = get_cache(domain);
	if (!cache || !cache->tdb) {
		goto do_query;
	}

	if (domain->online) {
		goto do_query;
	}

	retval = wcache_tdc_fetch_list(&dom_list, &num_domains);
	if (!retval || !num_domains || !dom_list) {
		TALLOC_FREE(dom_list);
		goto do_query;
	}

do_fetch_cache:
	trusts->array = TALLOC_ZERO_ARRAY(mem_ctx, struct netr_DomainTrust, num_domains);
	if (!trusts->array) {
		TALLOC_FREE(dom_list);
		return NT_STATUS_NO_MEMORY;
	}

	for (i = 0; i < num_domains; i++) {
		struct netr_DomainTrust *trust;
		struct dom_sid *sid;
		struct winbindd_domain *dom;

		dom = find_domain_from_name_noinit(dom_list[i].domain_name);
		if (dom && dom->internal) {
			continue;
		}

		trust = &trusts->array[trusts->count];
		trust->netbios_name = talloc_strdup(trusts->array, dom_list[i].domain_name);
		trust->dns_name = talloc_strdup(trusts->array, dom_list[i].dns_name);
		sid = talloc(trusts->array, struct dom_sid);
		if (!trust->netbios_name || !trust->dns_name ||
			!sid) {
			TALLOC_FREE(dom_list);
			TALLOC_FREE(trusts->array);
			return NT_STATUS_NO_MEMORY;
		}

		trust->trust_flags = dom_list[i].trust_flags;
		trust->trust_attributes = dom_list[i].trust_attribs;
		trust->trust_type = dom_list[i].trust_type;
		sid_copy(sid, &dom_list[i].sid);
		trust->sid = sid;
		trusts->count++;
	}

	TALLOC_FREE(dom_list);
	return NT_STATUS_OK;

do_query:
	/* Return status value returned by seq number check */

 	if (!NT_STATUS_IS_OK(domain->last_status))
 		return domain->last_status;

	DEBUG(10,("trusted_domains: [Cached] - doing backend query for info for domain %s\n",
		domain->name ));

	status = domain->backend->trusted_domains(domain, mem_ctx, trusts);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
		NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (!domain->internal &&
			!domain->online &&
			old_status) {
			retval = wcache_tdc_fetch_list(&dom_list, &num_domains);
			if (retval && num_domains && dom_list) {
				TALLOC_FREE(trusts->array);
				trusts->count = 0;
				goto do_fetch_cache;
			}
		}
	}
	/* no trusts gives NT_STATUS_NO_MORE_ENTRIES resetting to NT_STATUS_OK
	 * so that the generic centry handling still applies correctly -
	 * Guenther*/

	if (!NT_STATUS_IS_ERR(status)) {
		status = NT_STATUS_OK;
	}
 	return status;
}	

/* get lockout policy */
static NTSTATUS lockout_policy(struct winbindd_domain *domain,
 			       TALLOC_CTX *mem_ctx,
			       struct samr_DomInfo12 *policy)
{
 	struct winbind_cache *cache = get_cache(domain);
 	struct cache_entry *centry = NULL;
 	NTSTATUS status;
	bool old_status;

	old_status = domain->online;
	if (!cache->tdb)
		goto do_query;

	centry = wcache_fetch(cache, domain, "LOC_POL/%s", domain->name);

	if (!centry)
 		goto do_query;

do_fetch_cache:
	policy->lockout_duration = centry_nttime(centry);
	policy->lockout_window = centry_nttime(centry);
	policy->lockout_threshold = centry_uint16(centry);

 	status = centry->status;

	DEBUG(10,("lockout_policy: [Cached] - cached info for domain %s status: %s\n",
		domain->name, nt_errstr(status) ));

 	centry_free(centry);
 	return status;

do_query:
	ZERO_STRUCTP(policy);

	/* Return status value returned by seq number check */

 	if (!NT_STATUS_IS_OK(domain->last_status))
 		return domain->last_status;

	DEBUG(10,("lockout_policy: [Cached] - doing backend query for info for domain %s\n",
		domain->name ));

	status = domain->backend->lockout_policy(domain, mem_ctx, policy);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
		NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (cache->tdb &&
			!domain->internal &&
			!domain->online &&
			old_status) {
			centry = wcache_fetch(cache, domain, "LOC_POL/%s", domain->name);
			if (centry) {
				goto do_fetch_cache;
			}
		}
	}
	/* and save it */
 	refresh_sequence_number(domain, false);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	wcache_save_lockout_policy(domain, status, policy);

 	return status;
}

/* get password policy */
static NTSTATUS password_policy(struct winbindd_domain *domain,
				TALLOC_CTX *mem_ctx,
				struct samr_DomInfo1 *policy)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	NTSTATUS status;
	bool old_status;

	old_status = domain->online;
	if (!cache->tdb)
		goto do_query;

	centry = wcache_fetch(cache, domain, "PWD_POL/%s", domain->name);

	if (!centry)
		goto do_query;

do_fetch_cache:
	policy->min_password_length = centry_uint16(centry);
	policy->password_history_length = centry_uint16(centry);
	policy->password_properties = centry_uint32(centry);
	policy->max_password_age = centry_nttime(centry);
	policy->min_password_age = centry_nttime(centry);

	status = centry->status;

	DEBUG(10,("lockout_policy: [Cached] - cached info for domain %s status: %s\n",
		domain->name, nt_errstr(status) ));

	centry_free(centry);
	return status;

do_query:
	ZERO_STRUCTP(policy);

	/* Return status value returned by seq number check */

	if (!NT_STATUS_IS_OK(domain->last_status))
		return domain->last_status;

	DEBUG(10,("password_policy: [Cached] - doing backend query for info for domain %s\n",
		domain->name ));

	status = domain->backend->password_policy(domain, mem_ctx, policy);

	if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT) ||
		NT_STATUS_EQUAL(status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND)) {
		if (!domain->internal && old_status) {
			set_domain_offline(domain);
		}
		if (cache->tdb &&
			!domain->internal &&
			!domain->online &&
			old_status) {
			centry = wcache_fetch(cache, domain, "PWD_POL/%s", domain->name);
			if (centry) {
				goto do_fetch_cache;
			}
		}
	}
	/* and save it */
	refresh_sequence_number(domain, false);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	wcache_save_password_policy(domain, status, policy);

	return status;
}


/* Invalidate cached user and group lists coherently */

static int traverse_fn(TDB_CONTEXT *the_tdb, TDB_DATA kbuf, TDB_DATA dbuf, 
		       void *state)
{
	if (strncmp((const char *)kbuf.dptr, "UL/", 3) == 0 ||
	    strncmp((const char *)kbuf.dptr, "GL/", 3) == 0)
		tdb_delete(the_tdb, kbuf);

	return 0;
}

/* Invalidate the getpwnam and getgroups entries for a winbindd domain */

void wcache_invalidate_samlogon(struct winbindd_domain *domain, 
				const struct dom_sid *sid)
{
        fstring key_str, sid_string;
	struct winbind_cache *cache;

	/* dont clear cached U/SID and UG/SID entries when we want to logon
	 * offline - gd */

	if (lp_winbind_offline_logon()) {
		return;
	}

	if (!domain)
		return;

	cache = get_cache(domain);

        if (!cache->tdb) {
                return;
        }

	/* Clear U/SID cache entry */
	fstr_sprintf(key_str, "U/%s", sid_to_fstring(sid_string, sid));
	DEBUG(10, ("wcache_invalidate_samlogon: clearing %s\n", key_str));
	tdb_delete(cache->tdb, string_tdb_data(key_str));

	/* Clear UG/SID cache entry */
	fstr_sprintf(key_str, "UG/%s", sid_to_fstring(sid_string, sid));
	DEBUG(10, ("wcache_invalidate_samlogon: clearing %s\n", key_str));
	tdb_delete(cache->tdb, string_tdb_data(key_str));

	/* Samba/winbindd never needs this. */
	netsamlogon_clear_cached_user(sid);
}

bool wcache_invalidate_cache(void)
{
	struct winbindd_domain *domain;

	for (domain = domain_list(); domain; domain = domain->next) {
		struct winbind_cache *cache = get_cache(domain);

		DEBUG(10, ("wcache_invalidate_cache: invalidating cache "
			   "entries for %s\n", domain->name));
		if (cache) {
			if (cache->tdb) {
				tdb_traverse(cache->tdb, traverse_fn, NULL);
			} else {
				return false;
			}
		}
	}
	return true;
}

bool wcache_invalidate_cache_noinit(void)
{
	struct winbindd_domain *domain;

	for (domain = domain_list(); domain; domain = domain->next) {
		struct winbind_cache *cache;

		/* Skip uninitialized domains. */
		if (!domain->initialized && !domain->internal) {
			continue;
		}

		cache = get_cache(domain);

		DEBUG(10, ("wcache_invalidate_cache: invalidating cache "
			   "entries for %s\n", domain->name));
		if (cache) {
			if (cache->tdb) {
				tdb_traverse(cache->tdb, traverse_fn, NULL);
				/*
				 * Flushing cache has nothing to with domains.
				 * return here if we successfully flushed once.
				 * To avoid unnecessary traversing the cache.
				 */
				return true;
			} else {
				return false;
			}
		}
	}
	return true;
}

bool init_wcache(void)
{
	if (wcache == NULL) {
		wcache = SMB_XMALLOC_P(struct winbind_cache);
		ZERO_STRUCTP(wcache);
	}

	if (wcache->tdb != NULL)
		return true;

	/* when working offline we must not clear the cache on restart */
	wcache->tdb = tdb_open_log(cache_path("winbindd_cache.tdb"),
				WINBINDD_CACHE_TDB_DEFAULT_HASH_SIZE, 
				TDB_INCOMPATIBLE_HASH |
					(lp_winbind_offline_logon() ? TDB_DEFAULT : (TDB_DEFAULT | TDB_CLEAR_IF_FIRST)),
				O_RDWR|O_CREAT, 0600);

	if (wcache->tdb == NULL) {
		DEBUG(0,("Failed to open winbindd_cache.tdb!\n"));
		return false;
	}

	return true;
}

/************************************************************************
 This is called by the parent to initialize the cache file.
 We don't need sophisticated locking here as we know we're the
 only opener.
************************************************************************/

bool initialize_winbindd_cache(void)
{
	bool cache_bad = true;
	uint32 vers;

	if (!init_wcache()) {
		DEBUG(0,("initialize_winbindd_cache: init_wcache failed.\n"));
		return false;
	}

	/* Check version number. */
	if (tdb_fetch_uint32(wcache->tdb, WINBINDD_CACHE_VERSION_KEYSTR, &vers) &&
			vers == WINBINDD_CACHE_VERSION) {
		cache_bad = false;
	}

	if (cache_bad) {
		DEBUG(0,("initialize_winbindd_cache: clearing cache "
			"and re-creating with version number %d\n",
			WINBINDD_CACHE_VERSION ));

		tdb_close(wcache->tdb);
		wcache->tdb = NULL;

		if (unlink(cache_path("winbindd_cache.tdb")) == -1) {
			DEBUG(0,("initialize_winbindd_cache: unlink %s failed %s ",
				cache_path("winbindd_cache.tdb"),
				strerror(errno) ));
			return false;
		}
		if (!init_wcache()) {
			DEBUG(0,("initialize_winbindd_cache: re-initialization "
					"init_wcache failed.\n"));
			return false;
		}

		/* Write the version. */
		if (!tdb_store_uint32(wcache->tdb, WINBINDD_CACHE_VERSION_KEYSTR, WINBINDD_CACHE_VERSION)) {
			DEBUG(0,("initialize_winbindd_cache: version number store failed %s\n",
				tdb_errorstr(wcache->tdb) ));
			return false;
		}
	}

	tdb_close(wcache->tdb);
	wcache->tdb = NULL;
	return true;
}

void close_winbindd_cache(void)
{
	if (!wcache) {
		return;
	}
	if (wcache->tdb) {
		tdb_close(wcache->tdb);
		wcache->tdb = NULL;
	}
}

bool lookup_cached_sid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
		       char **domain_name, char **name,
		       enum lsa_SidType *type)
{
	struct winbindd_domain *domain;
	NTSTATUS status;

	domain = find_lookup_domain_from_sid(sid);
	if (domain == NULL) {
		return false;
	}
	status = wcache_sid_to_name(domain, sid, mem_ctx, domain_name, name,
				    type);
	return NT_STATUS_IS_OK(status);
}

bool lookup_cached_name(const char *domain_name,
			const char *name,
			struct dom_sid *sid,
			enum lsa_SidType *type)
{
	struct winbindd_domain *domain;
	NTSTATUS status;
	bool original_online_state;

	domain = find_lookup_domain_from_name(domain_name);
	if (domain == NULL) {
		return false;
	}

	/* If we are doing a cached logon, temporarily set the domain
	   offline so the cache won't expire the entry */

	original_online_state = domain->online;
	domain->online = false;
	status = wcache_name_to_sid(domain, domain_name, name, sid, type);
	domain->online = original_online_state;

	return NT_STATUS_IS_OK(status);
}

void cache_name2sid(struct winbindd_domain *domain, 
		    const char *domain_name, const char *name,
		    enum lsa_SidType type, const struct dom_sid *sid)
{
	refresh_sequence_number(domain, false);
	wcache_save_name_to_sid(domain, NT_STATUS_OK, domain_name, name,
				sid, type);
}

/*
 * The original idea that this cache only contains centries has
 * been blurred - now other stuff gets put in here. Ensure we
 * ignore these things on cleanup.
 */

static int traverse_fn_cleanup(TDB_CONTEXT *the_tdb, TDB_DATA kbuf, 
			       TDB_DATA dbuf, void *state)
{
	struct cache_entry *centry;

	if (is_non_centry_key(kbuf)) {
		return 0;
	}

	centry = wcache_fetch_raw((char *)kbuf.dptr);
	if (!centry) {
		return 0;
	}

	if (!NT_STATUS_IS_OK(centry->status)) {
		DEBUG(10,("deleting centry %s\n", (const char *)kbuf.dptr));
		tdb_delete(the_tdb, kbuf);
	}

	centry_free(centry);
	return 0;
}

/* flush the cache */
void wcache_flush_cache(void)
{
	if (!wcache)
		return;
	if (wcache->tdb) {
		tdb_close(wcache->tdb);
		wcache->tdb = NULL;
	}
	if (!winbindd_use_cache()) {
		return;
	}

	/* when working offline we must not clear the cache on restart */
	wcache->tdb = tdb_open_log(cache_path("winbindd_cache.tdb"),
				WINBINDD_CACHE_TDB_DEFAULT_HASH_SIZE, 
				TDB_INCOMPATIBLE_HASH |
				(lp_winbind_offline_logon() ? TDB_DEFAULT : (TDB_DEFAULT | TDB_CLEAR_IF_FIRST)),
				O_RDWR|O_CREAT, 0600);

	if (!wcache->tdb) {
		DEBUG(0,("Failed to open winbindd_cache.tdb!\n"));
		return;
	}

	tdb_traverse(wcache->tdb, traverse_fn_cleanup, NULL);

	DEBUG(10,("wcache_flush_cache success\n"));
}

/* Count cached creds */

static int traverse_fn_cached_creds(TDB_CONTEXT *the_tdb, TDB_DATA kbuf, TDB_DATA dbuf, 
			 	    void *state)
{
	int *cred_count = (int*)state;
 
	if (strncmp((const char *)kbuf.dptr, "CRED/", 5) == 0) {
		(*cred_count)++;
	}
	return 0;
}

NTSTATUS wcache_count_cached_creds(struct winbindd_domain *domain, int *count)
{
	struct winbind_cache *cache = get_cache(domain);

	*count = 0;

	if (!cache->tdb) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}
 
	tdb_traverse(cache->tdb, traverse_fn_cached_creds, (void *)count);

	return NT_STATUS_OK;
}

struct cred_list {
	struct cred_list *prev, *next;
	TDB_DATA key;
	fstring name;
	time_t created;
};
static struct cred_list *wcache_cred_list;

static int traverse_fn_get_credlist(TDB_CONTEXT *the_tdb, TDB_DATA kbuf, TDB_DATA dbuf, 
				    void *state)
{
	struct cred_list *cred;

	if (strncmp((const char *)kbuf.dptr, "CRED/", 5) == 0) {

		cred = SMB_MALLOC_P(struct cred_list);
		if (cred == NULL) {
			DEBUG(0,("traverse_fn_remove_first_creds: failed to malloc new entry for list\n"));
			return -1;
		}

		ZERO_STRUCTP(cred);

		/* save a copy of the key */

		fstrcpy(cred->name, (const char *)kbuf.dptr);		
		DLIST_ADD(wcache_cred_list, cred);
	}

	return 0;
}

NTSTATUS wcache_remove_oldest_cached_creds(struct winbindd_domain *domain, const struct dom_sid *sid)
{
	struct winbind_cache *cache = get_cache(domain);
	NTSTATUS status;
	int ret;
	struct cred_list *cred, *oldest = NULL;

	if (!cache->tdb) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	/* we possibly already have an entry */
 	if (sid && NT_STATUS_IS_OK(wcache_cached_creds_exist(domain, sid))) {

		fstring key_str, tmp;

		DEBUG(11,("we already have an entry, deleting that\n"));

		fstr_sprintf(key_str, "CRED/%s", sid_to_fstring(tmp, sid));

		tdb_delete(cache->tdb, string_tdb_data(key_str));

		return NT_STATUS_OK;
	}

	ret = tdb_traverse(cache->tdb, traverse_fn_get_credlist, NULL);
	if (ret == 0) {
		return NT_STATUS_OK;
	} else if ((ret == -1) || (wcache_cred_list == NULL)) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	ZERO_STRUCTP(oldest);

	for (cred = wcache_cred_list; cred; cred = cred->next) {

		TDB_DATA data;
		time_t t;

		data = tdb_fetch(cache->tdb, string_tdb_data(cred->name));
		if (!data.dptr) {
			DEBUG(10,("wcache_remove_oldest_cached_creds: entry for [%s] not found\n", 
				cred->name));
			status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
			goto done;
		}

		t = IVAL(data.dptr, 0);
		SAFE_FREE(data.dptr);

		if (!oldest) {
			oldest = SMB_MALLOC_P(struct cred_list);
			if (oldest == NULL) {
				status = NT_STATUS_NO_MEMORY;
				goto done;
			}

			fstrcpy(oldest->name, cred->name);
			oldest->created = t;
			continue;
		}

		if (t < oldest->created) {
			fstrcpy(oldest->name, cred->name);
			oldest->created = t;
		}
	}

	if (tdb_delete(cache->tdb, string_tdb_data(oldest->name)) == 0) {
		status = NT_STATUS_OK;
	} else {
		status = NT_STATUS_UNSUCCESSFUL;
	}
done:
	SAFE_FREE(wcache_cred_list);
	SAFE_FREE(oldest);

	return status;
}

/* Change the global online/offline state. */
bool set_global_winbindd_state_offline(void)
{
	TDB_DATA data;

	DEBUG(10,("set_global_winbindd_state_offline: offline requested.\n"));

	/* Only go offline if someone has created
	   the key "WINBINDD_OFFLINE" in the cache tdb. */

	if (wcache == NULL || wcache->tdb == NULL) {
		DEBUG(10,("set_global_winbindd_state_offline: wcache not open yet.\n"));
		return false;
	}

	if (!lp_winbind_offline_logon()) {
		DEBUG(10,("set_global_winbindd_state_offline: rejecting.\n"));
		return false;
	}

	if (global_winbindd_offline_state) {
		/* Already offline. */
		return true;
	}

	data = tdb_fetch_bystring( wcache->tdb, "WINBINDD_OFFLINE" );

	if (!data.dptr || data.dsize != 4) {
		DEBUG(10,("set_global_winbindd_state_offline: offline state not set.\n"));
		SAFE_FREE(data.dptr);
		return false;
	} else {
		DEBUG(10,("set_global_winbindd_state_offline: offline state set.\n"));
		global_winbindd_offline_state = true;
		SAFE_FREE(data.dptr);
		return true;
	}
}

void set_global_winbindd_state_online(void)
{
	DEBUG(10,("set_global_winbindd_state_online: online requested.\n"));

	if (!lp_winbind_offline_logon()) {
		DEBUG(10,("set_global_winbindd_state_online: rejecting.\n"));
		return;
	}

	if (!global_winbindd_offline_state) {
		/* Already online. */
		return;
	}
	global_winbindd_offline_state = false;

	if (!wcache->tdb) {
		return;
	}

	/* Ensure there is no key "WINBINDD_OFFLINE" in the cache tdb. */
	tdb_delete_bystring(wcache->tdb, "WINBINDD_OFFLINE");
}

bool get_global_winbindd_state_offline(void)
{
	return global_winbindd_offline_state;
}

/***********************************************************************
 Validate functions for all possible cache tdb keys.
***********************************************************************/

static struct cache_entry *create_centry_validate(const char *kstr, TDB_DATA data, 
						  struct tdb_validation_status *state)
{
	struct cache_entry *centry;

	centry = SMB_XMALLOC_P(struct cache_entry);
	centry->data = (unsigned char *)memdup(data.dptr, data.dsize);
	if (!centry->data) {
		SAFE_FREE(centry);
		return NULL;
	}
	centry->len = data.dsize;
	centry->ofs = 0;

	if (centry->len < 16) {
		/* huh? corrupt cache? */
		DEBUG(0,("create_centry_validate: Corrupt cache for key %s "
			 "(len < 16) ?\n", kstr));
		centry_free(centry);
		state->bad_entry = true;
		state->success = false;
		return NULL;
	}

	centry->status = NT_STATUS(centry_uint32(centry));
	centry->sequence_number = centry_uint32(centry);
	centry->timeout = centry_uint64_t(centry);
	return centry;
}

static int validate_seqnum(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
			   struct tdb_validation_status *state)
{
	if (dbuf.dsize != 8) {
		DEBUG(0,("validate_seqnum: Corrupt cache for key %s (len %u != 8) ?\n",
				keystr, (unsigned int)dbuf.dsize ));
		state->bad_entry = true;
		return 1;
	}
	return 0;
}

static int validate_ns(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
		       struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);
	if (!centry) {
		return 1;
	}

	(void)centry_uint32(centry);
	if (NT_STATUS_IS_OK(centry->status)) {
		struct dom_sid sid;
		(void)centry_sid(centry, &sid);
	}

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_ns: %s ok\n", keystr));
	return 0;
}

static int validate_sn(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
		       struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);
	if (!centry) {
		return 1;
	}

	if (NT_STATUS_IS_OK(centry->status)) {
		(void)centry_uint32(centry);
		(void)centry_string(centry, mem_ctx);
		(void)centry_string(centry, mem_ctx);
	}

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_sn: %s ok\n", keystr));
	return 0;
}

static int validate_u(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
		      struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);
	struct dom_sid sid;

	if (!centry) {
		return 1;
	}

	(void)centry_string(centry, mem_ctx);
	(void)centry_string(centry, mem_ctx);
	(void)centry_string(centry, mem_ctx);
	(void)centry_string(centry, mem_ctx);
	(void)centry_uint32(centry);
	(void)centry_sid(centry, &sid);
	(void)centry_sid(centry, &sid);

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_u: %s ok\n", keystr));
	return 0;
}

static int validate_loc_pol(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
			    struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);

	if (!centry) {
		return 1;
	}

	(void)centry_nttime(centry);
	(void)centry_nttime(centry);
	(void)centry_uint16(centry);

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_loc_pol: %s ok\n", keystr));
	return 0;
}

static int validate_pwd_pol(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
			    struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);

	if (!centry) {
		return 1;
	}

	(void)centry_uint16(centry);
	(void)centry_uint16(centry);
	(void)centry_uint32(centry);
	(void)centry_nttime(centry);
	(void)centry_nttime(centry);

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_pwd_pol: %s ok\n", keystr));
	return 0;
}

static int validate_cred(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
			 struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);

	if (!centry) {
		return 1;
	}

	(void)centry_time(centry);
	(void)centry_hash16(centry, mem_ctx);

	/* We only have 17 bytes more data in the salted cred case. */
	if (centry->len - centry->ofs == 17) {
		(void)centry_hash16(centry, mem_ctx);
	}

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_cred: %s ok\n", keystr));
	return 0;
}

static int validate_ul(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
		       struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);
	int32 num_entries, i;

	if (!centry) {
		return 1;
	}

	num_entries = (int32)centry_uint32(centry);

	for (i=0; i< num_entries; i++) {
		struct dom_sid sid;
		(void)centry_string(centry, mem_ctx);
		(void)centry_string(centry, mem_ctx);
		(void)centry_string(centry, mem_ctx);
		(void)centry_string(centry, mem_ctx);
		(void)centry_sid(centry, &sid);
		(void)centry_sid(centry, &sid);
	}

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_ul: %s ok\n", keystr));
	return 0;
}

static int validate_gl(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
		       struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);
	int32 num_entries, i;

	if (!centry) {
		return 1;
	}

	num_entries = centry_uint32(centry);

	for (i=0; i< num_entries; i++) {
		(void)centry_string(centry, mem_ctx);
		(void)centry_string(centry, mem_ctx);
		(void)centry_uint32(centry);
	}

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_gl: %s ok\n", keystr));
	return 0;
}

static int validate_ug(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
		       struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);
	int32 num_groups, i;

	if (!centry) {
		return 1;
	}

	num_groups = centry_uint32(centry);

	for (i=0; i< num_groups; i++) {
		struct dom_sid sid;
		centry_sid(centry, &sid);
	}

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_ug: %s ok\n", keystr));
	return 0;
}

static int validate_ua(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
		       struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);
	int32 num_aliases, i;

	if (!centry) {
		return 1;
	}

	num_aliases = centry_uint32(centry);

	for (i=0; i < num_aliases; i++) {
		(void)centry_uint32(centry);
	}

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_ua: %s ok\n", keystr));
	return 0;
}

static int validate_gm(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
		       struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);
	int32 num_names, i;

	if (!centry) {
		return 1;
	}

	num_names = centry_uint32(centry);

	for (i=0; i< num_names; i++) {
		struct dom_sid sid;
		centry_sid(centry, &sid);
		(void)centry_string(centry, mem_ctx);
		(void)centry_uint32(centry);
	}

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_gm: %s ok\n", keystr));
	return 0;
}

static int validate_dr(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
		       struct tdb_validation_status *state)
{
	/* Can't say anything about this other than must be nonzero. */
	if (dbuf.dsize == 0) {
		DEBUG(0,("validate_dr: Corrupt cache for key %s (len == 0) ?\n",
				keystr));
		state->bad_entry = true;
		state->success = false;
		return 1;
	}

	DEBUG(10,("validate_dr: %s ok\n", keystr));
	return 0;
}

static int validate_de(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
		       struct tdb_validation_status *state)
{
	/* Can't say anything about this other than must be nonzero. */
	if (dbuf.dsize == 0) {
		DEBUG(0,("validate_de: Corrupt cache for key %s (len == 0) ?\n",
				keystr));
		state->bad_entry = true;
		state->success = false;
		return 1;
	}

	DEBUG(10,("validate_de: %s ok\n", keystr));
	return 0;
}

static int validate_pwinfo(TALLOC_CTX *mem_ctx, const char *keystr,
			   TDB_DATA dbuf, struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);

	if (!centry) {
		return 1;
	}

	(void)centry_string(centry, mem_ctx);
	(void)centry_string(centry, mem_ctx);
	(void)centry_string(centry, mem_ctx);
	(void)centry_uint32(centry);

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_pwinfo: %s ok\n", keystr));
	return 0;
}

static int validate_nss_an(TALLOC_CTX *mem_ctx, const char *keystr,
			   TDB_DATA dbuf,
			   struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);

	if (!centry) {
		return 1;
	}

	(void)centry_string( centry, mem_ctx );

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_pwinfo: %s ok\n", keystr));
	return 0;
}

static int validate_nss_na(TALLOC_CTX *mem_ctx, const char *keystr,
			   TDB_DATA dbuf,
			   struct tdb_validation_status *state)
{
	struct cache_entry *centry = create_centry_validate(keystr, dbuf, state);

	if (!centry) {
		return 1;
	}

	(void)centry_string( centry, mem_ctx );

	centry_free(centry);

	if (!(state->success)) {
		return 1;
	}
	DEBUG(10,("validate_pwinfo: %s ok\n", keystr));
	return 0;
}

static int validate_trustdomcache(TALLOC_CTX *mem_ctx, const char *keystr, 
				  TDB_DATA dbuf,
				  struct tdb_validation_status *state)
{
	if (dbuf.dsize == 0) {
		DEBUG(0, ("validate_trustdomcache: Corrupt cache for "
			  "key %s (len ==0) ?\n", keystr));
		state->bad_entry = true;
		state->success = false;
		return 1;
	}

	DEBUG(10,    ("validate_trustdomcache: %s ok\n", keystr));
	DEBUGADD(10, ("  Don't trust me, I am a DUMMY!\n"));
	return 0;
}

static int validate_offline(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
			    struct tdb_validation_status *state)
{
	if (dbuf.dsize != 4) {
		DEBUG(0,("validate_offline: Corrupt cache for key %s (len %u != 4) ?\n",
				keystr, (unsigned int)dbuf.dsize ));
		state->bad_entry = true;
		state->success = false;
		return 1;
	}
	DEBUG(10,("validate_offline: %s ok\n", keystr));
	return 0;
}

static int validate_ndr(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
			struct tdb_validation_status *state)
{
	/*
	 * Ignore validation for now. The proper way to do this is with a
	 * checksum. Just pure parsing does not really catch much.
	 */
	return 0;
}

static int validate_cache_version(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf,
				  struct tdb_validation_status *state)
{
	if (dbuf.dsize != 4) {
		DEBUG(0, ("validate_cache_version: Corrupt cache for "
			  "key %s (len %u != 4) ?\n", 
			  keystr, (unsigned int)dbuf.dsize));
		state->bad_entry = true;
		state->success = false;
		return 1;
	}

	DEBUG(10, ("validate_cache_version: %s ok\n", keystr));
	return 0;
}

/***********************************************************************
 A list of all possible cache tdb keys with associated validation
 functions.
***********************************************************************/

struct key_val_struct {
	const char *keyname;
	int (*validate_data_fn)(TALLOC_CTX *mem_ctx, const char *keystr, TDB_DATA dbuf, struct tdb_validation_status* state);
} key_val[] = {
	{"SEQNUM/", validate_seqnum},
	{"NS/", validate_ns},
	{"SN/", validate_sn},
	{"U/", validate_u},
	{"LOC_POL/", validate_loc_pol},
	{"PWD_POL/", validate_pwd_pol},
	{"CRED/", validate_cred},
	{"UL/", validate_ul},
	{"GL/", validate_gl},
	{"UG/", validate_ug},
	{"UA", validate_ua},
	{"GM/", validate_gm},
	{"DR/", validate_dr},
	{"DE/", validate_de},
	{"NSS/PWINFO/", validate_pwinfo},
	{"TRUSTDOMCACHE/", validate_trustdomcache},
	{"NSS/NA/", validate_nss_na},
	{"NSS/AN/", validate_nss_an},
	{"WINBINDD_OFFLINE", validate_offline},
	{"NDR/", validate_ndr},
	{WINBINDD_CACHE_VERSION_KEYSTR, validate_cache_version},
	{NULL, NULL}
};

/***********************************************************************
 Function to look at every entry in the tdb and validate it as far as
 possible.
***********************************************************************/

static int cache_traverse_validate_fn(TDB_CONTEXT *the_tdb, TDB_DATA kbuf, TDB_DATA dbuf, void *state)
{
	int i;
	unsigned int max_key_len = 1024;
	struct tdb_validation_status *v_state = (struct tdb_validation_status *)state;

	/* Paranoia check. */
	if (strncmp("UA/", (const char *)kbuf.dptr, 3) == 0) {
		max_key_len = 1024 * 1024;
	}
	if (kbuf.dsize > max_key_len) {
		DEBUG(0, ("cache_traverse_validate_fn: key length too large: "
			  "(%u) > (%u)\n\n",
			  (unsigned int)kbuf.dsize, (unsigned int)max_key_len));
		return 1;
	}

	for (i = 0; key_val[i].keyname; i++) {
		size_t namelen = strlen(key_val[i].keyname);
		if (kbuf.dsize >= namelen && (
				strncmp(key_val[i].keyname, (const char *)kbuf.dptr, namelen)) == 0) {
			TALLOC_CTX *mem_ctx;
			char *keystr;
			int ret;

			keystr = SMB_MALLOC_ARRAY(char, kbuf.dsize+1);
			if (!keystr) {
				return 1;
			}
			memcpy(keystr, kbuf.dptr, kbuf.dsize);
			keystr[kbuf.dsize] = '\0';

			mem_ctx = talloc_init("validate_ctx");
			if (!mem_ctx) {
				SAFE_FREE(keystr);
				return 1;
			}

			ret = key_val[i].validate_data_fn(mem_ctx, keystr, dbuf, 
							  v_state);

			SAFE_FREE(keystr);
			talloc_destroy(mem_ctx);
			return ret;
		}
	}

	DEBUG(0,("cache_traverse_validate_fn: unknown cache entry\nkey :\n"));
	dump_data(0, (uint8 *)kbuf.dptr, kbuf.dsize);
	DEBUG(0,("data :\n"));
	dump_data(0, (uint8 *)dbuf.dptr, dbuf.dsize);
	v_state->unknown_key = true;
	v_state->success = false;
	return 1; /* terminate. */
}

static void validate_panic(const char *const why)
{
        DEBUG(0,("validating cache: would panic %s\n", why ));
	DEBUGADD(0, ("exiting instead (cache validation mode)\n"));
	exit(47);
}

static int wbcache_update_centry_fn(TDB_CONTEXT *tdb,
				    TDB_DATA key,
				    TDB_DATA data,
				    void *state)
{
	uint64_t ctimeout;
	TDB_DATA blob;

	if (is_non_centry_key(key)) {
		return 0;
	}

	if (data.dptr == NULL || data.dsize == 0) {
		if (tdb_delete(tdb, key) < 0) {
			DEBUG(0, ("tdb_delete for [%s] failed!\n",
				  key.dptr));
			return 1;
		}
	}

	/* add timeout to blob (uint64_t) */
	blob.dsize = data.dsize + 8;

	blob.dptr = SMB_XMALLOC_ARRAY(uint8_t, blob.dsize);
	if (blob.dptr == NULL) {
		return 1;
	}
	memset(blob.dptr, 0, blob.dsize);

	/* copy status and seqnum */
	memcpy(blob.dptr, data.dptr, 8);

	/* add timeout */
	ctimeout = lp_winbind_cache_time() + time(NULL);
	SBVAL(blob.dptr, 8, ctimeout);

	/* copy the rest */
	memcpy(blob.dptr + 16, data.dptr + 8, data.dsize - 8);

	if (tdb_store(tdb, key, blob, TDB_REPLACE) < 0) {
		DEBUG(0, ("tdb_store to update [%s] failed!\n",
			  key.dptr));
		SAFE_FREE(blob.dptr);
		return 1;
	}

	SAFE_FREE(blob.dptr);
	return 0;
}

static bool wbcache_upgrade_v1_to_v2(TDB_CONTEXT *tdb)
{
	int rc;

	DEBUG(1, ("Upgrade to version 2 of the winbindd_cache.tdb\n"));

	rc = tdb_traverse(tdb, wbcache_update_centry_fn, NULL);
	if (rc < 0) {
		return false;
	}

	return true;
}

/***********************************************************************
 Try and validate every entry in the winbindd cache. If we fail here,
 delete the cache tdb and return non-zero.
***********************************************************************/

int winbindd_validate_cache(void)
{
	int ret = -1;
	const char *tdb_path = cache_path("winbindd_cache.tdb");
	TDB_CONTEXT *tdb = NULL;
	uint32_t vers_id;
	bool ok;

	DEBUG(10, ("winbindd_validate_cache: replacing panic function\n"));
	smb_panic_fn = validate_panic;

	tdb = tdb_open_log(tdb_path, 
			   WINBINDD_CACHE_TDB_DEFAULT_HASH_SIZE,
			   TDB_INCOMPATIBLE_HASH |
			   ( lp_winbind_offline_logon() 
			     ? TDB_DEFAULT 
			     : TDB_DEFAULT | TDB_CLEAR_IF_FIRST ),
			   O_RDWR|O_CREAT, 
			   0600);
	if (!tdb) {
		DEBUG(0, ("winbindd_validate_cache: "
			  "error opening/initializing tdb\n"));
		goto done;
	}

	/* Version check and upgrade code. */
	if (!tdb_fetch_uint32(tdb, WINBINDD_CACHE_VERSION_KEYSTR, &vers_id)) {
		DEBUG(10, ("Fresh database\n"));
		tdb_store_uint32(tdb, WINBINDD_CACHE_VERSION_KEYSTR, WINBINDD_CACHE_VERSION);
		vers_id = WINBINDD_CACHE_VERSION;
	}

	if (vers_id != WINBINDD_CACHE_VERSION) {
		if (vers_id == WINBINDD_CACHE_VER1) {
			ok = wbcache_upgrade_v1_to_v2(tdb);
			if (!ok) {
				DEBUG(10, ("winbindd_validate_cache: upgrade to version 2 failed.\n"));
				unlink(tdb_path);
				goto done;
			}

			tdb_store_uint32(tdb,
					 WINBINDD_CACHE_VERSION_KEYSTR,
					 WINBINDD_CACHE_VERSION);
			vers_id = WINBINDD_CACHE_VER2;
		}
	}

	tdb_close(tdb);

	ret = tdb_validate_and_backup(tdb_path, cache_traverse_validate_fn);

	if (ret != 0) {
		DEBUG(10, ("winbindd_validate_cache: validation not successful.\n"));
		DEBUGADD(10, ("removing tdb %s.\n", tdb_path));
		unlink(tdb_path);
	}

done:
	DEBUG(10, ("winbindd_validate_cache: restoring panic function\n"));
	smb_panic_fn = smb_panic;
	return ret;
}

/***********************************************************************
 Try and validate every entry in the winbindd cache.
***********************************************************************/

int winbindd_validate_cache_nobackup(void)
{
	int ret = -1;
	const char *tdb_path = cache_path("winbindd_cache.tdb");

	DEBUG(10, ("winbindd_validate_cache: replacing panic function\n"));
	smb_panic_fn = validate_panic;


	if (wcache == NULL || wcache->tdb == NULL) {
		ret = tdb_validate_open(tdb_path, cache_traverse_validate_fn);
	} else {
		ret = tdb_validate(wcache->tdb, cache_traverse_validate_fn);
	}

	if (ret != 0) {
		DEBUG(10, ("winbindd_validate_cache_nobackup: validation not "
			   "successful.\n"));
	}

	DEBUG(10, ("winbindd_validate_cache_nobackup: restoring panic "
		   "function\n"));
	smb_panic_fn = smb_panic;
	return ret;
}

bool winbindd_cache_validate_and_initialize(void)
{
	close_winbindd_cache();

	if (lp_winbind_offline_logon()) {
		if (winbindd_validate_cache() < 0) {
			DEBUG(0, ("winbindd cache tdb corrupt and no backup "
				  "could be restored.\n"));
		}
	}

	return initialize_winbindd_cache();
}

/*********************************************************************
 ********************************************************************/

static bool add_wbdomain_to_tdc_array( struct winbindd_domain *new_dom,
				       struct winbindd_tdc_domain **domains, 
				       size_t *num_domains )
{
	struct winbindd_tdc_domain *list = NULL;
	size_t idx;
	int i;
	bool set_only = false;

	/* don't allow duplicates */

	idx = *num_domains;
	list = *domains;

	for ( i=0; i< (*num_domains); i++ ) {
		if ( strequal( new_dom->name, list[i].domain_name ) ) {
			DEBUG(10,("add_wbdomain_to_tdc_array: Found existing record for %s\n",
				  new_dom->name));
			idx = i;
			set_only = true;

			break;
		}
	}

	if ( !set_only ) {
		if ( !*domains ) {
			list = TALLOC_ARRAY( NULL, struct winbindd_tdc_domain, 1 );
			idx = 0;
		} else {
			list = TALLOC_REALLOC_ARRAY( *domains, *domains, 
						     struct winbindd_tdc_domain,  
						     (*num_domains)+1);
			idx = *num_domains;		
		}

		ZERO_STRUCT( list[idx] );
	}

	if ( !list )
		return false;

	list[idx].domain_name = talloc_strdup( list, new_dom->name );
	list[idx].dns_name = talloc_strdup( list, new_dom->alt_name );

	if ( !is_null_sid( &new_dom->sid ) ) {
		sid_copy( &list[idx].sid, &new_dom->sid );
	} else {
		sid_copy(&list[idx].sid, &global_sid_NULL);
	}

	if ( new_dom->domain_flags != 0x0 )
		list[idx].trust_flags = new_dom->domain_flags;	

	if ( new_dom->domain_type != 0x0 )
		list[idx].trust_type = new_dom->domain_type;

	if ( new_dom->domain_trust_attribs != 0x0 )
		list[idx].trust_attribs = new_dom->domain_trust_attribs;

	if ( !set_only ) {
		*domains = list;
		*num_domains = idx + 1;	
	}

	return true;
}

/*********************************************************************
 ********************************************************************/

static TDB_DATA make_tdc_key( const char *domain_name )
{
	char *keystr = NULL;
	TDB_DATA key = { NULL, 0 };

	if ( !domain_name ) {
		DEBUG(5,("make_tdc_key: Keyname workgroup is NULL!\n"));
		return key;
	}

	if (asprintf( &keystr, "TRUSTDOMCACHE/%s", domain_name ) == -1) {
		return key;
	}
	key = string_term_tdb_data(keystr);

	return key;	
}

/*********************************************************************
 ********************************************************************/

static int pack_tdc_domains( struct winbindd_tdc_domain *domains, 
			     size_t num_domains,
			     unsigned char **buf )
{
        unsigned char *buffer = NULL;
	int len = 0;
	int buflen = 0;
	int i = 0;

	DEBUG(10,("pack_tdc_domains: Packing %d trusted domains\n",
		  (int)num_domains));

	buflen = 0;

 again: 
	len = 0;

	/* Store the number of array items first */
	len += tdb_pack( buffer+len, buflen-len, "d", 
			 num_domains );

	/* now pack each domain trust record */
	for ( i=0; i<num_domains; i++ ) {

		fstring tmp;

		if ( buflen > 0 ) {
			DEBUG(10,("pack_tdc_domains: Packing domain %s (%s)\n",
				  domains[i].domain_name,
				  domains[i].dns_name ? domains[i].dns_name : "UNKNOWN" ));
		}

		len += tdb_pack( buffer+len, buflen-len, "fffddd",
				 domains[i].domain_name,
				 domains[i].dns_name,
				 sid_to_fstring(tmp, &domains[i].sid),
				 domains[i].trust_flags,
				 domains[i].trust_attribs,
				 domains[i].trust_type );
	}

	if ( buflen < len ) {
		SAFE_FREE(buffer);
		if ( (buffer = SMB_MALLOC_ARRAY(unsigned char, len)) == NULL ) {
			DEBUG(0,("pack_tdc_domains: failed to alloc buffer!\n"));
			buflen = -1;
			goto done;
		}
		buflen = len;
		goto again;
	}

	*buf = buffer;	

 done:	
	return buflen;	
}

/*********************************************************************
 ********************************************************************/

static size_t unpack_tdc_domains( unsigned char *buf, int buflen, 
				  struct winbindd_tdc_domain **domains )
{
	fstring domain_name, dns_name, sid_string;	
	uint32 type, attribs, flags;
	int num_domains;
	int len = 0;
	int i;
	struct winbindd_tdc_domain *list = NULL;

	/* get the number of domains */
	len += tdb_unpack( buf+len, buflen-len, "d", &num_domains);
	if ( len == -1 ) {
		DEBUG(5,("unpack_tdc_domains: Failed to unpack domain array\n"));		
		return 0;
	}

	list = TALLOC_ARRAY( NULL, struct winbindd_tdc_domain, num_domains );
	if ( !list ) {
		DEBUG(0,("unpack_tdc_domains: Failed to talloc() domain list!\n"));
		return 0;		
	}

	for ( i=0; i<num_domains; i++ ) {
		len += tdb_unpack( buf+len, buflen-len, "fffddd",
				   domain_name,
				   dns_name,
				   sid_string,
				   &flags,
				   &attribs,
				   &type );

		if ( len == -1 ) {
			DEBUG(5,("unpack_tdc_domains: Failed to unpack domain array\n"));
			TALLOC_FREE( list );			
			return 0;
		}

		DEBUG(11,("unpack_tdc_domains: Unpacking domain %s (%s) "
			  "SID %s, flags = 0x%x, attribs = 0x%x, type = 0x%x\n",
			  domain_name, dns_name, sid_string,
			  flags, attribs, type));

		list[i].domain_name = talloc_strdup( list, domain_name );
		list[i].dns_name = talloc_strdup( list, dns_name );
		if ( !string_to_sid( &(list[i].sid), sid_string ) ) {			
			DEBUG(10,("unpack_tdc_domains: no SID for domain %s\n",
				  domain_name));
		}
		list[i].trust_flags = flags;
		list[i].trust_attribs = attribs;
		list[i].trust_type = type;
	}

	*domains = list;

	return num_domains;
}

/*********************************************************************
 ********************************************************************/

static bool wcache_tdc_store_list( struct winbindd_tdc_domain *domains, size_t num_domains )
{
	TDB_DATA key = make_tdc_key( lp_workgroup() );	 
	TDB_DATA data = { NULL, 0 };
	int ret;

	if ( !key.dptr )
		return false;

	/* See if we were asked to delete the cache entry */

	if ( !domains ) {
		ret = tdb_delete( wcache->tdb, key );
		goto done;
	}

	data.dsize = pack_tdc_domains( domains, num_domains, &data.dptr );

	if ( !data.dptr ) {
		ret = -1;
		goto done;
	}

	ret = tdb_store( wcache->tdb, key, data, 0 );

 done:
	SAFE_FREE( data.dptr );
	SAFE_FREE( key.dptr );

	return ( ret != -1 );	
}

/*********************************************************************
 ********************************************************************/

bool wcache_tdc_fetch_list( struct winbindd_tdc_domain **domains, size_t *num_domains )
{
	TDB_DATA key = make_tdc_key( lp_workgroup() );
	TDB_DATA data = { NULL, 0 };

	*domains = NULL;	
	*num_domains = 0;	

	if ( !key.dptr )
		return false;

	data = tdb_fetch( wcache->tdb, key );

	SAFE_FREE( key.dptr );

	if ( !data.dptr ) 
		return false;

	*num_domains = unpack_tdc_domains( data.dptr, data.dsize, domains );

	SAFE_FREE( data.dptr );

	if ( !*domains )
		return false;

	return true;
}

/*********************************************************************
 ********************************************************************/

bool wcache_tdc_add_domain( struct winbindd_domain *domain )
{
	struct winbindd_tdc_domain *dom_list = NULL;
	size_t num_domains = 0;
	bool ret = false;

	DEBUG(10,("wcache_tdc_add_domain: Adding domain %s (%s), SID %s, "
		  "flags = 0x%x, attributes = 0x%x, type = 0x%x\n",
		  domain->name, domain->alt_name, 
		  sid_string_dbg(&domain->sid),
		  domain->domain_flags,
		  domain->domain_trust_attribs,
		  domain->domain_type));	

	if ( !init_wcache() ) {
		return false;
	}

	/* fetch the list */

	wcache_tdc_fetch_list( &dom_list, &num_domains );

	/* add the new domain */

	if ( !add_wbdomain_to_tdc_array( domain, &dom_list, &num_domains ) ) {
		goto done;		
	}	

	/* pack the domain */

	if ( !wcache_tdc_store_list( dom_list, num_domains ) ) {
		goto done;		
	}

	/* Success */

	ret = true;
 done:
	TALLOC_FREE( dom_list );

	return ret;	
}

/*********************************************************************
 ********************************************************************/

struct winbindd_tdc_domain * wcache_tdc_fetch_domain( TALLOC_CTX *ctx, const char *name )
{
	struct winbindd_tdc_domain *dom_list = NULL;
	size_t num_domains = 0;
	int i;
	struct winbindd_tdc_domain *d = NULL;	

	DEBUG(10,("wcache_tdc_fetch_domain: Searching for domain %s\n", name));

	if ( !init_wcache() ) {
		return false;
	}

	/* fetch the list */

	wcache_tdc_fetch_list( &dom_list, &num_domains );

	for ( i=0; i<num_domains; i++ ) {
		if ( strequal(name, dom_list[i].domain_name) ||
		     strequal(name, dom_list[i].dns_name) )
		{
			DEBUG(10,("wcache_tdc_fetch_domain: Found domain %s\n",
				  name));

			d = TALLOC_P( ctx, struct winbindd_tdc_domain );
			if ( !d )
				break;			

			d->domain_name = talloc_strdup( d, dom_list[i].domain_name );
			d->dns_name = talloc_strdup( d, dom_list[i].dns_name );
			sid_copy( &d->sid, &dom_list[i].sid );
			d->trust_flags   = dom_list[i].trust_flags;
			d->trust_type    = dom_list[i].trust_type;
			d->trust_attribs = dom_list[i].trust_attribs;

			break;
		}
	}

        TALLOC_FREE( dom_list );

	return d;	
}

/*********************************************************************
 ********************************************************************/

struct winbindd_tdc_domain*
	wcache_tdc_fetch_domainbysid(TALLOC_CTX *ctx,
				     const struct dom_sid *sid)
{
	struct winbindd_tdc_domain *dom_list = NULL;
	size_t num_domains = 0;
	int i;
	struct winbindd_tdc_domain *d = NULL;

	DEBUG(10,("wcache_tdc_fetch_domainbysid: Searching for domain %s\n",
		  sid_string_dbg(sid)));

	if (!init_wcache()) {
		return false;
	}

	/* fetch the list */

	wcache_tdc_fetch_list(&dom_list, &num_domains);

	for (i = 0; i<num_domains; i++) {
		if (sid_equal(sid, &(dom_list[i].sid))) {
			DEBUG(10, ("wcache_tdc_fetch_domainbysid: "
				   "Found domain %s for SID %s\n",
				   dom_list[i].domain_name,
				   sid_string_dbg(sid)));

			d = TALLOC_P(ctx, struct winbindd_tdc_domain);
			if (!d)
				break;

			d->domain_name = talloc_strdup(d,
						       dom_list[i].domain_name);

			d->dns_name = talloc_strdup(d, dom_list[i].dns_name);
			sid_copy(&d->sid, &dom_list[i].sid);
			d->trust_flags = dom_list[i].trust_flags;
			d->trust_type = dom_list[i].trust_type;
			d->trust_attribs = dom_list[i].trust_attribs;

			break;
		}
	}

        TALLOC_FREE(dom_list);

	return d;
}


/*********************************************************************
 ********************************************************************/

void wcache_tdc_clear( void )
{
	if ( !init_wcache() )
		return;

	wcache_tdc_store_list( NULL, 0 );

	return;	
}


/*********************************************************************
 ********************************************************************/

static void wcache_save_user_pwinfo(struct winbindd_domain *domain, 
				    NTSTATUS status,
				    const struct dom_sid *user_sid,
				    const char *homedir,
				    const char *shell,
				    const char *gecos,
				    uint32 gid)
{
	struct cache_entry *centry;
	fstring tmp;

	if ( (centry = centry_start(domain, status)) == NULL )
		return;

	centry_put_string( centry, homedir );
	centry_put_string( centry, shell );
	centry_put_string( centry, gecos );
	centry_put_uint32( centry, gid );

	centry_end(centry, "NSS/PWINFO/%s", sid_to_fstring(tmp, user_sid) );

	DEBUG(10,("wcache_save_user_pwinfo: %s\n", sid_string_dbg(user_sid) ));

	centry_free(centry);
}

#ifdef HAVE_ADS

NTSTATUS nss_get_info_cached( struct winbindd_domain *domain, 
			      const struct dom_sid *user_sid,
			      TALLOC_CTX *ctx,
			      const char **homedir, const char **shell,
			      const char **gecos, gid_t *p_gid)
{
	struct winbind_cache *cache = get_cache(domain);
	struct cache_entry *centry = NULL;
	NTSTATUS nt_status;
	fstring tmp;

	if (!cache->tdb)
		goto do_query;

	centry = wcache_fetch(cache, domain, "NSS/PWINFO/%s",
			      sid_to_fstring(tmp, user_sid));

	if (!centry)
		goto do_query;

	*homedir = centry_string( centry, ctx );
	*shell   = centry_string( centry, ctx );
	*gecos   = centry_string( centry, ctx );
	*p_gid   = centry_uint32( centry );	

	centry_free(centry);

	DEBUG(10,("nss_get_info_cached: [Cached] - user_sid %s\n",
		  sid_string_dbg(user_sid)));

	return NT_STATUS_OK;

do_query:

	nt_status = nss_get_info( domain->name, user_sid, ctx,
				  homedir, shell, gecos, p_gid );

	DEBUG(10, ("nss_get_info returned %s\n", nt_errstr(nt_status)));

	if ( NT_STATUS_IS_OK(nt_status) ) {
		DEBUG(10, ("result:\n\thomedir = '%s'\n", *homedir));
                DEBUGADD(10, ("\tshell = '%s'\n", *shell));
                DEBUGADD(10, ("\tgecos = '%s'\n", *gecos));
                DEBUGADD(10, ("\tgid = '%u'\n", (unsigned int)*p_gid));

		wcache_save_user_pwinfo( domain, nt_status, user_sid,
					 *homedir, *shell, *gecos, *p_gid );
	}	

	if ( NT_STATUS_EQUAL( nt_status, NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND ) ) {
		DEBUG(5,("nss_get_info_cached: Setting domain %s offline\n",
			 domain->name ));
		set_domain_offline( domain );
	}

	return nt_status;	
}

#endif

/* the cache backend methods are exposed via this structure */
struct winbindd_methods cache_methods = {
	true,
	query_user_list,
	enum_dom_groups,
	enum_local_groups,
	name_to_sid,
	sid_to_name,
	rids_to_names,
	query_user,
	lookup_usergroups,
	lookup_useraliases,
	lookup_groupmem,
	sequence_number,
	lockout_policy,
	password_policy,
	trusted_domains
};

static bool wcache_ndr_key(TALLOC_CTX *mem_ctx, char *domain_name,
			   uint32_t opnum, const DATA_BLOB *req,
			   TDB_DATA *pkey)
{
	char *key;
	size_t keylen;

	key = talloc_asprintf(mem_ctx, "NDR/%s/%d/", domain_name, (int)opnum);
	if (key == NULL) {
		return false;
	}
	keylen = talloc_get_size(key) - 1;

	key = talloc_realloc(mem_ctx, key, char, keylen + req->length);
	if (key == NULL) {
		return false;
	}
	memcpy(key + keylen, req->data, req->length);

	pkey->dptr = (uint8_t *)key;
	pkey->dsize = talloc_get_size(key);
	return true;
}

static bool wcache_opnum_cacheable(uint32_t opnum)
{
	switch (opnum) {
	case NDR_WBINT_PING:
	case NDR_WBINT_QUERYSEQUENCENUMBER:
	case NDR_WBINT_ALLOCATEUID:
	case NDR_WBINT_ALLOCATEGID:
	case NDR_WBINT_CHECKMACHINEACCOUNT:
	case NDR_WBINT_CHANGEMACHINEACCOUNT:
	case NDR_WBINT_PINGDC:
		return false;
	}
	return true;
}

bool wcache_fetch_ndr(TALLOC_CTX *mem_ctx, struct winbindd_domain *domain,
		      uint32_t opnum, const DATA_BLOB *req, DATA_BLOB *resp)
{
	TDB_DATA key, data;
	bool ret = false;

	if (!wcache_opnum_cacheable(opnum) ||
	    is_my_own_sam_domain(domain) ||
	    is_builtin_domain(domain)) {
		return false;
	}

	if (wcache->tdb == NULL) {
		return false;
	}

	if (!wcache_ndr_key(talloc_tos(), domain->name, opnum, req, &key)) {
		return false;
	}
	data = tdb_fetch(wcache->tdb, key);
	TALLOC_FREE(key.dptr);

	if (data.dptr == NULL) {
		return false;
	}
	if (data.dsize < 12) {
		goto fail;
	}

	if (!is_domain_offline(domain)) {
		uint32_t entry_seqnum, dom_seqnum, last_check;
		uint64_t entry_timeout;

		if (!wcache_fetch_seqnum(domain->name, &dom_seqnum,
					 &last_check)) {
			goto fail;
		}
		entry_seqnum = IVAL(data.dptr, 0);
		if (entry_seqnum != dom_seqnum) {
			DEBUG(10, ("Entry has wrong sequence number: %d\n",
				   (int)entry_seqnum));
			goto fail;
		}
		entry_timeout = BVAL(data.dptr, 4);
		if (time(NULL) > entry_timeout) {
			DEBUG(10, ("Entry has timed out\n"));
			goto fail;
		}
	}

	resp->data = (uint8_t *)talloc_memdup(mem_ctx, data.dptr + 12,
					      data.dsize - 12);
	if (resp->data == NULL) {
		DEBUG(10, ("talloc failed\n"));
		goto fail;
	}
	resp->length = data.dsize - 12;

	ret = true;
fail:
	SAFE_FREE(data.dptr);
	return ret;
}

void wcache_store_ndr(struct winbindd_domain *domain, uint32_t opnum,
		      const DATA_BLOB *req, const DATA_BLOB *resp)
{
	TDB_DATA key, data;
	uint32_t dom_seqnum, last_check;
	uint64_t timeout;

	if (!wcache_opnum_cacheable(opnum) ||
	    is_my_own_sam_domain(domain) ||
	    is_builtin_domain(domain)) {
		return;
	}

	if (wcache->tdb == NULL) {
		return;
	}

	if (!wcache_fetch_seqnum(domain->name, &dom_seqnum, &last_check)) {
		DEBUG(10, ("could not fetch seqnum for domain %s\n",
			   domain->name));
		return;
	}

	if (!wcache_ndr_key(talloc_tos(), domain->name, opnum, req, &key)) {
		return;
	}

	timeout = time(NULL) + lp_winbind_cache_time();

	data.dsize = resp->length + 12;
	data.dptr = talloc_array(key.dptr, uint8_t, data.dsize);
	if (data.dptr == NULL) {
		goto done;
	}

	SIVAL(data.dptr, 0, dom_seqnum);
	SBVAL(data.dptr, 4, timeout);
	memcpy(data.dptr + 12, resp->data, resp->length);

	tdb_store(wcache->tdb, key, data, 0);

done:
	TALLOC_FREE(key.dptr);
	return;
}
