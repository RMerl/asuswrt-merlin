// old: "Copyright 1994 by Henry Ware <al172@yfn.ysu.edu>. Copyleft same year."
// most code copyright 2002 Albert Cahalan
// 
// 27/05/2003 (Fabian Frederick) : Add unit conversion + interface
//               	 Export proc/stat access to libproc
//			 Adapt vmstat helpfile
// 31/05/2003 (Fabian) : Add diskstat support (/libproc)
// June 2003 (Fabian) : -S <x> -s & -s -S <x> patch
// June 2003 (Fabian) : -Adding diskstat against 3.1.9, slabinfo
//			 -patching 'header' in disk & slab
// July 2003 (Fabian) : -Adding disk partition output
//			-Adding disk table
//			-Syncing help / usage

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/dir.h>
#include <dirent.h>

#include "proc/sysinfo.h"
#include "proc/version.h"

static unsigned long dataUnit=1024;
static char szDataUnit [16];
#define UNIT_B        1
#define UNIT_k        1000
#define UNIT_K        1024
#define UNIT_m        1000000
#define UNIT_M        1048576

#define VMSTAT        0
#define DISKSTAT      0x00000001
#define VMSUMSTAT     0x00000002
#define SLABSTAT      0x00000004
#define PARTITIONSTAT 0x00000008
#define DISKSUMSTAT   0x00000010

static int statMode=VMSTAT;

#define FALSE 0
#define TRUE 1

static int a_option; /* "-a" means "show active/inactive" */

static unsigned sleep_time = 1;
static unsigned long num_updates;

static unsigned int height;   // window height
static unsigned int moreheaders=TRUE;


/////////////////////////////////////////////////////////////////////////

static void usage(void) NORETURN;
static void usage(void) {
  fprintf(stderr,"usage: vmstat [-V] [-n] [delay [count]]\n");
  fprintf(stderr,"              -V prints version.\n");
  fprintf(stderr,"              -n causes the headers not to be reprinted regularly.\n");
  fprintf(stderr,"              -a print inactive/active page stats.\n");
  fprintf(stderr,"              -d prints disk statistics\n");
  fprintf(stderr,"              -D prints disk table\n");
  fprintf(stderr,"              -p prints disk partition statistics\n");
  fprintf(stderr,"              -s prints vm table\n");
  fprintf(stderr,"              -m prints slabinfo\n");
  fprintf(stderr,"              -S unit size\n");
  fprintf(stderr,"              delay is the delay between updates in seconds. \n");
  fprintf(stderr,"              unit size k:1000 K:1024 m:1000000 M:1048576 (default is K)\n");
  fprintf(stderr,"              count is the number of updates.\n");
  exit(EXIT_FAILURE);
}

/////////////////////////////////////////////////////////////////////////////

#if 0
// produce:  "  6  ", "123  ", "123k ", etc.
static int format_1024(unsigned long long val64, char *restrict dst){
  unsigned oldval;
  const char suffix[] = " kmgtpe";
  unsigned level = 0;
  unsigned val32;

  if(val64 < 1000){   // special case to avoid "6.0  " when plain "  6  " would do
    val32 = val64;
    return sprintf(dst,"%3u  ",val32);
  }

  while(val64 > 0xffffffffull){
    level++;
    val64 /= 1024;
  }

  val32 = val64;

  while(val32 > 999){
    level++;
    oldval = val32;
    val32 /= 1024;
  }

  if(val32 < 10){
    unsigned fract = (oldval % 1024) * 10 / 1024;
    return sprintf(dst, "%u.%u%c ", val32, fract, suffix[level]);
  }
  return sprintf(dst, "%3u%c ", val32, suffix[level]);
}


// produce:  "  6  ", "123  ", "123k ", etc.
static int format_1000(unsigned long long val64, char *restrict dst){
  unsigned oldval;
  const char suffix[] = " kmgtpe";
  unsigned level = 0;
  unsigned val32;

  if(val64 < 1000){   // special case to avoid "6.0  " when plain "  6  " would do
    val32 = val64;
    return sprintf(dst,"%3u  ",val32);
  }

  while(val64 > 0xffffffffull){
    level++;
    val64 /= 1000;
  }

  val32 = val64;

  while(val32 > 999){
    level++;
    oldval = val32;
    val32 /= 1000;
  }

  if(val32 < 10){
    unsigned fract = (oldval % 1000) / 100;
    return sprintf(dst, "%u.%u%c ", val32, fract, suffix[level]);
  }
  return sprintf(dst, "%3u%c ", val32, suffix[level]);
}
#endif

////////////////////////////////////////////////////////////////////////////

static void new_header(void){
  printf("procs -----------memory---------- ---swap-- -----io---- -system-- ----cpu----\n");
  printf(
    "%2s %2s %6s %6s %6s %6s %4s %4s %5s %5s %4s %4s %2s %2s %2s %2s\n",
    "r","b",
    "swpd", "free", a_option?"inact":"buff", a_option?"active":"cache",
    "si","so",
    "bi","bo",
    "in","cs",
    "us","sy","id","wa"
  );
}

////////////////////////////////////////////////////////////////////////////

static unsigned long unitConvert(unsigned int size){
 float cvSize;
 cvSize=(float)size/dataUnit*((statMode==SLABSTAT)?1:1024);
 return ((unsigned long) cvSize);
}

////////////////////////////////////////////////////////////////////////////

static void new_format(void) {
  const char format[]="%2u %2u %6lu %6lu %6lu %6lu %4u %4u %5u %5u %4u %4u %2u %2u %2u %2u\n";
  unsigned int tog=0; /* toggle switch for cleaner code */
  unsigned int i;
  unsigned int hz = Hertz;
  unsigned int running,blocked,dummy_1,dummy_2;
  jiff cpu_use[2], cpu_nic[2], cpu_sys[2], cpu_idl[2], cpu_iow[2], cpu_xxx[2], cpu_yyy[2], cpu_zzz[2];
  jiff duse, dsys, didl, diow, dstl, Div, divo2;
  unsigned long pgpgin[2], pgpgout[2], pswpin[2], pswpout[2];
  unsigned int intr[2], ctxt[2];
  unsigned int sleep_half; 
  unsigned long kb_per_page = sysconf(_SC_PAGESIZE) / 1024ul;
  int debt = 0;  // handle idle ticks running backwards

  sleep_half=(sleep_time/2);
  new_header();
  meminfo();

  getstat(cpu_use,cpu_nic,cpu_sys,cpu_idl,cpu_iow,cpu_xxx,cpu_yyy,cpu_zzz,
	  pgpgin,pgpgout,pswpin,pswpout,
	  intr,ctxt,
	  &running,&blocked,
	  &dummy_1, &dummy_2);

  duse= *cpu_use + *cpu_nic; 
  dsys= *cpu_sys + *cpu_xxx + *cpu_yyy;
  didl= *cpu_idl;
  diow= *cpu_iow;
  dstl= *cpu_zzz;
  Div= duse+dsys+didl+diow+dstl;
  divo2= Div/2UL;
  printf(format,
	 running, blocked,
	 unitConvert(kb_swap_used), unitConvert(kb_main_free),
	 unitConvert(a_option?kb_inactive:kb_main_buffers),
	 unitConvert(a_option?kb_active:kb_main_cached),
	 (unsigned)( (*pswpin  * unitConvert(kb_per_page) * hz + divo2) / Div ),
	 (unsigned)( (*pswpout * unitConvert(kb_per_page) * hz + divo2) / Div ),
	 (unsigned)( (*pgpgin                * hz + divo2) / Div ),
	 (unsigned)( (*pgpgout               * hz + divo2) / Div ),
	 (unsigned)( (*intr                  * hz + divo2) / Div ),
	 (unsigned)( (*ctxt                  * hz + divo2) / Div ),
	 (unsigned)( (100*duse                    + divo2) / Div ),
	 (unsigned)( (100*dsys                    + divo2) / Div ),
	 (unsigned)( (100*didl                    + divo2) / Div ),
	 (unsigned)( (100*diow                    + divo2) / Div ) /* ,
	 (unsigned)( (100*dstl                    + divo2) / Div ) */
  );

  for(i=1;i<num_updates;i++) { /* \\\\\\\\\\\\\\\\\\\\ main loop ////////////////// */
    sleep(sleep_time);
    if (moreheaders && ((i%height)==0)) new_header();
    tog= !tog;

    meminfo();

    getstat(cpu_use+tog,cpu_nic+tog,cpu_sys+tog,cpu_idl+tog,cpu_iow+tog,cpu_xxx+tog,cpu_yyy+tog,cpu_zzz+tog,
	  pgpgin+tog,pgpgout+tog,pswpin+tog,pswpout+tog,
	  intr+tog,ctxt+tog,
	  &running,&blocked,
	  &dummy_1,&dummy_2);

    duse= cpu_use[tog]-cpu_use[!tog] + cpu_nic[tog]-cpu_nic[!tog];
    dsys= cpu_sys[tog]-cpu_sys[!tog] + cpu_xxx[tog]-cpu_xxx[!tog] + cpu_yyy[tog]-cpu_yyy[!tog];
    didl= cpu_idl[tog]-cpu_idl[!tog];
    diow= cpu_iow[tog]-cpu_iow[!tog];
    dstl= cpu_zzz[tog]-cpu_zzz[!tog];

    /* idle can run backwards for a moment -- kernel "feature" */
    if(debt){
      didl = (int)didl + debt;
      debt = 0;
    }
    if( (int)didl < 0 ){
      debt = (int)didl;
      didl = 0;
    }

    Div= duse+dsys+didl+diow+dstl;
    divo2= Div/2UL;
    printf(format,
           running, blocked,
	   unitConvert(kb_swap_used),unitConvert(kb_main_free),
	   unitConvert(a_option?kb_inactive:kb_main_buffers),
	   unitConvert(a_option?kb_active:kb_main_cached),
	   (unsigned)( ( (pswpin [tog] - pswpin [!tog])*unitConvert(kb_per_page)+sleep_half )/sleep_time ), /*si*/
	   (unsigned)( ( (pswpout[tog] - pswpout[!tog])*unitConvert(kb_per_page)+sleep_half )/sleep_time ), /*so*/
	   (unsigned)( (  pgpgin [tog] - pgpgin [!tog]             +sleep_half )/sleep_time ), /*bi*/
	   (unsigned)( (  pgpgout[tog] - pgpgout[!tog]             +sleep_half )/sleep_time ), /*bo*/
	   (unsigned)( (  intr   [tog] - intr   [!tog]             +sleep_half )/sleep_time ), /*in*/
	   (unsigned)( (  ctxt   [tog] - ctxt   [!tog]             +sleep_half )/sleep_time ), /*cs*/
	   (unsigned)( (100*duse+divo2)/Div ), /*us*/
	   (unsigned)( (100*dsys+divo2)/Div ), /*sy*/
	   (unsigned)( (100*didl+divo2)/Div ), /*id*/
	   (unsigned)( (100*diow+divo2)/Div )/*, //wa
	   (unsigned)( (100*dstl+divo2)/Div )  //st  */
    );
  }
}

////////////////////////////////////////////////////////////////////////////

static void diskpartition_header(const char *partition_name){
  printf("%-10s %10s %10s %10s %10s\n",partition_name, "reads  ", "read sectors", "writes   ", "requested writes");
}

////////////////////////////////////////////////////////////////////////////

static int diskpartition_format(const char* partition_name){
    FILE *fDiskstat;
    struct disk_stat *disks;
    struct partition_stat *partitions, *current_partition=NULL;
    unsigned long ndisks, j, k, npartitions;
    const char format[] = "%20u %10llu %10u %10u\n";

    fDiskstat=fopen("/proc/diskstats","rb");
    if(!fDiskstat){
        fprintf(stderr, "Your kernel doesn't support diskstat. (2.5.70 or above required)\n"); 
        exit(EXIT_FAILURE);
    }

    fclose(fDiskstat);
    ndisks=getdiskstat(&disks,&partitions);
    npartitions=getpartitions_num(disks, ndisks);
    for(k=0; k<npartitions; k++){
       if(!strcmp(partition_name, partitions[k].partition_name)){
                current_partition=&(partitions[k]); 
       }	
    }
    if(!current_partition){
         return -1;
    }
    diskpartition_header(partition_name);
    printf (format,
       current_partition->reads,current_partition->reads_sectors,current_partition->writes,current_partition->requested_writes);
    fflush(stdout);
    free(disks);
    free(partitions);
    for(j=1; j<num_updates; j++){ 
        if (moreheaders && ((j%height)==0)) diskpartition_header(partition_name);
        sleep(sleep_time);
        ndisks=getdiskstat(&disks,&partitions);
        npartitions=getpartitions_num(disks, ndisks);
	current_partition=NULL;
        for(k=0; k<npartitions; k++){
          if(!strcmp(partition_name, partitions[k].partition_name)){
                  current_partition=&(partitions[k]); 
          }	
        }
        if(!current_partition){
           return -1;
        }
        printf (format,
        current_partition->reads,current_partition->reads_sectors,current_partition->writes,current_partition->requested_writes);
        fflush(stdout);
        free(disks);
        free(partitions);
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////

static void diskheader(void){
  printf("disk- ------------reads------------ ------------writes----------- -----IO------\n");

  printf("%5s %6s %6s %7s %7s %6s %6s %7s %7s %6s %6s\n",
         " ", "total", "merged","sectors","ms","total","merged","sectors","ms","cur","sec");

}

////////////////////////////////////////////////////////////////////////////

static void diskformat(void){
  FILE *fDiskstat;
  struct disk_stat *disks;
  struct partition_stat *partitions;
  unsigned long ndisks,i,j,k;
  const char format[]="%-5s %6u %6u %7llu %7u %6u %6u %7llu %7u %6u %6u\n";
  if ((fDiskstat=fopen("/proc/diskstats", "rb"))){
    fclose(fDiskstat);
    ndisks=getdiskstat(&disks,&partitions);
    for(k=0; k<ndisks; k++){
      if (moreheaders && ((k%height)==0)) diskheader();
      printf(format,
        disks[k].disk_name,
        disks[k].reads,
        disks[k].merged_reads,
        disks[k].reads_sectors,
        disks[k].milli_reading,
        disks[k].writes,
        disks[k].merged_writes,
        disks[k].written_sectors,
        disks[k].milli_writing,
        disks[k].inprogress_IO?disks[k].inprogress_IO/1000:0,
        disks[k].milli_spent_IO?disks[k].milli_spent_IO/1000:0/*,
        disks[i].weighted_milli_spent_IO/1000*/
      );
      fflush(stdout);
    }
    free(disks);
    free(partitions);
    for(j=1; j<num_updates; j++){ 
      sleep(sleep_time);
      ndisks=getdiskstat(&disks,&partitions);
      for(i=0; i<ndisks; i++,k++){
        if (moreheaders && ((k%height)==0)) diskheader();
        printf(format,
          disks[i].disk_name,
          disks[i].reads,
          disks[i].merged_reads,
          disks[i].reads_sectors,
          disks[i].milli_reading,
          disks[i].writes,
          disks[i].merged_writes,
          disks[i].written_sectors,
          disks[i].milli_writing,
          disks[i].inprogress_IO?disks[i].inprogress_IO/1000:0,
          disks[i].milli_spent_IO?disks[i].milli_spent_IO/1000:0/*,
          disks[i].weighted_milli_spent_IO/1000*/
        );
        fflush(stdout);
      }
      free(disks);
      free(partitions);
    }
  }else{
    fprintf(stderr, "Your kernel doesn't support diskstat (2.5.70 or above required)\n"); 
    exit(EXIT_FAILURE);
  } 
}

////////////////////////////////////////////////////////////////////////////

static void slabheader(void){
  printf("%-24s %6s %6s %6s %6s\n","Cache","Num", "Total", "Size", "Pages");
}

////////////////////////////////////////////////////////////////////////////

static void slabformat (void){
  FILE *fSlab;
  struct slab_cache *slabs;
  unsigned long nSlab,i,j,k;
  const char format[]="%-24s %6u %6u %6u %6u\n";

  fSlab=fopen("/proc/slabinfo", "rb");
  if(!fSlab){
    fprintf(stderr, "Your kernel doesn't support slabinfo.\n");    
    return;
  }

  nSlab = getslabinfo(&slabs);
  for(k=0; k<nSlab; k++){
    if (moreheaders && ((k%height)==0)) slabheader();
    printf(format,
      slabs[k].name,
      slabs[k].active_objs,
      slabs[k].num_objs,
      slabs[k].objsize,
      slabs[k].objperslab
    );
  }
  free(slabs);
  for(j=1,k=1; j<num_updates; j++) { 
    sleep(sleep_time);
    nSlab = getslabinfo(&slabs);
    for(i=0; i<nSlab; i++,k++){
      if (moreheaders && ((k%height)==0)) slabheader();
      printf(format,
        slabs[i].name,
        slabs[i].active_objs,
        slabs[i].num_objs,
        slabs[i].objsize,
        slabs[i].objperslab
      );
    }
    free(slabs);
  }
}

////////////////////////////////////////////////////////////////////////////

static void disksum_format(void) {

  FILE *fDiskstat;
  struct disk_stat *disks;
  struct partition_stat *partitions;
  int ndisks, i;
  unsigned long reads, merged_reads, read_sectors, milli_reading, writes,
                merged_writes, written_sectors, milli_writing, inprogress_IO,
                milli_spent_IO, weighted_milli_spent_IO;

  reads=merged_reads=read_sectors=milli_reading=writes=merged_writes= \
  written_sectors=milli_writing=inprogress_IO=milli_spent_IO= \
  weighted_milli_spent_IO=0;

  if ((fDiskstat=fopen("/proc/diskstats", "rb"))){
    fclose(fDiskstat);
    ndisks=getdiskstat(&disks, &partitions);
    printf("%13d disks \n", ndisks);
    printf("%13d partitions \n", getpartitions_num(disks, ndisks));

    for(i=0; i<ndisks; i++){
         reads+=disks[i].reads;
         merged_reads+=disks[i].merged_reads;
         read_sectors+=disks[i].reads_sectors;
         milli_reading+=disks[i].milli_reading;
         writes+=disks[i].writes;
         merged_writes+=disks[i].merged_writes;
         written_sectors+=disks[i].written_sectors;
         milli_writing+=disks[i].milli_writing;
         inprogress_IO+=disks[i].inprogress_IO?disks[i].inprogress_IO/1000:0;
         milli_spent_IO+=disks[i].milli_spent_IO?disks[i].milli_spent_IO/1000:0;
      }

    printf("%13lu total reads\n",reads);
    printf("%13lu merged reads\n",merged_reads);
    printf("%13lu read sectors\n",read_sectors);
    printf("%13lu milli reading\n",milli_reading);
    printf("%13lu writes\n",writes);
    printf("%13lu merged writes\n",merged_writes);
    printf("%13lu written sectors\n",written_sectors);
    printf("%13lu milli writing\n",milli_writing);
    printf("%13lu inprogress IO\n",inprogress_IO);
    printf("%13lu milli spent IO\n",milli_spent_IO);

    free(disks);
    free(partitions);
  }
}

////////////////////////////////////////////////////////////////////////////

static void sum_format(void) {
  unsigned int running, blocked, btime, processes;
  jiff cpu_use, cpu_nic, cpu_sys, cpu_idl, cpu_iow, cpu_xxx, cpu_yyy, cpu_zzz;
  unsigned long pgpgin, pgpgout, pswpin, pswpout;
  unsigned int intr, ctxt;

  meminfo();

  getstat(&cpu_use, &cpu_nic, &cpu_sys, &cpu_idl,
          &cpu_iow, &cpu_xxx, &cpu_yyy, &cpu_zzz,
	  &pgpgin, &pgpgout, &pswpin, &pswpout,
	  &intr, &ctxt,
	  &running, &blocked,
	  &btime, &processes);

  printf("%13lu %s total memory\n", unitConvert(kb_main_total),szDataUnit);
  printf("%13lu %s used memory\n", unitConvert(kb_main_used),szDataUnit);
  printf("%13lu %s active memory\n", unitConvert(kb_active),szDataUnit);
  printf("%13lu %s inactive memory\n", unitConvert(kb_inactive),szDataUnit);
  printf("%13lu %s free memory\n", unitConvert(kb_main_free),szDataUnit);
  printf("%13lu %s buffer memory\n", unitConvert(kb_main_buffers),szDataUnit);
  printf("%13lu %s swap cache\n", unitConvert(kb_main_cached),szDataUnit);
  printf("%13lu %s total swap\n", unitConvert(kb_swap_total),szDataUnit);
  printf("%13lu %s used swap\n", unitConvert(kb_swap_used),szDataUnit);
  printf("%13lu %s free swap\n", unitConvert(kb_swap_free),szDataUnit);
  printf("%13Lu non-nice user cpu ticks\n", cpu_use);
  printf("%13Lu nice user cpu ticks\n", cpu_nic);
  printf("%13Lu system cpu ticks\n", cpu_sys);
  printf("%13Lu idle cpu ticks\n", cpu_idl);
  printf("%13Lu IO-wait cpu ticks\n", cpu_iow);
  printf("%13Lu IRQ cpu ticks\n", cpu_xxx);
  printf("%13Lu softirq cpu ticks\n", cpu_yyy);
  printf("%13Lu stolen cpu ticks\n", cpu_zzz);
  printf("%13lu pages paged in\n", pgpgin);
  printf("%13lu pages paged out\n", pgpgout);
  printf("%13lu pages swapped in\n", pswpin);
  printf("%13lu pages swapped out\n", pswpout);
  printf("%13u interrupts\n", intr);
  printf("%13u CPU context switches\n", ctxt);
  printf("%13u boot time\n", btime);
  printf("%13u forks\n", processes);
}

////////////////////////////////////////////////////////////////////////////

static void fork_format(void) {
  unsigned int running, blocked, btime, processes;
  jiff cpu_use, cpu_nic, cpu_sys, cpu_idl, cpu_iow, cpu_xxx, cpu_yyy, cpu_zzz;
  unsigned long pgpgin, pgpgout, pswpin, pswpout;
  unsigned int intr, ctxt;

  getstat(&cpu_use, &cpu_nic, &cpu_sys, &cpu_idl,
	  &cpu_iow, &cpu_xxx, &cpu_yyy, &cpu_zzz,
	  &pgpgin, &pgpgout, &pswpin, &pswpout,
	  &intr, &ctxt,
	  &running, &blocked,
	  &btime, &processes);

  printf("%13u forks\n", processes);
}

////////////////////////////////////////////////////////////////////////////

static int winhi(void) {
    struct winsize win;
    int rows = 24;
 
    if (ioctl(1, TIOCGWINSZ, &win) != -1 && win.ws_row > 0)
      rows = win.ws_row;
 
    return rows;
}

////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
  char partition[16];
  argc=0; /* redefined as number of integer arguments */
  for (argv++;*argv;argv++) {
    if ('-' ==(**argv)) {
      switch (*(++(*argv))) {
    
      case 'V':
	display_version();
	exit(0);
      case 'd':
	statMode |= DISKSTAT;
	break;
      case 'a':
	/* active/inactive mode */
	a_option=1;
        break;
      case 'f':
        // FIXME: check for conflicting args
	fork_format();
        exit(0);
      case 'm':
        statMode |= SLABSTAT; 	
	break;
      case 'D':
        statMode |= DISKSUMSTAT; 	
	break;
      case 'n':
	/* print only one header */
	moreheaders=FALSE;
        break;
      case 'p':
        statMode |= PARTITIONSTAT;
	if (argv[1]){
	  char *cp = *++argv;
	  if(!memcmp(cp,"/dev/",5)) cp += 5;
	  snprintf(partition, sizeof partition, "%s", cp);
	}else{
	  fprintf(stderr, "-p requires an argument\n");
          exit(EXIT_FAILURE);
	}
        break;
      case 'S':
	if (argv[1]){
	      ++argv;
	 	if (!strcmp(*argv, "k")) dataUnit=UNIT_k;
	 	else if (!strcmp(*argv, "K")) dataUnit=UNIT_K;
	 	else if (!strcmp(*argv, "m")) dataUnit=UNIT_m;
	 	else if (!strcmp(*argv, "M")) dataUnit=UNIT_M;
		else {fprintf(stderr, "-S requires k, K, m or M (default is kb)\n");
		     exit(EXIT_FAILURE);
		}
		strcpy(szDataUnit, *argv);
	 }else {fprintf(stderr, "-S requires an argument\n");
		exit(EXIT_FAILURE);
	 }
	break;
      case 's':
        statMode |= VMSUMSTAT; 	
	break;
      default:
	/* no other aguments defined yet. */
	usage();
      }
   }else{
      argc++;
      switch (argc) {
      case 1:
        if ((sleep_time = atoi(*argv)) == 0)
         usage();
       num_updates = ULONG_MAX;
       break;
      case 2:
        num_updates = atol(*argv);
       break;
      default:
       usage();
      } /* switch */
  }
}
  if (moreheaders) {
      int tmp=winhi()-3;
      height=((tmp>0)?tmp:22);
  }    
  setlinebuf(stdout);
  switch(statMode){
	case(VMSTAT):        new_format();
			     break;
	case(VMSUMSTAT):     sum_format();
			     break;
	case(DISKSTAT):      diskformat();
			     break;
	case(PARTITIONSTAT): if(diskpartition_format(partition)==-1)
                                  printf("Partition was not found\n");
			     break;	
	case(SLABSTAT):      slabformat();
			     break;
	case(DISKSUMSTAT):   disksum_format();  
			     break;	
	default:	     usage();
			     break;
  }
  return 0;
}


