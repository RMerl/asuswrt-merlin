#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/cpu.h>

#include <unistd.h>
#include <fcntl.h>

#define CPU_FILE    "/proc/cpuinfo"
#define STAT_FILE   "/proc/stat"
#define VMSTAT_FILE "/proc/vmstat"


    /* Which field(s) describe the type of CPU */
#if defined(__i386__) || defined(__x86_64__)
#define DESCR_FIELD  "vendor_id"
#define DESCR2_FIELD "model name"
#endif
#if defined(__powerpc__) || defined(__powerpc64__)
#define DESCR_FIELD  "cpu\t"
#endif
#if defined(__ia64__)
	/* since vendor is always Intel ... we don't parse vendor */
#define DESCR_FIELD  "family"
#endif


    /*
     * Initialise the list of CPUs on the system
     *   (including descriptions)
     *
     * XXX - Assumes x86-style /proc/cpuinfo format
     *       See CPUinfo database at
     *           http://www.rush3d.com/gcc/
     *                for info on alternative styles
     */
void init_cpu_linux( void ) {
    FILE *fp;
    char buf[1024], *cp;
    int  i, n = 0;
    netsnmp_cpu_info *cpu = netsnmp_cpu_get_byIdx( -1, 1 );
    strcpy(cpu->name, "Overall CPU statistics");

    fp = fopen( CPU_FILE, "r" );
    if (!fp) {
        snmp_log(LOG_ERR, "Can't open procinfo file %s\n", CPU_FILE);
        return;
    }
    while ( fgets( buf, sizeof(buf), fp)) {
        if ( sscanf( buf, "processor : %d", &i ) == 1)  {
            n++;
            cpu = netsnmp_cpu_get_byIdx( i, 1 );
            cpu->status = 2;  /* running */
            sprintf( cpu->name, "cpu%d", i );
#if defined(__s390__) || defined(__s390x__)
            strcat( cpu->descr, "An S/390 CPU" );
#endif
        }
#if defined(__s390__) || defined(__s390x__)
	/* s390 may have different format of CPU_FILE */
        else {
            if (sscanf( buf, "processor %d:", &i ) == 1)  {
                n++;
                cpu = netsnmp_cpu_get_byIdx( i, 1 );
                cpu->status = 2;  /* running */
                sprintf( cpu->name, "cpu%d", i );
                strcat( cpu->descr, "An S/390 CPU" );
            }
        }
#endif

#ifdef DESCR_FIELD
        if (!strncmp( buf, DESCR_FIELD, strlen(DESCR_FIELD))) {
            cp = strchr( buf, ':' );
            strcpy( cpu->descr, cp+2 );
            cp = strchr( cpu->descr, '\n' );
            *cp = 0;
        }
#endif
#ifdef DESCR2_FIELD
        if (!strncmp( buf, DESCR2_FIELD, strlen(DESCR2_FIELD))) {
            cp = strchr( buf, ':' );
            strcat( cpu->descr, cp );
            cp = strchr( cpu->descr, '\n' );
            *cp = 0;
        }
#endif
    }
    fclose(fp);
    cpu_num = n;
}

void _cpu_load_swap_etc( char *buff, netsnmp_cpu_info *cpu );

    /*
     * Load the latest CPU usage statistics
     */
int netsnmp_cpu_arch_load( netsnmp_cache *cache, void *magic ) {
    static char *buff  = NULL;
    static int   bsize = 0;
    static int   first = 1;
    static int   num_cpuline_elem = 0;
    int          bytes_read, statfd, i;
    char        *b1, *b2;
    unsigned long long cusell = 0, cicell = 0, csysll = 0, cidell = 0,
                       ciowll = 0, cirqll = 0, csoftll = 0, cstealll = 0,
                       cguestll = 0, cguest_nicell = 0;
    netsnmp_cpu_info* cpu;

    if ((statfd = open(STAT_FILE, O_RDONLY, 0)) == -1) {
        snmp_log_perror(STAT_FILE);
        return -1;
    }
    if (bsize == 0) {
        bsize = getpagesize()-1;
        buff = (char*)malloc(bsize+1);
    }
    while ((bytes_read = read(statfd, buff, bsize)) == bsize) {
        bsize += BUFSIZ;
        buff = (char*)realloc(buff, bsize+1);
        DEBUGMSGTL(("cpu", "/proc/stat buffer increased to %d\n", bsize));
        close(statfd);
        statfd = open(STAT_FILE, O_RDONLY, 0);
        if (statfd == -1) {
            snmp_log_perror(STAT_FILE);
            return -1;
	}
    }
    close(statfd);

    if ( bytes_read < 0 ) {
        snmp_log_perror(STAT_FILE "read error");
        return -1;
    }
    buff[bytes_read] = '\0';

        /*
         * CPU statistics (overall and per-CPU)
         */
    b1 = buff;
    while ((b2 = strstr( b1, "cpu" ))) {
        if (b2[3] == ' ') {
            cpu = netsnmp_cpu_get_byIdx( -1, 0 );
            if (!cpu) {
                snmp_log_perror("No (overall) CPU info entry");
                return -1;
            }
            b1 = b2+4; /* Skip "cpu " */
        } else {
            sscanf( b2, "cpu%d", &i );
                       /* Create on the fly to support non-x86 systems - see init */
            cpu = netsnmp_cpu_get_byIdx( i, 1 );
            if (!cpu) {
                snmp_log_perror("Missing CPU info entry");
                break;
            }
            b1 = b2+5; /* Skip "cpuN " */
        }

        num_cpuline_elem = sscanf(b1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
         &cusell, &cicell, &csysll, &cidell, &ciowll, &cirqll, &csoftll, &cstealll, &cguestll, &cguest_nicell);
        DEBUGMSGTL(("cpu", "/proc/stat cpu line number of elements: %i\n", num_cpuline_elem));

        /* kernel 2.6.33 and above */
        if (num_cpuline_elem == 10) {
            cpu->guestnice_ticks = (unsigned long long)cguest_nicell;
        }
        /* kernel 2.6.24 and above */
        if (num_cpuline_elem >= 9) {
            cpu->guest_ticks = (unsigned long long)cguestll;
        }
        /* kernel 2.6.11 and above */
        if (num_cpuline_elem >= 8) {
            cpu->steal_ticks = (unsigned long long)cstealll;
        }
        /* kernel 2.6 */
        if (num_cpuline_elem >= 5) {
            cpu->wait_ticks   = (unsigned long long)ciowll;
            cpu->intrpt_ticks = (unsigned long long)cirqll;
            cpu->sirq_ticks   = (unsigned long long)csoftll;
        }
        /* rest */
        cpu->user_ticks = (unsigned long long)cusell;
        cpu->nice_ticks = (unsigned long long)cicell;
        cpu->sys_ticks  = (unsigned long long)csysll;
        cpu->idle_ticks = (unsigned long long)cidell;
    }
    if ( b1 == buff ) {
	if (first)
	    snmp_log(LOG_ERR, "No cpu line in %s\n", STAT_FILE);
    }

        /*
         * Interrupt/Context Switch statistics
         *   XXX - Do these really belong here ?
         */
    cpu = netsnmp_cpu_get_byIdx( -1, 0 );
    _cpu_load_swap_etc( buff, cpu );

    /*
     * XXX - TODO: extract per-CPU statistics
     *    (Into separate netsnmp_cpu_info data structures)
     */

    first = 0;
    return 0;
}


        /*
         * Interrupt/Context Switch statistics
         *   XXX - Do these really belong here ?
         */
void _cpu_load_swap_etc( char *buff, netsnmp_cpu_info *cpu ) {
    static int   has_vmstat = 1;
    static char *vmbuff  = NULL;
    static int   vmbsize = 0;
    static int   first   = 1;
    int          bytes_read, vmstatfd;
    char        *b;
    unsigned long long pin, pout, swpin, swpout;
    unsigned long long itot, iticks, ctx;

    if (has_vmstat) {
      vmstatfd = open(VMSTAT_FILE, O_RDONLY, 0);
      if (vmstatfd == -1 ) {
            snmp_log(LOG_ERR, "cannot open %s\n", VMSTAT_FILE);
            has_vmstat = 0;
      } else {
        if (vmbsize == 0) {
	    vmbsize = getpagesize()-1;
	    vmbuff = (char*)malloc(vmbsize+1);
        }
        while ((bytes_read = read(vmstatfd, vmbuff, vmbsize)) == vmbsize) {
	    vmbsize += BUFSIZ;
	    vmbuff = (char*)realloc(vmbuff, vmbsize+1);
	    close(vmstatfd);
	    vmstatfd = open(VMSTAT_FILE, O_RDONLY, 0);
	    if (vmstatfd == -1) {
                snmp_log_perror("cannot open " VMSTAT_FILE);
                return;
	    }
        }
        close(vmstatfd);
        if ( bytes_read < 0 ) {
            snmp_log_perror(VMSTAT_FILE "read error");
            return;
        }
        vmbuff[bytes_read] = '\0';
      }
    }

    if (has_vmstat) {
	b = strstr(vmbuff, "pgpgin ");
	if (b) {
	    sscanf(b, "pgpgin %llu", &pin);
            cpu->pageIn  = (unsigned long long)pin*2;  /* ??? */
	} else {
	    if (first)
		snmp_log(LOG_ERR, "No pgpgin line in %s\n", VMSTAT_FILE);
            cpu->pageIn  = 0;
	}
	b = strstr(vmbuff, "pgpgout ");
	if (b) {
	    sscanf(b, "pgpgout %llu", &pout);
            cpu->pageOut = (unsigned long long)pout*2;  /* ??? */
	} else {
	    if (first)
		snmp_log(LOG_ERR, "No pgpgout line in %s\n", VMSTAT_FILE);
            cpu->pageOut = 0;
	}
	b = strstr(vmbuff, "pswpin ");
	if (b) {
	    sscanf(b, "pswpin %llu", &swpin);
            cpu->swapIn  = (unsigned long long)swpin;
	} else {
	    if (first)
		snmp_log(LOG_ERR, "No pswpin line in %s\n", VMSTAT_FILE);
            cpu->swapIn  = 0;
	}
	b = strstr(vmbuff, "pswpout ");
	if (b) {
	    sscanf(b, "pswpout %llu", &swpout);
            cpu->swapOut = (unsigned long long)swpout;
	} else {
	    if (first)
		snmp_log(LOG_ERR, "No pswpout line in %s\n", VMSTAT_FILE);
            cpu->swapOut = 0;
	}
    }
    else {
	b = strstr(buff, "page ");
	if (b) {
	    sscanf(b, "page %llu %llu", &pin, &pout);
            cpu->pageIn  = (unsigned long long)pin;
            cpu->pageOut = (unsigned long long)pout;
	} else {
	    if (first)
		snmp_log(LOG_ERR, "No page line in %s\n", STAT_FILE);
            cpu->pageIn  = cpu->pageOut = 0;
	}
	b = strstr(buff, "swap ");
	if (b) {
	    sscanf(b, "swap %llu %llu", &swpin, &swpout);
            cpu->swapIn  = (unsigned long long)swpin;
            cpu->swapOut = (unsigned long long)swpout;
	} else {
	    if (first)
		snmp_log(LOG_ERR, "No swap line in %s\n", STAT_FILE);
            cpu->swapIn  = cpu->swapOut = 0;
	}
    }

    b = strstr(buff, "intr ");
    if (b) {
	sscanf(b, "intr %llu %llu", &itot, &iticks);
        cpu->nInterrupts = (unsigned long long)itot;
        /* iticks not used? */
    } else {
	if (first)
	    snmp_log(LOG_ERR, "No intr line in %s\n", STAT_FILE);
    }
    b = strstr(buff, "ctxt ");
    if (b) {
	sscanf(b, "ctxt %llu", &ctx);
        cpu->nCtxSwitches = (unsigned long long)ctx;
    } else {
	if (first)
	    snmp_log(LOG_ERR, "No ctxt line in %s\n", STAT_FILE);
    }
    first = 0;
}

