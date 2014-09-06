#ifndef MTEEVENT_H
#define MTEEVENT_H

#include "disman/event/mteTrigger.h"

    /*
     * Values for mteEventActions field
     */
#define MTE_EVENT_NOTIFICATION 0x80    /* mteEventActions values */
#define MTE_EVENT_SET          0x40

    /*
     * Flags relating to the mteEventTable....
     */
#define MTE_EVENT_FLAG_ENABLED 0x01    /* for mteEventEnabled        */
#define MTE_EVENT_FLAG_ACTIVE  0x02    /* for mteEventEntryStatus    */
#define MTE_EVENT_FLAG_FIXED   0x04    /* for snmpd.conf persistence */
#define MTE_EVENT_FLAG_VALID   0x08    /* for row creation/undo      */

    /*
     * ...and to the mteEventSetTable
     */
#define MTE_SET_FLAG_OBJWILD   0x10    /* for mteEventSetObjectWildcard      */
#define MTE_SET_FLAG_CTXWILD   0x20    /* for mteEventSetContextNameWildcard */


    /*
     * All Event-MIB OCTET STRING objects are either short (32-character)
     *   tags, or SnmpAdminString/similar values (i.e. 255 characters)
     */
#define MTE_STR1_LEN	32
#define MTE_STR2_LEN	255

/*
 * Data structure for a (combined) event row
 * Covers both Notification and Set events
 */
struct mteEvent {
    /*
     * Index values 
     */
    char            mteOwner[MTE_STR1_LEN+1];
    char            mteEName[MTE_STR1_LEN+1];

    /*
     * Column values for the main mteEventTable
     */
    char            mteEventComment[MTE_STR2_LEN+1];
    u_char          mteEventActions;

    /*
     * Column values for Notification events (mteEventNotificationTable)
     */
    oid             mteNotification[MAX_OID_LEN];
    size_t          mteNotification_len;
    char            mteNotifyOwner[  MTE_STR1_LEN+1];
    char            mteNotifyObjects[MTE_STR1_LEN+1];

    /*
     * Column values for Set events  (mteEventSetTable)
     */
    oid             mteSetOID[MAX_OID_LEN];
    size_t          mteSetOID_len;
    long            mteSetValue;
    char            mteSetTarget[ MTE_STR2_LEN+1];
    char            mteSetContext[MTE_STR2_LEN+1];

    netsnmp_session *session;
    long            flags;
};

  /*
   * Container structure for the (combined) mteEvent*Tables,
   * and routine to create this.
   */
extern netsnmp_tdata *event_table_data;
extern void      init_event_table_data(void);

void          init_mteEvent(void);
void               mteEvent_removeEntry(netsnmp_tdata_row *row);
netsnmp_tdata_row *mteEvent_createEntry(const char *mteOwner,
                                        const char *mteEventName, int fixed);
int mteEvent_fire( char *owner, char *event,
                   struct mteTrigger *trigger,
                   oid  *suffix, size_t s_len );

#endif                          /* MTEEVENT_H */
