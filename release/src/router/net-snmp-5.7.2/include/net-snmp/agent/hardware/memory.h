typedef struct netsnmp_memory_info_s netsnmp_memory_info;

#define NETSNMP_MEM_TYPE_PHYSMEM  1
#define NETSNMP_MEM_TYPE_USERMEM  2
#define NETSNMP_MEM_TYPE_VIRTMEM  3
#define NETSNMP_MEM_TYPE_STEXT    4
#define NETSNMP_MEM_TYPE_RTEXT    5
#define NETSNMP_MEM_TYPE_MBUF     6
#define NETSNMP_MEM_TYPE_CACHED   7
#define NETSNMP_MEM_TYPE_SHARED   8
#define NETSNMP_MEM_TYPE_SHARED2  9
#define NETSNMP_MEM_TYPE_SWAP    10
    /* Leave space for individual swap devices */
#define NETSNMP_MEM_TYPE_MAX     30

struct netsnmp_memory_info_s {
     int  idx;
     int  type;
     char *descr;

     long units;
     long size;
     long free;
     long other;

     netsnmp_memory_info *next;
};


    /*
     * Possibly not all needed ??
     */
netsnmp_memory_info *netsnmp_memory_get_first(  int );
netsnmp_memory_info *netsnmp_memory_get_next( netsnmp_memory_info*, int );
netsnmp_memory_info *netsnmp_memory_get_byIdx(  int,   int );
netsnmp_memory_info *netsnmp_memory_get_next_byIdx(int,int );

netsnmp_cache *netsnmp_memory_get_cache( void );
int netsnmp_memory_load( void );
