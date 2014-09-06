/*
 * snmp_alarm.c:
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
/** @defgroup snmp_alarm  generic library based alarm timers for various parts of an application 
 *  @ingroup library
 * 
 *  @{
 */
#include <net-snmp/net-snmp-config.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/config_api.h>
#include <net-snmp/utilities.h>

#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/callback.h>
#include <net-snmp/library/snmp_alarm.h>

static struct snmp_alarm *thealarms = NULL;
static int      start_alarms = 0;
static unsigned int regnum = 1;

int
init_alarm_post_config(int majorid, int minorid, void *serverarg,
                       void *clientarg)
{
    start_alarms = 1;
    set_an_alarm();
    return SNMPERR_SUCCESS;
}

void
init_snmp_alarm(void)
{
    start_alarms = 0;
    snmp_register_callback(SNMP_CALLBACK_LIBRARY,
                           SNMP_CALLBACK_POST_READ_CONFIG,
                           init_alarm_post_config, NULL);
}

void
sa_update_entry(struct snmp_alarm *a)
{
    if (!timerisset(&a->t_lastM)) {
        /*
         * First call of sa_update_entry() for alarm a: set t_lastM and t_nextM.
         */
        netsnmp_get_monotonic_clock(&a->t_lastM);
        NETSNMP_TIMERADD(&a->t_lastM, &a->t, &a->t_nextM);
    } else if (!timerisset(&a->t_nextM)) {
        /*
         * We've been called but not reset for the next call.  
         */
        if (a->flags & SA_REPEAT) {
            if (timerisset(&a->t)) {
                NETSNMP_TIMERADD(&a->t_lastM, &a->t, &a->t_nextM);
            } else {
                DEBUGMSGTL(("snmp_alarm",
                            "update_entry: illegal interval specified\n"));
                snmp_alarm_unregister(a->clientreg);
            }
        } else {
            /*
             * Single time call, remove it.  
             */
            snmp_alarm_unregister(a->clientreg);
        }
    }
}

/**
 * This function removes the callback function from a list of registered
 * alarms, unregistering the alarm.
 *
 * @param clientreg is a unique unsigned integer representing a registered
 *	alarm which the client wants to unregister.
 *
 * @return void
 *
 * @see snmp_alarm_register
 * @see snmp_alarm_register_hr
 * @see snmp_alarm_unregister_all
 */
void
snmp_alarm_unregister(unsigned int clientreg)
{
    struct snmp_alarm *sa_ptr, **prevNext = &thealarms;

    for (sa_ptr = thealarms;
         sa_ptr != NULL && sa_ptr->clientreg != clientreg;
         sa_ptr = sa_ptr->next) {
        prevNext = &(sa_ptr->next);
    }

    if (sa_ptr != NULL) {
        *prevNext = sa_ptr->next;
        DEBUGMSGTL(("snmp_alarm", "unregistered alarm %d\n", 
		    sa_ptr->clientreg));
        /*
         * Note: do not free the clientarg, it's the client's responsibility 
         */
        free(sa_ptr);
    } else {
        DEBUGMSGTL(("snmp_alarm", "no alarm %d to unregister\n", clientreg));
    }
}

/**
 * This function unregisters all alarms currently stored.
 *
 * @return void
 *
 * @see snmp_alarm_register
 * @see snmp_alarm_register_hr
 * @see snmp_alarm_unregister
 */
void
snmp_alarm_unregister_all(void)
{
  struct snmp_alarm *sa_ptr, *sa_tmp;

  for (sa_ptr = thealarms; sa_ptr != NULL; sa_ptr = sa_tmp) {
    sa_tmp = sa_ptr->next;
    free(sa_ptr);
  }
  DEBUGMSGTL(("snmp_alarm", "ALL alarms unregistered\n"));
  thealarms = NULL;
}  

struct snmp_alarm *
sa_find_next(void)
{
    struct snmp_alarm *a, *lowest = NULL;

    for (a = thealarms; a != NULL; a = a->next)
        if (!(a->flags & SA_FIRED)
            && (lowest == NULL || timercmp(&a->t_nextM, &lowest->t_nextM, <)))
            lowest = a;

    return lowest;
}

NETSNMP_IMPORT struct snmp_alarm *sa_find_specific(unsigned int clientreg);
struct snmp_alarm *
sa_find_specific(unsigned int clientreg)
{
    struct snmp_alarm *sa_ptr;
    for (sa_ptr = thealarms; sa_ptr != NULL; sa_ptr = sa_ptr->next) {
        if (sa_ptr->clientreg == clientreg) {
            return sa_ptr;
        }
    }
    return NULL;
}

void
run_alarms(void)
{
    struct snmp_alarm *a;
    unsigned int    clientreg;
    struct timeval  t_now;

    /*
     * Loop through everything we have repeatedly looking for the next thing to
     * call until all events are finally in the future again.  
     */

    while ((a = sa_find_next()) != NULL) {
        netsnmp_get_monotonic_clock(&t_now);

        if (timercmp(&a->t_nextM, &t_now, >))
            return;

        clientreg = a->clientreg;
        a->flags |= SA_FIRED;
        DEBUGMSGTL(("snmp_alarm", "run alarm %d\n", clientreg));
        (*(a->thecallback)) (clientreg, a->clientarg);
        DEBUGMSGTL(("snmp_alarm", "alarm %d completed\n", clientreg));

        a = sa_find_specific(clientreg);
        if (a) {
            a->t_lastM = t_now;
            timerclear(&a->t_nextM);
            a->flags &= ~SA_FIRED;
            sa_update_entry(a);
        } else {
            DEBUGMSGTL(("snmp_alarm", "alarm %d deleted itself\n",
                        clientreg));
        }
    }
}



RETSIGTYPE
alarm_handler(int a)
{
    run_alarms();
    set_an_alarm();
}



/**
 * Look up the time at which the next alarm will fire.
 *
 * @param[out] alarm_tm Time at which the next alarm will fire.
 * @param[in] now Earliest time that should be written into *alarm_tm.
 *
 * @return Zero if no alarms are scheduled; non-zero 'clientreg' value
 *   identifying the first alarm that will fire if one or more alarms are
 *   scheduled.
 */
int
netsnmp_get_next_alarm_time(struct timeval *alarm_tm, const struct timeval *now)
{
    struct snmp_alarm *sa_ptr;

    sa_ptr = sa_find_next();

    if (sa_ptr) {
        netsnmp_assert(alarm_tm);
        netsnmp_assert(timerisset(&sa_ptr->t_nextM));
        if (timercmp(&sa_ptr->t_nextM, now, >))
            *alarm_tm = sa_ptr->t_nextM;
        else
            *alarm_tm = *now;
        return sa_ptr->clientreg;
    } else {
        return 0;
    }
}

/**
 * Get the time until the next alarm will fire.
 *
 * @param[out] delta Time until the next alarm.
 *
 * @return Zero if no alarms are scheduled; non-zero 'clientreg' value
 *   identifying the first alarm that will fire if one or more alarms are
 *   scheduled.
 */
int
get_next_alarm_delay_time(struct timeval *delta)
{
    struct timeval t_now, alarm_tm;
    int res;

    netsnmp_get_monotonic_clock(&t_now);
    res = netsnmp_get_next_alarm_time(&alarm_tm, &t_now);
    if (res)
        NETSNMP_TIMERSUB(&alarm_tm, &t_now, delta);
    return res;
}


void
set_an_alarm(void)
{
    struct timeval  delta;
    int             nextalarm = get_next_alarm_delay_time(&delta);

    /*
     * We don't use signals if they asked us nicely not to.  It's expected
     * they'll check the next alarm time and do their own calling of
     * run_alarms().  
     */

    if (nextalarm && !netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
					NETSNMP_DS_LIB_ALARM_DONT_USE_SIG)) {
#ifndef WIN32
# ifdef HAVE_SETITIMER
        struct itimerval it;

        it.it_value = delta;
        timerclear(&it.it_interval);

        signal(SIGALRM, alarm_handler);
        setitimer(ITIMER_REAL, &it, NULL);
        DEBUGMSGTL(("snmp_alarm", "schedule alarm %d in %ld.%03ld seconds\n",
                    nextalarm, (long) delta.tv_sec, (delta.tv_usec / 1000)));
# else  /* HAVE_SETITIMER */
#  ifdef SIGALRM
        signal(SIGALRM, alarm_handler);
        alarm(delta.tv_sec);
        DEBUGMSGTL(("snmp_alarm",
                    "schedule alarm %d in roughly %ld seconds\n", nextalarm,
                    delta.tv_sec));
#  endif  /* SIGALRM */
# endif  /* HAVE_SETITIMER */
#endif  /* WIN32 */

    } else {
        DEBUGMSGTL(("snmp_alarm", "no alarms found to schedule\n"));
    }
}


/**
 * This function registers function callbacks to occur at a specific time
 * in the future.
 *
 * @param when is an unsigned integer specifying when the callback function
 *             will be called in seconds.
 *
 * @param flags is an unsigned integer that specifies how frequent the callback
 *	function is called in seconds.  Should be SA_REPEAT or 0.  If  
 *	flags  is  set with SA_REPEAT, then the registered callback function
 *	will be called every SA_REPEAT seconds.  If flags is 0 then the 
 *	function will only be called once and then removed from the 
 *	registered alarm list.
 *
 * @param thecallback is a pointer SNMPAlarmCallback which is the callback 
 *	function being stored and registered.
 *
 * @param clientarg is a void pointer used by the callback function.  This 
 *	pointer is assigned to snmp_alarm->clientarg and passed into the
 *	callback function for the client's specific needs.
 *
 * @return Returns a unique unsigned integer(which is also passed as the first 
 *	argument of each callback), which can then be used to remove the
 *	callback from the list at a later point in the future using the
 *	snmp_alarm_unregister() function.  If memory could not be allocated
 *	for the snmp_alarm struct 0 is returned.
 *
 * @see snmp_alarm_unregister
 * @see snmp_alarm_register_hr
 * @see snmp_alarm_unregister_all
 */
unsigned int
snmp_alarm_register(unsigned int when, unsigned int flags,
                    SNMPAlarmCallback * thecallback, void *clientarg)
{
    struct timeval  t;

    if (0 == when) {
        t.tv_sec = 0;
        t.tv_usec = 1;
    } else {
        t.tv_sec = when;
        t.tv_usec = 0;
    }

    return snmp_alarm_register_hr(t, flags, thecallback, clientarg);
}


/**
 * This function offers finer granularity as to when the callback 
 * function is called by making use of t->tv_usec value forming the 
 * "when" aspect of snmp_alarm_register().
 *
 * @param t is a timeval structure used to specify when the callback 
 *	function(alarm) will be called.  Adds the ability to specify
 *	microseconds.  t.tv_sec and t.tv_usec are assigned
 *	to snmp_alarm->tv_sec and snmp_alarm->tv_usec respectively internally.
 *	The snmp_alarm_register function only assigns seconds(it's when 
 *	argument).
 *
 * @param flags is an unsigned integer that specifies how frequent the callback
 *	function is called in seconds.  Should be SA_REPEAT or NULL.  If  
 *	flags  is  set with SA_REPEAT, then the registered callback function
 *	will be called every SA_REPEAT seconds.  If flags is NULL then the 
 *	function will only be called once and then removed from the 
 *	registered alarm list.
 *
 * @param cb is a pointer SNMPAlarmCallback which is the callback 
 *	function being stored and registered.
 *
 * @param cd is a void pointer used by the callback function.  This 
 *	pointer is assigned to snmp_alarm->clientarg and passed into the
 *	callback function for the client's specific needs.
 *
 * @return Returns a unique unsigned integer(which is also passed as the first 
 *	argument of each callback), which can then be used to remove the
 *	callback from the list at a later point in the future using the
 *	snmp_alarm_unregister() function.  If memory could not be allocated
 *	for the snmp_alarm struct 0 is returned.
 *
 * @see snmp_alarm_register
 * @see snmp_alarm_unregister
 * @see snmp_alarm_unregister_all
 */
unsigned int
snmp_alarm_register_hr(struct timeval t, unsigned int flags,
                       SNMPAlarmCallback * cb, void *cd)
{
    struct snmp_alarm **s = NULL;

    for (s = &(thealarms); *s != NULL; s = &((*s)->next));

    *s = SNMP_MALLOC_STRUCT(snmp_alarm);
    if (*s == NULL) {
        return 0;
    }

    (*s)->t = t;
    (*s)->flags = flags;
    (*s)->clientarg = cd;
    (*s)->thecallback = cb;
    (*s)->clientreg = regnum++;
    (*s)->next = NULL;

    sa_update_entry(*s);

    DEBUGMSGTL(("snmp_alarm",
                "registered alarm %d, t = %ld.%03ld, flags=0x%02x\n",
                (*s)->clientreg, (long) (*s)->t.tv_sec, ((*s)->t.tv_usec / 1000),
                (*s)->flags));

    if (start_alarms) {
        set_an_alarm();
    }

    return (*s)->clientreg;
}

/**
 * This function resets an existing alarm.
 *
 * @param clientreg is a unique unsigned integer representing a registered
 *	alarm which the client wants to unregister.
 *
 * @return 0 on success, -1 if the alarm was not found
 *
 * @see snmp_alarm_register
 * @see snmp_alarm_register_hr
 * @see snmp_alarm_unregister
 */
int
snmp_alarm_reset(unsigned int clientreg)
{
    struct snmp_alarm *a;
    struct timeval  t_now;
    if ((a = sa_find_specific(clientreg)) != NULL) {
        netsnmp_get_monotonic_clock(&t_now);
        a->t_lastM.tv_sec = t_now.tv_sec;
        a->t_lastM.tv_usec = t_now.tv_usec;
        a->t_nextM.tv_sec = 0;
        a->t_nextM.tv_usec = 0;
        NETSNMP_TIMERADD(&t_now, &a->t, &a->t_nextM);
        return 0;
    }
    DEBUGMSGTL(("snmp_alarm_reset", "alarm %d not found\n",
                clientreg));
    return -1;
}
/**  @} */
