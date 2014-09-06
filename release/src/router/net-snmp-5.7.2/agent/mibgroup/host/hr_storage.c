/*
 *  Host Resources MIB - storage group implementation - hr_storage.c
 *
 */

#include <net-snmp/net-snmp-config.h>

#if defined(freebsd5)
/* undefine these in order to use getfsstat */
#undef HAVE_STATVFS
#undef HAVE_STRUCT_STATVFS_F_FRSIZE
#endif

#include <sys/types.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if (!defined(mingw32) && !defined(WIN32))
#if HAVE_UTMPX_H
#include <utmpx.h>
#else
#include <utmp.h>
#endif
#endif /* mingw32 */
#ifndef dynix
#if HAVE_SYS_VM_H
#include <sys/vm.h>
#if (!defined(KERNEL) || defined(MACH_USER_API)) && defined(HAVE_SYS_VMMETER_H) /*OS X does not #include <sys/vmmeter.h> if (defined(KERNEL) && !defined(MACH_USER_API)) */
#include <sys/vmmeter.h>
#endif
#else
#if HAVE_VM_VM_H
#include <vm/vm.h>
#if HAVE_MACHINE_TYPES_H
#include <machine/types.h>
#endif
#if HAVE_SYS_VMMETER_H
#include <sys/vmmeter.h>
#endif
#if HAVE_VM_VM_PARAM_H
#include <vm/vm_param.h>
#endif
#else
#if HAVE_SYS_VMPARAM_H
#include <sys/vmparam.h>
#endif
#if HAVE_SYS_VMMAC_H
#include <sys/vmmac.h>
#endif
#if HAVE_SYS_VMMETER_H
#include <sys/vmmeter.h>
#endif
#if HAVE_SYS_VMSYSTM_H
#include <sys/vmsystm.h>
#endif
#endif                          /* vm/vm.h */
#endif                          /* sys/vm.h */
#if defined(HAVE_UVM_UVM_PARAM_H) && defined(HAVE_UVM_UVM_EXTERN_H)
#include <uvm/uvm_param.h>
#include <uvm/uvm_extern.h>
#elif defined(HAVE_VM_VM_PARAM_H) && defined(HAVE_VM_VM_EXTERN_H)
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#endif
#if HAVE_KVM_H
#include <kvm.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_SYS_POOL_H
#if defined(MBPOOL_SYMBOL) && defined(MCLPOOL_SYMBOL)
#define __POOL_EXPOSE
#include <sys/pool.h>
#else
#undef HAVE_SYS_POOL_H
#endif
#endif
#if HAVE_SYS_MBUF_H
#include <sys/mbuf.h>
#endif
#if HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#if defined(CTL_HW) && defined(HW_PAGESIZE)
#define USE_SYSCTL
#endif
#if USE_MACH_HOST_STATISTICS
#include <mach/mach.h>
#elif defined(CTL_VM) && (defined(VM_METER) || defined(VM_UVMEXP))
#define USE_SYSCTL_VM
#endif
#endif                          /* if HAVE_SYS_SYSCTL_H */
#endif                          /* ifndef dynix */

#if (defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)) && HAVE_LIBPERFSTAT_H
#ifdef HAVE_SYS_PROTOSW_H
#include <sys/protosw.h>
#endif
#include <libperfstat.h>
#endif


#include "host_res.h"
#include "hr_storage.h"
#include "hr_filesys.h"
#include <net-snmp/agent/auto_nlist.h>

#if HAVE_MNTENT_H
#include <mntent.h>
#endif
#if HAVE_SYS_MNTTAB_H
#include <sys/mnttab.h>
#endif
#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#if HAVE_SYS_MOUNT_H
#ifdef __osf__
#undef m_next
#undef m_data
#endif
#include <sys/mount.h>
#endif
#ifdef HAVE_MACHINE_PARAM_H
#include <machine/param.h>
#endif
#include <sys/stat.h>

#if defined(hpux10) || defined(hpux11)
#include <sys/pstat.h>
#endif
#if defined(solaris2)
#if HAVE_SYS_SWAP_H
#include <sys/swap.h>
#endif
#endif

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_NBUTIL_H
#include <nbutil.h>
#endif

#include <net-snmp/utilities.h>
#include <net-snmp/output_api.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/memory.h>

#ifdef solaris2
#include "kernel_sunos5.h"
#endif

#include <net-snmp/agent/agent_read_config.h>
#include <net-snmp/library/read_config.h>

#define HRSTORE_MONOTONICALLY_INCREASING

        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/


#ifdef solaris2

extern struct mnttab *HRFS_entry;
#define HRFS_mount	mnt_mountp
#define HRFS_statfs	statvfs
#define HRFS_HAS_FRSIZE HAVE_STRUCT_STATVFS_F_FRSIZE

#elif defined(WIN32)
/* fake block size */
#define FAKED_BLOCK_SIZE 512

extern struct win_statfs *HRFS_entry;
#define HRFS_statfs	win_statfs
#define HRFS_mount	f_driveletter

#elif defined(HAVE_STATVFS) && defined(__NetBSD__)

extern struct statvfs *HRFS_entry;
extern int      fscount;
#define HRFS_statfs	statvfs
#define HRFS_mount	f_mntonname
#define HRFS_HAS_FRSIZE HAVE_STRUCT_STATVFS_F_FRSIZE

#elif defined(HAVE_STATVFS)  && defined(HAVE_STRUCT_STATVFS_MNT_DIR)

extern struct mntent *HRFS_entry;
extern int      fscount;
#define HRFS_statfs	statvfs
#define HRFS_mount	mnt_dir
#define HRFS_HAS_FRSIZE HAVE_STRUCT_STATVFS_F_FRSIZE

#elif defined(HAVE_GETFSSTAT) && !defined(HAVE_STATFS) && defined(HAVE_STATVFS)

extern struct statfs *HRFS_entry;
extern int      fscount;
#define HRFS_statfs	statvfs
#define HRFS_mount	f_mntonname
#define HRFS_HAS_FRSIZE STRUCT_STATVFS_HAS_F_FRSIZE

#elif defined(HAVE_GETFSSTAT)

extern struct statfs *HRFS_entry;
extern int      fscount;
#define HRFS_statfs	statfs
#define HRFS_mount	f_mntonname
#define HRFS_HAS_FRSIZE HAVE_STRUCT_STATFS_F_FRSIZE

#else

extern struct mntent *HRFS_entry;
#define HRFS_mount	mnt_dir
#define HRFS_statfs	statfs
#define HRFS_HAS_FRSIZE HAVE_STRUCT_STATFS_F_FRSIZE

#endif
	
#if defined(USE_MACH_HOST_STATISTICS)
mach_port_t myHost;
#endif

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

#ifdef solaris2
void            sol_get_swapinfo(int *, int *);
#endif

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
init_hr_storage(void)
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
    struct HRFS_statfs stat_buf;
    void                *ptr;
    netsnmp_memory_info *mem = NULL;

really_try_next:
	ptr = header_hrstoreEntry(vp, name, length, exact, var_len,
					write_method);
	if (ptr == NULL)
	    return NULL;

        store_idx = name[ HRSTORE_ENTRY_NAME_LENGTH ];
        if (store_idx > NETSNMP_MEM_TYPE_MAX ) {
            if ( netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                                        NETSNMP_DS_AGENT_SKIPNFSINHOSTRESOURCES) &&
                 Check_HR_FileSys_NFS())
                return NULL;  /* or goto try_next; */
	    if (HRFS_statfs(HRFS_entry->HRFS_mount, &stat_buf) < 0) {
		snmp_log_perror(HRFS_entry->HRFS_mount);
		goto try_next;
	    }
	} else {
	    mem = (netsnmp_memory_info*)ptr;
        }



    switch (vp->magic) {
    case HRSTORE_INDEX:
        long_return = store_idx;
        return (u_char *) & long_return;
    case HRSTORE_TYPE:
        if (store_idx > NETSNMP_MEM_TYPE_MAX)
            if (storageUseNFS && Check_HR_FileSys_NFS())
                storage_type_id[storage_type_len - 1] = 10;     /* Network Disk */
#if HAVE_HASMNTOPT && !(defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7))
            /* 
             * hasmntopt takes "const struct mntent*", but HRFS_entry has been
             * defined differently for AIX, so skip this for AIX
             */
            else if (hasmntopt(HRFS_entry, "loop") != NULL)
                storage_type_id[storage_type_len - 1] = 5;      /* Removable Disk */
#endif
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
            strlcpy(string, HRFS_entry->HRFS_mount, sizeof(string));
            *var_len = strlen(string);
            return (u_char *) string;
        } else {
            if ( !mem || !mem->descr )
                goto try_next;
            *var_len = strlen(mem->descr);
            return (u_char *) mem->descr;
        }
    case HRSTORE_UNITS:
        if (store_idx > NETSNMP_MEM_TYPE_MAX)
#if HRFS_HAS_FRSIZE
            long_return = stat_buf.f_frsize;
#else
            long_return = stat_buf.f_bsize;
#endif
        else {
            if ( !mem || mem->units == -1 )
                goto try_next;
            long_return = mem->units;
        }
        return (u_char *) & long_return;
    case HRSTORE_SIZE:
        if (store_idx > NETSNMP_MEM_TYPE_MAX)
            long_return = stat_buf.f_blocks;
        else {
            if ( !mem || mem->size == -1 )
                goto try_next;
            long_return = mem->size;
        }
        return (u_char *) & long_return;
    case HRSTORE_USED:
        if (store_idx > NETSNMP_MEM_TYPE_MAX)
            long_return = (stat_buf.f_blocks - stat_buf.f_bfree);
        else {
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

#ifdef solaris2
void
sol_get_swapinfo(int *totalP, int *usedP)
{
    struct anoninfo ainfo;

    if (swapctl(SC_AINFO, &ainfo) < 0) {
        *totalP = *usedP = 0;
        return;
    }

    *totalP = ainfo.ani_max;
    *usedP = ainfo.ani_resv;
}
#endif                          /* solaris2 */

#ifdef WIN32
char *win_realpath(const char *file_name, char *resolved_name)
{
	char szFile[_MAX_PATH + 1];
	char *pszRet;
 	
	pszRet = _fullpath(szFile, resolved_name, MAX_PATH);
 	
	return pszRet;  
}

static int win_statfs (const char *path, struct win_statfs *buf)
{
    HINSTANCE h;
    FARPROC f;
    int retval = 0;
    char tmp [MAX_PATH], resolved_path [MAX_PATH];
    GetFullPathName(path, MAX_PATH, resolved_path, NULL);
    /* TODO - Fix this! The realpath macro needs defined
     * or rewritten into the function.
     */
    
    win_realpath(path, resolved_path);
    
    if (!resolved_path)
    	retval = - 1;
    else {
    	/* check whether GetDiskFreeSpaceExA is supported */
        h = LoadLibraryA ("kernel32.dll");
        if (h)
			f = GetProcAddress (h, "GetDiskFreeSpaceExA");
        else
        	f = NULL;
		
        if (f) {
			ULARGE_INTEGER bytes_free, bytes_total, bytes_free2;
            if (!f (resolved_path, &bytes_free2, &bytes_total, &bytes_free)) {
				errno = ENOENT;
				retval = - 1;
			} else {
				buf -> f_bsize = FAKED_BLOCK_SIZE;
				buf -> f_bfree = (bytes_free.QuadPart) / FAKED_BLOCK_SIZE;
				buf -> f_files = buf -> f_blocks = (bytes_total.QuadPart) / FAKED_BLOCK_SIZE;
				buf -> f_ffree = buf -> f_bavail = (bytes_free2.QuadPart) / FAKED_BLOCK_SIZE;
			}
		} else {
			DWORD sectors_per_cluster, bytes_per_sector;
			if (h) FreeLibrary (h);
			if (!GetDiskFreeSpaceA (resolved_path, &sectors_per_cluster,
					&bytes_per_sector, &buf -> f_bavail, &buf -> f_blocks)) {
                errno = ENOENT;
                retval = - 1;
            } else {
                buf -> f_bsize = sectors_per_cluster * bytes_per_sector;
                buf -> f_files = buf -> f_blocks;
                buf -> f_ffree = buf -> f_bavail;
                buf -> f_bfree = buf -> f_bavail;
            }
		}
		if (h) FreeLibrary (h);
	}

	/* get the FS volume information */
	if (strspn (":", resolved_path) > 0) resolved_path [3] = '\0'; /* we want only the root */    
	if (GetVolumeInformation (resolved_path, NULL, 0, &buf -> f_fsid, &buf -> f_namelen, 
									NULL, tmp, MAX_PATH)) {
		if (strcasecmp ("NTFS", tmp) == 0) {
			buf -> f_type = NTFS_SUPER_MAGIC;
		} else {
			buf -> f_type = MSDOS_SUPER_MAGIC;
		}
	} else {
		errno = ENOENT;
		retval = - 1;
	}
	return retval;
}
#endif	/* WIN32 */
