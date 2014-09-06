/*
 * mt_support.h - multi-thread resource locking support declarations 
 */
/*
 * Author: Markku Laukkanen
 * Created: 6-Sep-1999
 * History:
 *  8-Sep-1999 M. Slifcak method names changed;
 *                        use array of resource locking structures.
 */

#ifndef MT_SUPPORT_H
#define MT_SUPPORT_H

#ifdef __cplusplus
extern          "C" {
#endif
  
/*
 * Lock group identifiers 
 */

#define MT_LIBRARY_ID      0
#define MT_APPLICATION_ID  1
#define MT_TOKEN_ID        2

#define MT_MAX_IDS         3    /* one greater than last from above */
#define MT_MAX_SUBIDS      10


/*
 * Lock resource identifiers for library resources 
 */

#define MT_LIB_NONE        0
#define MT_LIB_SESSION     1
#define MT_LIB_REQUESTID   2
#define MT_LIB_MESSAGEID   3
#define MT_LIB_SESSIONID   4
#define MT_LIB_TRANSID     5

#define MT_LIB_MAXIMUM     6    /* must be one greater than the last one */


#if defined(NETSNMP_REENTRANT) || defined(WIN32)

#if HAVE_PTHREAD_H
#include <pthread.h>
typedef pthread_mutex_t mutex_type;
#ifdef pthread_mutexattr_default
#define MT_MUTEX_INIT_DEFAULT pthread_mutexattr_default
#else
#define MT_MUTEX_INIT_DEFAULT 0
#endif

#elif defined(WIN32) || defined(cygwin)

#include <windows.h>
typedef CRITICAL_SECTION mutex_type;

#else  /*  HAVE_PTHREAD_H  */
error "There is no re-entrant support as defined."
#endif /*  HAVE_PTHREAD_H  */


NETSNMP_IMPORT
int             snmp_res_init(void);
NETSNMP_IMPORT
int             snmp_res_lock(int groupID, int resourceID);
NETSNMP_IMPORT
int             snmp_res_unlock(int groupID, int resourceID);
NETSNMP_IMPORT
int             snmp_res_destroy_mutex(int groupID, int resourceID);

#else /*  NETSNMP_REENTRANT  */

#ifndef WIN32
#define snmp_res_init() do {} while (0)
#define snmp_res_lock(x,y) do {} while (0)
#define snmp_res_unlock(x,y) do {} while (0)
#define snmp_res_destroy_mutex(x,y) do {} while (0)
#endif /*  WIN32  */

#endif /*  NETSNMP_REENTRANT  */

#ifdef __cplusplus
}
#endif
#endif /*  MT_SUPPORT_H  */
