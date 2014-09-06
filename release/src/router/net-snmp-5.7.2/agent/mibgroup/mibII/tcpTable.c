/*
 *  TCP MIB group Table implementation - tcpTable.c
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
#include <net-snmp/net-snmp-features.h>
#include "mibII_common.h"

#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#if HAVE_NETINET_TCP_TIMER_H
#include <netinet/tcp_timer.h>
#endif
#if HAVE_NETINET_TCPIP_H
#include <netinet/tcpip.h>
#endif
#if HAVE_NETINET_TCP_VAR_H
#include <netinet/tcp_var.h>
#endif
#if HAVE_NETLINK_NETLINK_H
#include <netlink/netlink.h>
#include <netlink/msg.h>
#include <linux/inet_diag.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>

#include "tcp.h"
#include "tcpTable.h"

netsnmp_feature_child_of(tcptable_all, libnetsnmpmibs)

netsnmp_feature_child_of(tcp_count_connections, tcptable_all)

#ifdef hpux11
#define	TCPTABLE_ENTRY_TYPE	mib_tcpConnEnt 
#define	TCPTABLE_STATE		State 
#define	TCPTABLE_LOCALADDRESS	LocalAddress 
#define	TCPTABLE_LOCALPORT	LocalPort 
#define	TCPTABLE_REMOTEADDRESS	RemAddress 
#define	TCPTABLE_REMOTEPORT	RemPort 
#define	TCPTABLE_IS_TABLE
#else

#ifdef solaris2
typedef struct netsnmp_tcpConnEntry_s netsnmp_tcpConnEntry;
struct netsnmp_tcpConnEntry_s {
    mib2_tcpConnEntry_t   entry;
    netsnmp_tcpConnEntry *inp_next;
};
#define	TCPTABLE_ENTRY_TYPE	netsnmp_tcpConnEntry
#define	TCPTABLE_STATE		entry.tcpConnState 
#define	TCPTABLE_LOCALADDRESS	entry.tcpConnLocalAddress 
#define	TCPTABLE_LOCALPORT	entry.tcpConnLocalPort 
#define	TCPTABLE_REMOTEADDRESS	entry.tcpConnRemAddress 
#define	TCPTABLE_REMOTEPORT	entry.tcpConnRemPort 
#define	TCPTABLE_IS_LINKED_LIST
#else

#ifdef HAVE_IPHLPAPI_H
#include <iphlpapi.h>
#define	TCPTABLE_ENTRY_TYPE	MIB_TCPROW
#define	TCPTABLE_STATE		dwState 
#define	TCPTABLE_LOCALADDRESS	dwLocalAddr
#define	TCPTABLE_LOCALPORT	dwLocalPort 
#define	TCPTABLE_REMOTEADDRESS	dwRemoteAddr 
#define	TCPTABLE_REMOTEPORT	dwRemotePort 
#define	TCPTABLE_IS_TABLE
#else

#ifdef linux
#define	TCPTABLE_ENTRY_TYPE	struct inpcb 
#define	TCPTABLE_STATE		inp_state 
#define	TCPTABLE_LOCALADDRESS	inp_laddr.s_addr 
#define	TCPTABLE_LOCALPORT	inp_lport
#define	TCPTABLE_REMOTEADDRESS	inp_faddr.s_addr 
#define	TCPTABLE_REMOTEPORT	inp_fport
#define	TCPTABLE_IS_LINKED_LIST

#else			/* everything else */

typedef struct netsnmp_inpcb_s netsnmp_inpcb;
struct netsnmp_inpcb_s {
    struct inpcb    pcb;
    int             state;
    netsnmp_inpcb  *inp_next;
};
#undef INP_NEXT_SYMBOL
#define INP_NEXT_SYMBOL		inp_next
#define	TCPTABLE_ENTRY_TYPE	netsnmp_inpcb 
#define	TCPTABLE_STATE		state 
#define	TCPTABLE_LOCALADDRESS	pcb.inp_laddr.s_addr 
#define	TCPTABLE_LOCALPORT	pcb.inp_lport
#define	TCPTABLE_REMOTEADDRESS	pcb.inp_faddr.s_addr 
#define	TCPTABLE_REMOTEPORT	pcb.inp_fport
#define	TCPTABLE_IS_LINKED_LIST

#endif                          /* linux */
#endif                          /* WIN32 cygwin */
#endif                          /* solaris2 */
#endif                          /* hpux11 */

				/* Head of linked list, or root of table */
TCPTABLE_ENTRY_TYPE	*tcp_head  = NULL;
int                      tcp_size  = 0;	/* Only used for table-based systems */
int                      tcp_estab = 0;


	/*
	 *
	 * Initialization and handler routines are common to all architectures
	 *
	 */
#ifndef MIB_STATS_CACHE_TIMEOUT
#define MIB_STATS_CACHE_TIMEOUT	5
#endif
#ifndef TCP_STATS_CACHE_TIMEOUT
#define TCP_STATS_CACHE_TIMEOUT	MIB_STATS_CACHE_TIMEOUT
#endif

#if defined(TCP_PORTS_IN_HOST_ORDER) && TCP_PORTS_IN_HOST_ORDER
#define TCP_PORT_TO_HOST_ORDER(x) x
#else
#define TCP_PORT_TO_HOST_ORDER(x) ntohs(x)
#endif

void
init_tcpTable(void)
{
    const oid tcpTable_oid[] = { SNMP_OID_MIB2, 6, 13 };

    netsnmp_table_registration_info *table_info;
    netsnmp_iterator_info           *iinfo;
    netsnmp_handler_registration    *reginfo;
    int                              rc;

    DEBUGMSGTL(("mibII/tcpTable", "Initialising TCP Table\n"));
    /*
     * Create the table data structure, and define the indexing....
     */
    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    if (!table_info) {
        return;
    }
    netsnmp_table_helper_add_indexes(table_info, ASN_IPADDRESS,
                                                 ASN_INTEGER,
                                                 ASN_IPADDRESS,
                                                 ASN_INTEGER, 0);
    table_info->min_column = TCPCONNSTATE;
    table_info->max_column = TCPCONNREMOTEPORT;


    /*
     * .... and iteration information ....
     */
    iinfo      = SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
    if (!iinfo) {
        return;
    }
    iinfo->get_first_data_point = tcpTable_first_entry;
    iinfo->get_next_data_point  = tcpTable_next_entry;
    iinfo->table_reginfo        = table_info;
#if defined (WIN32) || defined (cygwin)
    iinfo->flags               |= NETSNMP_ITERATOR_FLAG_SORTED;
#endif /* WIN32 || cygwin */


    /*
     * .... and register the table with the agent.
     */
    reginfo = netsnmp_create_handler_registration("tcpTable",
            tcpTable_handler,
            tcpTable_oid, OID_LENGTH(tcpTable_oid),
            HANDLER_CAN_RONLY),
    rc = netsnmp_register_table_iterator2(reginfo, iinfo);
    if (rc != SNMPERR_SUCCESS)
        return;

    /*
     * .... with a local cache
     *    (except for Solaris, which uses a different approach)
     */
    netsnmp_inject_handler( reginfo,
		    netsnmp_get_cache_handler(TCP_STATS_CACHE_TIMEOUT,
			   		tcpTable_load, tcpTable_free,
					tcpTable_oid, OID_LENGTH(tcpTable_oid)));
}



int
tcpTable_handler(netsnmp_mib_handler          *handler,
                 netsnmp_handler_registration *reginfo,
                 netsnmp_agent_request_info   *reqinfo,
                 netsnmp_request_info         *requests)
{
    netsnmp_request_info  *request;
    netsnmp_variable_list *requestvb;
    netsnmp_table_request_info *table_info;
    TCPTABLE_ENTRY_TYPE	  *entry;
    oid      subid;
    long     port;
    long     state;

    DEBUGMSGTL(("mibII/tcpTable", "Handler - mode %s\n",
                    se_find_label_in_slist("agent_mode", reqinfo->mode)));
    switch (reqinfo->mode) {
    case MODE_GET:
        for (request=requests; request; request=request->next) {
            requestvb = request->requestvb;
            DEBUGMSGTL(( "mibII/tcpTable", "oid: "));
            DEBUGMSGOID(("mibII/tcpTable", requestvb->name,
                                           requestvb->name_length));
            DEBUGMSG((   "mibII/tcpTable", "\n"));

            entry = (TCPTABLE_ENTRY_TYPE *)netsnmp_extract_iterator_context(request);
            if (!entry)
                continue;
            table_info = netsnmp_extract_table_info(request);
            subid      = table_info->colnum;

            switch (subid) {
            case TCPCONNSTATE:
                state = entry->TCPTABLE_STATE;
	        snmp_set_var_typed_value(requestvb, ASN_INTEGER,
                                 (u_char *)&state, sizeof(state));
                break;
            case TCPCONNLOCALADDRESS:
#if defined(osf5) && defined(IN6_EXTRACT_V4ADDR)
	        snmp_set_var_typed_value(requestvb, ASN_IPADDRESS,
                              (u_char*)IN6_EXTRACT_V4ADDR(&entry->pcb.inp_laddr),
                                sizeof(IN6_EXTRACT_V4ADDR(&entry->pcb.inp_laddr)));
#else
	        snmp_set_var_typed_value(requestvb, ASN_IPADDRESS,
                                 (u_char *)&entry->TCPTABLE_LOCALADDRESS,
                                     sizeof(entry->TCPTABLE_LOCALADDRESS));
#endif
                break;
            case TCPCONNLOCALPORT:
                port = TCP_PORT_TO_HOST_ORDER((u_short)entry->TCPTABLE_LOCALPORT);
	        snmp_set_var_typed_value(requestvb, ASN_INTEGER,
                                 (u_char *)&port, sizeof(port));
                break;
            case TCPCONNREMOTEADDRESS:
#if defined(osf5) && defined(IN6_EXTRACT_V4ADDR)
	        snmp_set_var_typed_value(requestvb, ASN_IPADDRESS,
                              (u_char*)IN6_EXTRACT_V4ADDR(&entry->pcb.inp_laddr),
                                sizeof(IN6_EXTRACT_V4ADDR(&entry->pcb.inp_laddr)));
#else
	        snmp_set_var_typed_value(requestvb, ASN_IPADDRESS,
                                 (u_char *)&entry->TCPTABLE_REMOTEADDRESS,
                                     sizeof(entry->TCPTABLE_REMOTEADDRESS));
#endif
                break;
            case TCPCONNREMOTEPORT:
                port = TCP_PORT_TO_HOST_ORDER((u_short)entry->TCPTABLE_REMOTEPORT);
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
        snmp_log(LOG_WARNING, "mibII/tcpTable: Unsupported mode (%d)\n",
                               reqinfo->mode);
        break;
    default:
        snmp_log(LOG_WARNING, "mibII/tcpTable: Unrecognised mode (%d)\n",
                               reqinfo->mode);
        break;
    }

    return SNMP_ERR_NOERROR;
}

#ifndef NETSNMP_FEATURE_REMOVE_TCP_COUNT_CONNECTIONS
int
TCP_Count_Connections( void ) {
    tcpTable_load(NULL, NULL);
    return tcp_estab;
}
#endif /* NETSNMP_FEATURE_REMOVE_TCP_COUNT_CONNECTIONS */

	/*
	 * Two forms of iteration hook routines:
	 *    One for when the TCP table is stored as a table
	 *    One for when the TCP table is stored as a linked list
	 *
	 * Also applies to the cache-handler free routine
	 */

#ifdef	TCPTABLE_IS_TABLE
netsnmp_variable_list *
tcpTable_first_entry(void **loop_context,
                     void **data_context,
                     netsnmp_variable_list *index,
                     netsnmp_iterator_info *data)
{
    /*
     * XXX - How can we tell if the cache is valid?
     *       No access to 'reqinfo'
     */
    if (tcp_size == 0)
        return NULL;

    /*
     * Point to the first entry, and use the
     * 'next_entry' hook to retrieve this row
     */
    *loop_context = 0;
    return tcpTable_next_entry( loop_context, data_context, index, data );
}

netsnmp_variable_list *
tcpTable_next_entry( void **loop_context,
                     void **data_context,
                     netsnmp_variable_list *index,
                     netsnmp_iterator_info *data)
{
    int i = (int)*loop_context;
    netsnmp_variable_list *idx;
    long port;

    if (tcp_size < i)
        return NULL;

    /*
     * Set up the indexing for the specified row...
     */
    idx = index;
#if defined (WIN32) || defined (cygwin)
    port = ntohl((u_long)tcp_head[i].TCPTABLE_LOCALADDRESS);
    snmp_set_var_value(idx, (u_char *)&port,
                                sizeof(tcp_head[i].TCPTABLE_LOCALADDRESS));
#else
    snmp_set_var_value(idx, (u_char *)&tcp_head[i].TCPTABLE_LOCALADDRESS,
                                sizeof(tcp_head[i].TCPTABLE_LOCALADDRESS));
#endif

    port = TCP_PORT_TO_HOST_ORDER((u_short)tcp_head[i].TCPTABLE_LOCALPORT);
    idx = idx->next_variable;
    snmp_set_var_value(idx, (u_char*)&port, sizeof(port));

    idx = idx->next_variable;
#if defined (WIN32) || defined (cygwin)
    port = ntohl((u_long)tcp_head[i].TCPTABLE_REMOTEADDRESS);
    snmp_set_var_value(idx, (u_char *)&port,
                                sizeof(tcp_head[i].TCPTABLE_REMOTEADDRESS));
#else
    snmp_set_var_value(idx, (u_char *)&tcp_head[i].TCPTABLE_REMOTEADDRESS,
                                sizeof(tcp_head[i].TCPTABLE_REMOTEADDRESS));
#endif

    port = TCP_PORT_TO_HOST_ORDER((u_short)tcp_head[i].TCPTABLE_REMOTEPORT);
    idx = idx->next_variable;
    snmp_set_var_value(idx, (u_char*)&port, sizeof(port));

    /*
     * ... return the data structure for this row,
     * and update the loop context ready for the next one.
     */
    *data_context = (void*)&tcp_head[i];
    *loop_context = (void*)++i;

    return index;
}

void
tcpTable_free(netsnmp_cache *cache, void *magic)
{
#if defined (WIN32) || defined (cygwin)
    if (tcp_head) {
		/* the allocated structure is a count followed by table entries */
		free((char *)(tcp_head) - sizeof(DWORD));
	}
#else
	if (tcp_head)
        free(tcp_head);
#endif
    tcp_head  = NULL;
    tcp_size  = 0;
    tcp_estab = 0;
}
#else
#ifdef TCPTABLE_IS_LINKED_LIST
netsnmp_variable_list *
tcpTable_first_entry(void **loop_context,
                     void **data_context,
                     netsnmp_variable_list *index,
                     netsnmp_iterator_info *data)
{
    /*
     * XXX - How can we tell if the cache is valid?
     *       No access to 'reqinfo'
     */
    if (tcp_head == NULL)
        return NULL;

    /*
     * Point to the first entry, and use the
     * 'next_entry' hook to retrieve this row
     */
    *loop_context = (void*)tcp_head;
    return tcpTable_next_entry( loop_context, data_context, index, data );
}

netsnmp_variable_list *
tcpTable_next_entry( void **loop_context,
                     void **data_context,
                     netsnmp_variable_list *index,
                     netsnmp_iterator_info *data)
{
    TCPTABLE_ENTRY_TYPE	 *entry = (TCPTABLE_ENTRY_TYPE *)*loop_context;
    netsnmp_variable_list *idx;
    long addr, port;

    if (!entry)
        return NULL;

    /*
     * Set up the indexing for the specified row...
     */
    idx = index;
#if defined(osf5) && defined(IN6_EXTRACT_V4ADDR)
    addr = ntohl(IN6_EXTRACT_V4ADDR(&entry->pcb.inp_laddr));
#else
    addr = ntohl(entry->TCPTABLE_LOCALADDRESS);
#endif
    snmp_set_var_value(idx, (u_char *)&addr, sizeof(addr));

    port = TCP_PORT_TO_HOST_ORDER(entry->TCPTABLE_LOCALPORT);
    idx = idx->next_variable;
    snmp_set_var_value(idx, (u_char*)&port, sizeof(port));

    idx = idx->next_variable;
#if defined(osf5) && defined(IN6_EXTRACT_V4ADDR)
    addr = ntohl(IN6_EXTRACT_V4ADDR(&entry->pcb.inp_faddr));
#else
    addr = ntohl(entry->TCPTABLE_REMOTEADDRESS);
#endif
    snmp_set_var_value(idx, (u_char *)&addr, sizeof(addr));

    port = TCP_PORT_TO_HOST_ORDER(entry->TCPTABLE_REMOTEPORT);
    idx = idx->next_variable;
    snmp_set_var_value(idx, (u_char*)&port, sizeof(port));

    /*
     * ... return the data structure for this row,
     * and update the loop context ready for the next one.
     */
    *data_context = (void*)entry;
    *loop_context = (void*)entry->INP_NEXT_SYMBOL;
    return index;
}

void
tcpTable_free(netsnmp_cache *cache, void *magic)
{
    TCPTABLE_ENTRY_TYPE *p;
    while (tcp_head) {
        p = tcp_head;
        tcp_head = tcp_head->INP_NEXT_SYMBOL;
        free(p);
    }

    tcp_head  = NULL;
    tcp_size  = 0;
    tcp_estab = 0;
}
#endif		/* TCPTABLE_IS_LINKED_LIST */
#endif		/* TCPTABLE_IS_TABLE */


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
tcpTable_load(netsnmp_cache *cache, void *vmagic)
{
    int             fd;
    struct nmparms  p;
    int             val = 0;
    unsigned int    ulen;
    int             ret;
    int             i;

    tcpTable_free(NULL, NULL);

    if ((fd = open_mib("/dev/ip", O_RDONLY, 0, NM_ASYNC_OFF)) >= 0) {
        p.objid = ID_tcpConnNumEnt;
        p.buffer = (void *) &val;
        ulen = sizeof(int);
        p.len = &ulen;
        if ((ret = get_mib_info(fd, &p)) == 0)
            tcp_size = val;

        if (tcp_size > 0) {
            ulen = (unsigned) tcp_size *sizeof(mib_tcpConnEnt);
            tcp_head = (mib_tcpConnEnt *) malloc(ulen);
            p.objid = ID_tcpConnTable;
            p.buffer = (void *) tcp_head;
            p.len = &ulen;
            if ((ret = get_mib_info(fd, &p)) < 0) {
                tcp_size = 0;
            }
        }

        close_mib(fd);
    }

    /*
     * Count the number of established connections
     * Probably not actually necessary for HP-UX
     */
    for (i = 0; i < tcp_size; i++) {
        if (tcp_head[i].State == 5 /* established */ ||
            tcp_head[i].State == 8 /*  closeWait  */ )
            tcp_estab++;
    }

    if (tcp_size > 0) {
        DEBUGMSGTL(("mibII/tcpTable", "Loaded TCP Table (hpux11)\n"));
        return 0;
    }
    DEBUGMSGTL(("mibII/tcpTable", "Failed to load TCP Table (hpux11)\n"));
    return -1;
}
#else                           /* hpux11 */

#ifdef linux

/*  see <netinet/tcp.h> */
#define TCP_ALL ((1 << (TCP_CLOSING + 1)) - 1)

#if HAVE_NETLINK_NETLINK_H
static int
tcpTable_load_netlink(void)
{
	/*  TODO: perhaps use permanent nl handle? */
	struct nl_handle *nl = nl_handle_alloc();

	if (nl == NULL) {
		DEBUGMSGTL(("mibII/tcpTable", "Failed to allocate netlink handle\n"));
		snmp_log(LOG_ERR, "snmpd: Failed to allocate netlink handle\n");
		return -1;
	}

	if (nl_connect(nl, NETLINK_INET_DIAG) < 0) {
		DEBUGMSGTL(("mibII/tcpTable", "Failed to connect to netlink: %s\n", nl_geterror()));
		snmp_log(LOG_ERR, "snmpd: Couldn't connect to netlink: %s\n", nl_geterror());
		nl_handle_destroy(nl);
		return -1;
	}

	struct inet_diag_req req = {
		.idiag_family = AF_INET,
		.idiag_states = TCP_ALL,
	};

	struct nl_msg *nm = nlmsg_alloc_simple(TCPDIAG_GETSOCK, NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST);
	nlmsg_append(nm, &req, sizeof(struct inet_diag_req), 0);

	if (nl_send_auto_complete(nl, nm) < 0) {
		DEBUGMSGTL(("mibII/tcpTable", "nl_send_autocomplete(): %s\n", nl_geterror()));
		snmp_log(LOG_ERR, "snmpd: nl_send_autocomplete(): %s\n", nl_geterror());
		nl_handle_destroy(nl);
		return -1;
	}
	nlmsg_free(nm);

	struct sockaddr_nl peer;
	unsigned char *buf = NULL;
	int running = 1, len;

	while (running) {
		if ((len = nl_recv(nl, &peer, &buf, NULL)) <= 0) {
			DEBUGMSGTL(("mibII/tcpTable", "nl_recv(): %s\n", nl_geterror()));
			snmp_log(LOG_ERR, "snmpd: nl_recv(): %s\n", nl_geterror());
			nl_handle_destroy(nl);
			return -1;
		}

		struct nlmsghdr *h = (struct nlmsghdr*)buf;
		while (nlmsg_ok(h, len)) {
			if (h->nlmsg_type == NLMSG_DONE) {
				running = 0;
				break;
			}

			struct inet_diag_msg *r = nlmsg_data(h);

			if (r->idiag_family != AF_INET) {
				h = nlmsg_next(h, &len);
				continue;
			}

			struct inpcb    pcb, *nnew;
			static int      linux_states[12] =
				{ 1, 5, 3, 4, 6, 7, 11, 1, 8, 9, 2, 10 };

			memcpy(&pcb.inp_laddr.s_addr, r->id.idiag_src, r->idiag_family == AF_INET ? 4 : 6);
			memcpy(&pcb.inp_faddr.s_addr, r->id.idiag_dst, r->idiag_family == AF_INET ? 4 : 6);

			pcb.inp_lport = r->id.idiag_sport;
			pcb.inp_fport = r->id.idiag_dport;

			pcb.inp_state = (r->idiag_state & 0xf) < 12 ? linux_states[r->idiag_state & 0xf] : 2;
			if (pcb.inp_state == 5 /* established */ ||
				pcb.inp_state == 8 /*  closeWait  */ )
				tcp_estab++;
			pcb.uid = r->idiag_uid;

			nnew = SNMP_MALLOC_TYPEDEF(struct inpcb);
			if (nnew == NULL) {
				running = 0;
				/*  XXX report malloc error and return -1? */
				break;
			}
			memcpy(nnew, &pcb, sizeof(struct inpcb));
			nnew->inp_next = tcp_head;
			tcp_head       = nnew;

			h = nlmsg_next(h, &len);
		}
		free(buf);
	}

	nl_handle_destroy(nl);

	if (tcp_head) {
		DEBUGMSGTL(("mibII/tcpTable", "Loaded TCP Table using netlink\n"));
		return 0;
	}
	DEBUGMSGTL(("mibII/tcpTable", "Failed to load TCP Table (netlink)\n"));
	return -1;
}
#endif

int
tcpTable_load(netsnmp_cache *cache, void *vmagic)
{
    FILE           *in;
    char            line[256];

    tcpTable_free(cache, NULL);

#if HAVE_NETLINK_NETLINK_H
	if (tcpTable_load_netlink() == 0) {
		return 0;
	}
#endif

    if (!(in = fopen("/proc/net/tcp", "r"))) {
        DEBUGMSGTL(("mibII/tcpTable", "Failed to load TCP Table (linux1)\n"));
        NETSNMP_LOGONCE((LOG_ERR, "snmpd: cannot open /proc/net/tcp ...\n"));
        return -1;
    }

    /*
     * scan proc-file and build up a linked list 
     * This will actually be built up in reverse,
     *   but since the entries are unsorted, that doesn't matter.
     */
    while (line == fgets(line, sizeof(line), in)) {
        struct inpcb    pcb, *nnew;
        static int      linux_states[12] =
            { 1, 5, 3, 4, 6, 7, 11, 1, 8, 9, 2, 10 };
        unsigned int    lp, fp;
        int             state, uid;

        if (6 != sscanf(line,
                        "%*d: %x:%x %x:%x %x %*X:%*X %*X:%*X %*X %d",
                        &pcb.inp_laddr.s_addr, &lp,
                        &pcb.inp_faddr.s_addr, &fp, &state, &uid))
            continue;

        pcb.inp_lport = htons((unsigned short) lp);
        pcb.inp_fport = htons((unsigned short) fp);

        pcb.inp_state = (state & 0xf) < 12 ? linux_states[state & 0xf] : 2;
        if (pcb.inp_state == 5 /* established */ ||
            pcb.inp_state == 8 /*  closeWait  */ )
            tcp_estab++;
        pcb.uid = uid;

        nnew = SNMP_MALLOC_TYPEDEF(struct inpcb);
        if (nnew == NULL)
            break;
        memcpy(nnew, &pcb, sizeof(struct inpcb));
        nnew->inp_next = tcp_head;
        tcp_head       = nnew;
    }

    fclose(in);

    DEBUGMSGTL(("mibII/tcpTable", "Loaded TCP Table (linux)\n"));
    return 0;
}
#else                           /* linux */

#ifdef solaris2
static int
TCP_Cmp(void *addr, void *ep)
{
    if (memcmp((mib2_tcpConnEntry_t *) ep, (mib2_tcpConnEntry_t *) addr,
               sizeof(mib2_tcpConnEntry_t)) == 0)
        return (0);
    else
        return (1);
}

int
tcpTable_load(netsnmp_cache *cache, void *vmagic)
{
    mib2_tcpConnEntry_t   entry;
    netsnmp_tcpConnEntry *nnew;
    netsnmp_tcpConnEntry *prev_entry = NULL;

    tcpTable_free(NULL, NULL);

    if (getMibstat(MIB_TCP_CONN, &entry, sizeof(mib2_tcpConnEntry_t),
                   GET_FIRST, &TCP_Cmp, &entry) != 0) {
        DEBUGMSGTL(("mibII/tcpTable", "Failed to load TCP Table (solaris)\n"));
        return -1;
    }

    while (1) {
        /*
         * Build up a linked list copy of the getMibstat results
         * Note that since getMibstat returns rows in sorted order,
         *    we need to retain this order while building the list
         *    so new entries are added onto the end of the list.
         * Note 2: at least Solaris 8-10 do not return rows in
         *    sorted order anymore
         */
        nnew = SNMP_MALLOC_TYPEDEF(netsnmp_tcpConnEntry);
        if (nnew == NULL)
            break;
        memcpy(&(nnew->entry), &entry, sizeof(mib2_tcpConnEntry_t));
        if (!prev_entry)
            tcp_head = nnew;
        else
            prev_entry->inp_next = nnew;
        prev_entry = nnew;

        if (getMibstat(MIB_TCP_CONN, &entry, sizeof(mib2_tcpConnEntry_t),
                       GET_NEXT, &TCP_Cmp, &entry) != 0)
	    break;
    }

    if (tcp_head) {
        DEBUGMSGTL(("mibII/tcpTable", "Loaded TCP Table (solaris)\n"));
        return 0;
    }
    DEBUGMSGTL(("mibII/tcpTable", "Failed to load TCP Table (solaris)\n"));
    return -1;
}
#else                           /* solaris2 */

#if defined (WIN32) || defined (cygwin)
int
tcpTable_load(netsnmp_cache *cache, void *vmagic)
{
    PMIB_TCPTABLE pTcpTable = NULL;
    DWORD         dwActualSize = 0;
    DWORD         status = NO_ERROR;

    /*
     * query for the buffer size needed 
     */
    status = GetTcpTable(pTcpTable, &dwActualSize, TRUE);
    if (status == ERROR_INSUFFICIENT_BUFFER) {
        pTcpTable = (PMIB_TCPTABLE) malloc(dwActualSize);
        if (pTcpTable != NULL) {
            /*
             * Get the sorted TCP table 
             */
            status = GetTcpTable(pTcpTable, &dwActualSize, TRUE);
        }
    }

    if (status == NO_ERROR) {
        int           i;

        DEBUGMSGTL(("mibII/tcpTable", "Loaded TCP Table (WIN32)\n"));
        tcp_size = pTcpTable->dwNumEntries -1;  /* entries are counted starting with 0 */
        tcp_head = pTcpTable->table;

	/*
	 * Count the number of established connections
	 * Probably not actually necessary for Windows
	 */
	for (i = 0; i < tcp_size; i++) {
		if (tcp_head[i].dwState == 5 /* established */ ||
			tcp_head[i].dwState == 8 /*  closeWait  */ )
			tcp_estab++;
	}
        return 0;
    }

    DEBUGMSGTL(("mibII/tcpTable", "Failed to load TCP Table (win32)\n"));
	if (pTcpTable)
		free(pTcpTable);
    return -1;
}
#else                           /* WIN32 cygwin */

#if (defined(NETSNMP_CAN_USE_SYSCTL) && defined(TCPCTL_PCBLIST))

#if defined(freebsd4) || defined(darwin)
    #define NS_ELEM struct xtcpcb
#else
    #define NS_ELEM struct xinpcb
#endif

int
tcpTable_load(netsnmp_cache *cache, void *vmagic)
{
    size_t   len;
    int      sname[] = { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_PCBLIST };
    char     *tcpcb_buf = NULL;
#if defined(dragonfly)
    struct xinpcb  *xig = NULL;
    int      StateMap[] = { 1, 1, 2, 3, 4, 5, 8, 6, 10, 9, 7, 11 };
#else
    struct xinpgen *xig = NULL;
    int      StateMap[] = { 1, 2, 3, 4, 5, 8, 6, 10, 9, 7, 11 };
#endif
    netsnmp_inpcb  *nnew;

    tcpTable_free(NULL, NULL);

    /*
     *  Read in the buffer containing the TCP table data
     */
    len = 0;
    if (sysctl(sname, 4, 0, &len, 0, 0) < 0 ||
       (tcpcb_buf = malloc(len)) == NULL)
        return -1;
    if (sysctl(sname, 4, tcpcb_buf, &len, 0, 0) < 0) {
        free(tcpcb_buf);
        return -1;
    }

    /*
     *  Unpick this into the constituent 'xinpgen' structures, and extract
     *     the 'inpcb' elements into a linked list (built in reverse)
     */
#if defined(dragonfly)
    xig = (struct xinpcb  *) tcpcb_buf;
#else
    xig = (struct xinpgen *) tcpcb_buf;
    xig = (struct xinpgen *) ((char *) xig + xig->xig_len);
#endif

#if defined(dragonfly)
    while (xig && ((char *)xig + xig->xi_len < tcpcb_buf + len))
#else
    while (xig && (xig->xig_len > sizeof(struct xinpgen)))
#endif
    {
        nnew = SNMP_MALLOC_TYPEDEF(netsnmp_inpcb);
        if (!nnew)
            break;
        nnew->state = StateMap[((NS_ELEM *) xig)->xt_tp.t_state];
        if (nnew->state == 5 /* established */ ||
            nnew->state == 8 /*  closeWait  */ )
            tcp_estab++;
        memcpy(&(nnew->pcb), &(((NS_ELEM *) xig)->xt_inp),
                           sizeof(struct inpcb));

	if (nnew->pcb.inp_vflag & INP_IPV6)
	    free(nnew);
	else {
	    nnew->inp_next = tcp_head;
	    tcp_head   = nnew;
	}
#if defined(dragonfly)
        xig = (struct xinpcb  *) ((char *) xig + xig->xi_len);
#else
        xig = (struct xinpgen *) ((char *) xig + xig->xig_len);
#endif
    }

    free(tcpcb_buf);
    if (tcp_head) {
        DEBUGMSGTL(("mibII/tcpTable", "Loaded TCP Table (sysctl)\n"));
        return 0;
    }
    DEBUGMSGTL(("mibII/tcpTable", "Failed to load TCP Table (sysctl)\n"));
    return -1;
}
#undef NS_ELEM

#else		/* (defined(NETSNMP_CAN_USE_SYSCTL) && defined(TCPCTL_PCBLIST)) */
#ifdef PCB_TABLE
int
tcpTable_load(netsnmp_cache *cache, void *vmagic)
{
    struct inpcbtable table;
    struct inpcb   *entry;
    struct tcpcb    tcpcb;
    netsnmp_inpcb  *nnew;
    int      StateMap[] = { 1, 2, 3, 4, 5, 8, 6, 10, 9, 7, 11 };

    tcpTable_free(NULL, NULL);

    if (!auto_nlist(TCP_SYMBOL, (char *) &table, sizeof(table))) {
        DEBUGMSGTL(("mibII/tcpTable", "Failed to read inpcbtable\n"));
        return -1;
    }

    /*
     *  Set up a linked list
     */
    entry  = table.inpt_queue.cqh_first;
    while (entry) {
   
        nnew = SNMP_MALLOC_TYPEDEF(netsnmp_inpcb);
        if (!nnew)
            break;
        if (!NETSNMP_KLOOKUP(entry, (char *)&(nnew->pcb), sizeof(struct inpcb))) {
            DEBUGMSGTL(("mibII/tcpTable:TcpTable_load", "klookup failed\n"));
            break;
        }

        if (!NETSNMP_KLOOKUP(nnew->pcb.inp_ppcb, (char *)&tcpcb, sizeof(struct tcpcb))) {
            DEBUGMSGTL(("mibII/tcpTable:TcpTable_load", "klookup failed\n"));
            break;
        }
	nnew->state = StateMap[tcpcb.t_state];
        if (nnew->state == 5 /* established */ ||
            nnew->state == 8 /*  closeWait  */ )
            tcp_estab++;

        entry      = nnew->INP_NEXT_SYMBOL;	/* Next kernel entry */
	nnew->inp_next = tcp_head;
	tcp_head   = nnew;

        if (entry == table.inpt_queue.cqh_first)
            break;
    }

    if (tcp_head) {
        DEBUGMSGTL(("mibII/tcpTable", "Loaded TCP Table (pcb_table)\n"));
        return 0;
    }
    DEBUGMSGTL(("mibII/tcpTable", "Failed to load TCP Table (pcb_table)\n"));
    return -1;
}

#else				/* PCB_TABLE */
#ifdef TCP_SYMBOL
int
tcpTable_load(netsnmp_cache *cache, void *vmagic)
{
    struct inpcb   tcp_inpcb;
    struct tcpcb   tcpcb;
    netsnmp_inpcb  *nnew;
    struct inpcb   *entry;
#ifdef hpux
    int      StateMap[] = { 1, 2, 3, -1, 4, 5, 8, 6, 10, 9, 7, 11 };
#else
    int      StateMap[] = { 1, 2, 3,     4, 5, 8, 6, 10, 9, 7, 11 };
#endif

    tcpTable_free(NULL, NULL);

    if (!auto_nlist(TCP_SYMBOL, (char *) &tcp_inpcb, sizeof(tcp_inpcb))) {
        DEBUGMSGTL(("mibII/tcpTable", "Failed to read tcp_symbol\n"));
        return -1;
    }

    /*
     *  Set up a linked list
     */
    entry  = tcp_inpcb.INP_NEXT_SYMBOL;
    while (entry) {
   
        nnew = SNMP_MALLOC_TYPEDEF(netsnmp_inpcb);
        if (!nnew)
            break;
        if (!NETSNMP_KLOOKUP(entry, (char *)&(nnew->pcb), sizeof(struct inpcb))) {
            DEBUGMSGTL(("mibII/tcpTable:tcpTable_load", "klookup failed\n"));
            break;
        }
        if (!NETSNMP_KLOOKUP(nnew->pcb.inp_ppcb, (char *)&tcpcb, sizeof(struct tcpcb))) {
            DEBUGMSGTL(("mibII/tcpTable:tcpTable_load", "klookup failed\n"));
            break;
        }
	nnew->state    = StateMap[tcpcb.t_state];
        if (nnew->state == 5 /* established */ ||
            nnew->state == 8 /*  closeWait  */ )
            tcp_estab++;

        entry          = nnew->pcb.INP_NEXT_SYMBOL;	/* Next kernel entry */
	nnew->inp_next = tcp_head;
	tcp_head       = nnew;

        if (entry == tcp_inpcb.INP_NEXT_SYMBOL)
            break;
    }

    if (tcp_head) {
        DEBUGMSGTL(("mibII/tcpTable", "Loaded TCP Table (tcp_symbol)\n"));
        return 0;
    }
    DEBUGMSGTL(("mibII/tcpTable", "Failed to load TCP Table (tcp_symbol)\n"));
    return -1;
}

#else				/* UDB_SYMBOL */
int
tcpTable_load(netsnmp_cache *cache, void *vmagic)
{
    DEBUGMSGTL(("mibII/tcpTable", "Loading TCP Table not implemented\n"));
    return -1;
}
#endif				/* UDB_SYMBOL */
#endif				/* PCB_TABLE */
#endif		/* (defined(NETSNMP_CAN_USE_SYSCTL) && defined(TCPCTL_PCBLIST)) */
#endif                          /* WIN32 cygwin */
#endif                          /* linux */
#endif                          /* solaris2 */
#endif                          /* hpux11 */
