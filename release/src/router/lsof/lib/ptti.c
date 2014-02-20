/*
 * ptti.c -- BSD style print_tcptpi() function for lsof library
 */


/*
 * Copyright 1997 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */


#include "../machine.h"

#if	defined(USE_LIB_PRINT_TCPTPI)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: ptti.c,v 1.6 2008/10/21 16:13:23 abe Exp $";
# endif	/* !defined(lint) */

#define	TCPSTATES			/* activate tcpstates[] */
#include "../lsof.h"


/*
 * build_IPstates() -- build the TCP and UDP state tables
 *
 * Note: this module does not support a UDP state table.
 */

void
build_IPstates()
{

/*
 * Set the TcpNstates global variable.
 */
	TcpNstates = TCP_NSTATES;
	TcpSt = (char **)&tcpstates;
}


/*
 * print_tcptpi() - print TCP/TPI info
 */

void
print_tcptpi(nl)
	int nl;				/* 1 == '\n' required */
{
	int ps = 0;
	int s;

	if ((Ftcptpi & TCPTPI_STATE) && Lf->lts.type == 0) {
	    if (Ffield)
		(void) printf("%cST=", LSOF_FID_TCPTPI);
	    else
		putchar('(');
	    if (!TcpNstates)
		(void) build_IPstates();
	    if ((s = Lf->lts.state.i) < 0 || s >= TcpNstates)
		(void) printf("UNKNOWN_TCP_STATE_%d", s);
	    else
		(void) fputs(TcpSt[s], stdout);
	    ps++;
	    if (Ffield)
		putchar(Terminator);
	}

#if	defined(HASTCPTPIQ)
	if (Ftcptpi & TCPTPI_QUEUES) {
	    if (Lf->lts.rqs) {
		if (Ffield)
		    putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("QR=%lu", Lf->lts.rq);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	    if (Lf->lts.sqs) {
		if (Ffield)
		    putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("QS=%lu", Lf->lts.sq);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	}
#endif	/* defined(HASTCPTPIQ) */

#if	defined(HASSOOPT)
	if (Ftcptpi & TCPTPI_FLAGS) {
	    int opt;

	    if ((opt = Lf->lts.opt)
	    ||  Lf->lts.pqlens || Lf->lts.qlens || Lf->lts.qlims
	    ||  Lf->lts.rbszs  || Lf->lts.sbsz
	    ) {
		char sep = ' ';

		if (Ffield)
		    sep = LSOF_FID_TCPTPI;
		else if (!ps)
		    sep = '(';
		(void) printf("%cSO", sep);
		ps++;
		sep = '=';

# if	defined(SO_ACCEPTCONN)
		if (opt & SO_ACCEPTCONN) {
		    (void) printf("%cACCEPTCONN", sep);
		    opt &= ~SO_ACCEPTCONN;
		    sep = ',';
		}
# endif	/* defined(SO_ACCEPTCONN) */

# if	defined(SO_ACCEPTFILTER)
		if (opt & SO_ACCEPTFILTER) {
		    (void) printf("%cACCEPTFILTER", sep);
		    opt &= ~SO_ACCEPTFILTER;
		    sep = ',';
		}
# endif	/* defined(SO_ACCEPTFILTER) */

# if	defined(SO_AUDIT)
		if (opt & SO_AUDIT) {
		    (void) printf("%cAUDIT", sep);
		    opt &= ~SO_AUDIT;
		    sep = ',';
		}
# endif	/* defined(SO_AUDIT) */

# if	defined(SO_BINDANY)
		if (opt & SO_BINDANY) {
		    (void) printf("%cBINDANY", sep);
		    opt &= ~SO_BINDANY;
		    sep = ',';
		}
# endif	/* defined(SO_BINDANY) */

# if	defined(SO_BINTIME)
		if (opt & SO_BINTIME) {
		    (void) printf("%cBINTIME", sep);
		    opt &= ~SO_BINTIME;
		    sep = ',';
		}
# endif	/* defined(SO_BINTIME) */

# if	defined(SO_BROADCAST)
		if (opt & SO_BROADCAST) {
		    (void) printf("%cBROADCAST", sep);
		    opt &= ~SO_BROADCAST;
		    sep = ',';
		}
# endif	/* defined(SO_BROADCAST) */

# if	defined(SO_CKSUMRECV)
		if (opt & SO_CKSUMRECV) {
		    (void) printf("%cCKSUMRECV", sep);
		    opt &= ~SO_CKSUMRECV;
		    sep = ',';
		}
# endif	/* defined(SO_CKSUMRECV) */

# if	defined(SO_CLUA_IN_NOALIAS)
		if (opt & SO_CLUA_IN_NOALIAS) {
		    (void) printf("%cCLUA_IN_NOALIAS", sep);
		    opt &= ~SO_CLUA_IN_NOALIAS;
		    sep = ',';
		}
# endif	/* defined(SO_CLUA_IN_NOALIAS) */

# if	defined(SO_CLUA_IN_NOLOCAL)
		if (opt & SO_CLUA_IN_NOLOCAL) {
		    (void) printf("%cCLUA_IN_NOLOCAL", sep);
		    opt &= ~SO_CLUA_IN_NOLOCAL;
		    sep = ',';
		}
# endif	/* defined(SO_CLUA_IN_NOLOCAL) */

# if	defined(SO_DEBUG)
		if (opt & SO_DEBUG) {
		    (void) printf("%cDEBUG", sep);
		    opt &= ~ SO_DEBUG;
		    sep = ',';
		}
# endif	/* defined(SO_DEBUG) */

# if	defined(SO_DGRAM_ERRIND)
		if (opt & SO_DGRAM_ERRIND) {
		    (void) printf("%cDGRAM_ERRIND", sep);
		    opt &= ~SO_DGRAM_ERRIND;
		    sep = ',';
		}
# endif	/* defined(SO_DGRAM_ERRIND) */

# if	defined(SO_DONTROUTE)
		if (opt & SO_DONTROUTE) {
		    (void) printf("%cDONTROUTE", sep);
		    opt &= ~SO_DONTROUTE;
		    sep = ',';
		}
# endif	/* defined(SO_DONTROUTE) */

# if	defined(SO_DONTTRUNC)
		if (opt & SO_DONTTRUNC) {
		    (void) printf("%cDONTTRUNC", sep);
		    opt &= ~SO_DONTTRUNC;
		    sep = ',';
		}
# endif	/* defined(SO_DONTTRUNC) */

# if	defined(SO_EXPANDED_RIGHTS)
		if (opt & SO_EXPANDED_RIGHTS) {
		    (void) printf("%cEXPANDED_RIGHTS", sep);
		    opt &= ~SO_EXPANDED_RIGHTS;
		    sep = ',';
		}
# endif	/* defined(SO_EXPANDED_RIGHTS) */

# if	defined(SO_KEEPALIVE)
		if (opt & SO_KEEPALIVE) {
		    (void) printf("%cKEEPALIVE", sep);
		    if (Lf->lts.kai)
			(void) printf("=%d", Lf->lts.kai);
		    opt &= ~SO_KEEPALIVE;
		    sep = ',';
		}
# endif	/* defined(SO_KEEPALIVE) */

# if	defined(SO_KERNACCEPT)
		if (opt & SO_KERNACCEPT) {
		    (void) printf("%cKERNACCEPT", sep);
		    opt &= ~SO_KERNACCEPT;
		    sep = ',';
		}
# endif	/* defined(SO_KERNACCEPT) */

# if	defined(SO_IMASOCKET)
		if (opt & SO_IMASOCKET) {
		    (void) printf("%cIMASOCKET", sep);
		    opt &= ~SO_IMASOCKET;
		    sep = ',';
		}
# endif	/* defined(SO_IMASOCKET) */

# if	defined(SO_LINGER)
		if (opt & SO_LINGER) {
		    (void) printf("%cLINGER", sep);
		    if (Lf->lts.ltm)
			(void) printf("=%d", Lf->lts.ltm);
		    opt &= ~SO_LINGER;
		    sep = ',';
		}
# endif	/* defined(SO_LINGER) */

# if	defined(SO_LISTENING)
		if (opt & SO_LISTENING) {
		    (void) printf("%cLISTENING", sep);
		    opt &= ~SO_LISTENING;
		    sep = ',';
		}
# endif	/* defined(SO_LISTENING) */

# if	defined(SO_MGMT)
		if (opt & SO_MGMT) {
		    (void) printf("%cMGMT", sep);
		    opt &= ~SO_MGMT;
		    sep = ',';
		}
# endif	/* defined(SO_MGMT) */

# if	defined(SO_PAIRABLE)
		if (opt & SO_PAIRABLE) {
		    (void) printf("%cPAIRABLE", sep);
		    opt &= ~SO_PAIRABLE;
		    sep = ',';
		}
# endif	/* defined(SO_PAIRABLE) */

# if	defined(SO_RESVPORT)
		if (opt & SO_RESVPORT) {
		    (void) printf("%cRESVPORT", sep);
		    opt &= ~SO_RESVPORT;
		    sep = ',';
		}
# endif	/* defined(SO_RESVPORT) */

# if	defined(SO_NOREUSEADDR)
		if (opt & SO_NOREUSEADDR) {
		    (void) printf("%cNOREUSEADDR", sep);
		    opt &= ~SO_NOREUSEADDR;
		    sep = ',';
		}
# endif	/* defined(SO_NOREUSEADDR) */

# if	defined(SO_NOSIGPIPE)
		if (opt & SO_NOSIGPIPE) {
		    (void) printf("%cNOSIGPIPE", sep);
		    opt &= ~SO_NOSIGPIPE;
		    sep = ',';
		}
# endif	/* defined(SO_NOSIGPIPE) */

# if	defined(SO_OOBINLINE)
		if (opt & SO_OOBINLINE) {
		    (void) printf("%cOOBINLINE", sep);
		    opt &= ~SO_OOBINLINE;
		    sep = ',';
		}
# endif	/* defined(SO_OOBINLINE) */

# if	defined(SO_ORDREL)
		if (opt & SO_ORDREL) {
		    (void) printf("%cORDREL", sep);
		    opt &= ~SO_ORDREL;
		    sep = ',';
		}
# endif	/* defined(SO_ORDREL) */

		if (Lf->lts.pqlens) {
		    (void) printf("%cPQLEN=%u", sep, Lf->lts.pqlen);
		    sep = ',';
		}
		if (Lf->lts.qlens) {
		    (void) printf("%cQLEN=%u", sep, Lf->lts.qlen);
		    sep = ',';
		}
		if (Lf->lts.qlims) {
		    (void) printf("%cQLIM=%u", sep, Lf->lts.qlim);
		    sep = ',';
		}
		if (Lf->lts.rbszs) {
		    (void) printf("%cRCVBUF=%lu", sep, Lf->lts.rbsz);
		    sep = ',';
		}

# if	defined(SO_REUSEADDR)
		if (opt & SO_REUSEADDR) {
		    (void) printf("%cREUSEADDR", sep);
		    opt &= ~SO_REUSEADDR;
		    sep = ',';
		}
# endif	/* defined(SO_REUSEADDR) */

# if	defined(SO_REUSEALIASPORT)
		if (opt & SO_REUSEALIASPORT) {
		    (void) printf("%cREUSEALIASPORT", sep);
		    opt &= ~SO_REUSEALIASPORT;
		    sep = ',';
		}
# endif	/* defined(SO_REUSEALIASPORT) */

# if	defined(SO_REUSEPORT)
		if (opt & SO_REUSEPORT) {
		    (void) printf("%cREUSEPORT", sep);
		    opt &= ~SO_REUSEPORT;
		    sep = ',';
		}
# endif	/* defined(SO_REUSEPORT) */

# if	defined(SO_REUSERAD)
		if (opt & SO_REUSERAD) {
		    (void) printf("%cREUSERAD", sep);
		    opt &= ~SO_REUSERAD;
		    sep = ',';
		}
# endif	/* defined(SO_REUSERAD) */

# if	defined(SO_SECURITY_REQUEST)
		if (opt & SO_SECURITY_REQUEST) {
		    (void) printf("%cSECURITY_REQUEST", sep);
		    opt &= ~SO_SECURITY_REQUEST;
		    sep = ',';
		}
# endif	/* defined(SO_SECURITY_REQUEST) */

		if (Lf->lts.sbszs) {
		    (void) printf("%cSNDBUF=%lu", sep, Lf->lts.sbsz);
		    sep = ',';
		}

# if	defined(SO_TIMESTAMP)
		if (opt & SO_TIMESTAMP) {
		    (void) printf("%cTIMESTAMP", sep);
		    opt &= ~SO_TIMESTAMP;
		    sep = ',';
		}
# endif	/* defined(SO_TIMESTAMP) */

# if	defined(SO_UMC)
		if (opt & SO_UMC) {
		    (void) printf("%cUMC", sep);
		    opt &= ~SO_UMC;
		    sep = ',';
		}
# endif	/* defined(SO_UMC) */

# if	defined(SO_USE_IFBUFS)
		if (opt & SO_USE_IFBUFS) {
		    (void) printf("%cUSE_IFBUFS", sep);
		    opt &= ~SO_USE_IFBUFS;
		    sep = ',';
		}
# endif	/* defined(SO_USE_IFBUFS) */

# if	defined(SO_USELOOPBACK)
		if (opt & SO_USELOOPBACK) {
		    (void) printf("%cUSELOOPBACK", sep);
		    opt &= ~SO_USELOOPBACK;
		    sep = ',';
		}
# endif	/* defined(SO_USELOOPBACK) */

# if	defined(SO_WANTMORE)
		if (opt & SO_WANTMORE) {
		    (void) printf("%cWANTMORE", sep);
		    opt &= ~SO_WANTMORE;
		    sep = ',';
		}
# endif	/* defined(SO_WANTMORE) */

# if	defined(SO_WANTOOBFLAG)
		if (opt & SO_WANTOOBFLAG) {
		    (void) printf("%cWANTOOBFLAG", sep);
		    opt &= ~SO_WANTOOBFLAG;
		    sep = ',';
		}
# endif	/* defined(SO_WANTOOBFLAG) */

		if (opt)
		    (void) printf("%cUNKNOWN=%#x", sep, opt);
		if (Ffield)
		    putchar(Terminator);
	    }
	}
#endif	/* defined(HASSOOPT) */

#if	defined(HASSOSTATE)
	if (Ftcptpi & TCPTPI_FLAGS) {
	    unsigned int ss;

	    if ((ss = Lf->lts.ss)) {
		char sep = ' ';

		if (Ffield)
		    sep = LSOF_FID_TCPTPI;
		else if (!ps)
		    sep = '(';
		(void) printf("%cSS", sep);
		ps++;
		sep = '=';

# if	defined(SS_ASYNC)
		if (ss & SS_ASYNC) {
		    (void) printf("%cASYNC", sep);
		    ss &= ~SS_ASYNC;
		    sep = ',';
		}
# endif	/* defined(SS_ASYNC) */

# if	defined(SS_BOUND)
		if (ss & SS_BOUND) {
		    (void) printf("%cBOUND", sep);
		    ss &= ~SS_BOUND;
		    sep = ',';
		}
# endif	/* defined(SS_BOUND) */

# if	defined(HASSBSTATE)
#  if	defined(SBS_CANTRCVMORE)
		if (Lf->lts.sbs_rcv & SBS_CANTRCVMORE) {
		    (void) printf("%cCANTRCVMORE", sep);
		    Lf->lts.sbs_rcv &= ~SBS_CANTRCVMORE;
		    sep = ',';
		}
#  endif	/* defined(SBS_CANTRCVMORE) */

#  if	defined(SBS_CANTSENDMORE)
		if (Lf->lts.sbs_snd & SBS_CANTSENDMORE) {
		    (void) printf("%cCANTSENDMORE", sep);
		    Lf->lts.sbs_snd &= ~SBS_CANTSENDMORE;
		    sep = ',';
		}
#  endif	/* defined(SS_CANTSENDMORE) */
# else	/* !defined(HASSBSTATE) */

#  if	defined(SS_CANTRCVMORE)
		if (ss & SS_CANTRCVMORE) {
		    (void) printf("%cCANTRCVMORE", sep);
		    ss &= ~SS_CANTRCVMORE;
		    sep = ',';
		}
#  endif	/* defined(SS_CANTRCVMORE) */

#  if	defined(SS_CANTSENDMORE)
		if (ss & SS_CANTSENDMORE) {
		    (void) printf("%cCANTSENDMORE", sep);
		    ss &= ~SS_CANTSENDMORE;
		    sep = ',';
		}
#  endif	/* defined(SS_CANTSENDMORE) */
# endif	/* defined(HASSBSTATE) */

# if	defined(SS_COMP)
		if (ss & SS_COMP) {
		    (void) printf("%cCOMP", sep);
		    ss &= ~SS_COMP;
		    sep = ',';
		}
# endif	/* defined(SS_COMP) */

# if	defined(SS_CONNECTOUT)
		if (ss & SS_CONNECTOUT) {
		    (void) printf("%cCONNECTOUT", sep);
		    ss &= ~SS_CONNECTOUT;
		    sep = ',';
		}
# endif	/* defined(SS_CONNECTOUT) */

# if	defined(SS_HIPRI)
		if (ss & SS_HIPRI) {
		    (void) printf("%cHIPRI", sep);
		    ss &= ~SS_HIPRI;
		    sep = ',';
		}
# endif	/* defined(SS_HIPRI) */

# if	defined(SS_IGNERR)
		if (ss & SS_IGNERR) {
		    (void) printf("%cIGNERR", sep);
		    ss &= ~SS_IGNERR;
		    sep = ',';
		}
# endif	/* defined(SS_IGNERR) */

# if	defined(SS_INCOMP)
		if (ss & SS_INCOMP) {
		    (void) printf("%cINCOMP", sep);
		    ss &= ~SS_INCOMP;
		    sep = ',';
		}
# endif	/* defined(SS_INCOMP) */

# if	defined(SS_IOCWAIT)
		if (ss & SS_IOCWAIT) {
		    (void) printf("%cIOCWAIT", sep);
		    ss &= ~SS_IOCWAIT;
		    sep = ',';
		}
# endif	/* defined(SS_IOCWAIT) */

# if	defined(SS_ISCONFIRMING)
		if (ss & SS_ISCONFIRMING) {
		    (void) printf("%cISCONFIRMING", sep);
		    ss &= ~SS_ISCONFIRMING;
		    sep = ',';
		}
# endif	/* defined(SS_ISCONFIRMING) */

# if	defined(SS_ISCONNECTED)
		if (ss & SS_ISCONNECTED) {
		    (void) printf("%cISCONNECTED", sep);
		    ss &= ~SS_ISCONNECTED;
		    sep = ',';
		}
# endif	/* defined(SS_ISCONNECTED) */

# if	defined(SS_ISCONNECTING)
		if (ss & SS_ISCONNECTING) {
		    (void) printf("%cISCONNECTING", sep);
		    ss &= ~SS_ISCONNECTING;
		    sep = ',';
		}
# endif	/* defined(SS_ISCONNECTING) */

# if	defined(SS_ISDISCONNECTING)
		if (ss & SS_ISDISCONNECTING) {
		    (void) printf("%cISDISCONNECTING", sep);
		    ss &= ~SS_ISDISCONNECTING;
		    sep = ',';
		}
# endif	/* defined(SS_ISDISCONNECTING) */

# if	defined(SS_MORETOSEND)
		if (ss & SS_MORETOSEND) {
		    (void) printf("%cMORETOSEND", sep);
		    ss &= ~SS_MORETOSEND;
		    sep = ',';
		}
# endif	/* defined(SS_MORETOSEND) */

# if	defined(SS_NBIO)
		if (ss & SS_NBIO) {
		    (void) printf("%cNBIO", sep);
		    ss &= ~SS_NBIO;
		    sep = ',';
		}
# endif	/* defined(SS_NBIO) */

# if	defined(SS_NOCONN)
		if (ss & SS_NOCONN) {
		    (void) printf("%cNOCONN", sep);
		    ss &= ~SS_NOCONN;
		    sep = ',';
		}
# endif	/* defined(SS_NOCONN) */

# if	defined(SS_NODELETE)
		if (ss & SS_NODELETE) {
		    (void) printf("%cNODELETE", sep);
		    ss &= ~SS_NODELETE;
		    sep = ',';
		}
# endif	/* defined(SS_NODELETE) */

# if	defined(SS_NOFDREF)
		if (ss & SS_NOFDREF) {
		    (void) printf("%cNOFDREF", sep);
		    ss &= ~SS_NOFDREF;
		    sep = ',';
		}
# endif	/* defined(SS_NOFDREF) */

# if	defined(SS_NOGHOST)
		if (ss & SS_NOGHOST) {
		    (void) printf("%cNOGHOST", sep);
		    ss &= ~SS_NOGHOST;
		    sep = ',';
		}
# endif	/* defined(SS_NOGHOST) */

# if	defined(SS_NOINPUT)
		if (ss & SS_NOINPUT) {
		    (void) printf("%cNOINPUT", sep);
		    ss &= ~SS_NOINPUT;
		    sep = ',';
		}
# endif	/* defined(SS_NOINPUT) */

# if	defined(SS_PRIV)
		if (ss & SS_PRIV) {
		    (void) printf("%cPRIV", sep);
		    ss &= ~SS_PRIV;
		    sep = ',';
		}
# endif	/* defined(SS_PRIV) */

# if	defined(SS_QUEUE)
		if (ss & SS_QUEUE) {
		    (void) printf("%cQUEUE", sep);
		    ss &= ~SS_QUEUE;
		    sep = ',';
		}
# endif	/* defined(SS_QUEUE) */

# if	defined(HASSBSTATE)
#  if	defined(SBS_RCVATMARK)
		if (Lf->lts.sbs_rcv & SBS_RCVATMARK) {
		    (void) printf("%cRCVATMARK", sep);
		    Lf->lts.sbs_rcv &= ~SBS_RCVATMARK;
		    sep = ',';
		}
#  endif	/* defined(SBS_RCVATMARK) */

# else	/* !defined(HASSBSTATE) */
#  if	defined(SS_RCVATMARK)
		if (ss & SS_RCVATMARK) {
		    (void) printf("%cRCVATMARK", sep);
		    ss &= ~SS_RCVATMARK;
		    sep = ',';
		}
#  endif	/* defined(SS_RCVATMARK) */
# endif	/* defined(HASSBSTATE) */

# if	defined(SS_READWAIT)
		if (ss & SS_READWAIT) {
		    (void) printf("%cREADWAIT", sep);
		    ss &= ~SS_READWAIT;
		    sep = ',';
		}
# endif	/* defined(SS_READWAIT) */

# if	defined(SS_SETRCV)
		if (ss & SS_SETRCV) {
		    (void) printf("%cSETRCV", sep);
		    ss &= ~SS_SETRCV;
		    sep = ',';
		}
# endif	/* defined(SS_SETRCV) */

# if	defined(SS_SETSND)
		if (ss & SS_SETSND) {
		    (void) printf("%cSETSND", sep);
		    ss &= ~SS_SETSND;
		    sep = ',';
		}
# endif	/* defined(SS_SETSND) */

# if	defined(SS_SIGREAD)
		if (ss & SS_SIGREAD) {
		    (void) printf("%cSIGREAD", sep);
		    ss &= ~SS_SIGREAD;
		    sep = ',';
		}
# endif	/* defined(SS_SIGREAD) */

# if	defined(SS_SIGWRITE)
		if (ss & SS_SIGWRITE) {
		    (void) printf("%cSIGWRITE", sep);
		    ss &= ~SS_SIGWRITE;
		    sep = ',';
		}
# endif	/* defined(SS_SIGWRITE) */

# if	defined(SS_SPLICED)
		if (ss & SS_SPLICED) {
		    (void) printf("%cSPLICED", sep);
		    ss &= ~SS_SPLICED;
		    sep = ',';
		}
# endif	/* defined(SS_SPLICED) */

# if	defined(SS_WRITEWAIT)
		if (ss & SS_WRITEWAIT) {
		    (void) printf("%cWRITEWAIT", sep);
		    ss &= ~SS_WRITEWAIT;
		    sep = ',';
		}
# endif	/* defined(SS_WRITEWAIT) */

# if	defined(SS_ZOMBIE)
		if (ss & SS_ZOMBIE) {
		    (void) printf("%cZOMBIE", sep);
		    ss &= ~SS_ZOMBIE;
		    sep = ',';
		}
# endif	/* defined(SS_ZOMBIE) */

		if (ss)
		    (void) printf("%cUNKNOWN=%#x", sep, ss);
		if (Ffield)
		    putchar(Terminator);
	    }
	}
#endif	/* defined(HASSOSTATE) */

#if	defined(HASTCPOPT)
	if (Ftcptpi & TCPTPI_FLAGS) {
	    int topt;

	    if ((topt = Lf->lts.topt) || Lf->lts.msss) {
		char sep = ' ';

		if (Ffield)
		    sep = LSOF_FID_TCPTPI;
		else if (!ps)
		    sep = '(';
		(void) printf("%cTF", sep);
		ps++;
		sep = '=';

# if	defined(TF_ACKNOW)
		if (topt & TF_ACKNOW) {
		    (void) printf("%cACKNOW", sep);
		    topt &= ~TF_ACKNOW;
		    sep = ',';
		}
# endif	/* defined(TF_ACKNOW) */

# if	defined(TF_CANT_TXSACK)
		if (topt & TF_CANT_TXSACK) {
		    (void) printf("%cCANT_TXSACK", sep);
		    topt &= ~TF_CANT_TXSACK;
		    sep = ',';
		}
# endif	/* defined(TF_CANT_TXSACK) */

# if	defined(TF_DEAD)
		if (topt & TF_DEAD) {
		    (void) printf("%cDEAD", sep);
		    topt &= ~TF_DEAD;
		    sep = ',';
		}
# endif	/* defined(TF_DEAD) */

# if	defined(TF_DELACK)
		if (topt & TF_DELACK) {
		    (void) printf("%cDELACK", sep);
		    topt &= ~TF_DELACK;
		    sep = ',';
		}
# endif	/* defined(TF_DELACK) */

# if	defined(TF_DELAY_ACK)
		if (topt & TF_DELAY_ACK) {
		    (void) printf("%cDELAY_ACK", sep);
		    topt &= ~TF_DELAY_ACK;
		    sep = ',';
		}
# endif	/* defined(TF_DELAY_ACK) */

# if	defined(TF_DISABLE_ECN)
		if (topt & TF_DISABLE_ECN) {
		    (void) printf("%cDISABLE_ECN", sep);
		    topt &= ~TF_DISABLE_ECN;
		    sep = ',';
		}
# endif	/* defined(TF_DISABLE_ECN) */

# if	defined(TF_ECN)
		if (topt & TF_ECN) {
		    (void) printf("%cECN", sep);
		    topt &= ~TF_ECN;
		    sep = ',';
		}
# endif	/* defined(TF_ECN) */

# if	defined(TF_ECN_PERMIT)
		if (topt & TF_ECN_PERMIT) {
		    (void) printf("%cECN_PERMIT", sep);
		    topt &= ~TF_ECN_PERMIT;
		    sep = ',';
		}
# endif	/* defined(TF_ECN_PERMIT) */

# if	defined(TF_FASTRECOVERY)
		if (topt & TF_FASTRECOVERY) {
		    (void) printf("%cFASTRECOVERY", sep);
		    topt &= ~TF_FASTRECOVERY;
		    sep = ',';
		}
# endif	/* defined(TF_FASTRECOVERY) */

# if	defined(TF_FASTRXMT_PHASE)
		if (topt & TF_FASTRXMT_PHASE) {
		    (void) printf("%cFASTRXMT_PHASE", sep);
		    topt &= ~TF_FASTRXMT_PHASE;
		    sep = ',';
		}
# endif	/* defined(TF_FASTRXMT_PHASE) */

# if	defined(TF_HAVEACKED)
		if (topt & TF_HAVEACKED) {
		    (void) printf("%cHAVEACKED", sep);
		    topt &= ~TF_HAVEACKED;
		    sep = ',';
		}
# endif	/* defined(TF_HAVEACKED) */

# if	defined(TF_HAVECLOSED)
		if (topt & TF_HAVECLOSED) {
		    (void) printf("%cHAVECLOSED", sep);
		    topt &= ~TF_HAVECLOSED;
		    sep = ',';
		}
# endif	/* defined(TF_HAVECLOSED) */

# if	defined(TF_IGNR_RXSACK)
		if (topt & TF_IGNR_RXSACK) {
		    (void) printf("%cIGNR_RXSACK", sep);
		    topt &= ~TF_IGNR_RXSACK;
		    sep = ',';
		}
# endif	/* defined(TF_IGNR_RXSACK) */

# if	defined(TF_IOLOCK)
		if (topt & TF_IOLOCK) {
		    (void) printf("%cIOLOCK", sep);
		    topt &= ~TF_IOLOCK;
		    sep = ',';
		}
# endif	/* defined(TF_IOLOCK) */

# if	defined(TF_LARGESEND)
		if (topt & TF_LARGESEND) {
		    (void) printf("%cLARGESEND", sep);
		    topt &= ~TF_LARGESEND;
		    sep = ',';
		}
# endif	/* defined(TF_LARGESEND) */

# if	defined(TF_LASTIDLE)
		if (topt & TF_LASTIDLE) {
		    (void) printf("%cLASTIDLE", sep);
		    topt &= ~TF_LASTIDLE;
		    sep = ',';
		}
# endif	/* defined(TF_LASTIDLE) */

# if	defined(TF_LQ_OVERFLOW)
		if (topt & TF_LQ_OVERFLOW) {
		    (void) printf("%cLQ_OVERFLOW", sep);
		    topt &= ~TF_LQ_OVERFLOW;
		    sep = ',';
		}
# endif	/* defined(TF_LQ_OVERFLOW) */

		if (Lf->lts.msss) {
		    (void) printf("%cMSS=%lu", sep, Lf->lts.mss);
		    sep = ',';
		}

# if	defined(TF_MORETOCOME)
		if (topt & TF_MORETOCOME) {
		    (void) printf("%cMORETOCOME", sep);
		    topt &= ~TF_MORETOCOME;
		    sep = ',';
		}
# endif	/* defined(TF_MORETOCOME) */

# if	defined(TF_NEEDACK)
		if (topt & TF_NEEDACK) {
		    (void) printf("%cNEEDACK", sep);
		    topt &= ~TF_NEEDACK;
		    sep = ',';
		}
# endif	/* defined(TF_NEEDACK) */

# if	defined(TF_NEEDCLOSE)
		if (topt & TF_NEEDCLOSE) {
		    (void) printf("%cNEEDCLOSE", sep);
		    topt &= ~TF_NEEDCLOSE;
		    sep = ',';
		}
# endif	/* defined(TF_NEEDCLOSE) */

# if	defined(TF_NEEDFIN)
		if (topt & TF_NEEDFIN) {
		    (void) printf("%cNEEDFIN", sep);
		    topt &= ~TF_NEEDFIN;
		    sep = ',';
		}
# endif	/* defined(TF_NEEDFIN) */

# if	defined(TF_NEEDIN)
		if (topt & TF_NEEDIN) {
		    (void) printf("%cNEEDIN", sep);
		    topt &= ~TF_NEEDIN;
		    sep = ',';
		}
# endif	/* defined(TF_NEEDIN) */

# if	defined(TF_NEEDOUT)
		if (topt & TF_NEEDOUT) {
		    (void) printf("%cNEEDOUT", sep);
		    topt &= ~TF_NEEDOUT;
		    sep = ',';
		}
# endif	/* defined(TF_NEEDOUT) */

# if	defined(TF_NEEDSYN)
		if (topt & TF_NEEDSYN) {
		    (void) printf("%cNEEDSYN", sep);
		    topt &= ~TF_NEEDSYN;
		    sep = ',';
		}
# endif	/* defined(TF_NEEDSYN) */

# if	defined(TF_NEEDTIMER)
		if (topt & TF_NEEDTIMER) {
		    (void) printf("%cNEEDTIMER", sep);
		    topt &= ~TF_NEEDTIMER;
		    sep = ',';
		}
# endif	/* defined(TF_NEEDTIMER) */

# if	defined(TF_NEWRENO_RXMT)
		if (topt & TF_NEWRENO_RXMT) {
		    (void) printf("%cNEWRENO_RXMT", sep);
		    topt &= ~TF_NEWRENO_RXMT;
		    sep = ',';
		}
# endif	/* defined(TF_NEWRENO_RXMT) */

# if	defined(TF_NODELACK)
		if (topt & TF_NODELACK) {
		    (void) printf("%cNODELACK", sep);
		    topt &= ~TF_NODELACK;
		    sep = ',';
		}
# endif	/* defined(TF_NODELACK) */

# if	defined(TF_NODELAY)
		if (topt & TF_NODELAY) {
		    (void) printf("%cNODELAY", sep);
		    topt &= ~TF_NODELAY;
		    sep = ',';
		}
# endif	/* defined(TF_NODELAY) */

# if	defined(TF_NOOPT)
		if (topt & TF_NOOPT) {
		    (void) printf("%cNOOPT", sep);
		    topt &= ~TF_NOOPT;
		    sep = ',';
		}
# endif	/* defined(TF_NOOPT) */

# if	defined(TF_NOPUSH)
		if (topt & TF_NOPUSH) {
		    (void) printf("%cNOPUSH", sep);
		    topt &= ~TF_NOPUSH;
		    sep = ',';
		}
# endif	/* defined(TF_NOPUSH) */

# if	defined(TF_NO_PMTU)
		if (topt & TF_NO_PMTU) {
		    (void) printf("%cNO_PMTU", sep);
		    topt &= ~TF_NO_PMTU;
		    sep = ',';
		}
# endif	/* defined(TF_NO_PMTU) */

# if	defined(TF_RAW)
		if (topt & TF_RAW) {
		    (void) printf("%cRAW", sep);
		    topt &= ~TF_RAW;
		    sep = ',';
		}
# endif	/* defined(TF_RAW) */

# if	defined(TF_RCVD_CC)
		if (topt & TF_RCVD_CC) {
		    (void) printf("%cRCVD_CC", sep);
		    topt &= ~TF_RCVD_CC;
		    sep = ',';
		}
# endif	/* defined(TF_RCVD_CC) */

# if	defined(TF_RCVD_SCALE)
		if (topt & TF_RCVD_SCALE) {
		    (void) printf("%cRCVD_SCALE", sep);
		    topt &= ~TF_RCVD_SCALE;
		    sep = ',';
		}
# endif	/* defined(TF_RCVD_SCALE) */

# if	defined(TF_RCVD_CE)
		if (topt & TF_RCVD_CE) {
		    (void) printf("%cRCVD_CE", sep);
		    topt &= ~TF_RCVD_CE;
		    sep = ',';
		}
# endif	/* defined(TF_RCVD_CE) */

# if	defined(TF_RCVD_TS)
		if (topt & TF_RCVD_TS) {
		    (void) printf("%cRCVD_TS", sep);
		    topt &= ~TF_RCVD_TS;
		    sep = ',';
		}
# endif	/* defined(TF_RCVD_TS) */

# if	defined(TF_RCVD_TSTMP)
		if (topt & TF_RCVD_TSTMP) {
		    (void) printf("%cRCVD_TSTMP", sep);
		    topt &= ~TF_RCVD_TSTMP;
		    sep = ',';
		}
# endif	/* defined(TF_RCVD_TSTMP) */

# if	defined(TF_RCVD_WS)
		if (topt & TF_RCVD_WS) {
		    (void) printf("%cRCVD_WS", sep);
		    topt &= ~TF_RCVD_WS;
		    sep = ',';
		}
# endif	/* defined(TF_RCVD_WS) */

# if	defined(TF_REASSEMBLING)
		if (topt & TF_REASSEMBLING) {
		    (void) printf("%cREASSEMBLING", sep);
		    topt &= ~TF_REASSEMBLING;
		    sep = ',';
		}
# endif	/* defined(TF_REASSEMBLING) */

# if	defined(TF_REQ_CC)
		if (topt & TF_REQ_CC) {
		    (void) printf("%cREQ_CC", sep);
		    topt &= ~TF_REQ_CC;
		    sep = ',';
		}
# endif	/* defined(TF_REQ_CC) */

# if	defined(TF_REQ_SCALE)
		if (topt & TF_REQ_SCALE) {
		    (void) printf("%cREQ_SCALE", sep);
		    topt &= ~TF_REQ_SCALE;
		    sep = ',';
		}
# endif	/* defined(TF_REQ_SCALE) */

# if	defined(TF_REQ_TSTMP)
		if (topt & TF_REQ_TSTMP) {
		    (void) printf("%cREQ_TSTMP", sep);
		    topt &= ~TF_REQ_TSTMP;
		    sep = ',';
		}
# endif	/* defined(TF_REQ_TSTMP) */

# if	defined(TF_RFC1323)
		if (topt & TF_RFC1323) {
		    (void) printf("%cRFC1323", sep);
		    topt &= ~TF_RFC1323;
		    sep = ',';
		}
# endif	/* defined(TF_RFC1323) */

# if	defined(TF_RXWIN0SENT)
		if (topt & TF_RXWIN0SENT) {
		    (void) printf("%cRXWIN0SENT", sep);
		    topt &= ~TF_RXWIN0SENT;
		    sep = ',';
		}
# endif	/* defined(TF_RXWIN0SENT) */

# if	defined(TF_SACK_GENERATE)
		if (topt & TF_SACK_GENERATE) {
		    (void) printf("%cSACK_GENERATE", sep);
		    topt &= ~TF_SACK_GENERATE;
		    sep = ',';
		}
# endif	/* defined(TF_SACK_GENERATE) */

# if	defined(TF_SACK_PERMIT)
		if (topt & TF_SACK_PERMIT) {
		    (void) printf("%cSACK_PERMIT", sep);
		    topt &= ~TF_SACK_PERMIT;
		    sep = ',';
		}
# endif	/* defined(TF_SACK_PERMIT) */

# if	defined(TF_SACK_PROCESS)
		if (topt & TF_SACK_PROCESS) {
		    (void) printf("%cSACK_PROCESS", sep);
		    topt &= ~TF_SACK_PROCESS;
		    sep = ',';
		}
# endif	/* defined(TF_SACK_PROCESS) */

# if	defined(TF_SEND)
		if (topt & TF_SEND) {
		    (void) printf("%cSEND", sep);
		    topt &= ~TF_SEND;
		    sep = ',';
		}
# endif	/* defined(TF_SEND) */

# if	defined(TF_SEND_AND_DISCONNECT)
		if (topt & TF_SEND_AND_DISCONNECT) {
		    (void) printf("%cSEND_AND_DISCONNECT", sep);
		    topt &= ~TF_SEND_AND_DISCONNECT;
		    sep = ',';
		}
# endif	/* defined(TF_SEND_AND_DISCONNECT) */

# if	defined(TF_SENDCCNEW)
		if (topt & TF_SENDCCNEW) {
		    (void) printf("%cSENDCCNEW", sep);
		    topt &= ~TF_SENDCCNEW;
		    sep = ',';
		}
# endif	/* defined(TF_SENDCCNEW) */

# if	defined(TF_SEND_CWR)
		if (topt & TF_SEND_CWR) {
		    (void) printf("%cSEND_CWR", sep);
		    topt &= ~TF_SEND_CWR;
		    sep = ',';
		}
# endif	/* defined(TF_SEND_CWR) */

# if	defined(TF_SEND_ECHO)
		if (topt & TF_SEND_ECHO) {
		    (void) printf("%cSEND_ECHO", sep);
		    topt &= ~TF_SEND_ECHO;
		    sep = ',';
		}
# endif	/* defined(TF_SEND_ECHO) */

# if	defined(TF_SEND_TSTMP)
		if (topt & TF_SEND_TSTMP) {
		    (void) printf("%cSEND_TSTMP", sep);
		    topt &= ~TF_SEND_TSTMP;
		    sep = ',';
		}
# endif	/* defined(TF_SEND_TSTMP) */

# if	defined(TF_SENTFIN)
		if (topt & TF_SENTFIN) {
		    (void) printf("%cSENTFIN", sep);
		    topt &= ~TF_SENTFIN;
		    sep = ',';
		}
# endif	/* defined(TF_SENTFIN) */

# if	defined(TF_SENT_TS)
		if (topt & TF_SENT_TS) {
		    (void) printf("%cSENT_TS", sep);
		    topt &= ~TF_SENT_TS;
		    sep = ',';
		}
# endif	/* defined(TF_SENT_TS) */

# if	defined(TF_SENT_WS)
		if (topt & TF_SENT_WS) {
		    (void) printf("%cSENT_WS", sep);
		    topt &= ~TF_SENT_WS;
		    sep = ',';
		}
# endif	/* defined(TF_SENT_WS) */

# if	defined(TF_SIGNATURE)
		if (topt & TF_SIGNATURE) {
		    (void) printf("%cSIGNATURE", sep);
		    topt &= ~TF_SIGNATURE;
		    sep = ',';
		}
# endif	/* defined(TF_SIGNATURE) */

# if	defined(TF_SLOWLINK)
		if (topt & TF_SLOWLINK) {
		    (void) printf("%cSLOWLINK", sep);
		    topt &= ~TF_SLOWLINK;
		    sep = ',';
		}
# endif	/* defined(TF_SLOWLINK) */

# if	defined(TF_STDURG)
		if (topt & TF_STDURG) {
		    (void) printf("%cSTDURG", sep);
		    topt &= ~TF_STDURG;
		    sep = ',';
		}
# endif	/* defined(TF_STDURG) */

# if	defined(TF_SYN_REXMT)
		if (topt & TF_SYN_REXMT) {
		    (void) printf("%cSYN_REXMT", sep);
		    topt &= ~TF_SYN_REXMT;
		    sep = ',';
		}
# endif	/* defined(TF_SYN_REXMT) */

# if	defined(TF_UIOMOVED)
		if (topt & TF_UIOMOVED) {
		    (void) printf("%cUIOMOVED", sep);
		    topt &= ~TF_UIOMOVED;
		    sep = ',';
		}
# endif	/* defined(TF_UIOMOVED) */

# if	defined(TF_USE_SCALE)
		if (topt & TF_USE_SCALE) {
		    (void) printf("%cUSE_SCALE", sep);
		    topt &= ~TF_USE_SCALE;
		    sep = ',';
		}
# endif	/* defined(TF_USE_SCALE) */

# if	defined(TF_WASIDLE)
		if (topt & TF_WASIDLE) {
		    (void) printf("%cWASIDLE", sep);
		    topt &= ~TF_WASIDLE;
		    sep = ',';
		}
# endif	/* defined(TF_WASIDLE) */

# if	defined(TF_WASFRECOVERY)
		if (topt & TF_WASFRECOVERY) {
		    (void) printf("%cWASFRECOVERY", sep);
		    topt &= ~TF_WASFRECOVERY;
		    sep = ',';
		}
# endif	/* defined(TF_WASFRECOVERY) */

# if	defined(TF_WILL_SACK)
		if (topt & TF_WILL_SACK) {
		    (void) printf("%cWILL_SACK", sep);
		    topt &= ~TF_WILL_SACK;
		    sep = ',';
		}
# endif	/* defined(TF_WILL_SACK) */

		if (topt)
		    (void) printf("%cUNKNOWN=%#x", sep, topt);
		if (Ffield)
		    putchar(Terminator);
	    }
	}
#endif	/* defined(HASTCPOPT) */

#if	defined(HASTCPTPIW)
	if (Ftcptpi & TCPTPI_WINDOWS) {
	    if (Lf->lts.rws) {
		if (Ffield)
		    putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("WR=%lu", Lf->lts.rw);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	    if (Lf->lts.wws) {
		if (Ffield)
		    putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("WW=%lu", Lf->lts.ww);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	}
#endif	/* defined(HASTCPTPIW) */

	if (ps && !Ffield)
	    putchar(')');
	if (nl)
	    putchar('\n');
}
#else	/* !defined(USE_LIB_PRINT_TCPTPI) */
char ptti_d1[] = "d"; char *ptti_d2 = ptti_d1;
#endif	/* defined(USE_LIB_PRINT_TCPTPI) */
