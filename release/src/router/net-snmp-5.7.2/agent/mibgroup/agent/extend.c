
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/watcher.h>
#include <net-snmp/agent/agent_callbacks.h>

#include "agent/extend.h"
#include "utilities/execute.h"
#include "struct.h"

#ifndef USING_UCD_SNMP_EXTENSIBLE_MODULE
#include "util_funcs/header_simple_table.h"
#include "mibdefs.h"
#define SHELLCOMMAND 3
#endif

netsnmp_feature_require(extract_table_row_data)
netsnmp_feature_require(table_data_delete_table)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(insert_table_row)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

oid  ns_extend_oid[]    = { 1, 3, 6, 1, 4, 1, 8072, 1, 3, 2 };
oid  extend_count_oid[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 3, 2, 1 };
oid  extend_config_oid[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 3, 2, 2 };
oid  extend_out1_oid[]  = { 1, 3, 6, 1, 4, 1, 8072, 1, 3, 2, 3 };
oid  extend_out2_oid[]  = { 1, 3, 6, 1, 4, 1, 8072, 1, 3, 2, 4 };

typedef struct extend_registration_block_s {
    netsnmp_table_data *dinfo;
    oid                *root_oid;
    size_t              oid_len;
    long                num_entries;
    netsnmp_extend     *ehead;
    netsnmp_handler_registration       *reg[3];
    struct extend_registration_block_s *next;
} extend_registration_block;
extend_registration_block *ereg_head = NULL;


#ifndef USING_UCD_SNMP_EXTENSIBLE_MODULE
typedef struct netsnmp_old_extend_s {
    unsigned int idx;
    netsnmp_extend *exec_entry;
    netsnmp_extend *efix_entry;
} netsnmp_old_extend;

unsigned int             num_compatability_entries = 0;
unsigned int             max_compatability_entries = 50;
netsnmp_old_extend *compatability_entries;

WriteMethod fixExec2Error;
FindVarMethod var_extensible_old;
oid  old_extensible_variables_oid[] = { NETSNMP_UCDAVIS_MIB, NETSNMP_SHELLMIBNUM, 1 };
#ifndef NETSNMP_NO_WRITE_SUPPORT
struct variable2 old_extensible_variables[] = {
    {MIBINDEX,     ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {MIBINDEX}},
    {ERRORNAME,    ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {ERRORNAME}},
    {SHELLCOMMAND, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {SHELLCOMMAND}},
    {ERRORFLAG,    ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {ERRORFLAG}},
    {ERRORMSG,     ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {ERRORMSG}},
    {ERRORFIX,     ASN_INTEGER,  NETSNMP_OLDAPI_RWRITE,
     var_extensible_old, 1, {ERRORFIX}},
    {ERRORFIXCMD,  ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {ERRORFIXCMD}}
};
#else /* !NETSNMP_NO_WRITE_SUPPORT */
struct variable2 old_extensible_variables[] = {
    {MIBINDEX,     ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {MIBINDEX}},
    {ERRORNAME,    ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {ERRORNAME}},
    {SHELLCOMMAND, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {SHELLCOMMAND}},
    {ERRORFLAG,    ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {ERRORFLAG}},
    {ERRORMSG,     ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {ERRORMSG}},
    {ERRORFIX,     ASN_INTEGER,  NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {ERRORFIX}},
    {ERRORFIXCMD,  ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_extensible_old, 1, {ERRORFIXCMD}}
};
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
#endif


        /*************************
         *
         *  Main initialisation routine
         *
         *************************/

extend_registration_block *
_find_extension_block( oid *name, size_t name_len )
{
    extend_registration_block         *eptr;
    size_t len;
    for ( eptr=ereg_head; eptr; eptr=eptr->next ) {
        len = SNMP_MIN(name_len, eptr->oid_len);
        if (!snmp_oid_compare( name, len, eptr->root_oid, eptr->oid_len))
            return eptr;
    }
    return NULL;
}

extend_registration_block *
_register_extend( oid *base, size_t len )
{
    extend_registration_block         *eptr;
    oid oid_buf[MAX_OID_LEN];

    netsnmp_table_data                *dinfo;
    netsnmp_table_registration_info   *tinfo;
    netsnmp_watcher_info              *winfo;
    netsnmp_handler_registration      *reg = NULL;
    int                                rc;

    for ( eptr=ereg_head; eptr; eptr=eptr->next ) {
        if (!snmp_oid_compare( base, len, eptr->root_oid, eptr->oid_len))
            return eptr;
    }
    if (!eptr) {
        eptr = SNMP_MALLOC_TYPEDEF( extend_registration_block );
        if (!eptr)
            return NULL;
        eptr->root_oid = snmp_duplicate_objid( base, len );
        eptr->oid_len  = len;
        eptr->num_entries = 0;
        eptr->ehead       = NULL;
        eptr->dinfo       = netsnmp_create_table_data( "nsExtendTable" );
        eptr->next        = ereg_head;
        ereg_head         = eptr;
    }

    dinfo = eptr->dinfo;
    memcpy( oid_buf, base, len*sizeof(oid) );

        /*
         * Register the configuration table
         */
    tinfo = SNMP_MALLOC_TYPEDEF( netsnmp_table_registration_info );
    netsnmp_table_helper_add_indexes( tinfo, ASN_OCTET_STR, 0 );
    tinfo->min_column = COLUMN_EXTCFG_FIRST_COLUMN;
    tinfo->max_column = COLUMN_EXTCFG_LAST_COLUMN;
    oid_buf[len] = 2;
#ifndef NETSNMP_NO_WRITE_SUPPORT
    reg   = netsnmp_create_handler_registration(
                "nsExtendConfigTable", handle_nsExtendConfigTable, 
                oid_buf, len+1, HANDLER_CAN_RWRITE);
#else /* !NETSNMP_NO_WRITE_SUPPORT */
    reg   = netsnmp_create_handler_registration(
                "nsExtendConfigTable", handle_nsExtendConfigTable, 
                oid_buf, len+1, HANDLER_CAN_RONLY);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    rc = netsnmp_register_table_data( reg, dinfo, tinfo );
    if (rc != SNMPERR_SUCCESS) {
        goto bail;
    }
    netsnmp_handler_owns_table_info(reg->handler->next);
    eptr->reg[0] = reg;

        /*
         * Register the main output table
         *   using the same table_data handle.
         * This is sufficient to link the two tables,
         *   and implement the AUGMENTS behaviour
         */
    tinfo = SNMP_MALLOC_TYPEDEF( netsnmp_table_registration_info );
    netsnmp_table_helper_add_indexes( tinfo, ASN_OCTET_STR, 0 );
    tinfo->min_column = COLUMN_EXTOUT1_FIRST_COLUMN;
    tinfo->max_column = COLUMN_EXTOUT1_LAST_COLUMN;
    oid_buf[len] = 3;
    reg   = netsnmp_create_handler_registration(
                "nsExtendOut1Table", handle_nsExtendOutput1Table, 
                oid_buf, len+1, HANDLER_CAN_RONLY);
    rc = netsnmp_register_table_data( reg, dinfo, tinfo );
    if (rc != SNMPERR_SUCCESS)
        goto bail;
    netsnmp_handler_owns_table_info(reg->handler->next);
    eptr->reg[1] = reg;

        /*
         * Register the multi-line output table
         *   using a simple table helper.
         * This handles extracting the indexes from
         *   the request OID, but leaves most of
         *   the work to our handler routine.
         * Still, it was nice while it lasted...
         */
    tinfo = SNMP_MALLOC_TYPEDEF( netsnmp_table_registration_info );
    netsnmp_table_helper_add_indexes( tinfo, ASN_OCTET_STR, ASN_INTEGER, 0 );
    tinfo->min_column = COLUMN_EXTOUT2_FIRST_COLUMN;
    tinfo->max_column = COLUMN_EXTOUT2_LAST_COLUMN;
    oid_buf[len] = 4;
    reg   = netsnmp_create_handler_registration(
                "nsExtendOut2Table", handle_nsExtendOutput2Table, 
                oid_buf, len+1, HANDLER_CAN_RONLY);
    rc = netsnmp_register_table( reg, tinfo );
    if (rc != SNMPERR_SUCCESS)
        goto bail;
    netsnmp_handler_owns_table_info(reg->handler->next);
    eptr->reg[2] = reg;

        /*
         * Register a watched scalar to keep track of the number of entries
         */
    oid_buf[len] = 1;
    reg   = netsnmp_create_handler_registration(
                "nsExtendNumEntries", NULL, 
                oid_buf, len+1, HANDLER_CAN_RONLY);
    winfo = netsnmp_create_watcher_info(
                &(eptr->num_entries), sizeof(eptr->num_entries),
                ASN_INTEGER, WATCHER_FIXED_SIZE);
    rc = netsnmp_register_watched_scalar2( reg, winfo );
    if (rc != SNMPERR_SUCCESS)
        goto bail;

    return eptr;

bail:
    if (eptr->reg[2])
        netsnmp_unregister_handler(eptr->reg[2]);
    if (eptr->reg[1])
        netsnmp_unregister_handler(eptr->reg[1]);
    if (eptr->reg[0])
        netsnmp_unregister_handler(eptr->reg[0]);
    return NULL;
}

static void
_unregister_extend(extend_registration_block *eptr)
{
    extend_registration_block *prev;

    netsnmp_assert(eptr);
    for (prev = ereg_head; prev && prev->next != eptr; prev = prev->next)
	;
    if (prev) {
        netsnmp_assert(eptr == prev->next);
	prev->next = eptr->next;
    } else {
        netsnmp_assert(eptr == ereg_head);
	ereg_head = eptr->next;
    }

    netsnmp_table_data_delete_table(eptr->dinfo);
    free(eptr->root_oid);
    free(eptr);
}

int
extend_clear_callback(int majorID, int minorID,
                    void *serverarg, void *clientarg)
{
    extend_registration_block *eptr, *enext = NULL;

    for ( eptr=ereg_head; eptr; eptr=enext ) {
        enext=eptr->next;
        netsnmp_unregister_handler( eptr->reg[0] );
        netsnmp_unregister_handler( eptr->reg[1] );
        netsnmp_unregister_handler( eptr->reg[2] );
        SNMP_FREE(eptr);
    }
    ereg_head = NULL;
    return 0;
}

void init_extend( void )
{
    snmpd_register_config_handler("extend",    extend_parse_config, NULL, NULL);
    snmpd_register_config_handler("extend-sh", extend_parse_config, NULL, NULL);
    snmpd_register_config_handler("extendfix", extend_parse_config, NULL, NULL);
    snmpd_register_config_handler("exec2", extend_parse_config, NULL, NULL);
    snmpd_register_config_handler("sh2",   extend_parse_config, NULL, NULL);
    snmpd_register_config_handler("execFix2", extend_parse_config, NULL, NULL);
    (void)_register_extend( ns_extend_oid, OID_LENGTH(ns_extend_oid));

#ifndef USING_UCD_SNMP_EXTENSIBLE_MODULE
    snmpd_register_config_handler("exec", extend_parse_config, NULL, NULL);
    snmpd_register_config_handler("sh",   extend_parse_config, NULL, NULL);
    snmpd_register_config_handler("execFix", extend_parse_config, NULL, NULL);
    compatability_entries = (netsnmp_old_extend *)
        calloc( max_compatability_entries, sizeof(netsnmp_old_extend));
    REGISTER_MIB("ucd-extensible", old_extensible_variables,
                 variable2, old_extensible_variables_oid);
#endif

    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_PRE_UPDATE_CONFIG,
                           extend_clear_callback, NULL);
}

void
shutdown_extend(void)
{
#ifndef USING_UCD_SNMP_EXTENSIBLE_MODULE
    free(compatability_entries);
    compatability_entries = NULL;
#endif
    while (ereg_head)
	_unregister_extend(ereg_head);
}

        /*************************
         *
         *  Cached-data hooks
         *  see 'cache_handler' helper
         *
         *************************/

int
extend_load_cache(netsnmp_cache *cache, void *magic)
{
#ifndef USING_UTILITIES_EXECUTE_MODULE
    NETSNMP_LOGONCE((LOG_WARNING,"support for run_exec_command not available\n"));
    return -1;
#else
    int  out_len = 1024*100;
    char out_buf[ 1024*100 ];
    int  cmd_len = 255*2 + 2;	/* 2 * DisplayStrings */
    char cmd_buf[ 255*2 + 2 ];
    int  ret;
    char *cp;
    char *line_buf[ 1024 ];
    netsnmp_extend *extension = (netsnmp_extend *)magic;

    if (!magic)
        return -1;
    DEBUGMSGTL(( "nsExtendTable:cache", "load %s", extension->token ));
    if ( extension->args )
        snprintf( cmd_buf, cmd_len, "%s %s", extension->command, extension->args );
    else 
        snprintf( cmd_buf, cmd_len, "%s", extension->command );
    if ( extension->flags & NS_EXTEND_FLAGS_SHELL )
        ret = run_shell_command( cmd_buf, extension->input, out_buf, &out_len);
    else
        ret = run_exec_command(  cmd_buf, extension->input, out_buf, &out_len);
    DEBUGMSG(( "nsExtendTable:cache", ": %s : %d\n", cmd_buf, ret));
    if (ret >= 0) {
        if (out_buf[   out_len-1 ] == '\n')
            out_buf[ --out_len   ] =  '\0';	/* Stomp on trailing newline */
        extension->output   = strdup( out_buf );
        extension->out_len  = out_len;
        /*
         * Now we need to pick the output apart into separate lines.
         * Start by counting how many lines we've got, and keeping
         * track of where each line starts in a static buffer
         */
        extension->numlines = 1;
        line_buf[ 0 ] = extension->output;
        for (cp=extension->output; *cp; cp++) {
            if (*cp == '\n') {
                line_buf[ extension->numlines++ ] = cp+1;
            }
        }
        if ( extension->numlines > 1 ) {
            extension->lines = (char**)calloc( sizeof(char *), extension->numlines );
            memcpy( extension->lines, line_buf,
                                       sizeof(char *) * extension->numlines );
        } else {
            extension->lines = &extension->output;
        }
    }
    extension->result = ret;
    return ret;
#endif /* !defined(USING_UTILITIES_EXECUTE_MODULE) */
}

void
extend_free_cache(netsnmp_cache *cache, void *magic)
{
    netsnmp_extend *extension = (netsnmp_extend *)magic;
    if (!magic)
        return;

    DEBUGMSGTL(( "nsExtendTable:cache", "free %s\n", extension->token ));
    if (extension->output) {
        SNMP_FREE(extension->output);
        extension->output = NULL;
    }
    if ( extension->numlines > 1 ) {
        SNMP_FREE(extension->lines);
    }
    extension->lines  = NULL;
    extension->out_len  = 0;
    extension->numlines = 0;
}


        /*************************
         *
         *  Utility routines for setting up a new entry
         *  (either via SET requests, or the config file)
         *
         *************************/

void
_free_extension( netsnmp_extend *extension, extend_registration_block *ereg )
{
    netsnmp_extend *eptr  = NULL;
    netsnmp_extend *eprev = NULL;

    if (!extension)
        return;

    if (ereg) {
        /* Unlink from 'ehead' list */
        for (eptr=ereg->ehead; eptr; eptr=eptr->next) {
            if (eptr == extension)
                break;
            eprev = eptr;
        }
        if (!eptr) {
            snmp_log(LOG_ERR,
                     "extend: fell off end of list before finding extension\n");
            return;
        }
        if (eprev)
            eprev->next = eptr->next;
        else
            ereg->ehead = eptr->next;
        netsnmp_table_data_remove_and_delete_row( ereg->dinfo, extension->row);
    }

    SNMP_FREE( extension->token );
    SNMP_FREE( extension->cache );
    SNMP_FREE( extension->command );
    SNMP_FREE( extension->args  );
    SNMP_FREE( extension->input );
    SNMP_FREE( extension );
    return;
}

netsnmp_extend *
_new_extension( char *exec_name, int exec_flags, extend_registration_block *ereg )
{
    netsnmp_extend     *extension;
    netsnmp_table_row  *row;
    netsnmp_extend     *eptr1, *eptr2; 
    netsnmp_table_data *dinfo = ereg->dinfo;

    if (!exec_name)
        return NULL;
    extension = SNMP_MALLOC_TYPEDEF( netsnmp_extend );
    if (!extension)
        return NULL;
    extension->token    = strdup( exec_name );
    extension->flags    = exec_flags;
    extension->cache    = netsnmp_cache_create( 0, extend_load_cache,
                                                   extend_free_cache, NULL, 0 );
    if (extension->cache)
        extension->cache->magic = extension;

    row = netsnmp_create_table_data_row();
    if (!row || !extension->cache) {
        _free_extension( extension, ereg );
        SNMP_FREE( row );
        return NULL;
    }
    row->data = (void *)extension;
    extension->row = row;
    netsnmp_table_row_add_index( row, ASN_OCTET_STR,
                                 exec_name, strlen(exec_name));
    if ( netsnmp_table_data_add_row( dinfo, row) != SNMPERR_SUCCESS ) {
        /* _free_extension( extension, ereg ); */
        SNMP_FREE( extension );  /* Probably not sufficient */
        SNMP_FREE( row );
        return NULL;
    }

    ereg->num_entries++;
        /*
         *  Now add this structure to a private linked list.
         *  We don't need this for the main tables - the
         *   'table_data' helper will take care of those.
         *  But it's probably easier to handle the multi-line
         *  output table ourselves, for which we need access
         *  to the underlying data.
         *   So we'll keep a list internally as well.
         */
    for ( eptr1 = ereg->ehead, eptr2 = NULL;
          eptr1;
          eptr2 = eptr1, eptr1 = eptr1->next ) {

        if (strlen( eptr1->token )  > strlen( exec_name ))
            break;
        if (strlen( eptr1->token ) == strlen( exec_name ) &&
            strcmp( eptr1->token, exec_name ) > 0 )
            break;
    }
    if ( eptr2 )
        eptr2->next = extension;
    else
        ereg->ehead = extension;
    extension->next = eptr1;
    return extension;
}

void
extend_parse_config(const char *token, char *cptr)
{
    netsnmp_extend *extension;
    char exec_name[STRMAX];
    char exec_name2[STRMAX];     /* For use with UCD execFix directive */
    char exec_command[STRMAX];
    oid  oid_buf[MAX_OID_LEN];
    size_t oid_len;
    extend_registration_block *eptr;
    int  flags;

    cptr = copy_nword(cptr, exec_name,    sizeof(exec_name));
    if ( *exec_name == '.' ) {
        oid_len = MAX_OID_LEN - 2;
        if (0 == read_objid( exec_name, oid_buf, &oid_len )) {
            config_perror("ERROR: Unrecognised OID" );
            return;
        }
        cptr = copy_nword(cptr, exec_name,    sizeof(exec_name));
        if (!strcmp( token, "sh"   ) ||
            !strcmp( token, "exec" )) {
            config_perror("ERROR: This output format has been deprecated - Please use the 'extend' directive instead" );
            return;
        }
    } else {
        memcpy( oid_buf, ns_extend_oid, sizeof(ns_extend_oid));
        oid_len = OID_LENGTH(ns_extend_oid);
    }
    cptr = copy_nword(cptr, exec_command, sizeof(exec_command));
    /* XXX - check 'exec_command' exists & is executable */
    flags = (NS_EXTEND_FLAGS_ACTIVE | NS_EXTEND_FLAGS_CONFIG);
    if (!strcmp( token, "sh"        ) ||
        !strcmp( token, "extend-sh" ) ||
        !strcmp( token, "sh2" ))
        flags |= NS_EXTEND_FLAGS_SHELL;
    if (!strcmp( token, "execFix"   ) ||
        !strcmp( token, "extendfix" ) ||
        !strcmp( token, "execFix2" )) {
        strcpy( exec_name2, exec_name );
        strcat( exec_name, "Fix" );
        flags |= NS_EXTEND_FLAGS_WRITEABLE;
        /* XXX - Check for shell... */
    }

    eptr      = _register_extend( oid_buf, oid_len );
    extension = _new_extension( exec_name, flags, eptr );
    if (extension) {
        extension->command  = strdup( exec_command );
        if (cptr)
            extension->args = strdup( cptr );
    } else {
        snmp_log(LOG_ERR, "Failed to register extend entry '%s' - possibly duplicate name.\n", exec_name );
        return;
    }

#ifndef USING_UCD_SNMP_EXTENSIBLE_MODULE
    /*
     *  Compatability with the UCD extTable
     */
    if (!strcmp( token, "execFix"  )) {
        int  i;
        for ( i=0; i < num_compatability_entries; i++ ) {
            if (!strcmp( exec_name2,
                    compatability_entries[i].exec_entry->token))
                break;
        }
        if ( i == num_compatability_entries )
            config_perror("No matching exec entry" );
        else
            compatability_entries[ i ].efix_entry = extension;
            
    } else if (!strcmp( token, "sh"   ) ||
               !strcmp( token, "exec" )) {
        if ( num_compatability_entries == max_compatability_entries ) {
            /* XXX - should really use dynamic allocation */
            netsnmp_old_extend *new_compatability_entries;
            new_compatability_entries = realloc(compatability_entries,
                             max_compatability_entries*2*sizeof(netsnmp_old_extend));
            if (!new_compatability_entries)
                config_perror("No further UCD-compatible entries" );
            else {
                memset(new_compatability_entries+num_compatability_entries, 0,
                        sizeof(netsnmp_old_extend)*max_compatability_entries);
                max_compatability_entries *= 2;
                compatability_entries = new_compatability_entries;
            }
        }
        if (num_compatability_entries != max_compatability_entries)
            compatability_entries[
                num_compatability_entries++ ].exec_entry = extension;
    }
#endif
}

        /*************************
         *
         *  Main table handlers
         *  Most of the work is handled
         *   by the 'table_data' helper.
         *
         *************************/

int
handle_nsExtendConfigTable(netsnmp_mib_handler          *handler,
                     netsnmp_handler_registration *reginfo,
                     netsnmp_agent_request_info   *reqinfo,
                     netsnmp_request_info         *requests)
{
    netsnmp_request_info       *request;
    netsnmp_table_request_info *table_info;
    netsnmp_extend             *extension;
    extend_registration_block  *eptr;
    int  i;
    int  need_to_validate = 0;

    for ( request=requests; request; request=request->next ) {
        if (request->processed)
            continue;
        table_info = netsnmp_extract_table_info( request );
        extension  = (netsnmp_extend*)netsnmp_extract_table_row_data( request );

        DEBUGMSGTL(( "nsExtendTable:config", "varbind: "));
        DEBUGMSGOID(("nsExtendTable:config", request->requestvb->name,
                                             request->requestvb->name_length));
        DEBUGMSG((   "nsExtendTable:config", " (%s)\n",
                      se_find_label_in_slist("agent_mode", reqinfo->mode)));

        switch (reqinfo->mode) {
        case MODE_GET:
            switch (table_info->colnum) {
            case COLUMN_EXTCFG_COMMAND:
                snmp_set_var_typed_value(
                     request->requestvb, ASN_OCTET_STR,
                     extension->command,
                    (extension->command)?strlen(extension->command):0);
                break;
            case COLUMN_EXTCFG_ARGS:
                snmp_set_var_typed_value(
                     request->requestvb, ASN_OCTET_STR,
                     extension->args,
                    (extension->args)?strlen(extension->args):0);
                break;
            case COLUMN_EXTCFG_INPUT:
                snmp_set_var_typed_value(
                     request->requestvb, ASN_OCTET_STR,
                     extension->input,
                    (extension->input)?strlen(extension->input):0);
                break;
            case COLUMN_EXTCFG_CACHETIME:
                snmp_set_var_typed_value(
                     request->requestvb, ASN_INTEGER,
                    (u_char*)&extension->cache->timeout, sizeof(int));
                break;
            case COLUMN_EXTCFG_EXECTYPE:
                i = ((extension->flags & NS_EXTEND_FLAGS_SHELL) ?
                                         NS_EXTEND_ETYPE_SHELL :
                                         NS_EXTEND_ETYPE_EXEC);
                snmp_set_var_typed_value(
                     request->requestvb, ASN_INTEGER,
                    (u_char*)&i, sizeof(i));
                break;
            case COLUMN_EXTCFG_RUNTYPE:
                i = ((extension->flags & NS_EXTEND_FLAGS_WRITEABLE) ?
                                         NS_EXTEND_RTYPE_RWRITE :
                                         NS_EXTEND_RTYPE_RONLY);
                snmp_set_var_typed_value(
                     request->requestvb, ASN_INTEGER,
                    (u_char*)&i, sizeof(i));
                break;

            case COLUMN_EXTCFG_STORAGE:
                i = ((extension->flags & NS_EXTEND_FLAGS_CONFIG) ?
                                         ST_PERMANENT : ST_VOLATILE);
                snmp_set_var_typed_value(
                     request->requestvb, ASN_INTEGER,
                    (u_char*)&i, sizeof(i));
                break;
            case COLUMN_EXTCFG_STATUS:
                i = ((extension->flags & NS_EXTEND_FLAGS_ACTIVE) ?
                                         RS_ACTIVE :
                                         RS_NOTINSERVICE);
                snmp_set_var_typed_value(
                     request->requestvb, ASN_INTEGER,
                    (u_char*)&i, sizeof(i));
                break;

            default:
                netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
                continue;
            }
            break;

        /**********
         *
         * Start of SET handling
         *
         *   All config objects are potentially writable except
         *     nsExtendStorage which is fixed as either 'permanent'
         *     (if read from a config file) or 'volatile' (if set via SNMP)
         *   The string-based settings of a 'permanent' entry cannot 
         *     be changed - neither can the execution or run type.
         *   Such entries can be (temporarily) marked as inactive,
         *     and the cache timeout adjusted, but these changes are
         *     not persistent.
         *
         **********/

#ifndef NETSNMP_NO_WRITE_SUPPORT
        case MODE_SET_RESERVE1:
            /*
             * Validate the new assignments
             */
            switch (table_info->colnum) {
            case COLUMN_EXTCFG_COMMAND:
                if (request->requestvb->type != ASN_OCTET_STR) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGTYPE);
                    return SNMP_ERR_WRONGTYPE;
                }
                /*
                 * Must have a full path to the command
                 * XXX - Assumes Unix-style paths
                 */
                if (request->requestvb->val_len == 0 ||
                    request->requestvb->val.string[0] != '/') {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGVALUE);
                    return SNMP_ERR_WRONGVALUE;
                }
                /*
                 * XXX - need to check this file exists
                 *       (and is executable)
                 */

                if (extension && extension->flags & NS_EXTEND_FLAGS_CONFIG) {
                    /*
                     * config entries are "permanent" so can't be changed
                     */
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_NOTWRITABLE);
                    return SNMP_ERR_NOTWRITABLE;
                }
                break;

            case COLUMN_EXTCFG_ARGS:
            case COLUMN_EXTCFG_INPUT:
                if (request->requestvb->type != ASN_OCTET_STR) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGTYPE);
                    return SNMP_ERR_WRONGTYPE;
                }

                if (extension && extension->flags & NS_EXTEND_FLAGS_CONFIG) {
                    /*
                     * config entries are "permanent" so can't be changed
                     */
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_NOTWRITABLE);
                    return SNMP_ERR_NOTWRITABLE;
                }
                break;

            case COLUMN_EXTCFG_CACHETIME:
                if (request->requestvb->type != ASN_INTEGER) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGTYPE);
                    return SNMP_ERR_WRONGTYPE;
                }
                i = *request->requestvb->val.integer;
                /*
                 * -1 is a special value indicating "don't cache"
                 *    [[ XXX - should this be 0 ?? ]]
                 * Otherwise, cache times must be non-negative
                 */
                if (i < -1 ) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGVALUE);
                    return SNMP_ERR_WRONGVALUE;
                }
                break;

            case COLUMN_EXTCFG_EXECTYPE:
                if (request->requestvb->type != ASN_INTEGER) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGTYPE);
                    return SNMP_ERR_WRONGTYPE;
                }
                i = *request->requestvb->val.integer;
                if (i<1 || i>2) {  /* 'exec(1)' or 'shell(2)' only */
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGVALUE);
                    return SNMP_ERR_WRONGVALUE;
                }
                if (extension && extension->flags & NS_EXTEND_FLAGS_CONFIG) {
                    /*
                     * config entries are "permanent" so can't be changed
                     */
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_NOTWRITABLE);
                    return SNMP_ERR_NOTWRITABLE;
                }
                break;

            case COLUMN_EXTCFG_RUNTYPE:
                if (request->requestvb->type != ASN_INTEGER) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGTYPE);
                    return SNMP_ERR_WRONGTYPE;
                }
                /*
                 * 'run-on-read(1)', 'run-on-set(2)'
                 *  or 'run-command(3)' only
                 */
                i = *request->requestvb->val.integer;
                if (i<1 || i>3) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGVALUE);
                    return SNMP_ERR_WRONGVALUE;
                }
                /*
                 * 'run-command(3)' can only be used with
                 *  a pre-existing 'run-on-set(2)' entry.
                 */
                if (i==3 && !(extension && (extension->flags & NS_EXTEND_FLAGS_WRITEABLE))) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_INCONSISTENTVALUE);
                    return SNMP_ERR_INCONSISTENTVALUE;
                }
                /*
                 * 'run-command(3)' is the only valid assignment
                 *  for permanent (i.e. config) entries
                 */
                if ((extension && extension->flags & NS_EXTEND_FLAGS_CONFIG)
                    && i!=3 ) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_INCONSISTENTVALUE);
                    return SNMP_ERR_INCONSISTENTVALUE;
                }
                break;

            case COLUMN_EXTCFG_STATUS:
                if (request->requestvb->type != ASN_INTEGER) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGTYPE);
                    return SNMP_ERR_WRONGTYPE;
                }
                i = *request->requestvb->val.integer;
                switch (i) {
                case RS_ACTIVE:
                case RS_NOTINSERVICE:
                    if (!extension) {
                        /* Must be used with existing rows */
                        netsnmp_set_request_error(reqinfo, request,
                                                  SNMP_ERR_INCONSISTENTVALUE);
                        return SNMP_ERR_INCONSISTENTVALUE;
                    }
                    break;    /* OK */
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    if (extension) {
                        /* Can only be used to create new rows */
                        netsnmp_set_request_error(reqinfo, request,
                                                  SNMP_ERR_INCONSISTENTVALUE);
                        return SNMP_ERR_INCONSISTENTVALUE;
                    }
                    break;
                case RS_DESTROY:
                    break;
                default:
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGVALUE);
                    return SNMP_ERR_WRONGVALUE;
                }
                break;

            default:
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_ERR_NOTWRITABLE);
                return SNMP_ERR_NOTWRITABLE;
            }
            break;

        case MODE_SET_RESERVE2:
            switch (table_info->colnum) {
            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->val.integer;
                switch (i) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    eptr = _find_extension_block( request->requestvb->name,
                                                  request->requestvb->name_length );
                    extension = _new_extension( (char *) table_info->indexes->val.string,
                                                0, eptr );
                    if (!extension) {  /* failed */
                        netsnmp_set_request_error(reqinfo, request,
                                                  SNMP_ERR_RESOURCEUNAVAILABLE);
                        return SNMP_ERR_RESOURCEUNAVAILABLE;
                    }
                    netsnmp_insert_table_row( request, extension->row );
                }
            }
            break;

        case MODE_SET_FREE:
            switch (table_info->colnum) {
            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->val.integer;
                switch (i) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    eptr = _find_extension_block( request->requestvb->name,
                                                  request->requestvb->name_length );
                    _free_extension( extension, eptr );
                }
            }
            break;

        case MODE_SET_ACTION:
            switch (table_info->colnum) {
            case COLUMN_EXTCFG_COMMAND:
                extension->old_command = extension->command;
                extension->command = netsnmp_strdup_and_null(
                    request->requestvb->val.string,
                    request->requestvb->val_len);
                break;
            case COLUMN_EXTCFG_ARGS:
                extension->old_args = extension->args;
                extension->args = netsnmp_strdup_and_null(
                    request->requestvb->val.string,
                    request->requestvb->val_len);
                break;
            case COLUMN_EXTCFG_INPUT:
                extension->old_input = extension->input;
                extension->input = netsnmp_strdup_and_null(
                    request->requestvb->val.string,
                    request->requestvb->val_len);
                break;
            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->val.integer;
                switch (i) {
                case RS_ACTIVE:
                case RS_CREATEANDGO:
                    need_to_validate = 1;
                }
                break;
            }
            break;

        case MODE_SET_UNDO:
            switch (table_info->colnum) {
            case COLUMN_EXTCFG_COMMAND:
                if ( extension && extension->old_command ) {
                    SNMP_FREE(extension->command);
                    extension->command     = extension->old_command;
                    extension->old_command = NULL;
                }
                break;
            case COLUMN_EXTCFG_ARGS:
                if ( extension && extension->old_args ) {
                    SNMP_FREE(extension->args);
                    extension->args     = extension->old_args;
                    extension->old_args = NULL;
                }
                break;
            case COLUMN_EXTCFG_INPUT:
                if ( extension && extension->old_input ) {
                    SNMP_FREE(extension->input);
                    extension->input     = extension->old_input;
                    extension->old_input = NULL;
                }
                break;
            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->val.integer;
                switch (i) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    eptr = _find_extension_block( request->requestvb->name,
                                                  request->requestvb->name_length );
                    _free_extension( extension, eptr );
                }
                break;
            }
            break;

        case MODE_SET_COMMIT:
            switch (table_info->colnum) {
            case COLUMN_EXTCFG_CACHETIME:
                i = *request->requestvb->val.integer;
                extension->cache->timeout = i;
                break;

            case COLUMN_EXTCFG_RUNTYPE:
                i = *request->requestvb->val.integer;
                switch (i) {
                case 1:
                    extension->flags &= ~NS_EXTEND_FLAGS_WRITEABLE;
                    break;
                case 2:
                    extension->flags |=  NS_EXTEND_FLAGS_WRITEABLE;
                    break;
                case 3:
                    (void)netsnmp_cache_check_and_reload( extension->cache );
                    break;
                }
                break;

            case COLUMN_EXTCFG_EXECTYPE:
                i = *request->requestvb->val.integer;
                if ( i == NS_EXTEND_ETYPE_SHELL )
                    extension->flags |=  NS_EXTEND_FLAGS_SHELL;
                else
                    extension->flags &= ~NS_EXTEND_FLAGS_SHELL;
                break;

            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->val.integer;
                switch (i) {
                case RS_ACTIVE:
                case RS_CREATEANDGO:
                    extension->flags |= NS_EXTEND_FLAGS_ACTIVE;
                    break;
                case RS_NOTINSERVICE:
                case RS_CREATEANDWAIT:
                    extension->flags &= ~NS_EXTEND_FLAGS_ACTIVE;
                    break;
                case RS_DESTROY:
                    eptr = _find_extension_block( request->requestvb->name,
                                                  request->requestvb->name_length );
                    _free_extension( extension, eptr );
                    break;
                }
            }
            break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */ 

        default:
            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_GENERR);
            return SNMP_ERR_GENERR;
        }
    }

#ifndef NETSNMP_NO_WRITE_SUPPORT
    /*
     * If we're marking a given row as active,
     *  then we need to check that it's ready.
     */
    if (need_to_validate) {
        for ( request=requests; request; request=request->next ) {
            if (request->processed)
                continue;
            table_info = netsnmp_extract_table_info( request );
            extension  = (netsnmp_extend*)netsnmp_extract_table_row_data( request );
            switch (table_info->colnum) {
            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->val.integer;
                if (( i == RS_ACTIVE || i == RS_CREATEANDGO ) &&
                    !(extension && extension->command &&
                      extension->command[0] == '/' /* &&
                      is_executable(extension->command) */)) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_INCONSISTENTVALUE);
                    return SNMP_ERR_INCONSISTENTVALUE;
                }
            }
        }
    }
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    
    return SNMP_ERR_NOERROR;
}


int
handle_nsExtendOutput1Table(netsnmp_mib_handler          *handler,
                     netsnmp_handler_registration *reginfo,
                     netsnmp_agent_request_info   *reqinfo,
                     netsnmp_request_info         *requests)
{
    netsnmp_request_info       *request;
    netsnmp_table_request_info *table_info;
    netsnmp_extend             *extension;
    int len;

    for ( request=requests; request; request=request->next ) {
        if (request->processed)
            continue;
        table_info = netsnmp_extract_table_info( request );
        extension  = (netsnmp_extend*)netsnmp_extract_table_row_data( request );

        DEBUGMSGTL(( "nsExtendTable:output1", "varbind: "));
        DEBUGMSGOID(("nsExtendTable:output1", request->requestvb->name,
                                              request->requestvb->name_length));
        DEBUGMSG((   "nsExtendTable:output1", "\n"));

        switch (reqinfo->mode) {
        case MODE_GET:
            if (!extension || !(extension->flags & NS_EXTEND_FLAGS_ACTIVE)) {
                /*
                 * If this row is inactive, then skip it.
                 */
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_NOSUCHINSTANCE);
                continue;
            }
            if (!(extension->flags & NS_EXTEND_FLAGS_WRITEABLE) &&
                (netsnmp_cache_check_and_reload( extension->cache ) < 0 )) {
                /*
                 * If reloading the output cache of a 'run-on-read'
                 * entry fails, then skip it.
                 */
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_NOSUCHINSTANCE);
                continue;
            }
            if ((extension->flags & NS_EXTEND_FLAGS_WRITEABLE) &&
                (netsnmp_cache_check_expired( extension->cache ) == 1 )) {
                /*
                 * If the output cache of a 'run-on-write'
                 * entry has expired, then skip it.
                 */
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_NOSUCHINSTANCE);
                continue;
            }

            switch (table_info->colnum) {
            case COLUMN_EXTOUT1_OUTLEN:
                snmp_set_var_typed_value(
                     request->requestvb, ASN_INTEGER,
                    (u_char*)&extension->out_len, sizeof(int));
                break;
            case COLUMN_EXTOUT1_OUTPUT1:
                /* 
                 * If we've got more than one line,
                 * find the length of the first one.
                 * Otherwise find the length of the whole string.
                 */
                if (extension->numlines > 1) {
                    len = (extension->lines[1])-(extension->output) -1;
                } else if (extension->output) {
                    len = strlen(extension->output);
                } else {
                    len = 0;
                }
                snmp_set_var_typed_value(
                     request->requestvb, ASN_OCTET_STR,
                     extension->output, len);
                break;
            case COLUMN_EXTOUT1_OUTPUT2:
                snmp_set_var_typed_value(
                     request->requestvb, ASN_OCTET_STR,
                     extension->output,
                    (extension->output)?extension->out_len:0);
                break;
            case COLUMN_EXTOUT1_NUMLINES:
                snmp_set_var_typed_value(
                     request->requestvb, ASN_INTEGER,
                    (u_char*)&extension->numlines, sizeof(int));
                break;
            case COLUMN_EXTOUT1_RESULT:
                snmp_set_var_typed_value(
                     request->requestvb, ASN_INTEGER,
                    (u_char*)&extension->result, sizeof(int));
                break;
            default:
                netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
                continue;
            }
            break;
        default:
            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_GENERR);
            return SNMP_ERR_GENERR;
        }
    }
    return SNMP_ERR_NOERROR;
}


        /*************************
         *
         *  Multi-line output table handler
         *  Most of the work is handled here.
         *
         *************************/


/*
 *  Locate the appropriate entry for a given request
 */
netsnmp_extend *
_extend_find_entry( netsnmp_request_info       *request,
                    netsnmp_table_request_info *table_info,
                    int mode  )
{
    netsnmp_extend            *eptr;
    extend_registration_block *ereg;
    unsigned int line_idx;
    oid oid_buf[MAX_OID_LEN];
    int oid_len;
    int i;
    char  *token;
    size_t token_len;

    if (!request || !table_info || !table_info->indexes
                 || !table_info->indexes->next_variable) {
        DEBUGMSGTL(( "nsExtendTable:output2", "invalid invocation\n"));
        return NULL;
    }

    ereg = _find_extension_block( request->requestvb->name,
                                  request->requestvb->name_length );

    /***
     *  GET handling - find the exact entry being requested
     ***/
    if ( mode == MODE_GET ) {
        DEBUGMSGTL(( "nsExtendTable:output2", "GET: %s / %ld\n ",
                      table_info->indexes->val.string,
                     *table_info->indexes->next_variable->val.integer));
        for ( eptr = ereg->ehead; eptr; eptr = eptr->next ) {
            if ( !strcmp( eptr->token, (char *) table_info->indexes->val.string ))
                break;
        }

        if ( eptr ) {
            /*
             * Ensure the output is available...
             */
            if (!(eptr->flags & NS_EXTEND_FLAGS_ACTIVE) ||
               (netsnmp_cache_check_and_reload( eptr->cache ) < 0 ))
                return NULL;

            /*
             * ...and check the line requested is valid
             */
            line_idx = *table_info->indexes->next_variable->val.integer;
            if (line_idx < 1 || line_idx > eptr->numlines)
                return NULL;
        }
    }

        /***
         *  GETNEXT handling - find the first suitable entry
         ***/
    else {
        if (!table_info->indexes->val_len ) {
            DEBUGMSGTL(( "nsExtendTable:output2", "GETNEXT: first entry\n"));
            /*
             * Beginning of the table - find the first active
             *  (and successful) entry, and use the first line of it
             */
            for (eptr = ereg->ehead; eptr; eptr = eptr->next ) {
                if ((eptr->flags & NS_EXTEND_FLAGS_ACTIVE) &&
                    (netsnmp_cache_check_and_reload( eptr->cache ) >= 0 )) {
                    line_idx = 1;
                    break;
                }
            }
        } else {
            token     =  (char *) table_info->indexes->val.string;
            token_len =  table_info->indexes->val_len;
            line_idx  = *table_info->indexes->next_variable->val.integer;
            DEBUGMSGTL(( "nsExtendTable:output2", "GETNEXT: %s / %d\n ",
                          token, line_idx ));
            /*
             * Otherwise, find the first entry not earlier
             * than the requested token...
             */
            for (eptr = ereg->ehead; eptr; eptr = eptr->next ) {
                if ( strlen(eptr->token) > token_len )
                    break;
                if ( strlen(eptr->token) == token_len &&
                     strcmp(eptr->token, token) >= 0 )
                    break;
            }
            if (!eptr)
                return NULL;    /* (assuming there is one) */

            /*
             * ... and make sure it's active & the output is available
             * (or use the first following entry that is)
             */
            for (    ; eptr; eptr = eptr->next ) {
                if ((eptr->flags & NS_EXTEND_FLAGS_ACTIVE) &&
                    (netsnmp_cache_check_and_reload( eptr->cache ) >= 0 )) {
                    break;
                }
                line_idx = 1;
            }

            if (!eptr)
                return NULL;    /* (assuming there is one) */

            /*
             *  If we're working with the same entry that was requested,
             *  see whether we've reached the end of the output...
             */
            if (!strcmp( eptr->token, token )) {
                if ( eptr->numlines <= line_idx ) {
                    /*
                     * ... and if so, move on to the first line
                     * of the next (active and successful) entry.
                     */
                    line_idx = 1;
                    for (eptr = eptr->next ; eptr; eptr = eptr->next ) {
                        if ((eptr->flags & NS_EXTEND_FLAGS_ACTIVE) &&
                            (netsnmp_cache_check_and_reload( eptr->cache ) >= 0 )) {
                            break;
                        }
                    }
                } else {
                    /*
                     * Otherwise just use the next line of this entry.
                     */
                    line_idx++;
                }
            }
            else {
                /*
                 * If this is not the same entry that was requested,
                 * then we should return the first line.
                 */
                line_idx = 1;
            }
        }
        if (eptr) {
            DEBUGMSGTL(( "nsExtendTable:output2", "GETNEXT -> %s / %d\n ",
                          eptr->token, line_idx));
            /*
             * Since we're processing a GETNEXT request,
             * now we've found the appropriate entry (and line),
             * we need to update the varbind OID ...
             */
            memset(oid_buf, 0, sizeof(oid_buf));
            oid_len = ereg->oid_len;
            memcpy( oid_buf, ereg->root_oid, oid_len*sizeof(oid));
            oid_buf[ oid_len++ ] = 4;    /* nsExtendOutput2Table */
            oid_buf[ oid_len++ ] = 1;    /* nsExtendOutput2Entry */
            oid_buf[ oid_len++ ] = COLUMN_EXTOUT2_OUTLINE;
                                         /* string token index */
            oid_buf[ oid_len++ ] = strlen(eptr->token);
            for ( i=0; i < (int)strlen(eptr->token); i++ )
                oid_buf[ oid_len+i ] = eptr->token[i];
            oid_len += strlen( eptr->token );
                                         /* plus line number */
            oid_buf[ oid_len++ ] = line_idx;
            snmp_set_var_objid( request->requestvb, oid_buf, oid_len );
            /*
             * ... and index values to match.
             */
            snmp_set_var_value( table_info->indexes,
                                eptr->token, strlen(eptr->token));
            snmp_set_var_value( table_info->indexes->next_variable,
                                (const u_char*)&line_idx, sizeof(line_idx));
        }
    }
    return eptr;  /* Finally, signal success */
}

/*
 *  Multi-line output handler
 *  Locate the appropriate entry (using _extend_find_entry)
 *  and return the appropriate output line
 */
int
handle_nsExtendOutput2Table(netsnmp_mib_handler          *handler,
                     netsnmp_handler_registration *reginfo,
                     netsnmp_agent_request_info   *reqinfo,
                     netsnmp_request_info         *requests)
{
    netsnmp_request_info       *request;
    netsnmp_table_request_info *table_info;
    netsnmp_extend             *extension;
    char *cp;
    unsigned int line_idx;
    int len;

    for ( request=requests; request; request=request->next ) {
        if (request->processed)
            continue;

        table_info = netsnmp_extract_table_info( request );
        extension  = _extend_find_entry( request, table_info, reqinfo->mode );

        DEBUGMSGTL(( "nsExtendTable:output2", "varbind: "));
        DEBUGMSGOID(("nsExtendTable:output2", request->requestvb->name,
                                              request->requestvb->name_length));
        DEBUGMSG((   "nsExtendTable:output2", " (%s)\n",
                                    (extension) ? extension->token : "[none]"));

        if (!extension) {
            if (reqinfo->mode == MODE_GET)
                netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHINSTANCE);
            else
                netsnmp_set_request_error(reqinfo, request, SNMP_ENDOFMIBVIEW);
            continue;
        }

        switch (reqinfo->mode) {
        case MODE_GET:
        case MODE_GETNEXT:
            switch (table_info->colnum) {
            case COLUMN_EXTOUT2_OUTLINE:
                /* 
                 * Determine which line we've been asked for....
                 */
                line_idx = *table_info->indexes->next_variable->val.integer;
                if (line_idx < 1 || line_idx > extension->numlines) {
                    netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHINSTANCE);
                    continue;
                }
                cp  = extension->lines[line_idx-1];

                /* 
                 * ... and how long it is.
                 */
                if ( extension->numlines > line_idx )
                    len = (extension->lines[line_idx])-cp -1;
                else if (cp)
                    len = strlen(cp);
                else
                    len = 0;

                snmp_set_var_typed_value( request->requestvb,
                                          ASN_OCTET_STR, cp, len );
                break;
            default:
                netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
                continue;
            }
            break;
        default:
            netsnmp_set_request_error(reqinfo, request, SNMP_ERR_GENERR);
            return SNMP_ERR_GENERR;
        }
    }
    return SNMP_ERR_NOERROR;
}

#ifndef USING_UCD_SNMP_EXTENSIBLE_MODULE
        /*************************
         *
         *  Compatability with the UCD extTable
         *
         *************************/

u_char *
var_extensible_old(struct variable * vp,
                     oid * name,
                     size_t * length,
                     int exact,
                     size_t * var_len, WriteMethod ** write_method)
{
    netsnmp_old_extend *exten = NULL;
    static long     long_ret;
    unsigned int idx;

    if (header_simple_table
        (vp, name, length, exact, var_len, write_method, num_compatability_entries))
        return (NULL);

    idx = name[*length-1] -1;
	if (idx > max_compatability_entries)
		return NULL;
    exten = &compatability_entries[ idx ];
    if (exten) {
        switch (vp->magic) {
        case MIBINDEX:
            long_ret = name[*length - 1];
            return ((u_char *) (&long_ret));
        case ERRORNAME:        /* name defined in config file */
            *var_len = strlen(exten->exec_entry->token);
            return ((u_char *) (exten->exec_entry->token));
        case SHELLCOMMAND:
            *var_len = strlen(exten->exec_entry->command);
            return ((u_char *) (exten->exec_entry->command));
        case ERRORFLAG:        /* return code from the process */
            netsnmp_cache_check_and_reload( exten->exec_entry->cache );
            long_ret = exten->exec_entry->result;
            return ((u_char *) (&long_ret));
        case ERRORMSG:         /* first line of text returned from the process */
            netsnmp_cache_check_and_reload( exten->exec_entry->cache );
            if (exten->exec_entry->numlines > 1) {
                *var_len = (exten->exec_entry->lines[1])-
                           (exten->exec_entry->output) -1;
            } else if (exten->exec_entry->output) {
                *var_len = strlen(exten->exec_entry->output);
            } else {
                *var_len = 0;
            }
            return ((u_char *) (exten->exec_entry->output));
        case ERRORFIX:
            *write_method = fixExec2Error;
            long_return = 0;
            return ((u_char *) &long_return);

        case ERRORFIXCMD:
            if (exten->efix_entry) {
                *var_len = strlen(exten->efix_entry->command);
                return ((u_char *) exten->efix_entry->command);
            } else {
                *var_len = 0;
                return ((u_char *) &long_return);  /* Just needs to be non-null! */
            }
        }
        return NULL;
    }
    return NULL;
}


int
fixExec2Error(int action,
             u_char * var_val,
             u_char var_val_type,
             size_t var_val_len,
             u_char * statP, oid * name, size_t name_len)
{
    netsnmp_old_extend *exten = NULL;
    unsigned int idx;

    idx = name[name_len-1] -1;
    exten = &compatability_entries[ idx ];

#ifndef NETSNMP_NO_WRITE_SUPPORT
    switch (action) {
    case MODE_SET_RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR, "Wrong type != int\n");
            return SNMP_ERR_WRONGTYPE;
        }
        idx = *((long *) var_val);
        if (idx != 1) {
            snmp_log(LOG_ERR, "Wrong value != 1\n");
            return SNMP_ERR_WRONGVALUE;
        }
        if (!exten || !exten->efix_entry) {
            snmp_log(LOG_ERR, "No command to run\n");
            return SNMP_ERR_GENERR;
        }
        return SNMP_ERR_NOERROR;

    case MODE_SET_COMMIT:
        netsnmp_cache_check_and_reload( exten->efix_entry->cache );
    }
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    return SNMP_ERR_NOERROR;
}
#endif /* USING_UCD_SNMP_EXTENSIBLE_MODULE */
