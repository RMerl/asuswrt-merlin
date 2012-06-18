/*
 * LICENSE NOTICE.
 *
 * Use of the Microsoft Windows Rally Development Kit is covered under
 * the Microsoft Windows Rally Development Kit License Agreement,
 * which is provided within the Microsoft Windows Rally Development
 * Kit or at http://www.microsoft.com/whdc/rally/rallykit.mspx. If you
 * want a license from Microsoft to use the software in the Microsoft
 * Windows Rally Development Kit, you must (1) complete the designated
 * "licensee" information in the Windows Rally Development Kit License
 * Agreement, and (2) sign and return the Agreement AS IS to Microsoft
 * at the address provided in the Agreement.
 */

/*
 * Copyright (c) Microsoft Corporation 2005.  All rights reserved.
 * This software is provided with NO WARRANTY.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define	DECLARING_GLOBALS
#include "globals.h"

#include "statemachines.h"
#include "packetio.h"


/************************* C - T  &  P E R F - T E S T *************************/

extern void  packetio_send_discover(uint16_t seqnum, bool_t sendAck);
extern void  packetio_send_query(uint16_t seqnum, etheraddr_t* pDst);
extern void  packetio_send_emit(uint16_t thisSeqNum, etheraddr_t* pDst, uint16_t emiteeCnt);
extern void  packetio_send_charge(uint16_t thisSeqNum, etheraddr_t* pDst);
extern void  packetio_send_reset(etheraddr_t* pDst);

void (*testFcn)(void* state);

enum {
    noTests,
    testProbe,
    testCharge
} testType;

etheraddr_t	uutMAC;
bool_t		isAcking = FALSE;
uint16_t	txSeq = 1;
struct timeval	now;
uint		pauseTime;
uint		responseTime;
uint		delay;

void usec_sleep(uint32_t napLen)
{
    int                 ret;
    struct timeval	timeout = {0,0};

    timeval_add_us(&timeout,pauseTime);
    ret = select(0, NULL, NULL, NULL, &timeout);	// Sub-second sleep()
    if (ret<0)
        puts(strerror(errno));
}

void quit(void* state)
{
    puts("=======================\nSending Reset...");
    packetio_send_reset(&uutMAC);
    puts("=======================\n");
    exit(0);
}

void send_emit(void* state)
{
    puts("=======================\nSending Emit to use the Charge...");
    packetio_send_emit(1, &uutMAC, 63);
    puts("=======================\n");

    /* Wait a few seconds, then Exit */
    gettimeofday(&now, NULL);
    timeval_add_ms(&now, 3000);
    CANCEL(g_block_timer);
    g_block_timer = event_add(&now, quit, NULL);
}


void send_charges(void* state)
{
    int		i;

    puts("=======================\nSending Charge-packet-stream...");
    i = 63;
    do{
        packetio_send_charge(0/*g_neededPackets*/, &uutMAC);
        usec_sleep(pauseTime);
    } while (--i);
    puts("=======================\n");

    /* Schedule an emit packet in a few milliseconds */
    gettimeofday(&now, NULL);
    timeval_add_ms(&now, 300);
    CANCEL(g_block_timer);
    g_block_timer = event_add(&now, send_emit, NULL);
}


void send_queries(void* state)
{
    if (--g_neededPackets == 0)
    {
        delay = 200;
        testFcn = quit;
    } else {
        delay   = responseTime;
        testFcn = send_queries;
    }

    puts("=======================\nSending Query-packet...");
    packetio_send_query(txSeq++, &uutMAC);
    puts("=======================\n");

    /* Schedual a query sequence in a few milliseconds, or exit with a reset */
    gettimeofday(&now, NULL);
    timeval_add_ms(&now, delay);
    CANCEL(g_block_timer);
    g_block_timer = event_add(&now, testFcn, NULL);
}


void send_probes(void* state)
{
    int		i;
    topo_emitee_desc_t	probe;
    etheraddr_t         OUI_addr_40 = {{0x00,0x0d,0x3a,0xd7,0xf1,0x40}};
    etheraddr_t         OUI_addr_41 = {{0x00,0x0d,0x3a,0xd7,0xf1,0x41}};

    probe.ed_type = 1;
    probe.ed_pause = 0;
    memcpy(&probe.ed_src, &OUI_addr_40, sizeof(etheraddr_t));
    memcpy(&probe.ed_dst, &OUI_addr_41, sizeof(etheraddr_t));

    puts("=======================\nSending Probe-packet-storm...");
    for (i=0;i<255;i++)
    {
        packetio_tx_emitee(&probe);
        usec_sleep(pauseTime);
    }
    puts("=======================\n");

    g_neededPackets = (255/74)+1;

    /* Schedule a query sequence in a few milliseconds */
    gettimeofday(&now, NULL);
    timeval_add_ms(&now, 200);
    CANCEL(g_block_timer);
    g_block_timer = event_add(&now, send_queries, NULL);
}


void 
discover_all(void* state)
{
    isAcking = ETHERADDR_IS_ZERO(&uutMAC)?FALSE:TRUE;
    printf("=======================\nSending Discover-packet...%s\n",isAcking?"(acking)":"");
    printf("  uutMAC is " ETHERADDR_FMT "\n", ETHERADDR_PRINT(&uutMAC));

    packetio_send_discover(0,isAcking);
    puts("=======================\n");

    CANCEL(g_block_timer);
    gettimeofday(&now, NULL);
    if (isAcking)
    {
        /* Schedule test in one second */
        timeval_add_ms(&now, 1000);
        g_block_timer = event_add(&now, testFcn, NULL);
    } else {
        /* Schedule another discovery in a second */
        timeval_add_ms(&now, 1000);
        g_block_timer = event_add(&now, discover_all, NULL);
    }
}

/********************************************************************************/


static void
usage(void)
{
    fprintf(stderr, "usage: %s [-d] [-t TRACELEVEL] INTERFACE\n"
	    "\tRuns a link-layer topology discovery mapper on INTERFACE (eg eth0)\n"
	    "\t-t TRACELEVEL : select tracing by adding together:\n"
	    "\t\t0x02 : packet dump of protocol exchange\n"
	    "\t\t0x04 : Charge mechanism for protection against denial of service\n"
	    "\t\t0x08 : system information TLVs (type-length-value)\n"
	    "\t\t0x10 : State-Machine transitions for smS, smE, and smT\n",
	    g_Progname);
    exit(2);
}


void
init_from_conf_file()
{
    FILE   *conf_file;
    char   *line = NULL;
    #define LINEBUFLEN 256
    char    var[LINEBUFLEN];
    char    val[LINEBUFLEN];
    size_t  len  = 0;
    ssize_t numread;
    int     assigns;
    char    default_icon_path[] = {"/etc/icon.ico"};

    /* Set default values for configuration options */
    /* (avoid strdup() since it doesn't use xmalloc wrapper) */
    g_icon_path = xmalloc(strlen(default_icon_path)+1);
    strcpy(g_icon_path,default_icon_path);

    testFcn = NULL;
    testType = noTests;
    pauseTime = 100;		// usec
    responseTime = 200;		// msec

    /* Examine configuration file, if it exists */
    conf_file = fopen("/etc/lld2d.conf", "r");
    if (conf_file == NULL) return;
    while ((numread = getline(&line, &len, conf_file)) != -1)
    {
        var[0] = val[0] = '\0';
        assigns = sscanf(line, "%s = %s", var, val);

        if (assigns==2)
        {
            /* compare to the 1 allowed var... */
            if (!strcmp(var,"icon")) {
                char *path = NULL;
                char *cur  = NULL;

                path = xmalloc(strlen(val)+6); // always allow enough room for a possible prefix of '/etc/'
                cur = path;

                /* Check for leading '/' and prefix '/etc/' if missing */
                if (val[0] != '/')
                {
                    strcpy(cur,"/etc/"); cur += 5;
                }
                strncpy(cur,val,strlen(val));

                if (g_icon_path) xfree(g_icon_path);	// always use the most recent occurrence
                g_icon_path = path;
                DEBUG({printf("configvar 'g_icon_path' = %s\n", g_icon_path);})
            } else if (!strcmp(var,"test")) {
                if (!strcmp(val,"probe"))
                {
                    testType = testProbe;
                    testFcn  = send_probes;
                    printf("Testing with Probes\n");
                } else {
                    testType = testCharge;
                    testFcn  = send_charges;
                    printf("Testing with Charges\n");
                }
            } else if (!strcmp(var,"pause-usec")) {
                pauseTime = atoi(val);
                if (pauseTime > 100000) pauseTime = 0;
                printf("interpacket pause set to %d us\n",pauseTime);
            } else if (!strcmp(var,"response-window-msec")) {
                responseTime = atoi(val);
                if (responseTime > 1000) responseTime = 200;
                printf("pause for query-responses set to %d ms\n",responseTime);
            } else {
                warn("line ignored - var does not match a known string\n");
            }
        } else {
            warn("line ignored - var or val was missing or no equals\n");
        }

    }
    if (line!=NULL)  free(line);
    fclose(conf_file);
}


int
main(int argc, char **argv)
{
    char  *p;
    int    c;
    bool_t opt_debug = FALSE;
    int    opt_trace = 0;

    /* set program name to last component of filename */
    p = strrchr(argv[0], '/');
    if (p)
	g_Progname = p+1;
    else
	g_Progname = argv[0];

    opt_trace = 0;

    /* parse arguments */
    while ((c=getopt(argc, argv, "dt:")) != -1)
    {
	switch (c)
	{
	case 'd':
	    opt_debug = TRUE;
	    break;

	case 't':
	    opt_trace = atoi(optarg);
	    if (opt_trace == 0)
	    {
		fprintf(stderr, "%s: -t TRACELEVEL: parse error in \"%s\"\n",
			g_Progname, optarg);
		usage();
	    }
	    break;

	default:
	    opt_debug = TRUE;
	    break;
	}
    }

    if (optind >= argc)
    {
        g_interface = "eth0";
        printf("%s: no interface-name argument; '%s' assumed.\n", g_Progname, g_interface);
    } else {
        g_interface = strdup(argv[optind]);
    }

    /* initialise remaining process state */
    g_trace_flags = opt_trace;
    memset(&g_hwaddr,0,sizeof(etheraddr_t));

    g_smE_state = smE_Quiescent;
    g_smT_state = smT_Quiescent;

    memset (g_sessions,0,MAX_NUM_SESSIONS*sizeof(g_sessions[0]));

    memset(&g_band,0,sizeof(band_t));		/* BAND algorthm's state */
    g_osl = osl_init();				/* initialise OS-glue Layer */
    memset(g_rxbuf,0,(size_t)RXBUFSZ);
    memset(g_emitbuf,0,(size_t)RXBUFSZ);
    memset(g_txbuf,0,(size_t)TXBUFSZ);
    memset(g_re_txbuf,0,(size_t)TXBUFSZ);
//    g_sees = seeslist_new(NUM_SEES); -- not used, so seeslist.c doesn't need to be linked in.
    g_rcvd_pkt_len = 0;
    g_rtxseqnum = 0;
    g_re_tx_len = 0;
    g_generation = 0;
    g_sequencenum = 0;
    g_opcode = Opcode_INVALID;
    g_ctc_packets = 0;
    g_ctc_bytes = 0;

#if CAN_FOPEN_IN_SELECT_LOOP
    /* then we don't need a global to keep the stream open all the time...*/
#else
    g_procnetdev = fopen("/proc/net/dev","r");
    if (g_procnetdev<0)
        die("fopen of /proc/net/dev failed\n");
#endif

    /* Initialize the timers (inactivity timers are init'd when session is created) */
    g_block_timer = g_charge_timer = g_emit_timer = g_hello_timer = NULL;

    /* initialize things from the parameter file, /etc/lld2d.conf
     * currently, v1.0, this only involves LTLV pointers...
     * but the conf test has several more... */

    init_from_conf_file();

    event_init();

    osl_interface_open(g_osl, g_interface, NULL);
    osl_get_hwaddr(g_osl, &g_hwaddr);

#ifdef __DEBUG__
    IF_DEBUG
        printf("%s: sending from address: " ETHERADDR_FMT "\n",
	    g_Progname, ETHERADDR_PRINT(&g_hwaddr));
    END_DEBUG
#endif
    if (!opt_debug)
    {
	util_use_syslog();
	osl_become_daemon(g_osl);
    }
    DEBUG({printf("%s: sending on interface %s\n", g_Progname, g_interface);})

    osl_write_pidfile(g_osl);

    osl_drop_privs(g_osl);

    /* add IO handlers & run main event loop forever */

    {
        struct timeval	now;

        gettimeofday(&now, NULL);

        /* Start discovery immediately */
        g_block_timer = event_add(&now, discover_all, NULL);
    }

    event_mainloop();

    return 0;
}
