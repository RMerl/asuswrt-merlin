/*
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id$
 */

#include "defs.h"
#include <sys/syscall.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif
#ifdef HAVE_SYS_CONF_H
#include <sys/conf.h>
#endif
#ifdef HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#ifdef HAVE_SYS_TIHDR_H
#include <sys/tihdr.h>
#endif

#if defined(HAVE_SYS_STREAM_H) || defined(LINUX) || defined(FREEBSD)

#ifndef HAVE_STROPTS_H
#define RS_HIPRI 1
struct strbuf {
	int     maxlen;                 /* no. of bytes in buffer */
	int     len;                    /* no. of bytes returned */
	const char *buf;                /* pointer to data */
};
#define MORECTL 1
#define MOREDATA 2
#endif /* !HAVE_STROPTS_H */

#ifdef HAVE_SYS_TIUSER_H
#include <sys/tiuser.h>
#include <sys/sockmod.h>
#include <sys/timod.h>
#endif /* HAVE_SYS_TIUSER_H */

#ifndef FREEBSD
static const struct xlat msgflags[] = {
	{ RS_HIPRI,	"RS_HIPRI"	},
	{ 0,		NULL		},
};


static void
printstrbuf(tcp, sbp, getting)
struct tcb *tcp;
struct strbuf *sbp;
int getting;
{
	if (sbp->maxlen == -1 && getting)
		tprintf("{maxlen=-1}");
	else {
		tprintf("{");
		if (getting)
			tprintf("maxlen=%d, ", sbp->maxlen);
		tprintf("len=%d, buf=", sbp->len);
		printstr(tcp, (unsigned long) sbp->buf, sbp->len);
		tprintf("}");
	}
}

static void
printstrbufarg(tcp, arg, getting)
struct tcb *tcp;
int arg;
int getting;
{
	struct strbuf buf;

	if (arg == 0)
		tprintf("NULL");
	else if (umove(tcp, arg, &buf) < 0)
		tprintf("{...}");
	else
		printstrbuf(tcp, &buf, getting);
	tprintf(", ");
}

int
sys_putmsg(tcp)
struct tcb *tcp;
{
	int i;

	if (entering(tcp)) {
		/* fd */
		tprintf("%ld, ", tcp->u_arg[0]);
		/* control and data */
		for (i = 1; i < 3; i++)
			printstrbufarg(tcp, tcp->u_arg[i], 0);
		/* flags */
		printflags(msgflags, tcp->u_arg[3], "RS_???");
	}
	return 0;
}

#if defined(SPARC) || defined(SPARC64) || defined(SUNOS4) || defined(SVR4)
int
sys_getmsg(tcp)
struct tcb *tcp;
{
	int i, flags;

	if (entering(tcp)) {
		/* fd */
		tprintf("%lu, ", tcp->u_arg[0]);
	} else {
		if (syserror(tcp)) {
			tprintf("%#lx, %#lx, %#lx",
				tcp->u_arg[1], tcp->u_arg[2], tcp->u_arg[3]);
			return 0;
		}
		/* control and data */
		for (i = 1; i < 3; i++)
			printstrbufarg(tcp, tcp->u_arg[i], 1);
		/* pointer to flags */
		if (tcp->u_arg[3] == 0)
			tprintf("NULL");
		else if (umove(tcp, tcp->u_arg[3], &flags) < 0)
			tprintf("[?]");
		else {
			tprintf("[");
			printflags(msgflags, flags, "RS_???");
			tprintf("]");
		}
		/* decode return value */
		switch (tcp->u_rval) {
		case MORECTL:
			tcp->auxstr = "MORECTL";
			break;
		case MORECTL|MOREDATA:
			tcp->auxstr = "MORECTL|MOREDATA";
			break;
		case MOREDATA:
			tcp->auxstr = "MORECTL";
			break;
		default:
			tcp->auxstr = NULL;
			break;
		}
	}
	return RVAL_HEX | RVAL_STR;
}
#endif /* SPARC || SPARC64 || SUNOS4 || SVR4 */

#if defined SYS_putpmsg || defined SYS_getpmsg
static const struct xlat pmsgflags[] = {
#ifdef MSG_HIPRI
	{ MSG_HIPRI,	"MSG_HIPRI"	},
#endif
#ifdef MSG_AND
	{ MSG_ANY,	"MSG_ANY"	},
#endif
#ifdef MSG_BAND
	{ MSG_BAND,	"MSG_BAND"	},
#endif
	{ 0,		NULL		},
};
#endif

#ifdef SYS_putpmsg
int
sys_putpmsg(tcp)
struct tcb *tcp;
{
	int i;

	if (entering(tcp)) {
		/* fd */
		tprintf("%ld, ", tcp->u_arg[0]);
		/* control and data */
		for (i = 1; i < 3; i++)
			printstrbufarg(tcp, tcp->u_arg[i], 0);
		/* band */
		tprintf("%ld, ", tcp->u_arg[3]);
		/* flags */
		printflags(pmsgflags, tcp->u_arg[4], "MSG_???");
	}
	return 0;
}
#endif /* SYS_putpmsg */

#ifdef SYS_getpmsg
int
sys_getpmsg(tcp)
struct tcb *tcp;
{
	int i, flags;

	if (entering(tcp)) {
		/* fd */
		tprintf("%lu, ", tcp->u_arg[0]);
	} else {
		if (syserror(tcp)) {
			tprintf("%#lx, %#lx, %#lx, %#lx", tcp->u_arg[1],
				tcp->u_arg[2], tcp->u_arg[3], tcp->u_arg[4]);
			return 0;
		}
		/* control and data */
		for (i = 1; i < 3; i++)
			printstrbufarg(tcp, tcp->u_arg[i], 1);
		/* pointer to band */
		printnum(tcp, tcp->u_arg[3], "%d");
		tprintf(", ");
		/* pointer to flags */
		if (tcp->u_arg[4] == 0)
			tprintf("NULL");
		else if (umove(tcp, tcp->u_arg[4], &flags) < 0)
			tprintf("[?]");
		else {
			tprintf("[");
			printflags(pmsgflags, flags, "MSG_???");
			tprintf("]");
		}
		/* decode return value */
		switch (tcp->u_rval) {
		case MORECTL:
			tcp->auxstr = "MORECTL";
			break;
		case MORECTL|MOREDATA:
			tcp->auxstr = "MORECTL|MOREDATA";
			break;
		case MOREDATA:
			tcp->auxstr = "MORECTL";
			break;
		default:
			tcp->auxstr = NULL;
			break;
		}
	}
	return RVAL_HEX | RVAL_STR;
}
#endif /* SYS_getpmsg */

#endif /* !FREEBSD */


#ifdef HAVE_SYS_POLL_H

static const struct xlat pollflags[] = {
#ifdef POLLIN
	{ POLLIN,	"POLLIN"	},
	{ POLLPRI,	"POLLPRI"	},
	{ POLLOUT,	"POLLOUT"	},
#ifdef POLLRDNORM
	{ POLLRDNORM,	"POLLRDNORM"	},
#endif
#ifdef POLLWRNORM
	{ POLLWRNORM,	"POLLWRNORM"	},
#endif
#ifdef POLLRDBAND
	{ POLLRDBAND,	"POLLRDBAND"	},
#endif
#ifdef POLLWRBAND
	{ POLLWRBAND,	"POLLWRBAND"	},
#endif
	{ POLLERR,	"POLLERR"	},
	{ POLLHUP,	"POLLHUP"	},
	{ POLLNVAL,	"POLLNVAL"	},
#endif
	{ 0,		NULL		},
};

static int
decode_poll(struct tcb *tcp, long pts)
{
	struct pollfd fds;
	unsigned nfds;
	unsigned long size, start, cur, end, abbrev_end;
	int failed = 0;

	if (entering(tcp)) {
		nfds = tcp->u_arg[1];
		size = sizeof(fds) * nfds;
		start = tcp->u_arg[0];
		end = start + size;
		if (nfds == 0 || size / sizeof(fds) != nfds || end < start) {
			tprintf("%#lx, %d, ",
				tcp->u_arg[0], nfds);
			return 0;
		}
		if (abbrev(tcp)) {
			abbrev_end = start + max_strlen * sizeof(fds);
			if (abbrev_end < start)
				abbrev_end = end;
		} else {
			abbrev_end = end;
		}
		tprintf("[");
		for (cur = start; cur < end; cur += sizeof(fds)) {
			if (cur > start)
				tprintf(", ");
			if (cur >= abbrev_end) {
				tprintf("...");
				break;
			}
			if (umoven(tcp, cur, sizeof fds, (char *) &fds) < 0) {
				tprintf("?");
				failed = 1;
				break;
			}
			if (fds.fd < 0) {
				tprintf("{fd=%d}", fds.fd);
				continue;
			}
			tprintf("{fd=");
			printfd(tcp, fds.fd);
			tprintf(", events=");
			printflags(pollflags, fds.events, "POLL???");
			tprintf("}");
		}
		tprintf("]");
		if (failed)
			tprintf(" %#lx", start);
		tprintf(", %d, ", nfds);
		return 0;
	} else {
		static char outstr[1024];
		char str[64];
		const char *flagstr;
		unsigned int cumlen;

		if (syserror(tcp))
			return 0;
		if (tcp->u_rval == 0) {
			tcp->auxstr = "Timeout";
			return RVAL_STR;
		}

		nfds = tcp->u_arg[1];
		size = sizeof(fds) * nfds;
		start = tcp->u_arg[0];
		end = start + size;
		if (nfds == 0 || size / sizeof(fds) != nfds || end < start)
			return 0;
		if (abbrev(tcp)) {
			abbrev_end = start + max_strlen * sizeof(fds);
			if (abbrev_end < start)
				abbrev_end = end;
		} else {
			abbrev_end = end;
		}

		outstr[0] = '\0';
		cumlen = 0;

		for (cur = start; cur < end; cur += sizeof(fds)) {
			if (umoven(tcp, cur, sizeof fds, (char *) &fds) < 0) {
				++cumlen;
				if (cumlen < sizeof(outstr))
					strcat(outstr, "?");
				failed = 1;
				break;
			}
			if (!fds.revents)
				continue;
			if (!cumlen) {
				++cumlen;
				strcat(outstr, "[");
			} else {
				cumlen += 2;
				if (cumlen < sizeof(outstr))
					strcat(outstr, ", ");
			}
			if (cur >= abbrev_end) {
				cumlen += 3;
				if (cumlen < sizeof(outstr))
					strcat(outstr, "...");
				break;
			}
			sprintf(str, "{fd=%d, revents=", fds.fd);
			flagstr=sprintflags("", pollflags, fds.revents);
			cumlen += strlen(str) + strlen(flagstr) + 1;
			if (cumlen < sizeof(outstr)) {
				strcat(outstr, str);
				strcat(outstr, flagstr);
				strcat(outstr, "}");
			}
		}
		if (failed)
			return 0;

		if (cumlen && ++cumlen < sizeof(outstr))
			strcat(outstr, "]");

		if (pts) {
			char str[128];

			sprintf(str, "%sleft ", cumlen ? ", " : "");
			sprint_timespec(str + strlen(str), tcp, pts);
			if ((cumlen += strlen(str)) < sizeof(outstr))
				strcat(outstr, str);
		}

		if (!outstr[0])
			return 0;

		tcp->auxstr = outstr;
		return RVAL_STR;
	}
}

int
sys_poll(struct tcb *tcp)
{
	int rc = decode_poll(tcp, 0);
	if (entering(tcp)) {
#ifdef INFTIM
		if (tcp->u_arg[2] == INFTIM)
			tprintf("INFTIM");
		else
#endif
			tprintf("%ld", tcp->u_arg[2]);
	}
	return rc;
}

#ifdef LINUX
int
sys_ppoll(struct tcb *tcp)
{
	int rc = decode_poll(tcp, tcp->u_arg[2]);
	if (entering(tcp)) {
		print_timespec(tcp, tcp->u_arg[2]);
		tprintf(", ");
		print_sigset(tcp, tcp->u_arg[3], 0);
		tprintf(", %lu", tcp->u_arg[4]);
	}
	return rc;
}
#endif

#else /* !HAVE_SYS_POLL_H */
int
sys_poll(tcp)
struct tcb *tcp;
{
	return 0;
}
#endif

#if !defined(LINUX) && !defined(FREEBSD)

static const struct xlat stream_flush_options[] = {
	{ FLUSHR,	"FLUSHR"	},
	{ FLUSHW,	"FLUSHW"	},
	{ FLUSHRW,	"FLUSHRW"	},
#ifdef FLUSHBAND
	{ FLUSHBAND,	"FLUSHBAND"	},
#endif
	{ 0,		NULL		},
};

static const struct xlat stream_setsig_flags[] = {
	{ S_INPUT,	"S_INPUT"	},
	{ S_HIPRI,	"S_HIPRI"	},
	{ S_OUTPUT,	"S_OUTPUT"	},
	{ S_MSG,	"S_MSG"		},
#ifdef S_ERROR
	{ S_ERROR,	"S_ERROR"	},
#endif
#ifdef S_HANGUP
	{ S_HANGUP,	"S_HANGUP"	},
#endif
#ifdef S_RDNORM
	{ S_RDNORM,	"S_RDNORM"	},
#endif
#ifdef S_WRNORM
	{ S_WRNORM,	"S_WRNORM"	},
#endif
#ifdef S_RDBAND
	{ S_RDBAND,	"S_RDBAND"	},
#endif
#ifdef S_WRBAND
	{ S_WRBAND,	"S_WRBAND"	},
#endif
#ifdef S_BANDURG
	{ S_BANDURG,	"S_BANDURG"	},
#endif
	{ 0,		NULL		},
};

static const struct xlat stream_read_options[] = {
	{ RNORM,	"RNORM"		},
	{ RMSGD,	"RMSGD"		},
	{ RMSGN,	"RMSGN"		},
	{ 0,		NULL		},
};

static const struct xlat stream_read_flags[] = {
#ifdef RPROTDAT
	{ RPROTDAT,	"RPROTDAT"	},
#endif
#ifdef RPROTDIS
	{ RPROTDIS,	"RPROTDIS"	},
#endif
#ifdef RPROTNORM
	{ RPROTNORM,	"RPROTNORM"	},
#endif
	{ 0,		NULL		},
};

#ifndef RMODEMASK
#define RMODEMASK (~0)
#endif

#ifdef I_SWROPT
static const struct xlat stream_write_flags[] = {
	{ SNDZERO,	"SNDZERO"	},
	{ SNDPIPE,	"SNDPIPE"	},
	{ 0,		NULL		},
};
#endif /* I_SWROPT */

#ifdef I_ATMARK
static const struct xlat stream_atmark_options[] = {
	{ ANYMARK,	"ANYMARK"	},
	{ LASTMARK,	"LASTMARK"	},
	{ 0,		NULL		},
};
#endif /* I_ATMARK */

#ifdef TI_BIND
static const struct xlat transport_user_options[] = {
	{ T_CONN_REQ,	"T_CONN_REQ"	},
	{ T_CONN_RES,	"T_CONN_RES"	},
	{ T_DISCON_REQ,	"T_DISCON_REQ"	},
	{ T_DATA_REQ,	"T_DATA_REQ"	},
	{ T_EXDATA_REQ,	"T_EXDATA_REQ"	},
	{ T_INFO_REQ,	"T_INFO_REQ"	},
	{ T_BIND_REQ,	"T_BIND_REQ"	},
	{ T_UNBIND_REQ,	"T_UNBIND_REQ"	},
	{ T_UNITDATA_REQ,"T_UNITDATA_REQ"},
	{ T_OPTMGMT_REQ,"T_OPTMGMT_REQ"	},
	{ T_ORDREL_REQ,	"T_ORDREL_REQ"	},
	{ 0,		NULL		},
};

static const struct xlat transport_user_flags [] = {
	{ 0,		"0"		},
	{ T_MORE,	"T_MORE"	},
	{ T_EXPEDITED,	"T_EXPEDITED"	},
	{ T_NEGOTIATE,	"T_NEGOTIATE"	},
	{ T_CHECK,	"T_CHECK"	},
	{ T_DEFAULT,	"T_DEFAULT"	},
	{ T_SUCCESS,	"T_SUCCESS"	},
	{ T_FAILURE,	"T_FAILURE"	},
	{ T_CURRENT,	"T_CURRENT"	},
	{ T_PARTSUCCESS,"T_PARTSUCCESS"	},
	{ T_READONLY,	"T_READONLY"	},
	{ T_NOTSUPPORT,	"T_NOTSUPPORT"	},
	{ 0,		NULL		},
};


#ifdef HAVE_STRUCT_T_OPTHDR

static const struct xlat xti_level [] = {
	{ XTI_GENERIC,	"XTI_GENERIC"	},
	{ 0,		NULL		},
};

static const struct xlat xti_generic [] = {
	{ XTI_DEBUG,	"XTI_DEBUG"	},
	{ XTI_LINGER,	"XTI_LINGER"	},
	{ XTI_RCVBUF,	"XTI_RCVBUF"	},
	{ XTI_RCVLOWAT,	"XTI_RCVLOWAT"	},
	{ XTI_SNDBUF,	"XTI_SNDBUF"	},
	{ XTI_SNDLOWAT,	"XTI_SNDLOWAT"	},
	{ 0,		NULL		},
};



void
print_xti_optmgmt (tcp, addr, len)
struct tcb *tcp;
long addr;
int len;
{
	int c = 0;
	struct t_opthdr hdr;

	while (len >= (int) sizeof hdr) {
		if (umove(tcp, addr, &hdr) < 0) break;
		if (c++) {
			tprintf (", ");
		}
		else if (len > hdr.len + sizeof hdr) {
			tprintf ("[");
		}
		tprintf ("{level=");
		printxval (xti_level, hdr.level, "???");
		tprintf (", name=");
		switch (hdr.level) {
		    case XTI_GENERIC:
			printxval (xti_generic, hdr.name, "XTI_???");
			break;
		    default:
			tprintf ("%ld", hdr.name);
			break;
		}
		tprintf (", status=");
		printxval (transport_user_flags,  hdr.status, "T_???");
		addr += sizeof hdr;
		len -= sizeof hdr;
		if ((hdr.len -= sizeof hdr) > 0) {
			if (hdr.len > len) break;
			tprintf (", val=");
			if (len == sizeof (int))
				printnum (tcp, addr, "%d");
			else
				printstr (tcp, addr, hdr.len);
			addr += hdr.len;
			len -= hdr.len;
		}
		tprintf ("}");
	}
	if (len > 0) {
		if (c++) tprintf (", ");
		printstr (tcp, addr, len);
	}
	if (c > 1) tprintf ("]");
}

#endif


static void
print_optmgmt (tcp, addr, len)
struct tcb *tcp;
long addr;
int len;
{
	/* We don't know how to tell if TLI (socket) or XTI
	   optmgmt is being used yet, assume TLI. */
#if defined (HAVE_STRUCT_OPTHDR)
	print_sock_optmgmt (tcp, addr, len);
#elif defined (HAVE_STRUCT_T_OPTHDR)
	print_xti_optmgmt (tcp, addr, len);
#else
	printstr (tcp, addr, len);
#endif
}




static const struct xlat service_type [] = {
	{ T_COTS,	"T_COTS"	},
	{ T_COTS_ORD,	"T_COTS_ORD"	},
	{ T_CLTS,	"T_CLTS"	},
	{ 0,		NULL		},
};

static const struct xlat ts_state [] = {
	{ TS_UNBND,	"TS_UNBND"	},
	{ TS_WACK_BREQ,	"TS_WACK_BREQ"	},
	{ TS_WACK_UREQ,	"TS_WACK_UREQ"	},
	{ TS_IDLE,	"TS_IDLE"	},
	{ TS_WACK_OPTREQ,"TS_WACK_OPTREQ"},
	{ TS_WACK_CREQ,	"TS_WACK_CREQ"	},
	{ TS_WCON_CREQ,	"TS_WCON_CREQ"	},
	{ TS_WRES_CIND,	"TS_WRES_CIND"	},
	{ TS_WACK_CRES,	"TS_WACK_CRES"	},
	{ TS_DATA_XFER,	"TS_DATA_XFER"	},
	{ TS_WIND_ORDREL,"TS_WIND_ORDREL"},
	{ TS_WREQ_ORDREL,"TS_WREQ_ORDREL"},
	{ TS_WACK_DREQ6,"TS_WACK_DREQ6"	},
	{ TS_WACK_DREQ7,"TS_WACK_DREQ7"	},
	{ TS_WACK_DREQ9,"TS_WACK_DREQ9"	},
	{ TS_WACK_DREQ10,"TS_WACK_DREQ10"},
	{ TS_WACK_DREQ11,"TS_WACK_DREQ11"},
	{ 0,		NULL		},
};

static const struct xlat provider_flags [] = {
	{ 0,		"0"		},
	{ SENDZERO,	"SENDZERO"	},
	{ EXPINLINE,	"EXPINLINE"	},
	{ XPG4_1,	"XPG4_1"	},
	{ 0,		NULL		},
};


static const struct xlat tli_errors [] = {
	{ TBADADDR,	"TBADADDR"	},
	{ TBADOPT,	"TBADOPT"	},
	{ TACCES,	"TACCES"	},
	{ TBADF,	"TBADF"		},
	{ TNOADDR,	"TNOADDR"	},
	{ TOUTSTATE,	"TOUTSTATE"	},
	{ TBADSEQ,	"TBADSEQ"	},
	{ TSYSERR,	"TSYSERR"	},
	{ TLOOK,	"TLOOK"		},
	{ TBADDATA,	"TBADDATA"	},
	{ TBUFOVFLW,	"TBUFOVFLW"	},
	{ TFLOW,	"TFLOW"		},
	{ TNODATA,	"TNODATA"	},
	{ TNODIS,	"TNODIS"	},
	{ TNOUDERR,	"TNOUDERR"	},
	{ TBADFLAG,	"TBADFLAG"	},
	{ TNOREL,	"TNOREL"	},
	{ TNOTSUPPORT,	"TNOTSUPPORT"	},
	{ TSTATECHNG,	"TSTATECHNG"	},
	{ TNOSTRUCTYPE,	"TNOSTRUCTYPE"	},
	{ TBADNAME,	"TBADNAME"	},
	{ TBADQLEN,	"TBADQLEN"	},
	{ TADDRBUSY,	"TADDRBUSY"	},
	{ TINDOUT,	"TINDOUT"	},
	{ TPROVMISMATCH,"TPROVMISMATCH"	},
	{ TRESQLEN,	"TRESQLEN"	},
	{ TRESADDR,	"TRESADDR"	},
	{ TQFULL,	"TQFULL"	},
	{ TPROTO,	"TPROTO"	},
	{ 0,		NULL		},
};


static int
print_transport_message (tcp, expect, addr, len)
struct tcb *tcp;
int expect;
long addr;
int len;
{
	union T_primitives m;
	int c = 0;

	if (len < sizeof m.type) goto dump;

	if (umove (tcp, addr, &m.type) < 0) goto dump;

#define GET(type, struct)	\
	do {							\
		if (len < sizeof m.struct) goto dump;		\
		if (umove (tcp, addr, &m.struct) < 0) goto dump;\
		tprintf ("{");					\
		if (expect != type) {				\
			++c;					\
			tprintf (#type);			\
		}						\
	}							\
	while (0)

#define COMMA() \
	do { if (c++) tprintf (", "); } while (0)


#define STRUCT(struct, elem, print)					\
	do {								\
		COMMA ();						\
		if (m.struct.elem##_length < 0 ||			\
		    m.struct.elem##_offset < sizeof m.struct ||		\
		    m.struct.elem##_offset + m.struct.elem##_length > len) \
		{							\
			tprintf (#elem "_length=%ld, " #elem "_offset=%ld",\
				m.struct.elem##_length,			\
				m.struct.elem##_offset);		\
		}							\
		else {							\
			tprintf (#elem "=");				\
			print (tcp,					\
				 addr + m.struct.elem##_offset,		\
				 m.struct.elem##_length);		\
		}							\
	}								\
	while (0)

#define ADDR(struct, elem) STRUCT (struct, elem, printstr)

	switch (m.type) {
#ifdef T_CONN_REQ
	    case T_CONN_REQ:	/* connect request   */
		GET (T_CONN_REQ, conn_req);
		ADDR (conn_req, DEST);
		ADDR (conn_req, OPT);
		break;
#endif
#ifdef T_CONN_RES
	    case T_CONN_RES:	/* connect response   */
		GET (T_CONN_RES, conn_res);
#ifdef HAVE_STRUCT_T_CONN_RES_QUEUE_PTR
		COMMA ();
		tprintf ("QUEUE=%p", m.conn_res.QUEUE_ptr);
#elif defined HAVE_STRUCT_T_CONN_RES_ACCEPTOR_ID
		COMMA ();
		tprintf ("ACCEPTOR=%#lx", m.conn_res.ACCEPTOR_id);
#endif
		ADDR (conn_res, OPT);
		COMMA ();
		tprintf ("SEQ=%ld", m.conn_res.SEQ_number);
		break;
#endif
#ifdef T_DISCON_REQ
	    case T_DISCON_REQ:	/* disconnect request */
		GET (T_DISCON_REQ, discon_req);
		COMMA ();
		tprintf ("SEQ=%ld", m.discon_req.SEQ_number);
		break;
#endif
#ifdef T_DATA_REQ
	    case T_DATA_REQ:	/* data request       */
		GET (T_DATA_REQ, data_req);
		COMMA ();
		tprintf ("MORE=%ld", m.data_req.MORE_flag);
		break;
#endif
#ifdef T_EXDATA_REQ
	    case T_EXDATA_REQ:	/* expedited data req */
		GET (T_EXDATA_REQ, exdata_req);
		COMMA ();
		tprintf ("MORE=%ld", m.exdata_req.MORE_flag);
		break;
#endif
#ifdef T_INFO_REQ
	    case T_INFO_REQ:	/* information req    */
		GET (T_INFO_REQ, info_req);
		break;
#endif
#ifdef T_BIND_REQ
	    case T_BIND_REQ:	/* bind request       */
#ifdef O_T_BIND_REQ
	    case O_T_BIND_REQ:	/* Ugly xti/tli hack */
#endif
		GET (T_BIND_REQ, bind_req);
		ADDR (bind_req, ADDR);
		COMMA ();
		tprintf ("CONIND=%ld", m.bind_req.CONIND_number);
		break;
#endif
#ifdef T_UNBIND_REQ
	    case T_UNBIND_REQ:	/* unbind request     */
		GET (T_UNBIND_REQ, unbind_req);
		break;
#endif
#ifdef T_UNITDATA_REQ
	    case T_UNITDATA_REQ:	/* unitdata requset   */
		GET (T_UNITDATA_REQ, unitdata_req);
		ADDR (unitdata_req, DEST);
		ADDR (unitdata_req, OPT);
		break;
#endif
#ifdef T_OPTMGMT_REQ
	    case T_OPTMGMT_REQ:	/* manage opt req     */
		GET (T_OPTMGMT_REQ, optmgmt_req);
		COMMA ();
		tprintf ("MGMT=");
		printflags (transport_user_flags, m.optmgmt_req.MGMT_flags,
			    "T_???");
		STRUCT (optmgmt_req, OPT, print_optmgmt);
		break;
#endif
#ifdef T_ORDREL_REQ
	    case T_ORDREL_REQ:	/* orderly rel req    */
		GET (T_ORDREL_REQ, ordrel_req);
		break;
#endif
#ifdef T_CONN_IND
	    case T_CONN_IND:	/* connect indication */
		GET (T_CONN_IND, conn_ind);
		ADDR (conn_ind, SRC);
		ADDR (conn_ind, OPT);
		tprintf (", SEQ=%ld", m.conn_ind.SEQ_number);
		break;
#endif
#ifdef T_CONN_CON
	    case T_CONN_CON:	/* connect corfirm    */
		GET (T_CONN_CON, conn_con);
		ADDR (conn_con, RES);
		ADDR (conn_con, OPT);
		break;
#endif
#ifdef T_DISCON_IND
	    case T_DISCON_IND:	/* discon indication  */
		GET (T_DISCON_IND, discon_ind);
		COMMA ();
		tprintf ("DISCON=%ld, SEQ=%ld",
			 m.discon_ind.DISCON_reason, m.discon_ind.SEQ_number);
		break;
#endif
#ifdef T_DATA_IND
	    case T_DATA_IND:	/* data indication    */
		GET (T_DATA_IND, data_ind);
		COMMA ();
		tprintf ("MORE=%ld", m.data_ind.MORE_flag);
		break;
#endif
#ifdef T_EXDATA_IND
	    case T_EXDATA_IND:	/* expedited data ind */
		GET (T_EXDATA_IND, exdata_ind);
		COMMA ();
		tprintf ("MORE=%ld", m.exdata_ind.MORE_flag);
		break;
#endif
#ifdef T_INFO_ACK
	    case T_INFO_ACK:	/* info ack           */
		GET (T_INFO_ACK, info_ack);
		COMMA ();
		tprintf ("TSDU=%ld, ETSDU=%ld, CDATA=%ld, DDATA=%ld, "
			 "ADDR=%ld, OPT=%ld, TIDU=%ld, SERV=",
			 m.info_ack.TSDU_size, m.info_ack.ETSDU_size,
			 m.info_ack.CDATA_size, m.info_ack.DDATA_size,
			 m.info_ack.ADDR_size, m.info_ack.OPT_size,
			 m.info_ack.TIDU_size);
		printxval (service_type, m.info_ack.SERV_type, "T_???");
		tprintf (", CURRENT=");
		printxval (ts_state, m.info_ack.CURRENT_state, "TS_???");
		tprintf (", PROVIDER=");
		printflags (provider_flags, m.info_ack.PROVIDER_flag, "???");
		break;
#endif
#ifdef T_BIND_ACK
	    case T_BIND_ACK:	/* bind ack           */
		GET (T_BIND_ACK, bind_ack);
		ADDR (bind_ack, ADDR);
		tprintf (", CONIND=%ld", m.bind_ack.CONIND_number);
		break;
#endif
#ifdef T_ERROR_ACK
	    case T_ERROR_ACK:	/* error ack          */
		GET (T_ERROR_ACK, error_ack);
		COMMA ();
		tprintf ("ERROR=");
		printxval (transport_user_options,
			   m.error_ack.ERROR_prim, "TI_???");
		tprintf (", TLI=");
		printxval (tli_errors, m.error_ack.TLI_error, "T???");
		tprintf ("UNIX=%s", strerror (m.error_ack.UNIX_error));
		break;
#endif
#ifdef T_OK_ACK
	    case T_OK_ACK:	/* ok ack             */
		GET (T_OK_ACK, ok_ack);
		COMMA ();
		tprintf ("CORRECT=");
		printxval (transport_user_options,
			   m.ok_ack.CORRECT_prim, "TI_???");
		break;
#endif
#ifdef T_UNITDATA_IND
	    case T_UNITDATA_IND:	/* unitdata ind       */
		GET (T_UNITDATA_IND, unitdata_ind);
		ADDR (unitdata_ind, SRC);
		ADDR (unitdata_ind, OPT);
		break;
#endif
#ifdef T_UDERROR_IND
	    case T_UDERROR_IND:	/* unitdata error ind */
		GET (T_UDERROR_IND, uderror_ind);
		ADDR (uderror_ind, DEST);
		ADDR (uderror_ind, OPT);
		tprintf (", ERROR=%ld", m.uderror_ind.ERROR_type);
		break;
#endif
#ifdef T_OPTMGMT_ACK
	    case T_OPTMGMT_ACK:	/* manage opt ack     */
		GET (T_OPTMGMT_ACK, optmgmt_ack);
		COMMA ();
		tprintf ("MGMT=");
		printflags (transport_user_flags, m.optmgmt_ack.MGMT_flags,
			    "T_???");
		STRUCT (optmgmt_ack, OPT, print_optmgmt);
		break;
#endif
#ifdef T_ORDREL_IND
	case T_ORDREL_IND:	/* orderly rel ind    */
		GET (T_ORDREL_IND, ordrel_ind);
		break;
#endif
#ifdef T_ADDR_REQ
	    case T_ADDR_REQ:	/* address req        */
		GET (T_ADDR_REQ, addr_req);
		break;
#endif
#ifdef T_ADDR_ACK
	    case T_ADDR_ACK:	/* address response   */
		GET (T_ADDR_ACK, addr_ack);
		ADDR (addr_ack, LOCADDR);
		ADDR (addr_ack, REMADDR);
		break;
#endif
	    default:
	    dump:
		c = -1;
		printstr(tcp, addr, len);
		break;
	}

	if (c >= 0) tprintf ("}");

#undef ADDR
#undef COMMA
#undef STRUCT

	return 0;
}


#endif /* TI_BIND */


static int internal_stream_ioctl(struct tcb *tcp, int arg)
{
	struct strioctl si;
	struct ioctlent *iop;
	int in_and_out;
	int timod = 0;
#ifdef SI_GETUDATA
	struct si_udata udata;
#endif /* SI_GETUDATA */

	if (!arg)
		return 0;
	if (umove(tcp, arg, &si) < 0) {
		if (entering(tcp))
			tprintf(", {...}");
		return 1;
	}
	if (entering(tcp)) {
		iop = ioctl_lookup(si.ic_cmd);
		if (iop) {
			tprintf(", {ic_cmd=%s", iop->symbol);
			while ((iop = ioctl_next_match(iop)))
				tprintf(" or %s", iop->symbol);
		} else
			tprintf(", {ic_cmd=%#x", si.ic_cmd);
		if (si.ic_timout == INFTIM)
			tprintf(", ic_timout=INFTIM, ");
		else
			tprintf(" ic_timout=%d, ", si.ic_timout);
	}
	in_and_out = 1;
	switch (si.ic_cmd) {
#ifdef SI_GETUDATA
	case SI_GETUDATA:
		in_and_out = 0;
		break;
#endif /* SI_GETUDATA */
	}
	if (in_and_out) {
		if (entering(tcp))
			tprintf("/* in */ ");
		else
			tprintf(", /* out */ ");
	}
	if (in_and_out || entering(tcp))
		tprintf("ic_len=%d, ic_dp=", si.ic_len);
	switch (si.ic_cmd) {
#ifdef TI_BIND
	case TI_BIND:
		/* in T_BIND_REQ, out T_BIND_ACK */
		++timod;
		if (entering(tcp)) {
			print_transport_message (tcp,
						 T_BIND_REQ,
						 si.ic_dp, si.ic_len);
		}
		else {
			print_transport_message (tcp,
						 T_BIND_ACK,
						 si.ic_dp, si.ic_len);
		}
		break;
#endif /* TI_BIND */
#ifdef TI_UNBIND
	case TI_UNBIND:
		/* in T_UNBIND_REQ, out T_OK_ACK */
		++timod;
		if (entering(tcp)) {
			print_transport_message (tcp,
						 T_UNBIND_REQ,
						 si.ic_dp, si.ic_len);
		}
		else {
			print_transport_message (tcp,
						 T_OK_ACK,
						 si.ic_dp, si.ic_len);
		}
		break;
#endif /* TI_UNBIND */
#ifdef TI_GETINFO
	case TI_GETINFO:
		/* in T_INFO_REQ, out T_INFO_ACK */
		++timod;
		if (entering(tcp)) {
			print_transport_message (tcp,
						 T_INFO_REQ,
						 si.ic_dp, si.ic_len);
		}
		else {
			print_transport_message (tcp,
						 T_INFO_ACK,
						 si.ic_dp, si.ic_len);
		}
		break;
#endif /* TI_GETINFO */
#ifdef TI_OPTMGMT
	case TI_OPTMGMT:
		/* in T_OPTMGMT_REQ, out T_OPTMGMT_ACK */
		++timod;
		if (entering(tcp)) {
			print_transport_message (tcp,
						 T_OPTMGMT_REQ,
						 si.ic_dp, si.ic_len);
		}
		else {
			print_transport_message (tcp,
						 T_OPTMGMT_ACK,
						 si.ic_dp, si.ic_len);
		}
		break;
#endif /* TI_OPTMGMT */
#ifdef SI_GETUDATA
	case SI_GETUDATA:
		if (entering(tcp))
			break;
		if (umove(tcp, (int) si.ic_dp, &udata) < 0)
			tprintf("{...}");
		else {
			tprintf("{tidusize=%d, addrsize=%d, ",
				udata.tidusize, udata.addrsize);
			tprintf("optsize=%d, etsdusize=%d, ",
				udata.optsize, udata.etsdusize);
			tprintf("servtype=%d, so_state=%d, ",
				udata.servtype, udata.so_state);
			tprintf("so_options=%d", udata.so_options);
			tprintf("}");
		}
		break;
#endif /* SI_GETUDATA */
	default:
		printstr(tcp, (long) si.ic_dp, si.ic_len);
		break;
	}
	if (exiting(tcp)) {
		tprintf("}");
		if (timod && tcp->u_rval && !syserror(tcp)) {
			tcp->auxstr = xlookup (tli_errors, tcp->u_rval);
			return RVAL_STR + 1;
		}
	}

	return 1;
}

int
stream_ioctl(tcp, code, arg)
struct tcb *tcp;
int code, arg;
{
#ifdef I_LIST
	int i;
#endif
	int val;
#ifdef I_FLUSHBAND
	struct bandinfo bi;
#endif
	struct strpeek sp;
	struct strfdinsert sfi;
	struct strrecvfd srf;
#ifdef I_LIST
	struct str_list sl;
#endif

	/* I_STR is a special case because the data is read & written. */
	if (code == I_STR)
		return internal_stream_ioctl(tcp, arg);
	if (entering(tcp))
		return 0;

	switch (code) {
	case I_PUSH:
	case I_LOOK:
	case I_FIND:
		/* arg is a string */
		tprintf(", ");
		printpath(tcp, arg);
		return 1;
	case I_POP:
		/* doesn't take an argument */
		return 1;
	case I_FLUSH:
		/* argument is an option */
		tprintf(", ");
		printxval(stream_flush_options, arg, "FLUSH???");
		return 1;
#ifdef I_FLUSHBAND
	case I_FLUSHBAND:
		/* argument is a pointer to a bandinfo struct */
		if (umove(tcp, arg, &bi) < 0)
			tprintf(", {...}");
		else {
			tprintf(", {bi_pri=%d, bi_flag=", bi.bi_pri);
			printflags(stream_flush_options, bi.bi_flag, "FLUSH???");
			tprintf("}");
		}
		return 1;
#endif /* I_FLUSHBAND */
	case I_SETSIG:
		/* argument is a set of flags */
		tprintf(", ");
		printflags(stream_setsig_flags, arg, "S_???");
		return 1;
	case I_GETSIG:
		/* argument is a pointer to a set of flags */
		if (syserror(tcp))
			return 0;
		tprintf(", [");
		if (umove(tcp, arg, &val) < 0)
			tprintf("?");
		else
			printflags(stream_setsig_flags, val, "S_???");
		tprintf("]");
		return 1;
	case I_PEEK:
		/* argument is a pointer to a strpeek structure */
		if (syserror(tcp) || !arg)
			return 0;
		if (umove(tcp, arg, &sp) < 0) {
			tprintf(", {...}");
			return 1;
		}
		tprintf(", {ctlbuf=");
		printstrbuf(tcp, &sp.ctlbuf, 1);
		tprintf(", databuf=");
		printstrbuf(tcp, &sp.databuf, 1);
		tprintf(", flags=");
		printflags(msgflags, sp.flags, "RS_???");
		tprintf("}");
		return 1;
	case I_SRDOPT:
		/* argument is an option with flags */
		tprintf(", ");
		printxval(stream_read_options, arg & RMODEMASK, "R???");
		addflags(stream_read_flags, arg & ~RMODEMASK);
		return 1;
	case I_GRDOPT:
		/* argument is an pointer to an option with flags */
		if (syserror(tcp))
			return 0;
		tprintf(", [");
		if (umove(tcp, arg, &val) < 0)
			tprintf("?");
		else {
			printxval(stream_read_options,
				  arg & RMODEMASK, "R???");
			addflags(stream_read_flags, arg & ~RMODEMASK);
		}
		tprintf("]");
		return 1;
	case I_NREAD:
#ifdef I_GETBAND
	case I_GETBAND:
#endif
#ifdef I_SETCLTIME
	case I_SETCLTIME:
#endif
#ifdef I_GETCLTIME
	case I_GETCLTIME:
#endif
		/* argument is a pointer to a decimal integer */
		if (syserror(tcp))
			return 0;
		tprintf(", ");
		printnum(tcp, arg, "%d");
		return 1;
	case I_FDINSERT:
		/* argument is a pointer to a strfdinsert structure */
		if (syserror(tcp) || !arg)
			return 0;
		if (umove(tcp, arg, &sfi) < 0) {
			tprintf(", {...}");
			return 1;
		}
		tprintf(", {ctlbuf=");
		printstrbuf(tcp, &sfi.ctlbuf, 1);
		tprintf(", databuf=");
		printstrbuf(tcp, &sfi.databuf, 1);
		tprintf(", flags=");
		printflags(msgflags, sfi.flags, "RS_???");
		tprintf(", filedes=%d, offset=%d}", sfi.fildes, sfi.offset);
		return 1;
#ifdef I_SWROPT
	case I_SWROPT:
		/* argument is a set of flags */
		tprintf(", ");
		printflags(stream_write_flags, arg, "SND???");
		return 1;
#endif /* I_SWROPT */
#ifdef I_GWROPT
	case I_GWROPT:
		/* argument is an pointer to an option with flags */
		if (syserror(tcp))
			return 0;
		tprintf(", [");
		if (umove(tcp, arg, &val) < 0)
			tprintf("?");
		else
			printflags(stream_write_flags, arg, "SND???");
		tprintf("]");
		return 1;
#endif /* I_GWROPT */
	case I_SENDFD:
#ifdef I_CKBAND
	case I_CKBAND:
#endif
#ifdef I_CANPUT
	case I_CANPUT:
#endif
	case I_LINK:
	case I_UNLINK:
	case I_PLINK:
	case I_PUNLINK:
		/* argument is a decimal integer */
		tprintf(", %d", arg);
		return 1;
	case I_RECVFD:
		/* argument is a pointer to a strrecvfd structure */
		if (syserror(tcp) || !arg)
			return 0;
		if (umove(tcp, arg, &srf) < 0) {
			tprintf(", {...}");
			return 1;
		}
		tprintf(", {fd=%d, uid=%lu, gid=%lu}", srf.fd,
			(unsigned long) srf.uid, (unsigned long) srf.gid);
		return 1;
#ifdef I_LIST
	case I_LIST:
		if (syserror(tcp))
			return 0;
		if (arg == 0) {
			tprintf(", NULL");
			return 1;
		}
		if (umove(tcp, arg, &sl) < 0) {
			tprintf(", {...}");
			return 1;
		}
		tprintf(", {sl_nmods=%d, sl_modlist=[", sl.sl_nmods);
		for (i = 0; i < tcp->u_rval; i++) {
			if (i)
				tprintf(", ");
			printpath(tcp, (int) sl.sl_modlist[i].l_name);
		}
		tprintf("]}");
		return 1;
#endif /* I_LIST */
#ifdef I_ATMARK
	case I_ATMARK:
		tprintf(", ");
		printxval(stream_atmark_options, arg, "???MARK");
		return 1;
#endif /* I_ATMARK */
	default:
		return 0;
	}
}

#endif /* !LINUX && !FREEBSD */

#endif /* HAVE_SYS_STREAM_H || LINUX || FREEBSD */
