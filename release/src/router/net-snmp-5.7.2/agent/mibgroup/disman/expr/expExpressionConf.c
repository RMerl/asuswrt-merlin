/*
 * DisMan Expression MIB:
 *    Implementation of the expression table configuration handling.
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <ctype.h>

#include "utilities/iquery.h"
#include "disman/expr/expExpression.h"
#include "disman/expr/expObject.h"
#include "disman/expr/expExpressionConf.h"

netsnmp_feature_require(iquery)

/* Initializes the expExpressionConf module */
void
init_expExpressionConf(void)
{
    init_expr_table_data();

    /*
     * Register config handler for user-level (fixed) expressions...
     *    XXX - TODO
     */
    snmpd_register_config_handler("expression", parse_expression, NULL, NULL);

    /*
     * ... and persistent storage of dynamically configured entries.
     */
    snmpd_register_config_handler("_expETable", parse_expETable, NULL, NULL);

    /*
     * Register to save (non-fixed) entries when the agent shuts down
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_expETable, NULL);
}



/* ================================================
 *
 *  Handlers for loading/storing persistent expression entries
 *
 * ================================================ */

char *
_parse_expECols( char *line, struct expExpression *entry )
{
    void  *vp;
    size_t tmp;
    size_t len;

    len  = EXP_STR3_LEN; vp = entry->expExpression;
    line = read_config_read_data(ASN_OCTET_STR, line,  &vp, &len);

    line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
    entry->expValueType = tmp;

    len  = EXP_STR2_LEN; vp = entry->expComment;
    line = read_config_read_data(ASN_OCTET_STR, line,  &vp, &len);

    line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
    entry->expDeltaInterval = tmp;

    vp   = entry->expPrefix;
    entry->expPrefix_len = MAX_OID_LEN;
    line = read_config_read_data(ASN_OBJECT_ID, line, &vp,
                                &entry->expPrefix_len);

    line = read_config_read_data(ASN_UNSIGNED, line,  &tmp, NULL);
    entry->flags |= (tmp & EXP_FLAG_ACTIVE);

    return line;
}


void
parse_expression(const char *token, char *line)
{
    char   buf[ SPRINT_MAX_LEN];
    char   ename[EXP_STR1_LEN+1];
    oid    name_buf[MAX_OID_LEN];
    size_t name_len;
    char *cp, *cp2;
    struct expExpression *entry;
    struct expObject     *object;
    netsnmp_session *sess = NULL;
    int    type=EXPVALTYPE_COUNTER;
    int    i=1;

    DEBUGMSGTL(("disman:expr:conf", "Parsing expression config...  "));

    memset(buf,   0, sizeof(buf));
    memset(ename, 0, sizeof(ename));

    for (cp = copy_nword(line, buf, SPRINT_MAX_LEN);
         ;
         cp = copy_nword(cp,   buf, SPRINT_MAX_LEN)) {

        if (buf[0] == '-' ) {
            switch (buf[1]) {
            case 't':   /*  type */
                switch (buf[2]) {
                case 'c':   type = EXPVALTYPE_COUNTER;   break;
                case 'u':   type = EXPVALTYPE_UNSIGNED;  break;
                case 't':   type = EXPVALTYPE_TIMETICKS; break;
                case 'i':   type = EXPVALTYPE_INTEGER;   break;
                case 'a':   type = EXPVALTYPE_IPADDRESS; break;
                case 's':   type = EXPVALTYPE_STRING;    break;
                case 'o':   type = EXPVALTYPE_OID;       break;
                case 'C':   type = EXPVALTYPE_COUNTER64; break;
                }
                break;
            case 'u':   /*  user */
                cp     = copy_nword(cp, buf, SPRINT_MAX_LEN);
                sess   = netsnmp_iquery_user_session(buf);
                break;
            }
        } else {
            break;
        }
    }

    memcpy(ename, buf, sizeof(ename));
 /* cp    = copy_nword(line, ename, sizeof(ename)); */
    entry = expExpression_createEntry( "snmpd.conf", ename, 1 );
    if (!entry)
        return;

    cp2 = entry->expExpression;
    while (cp && *cp) {
        /*
         * Copy everything that can't possibly be a MIB
         * object name into the expression field...
         */
        /*   XXX - TODO - Handle string literals */
        if (!isalpha(*cp)) {
           *cp2++ = *cp++;
           continue;
        }
        /*
         * ... and copy the defined functions across as well
         *   XXX - TODO
         */

        /*
         * Anything else is presumably a MIB object (or instance).
         * Create an entry in the expObjectTable, and insert a
         *   corresponding parameter in the expression itself.
         */
        name_len = MAX_OID_LEN;
        cp = copy_nword(cp, buf, SPRINT_MAX_LEN);
        snmp_parse_oid( buf, name_buf, &name_len );
        object = expObject_createEntry( "snmpd.conf", ename, i, 1 );
        memcpy( object->expObjectID, name_buf, name_len*sizeof(oid));
        object->expObjectID_len = name_len;
        object->flags |= EXP_OBJ_FLAG_VALID
                      |  EXP_OBJ_FLAG_ACTIVE
                      |  EXP_OBJ_FLAG_OWILD;
        /*
         * The first such object can also be used as the
         * expExpressionPrefix
         */
        if ( i == 1 ) {
            memcpy( entry->expPrefix, name_buf, name_len*sizeof(oid));
            entry->expPrefix_len = name_len;
            object->flags |= EXP_OBJ_FLAG_PREFIX;
        }
        sprintf(cp2, "$%d", i++);
        while (*cp2)
            cp2++;  /* Skip over this parameter */
    }

    if (sess)
        entry->session      = sess;
    else
        entry->session      = netsnmp_query_get_default_session();
    entry->expDeltaInterval = 10;
    entry->expValueType     = type;
    entry->flags |= EXP_FLAG_VALID
                 |  EXP_FLAG_ACTIVE;
    expExpression_enable( entry );
    DEBUGMSG(("disman:expr:conf", "(%s, %s)\n", ename,
                                                entry->expExpression));
}

void
parse_expETable(const char *token, char *line)
{
    char   owner[EXP_STR1_LEN+1];
    char   ename[EXP_STR1_LEN+1];
    void  *vp;
    size_t len;
    struct expExpression *entry;

    DEBUGMSGTL(("disman:expr:conf", "Parsing mteExpressionTable config...  "));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( ename, 0, sizeof(ename));
    len   = EXP_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = EXP_STR1_LEN; vp = ename;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    entry = expExpression_createEntry( owner, ename, 0 );

    DEBUGMSG(("disman:expr:conf", "(%s, %s) ", owner, ename));
    
    /*
     * Read in the accessible column values.
     */
    line = _parse_expECols( line, entry );
    /*
     * XXX - Will need to read in the 'iquery' access information
     */
    entry->flags |= EXP_FLAG_VALID;

    DEBUGMSG(("disman:expr:conf", "\n"));
}


int
store_expETable(int majorID, int minorID, void *serverarg, void *clientarg)
{
    char                  line[SNMP_MAXBUF];
    char                 *cptr, *cp;
    void                 *vp;
    size_t                tint;
    netsnmp_tdata_row    *row;
    struct expExpression *entry;


    DEBUGMSGTL(("disman:expr:conf", "Storing expExpressionTable config:\n"));

    for (row = netsnmp_tdata_row_first( expr_table_data );
         row;
         row = netsnmp_tdata_row_next( expr_table_data, row )) {

        /*
         * Skip entries that were set up via static config directives
         */
        entry = (struct expExpression *)netsnmp_tdata_row_entry( row );
        if ( entry->flags & EXP_FLAG_FIXED )
            continue;

        DEBUGMSGTL(("disman:expr:conf", "  Storing (%s %s)\n",
                         entry->expOwner, entry->expName));

        /*
         * Save the basic expExpression entry
         */
        memset(line, 0, sizeof(line));
        strcat(line, "_expETable ");
        cptr = line + strlen(line);

        cp   = entry->expOwner;          tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        cp   = entry->expName;           tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );

        cp   = entry->expExpression;     tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        tint = entry->expValueType;
        cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );
        cp   = entry->expComment;        tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        tint = entry->expDeltaInterval;
        cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );

        vp   = entry->expPrefix;
        tint = entry->expPrefix_len;
        cptr = read_config_store_data(   ASN_OBJECT_ID, cptr, &vp,  &tint );

        tint = entry->flags;
        cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );

        /* XXX - Need to store the 'iquery' access information */
        snmpd_store_config(line);
    }

    DEBUGMSGTL(("disman:expr:conf", "  done.\n"));
    return SNMPERR_SUCCESS;
}
