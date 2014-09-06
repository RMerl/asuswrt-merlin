#ifndef SCHEDCORE_H
#define SCHEDCORE_H

config_require(utilities/iquery)

    /*
     * Values for schedType field
     */
#define SCHED_TYPE_PERIODIC   1
#define SCHED_TYPE_CALENDAR   2
#define SCHED_TYPE_ONESHOT    3

    /*
     * Schedule flags
     */
#define SCHEDULE_FLAG_ENABLED    0x01    /* for schedAdminStatus  */
#define SCHEDULE_FLAG_ACTIVE     0x02    /* for schedRowStatus    */
#define SCHEDULE_FLAG_VALID      0x04    /* for row creation/undo */

    /*
     * All Schedule-MIB OCTET STRING objects are either short (32-char)
     *   tags, or SnmpAdminString values (i.e. 255 characters)
     */
#define SCHED_STR1_LEN      32
#define SCHED_STR2_LEN     255

    /*
     * Data structure for a schedTable row entry 
     */
struct schedTable_entry {
    /*
     * Index values 
     */
    char            schedOwner[SCHED_STR1_LEN+1];
    char            schedName[ SCHED_STR1_LEN+1];

    /*
     * Column values - schedule actions
     */
    char            schedDescr[SCHED_STR2_LEN+1];
    u_long          schedInterval;
    char            schedWeekDay;
    char            schedMonth[2];
    char            schedDay[4+4];
    char            schedHour[3];
    char            schedMinute[8];
    char            schedContextName[SCHED_STR1_LEN+1];
    oid             schedVariable[   MAX_OID_LEN   ];
    size_t          schedVariable_len;
    long            schedValue;

    /*
     * Column values - schedule control
     */
    long            schedType;
    u_long          schedFailures;
    long            schedLastFailure;
    time_t          schedLastFailed;
    long            schedStorageType;
    u_long          schedTriggers;

    /*
     * Supporting values
     */
    time_t          schedLastRun;
    time_t          schedNextRun;
    unsigned int    schedCallbackID;
    netsnmp_session *session;
    long            flags;
};

/*
 * function declarations 
 */
extern netsnmp_tdata *schedule_table;
void             init_schedule_container(void);
void             init_schedCore(void);

netsnmp_tdata_row *
      schedTable_createEntry(const char *schedOwner, const char *schedName);
void  schedTable_removeEntry(netsnmp_tdata_row *row);
void  sched_nextTime(        struct schedTable_entry *entry );
void  sched_nextRowTime(     netsnmp_tdata_row *row );

#endif                          /* SCHEDCORE_H */
