/*	$OpenBSD: if.c,v 1.42 2005/03/13 16:05:50 mpf Exp $	*/
/*	$NetBSD: if.c,v 1.16.4.2 1996/06/07 21:46:46 thorpej Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef  INHERITED_CODE
#ifndef lint
#if 0
static char sccsid[] = "from: @(#)if.c	8.2 (Berkeley) 2/21/94";
#else
static char *rcsid = "$OpenBSD: if.c,v 1.42 2005/03/13 16:05:50 mpf Exp $";
#endif
#endif /* not lint */
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 1
#endif
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1
#endif
#include <signal.h>

#include "main.h"
#include "netstat.h"

#define	YES	1
#define	NO	0

static void sidewaysintpr(u_int);
static void timerSet(int interval_seconds);
static void timerPause(void);

    struct _if_info {
        char            name[128];
        char            ip[128], route[128];
        int             mtu;
        int             drops;
        unsigned int    ifindex;
                        /*
                         * Save "expandable" fields as string values
                         *  rather than integer statistics
                         */
        char            s_ipkts[20], s_ierrs[20];
        char            s_opkts[20], s_oerrs[20];
        char            s_ibytes[20], s_obytes[20];
        char            s_outq[20];
        unsigned long   ipkts, opkts;  /* Need to combine 2 MIB values */
        int             operstatus;
/*
        u_long          netmask;
        struct in_addr  ifip, ifroute;
 */
        struct _if_info *next;
    };


/*
 * Retrieve the interface addressing information
 * XXX - This could also be extended to handle non-IP interfaces
 */
void
_set_address( struct _if_info *cur_if )
{
    oid    ipaddr_oid[] = { 1,3,6,1,2,1,4,20,1,0 };
    size_t ipaddr_len   = OID_LENGTH( ipaddr_oid );
    static netsnmp_variable_list *addr_if_var  =NULL;
    static netsnmp_variable_list *addr_mask_var=NULL;
    netsnmp_variable_list *vp, *vp2;
    union {
        in_addr_t addr;
        char      data[4];
    } tmpAddr;
    char *cp;
    in_addr_t ifAddr, mask;

        /*
         *  Note that this information only needs to be retrieved 
         *    once, and can be re-used for subsequent calls.
         */
    if ( addr_if_var == NULL ) {
        ipaddr_oid[ 9 ] = 2;  /* ipAdEntIfIndex */
        snmp_varlist_add_variable( &addr_if_var, ipaddr_oid, ipaddr_len,
                                   ASN_NULL, NULL,  0);
        netsnmp_query_walk( addr_if_var, ss );

        ipaddr_oid[ 9 ] = 3;  /* ipAdEntNetMask */
        snmp_varlist_add_variable( &addr_mask_var, ipaddr_oid, ipaddr_len,
                                   ASN_NULL, NULL,  0);
        netsnmp_query_walk( addr_mask_var, ss );
    }

    /*
     * Find the address row relevant to this interface
     */
    for (vp=addr_if_var, vp2=addr_mask_var;  vp;
         vp=vp->next_variable, vp2=vp2->next_variable) {
        if ( vp->val.integer && *vp->val.integer == (int)cur_if->ifindex )
            break;
    }
    if (vp2) {
        /*
         * Always want a numeric interface IP address
         */
        snprintf( cur_if->ip, 128, "%" NETSNMP_PRIo "u.%" NETSNMP_PRIo "u."
                  "%" NETSNMP_PRIo "u.%" NETSNMP_PRIo "u",
                  vp2->name[10],
                  vp2->name[11],
                  vp2->name[12],
                  vp2->name[13]);

        /*
         * But re-use the routing table utilities/code for
         *   displaying the local network information
         */
        cp = tmpAddr.data;
        cp[0] = vp2->name[ 10 ] & 0xff;
        cp[1] = vp2->name[ 11 ] & 0xff;
        cp[2] = vp2->name[ 12 ] & 0xff;
        cp[3] = vp2->name[ 13 ] & 0xff;
        ifAddr = tmpAddr.addr;
        cp = tmpAddr.data;
        cp[0] = vp2->val.string[ 0 ] & 0xff;
        cp[1] = vp2->val.string[ 1 ] & 0xff;
        cp[2] = vp2->val.string[ 2 ] & 0xff;
        cp[3] = vp2->val.string[ 3 ] & 0xff;
        mask = tmpAddr.addr;
        snprintf( cur_if->route, 128, "%s", netname(ifAddr, mask));
    }
}


/*
 * Print a description of the network interfaces.
 */
void
intpr(int interval)
{
    oid    ifcol_oid[]  = { 1,3,6,1,2,1,2,2,1,0 };
    size_t ifcol_len    = OID_LENGTH( ifcol_oid );

    struct _if_info *if_head, *if_tail, *cur_if;
    netsnmp_variable_list *var, *vp;
           /*
            * Track maximum field widths, expanding as necessary
            *   This is one reason why results can't be
            *   displayed immediately they are retrieved.
            */
    int    max_name  = 4, max_ip    = 7, max_route = 7, max_outq  = 5;
    int    max_ipkts = 5, max_ierrs = 5, max_opkts = 5, max_oerrs = 5;
    int    max_ibytes = 6, max_obytes = 6;
    int    i;


    if (interval) {
        sidewaysintpr((unsigned)interval);
        return;
    }

        /*
         * The traditional "netstat -i" output combines information
         *   from two SNMP tables:
         *      ipAddrTable   (for the IP address/network)
         *      ifTable       (for the interface statistics)
         *
         * The previous approach was to retrieve (and save) the
         *   address information first. Then walk the main ifTable,
         *   add the relevant stored addresses, and saving the
         *   full information for each interface, before displaying 
         *   the results as a separate pass.
         *
         * This code reverses this general structure, by first retrieving
         *   (and storing) the interface statistics for the whole table,
         *   then inserting the address information obtained from the
         *   ipAddrTable, and finally displaying the results.
         * Such an arrangement should make it easier to extend this
         *   to handle non-IP interfaces (hence not in ipAddrTable)
         */
    if_head = NULL;
    if_tail = NULL;
    var     = NULL;

#define ADD_IFVAR( x ) ifcol_oid[ ifcol_len-1 ] = x; \
    snmp_varlist_add_variable( &var, ifcol_oid, ifcol_len, ASN_NULL, NULL,  0)
    ADD_IFVAR( 2 );                 /* ifName  */
    ADD_IFVAR( 4 );                 /* ifMtu   */
    ADD_IFVAR( 8 );                 /* ifOperStatus */
    /*
     * The Net/Open-BSD behaviour is to display *either* byte
     *   counts *or* packet/error counts (but not both). FreeBSD
     *   integrates the byte counts into the traditional display.
     *
     * The previous 'snmpnetstat' implementation followed the
     *   separatist model.  This re-write offers an opportunity
     *   to adopt the (more useful, IMO) Free-BSD approach.
     *
     * Or we could perhaps support both styles? :-)
     */
    if (bflag || oflag) {
        ADD_IFVAR( 10 );            /* ifInOctets   */
        ADD_IFVAR( 16 );            /* ifOutOctets  */
    }
    if (!oflag) {
        ADD_IFVAR( 11 );            /* ifInUcastPkts  */
        ADD_IFVAR( 12 );            /* ifInNUcastPkts */
        ADD_IFVAR( 14 );            /* ifInErrors     */
        ADD_IFVAR( 17 );            /* ifOutUcastPkts */
        ADD_IFVAR( 18 );            /* ifOutNUcastPkts */
        ADD_IFVAR( 20 );            /* ifOutErrors    */
        ADD_IFVAR( 21 );            /* ifOutQLen      */
    }
#if 0
    if (tflag) {
        ADD_IFVAR( XX );            /* ??? */
    }
#endif
    if (dflag) {
        ADD_IFVAR( 19 );            /* ifOutDiscards  */
    }
#undef ADD_IFVAR

        /*
	 * Now walk the ifTable, creating a list of interfaces
	 */
    while ( 1 ) {
        if (netsnmp_query_getnext( var, ss ) != SNMP_ERR_NOERROR)
            break;
        ifcol_oid[ ifcol_len-1 ] = 2;	/* ifDescr */
        if ( snmp_oid_compare( ifcol_oid, ifcol_len,
                               var->name, ifcol_len) != 0 )
            break;    /* End of Table */
        cur_if = SNMP_MALLOC_TYPEDEF( struct _if_info );
        if (!cur_if)
            break;
        cur_if->ifindex = var->name[ var->name_length-1 ];
        for ( vp=var; vp; vp=vp->next_variable ) {
            if ( ! vp->val.integer )
                continue;
            if ( var->name[ var->name_length-1 ] != cur_if->ifindex ) {
                /*
                 * Inconsistent index information
                 * XXX - Try to recover ?
                 */
                SNMP_FREE( cur_if );
                break;    /* not for now, no */
            }
            switch ( vp->name[ var->name_length-2 ] ) {
            case 2:     /* ifDescr */
                if (vp->val_len >= sizeof(cur_if->name))
                    vp->val_len  = sizeof(cur_if->name)-1;
                memmove( cur_if->name, vp->val.string, vp->val_len );
                cur_if->name[vp->val_len] = 0;
                if ((i = strlen(cur_if->name) + 1) > max_name)
                    max_name = i;
                break;
            case 4:     /* ifMtu   */
                cur_if->mtu = *vp->val.integer;
                break;
            case 8:     /* ifOperStatus   */
                cur_if->operstatus = *vp->val.integer;
                /* XXX - any special processing ?? */
                break;
            case 10:	/* ifInOctets     */
                sprintf(cur_if->s_ibytes, "%lu", *vp->val.integer);
                i = strlen(cur_if->s_ibytes);
                if (i > max_ibytes)
                    max_ibytes = i;
                break;
            case 11:	/* ifInUcastPkts  */
                cur_if->ipkts += *vp->val.integer;
                sprintf(cur_if->s_ipkts, "%lu", cur_if->ipkts);
                i = strlen(cur_if->s_ipkts);
                if (i > max_ipkts)
                    max_ipkts = i;
                break;
            case 12:	/* ifInNUcastPkts  */
                cur_if->ipkts += *vp->val.integer;
                sprintf(cur_if->s_ipkts, "%lu", cur_if->ipkts);
                i = strlen(cur_if->s_ipkts);
                if (i > max_ipkts)
                    max_ipkts = i;
                break;
            case 14:	/* ifInErrors      */
                sprintf(cur_if->s_ierrs, "%lu", *vp->val.integer);
                i = strlen(cur_if->s_ierrs);
                if (i > max_ierrs)
                    max_ierrs = i;
                break;
            case 16:	/* ifOutOctets      */
                sprintf(cur_if->s_obytes, "%lu", *vp->val.integer);
                i = strlen(cur_if->s_obytes);
                if (i > max_obytes)
                    max_obytes = i;
                break;
            case 17:	/* ifOutUcastPkts */
                cur_if->opkts += *vp->val.integer;
                sprintf(cur_if->s_opkts, "%lu", cur_if->opkts);
                i = strlen(cur_if->s_opkts);
                if (i > max_opkts)
                    max_opkts = i;
                break;
            case 18:	/* ifOutNUcastPkts */
                cur_if->opkts += *vp->val.integer;
                sprintf(cur_if->s_opkts, "%lu", cur_if->opkts);
                i = strlen(cur_if->s_opkts);
                if (i > max_opkts)
                    max_opkts = i;
                break;
            case 19:    /* ifOutDiscards   */
                cur_if->drops = *vp->val.integer;
                break;
            case 20:	/* ifOutErrors     */
                sprintf(cur_if->s_oerrs, "%lu", *vp->val.integer);
                i = strlen(cur_if->s_oerrs);
                if (i > max_oerrs)
                    max_oerrs = i;
                break;
            case 21:	/* ifOutQLen       */
                sprintf(cur_if->s_outq, "%lu", *vp->val.integer);
                i = strlen(cur_if->s_outq);
                if (i > max_outq)
                    max_outq = i;
                break;
            }
        }

        /*
         *  XXX - Perhaps query ifXTable for additional info ??
         *    (ifName/ifAlias, or HC counters)
         */

        /*
         * If we're to monitor a particular interface, then
         *   ignore all others.  It would be more efficient
         *   to check this earlier (as part of processing 
         *   the varbind list).  But performing this test here
         *   means we can recognise ifXTable names as well)
         */
        if ( intrface && strcmp( cur_if->name, intrface ) != 0) {
            SNMP_FREE( cur_if );
        }

        /*
         * Insert the IP address and network settings, and
         *   add the new _if_stat structure to the list.
         */
        if ( cur_if ) {
            _set_address( cur_if );
            i = strlen(cur_if->ip);
            if (i > max_ip)
                max_ip = i;
            i = strlen(cur_if->route);
            if (i > max_route)
                max_route = i;

            if ( if_tail ) {
                if_tail->next = cur_if;
                if_tail       = cur_if;
            } else {
                if_head       = cur_if;
                if_tail       = cur_if;
            }
        }
    }   /* while (1) */

        /*
         * Now display the specified results (in Free-BSD format)
         *   setting the field widths appropriately....
         */
    printf("%*.*s %5.5s %*.*s %*.*s",
           -max_name,  max_name,  "Name", "Mtu",
           -max_route, max_route, "Network",
           -max_ip,    max_ip,    "Address");
    if (oflag) {
        printf(" %*s %*s", max_ibytes,  "Ibytes",
                           max_obytes,  "Obytes");
    } else {
        printf(" %*s %*s", max_ipkts,   "Ipkts",
                           max_ierrs,   "Ierrs");
        if (bflag) 
            printf(" %*s", max_ibytes,  "Ibytes");

        printf(" %*s %*s", max_opkts,   "Opkts",
                           max_oerrs,   "Oerrs");
        if (bflag) 
            printf(" %*s", max_obytes,  "Obytes");

        printf(" %*s",     max_outq,    "Queue");
    }
 /* if (tflag)
        printf(" %s", "Time");
  */
    if (dflag)
        printf(" %s", "Drop");
    putchar('\n');

    for (cur_if = if_head; cur_if; cur_if=cur_if->next) {
        if (cur_if->name[0] == 0)
            continue;
        printf( "%*.*s %5d", -max_name,  max_name,  cur_if->name, cur_if->mtu);
        printf(" %*.*s",     -max_route, max_route, cur_if->route);
        printf(" %*.*s",     -max_ip,    max_ip,    cur_if->ip);

        if (oflag) {
            printf(" %*s %*s", max_ibytes,  cur_if->s_ibytes,
                               max_obytes,  cur_if->s_obytes);
        } else {
            printf(" %*s %*s", max_ipkts,   cur_if->s_ipkts,
                               max_ierrs,   cur_if->s_ierrs);
            if (bflag) 
                printf(" %*s", max_ibytes,  cur_if->s_ibytes);
    
            printf(" %*s %*s", max_opkts,   cur_if->s_opkts,
                               max_oerrs,   cur_if->s_oerrs);
            if (bflag) 
                printf(" %*s", max_obytes,  cur_if->s_obytes);
            printf(" %*s",     max_outq,    cur_if->s_outq);
        }
     /* if (tflag)
            printf(" %4d", cur_if->???);
      */
        if (dflag)
            printf(" %4d", cur_if->drops);
        putchar('\n');
    }

        /*
         * ... and tidy up.
         */
    for (cur_if = if_head; cur_if; cur_if=if_head) {
        if_head=cur_if->next;
        cur_if->next = NULL;
        SNMP_FREE( cur_if );
    }
}


#define	MAXIF	100
struct	iftot {
	char	ift_name[128];		/* interface name */
        int     ifIndex;
	u_long	ift_ip;			/* input packets */
	u_long	ift_ib;			/* input bytes */
	u_long	ift_ie;			/* input errors */
	u_long	ift_op;			/* output packets */
	u_long	ift_ob;			/* output bytes */
	u_long	ift_oe;			/* output errors */
	u_long	ift_co;			/* collisions */
	u_long	ift_dr;			/* drops */
};

int signalled;	/* set if alarm goes off "early" */

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
static void
sidewaysintpr(unsigned int interval)
{
    /*
     * As with the "one-shot" interface display, there are
     *   two different possible output formats.  The Net/
     *   Open-BSD style displays both information about a
     *   single interface *and* the overall totals.
     * The equivalent Free-BSD approach is to report on one
     *   or the other (rather than both).  This is probably
     *   more useful (IMO), and significantly more efficient.
     *   So that's the style implemented here.
     *
     * Note that the 'ifcol' OID buffer can represent a full
     *   instance (including ifIndex), rather than just a
     *   column object OID, as with the one-shot code.
     */
    oid    ifcol_oid[]  = { 1,3,6,1,2,1,2,2,1,0,0 };
    size_t ifcol_len    = OID_LENGTH( ifcol_oid );
    netsnmp_variable_list *var, *vp;
    struct iftot *ip  = NULL, *cur_if = NULL;    /* single I/F display */
    struct iftot *sum = NULL, *total  = NULL;    /* overall summary    */
    int    line;
    int    first;
    size_t i;

    var = NULL;
    if ( intrface ) {
        /*
         * Locate the ifIndex of the interface to monitor,
         *   by walking the ifDescr column of the ifTable
         */
        ifcol_oid[ ifcol_len-2 ] = 2;   /* ifDescr  */
        snmp_varlist_add_variable( &var, ifcol_oid, ifcol_len-1,
                                   ASN_NULL, NULL,  0);
        i = strlen(intrface);
        netsnmp_query_walk( var, ss );
        for (vp=var; vp; vp=vp->next_variable) {
            if (strncmp(intrface, (char *)vp->val.string, i) == 0 &&
                i == vp->val_len)
                break;  /* found requested interface */
        }
        /*
         * XXX - Might be worth searching ifName/ifAlias as well
         */
        if (!vp) {
            fprintf(stderr, "%s: unknown interface\n", intrface );
            exit(1);
        }

        /*
         *  Prepare the current and previous 'iftot' structures,
         *    and set the ifIndex value in the OID buffer.
         */
        ip     = SNMP_MALLOC_TYPEDEF( struct iftot );
        cur_if = SNMP_MALLOC_TYPEDEF( struct iftot );
        if (!ip || !cur_if) {
            fprintf(stderr, "internal error\n");
            exit(1);
        }
        ifcol_oid[ ifcol_len-1 ] = vp->name[ ifcol_len-1 ];
        snmp_free_var( var );
        var = NULL;
    } else {
        /*
         *  Prepare the current and previous 'iftot' structures.
         *    (using different pointers, for consistency with *BSD code)
         */
        sum   = SNMP_MALLOC_TYPEDEF( struct iftot );
        total = SNMP_MALLOC_TYPEDEF( struct iftot );
        if (!sum || !total) {
            fprintf(stderr, "internal error\n");
            exit(1);
        }
    }

    timerSet( interval );
    first = 1;
banner:
    printf( "%17s %14s %16s", "input",
        intrface ? intrface : "(Total)", "output");
    putchar('\n');
    printf( "%10s %5s %10s %10s %5s %10s %5s",
        "packets", "errs", "bytes", "packets", "errs", "bytes", "colls");
    if (dflag)
	printf(" %5.5s", "drops");
    putchar('\n');
    fflush(stdout);
    line = 0;
loop:
    if ( intrface ) {
#define ADD_IFVAR( x ) ifcol_oid[ ifcol_len-2 ] = x; \
    snmp_varlist_add_variable( &var, ifcol_oid, ifcol_len, ASN_NULL, NULL,  0)
    /*  if (bflag) { */
            ADD_IFVAR( 10 );        /* ifInOctets     */
            ADD_IFVAR( 16 );        /* ifOutOctets    */
    /*  } */
        ADD_IFVAR( 11 );            /* ifInUcastPkts  */
        ADD_IFVAR( 12 );            /* ifInNUcastPkts */
        ADD_IFVAR( 14 );            /* ifInErrors     */
        ADD_IFVAR( 17 );            /* ifOutUcastPkts */
        ADD_IFVAR( 18 );            /* ifOutNUcastPkts */
        ADD_IFVAR( 20 );            /* ifOutErrors    */
        ADD_IFVAR( 21 );            /* ifOutQLen      */
        if (dflag) {
            ADD_IFVAR( 19 );        /* ifOutDiscards  */
        }
#undef ADD_IFVAR

        netsnmp_query_get( var, ss );   /* Or parallel walk ?? */
        cur_if->ift_ip = 0;
        cur_if->ift_ib = 0;
        cur_if->ift_ie = 0;
        cur_if->ift_op = 0;
        cur_if->ift_ob = 0;
        cur_if->ift_oe = 0;
        cur_if->ift_co = 0;
        cur_if->ift_dr = 0;
        cur_if->ifIndex = var->name[ ifcol_len-1 ];
        for (vp=var; vp; vp=vp->next_variable) {
            if ( ! vp->val.integer )
                continue;
            switch (vp->name[ifcol_len-2]) {
            case 10:    /* ifInOctets */
                cur_if->ift_ib = *vp->val.integer;
                break;
            case 11:    /* ifInUcastPkts */
                cur_if->ift_ip += *vp->val.integer;
                break;
            case 12:    /* ifInNUcastPkts */
                cur_if->ift_ip += *vp->val.integer;
                break;
            case 14:    /* ifInErrors */
                cur_if->ift_ie = *vp->val.integer;
                break;
            case 16:    /* ifOutOctets */
                cur_if->ift_ob = *vp->val.integer;
                break;
            case 17:    /* ifOutUcastPkts */
                cur_if->ift_op += *vp->val.integer;
                break;
            case 18:    /* ifOutNUcastPkts */
                cur_if->ift_op += *vp->val.integer;
                break;
            case 19:    /* ifOutDiscards */
                cur_if->ift_dr = *vp->val.integer;
                break;
            case 20:    /* ifOutErrors */
                cur_if->ift_oe = *vp->val.integer;
                break;
            case 21:    /* ifOutQLen */
                cur_if->ift_co = *vp->val.integer;
                break;
            }
        }
        snmp_free_varbind( var );
        var = NULL;
 
        if (!first) {
 	    printf("%10lu %5lu %10lu %10lu %5lu %10lu %5lu",
 		cur_if->ift_ip - ip->ift_ip,
 		cur_if->ift_ie - ip->ift_ie,
		cur_if->ift_ib - ip->ift_ib,
		cur_if->ift_op - ip->ift_op,
		cur_if->ift_oe - ip->ift_oe,
		cur_if->ift_ob - ip->ift_ob,
		cur_if->ift_co - ip->ift_co);
	    if (dflag)
		printf(" %5lu", cur_if->ift_dr - ip->ift_dr);
            putchar('\n');
            fflush(stdout);
	}
	ip->ift_ip = cur_if->ift_ip;
	ip->ift_ie = cur_if->ift_ie;
	ip->ift_ib = cur_if->ift_ib;
	ip->ift_op = cur_if->ift_op;
	ip->ift_oe = cur_if->ift_oe;
	ip->ift_ob = cur_if->ift_ob;
	ip->ift_co = cur_if->ift_co;
	ip->ift_dr = cur_if->ift_dr;
    }  /* (single) interface */
    else {
	sum->ift_ip = 0;
	sum->ift_ib = 0;
	sum->ift_ie = 0;
	sum->ift_op = 0;
	sum->ift_ob = 0;
	sum->ift_oe = 0;
	sum->ift_co = 0;
	sum->ift_dr = 0;
#define ADD_IFVAR( x ) ifcol_oid[ ifcol_len-2 ] = x; \
    snmp_varlist_add_variable( &var, ifcol_oid, ifcol_len-1, ASN_NULL, NULL,  0)
        ADD_IFVAR( 11 );            /* ifInUcastPkts  */
        ADD_IFVAR( 12 );            /* ifInNUcastPkts */
        ADD_IFVAR( 14 );            /* ifInErrors     */
        ADD_IFVAR( 17 );            /* ifOutUcastPkts */
        ADD_IFVAR( 18 );            /* ifOutNUcastPkts */
        ADD_IFVAR( 20 );            /* ifOutErrors    */
        ADD_IFVAR( 21 );            /* ifOutQLen      */
    /*  if (bflag) { */
            ADD_IFVAR( 10 );        /* ifInOctets     */
            ADD_IFVAR( 16 );        /* ifOutOctets    */
    /*  } */
        if (dflag) {
            ADD_IFVAR( 19 );        /* ifOutDiscards  */
        }
#undef ADD_IFVAR

        ifcol_oid[ ifcol_len-2 ] = 11;       /* ifInUcastPkts */
        while ( 1 ) {
            if (netsnmp_query_getnext( var, ss ) != SNMP_ERR_NOERROR)
                break;
            if ( snmp_oid_compare( ifcol_oid, ifcol_len-2,
                                   var->name, ifcol_len-2) != 0 )
                break;    /* End of Table */
            
            for ( vp=var; vp; vp=vp->next_variable ) {
                if ( ! vp->val.integer )
                    continue;
                switch ( vp->name[ ifcol_len-2 ] ) {
                case 10:    /* ifInOctets */
                    sum->ift_ib += *vp->val.integer;
                    break;
                case 11:    /* ifInUcastPkts */
                    sum->ift_ip += *vp->val.integer;
                    break;
                case 12:    /* ifInNUcastPkts */
                    sum->ift_ip += *vp->val.integer;
                    break;
                case 14:    /* ifInErrors */
                    sum->ift_ie += *vp->val.integer;
                    break;
                case 16:    /* ifOutOctets */
                    sum->ift_ob += *vp->val.integer;
                    break;
                case 17:    /* ifOutUcastPkts */
                    sum->ift_op += *vp->val.integer;
                    break;
                case 18:    /* ifOutNUcastPkts */
                    sum->ift_op += *vp->val.integer;
                    break;
                case 19:    /* ifOutDiscards */
                    sum->ift_dr += *vp->val.integer;
                    break;
                case 20:    /* ifOutErrors */
                    sum->ift_oe += *vp->val.integer;
                    break;
                case 21:    /* ifOutQLen */
                    sum->ift_co += *vp->val.integer;
                    break;
                }
            }
            /*
             * Now loop to retrieve the next entry from the table.
             */
        }   /* while (1) */

        snmp_free_varbind( var );
        var = NULL;
 
        if (!first) {
 	    printf("%10lu %5lu %10lu %10lu %5lu %10lu %5lu",
 		sum->ift_ip - total->ift_ip,
 		sum->ift_ie - total->ift_ie,
		sum->ift_ib - total->ift_ib,
		sum->ift_op - total->ift_op,
		sum->ift_oe - total->ift_oe,
		sum->ift_ob - total->ift_ob,
		sum->ift_co - total->ift_co);
	    if (dflag)
		printf(" %5lu", sum->ift_dr - total->ift_dr);
            putchar('\n');
            fflush(stdout);
	}
	total->ift_ip = sum->ift_ip;
	total->ift_ie = sum->ift_ie;
	total->ift_ib = sum->ift_ib;
	total->ift_op = sum->ift_op;
	total->ift_oe = sum->ift_oe;
	total->ift_ob = sum->ift_ob;
	total->ift_co = sum->ift_co;
	total->ift_dr = sum->ift_dr;
    }  /* overall summary */

    timerPause();
    timerSet(interval);
    line++;
    first = 0;
    if (line == 21)
    	goto banner;
    else
    	goto loop;
    /*NOTREACHED*/
}


/*
 * timerSet sets or resets the timer to fire in "interval" seconds.
 * timerPause waits only if the timer has not fired.
 * timing precision is not considered important.
 */

#if (defined(WIN32) || defined(cygwin))
static int      sav_int;
static time_t   timezup;
static void
timerSet(int interval_seconds)
{
    sav_int = interval_seconds;
    timezup = time(0) + interval_seconds;
}

/*
 * you can do better than this ! 
 */
static void
timerPause(void)
{
    time_t          now;
    while (time(&now) < timezup)
#ifdef WIN32
        Sleep(400);
#else
    {
        struct timeval  tx;
        tx.tv_sec = 0;
        tx.tv_usec = 400 * 1000;        /* 400 milliseconds */
        select(0, 0, 0, 0, &tx);
    }
#endif
}

#else

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
RETSIGTYPE
catchalarm(int sig)
{
    signalled = YES;
}

static void
timerSet(int interval_seconds)
{
#ifdef HAVE_SIGSET
    (void) sigset(SIGALRM, catchalarm);
#else
    (void) signal(SIGALRM, catchalarm);
#endif
    signalled = NO;
    (void) alarm(interval_seconds);
}

static void
timerPause(void)
{
#ifdef HAVE_SIGHOLD
    sighold(SIGALRM);
    if (!signalled) {
        sigpause(SIGALRM);
    }
#else
    int             oldmask;
    oldmask = sigblock(sigmask(SIGALRM));
    if (!signalled) {
        sigpause(0);
    }
    sigsetmask(oldmask);
#endif
}

#endif                          /* !WIN32 && !cygwin */
