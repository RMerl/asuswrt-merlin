/*
 *  Host Resources MIB - storage group implementation - hrh_storage.c
 *
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/memory.h>
#include <net-snmp/agent/hardware/fsys.h>
#include "host_res.h"
#include "hrh_filesys.h"
#include "hrh_storage.h"
#include "hr_disk.h"
#include <net-snmp/utilities.h>


#include <sys/types.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if TIME_WITH_SYS_TIME
# ifdef WIN32
#  include <windows.h>
#  include <errno.h>
#  include <sys/timeb.h>
# else
#  include <sys/time.h>
# endif
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/output_api.h>

#include <net-snmp/agent/agent_read_config.h>
#include <net-snmp/library/read_config.h>

#define HRSTORE_MONOTONICALLY_INCREASING

        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/


extern netsnmp_fsys_info *HRFS_entry;

static void parse_storage_config(const char *, char *);

        /*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/
int             Get_Next_HR_Store(void);
void            Init_HR_Store(void);
int             header_hrstore(struct variable *, oid *, size_t *, int,
                               size_t *, WriteMethod **);
void*           header_hrstoreEntry(struct variable *, oid *, size_t *,
                                    int, size_t *, WriteMethod **);
Netsnmp_Node_Handler handle_memsize;


#define	HRSTORE_MEMSIZE		1
#define	HRSTORE_INDEX		2
#define	HRSTORE_TYPE		3
#define	HRSTORE_DESCR		4
#define	HRSTORE_UNITS		5
#define	HRSTORE_SIZE		6
#define	HRSTORE_USED		7
#define	HRSTORE_FAILS		8

struct variable2 hrstore_variables[] = {
    {HRSTORE_INDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrstore, 1, {1}},
    {HRSTORE_TYPE, ASN_OBJECT_ID, NETSNMP_OLDAPI_RONLY,
     var_hrstore, 1, {2}},
    {HRSTORE_DESCR, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrstore, 1, {3}},
    {HRSTORE_UNITS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrstore, 1, {4}},
    {HRSTORE_SIZE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrstore, 1, {5}},
    {HRSTORE_USED, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrstore, 1, {6}},
    {HRSTORE_FAILS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_hrstore, 1, {7}}
};
oid             hrstore_variables_oid[] = { 1, 3, 6, 1, 2, 1, 25, 2 };
oid             hrMemorySize_oid[]   = { 1, 3, 6, 1, 2, 1, 25, 2, 2 };
oid             hrStorageTable_oid[] = { 1, 3, 6, 1, 2, 1, 25, 2, 3, 1 };


void
init_hrh_storage(void)
{
    char *appname;

    netsnmp_register_scalar(
        netsnmp_create_handler_registration("host/hrMemorySize", handle_memsize,
                           hrMemorySize_oid, OID_LENGTH(hrMemorySize_oid),
                                             HANDLER_CAN_RONLY));
    REGISTER_MIB("host/hr_storage", hrstore_variables, variable2,
                 hrStorageTable_oid);

    appname = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_APPTYPE);
    netsnmp_ds_register_config(ASN_BOOLEAN, appname, "skipNFSInHostResources", 
			       NETSNMP_DS_APPLICATION_ID,
			       NETSNMP_DS_AGENT_SKIPNFSINHOSTRESOURCES);

    netsnmp_ds_register_config(ASN_BOOLEAN, appname, "realStorageUnits",
                   NETSNMP_DS_APPLICATION_ID,
                   NETSNMP_DS_AGENT_REALSTORAGEUNITS);

    snmpd_register_config_handler("storageUseNFS", parse_storage_config, NULL,
	"1 | 2\t\t(1 = enable, 2 = disable)");
}

static int storageUseNFS = 1;	/* Default to reporting NFS mounts as NetworkDisk */

static void
parse_storage_config(const char *token, char *cptr)
{
    char *val;
    int ival;
    char *st;

    val = strtok_r(cptr, " \t", &st);
    if (!val) {
        config_perror("Missing FLAG parameter in storageUseNFS");
        return;
    }
    ival = atoi(val);
    if (ival < 1 || ival > 2) {
        config_perror("storageUseNFS must be 1 or 2");
        return;
    }
    storageUseNFS = (ival == 1) ? 1 : 0;
}

/*
 * header_hrstoreEntry(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */

void *
header_hrstoreEntry(struct variable *vp,
                    oid * name,
                    size_t * length,
                    int exact,
                    size_t * var_len, WriteMethod ** write_method)
{
#define HRSTORE_ENTRY_NAME_LENGTH	11
    oid             newname[MAX_OID_LEN];
    int             storage_idx, LowIndex = -1;
    int             result;
    int                  idx = -1;
    netsnmp_memory_info *mem  = NULL;

    DEBUGMSGTL(("host/hr_storage", "var_hrstoreEntry: request "));
    DEBUGMSGOID(("host/hr_storage", name, *length));
    DEBUGMSG(("host/hr_storage", " exact=%d\n", exact));

    memcpy((char *) newname, (char *) vp->name,
           (int) vp->namelen * sizeof(oid));
    result = snmp_oid_compare(name, *length, vp->name, vp->namelen);

    DEBUGMSGTL(("host/hr_storage", "var_hrstoreEntry: compare "));
    DEBUGMSGOID(("host/hr_storage", vp->name, vp->namelen));
    DEBUGMSG(("host/hr_storage", " => %d\n", result));


    if (result < 0 ||
        *length <= HRSTORE_ENTRY_NAME_LENGTH ) {
       /*
        * Requested OID too early or too short to refer
        *   to a valid row (for the current column object).
        * GET requests should fail, GETNEXT requests
        *   should use the first row.
        */
        if ( exact )
            return NULL;
        netsnmp_memory_load();
        mem = netsnmp_memory_get_first( 0 );
    }
    else {
        /*
         * Otherwise, retrieve the requested
         *  (or following) row as appropriate.
         */
        if ( exact && *length > HRSTORE_ENTRY_NAME_LENGTH+1 )
            return NULL;   /* Too long for a valid instance */
        idx = name[ HRSTORE_ENTRY_NAME_LENGTH ];
        if ( idx < NETSNMP_MEM_TYPE_MAX ) {
            netsnmp_memory_load();
            mem = ( exact ? netsnmp_memory_get_byIdx( idx, 0 ) :
                       netsnmp_memory_get_next_byIdx( idx, 0 ));
        }
    }

    /*
     * If this matched a memory-based entry, then
     *    update the OID parameter(s) for GETNEXT requests.
     */
    if ( mem ) {
        if ( !exact ) {
            newname[ HRSTORE_ENTRY_NAME_LENGTH ] = mem->idx;
            memcpy((char *) name, (char *) newname,
                   ((int) vp->namelen + 1) * sizeof(oid));
            *length = vp->namelen + 1;
        }
    }
    /*
     * If this didn't match a memory-based entry,
     *   then consider the disk-based storage.
     */
    else {
        Init_HR_Store();
        for (;;) {
            storage_idx = Get_Next_HR_Store();
            DEBUGMSG(("host/hr_storage", "(index %d ....", storage_idx));
            if (storage_idx == -1)
                break;
            newname[HRSTORE_ENTRY_NAME_LENGTH] = storage_idx;
            DEBUGMSGOID(("host/hr_storage", newname, *length));
            DEBUGMSG(("host/hr_storage", "\n"));
            result = snmp_oid_compare(name, *length, newname, vp->namelen + 1);
            if (exact && (result == 0)) {
                LowIndex = storage_idx;
                /*
                 * Save storage status information 
                 */
                break;
            }
            if ((!exact && (result < 0)) &&
                (LowIndex == -1 || storage_idx < LowIndex)) {
                LowIndex = storage_idx;
                /*
                 * Save storage status information 
                 */
#ifdef HRSTORE_MONOTONICALLY_INCREASING
                break;
#endif
            }
        }
        if ( LowIndex != -1 ) {
            if ( !exact ) {
                newname[ HRSTORE_ENTRY_NAME_LENGTH ] = LowIndex;
                memcpy((char *) name, (char *) newname,
                       ((int) vp->namelen + 1) * sizeof(oid));
                *length = vp->namelen + 1;
            }
            mem = (netsnmp_memory_info*)0xffffffff;   /* To indicate 'success' */
        }
    }

    *write_method = (WriteMethod*)0;
    *var_len = sizeof(long);    /* default to 'long' results */

    /*
     *  ... and return the appropriate row
     */
    DEBUGMSGTL(("host/hr_storage", "var_hrstoreEntry: process "));
    DEBUGMSGOID(("host/hr_storage", name, *length));
    DEBUGMSG(("host/hr_storage", " (%p)\n", mem));
    return (void*)mem;
}

oid             storage_type_id[] = { 1, 3, 6, 1, 2, 1, 25, 2, 1, 1 };  /* hrStorageOther */
int             storage_type_len =
    sizeof(storage_type_id) / sizeof(storage_type_id[0]);

        /*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

int
handle_memsize(netsnmp_mib_handler *handler,
                netsnmp_handler_registration *reginfo,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests)
{
    netsnmp_memory_info *mem_info;
    int val;

    /*
     * We just need to handle valid GET requests, as invalid instances
     *   are rejected automatically, and (valid) GETNEXT requests are
     *   converted into the appropriate GET request.
     *
     * We also only ever receive one request at a time.
     */
    switch (reqinfo->mode) {
    case MODE_GET:
        netsnmp_memory_load();
        mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 0 );
        if ( !mem_info || mem_info->size == -1 || mem_info->units == -1 )
            netsnmp_set_request_error( reqinfo, requests, SNMP_NOSUCHOBJECT );
	else {
            val  =  mem_info->size;     /* memtotal */
            val *= (mem_info->units/1024);
            snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
                                     (u_char *)&val, sizeof(val));
        }
        return SNMP_ERR_NOERROR;

    default:
        /*
         * we should never get here, so this is a really bad error 
         */
        snmp_log(LOG_ERR, "unknown mode (%d) in handle_memsize\n",
                 reqinfo->mode);
        return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}


u_char         *
var_hrstore(struct variable *vp,
            oid * name,
            size_t * length,
            int exact, size_t * var_len, WriteMethod ** write_method)
{
    int             store_idx = 0;
    static char     string[1024];
    void                *ptr;
    netsnmp_memory_info *mem = NULL;

really_try_next:
	ptr = header_hrstoreEntry(vp, name, length, exact, var_len,
					write_method);
	if (ptr == NULL)
	    return NULL;

        store_idx = name[ HRSTORE_ENTRY_NAME_LENGTH ];
        if (HRFS_entry &&
	    store_idx > NETSNMP_MEM_TYPE_MAX &&
            netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                                   NETSNMP_DS_AGENT_SKIPNFSINHOSTRESOURCES) &&
            Check_HR_FileSys_NFS())
            return NULL;
        if (store_idx <= NETSNMP_MEM_TYPE_MAX ) {
	    mem = (netsnmp_memory_info*)ptr;
        }



    switch (vp->magic) {
    case HRSTORE_INDEX:
        long_return = store_idx;
        return (u_char *) & long_return;
    case HRSTORE_TYPE:
        if (store_idx > NETSNMP_MEM_TYPE_MAX)
            if (HRFS_entry->flags & NETSNMP_FS_FLAG_REMOTE )
                storage_type_id[storage_type_len - 1] = 10;     /* Network Disk */
            else if (HRFS_entry->flags & NETSNMP_FS_FLAG_REMOVE )
                storage_type_id[storage_type_len - 1] = 5;      /* Removable Disk */
            else
                storage_type_id[storage_type_len - 1] = 4;      /* Assume fixed */
        else
            switch (store_idx) {
            case NETSNMP_MEM_TYPE_PHYSMEM:
            case NETSNMP_MEM_TYPE_USERMEM:
                storage_type_id[storage_type_len - 1] = 2;      /* RAM */
                break;
            case NETSNMP_MEM_TYPE_VIRTMEM:
            case NETSNMP_MEM_TYPE_SWAP:
                storage_type_id[storage_type_len - 1] = 3;      /* Virtual Mem */
                break;
            default:
                storage_type_id[storage_type_len - 1] = 1;      /* Other */
                break;
            }
        *var_len = sizeof(storage_type_id);
        return (u_char *) storage_type_id;
    case HRSTORE_DESCR:
        if (store_idx > NETSNMP_MEM_TYPE_MAX) {
            strlcpy(string, HRFS_entry->path, sizeof(string));
            *var_len = strlen(string);
            return (u_char *) string;
        } else {
            if ( !mem || !mem->descr )
                goto try_next;
            *var_len = strlen(mem->descr);
            return (u_char *) mem->descr;
        }
    case HRSTORE_UNITS:
        if (store_idx > NETSNMP_MEM_TYPE_MAX) {
            if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                    NETSNMP_DS_AGENT_REALSTORAGEUNITS))
                long_return = HRFS_entry->units & 0xffffffff;
            else
                long_return = HRFS_entry->units_32;
        } else {
            if ( !mem || mem->units == -1 )
                goto try_next;
            long_return = mem->units;
        }
        return (u_char *) & long_return;
    case HRSTORE_SIZE:
        if (store_idx > NETSNMP_MEM_TYPE_MAX) {
            if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                    NETSNMP_DS_AGENT_REALSTORAGEUNITS))
                long_return = HRFS_entry->size & 0xffffffff;
            else
                long_return = HRFS_entry->size_32;
        } else {
            if ( !mem || mem->size == -1 )
                goto try_next;
            long_return = mem->size;
        }
        return (u_char *) & long_return;
    case HRSTORE_USED:
        if (store_idx > NETSNMP_MEM_TYPE_MAX) {
            if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                    NETSNMP_DS_AGENT_REALSTORAGEUNITS))
                long_return = HRFS_entry->used & 0xffffffff;
            else
                long_return = HRFS_entry->used_32;
        } else {
            if ( !mem || mem->size == -1 || mem->free == -1 )
                goto try_next;
            long_return = mem->size - mem->free;
        }
        return (u_char *) & long_return;
    case HRSTORE_FAILS:
        if (store_idx > NETSNMP_MEM_TYPE_MAX)
#if NETSNMP_NO_DUMMY_VALUES
	    goto try_next;
#else
            long_return = 0;
#endif
        else {
            if ( !mem || mem->other == -1 )
                goto try_next;
            long_return = mem->other;
        }
        return (u_char *) & long_return;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_hrstore\n",
                    vp->magic));
    }
    return NULL;

  try_next:
    if (!exact)
        goto really_try_next;

    return NULL;
}


        /*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

static int      HRS_index;

void
Init_HR_Store(void)
{
    HRS_index = 0;
    Init_HR_FileSys();
}

int
Get_Next_HR_Store(void)
{
    /*
     * File-based storage 
     */
	for (;;) {
    	HRS_index = Get_Next_HR_FileSys();
		if (HRS_index >= 0) {
			if (!(netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
							NETSNMP_DS_AGENT_SKIPNFSINHOSTRESOURCES) && 
						Check_HR_FileSys_NFS())) {
				return HRS_index + NETSNMP_MEM_TYPE_MAX;	
			}
		} else {
			return -1;
		}	
	}
}

