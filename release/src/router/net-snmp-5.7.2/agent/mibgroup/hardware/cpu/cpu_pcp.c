/*
 *   pcp interface
 *     e.g. IRIX
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/cpu.h>

#include <unistd.h>
#include <pcp/pmapi.h>

/*
 * Performance Metrics Name Space Map
 * Built by pmgenmap from the file irix_kernstats.pcp
 */
char *kernstats[] = {
#define NCPU	0
	"hinv.ncpu",
#define CPUTYPE	1
	"hinv.cputype",
#define CPUIDLE	2
	"kernel.all.cpu.idle",
#define CPUINTR	3
	"kernel.all.cpu.intr",
#define CPUSYS	4
	"kernel.all.cpu.sys",
#define CPUUSER	5
	"kernel.all.cpu.user",
#define CPUWAIT	6
	"kernel.all.cpu.wait.total",
#define PAGESIN	7
	"swap.pagesin",
#define PAGESOUT	8
	"swap.pagesout",
#define SWAPIN	9
	"swap.in",
#define SWAPOUT	10
	"swap.out",
#define INTR	11
	"kernel.all.intr.non_vme",
#define CTXT	12
	"kernel.all.kswitch"
};

# define MAX_MID 17

pmResult *resp;
pmID pmidlist[MAX_MID];
int numpmid;
int pmInitDone = 0;

/* initialize pcp if necessary */
void init_pcp () {
    int err;

    if (pmInitDone == 1) {
       return;
    }

    snmp_log_perror("Initializing pcp");
    numpmid = sizeof(kernstats)/sizeof(kernstats[0]);

    /* Load default namespace */
    if ((err=pmLoadNameSpace(PM_NS_DEFAULT)) < 0) {
       snmp_log_perror("pmLoadNameSpace returned an error.");
       snmp_log_perror(pmErrStr(err));
       exit (1);
    }
 
    /* get mappings between internal IDs and external IDs */
    if ((err=pmLookupName(numpmid, kernstats, pmidlist)) < 0) {
       snmp_log_perror("pmLookupName returned an error.");
       snmp_log_perror(pmErrStr(err));
       exit (1);
    }
 
    /* specify a context to use */
    /* a type of PM_CONTEXT_HOST lets you specify a hostname */
    /* a type of PM_CONTEXT_LOCAL should ignore the string param */
    if ((err=pmNewContext(PM_CONTEXT_LOCAL,"localhost")) < 0) {
       snmp_log_perror("pmNewContext returned error opening a LOCAL Context");
       snmp_log_perror(pmErrStr(err));
 
       if ((err=pmNewContext(PM_CONTEXT_HOST,"localhost")) < 0) {
          snmp_log_perror("pmNewContext returned error opening a HOST Context");
          snmp_log_perror(pmErrStr(err));
          exit(1);
       }
    }
    snmp_log_perror ("done initializing pcp");
    pmInitDone = 1;
}

    /*
     * Initialise the list of CPUs on the system
     *   (including descriptions)
     */
void init_cpu_pcp( void ) {
    int  i, n = 0;
    char tstr[1024];
    int err;
    netsnmp_cpu_info *cpu;

    init_pcp();

    /* At this stage, pmidlist contains the PMID for my metrics of interest */

    cpu = netsnmp_cpu_get_byIdx( -1, 1 );
    strcpy(cpu->name, "Overall CPU statistics");

    if ((err=pmFetch(numpmid, pmidlist, &resp)) < 0) {
       snmp_log_perror ("init_cpu_pcp: pmFetch returned error");
       snmp_log_perror (pmErrStr(err));
       exit (1);
    }
    cpu_num = resp->vset[NCPU]->vlist[0].value.lval;
    pmFreeResult(resp);

    for (i=0; i<cpu_num ; i++) {
       cpu = netsnmp_cpu_get_byIdx( i, 1 );
       sprintf(tstr, "cpu%d",i);
       strcpy(cpu->name,  tstr);
       strcpy(cpu->descr, "An electronic chip that makes the computer work");
    }
}

/*void _cpu_load_swap_etc( char *buff, netsnmp_cpu_info *cpu );*/

    /*
     * Load the latest CPU usage statistics
     */
int netsnmp_cpu_arch_load( netsnmp_cache *cache, void *magic ) {
    int err;
    /*static char *buff  = NULL;*/
    static int   first = 1;
    netsnmp_cpu_info* cpu;

    init_pcp();

        /*
         * CPU statistics (overall and per-CPU)
         */
    if ((err=pmFetch(numpmid, pmidlist, &resp)) < 0) {
       snmp_log_perror ("netsnmp_cpu_arch_load: pmFetch returned an error.");
       snmp_log_perror (pmErrStr(err));
       exit (1);
    }

    cpu = netsnmp_cpu_get_byIdx( -1, 0 );
    if (!cpu) {
       snmp_log_perror ("netsnmp_cpu_arch_load: netsnmp_cpu_get_byIdx failed!");
       exit(1);
    }

    cpu->wait_ticks   = (unsigned long long)resp->vset[CPUWAIT]->vlist[0].value.lval / 10;
    cpu->intrpt_ticks = (unsigned long long)resp->vset[CPUINTR]->vlist[0].value.lval / 10;
    /*cpu->sirq_ticks   = (unsigned long)csoftll / 10;*/
    cpu->user_ticks = (unsigned long long)resp->vset[CPUUSER]->vlist[0].value.lval / 10;
    /*cpu->nice_ticks = (unsigned long)cicell / 10;*/
    cpu->sys_ticks  = (unsigned long long)resp->vset[CPUSYS]->vlist[0].value.lval / 10;
    cpu->idle_ticks = (unsigned long long)resp->vset[CPUIDLE]->vlist[0].value.lval / 10;


        /*
         * Interrupt/Context Switch statistics
         *   XXX - Do these really belong here ?
         */
    /*cpu = netsnmp_cpu_get_byIdx( -1, 0 );*/
    /*_cpu_load_swap_etc( buff, cpu );*/
    cpu->pageIn  = (unsigned long long)resp->vset[PAGESIN]->vlist[0].value.lval;
    cpu->pageOut = (unsigned long long)resp->vset[PAGESOUT]->vlist[0].value.lval;
    cpu->swapIn  = (unsigned long long)resp->vset[SWAPIN]->vlist[0].value.lval;
    cpu->swapOut = (unsigned long long)resp->vset[SWAPOUT]->vlist[0].value.lval;
    cpu->nInterrupts = (unsigned long long)resp->vset[INTR]->vlist[0].value.lval;
    cpu->nCtxSwitches = (unsigned long long)resp->vset[CTXT]->vlist[0].value.lval;

    /*
     * XXX - TODO: extract per-CPU statistics
     *    (Into separate netsnmp_cpu_info data structures)
     */

    /* free pcp response */
    pmFreeResult(resp);

    first = 0;
    return 0;
}



