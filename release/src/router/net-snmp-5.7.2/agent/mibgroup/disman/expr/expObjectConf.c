/*
 * DisMan Expression MIB:
 *    Implementation of the object table configuration handling.
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "utilities/iquery.h"
#include "disman/expr/expObject.h"
#include "disman/expr/expObjectConf.h"

netsnmp_feature_require(iquery)

/* Initializes the expObjectConf module */
void
init_expObjectConf(void)
{
    init_expObject_table_data();

    /*
     * Register config handler for persistent storage
     *     of dynamically configured entries.
     */
    snmpd_register_config_handler("_expOTable", parse_expOTable, NULL, NULL);

    /*
     * Register to save (non-fixed) entries when the agent shuts down
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_expOTable, NULL);
}



/* ================================================
 *
 *  Handlers for loading/storing persistent expression entries
 *
 * ================================================ */

char *
_parse_expOCols( char *line, struct expObject *entry )
{
    void  *vp;
    size_t tmp;

    vp   = entry->expObjectID;
    entry->expObjectID_len = MAX_OID_LEN;
    line = read_config_read_data(ASN_OBJECT_ID, line, &vp,
                                &entry->expObjectID_len);

    line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
    entry->expObjectSampleType = tmp;

    vp   = entry->expObjDeltaD;
    entry->expObjDeltaD_len = MAX_OID_LEN;
    line = read_config_read_data(ASN_OBJECT_ID, line, &vp,
                                &entry->expObjDeltaD_len);

    line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
    entry->expObjDiscontinuityType = tmp;

    vp   = entry->expObjCond;
    entry->expObjCond_len = MAX_OID_LEN;
    line = read_config_read_data(ASN_OBJECT_ID, line, &vp,
                                &entry->expObjCond_len);

    line = read_config_read_data(ASN_UNSIGNED, line,  &tmp, NULL);
    entry->flags |= tmp;

    return line;
}


void
parse_expOTable(const char *token, char *line)
{
    char   owner[EXP_STR1_LEN+1];
    char   oname[EXP_STR1_LEN+1];
    long   idx;
    void  *vp;
    size_t len;
    struct expObject *entry;

    DEBUGMSGTL(("disman:expr:conf", "Parsing mteObjectTable config...  "));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( oname, 0, sizeof(oname));
    len   = EXP_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = EXP_STR1_LEN; vp = oname;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    line  = read_config_read_data(ASN_UNSIGNED,  line, &idx, NULL);
    entry = expObject_createEntry( owner, oname, idx, 0 );

    DEBUGMSG(("disman:expr:conf", "(%s, %s, %ld) ", owner, oname, idx));
    
    /*
     * Read in the accessible column values.
     */
    line = _parse_expOCols( line, entry );
    /*
     * XXX - Will need to read in the 'iquery' access information
     */
    entry->flags |= EXP_OBJ_FLAG_VALID;

    DEBUGMSG(("disman:expr:conf", "\n"));
}


int
store_expOTable(int majorID, int minorID, void *serverarg, void *clientarg)
{
    char                  line[SNMP_MAXBUF];
    char                 *cptr, *cp;
    void                 *vp;
    size_t                tint;
    netsnmp_tdata_row    *row;
    struct expObject *entry;


    DEBUGMSGTL(("disman:expr:conf", "Storing expObjectTable config:\n"));

    for (row = netsnmp_tdata_row_first( expObject_table_data );
         row;
         row = netsnmp_tdata_row_next( expObject_table_data, row )) {

        /*
         * Skip entries that were set up via static config directives
         */
        entry = (struct expObject *)netsnmp_tdata_row_entry( row );
        if ( entry->flags & EXP_OBJ_FLAG_FIXED )
            continue;

        DEBUGMSGTL(("disman:expr:conf", "  Storing (%s %s %lu)\n",
                    entry->expOwner, entry->expName, entry->expObjectIndex));

        /*
         * Save the basic expObject entry, indexes...
         */
        memset(line, 0, sizeof(line));
        strcat(line, "_expOTable ");
        cptr = line + strlen(line);

        cp   = entry->expOwner;          tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        cp   = entry->expName;           tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        tint = entry->expObjectIndex;
        cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );

        /*
         * ... and column values.
         */
        vp   = entry->expObjectID;
        tint = entry->expObjectID_len;
        cptr = read_config_store_data(   ASN_OBJECT_ID, cptr, &vp,  &tint );
        tint = entry->expObjectSampleType;
        cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );

        vp   = entry->expObjDeltaD;
        tint = entry->expObjDeltaD_len;
        cptr = read_config_store_data(   ASN_OBJECT_ID, cptr, &vp,  &tint );
        tint = entry->expObjDiscontinuityType;
        cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );

        vp   = entry->expObjCond;
        tint = entry->expObjCond_len;
        cptr = read_config_store_data(   ASN_OBJECT_ID, cptr, &vp,  &tint );

        tint = entry->flags;
        cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );

        snmpd_store_config(line);
    }

    DEBUGMSGTL(("disman:expr:conf", "  done.\n"));
    return SNMPERR_SUCCESS;
}
