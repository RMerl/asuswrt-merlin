/*
   Unix SMB/CIFS implementation.

   Winbind daemon - krb5 credential cache functions
   and in-memory cache functions.

   Copyright (C) Guenther Deschner 2005-2006
   Copyright (C) Jeremy Allison 2006

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
#include "winbindd.h"
#include "../libcli/auth/libcli_auth.h"
#include "smb_krb5.h"
#include "libads/kerberos_proto.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

/* uncomment this to do fast debugging on the krb5 ticket renewal event */
#ifdef DEBUG_KRB5_TKT_RENEWAL
#undef DEBUG_KRB5_TKT_RENEWAL
#endif

#define MAX_CCACHES 100

static struct WINBINDD_CCACHE_ENTRY *ccache_list;
static void krb5_ticket_gain_handler(struct event_context *,
				     struct timed_event *,
				     struct timeval,
				     void *);
static void add_krb5_ticket_gain_handler_event(struct WINBINDD_CCACHE_ENTRY *,
				     struct timeval);

/* The Krb5 ticket refresh handler should be scheduled
   at one-half of the period from now till the tkt
   expiration */

static time_t krb5_event_refresh_time(time_t end_time)
{
	time_t rest = end_time - time(NULL);
	return end_time - rest/2;
}

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

void ccache_remove_all_after_fork(void)
{
	struct WINBINDD_CCACHE_ENTRY *cur, *next;

	for (cur = ccache_list; cur; cur = next) {
		next = cur->next;
		DLIST_REMOVE(ccache_list, cur);
		TALLOC_FREE(cur->event);
		TALLOC_FREE(cur);
	}

	return;
}

/****************************************************************
 Do the work of refreshing the ticket.
****************************************************************/

static void krb5_ticket_refresh_handler(struct event_context *event_ctx,
					struct timed_event *te,
					struct timeval now,
					void *private_data)
{
	struct WINBINDD_CCACHE_ENTRY *entry =
		talloc_get_type_abort(private_data, struct WINBINDD_CCACHE_ENTRY);
#ifdef HAVE_KRB5
	int ret;
	time_t new_start;
	time_t expire_time = 0;
	struct WINBINDD_MEMORY_CREDS *cred_ptr = entry->cred_ptr;
#endif

	DEBUG(10,("krb5_ticket_refresh_handler called\n"));
	DEBUGADD(10,("event called for: %s, %s\n",
		entry->ccname, entry->username));

	TALLOC_FREE(entry->event);

#ifdef HAVE_KRB5

	/* Kinit again if we have the user password and we can't renew the old
	 * tgt anymore 
	 * NB
	 * This happens when machine are put to sleep for a very long time. */

	if (entry->renew_until < time(NULL)) {
rekinit:
		if (cred_ptr && cred_ptr->pass) {

			set_effective_uid(entry->uid);

			ret = kerberos_kinit_password_ext(entry->principal_name,
							  cred_ptr->pass,
							  0, /* hm, can we do time correction here ? */
							  &entry->refresh_time,
							  &entry->renew_until,
							  entry->ccname,
							  False, /* no PAC required anymore */
							  True,
							  WINBINDD_PAM_AUTH_KRB5_RENEW_TIME,
							  NULL);
			gain_root_privilege();

			if (ret) {
				DEBUG(3,("krb5_ticket_refresh_handler: "
					"could not re-kinit: %s\n",
					error_message(ret)));
				/* destroy the ticket because we cannot rekinit
				 * it, ignore error here */
				ads_kdestroy(entry->ccname);

				/* Don't break the ticket refresh chain: retry 
				 * refreshing ticket sometime later when KDC is 
				 * unreachable -- BoYang. More error code handling
				 * here? 
				 * */

				if ((ret == KRB5_KDC_UNREACH)
				    || (ret == KRB5_REALM_CANT_RESOLVE)) {
#if defined(DEBUG_KRB5_TKT_RENEWAL)
					new_start = time(NULL) + 30;
#else
					new_start = time(NULL) +
						    MAX(30, lp_winbind_cache_time());
#endif
					add_krb5_ticket_gain_handler_event(entry,
							timeval_set(new_start, 0));
					return;
				}
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
			expire_time = entry->refresh_time;
			new_start = krb5_event_refresh_time(entry->refresh_time);
#endif
			goto done;
		} else {
				/* can this happen? 
				 * No cached credentials
				 * destroy ticket and refresh chain 
				 * */
				ads_kdestroy(entry->ccname);
				TALLOC_FREE(entry->event);
				return;
		}
	}

	set_effective_uid(entry->uid);

	ret = smb_krb5_renew_ticket(entry->ccname,
				    entry->principal_name,
				    entry->service,
				    &new_start);
#if defined(DEBUG_KRB5_TKT_RENEWAL)
	new_start = time(NULL) + 30;
#else
	expire_time = new_start;
	new_start = krb5_event_refresh_time(new_start);
#endif

	gain_root_privilege();

	if (ret) {
		DEBUG(3,("krb5_ticket_refresh_handler: "
			"could not renew tickets: %s\n",
			error_message(ret)));
		/* maybe we are beyond the renewing window */

		/* evil rises here, we refresh ticket failed,
		 * but the ticket might be expired. Therefore,
		 * When we refresh ticket failed, destory the 
		 * ticket */

		ads_kdestroy(entry->ccname);

		/* avoid breaking the renewal chain: retry in
		 * lp_winbind_cache_time() seconds when the KDC was not
		 * available right now. 
		 * the return code can be KRB5_REALM_CANT_RESOLVE. 
		 * More error code handling here? */

		if ((ret == KRB5_KDC_UNREACH) 
		    || (ret == KRB5_REALM_CANT_RESOLVE)) {
#if defined(DEBUG_KRB5_TKT_RENEWAL)
			new_start = time(NULL) + 30;
#else
			new_start = time(NULL) +
				    MAX(30, lp_winbind_cache_time());
#endif
			/* ticket is destroyed here, we have to regain it
			 * if it is possible */
			add_krb5_ticket_gain_handler_event(entry,
						timeval_set(new_start, 0));
			return;
		}

		/* This is evil, if the ticket was already expired.
		 * renew ticket function returns KRB5KRB_AP_ERR_TKT_EXPIRED.
		 * But there is still a chance that we can rekinit it. 
		 *
		 * This happens when user login in online mode, and then network
		 * down or something cause winbind goes offline for a very long time,
		 * and then goes online again. ticket expired, renew failed.
		 * This happens when machine are put to sleep for a long time,
		 * but shorter than entry->renew_util.
		 * NB
		 * Looks like the KDC is reachable, we want to rekinit as soon as
		 * possible instead of waiting some time later. */
		if ((ret == KRB5KRB_AP_ERR_TKT_EXPIRED)
		    || (ret == KRB5_FCC_NOFILE)) goto rekinit;

		return;
	}

done:
	/* in cases that ticket will be unrenewable soon, we don't try to renew ticket 
	 * but try to regain ticket if it is possible */
	if (entry->renew_until && expire_time
	     && (entry->renew_until <= expire_time)) {
		/* try to regain ticket 10 seconds before expiration */
		expire_time -= 10;
		add_krb5_ticket_gain_handler_event(entry,
					timeval_set(expire_time, 0));
		return;
	}

	if (entry->refresh_time == 0) {
		entry->refresh_time = new_start;
	}
	entry->event = event_add_timed(winbind_event_context(), entry,
				       timeval_set(new_start, 0),
				       krb5_ticket_refresh_handler,
				       entry);

#endif
}

/****************************************************************
 Do the work of regaining a ticket when coming from offline auth.
****************************************************************/

static void krb5_ticket_gain_handler(struct event_context *event_ctx,
				     struct timed_event *te,
				     struct timeval now,
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
	DEBUGADD(10,("event called for: %s, %s\n",
		entry->ccname, entry->username));

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

	if (!domain->online) {
		goto retry_later;
	}

	set_effective_uid(entry->uid);

	ret = kerberos_kinit_password_ext(entry->principal_name,
					  cred_ptr->pass,
					  0, /* hm, can we do time correction here ? */
					  &entry->refresh_time,
					  &entry->renew_until,
					  entry->ccname,
					  False, /* no PAC required anymore */
					  True,
					  WINBINDD_PAM_AUTH_KRB5_RENEW_TIME,
					  NULL);
	gain_root_privilege();

	if (ret) {
		DEBUG(3,("krb5_ticket_gain_handler: "
			"could not kinit: %s\n",
			error_message(ret)));
		/* evil. If we cannot do it, destroy any the __maybe__ 
		 * __existing__ ticket */
		ads_kdestroy(entry->ccname);
		goto retry_later;
	}

	DEBUG(10,("krb5_ticket_gain_handler: "
		"successful kinit for: %s in ccache: %s\n",
		entry->principal_name, entry->ccname));

	goto got_ticket;

  retry_later:
 
#if defined(DEBUG_KRB5_TKT_RENEWAL)
 	t = timeval_set(time(NULL) + 30, 0);
#else
	t = timeval_current_ofs(MAX(30, lp_winbind_cache_time()), 0);
#endif

	add_krb5_ticket_gain_handler_event(entry, t);
	return;

  got_ticket:

#if defined(DEBUG_KRB5_TKT_RENEWAL)
	t = timeval_set(time(NULL) + 30, 0);
#else
	t = timeval_set(krb5_event_refresh_time(entry->refresh_time), 0);
#endif

	if (entry->refresh_time == 0) {
		entry->refresh_time = t.tv_sec;
	}
	entry->event = event_add_timed(winbind_event_context(),
				       entry,
				       t,
				       krb5_ticket_refresh_handler,
				       entry);

	return;
#endif
}

/**************************************************************
 The gain initial ticket case is recognised as entry->refresh_time
 is always zero.
**************************************************************/

static void add_krb5_ticket_gain_handler_event(struct WINBINDD_CCACHE_ENTRY *entry,
				     struct timeval t)
{
	entry->refresh_time = 0;
	entry->event = event_add_timed(winbind_event_context(),
				       entry,
				       t,
				       krb5_ticket_gain_handler,
				       entry);
}

void ccache_regain_all_now(void)
{
	struct WINBINDD_CCACHE_ENTRY *cur;
	struct timeval t = timeval_current();

	for (cur = ccache_list; cur; cur = cur->next) {
		struct timed_event *new_event;

		/*
		 * if refresh_time is 0, we know that the
		 * the event has the krb5_ticket_gain_handler
		 */
		if (cur->refresh_time == 0) {
			new_event = event_add_timed(winbind_event_context(),
						    cur,
						    t,
						    krb5_ticket_gain_handler,
						    cur);
		} else {
			new_event = event_add_timed(winbind_event_context(),
						    cur,
						    t,
						    krb5_ticket_refresh_handler,
						    cur);
		}

		if (!new_event) {
			continue;
		}

		TALLOC_FREE(cur->event);
		cur->event = new_event;
	}

	return;
}

/****************************************************************
 Check if an ccache entry exists.
****************************************************************/

bool ccache_entry_exists(const char *username)
{
	struct WINBINDD_CCACHE_ENTRY *entry = get_ccache_by_username(username);
	return (entry != NULL);
}

/****************************************************************
 Ensure we're changing the correct entry.
****************************************************************/

bool ccache_entry_identical(const char *username,
			    uid_t uid,
			    const char *ccname)
{
	struct WINBINDD_CCACHE_ENTRY *entry = get_ccache_by_username(username);

	if (!entry) {
		return False;
	}

	if (entry->uid != uid) {
		DEBUG(0,("cache_entry_identical: uid's differ: %u != %u\n",
			(unsigned int)entry->uid, (unsigned int)uid));
		return False;
	}
	if (!strcsequal(entry->ccname, ccname)) {
		DEBUG(0,("cache_entry_identical: "
			"ccnames differ: (cache) %s != (client) %s\n",
                        entry->ccname, ccname));
		return False;
	}
	return True;
}

NTSTATUS add_ccache_to_list(const char *princ_name,
			    const char *ccname,
			    const char *service,
			    const char *username,
			    const char *pass,
			    const char *realm,
			    uid_t uid,
			    time_t create_time,
			    time_t ticket_end,
			    time_t renew_until,
			    bool postponed_request)
{
	struct WINBINDD_CCACHE_ENTRY *entry = NULL;
	struct timeval t;
	NTSTATUS ntret;
#ifdef HAVE_KRB5
	int ret;
#endif

	if ((username == NULL && princ_name == NULL) ||
	    ccname == NULL || uid < 0) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (ccache_entry_count() + 1 > MAX_CCACHES) {
		DEBUG(10,("add_ccache_to_list: "
			"max number of ccaches reached\n"));
		return NT_STATUS_NO_MORE_ENTRIES;
	}

	/* If it is cached login, destroy krb5 ticket
	 * to avoid surprise. */
#ifdef HAVE_KRB5
	if (postponed_request) {
		/* ignore KRB5_FCC_NOFILE error here */
		ret = ads_kdestroy(ccname);
		if (ret == KRB5_FCC_NOFILE) {
			ret = 0;
		}
		if (ret) {
			DEBUG(0, ("add_ccache_to_list: failed to destroy "
				   "user krb5 ccache %s with %s\n", ccname,
				   error_message(ret)));
			return krb5_to_nt_status(ret);
		}
		DEBUG(10, ("add_ccache_to_list: successfully destroyed "
			   "krb5 ccache %s for user %s\n", ccname,
			   username));
	}
#endif

	/* Reference count old entries */
	entry = get_ccache_by_username(username);
	if (entry) {
		/* Check cached entries are identical. */
		if (!ccache_entry_identical(username, uid, ccname)) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		entry->ref_count++;
		DEBUG(10,("add_ccache_to_list: "
			"ref count on entry %s is now %d\n",
			username, entry->ref_count));
		/* FIXME: in this case we still might want to have a krb5 cred
		 * event handler created - gd
		 * Add ticket refresh handler here */

		if (!lp_winbind_refresh_tickets() || renew_until <= 0) {
			return NT_STATUS_OK;
		}

		if (!entry->event) {
			if (postponed_request) {
				t = timeval_current_ofs(MAX(30, lp_winbind_cache_time()), 0);
				add_krb5_ticket_gain_handler_event(entry, t);
			} else {
				/* Renew at 1/2 the ticket expiration time */
#if defined(DEBUG_KRB5_TKT_RENEWAL)
				t = timeval_set(time(NULL)+30, 0);
#else
				t = timeval_set(krb5_event_refresh_time(ticket_end),
						0);
#endif
				if (!entry->refresh_time) {
					entry->refresh_time = t.tv_sec;
				}
				entry->event = event_add_timed(winbind_event_context(),
							       entry,
							       t,
							       krb5_ticket_refresh_handler,
							       entry);
			}

			if (!entry->event) {
				ntret = remove_ccache(username);
				if (!NT_STATUS_IS_OK(ntret)) {
					DEBUG(0, ("add_ccache_to_list: Failed to remove krb5 "
						  "ccache %s for user %s\n", entry->ccname,
						  entry->username));
					DEBUG(0, ("add_ccache_to_list: error is %s\n",
						  nt_errstr(ntret)));
					return ntret;
				}
				return NT_STATUS_NO_MEMORY;
			}

			DEBUG(10,("add_ccache_to_list: added krb5_ticket handler\n"));

		}

		/*
		 * If we're set up to renew our krb5 tickets, we must
		 * cache the credentials in memory for the ticket
		 * renew function (or increase the reference count
		 * if we're logging in more than once). Fix inspired
		 * by patch from Ian Gordon <ian.gordon@strath.ac.uk>
		 * for bugid #9098.
		 */

		ntret = winbindd_add_memory_creds(username, uid, pass);
		DEBUG(10, ("winbindd_add_memory_creds returned: %s\n",
			nt_errstr(ntret)));

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

	if (!lp_winbind_refresh_tickets() || renew_until <= 0) {
		goto add_entry;
	}

	if (postponed_request) {
		t = timeval_current_ofs(MAX(30, lp_winbind_cache_time()), 0);
		add_krb5_ticket_gain_handler_event(entry, t);
	} else {
		/* Renew at 1/2 the ticket expiration time */
#if defined(DEBUG_KRB5_TKT_RENEWAL)
		t = timeval_set(time(NULL)+30, 0);
#else
		t = timeval_set(krb5_event_refresh_time(ticket_end), 0);
#endif
		if (entry->refresh_time == 0) {
			entry->refresh_time = t.tv_sec;
		}
		entry->event = event_add_timed(winbind_event_context(),
					       entry,
					       t,
					       krb5_ticket_refresh_handler,
					       entry);
	}

	if (!entry->event) {
		goto no_mem;
	}

	DEBUG(10,("add_ccache_to_list: added krb5_ticket handler\n"));

 add_entry:

	DLIST_ADD(ccache_list, entry);

	DEBUG(10,("add_ccache_to_list: "
		"added ccache [%s] for user [%s] to the list\n",
		ccname, username));

	if (entry->event) {
		/*
		 * If we're set up to renew our krb5 tickets, we must
		 * cache the credentials in memory for the ticket
		 * renew function. Fix inspired by patch from
		 * Ian Gordon <ian.gordon@strath.ac.uk> for
		 * bugid #9098.
		 */

		ntret = winbindd_add_memory_creds(username, uid, pass);
		DEBUG(10, ("winbindd_add_memory_creds returned: %s\n",
			nt_errstr(ntret)));
	}

	return NT_STATUS_OK;

 no_mem:

	TALLOC_FREE(entry);
	return NT_STATUS_NO_MEMORY;
}

/*******************************************************************
 Remove a WINBINDD_CCACHE_ENTRY entry and the krb5 ccache if no longer
 referenced.
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
		DEBUG(0,("remove_ccache: logic error. "
			"ref count for user %s = %d\n",
			username, entry->ref_count));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	entry->ref_count--;

	if (entry->ref_count > 0) {
		DEBUG(10,("remove_ccache: entry %s ref count now %d\n",
			username, entry->ref_count));
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
		DEBUG(0,("remove_ccache: "
			"failed to destroy user krb5 ccache %s with: %s\n",
			entry->ccname, error_message(ret)));
	} else {
		DEBUG(10,("remove_ccache: "
			"successfully destroyed krb5 ccache %s for user %s\n",
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

static NTSTATUS store_memory_creds(struct WINBINDD_MEMORY_CREDS *memcredp,
				   const char *pass)
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
	memset(memcredp->nt_hash, 0x0, memcredp->len);

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
		memcpy(memcredp->pass, pass,
		       memcredp->len - NT_HASH_LEN - LM_HASH_LEN);
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

static NTSTATUS winbindd_add_memory_creds_internal(const char *username,
						   uid_t uid,
						   const char *pass)
{
	/* Shortcut to ensure we don't store if no mlock. */
#if !defined(HAVE_MLOCK) || !defined(HAVE_MUNLOCK)
	return NT_STATUS_OK;
#else
	NTSTATUS status;
	struct WINBINDD_MEMORY_CREDS *memcredp = NULL;

	memcredp = find_memory_creds_by_name(username);
	if (uid == (uid_t)-1) {
		DEBUG(0,("winbindd_add_memory_creds_internal: "
			"invalid uid for user %s.\n", username));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (memcredp) {
		/* Already exists. Increment the reference count and replace stored creds. */
		if (uid != memcredp->uid) {
			DEBUG(0,("winbindd_add_memory_creds_internal: "
				"uid %u for user %s doesn't "
				"match stored uid %u. Replacing.\n",
				(unsigned int)uid, username,
				(unsigned int)memcredp->uid));
			memcredp->uid = uid;
		}
		memcredp->ref_count++;
		DEBUG(10,("winbindd_add_memory_creds_internal: "
			"ref count for user %s is now %d\n",
			username, memcredp->ref_count));
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

	DEBUG(10,("winbindd_add_memory_creds_internal: "
		"added entry for user %s\n", username));

	return NT_STATUS_OK;
#endif
}

/*************************************************************
 Store users credentials in memory. If we also have a
 struct WINBINDD_CCACHE_ENTRY for this username with a
 refresh timer, then store the plaintext of the password
 and associate the new credentials with the struct WINBINDD_CCACHE_ENTRY.
*************************************************************/

NTSTATUS winbindd_add_memory_creds(const char *username,
				   uid_t uid,
				   const char *pass)
{
	struct WINBINDD_CCACHE_ENTRY *entry = get_ccache_by_username(username);
	NTSTATUS status;

	status = winbindd_add_memory_creds_internal(username, uid, pass);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (entry) {
		struct WINBINDD_MEMORY_CREDS *memcredp = NULL;
		memcredp = find_memory_creds_by_name(username);
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
	struct WINBINDD_MEMORY_CREDS *memcredp = NULL;
	struct WINBINDD_CCACHE_ENTRY *entry = NULL;
	NTSTATUS status = NT_STATUS_OK;

	memcredp = find_memory_creds_by_name(username);
	entry = get_ccache_by_username(username);

	if (!memcredp) {
		DEBUG(10,("winbindd_delete_memory_creds: unknown user %s\n",
			username));
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (memcredp->ref_count <= 0) {
		DEBUG(0,("winbindd_delete_memory_creds: logic error. "
			"ref count for user %s = %d\n",
			username, memcredp->ref_count));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	memcredp->ref_count--;
	if (memcredp->ref_count <= 0) {
		delete_memory_creds(memcredp);
		DLIST_REMOVE(memory_creds_list, memcredp);
		talloc_destroy(memcredp);
		DEBUG(10,("winbindd_delete_memory_creds: "
			"deleted entry for user %s\n",
			username));
	} else {
		DEBUG(10,("winbindd_delete_memory_creds: "
			"entry for user %s ref_count now %d\n",
			username, memcredp->ref_count));
	}

	if (entry) {
		/* Ensure we have no dangling references to this. */
		entry->cred_ptr = NULL;
	}

	return status;
}

/***********************************************************
 Replace the required creds with new ones (password change).
***********************************************************/

NTSTATUS winbindd_replace_memory_creds(const char *username,
				       const char *pass)
{
	struct WINBINDD_MEMORY_CREDS *memcredp = NULL;

	memcredp = find_memory_creds_by_name(username);
	if (!memcredp) {
		DEBUG(10,("winbindd_replace_memory_creds: unknown user %s\n",
			username));
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	DEBUG(10,("winbindd_replace_memory_creds: replaced creds for user %s\n",
		username));

	return winbindd_replace_memory_creds_internal(memcredp, pass);
}
