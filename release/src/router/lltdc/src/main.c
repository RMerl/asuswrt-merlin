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

//#define CHECKING_PACKING 1

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#define	DECLARING_GLOBALS
#include "globals.h"

#include "statemachines.h"
#include "packetio.h"

extern void qos_init(void);

bool_t          isConfTest;

static void
usage(void)
{
    fprintf(stderr, "usage: %s [-d] [-t TRACELEVEL] INTERFACE [WIRELESS-IF]\n"
	    "\tRuns a link-layer topology discovery daemon on INTERFACE (eg eth0)\n"
	    "\t-d : don't background, and log moderate tracing to stdout (debug mode)\n"
	    "\t-t TRACELEVEL : select tracing by adding together:\n"
	    "\t\t0x01 : BAND network load control calculations\n"
	    "\t\t0x02 : packet dump of protocol exchange\n"
	    "\t\t0x04 : Charge mechanism for protection against denial of service\n"
	    "\t\t0x08 : system information TLVs (type-length-value)\n"
	    "\t\t0x10 : State-Machine transitions for smS, smE, and smT\n"
	    "\t\t0x20 : Qos/qWave extensions\n",
	    g_Progname);
    exit(2);
}


static void
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

    /* Examine configuration file, if it exists */
    conf_file = fopen("/etc/lld2d.conf", "r");
    if (conf_file == NULL)  return;
    while ((numread = getline(&line, &len, conf_file)) != -1)
    {
        var[0] = val[0] = '\0';
        assigns = sscanf(line, "%s = %s", var, val);

        if (assigns==2)
        {
            /* compare to each of the 2 allowed vars... */
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
            } else if (!strcmp(var,"jumbo-icon")) {
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

                if (g_jumbo_icon_path) xfree(g_jumbo_icon_path);	// always use the most recent occurrence
                g_jumbo_icon_path = path;
                DEBUG({printf("configvar 'g_jumbo_icon_path' = %s\n", g_jumbo_icon_path);})
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

//-----------------------------------------------------------------------------------------------------------//
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

    /* set a module flag if this is the confidence test */
    isConfTest = (strstr(g_Progname,"conftest") != NULL ? TRUE : FALSE);

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
	    if (isConfTest)
            {
                opt_debug = TRUE;
                opt_trace = TRC_PACKET + TRC_STATE;
            } else {
                usage();
            }
	    break;
	}
    }

    if (isConfTest)
    {
        if (optind >= argc)
        {
            g_interface = strstr(g_Progname,"x86") != NULL ? "eth0" : "br0";
            g_wl_interface = strstr(g_Progname,"x86") != NULL ? "eth0" : "eth1";
            printf("%s: no interface-name argument; '%s' assumed.\n", g_Progname, g_interface);
        } else {
            g_interface = strdup(argv[optind]);
            if ((optind+1) >= argc)
            {
                g_wl_interface = g_interface;
            } else {
                g_wl_interface = strdup(argv[optind+1]);
            }
        }
    } else {
        if (optind >= argc)
        {
            fprintf(stderr, "%s: error: missing INTERFACE name argument\n", g_Progname);
            usage();
        } else {
            g_interface = strdup(argv[optind]);
            if ((optind+1) >= argc)
            {
                g_wl_interface = g_interface;
            } else {
                g_wl_interface = strdup(argv[optind+1]);
            }
        }
    }

#ifdef CHECKING_PACKING
    printf("etherHdr: " FMT_SIZET "   baseHdr: " FMT_SIZET "    discoverHdr: " \
            FMT_SIZET "    helloHdr: " FMT_SIZET "    qltlvHdr: " FMT_SIZET "\n",
            sizeof(topo_ether_header_t),sizeof(topo_base_header_t),sizeof(topo_discover_header_t),
            sizeof(topo_hello_header_t),sizeof(topo_qltlv_header_t));
    assert(sizeof(topo_ether_header_t) == 14);
    assert(sizeof(topo_base_header_t) == 18);
    assert(sizeof(topo_discover_header_t) == 4);
    assert(sizeof(topo_hello_header_t) == 14);
    assert(sizeof(topo_qltlv_header_t) == 4);
    puts("Passed struct-packing checks!");
#endif

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
    g_sees = seeslist_new(NUM_SEES);
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
     * currently, v1.0, this only involves LTLV pointers... */

    init_from_conf_file();

    event_init();
    qos_init();

    osl_interface_open(g_osl, g_interface, NULL);
    osl_get_hwaddr(g_osl, &g_hwaddr);

    IF_DEBUG
        printf("%s: listening at address: " ETHERADDR_FMT "\n",
	    g_Progname, ETHERADDR_PRINT(&g_hwaddr));
    END_DEBUG
    if (!opt_debug)
    {
        DEBUG({printf("%s: Using syslog\n", g_Progname);})
        util_use_syslog();
        DEBUG({printf("%s: Daemonizing...\n", g_Progname);})
        osl_become_daemon(g_osl);
    }
    DEBUG({printf("%s: listening on interface %s\n", g_Progname, g_interface);})

    osl_write_pidfile(g_osl);

    osl_drop_privs(g_osl);

    /* add IO handlers & run main event loop forever */
    event_mainloop();

    return 0;
}
