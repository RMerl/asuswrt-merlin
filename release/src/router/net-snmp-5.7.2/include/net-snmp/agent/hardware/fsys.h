typedef struct netsnmp_fsys_info_s netsnmp_fsys_info;

#define _NETSNMP_FS_TYPE_SKIP_BIT   0x2000
#define _NETSNMP_FS_TYPE_LOCAL      0x1000

   /*
    * Enumeration from HOST-RESOURCES-TYPES mib
    */
#define NETSNMP_FS_TYPE_OTHER	   1
#define NETSNMP_FS_TYPE_UNKNOWN	   2
#define NETSNMP_FS_TYPE_BERKELEY   3
#define NETSNMP_FS_TYPE_SYSV	   4
#define NETSNMP_FS_TYPE_FAT	   5
#define NETSNMP_FS_TYPE_HPFS	   6
#define NETSNMP_FS_TYPE_HFS	   7
#define NETSNMP_FS_TYPE_MFS	   8
#define NETSNMP_FS_TYPE_NTFS	   9
#define NETSNMP_FS_TYPE_VNODE	   10
#define NETSNMP_FS_TYPE_JFS	   11
#define NETSNMP_FS_TYPE_ISO9660	   12
#define NETSNMP_FS_TYPE_ROCKRIDGE  13
#define NETSNMP_FS_TYPE_NFS	   14
#define NETSNMP_FS_TYPE_NETWARE	   15
#define NETSNMP_FS_TYPE_AFS	   16
#define NETSNMP_FS_TYPE_DFS	   17
#define NETSNMP_FS_TYPE_APPLESHARE 18
#define NETSNMP_FS_TYPE_RFS	   19
#define NETSNMP_FS_TYPE_DGCS	   20
#define NETSNMP_FS_TYPE_BOOTFS	   21
#define NETSNMP_FS_TYPE_FAT32	   22
#define NETSNMP_FS_TYPE_EXT2	   23

   /*
    * Additional enumerationis - not listed in that MIB
    */
#define NETSNMP_FS_TYPE_IGNORE	   1 | _NETSNMP_FS_TYPE_LOCAL | _NETSNMP_FS_TYPE_SKIP_BIT

#define NETSNMP_FS_TYPE_PROC	   2 | _NETSNMP_FS_TYPE_LOCAL | _NETSNMP_FS_TYPE_SKIP_BIT

#define NETSNMP_FS_TYPE_DEVPTS	   3 | _NETSNMP_FS_TYPE_LOCAL | _NETSNMP_FS_TYPE_SKIP_BIT
#define NETSNMP_FS_TYPE_SYSFS	   4 | _NETSNMP_FS_TYPE_LOCAL | _NETSNMP_FS_TYPE_SKIP_BIT
#define NETSNMP_FS_TYPE_TMPFS	   5 | _NETSNMP_FS_TYPE_LOCAL
#define NETSNMP_FS_TYPE_USBFS	   6 | _NETSNMP_FS_TYPE_LOCAL

#define NETSNMP_FS_FLAG_ACTIVE   0x01
#define NETSNMP_FS_FLAG_REMOTE   0x02
#define NETSNMP_FS_FLAG_RONLY    0x04
#define NETSNMP_FS_FLAG_BOOTABLE 0x08
#define NETSNMP_FS_FLAG_REMOVE   0x10
#define NETSNMP_FS_FLAG_UCD      0x20

#define NETSNMP_FS_FIND_CREATE     1   /* or use one of the type values */
#define NETSNMP_FS_FIND_EXIST      0

struct netsnmp_fsys_info_s {
     netsnmp_index  idx;
  /* int  idx; */
 
     char path[  SNMP_MAXPATH+1];
     char device[SNMP_MAXPATH+1];
     int  type;

     unsigned long long size;
     unsigned long long used;
     unsigned long long avail;
     unsigned long long units;

     /* artificially computed values, both 'size_32' and 'units_32' fit INT32 */
     unsigned long size_32;
     unsigned long used_32;
     unsigned long avail_32;
     unsigned long units_32;

     unsigned long long inums_total;
     unsigned long long inums_avail;

     int  minspace;
     int  minpercent;

     long flags;

     netsnmp_fsys_info *next;
};


    /*
     * Possibly not all needed ??
     */
netsnmp_fsys_info *netsnmp_fsys_get_first( void );
netsnmp_fsys_info *netsnmp_fsys_get_next( netsnmp_fsys_info* );
netsnmp_fsys_info *netsnmp_fsys_get_byIdx(  int,   int );
netsnmp_fsys_info *netsnmp_fsys_get_next_byIdx(int,int );

netsnmp_fsys_info *netsnmp_fsys_by_device(  char*, int );
netsnmp_fsys_info *netsnmp_fsys_by_path(    char*, int );

netsnmp_cache *netsnmp_fsys_get_cache( void );
int  netsnmp_fsys_load( netsnmp_cache *cache, void *data );
void netsnmp_fsys_free( netsnmp_cache *cache, void *data );

int netsnmp_fsys_size( netsnmp_fsys_info* );
int netsnmp_fsys_used( netsnmp_fsys_info* );
int netsnmp_fsys_avail(netsnmp_fsys_info* );

unsigned long long netsnmp_fsys_size_ull( netsnmp_fsys_info* );
unsigned long long netsnmp_fsys_used_ull( netsnmp_fsys_info* );
unsigned long long netsnmp_fsys_avail_ull(netsnmp_fsys_info* );

void netsnmp_fsys_calculate32( netsnmp_fsys_info *f);
