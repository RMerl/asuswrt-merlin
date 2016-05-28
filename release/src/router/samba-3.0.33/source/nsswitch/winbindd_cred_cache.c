/*
   Unix SMB/CIFS implementation.

   Winbind daemon - krb5 credential cache functions
   and in-memory cache functions.

   Copyright (C) Guenther Deschner 2005-2006
   Copyright (C) Jeremy Allison 2006
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "winbindd.h"
#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

/* uncomment this to to fast debugging on the krb5 ticket renewal event */
#ifdef DEBUG_KRB5_TKT_RENEWAL
#undef DEBUG_KRB5_TKT_RENEWAL
#endif

#define MAX_CCACHES 100

static struct WINBINDD_CCACHE_ENTRY *ccache_list;

/* The Krb5 ticket refresh handler should be scheduled 
   at one-half of the period from now till the tkt 
   expiration */
#define KRB5_EVENT_REFRESH_TIME(x) ((x) - (((x) - time(NULL))/2))

/****************************************************************
 Find an entry by name.
****************************************************************/

static struct WINBINDD_CCACHE_ENTRY *get_ccache_by_username(const char *username)
{
	struct WINBINDD_CCACHE_ENTRY *entry;

	for (entry = ccache_list; entry; entry = entry->next) {
		if (strequal(entry->username, username)) {
			return entry;
		}
	}
	return NULL;
}

/****************************************************************
 How many do we have ?
****************************************************************/

static int ccache_entry_count(void)
{
	struct WINBINDD_CCACHE_ENTRY *entry;
	int i = 0;

	for (entry = ccache_list; entry; entry = entry->next) {
		i++;
	}
	return i;
}

/****************************************************************
 Do the work of refreshing the ticket.
****************************************************************/

static void krb5_ticket_refresh_handler(struct event_context *event_ctx,
					struct timed_event *te,
					const struct timeval *now,
					void *private_data)
{
	struct WINBINDD_CCACHE_ENTRY *entry =
		talloc_get_type_abort(private_data, struct WINBINDD_CCACHE_ENTRY);
#ifdef HAVE_KRB5
	int ret;
	time_t new_start;
	struct WINBINDD_MEMORY_CREDS *cred_ptr = entry->cred_ptr;
#endif

	DEBUG(10,("krb5_ticket_refresh_handler called\n"));
	DEBUGADD(10,("event called for: %s, %s\n", entry->ccname, entry->username));

	TALLOC_FREE(entry->event);

#ifdef HAVE_KRB5

	/* Kinit again if we have the user password and we can't renew the old
	 * tgt anymore */

	if ((entry->renew_until < time(NULL)) && cred_ptr && cred_ptr->pass) {
	     
		set_effective_uid(entry->uid);

		ret = kerberos_kinit_password_ext(entry->principal_name,
						  cred_ptr->pass,
						  0, /* hm, can we do time correction here ? */
						  &entry->refresh_time,
						  &entry->renew_until,
						  entry->ccname,
						  False, /* no PAC required anymore */
						  True,
						  WINBINDD_PAM_AUTH_KRB5_RENEW_TIME);
		gain_root_privilege();

		if (ret) {
			DEBUG(3,("krb5_ticket_refresh_handler: could not re-kinit: %s\n",
				error_message(ret)));
			TALLOC_FREE(entry->event);
			return;
		}

		DEBUG(10,("krb5_ticket_refresh_handler: successful re-kinit "
			"for: %s in ccache: %s\n", 
			entry->principal_name, entry->ccname));

#if defined(DEBUG_KRB5_TKT_RENEWAL)
		new_start = time(NULL) + 30;		
#else
		/* The tkt should be refreshed at one-half the period
		   from now to the expiration time */
		new_start = KRB5_EVENT_REFRESH_TIME(entry->refresh_time);
#endif

		goto done;
	}

	set_effective_uid(entry->uid);

	ret = smb_krb5_renew_ticket(entry->ccname, 
				    entry->principal_name,
				    entry->service,
				    &new_start);
#if defined(DEBUG_KRB5_TKT_RENEWAL)
	new_start = time(NULL) + 30;
#else
	new_start = KRB5_EVENT_REFRESH_TIME(new_start);
#endif

	gain_root_privilege();

	if (ret) {
		DEBUG(3,("krb5_ticket_refresh_handler: could not renew tickets: %s\n",
			error_message(ret)));
		/* maybe we are beyond the renewing window */

		/* avoid breaking the renewal chain: retry in lp_winbind_cache_time()
		 * seconds when the KDC was not available right now. */

		if (ret == KRB5_KDC_UNREACH) {
			new_start = time(NULL) + MAX(30, lp_winbind_cache_time());
			goto done;
		}

		return;
	}

done:

	entry->event = event_add_timed(winbind_event_context(), entry, 
				       timeval_set(new_start, 0),
				       "krb5_ticket_refresh_handler",
				       krb5_ticket_refresh_handler,
				       entry);

#endif
}

/****************************************************************
 Do the work of regaining a ticket when coming from offline auth.
****************************************************************/

static void krb5_ticket_gain_handler(struct event_context *event_ctx,
				     struct timed_event *te,
					const struct timeval *now,
					void *private_data)
{
	struct WINBINDD_CCACHE_ENTRY *entry =
		talloc_get_type_abort(private_data, struct WINBINDD_CCACHE_ENTRY);
#ifdef HAVE_KRB5
	int ret;
	struct timeval t;
	struct WINBINDD_MEMORY_CREDS *cred_ptr = entry->cred_ptr;
	struct winbindd_domain *domain = NULL;
#endif

	DEBUG(10,("krb5_ticket_gain_handler called\n"));
	DEBUGADD(10,("event called for: %s, %s\n", entry->ccname, entry->username));

	TALLOC_FREE(entry->event);

#ifdef HAVE_KRB5

	if (!cred_ptr || !cred_ptr->pass) {
		DEBUG(10,("krb5_ticket_gain_handler: no memory creds\n"));
		return;
	}

	if ((domain = find_domain_from_name(entry->realm)) == NULL) {
		DEBUG(0,("krb5_ticket_gain_handler: unknown domain\n"));
		return;
	}

	if (domain->online) {

		set_effective_uid(entry->uid);

		ret = kerberos_kinit_password_ext(entry->principal_name,
						cred_ptr->pass,
						0, /* hm, can we do time correction here ? */
						&entry->refresh_time,
						&entry->renew_until,
						entry->ccname,
						False, /* no PAC required anymore */
						True,
						WINBINDD_PAM_AUTH_KRB5_RENEW_TIME);
		gain_root_privilege();

		if (ret) {
			DEBUG(3,("krb5_ticket_gain_handler: could not kinit: %s\n",
				error_message(ret)));
			goto retry_later;
		}

		DEBUG(10,("krb5_ticket_gain_handler: successful kinit for: %s in ccache: %s\n",
			entry->principal_name, entry->ccname));

		goto got_ticket;
	}

  retry_later:

	entry->event = event_add_timed(winbind_event_context(), entry,
					timeval_current_ofs(MAX(30, lp_winbind_cache_time()), 0),
					"krb5_ticket_gain_handler",
					krb5_ticket_gain_handler,
					entry);

	return;

  got_ticket:

#if defined(DEBUG_KRB5_TKT_RENEWAL)
	t = timeval_set(time(NULL) + 30, 0);
#else
	t = timeval_set(KRB5_EVENT_REFRESH_TIME(entry->refresh_time), 0);
#endif

	entry->event = event_add_timed(winbind_event_context(), entry,
					t,
					"krb5_ticket_refresh_handler",
					krb5_ticket_refresh_handler,
					entry);

	return;
#endif
}

/****************************************************************
 Check if an ccache entry exists.
****************************************************************/

BOOL ccache_entry_exists(const char *username)
{
	struct WINBINDD_CCACHE_ENTRY *entry = get_ccache_by_username(username);
	return (entry != NULL);
}

/****************************************************************
 Ensure we're changing the correct entry.
****************************************************************/

BOOL ccache_entry_identical(const char *username, uid_t uid, const char *ccname)
{
	struct WINBINDD_CCACHE_ENTRY *entry = get_ccache_by_username(username);

	if (!entry) {
		return False;
	}

	if (entry->uid != uid) {
		DEBUG(0,("cache_entry_identical: uid's differ: %u != %u\n",
			(unsigned int)entry->uid, (unsigned int)uid ));
		return False;
	}
	if (!strcsequal(entry->ccname, ccname)) {
		DEBUG(0,("cache_entry_identical: ccnames differ: (cache) %s != (client) %s\n",
                        entry->ccname, ccname));
		return False;
	}
	return True;
}

NTSTATUS add_ccache_to_list(const char *princ_name,
			    const char *ccname,
			    const char *service,
			    const char *username, 
			    const char *realm,
			    uid_t uid,
			    time_t create_time, 
			    time_t ticket_end, 
			    time_t renew_until, 
			    BOOL postponed_request)
{
	struct WINBINDD_CCACHE_ENTRY *entry = NULL;

	if ((username == NULL && princ_name == NULL) || ccname == NULL || uid < 0) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (ccache_entry_count() + 1 > MAX_CCACHES) {
		DEBUG(10,("add_ccache_to_list: max number of ccaches reached\n"));
		return NT_STATUS_NO_MORE_ENTRIES;
	}

	/* Reference count old entries */
	entry = get_ccache_by_username(username);
	if (entry) {
		/* Check cached entries are identical. */
		if (!ccache_entry_identical(username, uid, ccname)) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		entry->ref_count++;
		DEBUG(10,("add_ccache_to_list: ref count on entry %s is now %d\n",
			username, entry->ref_count));
		/* FIXME: in this case we still might want to have a krb5 cred
		 * event handler created - gd*/
		return NT_STATUS_OK;
	}
	
	entry = TALLOC_P(NULL, struct WINBINDD_CCACHE_ENTRY);
	if (!entry) {
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCTP(entry);

	if (username) {
		entry->username = talloc_strdup(entry, username);
		if (!entry->username) {
			goto no_mem;
		}
	}
	if (princ_name) {
		entry->principal_name = talloc_strdup(entry, princ_name);
		if (!entry->principal_name) {
			goto no_mem;
		}
	}
	if (service) {
		entry->service = talloc_strdup(entry, service);
		if (!entry->service) {
			goto no_mem;
		}
	}

	entry->ccname = talloc_strdup(entry, ccname);
	if (!entry->ccname) {
		goto no_mem;
	}

	entry->realm = talloc_strdup(entry, realm);
	if (!entry->realm) {
		goto no_mem;
	}

	entry->create_time = create_time;
	entry->renew_until = renew_until;
	entry->uid = uid;
	entry->ref_count = 1;

	if (lp_winbind_refresh_tickets() && renew_until > 0) {
		if (postponed_request) {
			entry->event = event_add_timed(winbind_event_context(), entry,
						timeval_current_ofs(MAX(30, lp_winbind_cache_time()), 0),
						"krb5_ticket_gain_handler",
						krb5_ticket_gain_handler,
						entry);
		} else {
			/* Renew at 1/2 the ticket expiration time */
			entry->event = event_add_timed(winbind_event_context(), entry,
#if defined(DEBUG_KRB5_TKT_RENEWAL)
						timeval_set(time(NULL)+30, 0),
#else
						timeval_set(KRB5_EVENT_REFRESH_TIME(ticket_end), 0),
#endif
						"krb5_ticket_refresh_handler",
						krb5_ticket_refresh_handler,
						entry);
		}

		if (!entry->event) {
			goto no_mem;
		}

		DEBUG(10,("add_ccache_to_list: added krb5_ticket handler\n"));
	}

	DLIST_ADD(ccache_list, entry);

	DEBUG(10,("add_ccache_to_list: added ccache [%s] for user [%s] to the list\n", ccname, username));

	return NT_STATUS_OK;

 no_mem:

	TALLOC_FREE(entry);
	return NT_STATUS_NO_MEMORY;
}

/*******************************************************************
 Remove a WINBINDD_CCACHE_ENTRY entry and the krb5 ccache if no longer referenced.
*******************************************************************/

NTSTATUS remove_ccache(const char *username)
{
	struct WINBINDD_CCACHE_ENTRY *entry = get_ccache_by_username(username);
	NTSTATUS status = NT_STATUS_OK;
#ifdef HAVE_KRB5
	krb5_error_code ret;
#endif

	if (!entry) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (entry->ref_count <= 0) {
		DEBUG(0,("remove_ccache: logic error. ref count for user %s = %d\n",
			username, entry->ref_count));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	entry->ref_count--;

	if (entry->ref_count > 0) {
		DEBUG(10,("remove_ccache: entry %s ref count now %d\n",
			username, entry->ref_count ));
		return NT_STATUS_OK;
	}

	/* no references any more */

	DLIST_REMOVE(ccache_list, entry);
	TALLOC_FREE(entry->event); /* unregisters events */

#ifdef HAVE_KRB5
	ret = ads_kdestroy(entry->ccname);

	/* we ignore the error when there has been no credential cache */
	if (ret == KRB5_FCC_NOFILE) {
		ret = 0;
	} else if (ret) {
		DEBUG(0,("remove_ccache: failed to destroy user krb5 ccache %s with: %s\n",
			entry->ccname, error_message(ret)));
	} else {
		DEBUG(10,("remove_ccache: successfully destroyed krb5 ccache %s for user %s\n",
			entry->ccname, username));
	}
	status = krb5_to_nt_status(ret);
#endif

	TALLOC_FREE(entry);
 	DEBUG(10,("remove_ccache: removed ccache for user %s\n", username));

	return status;
}

/*******************************************************************
 In memory credentials cache code.
*******************************************************************/

static struct WINBINDD_MEMORY_CREDS *memory_creds_list;

/***********************************************************
 Find an entry on the list by name.
***********************************************************/

struct WINBINDD_MEMORY_CREDS *find_memory_creds_by_name(const char *username)
{
	struct WINBINDD_MEMORY_CREDS *p;

	for (p = memory_creds_list; p; p = p->next) {
		if (strequal(p->username, username)) {
			return p;
		}
	}
	return NULL;
}

/***********************************************************
 Store the required creds and mlock them.
***********************************************************/

static NTSTATUS store_memory_creds(struct WINBINDD_MEMORY_CREDS *memcredp, const char *pass)
{
#if !defined(HAVE_MLOCK)
	return NT_STATUS_OK;
#else
	/* new_entry->nt_hash is the base pointer for the block
	   of memory pointed into by new_entry->lm_hash and
	   new_entry->pass (if we're storing plaintext). */

	memcredp->len = NT_HASH_LEN + LM_HASH_LEN;
	if (pass) {
		memcredp->len += strlen(pass)+1;
	}


#if defined(LINUX)
	/* aligning the memory on on x86_64 and compiling 
	   with gcc 4.1 using -O2 causes a segv in the 
	   next memset()  --jerry */
	memcredp->nt_hash = SMB_MALLOC_ARRAY(unsigned char, memcredp->len);
#else
	/* On non-linux platforms, mlock()'d memory must be aligned */
	memcredp->nt_hash = SMB_MEMALIGN_ARRAY(unsigned char, 
					       getpagesize(), memcredp->len);
#endif
	if (!memcredp->nt_hash) {
		return NT_STATUS_NO_MEMORY;
	}
	memset( memcredp->nt_hash, 0x0, memcredp->len );

	memcredp->lm_hash = memcredp->nt_hash + NT_HASH_LEN;

#ifdef DEBUG_PASSWORD
	DEBUG(10,("mlocking memory: %p\n", memcredp->nt_hash));
#endif		
	if ((mlock(memcredp->nt_hash, memcredp->len)) == -1) {
		DEBUG(0,("failed to mlock memory: %s (%d)\n", 
			strerror(errno), errno));
		SAFE_FREE(memcredp->nt_hash);
		return map_nt_error_from_unix(errno);
	}

#ifdef DEBUG_PASSWORD
	DEBUG(10,("mlocked memory: %p\n", memcredp->nt_hash));
#endif		

	/* Create and store the password hashes. */
	E_md4hash(pass, memcredp->nt_hash);
	E_deshash(pass, memcredp->lm_hash);

	if (pass) {
		memcredp->pass = (char *)memcredp->lm_hash + LM_HASH_LEN;
		memcpy(memcredp->pass, pass, memcredp->len - NT_HASH_LEN - LM_HASH_LEN);
	}

	return NT_STATUS_OK;
#endif
}

/***********************************************************
 Destroy existing creds.
***********************************************************/

static NTSTATUS delete_memory_creds(struct WINBINDD_MEMORY_CREDS *memcredp)
{
#if !defined(HAVE_MUNLOCK)
	return NT_STATUS_OK;
#else
	if (munlock(memcredp->nt_hash, memcredp->len) == -1) {
		DEBUG(0,("failed to munlock memory: %s (%d)\n", 
			strerror(errno), errno));
		return map_nt_error_from_unix(errno);
	}
	memset(memcredp->nt_hash, '\0', memcredp->len);
	SAFE_FREE(memcredp->nt_hash);
	memcredp->nt_hash = NULL;
	memcredp->lm_hash = NULL;
	memcredp->pass = NULL;
	memcredp->len = 0;
	return NT_STATUS_OK;
#endif
}

/***********************************************************
 Replace the required creds with new ones (password change).
***********************************************************/

static NTSTATUS winbindd_replace_memory_creds_internal(struct WINBINDD_MEMORY_CREDS *memcredp,
						const char *pass)
{
	NTSTATUS status = delete_memory_creds(memcredp);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	return store_memory_creds(memcredp, pass);
}

/*************************************************************
 Store credentials in memory in a list.
*************************************************************/

static NTSTATUS winbindd_add_memory_creds_internal(const char *username, uid_t uid, const char *pass)
{
	/* Shortcut to ensure we don't store if no mlock. */
#if !defined(HAVE_MLOCK) || !defined(HAVE_MUNLOCK)
	return NT_STATUS_OK;
#else
	NTSTATUS status;
	struct WINBINDD_MEMORY_CREDS *memcredp = find_memory_creds_by_name(username);

	if (uid == (uid_t)-1) {
		DEBUG(0,("winbindd_add_memory_creds_internal: invalid uid for user %s.\n",
			username ));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (memcredp) {
		/* Already exists. Increment the reference count and replace stored creds. */
		if (uid != memcredp->uid) {
			DEBUG(0,("winbindd_add_memory_creds_internal: uid %u for user %s doesn't "
				"match stored uid %u. Replacing.\n",
				(unsigned int)uid, username, (unsigned int)memcredp->uid ));
			memcredp->uid = uid;
		}
		memcredp->ref_count++;
		DEBUG(10,("winbindd_add_memory_creds_internal: ref count for user %s is now %d\n",
			username, memcredp->ref_count ));
		return winbindd_replace_memory_creds_internal(memcredp, pass);
	}

	memcredp = TALLOC_ZERO_P(NULL, struct WINBINDD_MEMORY_CREDS);
	if (!memcredp) {
		return NT_STATUS_NO_MEMORY;
	}
	memcredp->username = talloc_strdup(memcredp, username);
	if (!memcredp->username) {
		talloc_destroy(memcredp);
		return NT_STATUS_NO_MEMORY;
	}

	status = store_memory_creds(memcredp, pass);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_destroy(memcredp);
		return status;
	}

	memcredp->uid = uid;
	memcredp->ref_count = 1;
	DLIST_ADD(memory_creds_list, memcredp);

	DEBUG(10,("winbindd_add_memory_creds_internal: added entry for user %s\n",
		username ));

	return NT_STATUS_OK;
#endif
}

/*************************************************************
 Store users credentials in memory. If we also have a 
 struct WINBINDD_CCACHE_ENTRY for this username with a
 refresh timer, then store the plaintext of the password
 and associate the new credentials with the struct WINBINDD_CCACHE_ENTRY.
*************************************************************/

NTSTATUS winbindd_add_memory_creds(const char *username, uid_t uid, const char *pass)
{
	struct WINBINDD_CCACHE_ENTRY *entry = get_ccache_by_username(username);
	NTSTATUS status;

	status = winbindd_add_memory_creds_internal(username, uid, pass);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (entry) {
		struct WINBINDD_MEMORY_CREDS *memcredp = find_memory_creds_by_name(username);
		if (memcredp) {
			entry->cred_ptr = memcredp;
		}
	}

	return status;
}

/*************************************************************
 Decrement the in-memory ref count - delete if zero.
*************************************************************/

NTSTATUS winbindd_delete_memory_creds(const char *username)
{
	struct WINBINDD_MEMORY_CREDS *memcredp = find_memory_creds_by_name(username);
	struct WINBINDD_CCACHE_ENTRY *entry = get_ccache_by_username(username);
	NTSTATUS status = NT_STATUS_OK;

	if (!memcredp) {
		DEBUG(10,("winbindd_delete_memory_creds: unknown user %s\n",
			username ));
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (memcredp->ref_count <= 0) {
		DEBUG(0,("winbindd_delete_memory_creds: logic error. ref count for user %s = %d\n",
			username, memcredp->ref_count));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	memcredp->ref_count--;
	if (memcredp->ref_count <= 0) {
		delete_memory_creds(memcredp);
		DLIST_REMOVE(memory_creds_list, memcredp);
		talloc_destroy(memcredp);
		DEBUG(10,("winbindd_delete_memory_creds: deleted entry for user %s\n",
			username));
	} else {
		DEBUG(10,("winbindd_delete_memory_creds: entry for user %s ref_count now %d\n",
			username, memcredp->ref_count));
	}

	if (entry) {
		entry->cred_ptr = NULL; /* Ensure we have no dangling references to this. */
	}
	return status;
}

/***********************************************************
 Replace the required creds with new ones (password change).
***********************************************************/

NTSTATUS winbindd_replace_memory_creds(const char *username, const char *pass)
{
	struct WINBINDD_MEMORY_CREDS *memcredp = find_memory_creds_by_name(username);

	if (!memcredp) {
		DEBUG(10,("winbindd_replace_memory_creds: unknown user %s\n",
			username ));
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	DEBUG(10,("winbindd_replace_memory_creds: replaced creds for user %s\n",
		username ));

	return winbindd_replace_memory_creds_internal(memcredp, pass);
}
