/*
 * DisMan Event MIB:
 *     Implementation of the object table configure handling
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/agent_callbacks.h>
#include "disman/event/mteObjects.h"
#include "disman/event/mteObjectsConf.h"


/** Initializes the mteObjectsConf module */
void
init_mteObjectsConf(void)
{
    init_objects_table_data();

    /*
     * Register config handlers for current and previous style
     *   persistent configuration directives
     */
    snmpd_register_config_handler("_mteOTable",
                                   parse_mteOTable, NULL, NULL);
    snmpd_register_config_handler("mteObjectsTable",
                                   parse_mteOTable, NULL, NULL);
    /*
     * Register to save (non-fixed) entries when the agent shuts down
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_mteOTable, NULL);
    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_PRE_UPDATE_CONFIG,
                           clear_mteOTable, NULL);
}


void
parse_mteOTable(const char *token, char *line)
{
    char   owner[MTE_STR1_LEN+1];
    char   oname[MTE_STR1_LEN+1];
    void  *vp;
    u_long index;
    size_t tmpint;
    size_t len;
    netsnmp_tdata_row *row;
    struct mteObject  *entry;

    DEBUGMSGTL(("disman:event:conf", "Parsing mteObjectTable config...  "));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( oname, 0, sizeof(oname));
    len   = MTE_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,    &len);
    len   = MTE_STR1_LEN; vp = oname;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,    &len);
    line  = read_config_read_data(ASN_UNSIGNED,  line, &index, &len);

    DEBUGMSG(("disman:event:conf", "(%s, %s, %lu) ", owner, oname, index));

    row   = mteObjects_createEntry( owner, oname, index, 0 );
    /* entry = (struct mteObject *)netsnmp_tdata_row_entry( row ); */
    entry = (struct mteObject *)row->data;

    
    /*
     * Read in the accessible column values
     */
    entry->mteObjectID_len = MAX_OID_LEN;
    vp   = entry->mteObjectID;
    line = read_config_read_data(ASN_OBJECT_ID, line, &vp,
                                &entry->mteObjectID_len);

    if (!strcasecmp(token, "mteObjectsTable")) {
        /*
         * The previous Event-MIB implementation saved
         *   these fields as separate (integer) values
         * Accept this (for backwards compatability)
         */
        line = read_config_read_data(ASN_UNSIGNED, line, &tmpint, &len);
        if (tmpint == TV_TRUE)
            entry->flags |= MTE_OBJECT_FLAG_WILD;
        line = read_config_read_data(ASN_UNSIGNED, line, &tmpint, &len);
        if (tmpint == RS_ACTIVE)
            entry->flags |= MTE_OBJECT_FLAG_ACTIVE;
    } else {
        /*
         * This implementation saves the (relevant) flag bits directly
         */
        line = read_config_read_data(ASN_UNSIGNED, line, &tmpint, &len);
        if (tmpint & MTE_OBJECT_FLAG_WILD)
            entry->flags |= MTE_OBJECT_FLAG_WILD;
        if (tmpint & MTE_OBJECT_FLAG_ACTIVE)
            entry->flags |= MTE_OBJECT_FLAG_ACTIVE;
    }

    entry->flags |= MTE_OBJECT_FLAG_VALID;

    DEBUGMSG(("disman:event:conf", "\n"));
}



int
store_mteOTable(int majorID, int minorID, void *serverarg, void *clientarg)
{
    char            line[SNMP_MAXBUF];
    char           *cptr, *cp;
    void           *vp;
    size_t          tint;
    netsnmp_tdata_row *row;
    struct mteObject  *entry;


    DEBUGMSGTL(("disman:event:conf", "Storing mteObjectTable config:\n"));

    for (row = netsnmp_tdata_row_first( objects_table_data );
         row;
         row = netsnmp_tdata_row_next( objects_table_data, row )) {

        /*
         * Skip entries that were set up via static config directives
         */
        entry = (struct mteObject *)netsnmp_tdata_row_entry( row );
        if ( entry->flags & MTE_OBJECT_FLAG_FIXED )
            continue;

        DEBUGMSGTL(("disman:event:conf", "  Storing (%s %s %ld)\n",
                         entry->mteOwner, entry->mteOName, entry->mteOIndex));
        memset(line, 0, sizeof(line));
        strcat(line, "_mteOTable ");
        cptr = line + strlen(line);

        cp = entry->mteOwner; tint = strlen( cp );
        cptr = read_config_store_data(ASN_OCTET_STR, cptr, &cp, &tint );
        cp = entry->mteOName; tint = strlen( cp );
        cptr = read_config_store_data(ASN_OCTET_STR, cptr, &cp, &tint );
        cptr = read_config_store_data(ASN_UNSIGNED,  cptr,
                                      &entry->mteOIndex, NULL);
        vp   = entry->mteObjectID;
        cptr = read_config_store_data(ASN_OBJECT_ID, cptr, &vp,
                                      &entry->mteObjectID_len);
        tint = entry->flags & (MTE_OBJECT_FLAG_WILD|MTE_OBJECT_FLAG_ACTIVE); 
        cptr = read_config_store_data(ASN_UNSIGNED,  cptr, &tint, NULL);
        snmpd_store_config(line);
    }

    DEBUGMSGTL(("disman:event:conf", "  done.\n"));
    return SNMPERR_SUCCESS;
}

int
clear_mteOTable(int majorID, int minorID, void *serverarg, void *clientarg)
{
    netsnmp_tdata_row    *row;
    netsnmp_variable_list owner_var;

    /*
     * We're only interested in entries set up via the config files
     */
    memset( &owner_var, 0, sizeof(netsnmp_variable_list));
    snmp_set_var_typed_value( &owner_var,  ASN_OCTET_STR,
                             "snmpd.conf", strlen("snmpd.conf"));
    while (( row = netsnmp_tdata_row_next_byidx( objects_table_data,
                                                &owner_var ))) {
        /*
         * XXX - check for owner of "snmpd.conf"
         *       and break at the end of these
         */
        netsnmp_tdata_remove_and_delete_row( objects_table_data, row );
    }
    return SNMPERR_SUCCESS;
}
