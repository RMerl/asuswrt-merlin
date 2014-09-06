#ifndef MTETRIGGER_H
#define MTETRIGGER_H

extern oid    _sysUpTime_instance[];
extern size_t _sysUpTime_inst_len;
    /*
     * Flags relating to the mteTriggerTable and related tables
     */
#define MTE_TRIGGER_FLAG_DELTA   0x01    /* for mteTriggerSampleType       */
#define MTE_TRIGGER_FLAG_VWILD   0x02    /* for mteTriggerValueIDWildcard  */
#define MTE_TRIGGER_FLAG_CWILD   0x04    /* for mteTriggerContextWildcard  */
#define MTE_TRIGGER_FLAG_DWILD   0x08    /* for mteTriggerDeltaDIDWildcard */
#define MTE_TRIGGER_FLAG_SYSUPT  0x10    /* using default mteTriggerDeltaID */

#define MTE_TRIGGER_FLAG_BSTART  0x20    /* for mteTriggerBooleanStartup   */

#define MTE_TRIGGER_FLAG_ENABLED 0x0100  /* for mteTriggerEnabled          */
#define MTE_TRIGGER_FLAG_ACTIVE  0x0200  /* for mteTriggerEntryStatus      */
#define MTE_TRIGGER_FLAG_FIXED   0x0400  /* for snmpd.conf persistence     */
#define MTE_TRIGGER_FLAG_VALID   0x0800  /* for row creation/undo          */


    /*
     * Values for the mteTriggerTest field
     */
#define MTE_TRIGGER_EXISTENCE  0x80    /* mteTriggerTest values */
#define MTE_TRIGGER_BOOLEAN    0x40
#define MTE_TRIGGER_THRESHOLD  0x20

    /*
     * Values for the mteTriggerSampleType field
     */
#define MTE_SAMPLE_ABSOLUTE       1    /* mteTriggerSampleType values */
#define MTE_SAMPLE_DELTA          2

    /*
     * Values for the mteTriggerDeltaDiscontinuityIDType field
     */
#define MTE_DELTAD_TTICKS         1
#define MTE_DELTAD_TSTAMP         2
#define MTE_DELTAD_DATETIME       3

    /*
     * Values for the mteTriggerExistenceTest 
     *   and mteTriggerExistenceStartup fields
     */
#define MTE_EXIST_PRESENT      0x80
#define MTE_EXIST_ABSENT       0x40
#define MTE_EXIST_CHANGED      0x20

    /*
     * Values for the mteTriggerBooleanComparison field
     */
#define MTE_BOOL_UNEQUAL          1
#define MTE_BOOL_EQUAL            2
#define MTE_BOOL_LESS             3
#define MTE_BOOL_LESSEQUAL        4
#define MTE_BOOL_GREATER          5
#define MTE_BOOL_GREATEREQUAL     6

    /*
     * Values for the mteTriggerThresholdStartup field
     */
#define MTE_THRESH_START_RISE     1
#define MTE_THRESH_START_FALL     2
#define MTE_THRESH_START_RISEFALL 3
        /* Note that RISE and FALL values can be used for bit-wise
           tests as well, since RISEFALL = RISE | FALL */


    /*
     * Flags to indicate which triggers are armed, and ready to fire.
     */
#define MTE_ARMED_TH_RISE       0x01
#define MTE_ARMED_TH_FALL       0x02
#define MTE_ARMED_TH_DRISE      0x04
#define MTE_ARMED_TH_DFALL      0x08
#define MTE_ARMED_BOOLEAN       0x10
#define MTE_ARMED_ALL           0x1f

    /*
     * All Event-MIB OCTET STRING objects are either short (32-character)
     *   tags, or SnmpAdminString/similar values (i.e. 255 characters)
     */
#define MTE_STR1_LEN	32
#define MTE_STR2_LEN	255

/*
 * Data structure for a (combined) trigger row.  Covers delta samples,
 *   and all types (Existence, Boolean and Threshold) of trigger.
 */
struct mteTrigger {
    /*
     * Index values 
     */
    char            mteOwner[MTE_STR1_LEN+1];
    char            mteTName[MTE_STR1_LEN+1];

    /*
     * Column values for the main mteTriggerTable
     */
    char            mteTriggerComment[MTE_STR2_LEN+1];
    char            mteTriggerTest;
    oid             mteTriggerValueID[MAX_OID_LEN];
    size_t          mteTriggerValueID_len;
    char            mteTriggerTarget[ MTE_STR2_LEN+1];
    char            mteTriggerContext[MTE_STR2_LEN+1];
    u_long          mteTriggerFrequency;
    char            mteTriggerOOwner[ MTE_STR1_LEN+1];
    char            mteTriggerObjects[MTE_STR1_LEN+1];

    netsnmp_session *session;
    long            flags;

    /*
     * Column values for the mteTriggerDeltaTable
     */
    oid             mteDeltaDiscontID[MAX_OID_LEN];
    size_t          mteDeltaDiscontID_len;
    long            mteDeltaDiscontIDType;

    /*
     * Column values for Existence tests (mteTriggerExistenceTable)
     */
    u_char          mteTExTest;
    u_char          mteTExStartup;
    char            mteTExObjOwner[MTE_STR1_LEN+1];
    char            mteTExObjects[ MTE_STR1_LEN+1];
    char            mteTExEvOwner[ MTE_STR1_LEN+1];
    char            mteTExEvent[   MTE_STR1_LEN+1];

    /*
     * Column values for Boolean tests (mteTriggerBooleanTable)
     */
    long            mteTBoolComparison;
    long            mteTBoolValue;
    char            mteTBoolObjOwner[MTE_STR1_LEN+1];
    char            mteTBoolObjects[ MTE_STR1_LEN+1];
    char            mteTBoolEvOwner[ MTE_STR1_LEN+1];
    char            mteTBoolEvent[   MTE_STR1_LEN+1];

    /*
     * Column values for Threshold tests (mteTriggerThresholdTable)
     */
    long            mteTThStartup;
    long            mteTThRiseValue;
    long            mteTThFallValue;
    long            mteTThDRiseValue;
    long            mteTThDFallValue;
    char            mteTThObjOwner[  MTE_STR1_LEN+1];
    char            mteTThObjects[   MTE_STR1_LEN+1];
    char            mteTThRiseOwner[ MTE_STR1_LEN+1];
    char            mteTThRiseEvent[ MTE_STR1_LEN+1];
    char            mteTThFallOwner[ MTE_STR1_LEN+1];
    char            mteTThFallEvent[ MTE_STR1_LEN+1];
    char            mteTThDRiseOwner[MTE_STR1_LEN+1];
    char            mteTThDRiseEvent[MTE_STR1_LEN+1];
    char            mteTThDFallOwner[MTE_STR1_LEN+1];
    char            mteTThDFallEvent[MTE_STR1_LEN+1];

    /*
     *  Additional fields for operation of the Trigger tables:
     *     monitoring...
     */
    unsigned int    alarm;
    long            sysUpTime;
    netsnmp_variable_list *old_results;
    netsnmp_variable_list *old_deltaDs;

    /*
     *  ... stats...
     */
    long            count;

    /*
     *  ... and firing.
     */
    char           *mteTriggerXOwner;
    char           *mteTriggerXObjects;
    netsnmp_variable_list *mteTriggerFired;
};

  /*
   * Container structure for the (combined) mteTrigger*Tables,
   * and routine to create this.
   */
extern netsnmp_tdata *trigger_table_data;
extern void      init_trigger_table_data(void);

void          init_mteTrigger(void);
void               mteTrigger_removeEntry(netsnmp_tdata_row *row);
netsnmp_tdata_row *mteTrigger_createEntry(const char *mteOwner,
                                          char *mteTriggerName, int fixed);
void               mteTrigger_enable(    struct mteTrigger *entry );
void               mteTrigger_disable(   struct mteTrigger *entry );

long mteTrigger_getNumEntries(int max);

#endif                          /* MTETRIGGER_H */
