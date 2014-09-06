/*
 *  UDP MIB group Table implementation - udpTable.c
 *
 */

/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include <net-snmp/net-snmp-config.h>
#include "mibII_common.h"

#if HAVE_NETINET_UDP_H
#include <netinet/udp.h>
#endif
#if HAVE_NETINET_UDP_VAR_H
#include <netinet/udp_var.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>

#ifdef linux
#include "tcpTable.h"
#endif
#include "udp.h"
#include "udpTable.h"

#ifdef hpux11
#define	UDPTABLE_ENTRY_TYPE	mib_udpLsnEnt 
#define	UDPTABLE_LOCALADDRESS	LocalAddress 
#define	UDPTABLE_LOCALPORT	LocalPort 
#define	UDPTABLE_IS_TABLE
#else

#ifdef solaris2
typedef struct netsnmp_udpEntry_s netsnmp_udpEntry;
struct netsnmp_udpEntry_s {
    mib2_udpEntry_t   entry;
    netsnmp_udpEntry *inp_next;
};
#define	UDPTABLE_ENTRY_TYPE	netsnmp_udpEntry
#define	UDPTABLE_LOCALADDRESS	entry.udpLocalAddress 
#define	UDPTABLE_LOCALPORT	entry.udpLocalPort 
#define	UDPTABLE_IS_LINKED_LIST
#else

#ifdef HAVE_IPHLPAPI_H
#include <iphlpapi.h>
#define	UDPTABLE_ENTRY_TYPE	MIB_UDPROW		/* ??? */
#define	UDPTABLE_LOCALADDRESS	dwLocalAddr
#define	UDPTABLE_LOCALPORT	dwLocalPort 
#define	UDPTABLE_IS_TABLE
#else			/* everything else */

#ifdef linux
#define INP_NEXT_SYMBOL		inp_next
#endif
#ifdef openbsd4
#define INP_NEXT_SYMBOL		inp_queue.cqe_next	/* or set via <net-snmp/system/openbsd.h> */
#endif

#if defined(freebsd4) || defined(darwin) || defined(osf5)
typedef struct netsnmp_inpcb_s netsnmp_inpcb;
struct netsnmp_inpcb_s {
    struct inpcb    pcb;
    int             state;
    netsnmp_inpcb  *inp_next;
};
#define	UDPTABLE_ENTRY_TYPE	netsnmp_inpcb 
#define	UDPTABLE_LOCALADDRESS	pcb.inp_laddr.s_addr 
#define	UDPTABLE_LOCALPORT	pcb.inp_lport
#else
#define	UDPTABLE_ENTRY_TYPE	struct inpcb 
#define	UDPTABLE_LOCALADDRESS	inp_laddr.s_addr 
#define	UDPTABLE_LOCALPORT	inp_lport
#endif
#define	UDPTABLE_IS_LINKED_LIST

#endif                          /* WIN32 cygwin */
#endif                          /* solaris2 */
#endif                          /* hpux11 */

				/* Head of linked list, or root of table */
UDPTABLE_ENTRY_TYPE	*udp_head  = NULL;
int                      udp_size  = 0;	/* Only used for table-based systems */


	/*
	 *
	 * Initialization and handler routines are common to all architectures
	 *
	 */
#ifndef MIB_STATS_CACHE_TIMEOUT
#define MIB_STATS_CACHE_TIMEOUT	5
#endif
#ifndef UDP_STATS_CACHE_TIMEOUT
#define UDP_STATS_CACHE_TIMEOUT	MIB_STATS_CACHE_TIMEOUT
#endif

#ifdef UDP_ADDRESSES_IN_HOST_ORDER
#define UDP_ADDRESS_TO_HOST_ORDER(x) x
#define UDP_ADDRESS_TO_NETWORK_ORDER(x) htonl(x)
#else
#define UDP_ADDRESS_TO_HOST_ORDER(x) ntohl(x)
#define UDP_ADDRESS_TO_NETWORK_ORDER(x) x
#endif

#ifdef UDP_PORTS_IN_HOST_ORDER
#define UDP_PORT_TO_HOST_ORDER(x) x
#else
#define UDP_PORT_TO_HOST_ORDER(x) ntohs(x)
#endif


oid             udpTable_oid[] = { SNMP_OID_MIB2, 7, 5 };

void
init_udpTable(void)
{
    netsnmp_table_registration_info *table_info;
    netsnmp_iterator_info           *iinfo;
    netsnmp_handler_registration    *reginfo;
    int                              rc;

    DEBUGMSGTL(("mibII/udpTable", "Initialising UDP Table\n"));
    /*
     * Create the table data structure, and define the indexing....
     */
    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    if (!table_info) {
        return;
    }
    netsnmp_table_helper_add_indexes(table_info, ASN_IPADDRESS,
                                                 ASN_INTEGER, 0);
    table_info->min_column = UDPLOCALADDRESS;
    table_info->max_column = UDPLOCALPORT;


    /*
     * .... and iteration information ....
     */
    iinfo      = SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
    if (!iinfo) {
        return;
    }
    iinfo->get_first_data_point = udpTable_first_entry;
    iinfo->get_next_data_point  = udpTable_next_entry;
    iinfo->table_reginfo        = table_info;
#if defined (WIN32) || defined (cygwin)
    iinfo->flags               |= NETSNMP_ITERATOR_FLAG_SORTED;
#endif /* WIN32 || cygwin */


    /*
     * .... and register the table with the agent.
     */
    reginfo = netsnmp_create_handler_registration("udpTable",
            udpTable_handler,
            udpTable_oid, OID_LENGTH(udpTable_oid),
            HANDLER_CAN_RONLY),
    rc = netsnmp_register_table_iterator2(reginfo, iinfo);
    if (rc != SNMPERR_SUCCESS)
        return;

    /*
     * .... with a local cache
     */
    netsnmp_inject_handler( reginfo,
		    netsnmp_get_cache_handler(UDP_STATS_CACHE_TIMEOUT,
			   		udpTable_load, udpTable_free,
					udpTable_oid, OID_LENGTH(udpTable_oid)));
}



int
udpTable_handler(netsnmp_mib_handler          *handler,
                 netsnmp_handler_registration *reginfo,
                 netsnmp_agent_request_info   *reqinfo,
                 netsnmp_request_info         *requests)
{
    netsnmp_request_info  *request;
    netsnmp_variable_list *requestvb;
    netsnmp_table_request_info *table_info;
    UDPTABLE_ENTRY_TYPE	  *entry;
    oid      subid;
    long     port;
    in_addr_t addr;

    DEBUGMSGTL(("mibII/udpTable", "Handler - mode %s\n",
                    se_find_label_in_slist("agent_mode", reqinfo->mode)));
    switch (reqinfo->mode) {
    case MODE_GET:
        for (request=requests; request; request=request->next) {
            requestvb = request->requestvb;
            DEBUGMSGTL(( "mibII/udpTable", "oid: "));
            DEBUGMSGOID(("mibII/udpTable", requestvb->name,
                                           requestvb->name_length));
            DEBUGMSG((   "mibII/udpTable", "\n"));

            entry = (UDPTABLE_ENTRY_TYPE *)netsnmp_extract_iterator_context(request);
            if (!entry)
                continue;
            table_info = netsnmp_extract_table_info(request);
            subid      = table_info->colnum;

            switch (subid) {
            case UDPLOCALADDRESS:
#if defined(osf5) && defined(IN6_EXTRACT_V4ADDR)
                addr = ntohl(IN6_EXTRACT_V4ADDR(&entry->pcb.inp_laddr));
	        snmp_set_var_typed_value(requestvb, ASN_IPADDRESS,
                                         (u_char*)&addr,
                                         sizeof(uint32_t));
#else
                addr = UDP_ADDRESS_TO_HOST_ORDER(entry->UDPTABLE_LOCALADDRESS);
	        snmp_set_var_typed_value(requestvb, ASN_IPADDRESS,
                                         (u_char *)&addr,
                                         sizeof(uint32_t));
#endif
                break;
            case UDPLOCALPORT:
                port = UDP_PORT_TO_HOST_ORDER((u_short)entry->UDPTABLE_LOCALPORT);
	        snmp_set_var_typed_value(requestvb, ASN_INTEGER,
                                 (u_char *)&port, sizeof(port));
                break;
	    }
	}
        break;

    case MODE_GETNEXT:
    case MODE_GETBULK:
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
        snmp_log(LOG_WARNING, "mibII/udpTable: Unsupported mode (%d)\n",
                               reqinfo->mode);
        break;
    default:
        snmp_log(LOG_WARNING, "mibII/udpTable: Unrecognised mode (%d)\n",
                               reqinfo->mode);
        break;
    }

    return SNMP_ERR_NOERROR;
}

	/*
	 * Two forms of iteration hook routines:
	 *    One for when the UDP table is stored as a table
	 *    One for when the UDP table is stored as a linked list
	 *
	 * Also applies to the cache-handler free routine
	 */

#ifdef	UDPTABLE_IS_TABLE
netsnmp_variable_list *
udpTable_first_entry(void **loop_context,
                     void **data_context,
                     netsnmp_variable_list *index,
                     netsnmp_iterator_info *data)
{
    /*
     * XXX - How can we tell if the cache is valid?
     *       No access to 'reqinfo'
     */
    if (udp_size == 0)
        return NULL;

    /*
     * Point to the first entry, and use the
     * 'next_entry' hook to retrieve this row
     */
    *loop_context = 0;
    return udpTable_next_entry( loop_context, data_context, index, data );
}

netsnmp_variable_list *
udpTable_next_entry( void **loop_context,
                     void **data_context,
                     netsnmp_variable_list *index,
                     netsnmp_iterator_info *data)
{
    int i = (int)*loop_context;
    long port;

    if (udp_size < i)
        return NULL;

    /*
     * Set up the indexing for the specified row...
     */
#if defined (WIN32) || defined (cygwin)
    port = ntohl((u_long)udp_head[i].UDPTABLE_LOCALADDRESS);
    snmp_set_var_value(index, (u_char *)&port,
                                  sizeof(udp_head[i].UDPTABLE_LOCALADDRESS));
#else
    snmp_set_var_value(index, (u_char *)&udp_head[i].UDPTABLE_LOCALADDRESS,
                                  sizeof(udp_head[i].UDPTABLE_LOCALADDRESS));
#endif
    port = UDP_PORT_TO_HOST_ORDER((u_short)udp_head[i].UDPTABLE_LOCALPORT);
    snmp_set_var_value(index->next_variable,
                               (u_char*)&port, sizeof(port));
    /*
     * ... return the data structure for this row,
     * and update the loop context ready for the next one.
     */
    *data_context = (void*)&udp_head[i];
    *loop_context = (void*)++i;
    return index;
}

void
udpTable_free(netsnmp_cache *cache, void *magic)
{
#if defined (WIN32) || defined (cygwin)
    if (udp_head) {
		/* the allocated structure is a count followed by table entries */
		free((char *)(udp_head) - sizeof(DWORD));
	}
#else
    if (udp_head)
        free(udp_head);
#endif
    udp_head = NULL;
    udp_size = 0;
}
#else
#ifdef UDPTABLE_IS_LINKED_LIST
netsnmp_variable_list *
udpTable_first_entry(void **loop_context,
                     void **data_context,
                     netsnmp_variable_list *index,
                     netsnmp_iterator_info *data)
{
    /*
     * XXX - How can we tell if the cache is valid?
     *       No access to 'reqinfo'
     */
    if (udp_head == NULL)
        return NULL;

    /*
     * Point to the first entry, and use the
     * 'next_entry' hook to retrieve this row
     */
    *loop_context = (void*)udp_head;
    return udpTable_next_entry( loop_context, data_context, index, data );
}

netsnmp_variable_list *
udpTable_next_entry( void **loop_context,
                     void **data_context,
                     netsnmp_variable_list *index,
                     netsnmp_iterator_info *data)
{
    UDPTABLE_ENTRY_TYPE	 *entry = (UDPTABLE_ENTRY_TYPE *)*loop_context;
    long port;
    long addr;

    if (!entry)
        return NULL;

    /*
     * Set up the indexing for the specified row...
     */
#if defined(osf5) && defined(IN6_EXTRACT_V4ADDR)
                snmp_set_var_value(index,
                              (u_char*)&IN6_EXTRACT_V4ADDR(&entry->pcb.inp_laddr),
                                 sizeof(IN6_EXTRACT_V4ADDR(&entry->pcb.inp_laddr)));
#else
    addr = UDP_ADDRESS_TO_NETWORK_ORDER((in_addr_t)entry->UDPTABLE_LOCALADDRESS);
    snmp_set_var_value(index, (u_char *)&addr,
                                 sizeof(addr));
#endif
    port = UDP_PORT_TO_HOST_ORDER(entry->UDPTABLE_LOCALPORT);
    snmp_set_var_value(index->next_variable,
                               (u_char*)&port, sizeof(port));

    /*
     * ... return the data structure for this row,
     * and update the loop context ready for the next one.
     */
    *data_context = (void*)entry;
    *loop_context = (void*)entry->INP_NEXT_SYMBOL;
    return index;
}

void
udpTable_free(netsnmp_cache *cache, void *magic)
{
    UDPTABLE_ENTRY_TYPE	 *p;
    while (udp_head) {
        p = udp_head;
        udp_head = udp_head->INP_NEXT_SYMBOL;
        free(p);
    }

    udp_head = NULL;
}
#endif		/* UDPTABLE_IS_LINKED_LIST */
#endif		/* UDPTABLE_IS_TABLE */


	/*
	 *
	 * The cache-handler loading routine is the main
	 *    place for architecture-specific code
	 *
	 * Load into either a table structure, or a linked list
	 *    depending on the system architecture
	 */


#ifdef hpux11
int
udpTable_load(netsnmp_cache *cache, void *vmagic)
{
    int             fd;
    struct nmparms  p;
    int             val = 0;
    unsigned int    ulen;
    int             ret;

    udpTable_free(NULL, NULL);

    if ((fd = open_mib("/dev/ip", O_RDONLY, 0, NM_ASYNC_OFF)) >= 0) {
        p.objid = ID_udpLsnNumEnt;
        p.buffer = (void *) &val;
        ulen = sizeof(int);
        p.len = &ulen;
        if ((ret = get_mib_info(fd, &p)) == 0)
            udp_size = val;

        if (udp_size > 0) {
            ulen = (unsigned) udp_size *sizeof(mib_udpLsnEnt);
            udp_head = (mib_udpLsnEnt *) malloc(ulen);
            p.objid = ID_udpLsnTable;
            p.buffer = (void *) udp_head;
            p.len = &ulen;
            if ((ret = get_mib_info(fd, &p)) < 0) {
                udp_size = 0;
            }
        }

        close_mib(fd);
    }

    if (udp_size > 0) {
        DEBUGMSGTL(("mibII/udpTable", "Loaded UDP Table (hpux11)\n"));
        return 0;
    }
    DEBUGMSGTL(("mibII/udpTable", "Failed to load UDP Table (hpux11)\n"));
    return -1;
}
#else                           /* hpux11 */

#ifdef linux
int
udpTable_load(netsnmp_cache *cache, void *vmagic)
{
    FILE           *in;
    char            line[256];

    udpTable_free(cache, NULL);

    if (!(in = fopen("/proc/net/udp", "r"))) {
        DEBUGMSGTL(("mibII/udpTable", "Failed to load UDP Table (linux)\n"));
        NETSNMP_LOGONCE((LOG_ERR, "snmpd: cannot open /proc/net/udp ...\n"));
        return -1;
    }

    /*
     * scan proc-file and build up a linked list 
     * This will actually be built up in reverse,
     *   but since the entries are unsorted, that doesn't matter.
     */
    while (line == fgets(line, sizeof(line), in)) {
        struct inpcb    pcb, *nnew;
        unsigned int    state, lport;

        memset(&pcb, 0, sizeof(pcb));

        if (3 != sscanf(line, "%*d: %x:%x %*x:%*x %x",
                        &pcb.inp_laddr.s_addr, &lport, &state))
            continue;

        if (state != 7)         /* fix me:  UDP_LISTEN ??? */
            continue;

        /* store in network byte order */
        pcb.inp_laddr.s_addr = htonl(pcb.inp_laddr.s_addr);
        pcb.inp_lport = htons((unsigned short) (lport));

        nnew = SNMP_MALLOC_TYPEDEF(struct inpcb);
        if (nnew == NULL)
            break;
        memcpy(nnew, &pcb, sizeof(struct inpcb));
        nnew->inp_next = udp_head;
        udp_head       = nnew;
    }

    fclose(in);

    DEBUGMSGTL(("mibII/udpTable", "Loaded UDP Table (linux)\n"));
    return 0;
}
#else                           /* linux */

#ifdef solaris2
static int
UDP_Cmp(void *addr, void *ep)
{
    if (memcmp((mib2_udpEntry_t *) ep, (mib2_udpEntry_t *) addr,
               sizeof(mib2_udpEntry_t)) == 0)
        return (0);
    else
        return (1);
}


int
udpTable_load(netsnmp_cache *cache, void *vmagic)
{
    mib2_udpEntry_t   entry;
    netsnmp_udpEntry *nnew;
    netsnmp_udpEntry *prev_entry = NULL;


    udpTable_free(NULL, NULL);

    if (getMibstat(MIB_UDP_LISTEN, &entry, sizeof(mib2_udpEntry_t),
                   GET_FIRST, &UDP_Cmp, &entry) != 0) {
        DEBUGMSGTL(("mibII/udpTable", "Failed to load UDP Table (solaris)\n"));
        return -1;
    }

    while (1) {
        /*
         * Not interested in 'idle' entries, apparently....
         */
        DEBUGMSGTL(("mibII/udpTable", "UDP Entry %x:%d (%d)\n",
                     entry.udpLocalAddress, entry.udpLocalPort, entry.udpEntryInfo.ue_state));
        if (entry.udpEntryInfo.ue_state == MIB2_UDP_idle) {
            /*
             * Build up a linked list copy of the getMibstat results
             * Note that since getMibstat returns rows in sorted order,
	     *    we need to retain this order while building the list
	     *    so new entries are added onto the end of the list.
             * xxx-rks: WARNING: this is NOT TRUE on the sf cf solaris boxes.
             */
            nnew = SNMP_MALLOC_TYPEDEF(netsnmp_udpEntry);
            if (nnew == NULL)
                break;
            memcpy(&(nnew->entry), &entry, sizeof(mib2_udpEntry_t));
            if (!prev_entry)
	        udp_head = nnew;
	    else
	        prev_entry->inp_next = nnew;
	    prev_entry = nnew;
	}

        if (getMibstat(MIB_UDP_LISTEN, &entry, sizeof(mib2_udpEntry_t),
                       GET_NEXT, &UDP_Cmp, &entry) != 0)
	    break;
    }

    if (udp_head) {
        DEBUGMSGTL(("mibII/udpTable", "Loaded UDP Table (solaris)\n"));
        return 0;
    }
    DEBUGMSGTL(("mibII/udpTable", "Failed to load UDP Table (solaris)\n"));
    return -1;
}
#else                           /* solaris2 */

#if defined (WIN32) || defined (cygwin)
int
udpTable_load(netsnmp_cache *cache, void *vmagic)
{
    PMIB_UDPTABLE pUdpTable = NULL;
    DWORD         dwActualSize = 0;
    DWORD         status = NO_ERROR;

    /*
     * query for the buffer size needed 
     */
    status = GetUdpTable(pUdpTable, &dwActualSize, TRUE);
    if (status == ERROR_INSUFFICIENT_BUFFER) {
        pUdpTable = (PMIB_UDPTABLE) malloc(dwActualSize);
        if (pUdpTable != NULL) {
            /*
             * Get the sorted UDP table 
             */
            status = GetUdpTable(pUdpTable, &dwActualSize, TRUE);
        }
    }
    if (status == NO_ERROR) {
        DEBUGMSGTL(("mibII/udpTable", "Loaded UDP Table (win32)\n"));
        udp_size = pUdpTable->dwNumEntries -1;  /* entries are counted starting with 0 */
        udp_head = pUdpTable->table;
        return 0;
    }
    DEBUGMSGTL(("mibII/udpTable", "Failed to load UDP Table (win32)\n"));
    if (pUdpTable)
	free(pUdpTable);
    return -1;
}
#else                           /* WIN32 cygwin*/

#if (defined(NETSNMP_CAN_USE_SYSCTL) && defined(UDPCTL_PCBLIST))
int
udpTable_load(netsnmp_cache *cache, void *vmagic)
{
    size_t   len;
    int      sname[] = { CTL_NET, PF_INET, IPPROTO_UDP, UDPCTL_PCBLIST };
    char     *udpcb_buf = NULL;
#if defined(dragonfly)
    struct xinpcb  *xig = NULL;
#else
    struct xinpgen *xig = NULL;
#endif
    UDPTABLE_ENTRY_TYPE  *nnew;

    udpTable_free(NULL, NULL);

    /*
     *  Read in the buffer containing the UDP table data
     */
    len = 0;
    if (sysctl(sname, 4, 0, &len, 0, 0) < 0 ||
       (udpcb_buf = malloc(len)) == NULL)
        return -1;
    if (sysctl(sname, 4, udpcb_buf, &len, 0, 0) < 0) {
        free(udpcb_buf);
        return -1;
    }

    /*
     *  Unpick this into the constituent 'xinpgen' structures, and extract
     *     the 'inpcb' elements into a linked list (built in reverse)
     */
#if defined(dragonfly)
    xig = (struct xinpcb  *) udpcb_buf;
#else
    xig = (struct xinpgen *) udpcb_buf;
    xig = (struct xinpgen *) ((char *) xig + xig->xig_len);
#endif

#if defined(dragonfly)
    while (xig && ((char *)xig + xig->xi_len < udpcb_buf + len))
#else
    while (xig && (xig->xig_len > sizeof(struct xinpgen)))
#endif
    {
        nnew = SNMP_MALLOC_TYPEDEF(UDPTABLE_ENTRY_TYPE);
        if (!nnew)
            break;
        memcpy(&nnew->pcb, &((struct xinpcb *) xig)->xi_inp, sizeof(struct inpcb));
	nnew->inp_next = udp_head;
	udp_head   = nnew;
#if defined(dragonfly)
        xig = (struct xinpcb  *) ((char *) xig + xig->xi_len);
#else
        xig = (struct xinpgen *) ((char *) xig + xig->xig_len);
#endif
    }

    free(udpcb_buf);
    if (udp_head) {
        DEBUGMSGTL(("mibII/udpTable", "Loaded UDP Table (sysctl)\n"));
        return 0;
    }
    DEBUGMSGTL(("mibII/udpTable", "Failed to load UDP Table (sysctl)\n"));
    return -1;
}
#else		/* (defined(NETSNMP_CAN_USE_SYSCTL) && defined(UDPCTL_PCBLIST)) */
#ifdef PCB_TABLE
int
udpTable_load(netsnmp_cache *cache, void *vmagic)
{
    struct inpcbtable table;
    struct inpcb   *nnew, *entry;

    udpTable_free(NULL, NULL);

    if (!auto_nlist(UDB_SYMBOL, (char *) &table, sizeof(table))) {
        DEBUGMSGTL(("mibII/udpTable", "Failed to read inpcbtable\n"));
        return -1;
    }

    /*
     *  Set up a linked list
     */
    entry  = table.inpt_queue.cqh_first;
    while (entry) {
   
        nnew = SNMP_MALLOC_TYPEDEF(struct inpcb);
        if (!nnew)
            break;

        if (!NETSNMP_KLOOKUP(entry, (char *) nnew, sizeof(struct inpcb))) {
            DEBUGMSGTL(("mibII/udpTable:udpTable_load", "klookup failed\n"));
            break;
        }

        entry    = nnew->inp_queue.cqe_next;	/* Next kernel entry */
	nnew->inp_queue.cqe_next = udp_head;
	udp_head = nnew;

        if (entry == table.inpt_queue.cqh_first)
            break;
    }

    if (udp_head) {
        DEBUGMSGTL(("mibII/udpTable", "Loaded UDP Table (pcb_table)\n"));
        return 0;
    }
    DEBUGMSGTL(("mibII/udpTable", "Failed to load UDP Table (pcb_table)\n"));
    return -1;
}

#else				/* PCB_TABLE */
#ifdef UDB_SYMBOL
int
udpTable_load(netsnmp_cache *cache, void *vmagic)
{
    struct inpcb   udp_inpcb;
    struct inpcb   *nnew, *entry;

    udpTable_free(NULL, NULL);

    if (!auto_nlist(UDB_SYMBOL, (char *) &udp_inpcb, sizeof(udp_inpcb))) {
        DEBUGMSGTL(("mibII/udpTable", "Failed to read udb_symbol\n"));
        return -1;
    }

    /*
     *  Set up a linked list
     */
    entry  = udp_inpcb.INP_NEXT_SYMBOL;
    while (entry) {
   
        nnew = SNMP_MALLOC_TYPEDEF(struct inpcb);
        if (!nnew)
            break;

        if (!NETSNMP_KLOOKUP(entry, (char *) nnew, sizeof(struct inpcb))) {
            DEBUGMSGTL(("mibII/udpTable:udpTable_load", "klookup failed\n"));
            break;
        }

        entry    = nnew->INP_NEXT_SYMBOL;		/* Next kernel entry */
	nnew->INP_NEXT_SYMBOL = udp_head;
	udp_head = nnew;

        if (entry == udp_inpcb.INP_NEXT_SYMBOL)
            break;
    }

    if (udp_head) {
        DEBUGMSGTL(("mibII/udpTable", "Loaded UDP Table (udb_symbol)\n"));
        return 0;
    }
    DEBUGMSGTL(("mibII/udpTable", "Failed to load UDP Table (udb_symbol)\n"));
    return -1;
}

#else				/* UDB_SYMBOL */
int
udpTable_load(netsnmp_cache *cache, void *vmagic)
{
    DEBUGMSGTL(("mibII/udpTable", "Loading UDP Table not implemented\n"));
    return -1;
}
#endif				/* UDB_SYMBOL */
#endif				/* PCB_TABLE */
#endif		/* (defined(NETSNMP_CAN_USE_SYSCTL) && defined(UDPCTL_PCBLIST)) */
#endif                          /* WIN32 cygwin*/
#endif                          /* linux */
#endif                          /* solaris2 */
#endif                          /* hpux11 */
