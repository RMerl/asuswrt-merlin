/*
 * callback.c: A generic callback mechanism 
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
/** @defgroup callback A generic callback mechanism 
 *  @ingroup library
 * 
 *  @{
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <sys/types.h>
#include <stdio.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if !defined(mingw32) && defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/utilities.h>

#include <net-snmp/library/callback.h>
#include <net-snmp/library/snmp_api.h>

netsnmp_feature_child_of(callbacks_all, libnetsnmp)

netsnmp_feature_child_of(callback_count, callbacks_all)
netsnmp_feature_child_of(callback_list, callbacks_all)

/*
 * the inline callback methods use major/minor to index into arrays.
 * all users in this function do range checking before calling these
 * functions, so it is redundant for them to check again. But if you
 * want to be paranoid, define this var, and additional range checks
 * will be performed.
 * #define NETSNMP_PARANOID_LEVEL_HIGH 1 
 */

static int _callback_need_init = 1;
static struct snmp_gen_callback
               *thecallbacks[MAX_CALLBACK_IDS][MAX_CALLBACK_SUBIDS];

#define CALLBACK_NAME_LOGGING 1
#ifdef CALLBACK_NAME_LOGGING
static const char *types[MAX_CALLBACK_IDS] = { "LIB", "APP" };
static const char *lib[MAX_CALLBACK_SUBIDS] = {
    "POST_READ_CONFIG", /* 0 */
    "STORE_DATA", /* 1 */
    "SHUTDOWN", /* 2 */
    "POST_PREMIB_READ_CONFIG", /* 3 */
    "LOGGING", /* 4 */
    "SESSION_INIT", /* 5 */
    NULL, /* 6 */
    NULL, /* 7 */
    NULL, /* 8 */
    NULL, /* 9 */
    NULL, /* 10 */
    NULL, /* 11 */
    NULL, /* 12 */
    NULL, /* 13 */
    NULL, /* 14 */
    NULL /* 15 */
};
#endif

/*
 * extremely simplistic locking, just to find problems were the
 * callback list is modified while being traversed. Not intended
 * to do any real protection, or in any way imply that this code
 * has been evaluated for use in a multi-threaded environment.
 * In 5.2, it was a single lock. For 5.3, it has been updated to
 * a lock per callback, since a particular callback may trigger
 * registration/unregistration of other callbacks (eg AgentX
 * subagents do this).
 */
#define LOCK_PER_CALLBACK_SUBID 1
#ifdef LOCK_PER_CALLBACK_SUBID
static int _locks[MAX_CALLBACK_IDS][MAX_CALLBACK_SUBIDS];
#define CALLBACK_LOCK(maj,min) ++_locks[maj][min]
#define CALLBACK_UNLOCK(maj,min) --_locks[maj][min]
#define CALLBACK_LOCK_COUNT(maj,min) _locks[maj][min]
#else
static int _lock;
#define CALLBACK_LOCK(maj,min) ++_lock
#define CALLBACK_UNLOCK(maj,min) --_lock
#define CALLBACK_LOCK_COUNT(maj,min) _lock
#endif

NETSNMP_STATIC_INLINE int
_callback_lock(int major, int minor, const char* warn, int do_assert)
{
    int lock_holded=0;
    struct timeval lock_time = { 0, 1000 };

#ifdef NETSNMP_PARANOID_LEVEL_HIGH
    if (major >= MAX_CALLBACK_IDS || minor >= MAX_CALLBACK_SUBIDS) {
        netsnmp_assert("bad callback id");
        return 1;
    }
#endif
    
#ifdef CALLBACK_NAME_LOGGING
    DEBUGMSGTL(("9:callback:lock", "locked (%s,%s)\n",
                types[major], (SNMP_CALLBACK_LIBRARY == major) ?
                SNMP_STRORNULL(lib[minor]) : "null"));
#endif
    while (CALLBACK_LOCK_COUNT(major,minor) >= 1 && ++lock_holded < 100)
	select(0, NULL, NULL, NULL, &lock_time);

    if(lock_holded >= 100) {
        if (NULL != warn)
            snmp_log(LOG_WARNING,
                     "lock in _callback_lock sleeps more than 100 milliseconds in %s\n", warn);
        if (do_assert)
            netsnmp_assert(lock_holded < 100);
        
        return 1;
    }

    CALLBACK_LOCK(major,minor);
    return 0;
}

NETSNMP_STATIC_INLINE void
_callback_unlock(int major, int minor)
{
#ifdef NETSNMP_PARANOID_LEVEL_HIGH
    if (major >= MAX_CALLBACK_IDS || minor >= MAX_CALLBACK_SUBIDS) {
        netsnmp_assert("bad callback id");
        return;
    }
#endif
    
    CALLBACK_UNLOCK(major,minor);

#ifdef CALLBACK_NAME_LOGGING
    DEBUGMSGTL(("9:callback:lock", "unlocked (%s,%s)\n",
                types[major], (SNMP_CALLBACK_LIBRARY == major) ?
                SNMP_STRORNULL(lib[minor]) : "null"));
#endif
}


/*
 * the chicken. or the egg.  You pick. 
 */
void
init_callbacks(void)
{
    /*
     * (poses a problem if you put init_callbacks() inside of
     * init_snmp() and then want the app to register a callback before
     * init_snmp() is called in the first place.  -- Wes 
     */
    if (0 == _callback_need_init)
        return;
    
    _callback_need_init = 0;
    
    memset(thecallbacks, 0, sizeof(thecallbacks)); 
#ifdef LOCK_PER_CALLBACK_SUBID
    memset(_locks, 0, sizeof(_locks));
#else
    _lock = 0;
#endif
    
    DEBUGMSGTL(("callback", "initialized\n"));
}

/**
 * This function registers a generic callback function.  The major and
 * minor values are used to set the new_callback function into a global 
 * static multi-dimensional array of type struct snmp_gen_callback.  
 * The function makes sure to append this callback function at the end
 * of the link list, snmp_gen_callback->next.
 *
 * @param major is the SNMP callback major type used
 * 		- SNMP_CALLBACK_LIBRARY
 *              - SNMP_CALLBACK_APPLICATION
 *
 * @param minor is the SNMP callback minor type used
 *		- SNMP_CALLBACK_POST_READ_CONFIG
 *		- SNMP_CALLBACK_STORE_DATA	        
 *		- SNMP_CALLBACK_SHUTDOWN		        
 *		- SNMP_CALLBACK_POST_PREMIB_READ_CONFIG	
 *		- SNMP_CALLBACK_LOGGING			
 *		- SNMP_CALLBACK_SESSION_INIT	       
 *
 * @param new_callback is the callback function that is registered.
 *
 * @param arg when not NULL is a void pointer used whenever new_callback 
 *	function is exercised. Ownership is transferred to the twodimensional
 *      thecallbacks[][] array. The function clear_callback() will deallocate
 *      the memory pointed at by calling free().
 *
 * @return 
 *	Returns SNMPERR_GENERR if major is >= MAX_CALLBACK_IDS or minor is >=
 *	MAX_CALLBACK_SUBIDS or a snmp_gen_callback pointer could not be 
 *	allocated, otherwise SNMPERR_SUCCESS is returned.
 * 	- \#define MAX_CALLBACK_IDS    2
 *	- \#define MAX_CALLBACK_SUBIDS 16
 *
 * @see snmp_call_callbacks
 * @see snmp_unregister_callback
 */
int
snmp_register_callback(int major, int minor, SNMPCallback * new_callback,
                       void *arg)
{
    return netsnmp_register_callback( major, minor, new_callback, arg,
                                      NETSNMP_CALLBACK_DEFAULT_PRIORITY);
}

/**
 * Register a callback function.
 *
 * @param major        Major callback event type.
 * @param minor        Minor callback event type.
 * @param new_callback Callback function being registered.
 * @param arg          Argument that will be passed to the callback function.
 * @param priority     Handler invocation priority. When multiple handlers have
 *   been registered for the same (major, minor) callback event type, handlers
 *   with the numerically lowest priority will be invoked first. Handlers with
 *   identical priority are invoked in the order they have been registered.
 *
 * @see snmp_register_callback
 */
int
netsnmp_register_callback(int major, int minor, SNMPCallback * new_callback,
                          void *arg, int priority)
{
    struct snmp_gen_callback *newscp = NULL, *scp = NULL;
    struct snmp_gen_callback **prevNext = &(thecallbacks[major][minor]);

    if (major >= MAX_CALLBACK_IDS || minor >= MAX_CALLBACK_SUBIDS) {
        return SNMPERR_GENERR;
    }

    if (_callback_need_init)
        init_callbacks();

    _callback_lock(major,minor, "netsnmp_register_callback", 1);
    
    if ((newscp = SNMP_MALLOC_STRUCT(snmp_gen_callback)) == NULL) {
        _callback_unlock(major,minor);
        return SNMPERR_GENERR;
    } else {
        newscp->priority = priority;
        newscp->sc_client_arg = arg;
        newscp->sc_callback = new_callback;
        newscp->next = NULL;

        for (scp = thecallbacks[major][minor]; scp != NULL;
             scp = scp->next) {
            if (newscp->priority < scp->priority) {
                newscp->next = scp;
                break;
            }
            prevNext = &(scp->next);
        }

        *prevNext = newscp;

        DEBUGMSGTL(("callback", "registered (%d,%d) at %p with priority %d\n",
                    major, minor, newscp, priority));
        _callback_unlock(major,minor);
        return SNMPERR_SUCCESS;
    }
}

/**
 * This function calls the callback function for each registered callback of
 * type major and minor.
 *
 * @param major is the SNMP callback major type used
 *
 * @param minor is the SNMP callback minor type used
 *
 * @param caller_arg is a void pointer which is sent in as the callback's 
 *	serverarg parameter, if needed.
 *
 * @return Returns SNMPERR_GENERR if major is >= MAX_CALLBACK_IDS or
 * minor is >= MAX_CALLBACK_SUBIDS, otherwise SNMPERR_SUCCESS is returned.
 *
 * @see snmp_register_callback
 * @see snmp_unregister_callback
 */
int
snmp_call_callbacks(int major, int minor, void *caller_arg)
{
    struct snmp_gen_callback *scp;
    unsigned int    count = 0;
    
    if (major >= MAX_CALLBACK_IDS || minor >= MAX_CALLBACK_SUBIDS) {
        return SNMPERR_GENERR;
    }
    
    if (_callback_need_init)
        init_callbacks();

#ifdef LOCK_PER_CALLBACK_SUBID
    _callback_lock(major,minor,"snmp_call_callbacks", 1);
#else
    /*
     * Notes:
     * - this gets hit the first time a trap is sent after a new trap
     *   destination has been added (session init cb during send trap cb)
     */
    _callback_lock(major,minor, NULL, 0);
#endif

    DEBUGMSGTL(("callback", "START calling callbacks for maj=%d min=%d\n",
                major, minor));

    /*
     * for each registered callback of type major and minor 
     */
    for (scp = thecallbacks[major][minor]; scp != NULL; scp = scp->next) {

        /*
         * skip unregistered callbacks
         */
        if(NULL == scp->sc_callback)
            continue;

        DEBUGMSGTL(("callback", "calling a callback for maj=%d min=%d\n",
                    major, minor));

        /*
         * call them 
         */
        (*(scp->sc_callback)) (major, minor, caller_arg,
                               scp->sc_client_arg);
        count++;
    }

    DEBUGMSGTL(("callback",
                "END calling callbacks for maj=%d min=%d (%d called)\n",
                major, minor, count));

    _callback_unlock(major,minor);
    return SNMPERR_SUCCESS;
}

#ifndef NETSNMP_FEATURE_REMOVE_CALLBACK_COUNT
int
snmp_count_callbacks(int major, int minor)
{
    int             count = 0;
    struct snmp_gen_callback *scp;

    if (major >= MAX_CALLBACK_IDS || minor >= MAX_CALLBACK_SUBIDS) {
        return SNMPERR_GENERR;
    }
    
    if (_callback_need_init)
        init_callbacks();

    for (scp = thecallbacks[major][minor]; scp != NULL; scp = scp->next) {
        count++;
    }

    return count;
}
#endif /* NETSNMP_FEATURE_REMOVE_CALLBACK_COUNT */

int
snmp_callback_available(int major, int minor)
{
    if (major >= MAX_CALLBACK_IDS || minor >= MAX_CALLBACK_SUBIDS) {
        return SNMPERR_GENERR;
    }
    
    if (_callback_need_init)
        init_callbacks();

    if (thecallbacks[major][minor] != NULL) {
        return SNMPERR_SUCCESS;
    }

    return SNMPERR_GENERR;
}

/**
 * This function unregisters a specified callback function given a major
 * and minor type.
 *
 * Note: no bound checking on major and minor.
 *
 * @param major is the SNMP callback major type used
 *
 * @param minor is the SNMP callback minor type used
 *
 * @param target is the callback function that will be unregistered.
 *
 * @param arg is a void pointer used for comparison against the registered 
 *	callback's sc_client_arg variable.
 *
 * @param matchargs is an integer used to bypass the comparison of arg and the
 *	callback's sc_client_arg variable only when matchargs is set to 0.
 *
 *
 * @return
 *        Returns the number of callbacks that were unregistered.
 *
 * @see snmp_register_callback
 * @see snmp_call_callbacks
 */

int
snmp_unregister_callback(int major, int minor, SNMPCallback * target,
                         void *arg, int matchargs)
{
    struct snmp_gen_callback *scp = thecallbacks[major][minor];
    struct snmp_gen_callback **prevNext = &(thecallbacks[major][minor]);
    int             count = 0;

    if (major >= MAX_CALLBACK_IDS || minor >= MAX_CALLBACK_SUBIDS)
        return SNMPERR_GENERR;

    if (_callback_need_init)
        init_callbacks();

#ifdef LOCK_PER_CALLBACK_SUBID
    _callback_lock(major,minor,"snmp_unregister_callback", 1);
#else
    /*
     * Notes;
     * - this gets hit at shutdown, during cleanup. No easy fix.
     */
    _callback_lock(major,minor,"snmp_unregister_callback", 0);
#endif

    while (scp != NULL) {
        if ((scp->sc_callback == target) &&
            (!matchargs || (scp->sc_client_arg == arg))) {
            DEBUGMSGTL(("callback", "unregistering (%d,%d) at %p\n", major,
                        minor, scp));
            if(1 == CALLBACK_LOCK_COUNT(major,minor)) {
                *prevNext = scp->next;
                SNMP_FREE(scp);
                scp = *prevNext;
            }
            else {
                scp->sc_callback = NULL;
                /** set cleanup flag? */
            }
            count++;
        } else {
            prevNext = &(scp->next);
            scp = scp->next;
        }
    }

    _callback_unlock(major,minor);
    return count;
}

/**
 * find and clear client args that match ptr
 *
 * @param ptr  pointer to search for
 * @param i    callback id to start at
 * @param j    callback subid to start at
 */
int
netsnmp_callback_clear_client_arg(void *ptr, int i, int j)
{
    struct snmp_gen_callback *scp = NULL;
    int rc = 0;

    /*
     * don't init i and j before loop, since the caller specified
     * the starting point explicitly. But *after* the i loop has
     * finished executing once, init j to 0 for the next pass
     * through the subids.
     */
    for (; i < MAX_CALLBACK_IDS; i++,j=0) {
        for (; j < MAX_CALLBACK_SUBIDS; j++) {
            scp = thecallbacks[i][j]; 
            while (scp != NULL) {
                if ((NULL != scp->sc_callback) &&
                    (scp->sc_client_arg != NULL) &&
                    (scp->sc_client_arg == ptr)) {
                    DEBUGMSGTL(("9:callback", "  clearing %p at [%d,%d]\n", ptr, i, j));
                    scp->sc_client_arg = NULL;
                    ++rc;
                }
                scp = scp->next;
            }
        }
    }

    if (0 != rc) {
        DEBUGMSGTL(("callback", "removed %d client args\n", rc));
    }

    return rc;
}

void
clear_callback(void)
{
    unsigned int i = 0, j = 0;
    struct snmp_gen_callback *scp = NULL;

    if (_callback_need_init)
        init_callbacks();

    DEBUGMSGTL(("callback", "clear callback\n"));
    for (i = 0; i < MAX_CALLBACK_IDS; i++) {
        for (j = 0; j < MAX_CALLBACK_SUBIDS; j++) {
            _callback_lock(i,j, "clear_callback", 1);
            scp = thecallbacks[i][j];
            while (scp != NULL) {
                thecallbacks[i][j] = scp->next;
                /*
                 * if there is a client arg, check for duplicates
                 * and then free it.
                 */
                if ((NULL != scp->sc_callback) &&
                    (scp->sc_client_arg != NULL)) {
                    void *tmp_arg;
                    /*
                     * save the client arg, then set it to null so that it
                     * won't look like a duplicate, then check for duplicates
                     * starting at the current i,j (earlier dups should have
                     * already been found) and free the pointer.
                     */
                    tmp_arg = scp->sc_client_arg;
                    scp->sc_client_arg = NULL;
                    DEBUGMSGTL(("9:callback", "  freeing %p at [%d,%d]\n", tmp_arg, i, j));
                    (void)netsnmp_callback_clear_client_arg(tmp_arg, i, j);
                    free(tmp_arg);
                }
                SNMP_FREE(scp);
                scp = thecallbacks[i][j];
            }
            _callback_unlock(i,j);
        }
    }
}

#ifndef NETSNMP_FEATURE_REMOVE_CALLBACK_LIST
struct snmp_gen_callback *
snmp_callback_list(int major, int minor)
{
    if (_callback_need_init)
        init_callbacks();

    return (thecallbacks[major][minor]);
}
#endif /* NETSNMP_FEATURE_REMOVE_CALLBACK_LIST */
/**  @} */
