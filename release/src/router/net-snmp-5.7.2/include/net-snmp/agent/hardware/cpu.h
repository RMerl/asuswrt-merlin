typedef struct netsnmp_cpu_info_s netsnmp_cpu_info;
extern int cpu_num;

                 /* For rolling averages */
struct netsnmp_cpu_history {
     unsigned long long user_hist;
     unsigned long long sys_hist;
     unsigned long long idle_hist;
     unsigned long long nice_hist;
     unsigned long long total_hist;

     unsigned long long ctx_hist;
     unsigned long long intr_hist;
     unsigned long long swpi_hist;
     unsigned long long swpo_hist;
     unsigned long long pagei_hist;
     unsigned long long pageo_hist;
};

struct netsnmp_cpu_info_s {
     int  idx;
                 /* For hrDeviceTable */
     char name[  SNMP_MAXBUF ];
     char descr[ SNMP_MAXBUF ];
     int  status;

                 /* For UCD cpu stats */
     unsigned long long user_ticks;
     unsigned long long nice_ticks;
     unsigned long long sys_ticks;
     unsigned long long idle_ticks;
     unsigned long long wait_ticks;
     unsigned long long kern_ticks;
     unsigned long long intrpt_ticks;
     unsigned long long sirq_ticks;
     unsigned long long steal_ticks;
     unsigned long long guest_ticks;
     unsigned long long guestnice_ticks;

     unsigned long long total_ticks;
     unsigned long long sys2_ticks;  /* For non-atomic system counts */

                 /* For paging-related UCD stats */
              /* XXX - Do these belong elsewhere ?? */
              /* XXX - Do Not Use - Subject to Change */
     unsigned long long pageIn;
     unsigned long long pageOut;
     unsigned long long swapIn;
     unsigned long long swapOut;
     unsigned long long nInterrupts;
     unsigned long long nCtxSwitches;

     struct netsnmp_cpu_history *history;

     netsnmp_cpu_info *next;
};


    /*
     * Possibly not all needed ??
     */
netsnmp_cpu_info *netsnmp_cpu_get_first(  void );
netsnmp_cpu_info *netsnmp_cpu_get_next( netsnmp_cpu_info* );
netsnmp_cpu_info *netsnmp_cpu_get_byIdx(  int,   int );
netsnmp_cpu_info *netsnmp_cpu_get_byName( char*, int );

netsnmp_cache *netsnmp_cpu_get_cache( void );
int netsnmp_cpu_load( void );
