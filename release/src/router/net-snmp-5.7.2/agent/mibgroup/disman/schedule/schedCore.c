/*
 * DisMan Schedule MIB:
 *     Core implementation of the schedule handling behaviour
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/schedule/schedCore.h"
#include "utilities/iquery.h"

netsnmp_feature_require(iquery)

netsnmp_feature_child_of(sched_nextrowtime, netsnmp_unused)

netsnmp_tdata *schedule_table;


#if !defined(HAVE_LOCALTIME_R) && !defined(localtime_r)
/*
 * localtime_r() replacement for older MinGW versions.
 * Note: this implementation is not thread-safe, while it should.
 */
struct tm      *
localtime_r(const time_t * timer, struct tm *result)
{
    struct tm      *result_p;

    result_p = localtime(timer);
    if (result && result_p)
        *result = *result_p;
    return result_p;
}
#endif


    /*
     * Initialize the container for the schedule table,
     * regardless of which initialisation routine is called first.
     */
void
init_schedule_container(void)
{
    DEBUGMSGTL(("disman:schedule:init", "init schedule container\n"));
    if (!schedule_table) {
        schedule_table = netsnmp_tdata_create_table("schedTable", 0);
        DEBUGMSGTL(("disman:schedule:init",
                        "create schedule container(%p)\n", schedule_table));
    }
}

/** Initializes the schedCore module */
void
init_schedCore(void)
{
    /*
     * Create a table structure for the schedule table
     * This will typically be registered by the schedTable module
     */
    DEBUGMSGTL(("disman:schedule:init", "Initializing core module\n"));
    init_schedule_container();
}


/*
 * Callback to invoke a scheduled action
 */
static void
_sched_callback( unsigned int reg, void *magic )
{
    struct schedTable_entry *entry = (struct schedTable_entry *)magic;
    int ret;
    netsnmp_variable_list assign;

    if ( !entry ) {
        DEBUGMSGTL(("disman:schedule:callback", "missing entry\n"));
        return;
    }
    entry->schedLastRun = time(NULL);
    entry->schedTriggers++;

    DEBUGMSGTL(( "disman:schedule:callback", "assignment "));
    DEBUGMSGOID(("disman:schedule:callback", entry->schedVariable,
                                             entry->schedVariable_len));
    DEBUGMSG((   "disman:schedule:callback", " = %ld\n", entry->schedValue));

    memset(&assign, 0, sizeof(netsnmp_variable_list));
    snmp_set_var_objid(&assign, entry->schedVariable, entry->schedVariable_len);
    snmp_set_var_typed_value(&assign, ASN_INTEGER,
                                     (u_char *)&entry->schedValue,
                                         sizeof(entry->schedValue));

    ret = netsnmp_query_set( &assign, entry->session );
    if ( ret != SNMP_ERR_NOERROR ) {
        DEBUGMSGTL(( "disman:schedule:callback",
                     "assignment failed (%d)\n", ret));
        entry->schedFailures++;
        entry->schedLastFailure = ret;
        time ( &entry->schedLastFailed );
    }

    sched_nextTime( entry );
}


    /*
     * Internal utility routines to help interpret
     *  calendar-based schedule bit strings
     */
static u_char _masks[] = { /* 0xff, */ 0x7f, 0x3f, 0x1f,
                           0x0f, 0x07, 0x03, 0x01, 0x00 };
static u_char _bits[]  = { 0x80, 0x40, 0x20, 0x10,
                           0x08, 0x04, 0x02, 0x01 };

/*
 * Are any of the bits set?
 */
static int
_bit_allClear( char *pattern, int len ) {
    int i;

    for (i=0; i<len; i++) {
        if ( pattern[i] != 0 )
            return 0;    /* At least one bit set */
    }
    return 1;  /* All bits clear */ 
}

/*
 * Is a particular bit set?
 */
static int
_bit_set( char *pattern, int bit ) {
    int major, minor;

    major = bit/8;
    minor = bit%8;
    if ( pattern[major] & _bits[minor] ) {
        return 1; /* Specified bit is set */
    }
    return 0;     /* Bit not set */
}

/*
 * What is the next bit set?
 *   (after a specified point)
 */
static int
_bit_next( char *pattern, int current, size_t len ) {
    char buf[ 8 ];
    int major, minor, i, j;

        /* Make a working copy of the bit pattern */
    memset( buf, 0, 8 );
    memcpy( buf, pattern, len );

        /*
         * If we're looking for the first bit after some point,
         * then clear all earlier bits from the working copy.
         */
    if ( current > -1 ) {
        major = current/8;
        minor = current%8;
        for ( i=0; i<major; i++ )
            buf[i]=0;
        buf[major] &= _masks[minor];
    }

        /*
         * Look for the first bit that's set
         */
    for ( i=0; i<(int)len; i++ ) {
        if ( buf[i] != 0 ) {
            major = i*8;
            for ( j=0; j<8; j++ ) {
                if ( buf[i] & _bits[j] ) {
                    return major+j;
                }
            }
        }
    }
    return -1;     /* No next bit */
}


static int _daysPerMonth[] = { 31, 28, 31, 30,
                               31, 30, 31, 31,
                               30, 31, 30, 31, 29 };

static u_char _truncate[] = { 0xfe, 0xf0, 0xfe, 0xfc,
                              0xfe, 0xfc, 0xfe, 0xfe,
                              0xfc, 0xfe, 0xfc, 0xfe, 0xf8 };

/*
 * What is the next day with a relevant bit set?
 *
 * Merge the forward and reverse day bits into a single
 *   pattern relevant for this particular month,
 *   and apply the standard _bit_next() call.
 * Then check this result against the day of the week bits.
 */
static int
_bit_next_day( char *day_pattern, char weekday_pattern,
               int day, int month, int year ) {
    char buf[4];
    union {
        char buf2[4];
        int int_val;
    } rev;
    int next_day, i;
    struct tm tm_val;

        /* Make a working copy of the forward day bits ... */
    memset( buf,  0, 4 );
    memcpy( buf,  day_pattern, 4 );

        /* ... and another (right-aligned) of the reverse day bits */
    memset( rev.buf2, 0, 4 );
    memcpy( rev.buf2, day_pattern+4, 4 );
    rev.int_val >>= 2;
    if ( buf[3] & 0x01 )
        rev.buf2[0] |= 0x40;
    if ( month == 3 || month == 5 ||
         month == 8 || month == 10 )
        rev.int_val >>= 1;  /* April, June, September, November */
    if ( month == 1 )
        rev.int_val >>= 3;  /* February */
    if ( month == 12 )
        rev.int_val >>= 2;  /* February (leap year) */

        /* Combine the two bit patterns, and truncate to the month end */
    for ( i=0; i<4; i++ ) {
        if ( rev.buf2[i] & 0x80 ) buf[3-i] |= 0x01;
        if ( rev.buf2[i] & 0x40 ) buf[3-i] |= 0x02;
        if ( rev.buf2[i] & 0x20 ) buf[3-i] |= 0x04;
        if ( rev.buf2[i] & 0x10 ) buf[3-i] |= 0x08;
        if ( rev.buf2[i] & 0x08 ) buf[3-i] |= 0x10;
        if ( rev.buf2[i] & 0x04 ) buf[3-i] |= 0x20;
        if ( rev.buf2[i] & 0x02 ) buf[3-i] |= 0x40;
        if ( rev.buf2[i] & 0x01 ) buf[3-i] |= 0x80;
    }

    buf[3] &= _truncate[ month ];

    next_day = day-1;  /* tm_day is 1-based, not 0-based */
    do {
        next_day = _bit_next( buf, next_day, 4 );
        if ( next_day < 0 )
            return -1;

            /*
             * Calculate the day of the week, and
             * check this against the weekday pattern
             */
        memset( &tm_val,  0, sizeof(struct tm));
        tm_val.tm_mday = next_day+1;
        tm_val.tm_mon  = month;
        tm_val.tm_year = year;
        mktime( &tm_val );
    } while ( !_bit_set( &weekday_pattern, tm_val.tm_wday ));
    return next_day+1; /* Convert back to 1-based list */
}


/*
 * determine the time for the next scheduled action of a given entry
 */
void
sched_nextTime( struct schedTable_entry *entry )
{
    time_t now;
    struct tm now_tm, next_tm;
    int rev_day, mon;

    time( &now );

    if ( !entry ) {
        DEBUGMSGTL(("disman:schedule:time", "missing entry\n"));
        return;
    }

    if ( entry->schedCallbackID )
        snmp_alarm_unregister( entry->schedCallbackID );

    if (!(entry->flags & SCHEDULE_FLAG_ENABLED) ||
        !(entry->flags & SCHEDULE_FLAG_ACTIVE))  {
        DEBUGMSGTL(("disman:schedule:time", "inactive entry\n"));
        return;
    }

    switch ( entry->schedType ) {
    case SCHED_TYPE_PERIODIC:
        if ( !entry->schedInterval ) {
            DEBUGMSGTL(("disman:schedule:time", "periodic: no interval\n"));
            return;
        }
        if ( entry->schedLastRun ) {
             entry->schedNextRun = entry->schedLastRun +
                                   entry->schedInterval;
        } else {
             entry->schedNextRun = now + entry->schedInterval;
        }
        DEBUGMSGTL(("disman:schedule:time", "periodic: (%ld) %s",
                    (long) entry->schedNextRun, ctime(&entry->schedNextRun)));
        break;

    case SCHED_TYPE_ONESHOT:
        if ( entry->schedLastRun ) {
            DEBUGMSGTL(("disman:schedule:time", "one-shot: expired (%ld) %s",
                        (long) entry->schedNextRun,
                        ctime(&entry->schedNextRun)));
            return;
        }
        /* Fallthrough */
        DEBUGMSGTL(("disman:schedule:time", "one-shot: fallthrough\n"));
    case SCHED_TYPE_CALENDAR:
        /*
         *  Check for complete time specification
         *  If any of the five fields have no bits set,
         *    the entry can't possibly match any time.
         */
        if ( _bit_allClear( entry->schedMinute, 8 ) ||
             _bit_allClear( entry->schedHour,   3 ) ||
             _bit_allClear( entry->schedDay,  4+4 ) ||
             _bit_allClear( entry->schedMonth,  2 ) ||
             _bit_allClear(&entry->schedWeekDay, 1 )) {
            DEBUGMSGTL(("disman:schedule:time", "calendar: incomplete spec\n"));
            return;
        }

        /*
         *  Calculate the next run time:
         *
         *  If the current Month, Day & Hour bits are set
         *    calculate the next specified minute
         *  If this fails (or the current Hour bit is not set)
         *    use the first specified minute,
         *    and calculate the next specified hour
         *  If this fails (or the current Day bit is not set)
         *    use the first specified minute and hour
         *    and calculate the next specified day (in this month)
         *  If this fails (or the current Month bit is not set)
         *    use the first specified minute and hour
         *    calculate the next specified month, and
         *    the first specified day (in that month)
         */

        (void) localtime_r( &now, &now_tm );
        (void) localtime_r( &now, &next_tm );

        next_tm.tm_mon=-1;
        next_tm.tm_mday=-1;
        next_tm.tm_hour=-1;
        next_tm.tm_min=-1;
        next_tm.tm_sec=0;
        if ( _bit_set( entry->schedMonth, now_tm.tm_mon )) {
            next_tm.tm_mon = now_tm.tm_mon;
            rev_day = _daysPerMonth[ now_tm.tm_mon ] - now_tm.tm_mday;
            if ( _bit_set( &entry->schedWeekDay, now_tm.tm_wday ) &&
                (_bit_set( entry->schedDay, now_tm.tm_mday-1 ) ||
                 _bit_set( entry->schedDay, 31+rev_day ))) {
                next_tm.tm_mday = now_tm.tm_mday;

                if ( _bit_set( entry->schedHour, now_tm.tm_hour )) {
                    next_tm.tm_hour = now_tm.tm_hour;
                    /* XXX - Check Fall timechange */
                    next_tm.tm_min = _bit_next( entry->schedMinute,
                                                  now_tm.tm_min, 8 );
                } else {
                    next_tm.tm_min = -1;
                }
   
                if ( next_tm.tm_min == -1 ) {
                    next_tm.tm_min  = _bit_next( entry->schedMinute, -1, 8 );
                    next_tm.tm_hour = _bit_next( entry->schedHour,
                                                   now_tm.tm_hour, 3 );
                }
            } else {
                next_tm.tm_hour = -1;
            }

            if ( next_tm.tm_hour == -1 ) {
                next_tm.tm_min  = _bit_next( entry->schedMinute, -1, 8 );
                next_tm.tm_hour = _bit_next( entry->schedHour,   -1, 3 );
                    /* Handle leap years */
                mon = now_tm.tm_mon;
                if ( mon == 1 && (now_tm.tm_year%4 == 0) )
                    mon = 12;
                next_tm.tm_mday = _bit_next_day( entry->schedDay,
                                                 entry->schedWeekDay,
                                                 now_tm.tm_mday,
                                                 mon, now_tm.tm_year );
            }
        } else {
            next_tm.tm_min  = _bit_next( entry->schedMinute, -1, 2 );
            next_tm.tm_hour = _bit_next( entry->schedHour,   -1, 3 );
            next_tm.tm_mday = -1;
            next_tm.tm_mon  = now_tm.tm_mon;
        }

        while ( next_tm.tm_mday == -1 ) {
            next_tm.tm_mon  = _bit_next( entry->schedMonth,
                                          next_tm.tm_mon, 2 );
            if ( next_tm.tm_mon == -1 ) {
                next_tm.tm_year++;
                next_tm.tm_mon  = _bit_next( entry->schedMonth,
                                             -1, 2 );
            }
                /* Handle leap years */
            mon = next_tm.tm_mon;
            if ( mon == 1 && (next_tm.tm_year%4 == 0) )
                mon = 12;
            next_tm.tm_mday = _bit_next_day( entry->schedDay,
                                             entry->schedWeekDay,
                                             -1, mon, next_tm.tm_year );
            /* XXX - catch infinite loop */
        }

        /* XXX - Check for Spring timechange */

        /*
         * 'next_tm' now contains the time for the next scheduled run
         */
        entry->schedNextRun = mktime( &next_tm );
        DEBUGMSGTL(("disman:schedule:time", "calendar: (%ld) %s",
                    (long) entry->schedNextRun, ctime(&entry->schedNextRun)));
        return;

    default:
        DEBUGMSGTL(("disman:schedule:time", "unknown type (%ld)\n",
                                             entry->schedType));
        return;
    }
    entry->schedCallbackID = snmp_alarm_register(
                                entry->schedNextRun - now,
                                0, _sched_callback, entry );
    return;
}

#ifndef NETSNMP_FEATURE_REMOVE_SCHED_NEXTROWTIME
void
sched_nextRowTime( netsnmp_tdata_row *row )
{
    sched_nextTime((struct schedTable_entry *) row->data );
}
#endif /* NETSNMP_FEATURE_REMOVE_SCHED_NEXTROWTIME */

/*
 * create a new row in the table 
 */
netsnmp_tdata_row *
schedTable_createEntry(const char *schedOwner, const char *schedName)
{
    struct schedTable_entry *entry;
    netsnmp_tdata_row *row;

    DEBUGMSGTL(("disman:schedule:entry", "creating entry (%s, %s)\n",
                                          schedOwner, schedName));
    entry = SNMP_MALLOC_TYPEDEF(struct schedTable_entry);
    if (!entry)
        return NULL;

    row = netsnmp_tdata_create_row();
    if (!row) {
        SNMP_FREE(entry);
        return NULL;
    }
    row->data = entry;
    /*
     * Set the indexing for this entry, both in the row
     *  data structure, and in the table_data helper.
     */
    if (schedOwner) {
        memcpy(entry->schedOwner, schedOwner, strlen(schedOwner));
        netsnmp_tdata_row_add_index(row, ASN_OCTET_STR,
                           entry->schedOwner, strlen(schedOwner));
    }
    else
        netsnmp_tdata_row_add_index(row, ASN_OCTET_STR, "", 0 );

    memcpy(    entry->schedName,  schedName,  strlen(schedName));
    netsnmp_tdata_row_add_index(row, ASN_OCTET_STR,
                           entry->schedName,  strlen(schedName));
    /*
     * Set the (non-zero) default values in the row data structure.
     */
    entry->schedType         = SCHED_TYPE_PERIODIC;
    entry->schedVariable_len = 2;   /* .0.0 */

    netsnmp_tdata_add_row(schedule_table, row);
    return row;
}


/*
 * remove a row from the table 
 */
void
schedTable_removeEntry(netsnmp_tdata_row *row)
{
    struct schedTable_entry *entry;

    if (!row || !row->data) {
        DEBUGMSGTL(("disman:schedule:entry", "remove: missing entry\n"));
        return;                 /* Nothing to remove */
    }
    entry = (struct schedTable_entry *)
        netsnmp_tdata_remove_and_delete_row(schedule_table, row);
    if (entry) {
        DEBUGMSGTL(("disman:schedule:entry", "remove entry (%s, %s)\n",
                                     entry->schedOwner, entry->schedName));
        SNMP_FREE(entry);
    }
}
