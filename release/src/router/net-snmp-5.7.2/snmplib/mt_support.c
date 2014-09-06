/*
 * mt_support.c - multi-thread resource locking support 
 */
/*
 * Author: Markku Laukkanen
 * Created: 6-Sep-1999
 * History:
 *  8-Sep-1999 M. Slifcak method names changed;
 *                        use array of resource locking structures.
 */

#include <net-snmp/net-snmp-config.h>
#include <errno.h>
#include <net-snmp/library/mt_support.h>

#ifdef __cplusplus
extern          "C" {
#endif

#ifdef NETSNMP_REENTRANT

static mutex_type s_res[MT_MAX_IDS][MT_LIB_MAXIMUM];  /* locking structures */

static mutex_type *
_mt_res(int groupID, int resourceID)
{
    if (groupID < 0) {
	return 0;
    }
    if (groupID >= MT_MAX_IDS) {
	return 0;
    }
    if (resourceID < 0) {
	return 0;
    }
    if (resourceID >= MT_LIB_MAXIMUM) {
	return 0;
    }
    return (&s_res[groupID][resourceID]);
}

static int
snmp_res_init_mutex(mutex_type *mutex)
{
    int rc = 0;
#if HAVE_PTHREAD_H
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    rc = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
#elif defined(WIN32)
    InitializeCriticalSection(mutex);
#endif

    return rc;
}

int
snmp_res_init(void)
{
    int ii, jj, rc = 0;
    mutex_type *mutex;

    for (jj = 0; (0 == rc) && (jj < MT_MAX_IDS); jj++) {
	for (ii = 0; (0 == rc) && (ii < MT_LIB_MAXIMUM); ii++) {
	    mutex = _mt_res(jj, ii);
	    if (!mutex) {
		continue;
	    }
	    rc = snmp_res_init_mutex(mutex);
	}
    }

    return rc;
}

int
snmp_res_destroy_mutex(int groupID, int resourceID)
{
    int rc = 0;
    mutex_type *mutex = _mt_res(groupID, resourceID);
    if (!mutex) {
	return EFAULT;
    }

#if HAVE_PTHREAD_H
    rc = pthread_mutex_destroy(mutex);
#elif defined(WIN32)
    DeleteCriticalSection(mutex);
#endif

    return rc;
}

int
snmp_res_lock(int groupID, int resourceID)
{
    int rc = 0;
    mutex_type *mutex = _mt_res(groupID, resourceID);
    
    if (!mutex) {
	return EFAULT;
    }

#if HAVE_PTHREAD_H
    rc = pthread_mutex_lock(mutex);
#elif defined(WIN32)
    EnterCriticalSection(mutex);
#endif

    return rc;
}

int
snmp_res_unlock(int groupID, int resourceID)
{
    int rc = 0;
    mutex_type *mutex = _mt_res(groupID, resourceID);

    if (!mutex) {
	return EFAULT;
    }

#if HAVE_PTHREAD_H
    rc = pthread_mutex_unlock(mutex);
#elif defined(WIN32)
    LeaveCriticalSection(mutex);
#endif

    return rc;
}

#else  /*  NETSNMP_REENTRANT  */
#ifdef WIN32

/*
 * Provide "do nothing" targets for Release (.DLL) builds. 
 */

int
snmp_res_init(void)
{
    return 0;
}

int
snmp_res_lock(int groupID, int resourceID)
{
    return 0;
}

int
snmp_res_unlock(int groupID, int resourceID)
{
    return 0;
}

int
snmp_res_destroy_mutex(int groupID, int resourceID)
{
    return 0;
}
#endif /*  WIN32  */
#endif /*  NETSNMP_REENTRANT  */

#ifdef __cplusplus
}
#endif
