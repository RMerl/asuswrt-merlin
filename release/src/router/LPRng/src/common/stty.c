/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: stty.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


#include "lp.h"
#include "stty.h"
/**** ENDINCLUDE ****/

#if USE_STTY == TERMIO
# ifdef HAVE_TERMIO_H
#    include <termio.h>
#  endif
#  ifdef HAVE_SYS_TERMIO_H
#    include <sys/termio.h>
#  endif
#endif

#if USE_STTY == SGTTYB
# include <sgtty.h>
#endif

#if USE_STTY == TERMIOS
# include <termios.h>
# ifdef USE_TERMIOX
#  if defined(_SCO_DS) && !defined(_SVID3)
#   define _SVID3 1   /* needed to enable #defines in SCO v5 termiox.h */
#  endif
#  include <sys/termiox.h>
# endif
#endif


#if !defined(TIOCEXCL) && defined(HAVE_SYS_TTOLD_H) && !defined(IRIX)
#  include <sys/ttold.h>
#endif

#if USE_STTY == SGTTYB

# if !defined(B19200)
#  ifdef EXTA
#   define B19200 EXTA
#  else
#   define B19200 B9600
#  endif  /* !defined(EXTA) */
# endif  /* !defined(B19200) */

#if !defined(B38400)
#ifdef EXTB
#define B38400 EXTB
#else
#define B38400 B9600
#endif  /* !defined(EXTB) */
#endif  /* !defined(B38400) */

 static struct bauds {
    char *string;
    int baud;
    int speed;
} bauds[] = {

    { "110", 110, B110, },
    { "134", 134, B134, },
    { "150", 150, B150, },
    { "300", 300, B300, },
    { "600", 600, B600, },
    { "1200", 1200, B1200, },
    { "1800", 1800, B1800, },
    { "2400", 2400, B2400, },
    { "4800", 4800, B4800, },
    { "9600", 9600, B9600, },
    { "19200", 19200, B19200, },
    { "38400", 38400, B38400, },
#if defined(B57600)
    { "57600", 57600, B57600, },
#endif
#if defined(B115200)
    { "115200", 115200, B115200, },
#endif
#if defined(B230400)
    { "230400", 230400, B230400, },
#endif
#if defined(B460800)
    { "460800", 460800, B460800, },
#endif
#if defined(B500000)
    { "500000", 500000, B500000, },
#endif
#if defined(B576000)
    { "576000", 576000, B576000, },
#endif
#if defined(B921600)
    { "921600", 921600, B921600, },
#endif
    { (char *) 0, 0, 0 }
};

/*
 * Set terminal modes.
 * 
 * jmason: I've added support for some combination modes,
 *  such as "pass8" and "parity".
 */

 struct tchars termctrl;
 struct ltchars linectrl;
 struct sgttyb mode;

 static struct {
    char *string;
    int set;
    int reset;
    int lset;
    int lreset;

} modes[] = {
    { "bs0", BS0, BS1, 0, 0 },
    { "bs1", BS1, BS1, 0, 0 },
    { "cbreak", CBREAK, 0, 0, 0 },
    { "-cbreak", 0, CBREAK, 0, 0 },
    { "cooked", 0, RAW, 0, 0 },
    { "cr0", CR0, CR3, 0, 0 },
    { "cr1", CR1, CR3, 0, 0 },
    { "cr2", CR2, CR3, 0, 0 },
    { "cr3", CR3, CR3, 0, 0 },
    { "decctlq", 0, 0, LDECCTQ, 0 },
    { "-decctlq", 0, 0, 0, LDECCTQ, },
    { "echo", ECHO, 0, 0, 0 },
    { "-echo", 0, ECHO, 0, 0 },
    { "even", EVENP, 0, 0, 0 },
    { "-even", 0, EVENP, 0, 0 },
    { "ff0", FF0, FF1, 0, 0 },
    { "ff1", FF1, FF1, 0, 0 },
    { "lcase", LCASE, 0, 0, 0 },
    { "-lcase", 0, LCASE, 0, 0 },
    { "litout", 0, 0, LLITOUT, 0 },
    { "-litout", 0, 0, 0, LLITOUT, },
    { "nl", 0, CRMOD, 0, 0 },
    { "-nl", CRMOD, 0, 0, 0 },
    { "nl0", NL0, NL3, 0, 0 },
    { "nl1", NL1, NL3, 0, 0 },
    { "nl2", NL2, NL3, 0, 0 },
    { "nl3", NL3, NL3, 0, 0 },
    { "noflsh", 0, 0, LNOFLSH, 0 },
    { "-noflsh", 0, 0, 0, LNOFLSH, },
    { "nohang", 0, 0, LNOHANG, 0 },
    { "-nohang", 0, 0, 0, LNOHANG, },
    { "odd", ODDP, 0, 0, 0 },
    { "-odd", 0, ODDP, 0, 0 },
    { "raw", RAW, 0, 0, 0 },
    { "-raw", 0, RAW, 0, 0 },
    { "tab0", TAB0, XTABS, 0, 0 },
    { "tab1", TAB1, XTABS, 0, 0 },
    { "tab2", TAB2, XTABS, 0, 0 },
    { "tabs", 0, XTABS, 0, 0 },
    { "-tabs", XTABS, 0, 0, 0 },
    { "tandem", TANDEM, 0, 0, 0 },
    { "-tandem", 0, TANDEM, 0, 0 },
#ifndef IS_NEXT
    { "tilde", 0, 0, LTILDE, 0 },
    { "-tilde", 0, 0, 0, LTILDE, },
#endif
    { "tn300", CR1, ALLDELAY, 0, 0 },
    { "tty33", CR1, ALLDELAY, 0, 0 },
    { "tty37", FF1 + CR2 + TAB1 + NL1, ALLDELAY, 0, 0 },
    { "vt05", NL2, ALLDELAY, 0, 0 },

    /* jmason modes and synonyms: */
    { "evenp", EVENP, 0, 0, 0 },
    { "-evenp", 0, EVENP, 0, 0 },
    { "parity", EVENP, 0, 0, 0 },
    { "-parity", 0, EVENP|ODDP, 0, 0 },
    { "oddp", ODDP, 0, 0, 0 },
    { "-oddp", 0, ODDP, 0, 0 },
#ifdef LPASS8
    { "pass8", 0, 0, LPASS8, 0 },
    { "-pass8", 0, 0, 0, LPASS8, },
#endif
	{0}
};


 static struct special {
    char *name;
    char *cp;
    char def;
} special[] = {

    { "stop", &termctrl.t_stopc, CSTOP },
    { "start", &termctrl.t_startc, CSTART },
    { 0, 0, 0 }
};

void Do_stty( int fd )
{
	int i, count;
	int localmode;
	int linedisc;
	char *arg, *option;
	struct line_list l;

	Init_line_list(&l);
	Split(&l,Stty_command_DYN,Whitespace,0,0,0,0,0,0);
	Check_max(&l,1);
	l.list[l.count] = 0;
	DEBUG3("Do_stty: using SGTTYB, fd %d", fd );
	if( ioctl( fd, TIOCGETP, &mode) < 0
		|| ioctl( fd, TIOCGETC, &termctrl) < 0
		|| ioctl( fd, TIOCLGET, &localmode) < 0
		|| ioctl( fd, TIOCGLTC, &linectrl) < 0 ){
		LOGERR_DIE(LOG_INFO) "cannot get tty parameters");
	}
	DEBUG2("stty: before mode 0x%x, lmode 0x%x, speed 0x%x",
			 mode.sg_flags, localmode, mode.sg_ispeed);
	if( Baud_rate_DYN ){
		for( i = 0; bauds[i].baud && Baud_rate_DYN != bauds[i].baud; i++);
		if( i == 0) {
			FATAL(LOG_INFO) "illegal baud rate %d", Baud_rate_DYN );
		}
		mode.sg_ispeed = mode.sg_ospeed = bauds[i].speed;
	}

	for( count = 0; count < l.count; ++count ){
		arg = l.list[count];
		for( i = 0; modes[i].string && safestrcasecmp( modes[i].string, arg); i++);
		if( modes[i].string ){
			DEBUG3("stty: modes %s, mc 0x%x ms 0x%x lc 0x%x ls 0x%x",
					 modes[i].string, modes[i].reset, modes[i].set,
					 modes[i].lreset, modes[i].lset);
			mode.sg_flags &= ~modes[i].reset;
			mode.sg_flags |= modes[i].set;
			localmode &= ~modes[i].lreset;
			localmode |= modes[i].lset;
			continue;
		}
		for( i = 0; special[i].name && safestrcasecmp( special[i].name, arg); i++);
		if( special[i].name) {
			++count;
			option = l.list[count];
			if( option == 0) {
				FATAL(LOG_INFO) "stty: missing parameter for %s", arg);
			}
			if( option[0] == '^') {
				if( option[1] == '?') {
					*special[i].cp = 0177;
				} else {
					*special[i].cp = 037 & option[1];
				}
			} else {
				*special[i].cp = option[0];
			}
			DEBUG3("stty: special %s %s", arg, option);
			continue;
		}
		for( i = 0; bauds[i].string && safestrcasecmp( bauds[i].string, arg); i++);
		if( bauds[i].string) {
			DEBUG3("stty: speed %s", arg);
			mode.sg_ispeed = mode.sg_ospeed = bauds[i].speed;
			continue;
		}
		if( !safestrcasecmp( "new", arg)) {
			DEBUG3("stty: ldisc %s", arg);
			linedisc = NTTYDISC;
			if( ioctl( fd, TIOCSETD, &linedisc) < 0)
				LOGERR_DIE(LOG_INFO) "stty: TIOCSETD ioctl failed");
			continue;
		}
		if( !safestrcasecmp( "old", arg)) {
			DEBUG3("stty: ldisc %s", arg);
			linedisc = 0;
			if( ioctl( fd, TIOCSETD, &linedisc) < 0)
				LOGERR_DIE(LOG_INFO) "stty: TIOCSETD ioctl failed");
			continue;
		}
		FATAL(LOG_INFO) "unknown mode: %s\n", arg);
	}
	DEBUG2("stty: after mode 0x%x, lmode 0x%x, speed 0x%x",
			 mode.sg_flags, localmode, mode.sg_ispeed);
	if( ioctl( fd, TIOCSETN, &mode) < 0
		|| ioctl( fd, TIOCSETC, &termctrl) < 0
		|| ioctl( fd, TIOCSLTC, &linectrl) < 0
		|| ioctl( fd, TIOCLSET, &localmode) < 0) {
		LOGERR_DIE(LOG_NOTICE) "cannot set tty parameters");
	}
	Free_line_list(&l);
}
#endif

#if USE_STTY == TERMIO

/* the folks who are responsible for include files
 * must be the lowest paid/motivated ones in
 * OS development... (mj)
 */

#if !defined(TOSTOP) && defined(_TOSTOP)
#define TOSTOP _TOSTOP
#endif

 static struct bauds {
    char *string;
    int baud;
    int speed;
}     bauds[] = {

    { "110", 110, B110, },
    { "134", 134, B134, },
    { "150", 150, B150, },
    { "300", 300, B300, },
    { "600", 600, B600, },
    { "1200", 1200, B1200, },
    { "1800", 1800, B1800, },
    { "2400", 2400, B2400, },
    { "4800", 4800, B4800, },
    { "9600", 9600, B9600, },
    { "19200", 19200, B19200, },
    { "38400", 38400, B38400, },
#if defined(B57600)
    { "57600", 57600, B57600, },
#endif
#if defined(B115200)
    { "115200", 115200, B115200, },
#endif
#if defined(B230400)
    { "230400", 230400, B230400, },
#endif
#if defined(B460800)
    { "460800", 460800, B460800, },
#endif
#if defined(B500000)
    { "500000", 500000, B500000, },
#endif
#if defined(B576000)
    { "576000", 576000, B576000, },
#endif
#if defined(B921600)
    { "921600", 921600, B921600, },
#endif
    { (char *) 0, 0, 0 }
};

 struct termio tio;
 static struct {
    char *string;
    int iset;
    int ireset;
    int oset;
    int oreset;
    int cset;
    int creset;
    int lset;
    int lreset;
}      tmodes[] = {

    /* input modes */
    { "ignbrk", IGNBRK, 0, 0, 0, 0, 0, 0, 0 },
    { "-ignbrk", 0, IGNBRK, 0, 0, 0, 0, 0, 0 },
    { "brkint", BRKINT, 0, 0, 0, 0, 0, 0, 0 },
    { "-brkint", 0, BRKINT, 0, 0, 0, 0, 0, 0 },
    { "ignpar", IGNPAR, 0, 0, 0, 0, 0, 0, 0 },
    { "-ignpar", 0, IGNPAR, 0, 0, 0, 0, 0, 0 },
    { "parmrk", PARMRK, 0, 0, 0, 0, 0, 0, 0 },
    { "-parmrk", 0, PARMRK, 0, 0, 0, 0, 0, 0 },
    { "inpck", INPCK, 0, 0, 0, 0, 0, 0, 0 },
    { "-inpck", 0, INPCK, 0, 0, 0, 0, 0, 0 },
    { "istrip", ISTRIP, 0, 0, 0, 0, 0, 0, 0 },
    { "-istrip", 0, ISTRIP, 0, 0, 0, 0, 0, 0 },
    { "inlcr", INLCR, 0, 0, 0, 0, 0, 0, 0 },
    { "-inlcr", 0, INLCR, 0, 0, 0, 0, 0, 0 },
    { "igncr", IGNCR, 0, 0, 0, 0, 0, 0, 0 },
    { "-igncr", 0, IGNCR, 0, 0, 0, 0, 0, 0 },
    { "icrnl", ICRNL, 0, 0, 0, 0, 0, 0, 0 },
    { "-icrnl", 0, ICRNL, 0, 0, 0, 0, 0, 0 },
    { "lcase", IUCLC, 0, 0, 0, 0, 0, 0, 0 },
    { "iuclc", IUCLC, 0, 0, 0, 0, 0, 0, 0 },
    { "-lcase", 0, IUCLC, 0, 0, 0, 0, 0, 0 },
    { "-iuclc", 0, IUCLC, 0, 0, 0, 0, 0, 0 },
    { "ixon", IXON, 0, 0, 0, 0, 0, 0, 0 },
    { "-ixon", 0, IXON, 0, 0, 0, 0, 0, 0 },
    { "ixany", IXANY, 0, 0, 0, 0, 0, 0, 0 },
    { "-ixany", 0, IXANY, 0, 0, 0, 0, 0, 0 },
    { "ixoff", IXOFF, 0, 0, 0, 0, 0, 0, 0 },
    { "-ixoff", 0, IXOFF, 0, 0, 0, 0, 0, 0 },
    { "decctlq", IXANY, 0, 0, 0, 0, 0, 0, 0 },
    { "-decctlq", 0, IXANY, 0, 0, 0, 0, 0, 0 },
    { "tandem", IXOFF, 0, 0, 0, 0, 0, 0, 0 },
    { "-tandem", 0, IXOFF, 0, 0, 0, 0, 0, 0 },
#ifdef IMAXBEL
    { "imaxbel", IMAXBEL, 0, 0, 0, 0, 0, 0, 0 },
    { "-maxbel", 0, IMAXBEL, 0, 0, 0, 0, 0, 0 },
#endif
    /* output modes */
    { "opost", 0, 0, OPOST, 0, 0, 0, 0, 0 },
    { "-opost", 0, 0, 0, OPOST, 0, 0, 0, 0 },
    { "olcuc", 0, 0, OLCUC, 0, 0, 0, 0, 0 },
    { "-olcuc", 0, 0, 0, OLCUC, 0, 0, 0, 0 },
    { "onlcr", 0, 0, ONLCR, 0, 0, 0, 0, 0 },
    { "-onlcr", 0, 0, 0, ONLCR, 0, 0, 0, 0 },
    { "ocrnl", 0, 0, OCRNL, 0, 0, 0, 0, 0 },
    { "-ocrnl", 0, 0, 0, OCRNL, 0, 0, 0, 0 },
    { "onocr", 0, 0, ONOCR, 0, 0, 0, 0, 0 },
    { "-onocr", 0, 0, 0, ONOCR, 0, 0, 0, 0 },
    { "onlret", 0, 0, ONLRET, 0, 0, 0, 0, 0 },
    { "-onlret", 0, 0, 0, ONLRET, 0, 0, 0, 0 },
    { "ofill", 0, 0, OFILL, 0, 0, 0, 0, 0 },
    { "-ofill", 0, 0, 0, OFILL, 0, 0, 0, 0 },
    { "ofdel", 0, 0, OFDEL, 0, 0, 0, 0, 0 },
    { "-ofdel", 0, 0, 0, OFDEL, 0, 0, 0, 0 },
    { "nl0", 0, 0, NL0, NLDLY, 0, 0, 0, 0 },
    { "nl1", 0, 0, NL1, NLDLY, 0, 0, 0, 0 },
    { "cr0", 0, 0, CR1, CRDLY, 0, 0, 0, 0 },
    { "cr1", 0, 0, CR1, CRDLY, 0, 0, 0, 0 },
    { "cr2", 0, 0, CR2, CRDLY, 0, 0, 0, 0 },
    { "cr3", 0, 0, CR3, CRDLY, 0, 0, 0, 0 },
    { "tab0", 0, 0, TAB0, TABDLY, 0, 0, 0, 0 },
    { "tab1", 0, 0, TAB1, TABDLY, 0, 0, 0, 0 },
    { "tab2", 0, 0, TAB2, TABDLY, 0, 0, 0, 0 },
    { "tab3", 0, 0, TAB3, TABDLY, 0, 0, 0, 0 },
    { "bs0", 0, 0, BS0, BSDLY, 0, 0, 0, 0 },
    { "bs1", 0, 0, BS1, BSDLY, 0, 0, 0, 0 },
    { "vt0", 0, 0, VT0, VTDLY, 0, 0, 0, 0 },
    { "vt1", 0, 0, VT1, VTDLY, 0, 0, 0, 0 },
    { "ff0", 0, 0, FF0, FFDLY, 0, 0, 0, 0 },
    { "ff1", 0, 0, FF1, FFDLY, 0, 0, 0, 0 },
    { "nopost", 0, 0, 0, OPOST, 0, 0, 0, 0 },
    { "-nopost", 0, 0, OPOST, 0, 0, 0, 0, 0 },
    { "fill", 0, 0, OFILL, OFDEL, 0, 0, 0, 0 },
    { "-fill", 0, 0, 0, OFILL | OFDEL, 0, 0, 0, 0 },
    { "nul-fill", 0, 0, OFILL, OFDEL, 0, 0, 0, 0 },
    { "del-fill", 0, 0, OFILL | OFDEL, 0, 0, 0, 0, 0 },
#ifdef XTABS
    { "tabs", 0, 0, 0, XTABS | TABDLY, 0, 0, 0, 0 },
    { "-tabs", 0, 0, XTABS, TABDLY, 0, 0, 0, 0 },
#endif
    /* control modes */
    { "cs5", 0, 0, 0, 0, CS5, CSIZE, 0, 0 },
    { "cs6", 0, 0, 0, 0, CS6, CSIZE, 0, 0 },
    { "cs7", 0, 0, 0, 0, CS7, CSIZE, 0, 0 },
    { "cs8", 0, 0, 0, 0, CS8, CSIZE, 0, 0 },
    { "cstopb", 0, 0, 0, 0, CSTOPB, 0, 0, 0 },
    { "-cstopb", 0, 0, 0, 0, 0, CSTOPB, 0, 0 },
    { "cread", 0, 0, 0, 0, CREAD, 0, 0, 0 },
    { "-cread", 0, 0, 0, 0, 0, CREAD, 0, 0 },
    { "parenb", 0, 0, 0, 0, PARENB, 0, 0, 0 },
    { "-parenb", 0, 0, 0, 0, 0, PARENB, 0, 0 },
    { "parodd", 0, 0, 0, 0, PARODD, 0, 0, 0 },
    { "-parodd", 0, 0, 0, 0, 0, PARODD, 0, 0 },
    { "hupcl", 0, 0, 0, 0, HUPCL, 0, 0, 0 },
    { "-hupcl", 0, 0, 0, 0, 0, HUPCL, 0, 0 },
    { "clocal", 0, 0, 0, 0, CLOCAL, 0, 0, 0 },
    { "-clocal", 0, 0, 0, 0, 0, CLOCAL, 0, 0 },
#ifdef LOBLK
    { "loblk", 0, 0, 0, 0, LOBLK, 0, 0, 0 },
    { "-loblk", 0, 0, 0, 0, 0, LOBLK, 0, 0 },
#endif
    { "parity", 0, 0, 0, 0, PARENB | CS7, PARODD | CSIZE, 0, 0 },
    { "-parity", 0, 0, 0, 0, CS8, PARENB | CSIZE, 0, 0 },
    { "evenp", 0, 0, 0, 0, PARENB | CS7, PARODD | CSIZE, 0, 0 },
    { "-evenp", 0, 0, 0, 0, CS8, PARENB | CSIZE, 0, 0 },
    { "oddp", 0, 0, 0, 0, PARENB | PARODD | CS7, CSIZE, 0, 0 },
    { "-oddp", 0, 0, 0, 0, CS8, PARENB | PARODD | CSIZE, 0, 0 },
    { "stopb", 0, 0, 0, 0, CSTOPB, 0, 0, 0 },
    { "-stopb", 0, 0, 0, 0, 0, CSTOPB, 0, 0 },
    { "hup", 0, 0, 0, 0, HUPCL, 0, 0, 0 },
    { "-hup", 0, 0, 0, 0, 0, HUPCL, 0, 0 },
#ifdef CRTSCTS
    { "crtscts", 0, 0, 0, 0, CRTSCTS, 0, 0, 0 },
    { "-crtscts", 0, 0, 0, 0, 0, CRTSCTS, 0, 0 },
#endif
    /* local modes */
    { "isig", 0, 0, 0, 0, 0, 0, ISIG, 0 },
    { "-isig", 0, 0, 0, 0, 0, 0, 0, ISIG },
    { "noisig", 0, 0, 0, 0, 0, 0, 0, ISIG },
    { "-noisig", 0, 0, 0, 0, 0, 0, ISIG, 0 },
    { "icanon", 0, 0, 0, 0, 0, 0, ICANON, 0 },
    { "-icanon", 0, 0, 0, 0, 0, 0, 0, ICANON },
    { "cbreak", 0, 0, 0, 0, 0, 0, ICANON, 0 },
    { "-cbreak", 0, 0, 0, 0, 0, 0, 0, ICANON },
    { "xcase", 0, 0, 0, 0, 0, 0, XCASE, 0 },
    { "-xcase", 0, 0, 0, 0, 0, 0, 0, XCASE },
    { "echo", 0, 0, 0, 0, 0, 0, ECHO, 0 },
    { "-echo", 0, 0, 0, 0, 0, 0, 0, ECHO },
    { "echoe", 0, 0, 0, 0, 0, 0, ECHOE, 0 },
    { "-echoe", 0, 0, 0, 0, 0, 0, 0, ECHOE },
    { "crterase", 0, 0, 0, 0, 0, 0, ECHOE, 0 },
    { "-crterase", 0, 0, 0, 0, 0, 0, 0, ECHOE },
    { "echok", 0, 0, 0, 0, 0, 0, ECHOK, 0 },
    { "-echok", 0, 0, 0, 0, 0, 0, 0, ECHOK },
    { "lfkc", 0, 0, 0, 0, 0, 0, ECHOK, 0 },
    { "-lfkc", 0, 0, 0, 0, 0, 0, 0, ECHOK },
    { "echonl", 0, 0, 0, 0, 0, 0, ECHONL, 0 },
    { "-echonl", 0, 0, 0, 0, 0, 0, 0, ECHONL },
    { "noflsh", 0, 0, 0, 0, 0, 0, NOFLSH, 0 },
    { "-noflsh", 0, 0, 0, 0, 0, 0, 0, NOFLSH },
    { "tostop", 0, 0, 0, 0, 0, 0, TOSTOP, 0 },
    { "-tostop", 0, 0, 0, 0, 0, 0, 0, TOSTOP },
#ifdef ECHOCTL
    { "echoctl", 0, 0, 0, 0, 0, 0, ECHOCTL, 0 },
    { "-echoctl", 0, 0, 0, 0, 0, 0, 0, ECHOCTL },
    { "ctlecho", 0, 0, 0, 0, 0, 0, ECHOCTL, 0 },
    { "-ctlecho", 0, 0, 0, 0, 0, 0, 0, ECHOCTL },
#endif
#ifdef ECHOPRT
    { "echoprt", 0, 0, 0, 0, 0, 0, ECHOPRT, 0 },
    { "-echoprt", 0, 0, 0, 0, 0, 0, 0, ECHOPRT },
    { "prterase", 0, 0, 0, 0, 0, 0, ECHOPRT, 0 },
    { "-prterase", 0, 0, 0, 0, 0, 0, 0, ECHOPRT },
#endif
#ifdef ECHOKE
    { "echoke", 0, 0, 0, 0, 0, 0, ECHOKE, 0 },
    { "-echoke", 0, 0, 0, 0, 0, 0, 0, ECHOKE },
    { "crtkill", 0, 0, 0, 0, 0, 0, ECHOKE, 0 },
    { "-crtkill", 0, 0, 0, 0, 0, 0, 0, ECHOKE },
#endif
    /* convenience modes */
    { "lcase", IUCLC, 0, OLCUC, 0, 0, 0, XCASE, 0 },
    { "-lcase", 0, IUCLC, 0, OLCUC, 0, 0, 0, XCASE },
    { "nl", 0, ICRNL, 0, ONLCR, 0, 0, 0, 0 },
    { "-nl", ICRNL, INLCR | IGNCR, ONLCR, OCRNL | ONLRET, 0, 0, 0, 0 },
    { "litout", 0, 0, OPOST, 0, CS8, CSIZE | PARENB, 0, 0 },
    { "-litout", 0, 0, 0, OPOST, CS7 | PARENB, CSIZE, 0, 0 },
    { "pass8", 0, ISTRIP, 0, 0, CS8, CSIZE | PARENB, 0, 0 },
    { "-pass8", ISTRIP, 0, 0, 0, CS7 | PARENB, CSIZE, 0, 0 },
    { "raw", 0, -1, 0, OPOST, CS8, CSIZE | PARENB, 0, ISIG | ICANON | XCASE,
    },
#ifdef IMAXBEL
    { "-raw", BRKINT | IGNPAR | ISTRIP | ICRNL | IXON | IMAXBEL,
                0, OPOST, 0, CS7 | PARENB, CSIZE, ISIG | ICANON, 0 },
    { "sane", BRKINT | IGNPAR | ISTRIP | ICRNL | IXON | IMAXBEL,
                IGNBRK | PARMRK | INPCK | INLCR | IGNCR | IUCLC | IXOFF,
                OPOST | ONLCR,
                OLCUC | OCRNL | ONOCR | ONLRET | OFILL | OFDEL | NLDLY | CRDLY|
                TABDLY | BSDLY | VTDLY | FFDLY,
                CS7 | PARENB | CREAD, CSIZE | PARODD | CLOCAL,
                ISIG | ICANON | ECHO | ECHOK, XCASE | ECHOE | ECHONL | NOFLSH},
#endif                          /* IMAXBEL */
    { "cooked", BRKINT | IGNPAR | ISTRIP | ICRNL | IXON, 0, OPOST, 0,
                CS7 | PARENB, CSIZE, ISIG | ICANON, 0 },
    {0},
};

void Do_stty( int fd )
{
	int i, count;
	char *arg;
	struct line_list l;

	Init_line_list(&l);
	Split(&l,Stty_command_DYN,Whitespace,0,0,0,0,0,0);
	Check_max(&l,1);
	l.list[l.count] = 0;

	DEBUG3("Do_stty: using TERMIO, fd %d", fd );
	if( ioctl( fd, TCGETA, &tio) < 0) {
		LOGERR_DIE(LOG_INFO) "cannot get tty parameters");
	}
	DEBUG2("stty: before imode 0x%x, omode 0x%x, cmode 0x%x, lmode 0x%x",
			 tio.c_iflag, tio.c_oflag, tio.c_cflag, tio.c_lflag);

	if( Baud_rate_DYN ){
		for( i = 0; bauds[i].baud && Baud_rate_DYN != bauds[i].baud; i++);
		if( i == 0) {
			FATAL(LOG_INFO) "illegal baud rate %d", Baud_rate_DYN);
		}
		tio.c_cflag &= ~CBAUD;
		tio.c_cflag |= bauds[i].speed;
	}
	for( count = 0; count < l.count; ++count ){
		arg = l.list[count];
		for( i = 0;
			tmodes[i].string && safestrcasecmp( tmodes[i].string, arg); i++);

		if( tmodes[i].string) {
			DEBUG3("stty: modes %s, ic 0x%x is 0x%x oc 0x%x os 0x%x cc 0x%x cs 0x%x lc 0x%x ls 0x%x",
					 tmodes[i].string, tmodes[i].ireset, tmodes[i].iset,
					 tmodes[i].oreset, tmodes[i].oset, tmodes[i].creset, tmodes[i].cset,
					 tmodes[i].lreset, tmodes[i].lset);

			tio.c_iflag &= ~tmodes[i].ireset;
			tio.c_iflag |= tmodes[i].iset;
			tio.c_oflag &= ~tmodes[i].oreset;
			tio.c_oflag |= tmodes[i].oset;
			tio.c_cflag &= ~tmodes[i].creset;
			tio.c_cflag |= tmodes[i].cset;
			tio.c_lflag &= ~tmodes[i].lreset;
			tio.c_lflag |= tmodes[i].lset;
			continue;
		}
		for( i = 0; bauds[i].string && safestrcasecmp( bauds[i].string, arg); i++);
		if( bauds[i].string) {
			DEBUG3("stty: speed %s", arg);
			tio.c_cflag &= ~CBAUD;
			tio.c_cflag |= bauds[i].speed;
			continue;
		}
		FATAL(LOG_INFO) "unknown mode: %s\n", arg);
	}

	if( Read_write_DYN && (tio.c_cflag & ICANON) == 0) {
		/* VMIN & VTIME: suggested by Michael Joosten
		 * only do this if ICANON is off -- Martin Forssen
		 */
		DEBUG2("setting port to read/write with unbuffered reads");
		tio.c_cc[VMIN] = 1;
		tio.c_cc[VTIME] = 0;
	}
	DEBUG2("stty: before imode 0x%x, omode 0x%x, cmode 0x%x, lmode 0x%x",
			 tio.c_iflag, tio.c_oflag, tio.c_cflag, tio.c_lflag);
	if( ioctl( fd, TCSETA, &tio) < 0) {
		LOGERR_DIE(LOG_NOTICE) "cannot set tty parameters");
	}
	Free_line_list(&l);
}
#endif

#if USE_STTY == TERMIOS

#ifndef HAVE_TCSETATTR
#define tcgetattr(fd,tdat)		(ioctl( (fd), TCGETS, (tdat)))
#define tcsetattr(fd,flags,tdat)	(ioctl( (fd), TCSETS, (tdat)))
					/* ignore the flags arg. */
#endif

 static struct bauds {
	char *string;
	int baud;
	int speed;
}     bauds[] = {

	{ "B50", 50, B50, }, { "50", 50, B50, },
	{ "B75", 75, B75, }, { "75", 75, B75, },
	{ "B110", 110, B110, }, { "110", 110, B110, },
	{ "B134", 134, B134, }, { "134", 134, B134, },
	{ "B150", 150, B150, }, { "150", 150, B150, },
	{ "B300", 300, B300, }, { "300", 300, B300, },
	{ "B600", 600, B600, }, { "600", 600, B600, },
	{ "B1200", 1200, B1200, }, { "1200", 1200, B1200, },
	{ "B1800", 1800, B1800, }, { "1800", 1800, B1800, },
	{ "B2400", 2400, B2400, }, { "2400", 2400, B2400, },
	{ "B4800", 4800, B4800, }, { "4800", 4800, B4800, },
	{ "B9600", 9600, B9600, }, { "9600", 9600, B9600, },
	{ "B19200", 19200, B19200, }, { "19200", 19200, B19200, },
	{ "B38400", 38400, B38400, }, { "38400", 38400, B38400, },
#ifdef EXTA
	{"EXTA", EXTA, EXTA},
	{"EXTB", EXTB, EXTB},
#endif
#if defined(B57600)
    { "57600", 57600, B57600, }, { "B57600", 57600, B57600, },
#endif
#if defined(B115200)
    { "115200", 115200, B115200, }, { "B115200", 115200, B115200, },
#endif
#if defined(B230400)
    { "230400", 230400, B230400, }, { "B230400", 230400, B230400, },
#endif
#if defined(B460800)
    { "460800", 460800, B460800, }, { "B460800", 460800, B460800, },
#endif
#if defined(B500000)
    { "500000", 500000, B500000, }, { "B500000", 500000, B500000, },
#endif
#if defined(B576000)
    { "576000", 576000, B576000, }, { "B576000", 576000, B576000, },
#endif
#if defined(B921600)
    { "921600", 921600, B921600, }, { "B921600", 921600, B921600, },
#endif
	{ (char *) 0, 0, 0 }
};

 struct s_term_dat {
	char *name;
	uint or_dat;
	uint and_dat;
};

#undef FLAGS
#ifndef _UNPROTO_
#define FLAGS(X) { #X, X , 0 }, { "-" #X, 0, X }
#else
#define __string(X) "X"
#define FLAGS(X) { __string(X), X , 0 }, { "-" __string(X), 0, X }
#endif
 


/* c_iflag bits */
 static struct s_term_dat c_i_dat[] =
{
	FLAGS(IGNBRK),
	FLAGS(BRKINT),
	FLAGS(IGNPAR),
	FLAGS(PARMRK),
	FLAGS(INPCK),
	FLAGS(ISTRIP),
	FLAGS(INLCR),
	FLAGS(IGNCR),
	FLAGS(ICRNL),
#ifdef IUCLC
	FLAGS(IUCLC),
 	{"lcase", IUCLC, 0}, {"-lcase", 0, IUCLC },
#endif
	FLAGS(IXON),
	FLAGS(IXANY),
	FLAGS(IXOFF),

#ifdef IMAXBEL
	FLAGS(IMAXBEL),
#else
	/* work out what they do and simulate with IFLAGONE|IFLAGTWO */
#endif

	/* jmason addition: */
 	{"PASS8", 0, ISTRIP}, {"-PASS8", ISTRIP, 0},
	{0, 0, 0}
};

/* c_oflag bits */
 static struct s_term_dat c_o_dat[] =
{
	FLAGS(OPOST),
#ifdef OLCUC
	FLAGS(OLCUC),
#endif
#ifdef ONLCR
	FLAGS(ONLCR),
 	{"crmod", ONLCR, 0}, {"-crmod", 0, ONLCR},
#endif
#ifdef OCRNL
	FLAGS(OCRNL),
#endif
#ifdef ONOCR
	FLAGS(ONOCR),
#endif
#ifdef ONLRET
	FLAGS(ONLRET),
#endif
#ifdef OFILL
	FLAGS(OFILL),
#endif
#ifdef OFDEL
	FLAGS(OFDEL),
#endif
#ifdef NLDLY
	{"NL0", 0, NLDLY},
	{"NL1", NL1, NLDLY},
#endif
#ifdef CRDLY
	{"CR0", CR0, CRDLY},
	{"CR1", CR1, CRDLY},
	{"CR2", CR2, CRDLY},
	{"CR3", CR3, CRDLY},
#endif
#ifdef TABDLY
	{"TAB0", TAB0, TABDLY},
	{"TAB1", TAB1, TABDLY},
	{"TAB2", TAB2, TABDLY},
	{"TAB3", TAB3, TABDLY},
#endif
#ifdef BSDLY
	{"BS0", BS0, BSDLY},
	{"BS1", BS1, BSDLY},
#endif
#ifdef VTDLY
	{"VT0", VT0, VTDLY},
	{"VT1", VT1, VTDLY},
#endif
#ifdef FFDLY
	{"FF0", FF0, FFDLY},
	{"FF1", FF1, FFDLY},
#endif

	/* jmason addition: */
#ifdef TABDLY
	{"TABS", TAB3, TABDLY},
#endif
	{0, 0, 0}
};

/* c_cflag bit meaning */
 static struct s_term_dat c_c_dat[] =
{
	{"CS5", CS5, CSIZE},
	{"CS6", CS6, CSIZE},
	{"CS7", CS7, CSIZE},
	{"CS8", CS8, CSIZE},
	{"CSTOPB", CSTOPB, 0},
	{"-CSTOPB", 0, CSTOPB},
	{"CREAD", CREAD, 0},
	{"-CREAD", 0, CREAD},
	{"PARENB", PARENB, 0},
	{"-PARENB", 0, PARENB},
	{"PARODD", PARODD, 0},
	{"-PARODD", 0, PARODD},
	{"HUPCL", HUPCL, 0},
	{"-HUPCL", 0, HUPCL},
	{"CLOCAL", CLOCAL, 0},
	{"-CLOCAL", 0, CLOCAL},

#ifdef CRTSCTS
	/* work out what they do and simulate with IFLAGONE|IFLAGTWO */
	{"CRTSCTS", CRTSCTS, 0},
	{"-CRTSCTS", 0, CRTSCTS},
#endif

	/* jmason additions, from SunOS stty(1v) combination modes: */
	{"even", PARENB | CS7, PARODD | CSIZE},
	{"-even", CS8, PARENB | CSIZE},
	{"EVENP", PARENB | CS7, PARODD | CSIZE},
	{"-EVENP", CS8, PARENB | CSIZE},
	{"PARITY", PARENB | CS7, PARODD | CSIZE},
	{"-PARITY", CS8, PARENB | CSIZE},
	{"odd", PARODD| PARENB | CS7, CSIZE},
	{"-odd", CS8, PARENB | PARODD | CSIZE},
	{"ODDP", PARODD | PARENB | CS7, CSIZE},
	{"-ODDP", CS8, PARENB | PARODD | CSIZE},
	{"PASS8", CS8, PARENB | CSIZE},
	{"-PASS8", PARENB | CS7, CSIZE},
	{0, 0, 0}
};

/* c_lflag bits */
 static struct s_term_dat c_l_dat[] =
{
	{"ISIG", ISIG, 0},
	{"-ISIG", 0, ISIG},
	{"cooked", ICANON, 0},
	{"-raw", ICANON, 0},
	{"cbreak", ICANON, 0},
	{"ICANON", ICANON, 0},
	{"-ICANON", 0, ICANON},
	{"raw", 0, ICANON},
	{"-cooked", 0, ICANON},
#ifdef XCASE
	{"XCASE", XCASE, 0},
	{"-XCASE", 0, XCASE},
#endif
	{"ECHO", ECHO, 0},
	{"-ECHO", 0, ECHO},
	{"ECHOE", ECHOE, 0},
	{"-ECHOE", 0, ECHOE},
	{"ECHOK", ECHOK, 0},
	{"-ECHOK", 0, ECHOK},
	{"ECHONL", ECHONL, 0},
	{"-ECHONL", 0, ECHONL},
	{"NOFLSH", NOFLSH, 0},
	{"-NOFLSH", 0, NOFLSH},
	{"TOSTOP", TOSTOP, 0},
	{"-TOSTOP", 0, TOSTOP},
	{"IEXTEN", IEXTEN, 0},
	{"-IEXTEN", 0, IEXTEN},

#ifdef ECHOCTL
	{"ECHOCTL", ECHOCTL, 0},
	{"-ECHOCTL", 0, ECHOCTL},
	{"CTLECHO", ECHOCTL, 0},
	{"-CTLECHO", 0, ECHOCTL},
#else
	/* work out what they do and simulate with IFLAGONE|IFLAGTWO */
#endif
#ifdef ECHOPRT
	{"ECHOPRT", ECHOPRT, 0},
	{"-ECHOPRT", 0, ECHOPRT},
	{"PRTERASE", ECHOPRT, 0},
	{"-PRTERASE", 0, ECHOPRT},
	/* ditto etc. -- you get the story */
#endif
#ifdef ECHOKE
	{"ECHOKE", ECHOKE, 0},
	{"-ECHOKE", 0, ECHOKE},
	{"CRTKILL", ECHOKE, 0},
	{"-CRTKILL", 0, ECHOKE},
#endif
#ifdef FLUSHO
	{"FLUSHO", FLUSHO, 0},
	{"-FLUSHO", 0, FLUSHO},
#endif
#ifdef PENDIN
	{"PENDIN", PENDIN, 0},
	{"-PENDIN", 0, PENDIN},
#endif
	{ 0 ,0 ,0 }
};

#ifdef USE_TERMIOX
/* termiox bits */
 static struct s_term_dat tx_x_dat[] =
{
	{"RTSXOFF", RTSXOFF, 0},
	{"-RTSXOFF", 0, RTSXOFF},
	{"CTSXON", CTSXON, 0},
	{"-CTSXON", 0, CTSXON}
};

 struct termiox tx_dat;
#endif /* USE_TERMIOX */

 struct termios t_dat;

 static struct special {
	char *name;
	char *cp;
}       special[] = {

	{ "stop", (char *)&(t_dat.c_cc[VSTOP]) },
	{ "start", (char *)&(t_dat.c_cc[VSTART]) },
	{ 0, 0 }
};

void Do_stty( int fd )
{
#ifdef USE_TERMIOX
	int termiox_fail = 0;
#endif
	int i,count;
	char *arg, *option;
	struct line_list l;

	Init_line_list(&l);
	Split(&l,Stty_command_DYN,Whitespace,0,0,0,0,0,0);
	Check_max(&l,1);
	l.list[l.count] = 0;

	DEBUG3("Do_stty: using TERMIOS, fd %d", fd );
	if( tcgetattr( fd, &t_dat) < 0 ){
		LOGERR_DIE(LOG_INFO) "cannot get tty parameters");
	}
#ifdef USE_TERMIOX
	if( ioctl( fd, TCGETX, &tx_dat) < 0 ){
		DEBUG1("stty: TCGETX failed");
		termiox_fail = 1;
	}
#endif
	DEBUG2("stty: before iflag 0x%x, oflag 0x%x, cflag 0x%x lflag 0x%x",
			 (int)t_dat.c_iflag, (int)t_dat.c_oflag,
			(int)t_dat.c_cflag, (int)t_dat.c_lflag);
	if( Baud_rate_DYN ){
		for( i = 0; bauds[i].baud && Baud_rate_DYN != bauds[i].baud; i++);
		if( i == 0 ){
			FATAL(LOG_INFO) "illegal baud rate %d", Baud_rate_DYN );
		}
		DEBUG2("stty: before baudrate : cflag 0x%x",(int)t_dat.c_cflag);

#ifdef HAVE_CFSETISPEED
		DEBUG2("Do_stty: using cfsetispeed/cfsetospeed");
		/* POSIX baudrate manipulation */
		cfsetispeed( &t_dat, bauds[i].speed);
		cfsetospeed( &t_dat, bauds[i].speed);
#else
		DEBUG2("Do_stty: setting tdat.c_cflag");
		t_dat.c_cflag &= ~CBAUD;
		t_dat.c_cflag |= bauds[i].speed;
#endif

		DEBUG2("stty: after baudrate : cflag 0x%x",(int)t_dat.c_cflag);
	}

	for( count = 0; count < l.count; ++count ){
		arg = l.list[count];
		for( i = 0; bauds[i].string && safestrcasecmp( bauds[i].string, arg); i++);

		if( bauds[i].string ){
#ifdef HAVE_CFSETISPEED
			DEBUG2("Do_stty: using cfsetispeed/cfsetospeed");
			/* POSIX baudrate manipulation */
			cfsetispeed( &t_dat, bauds[i].speed);
			cfsetospeed( &t_dat, bauds[i].speed);
#else
			DEBUG2("Do_stty: setting tdat.c_cflag");
			t_dat.c_cflag &= ~CBAUD;
			t_dat.c_cflag |= bauds[i].speed;
#endif
			continue;
		}
		for( i = 0; c_i_dat[i].name && safestrcasecmp( c_i_dat[i].name, arg); i++);
		if( c_i_dat[i].name ){
			DEBUG3("stty: c_iflag %s, ms 0x%x mc 0x%x",
					 c_i_dat[i].name, c_i_dat[i].or_dat, c_i_dat[i].and_dat);
			t_dat.c_iflag &= ~(c_i_dat[i].and_dat);
			t_dat.c_iflag |= c_i_dat[i].or_dat;
			continue;
		}
		for( i = 0; c_o_dat[i].name && safestrcasecmp( c_o_dat[i].name, arg); i++);
		if( c_o_dat[i].name ){
			DEBUG3("stty: c_oflag %s, ms 0x%x mc 0x%x",
					 c_o_dat[i].name, c_o_dat[i].or_dat, c_o_dat[i].and_dat);
			t_dat.c_oflag &= ~(c_o_dat[i].and_dat);
			t_dat.c_oflag |= c_o_dat[i].or_dat;
			continue;
		}
		for( i = 0; c_c_dat[i].name && safestrcasecmp( c_c_dat[i].name, arg); i++);
		if( c_c_dat[i].name ){
			DEBUG3("stty: c_cflag %s, ms 0x%x mc 0x%x",
					 c_c_dat[i].name, c_c_dat[i].or_dat, c_c_dat[i].and_dat);
			t_dat.c_cflag &= ~(c_c_dat[i].and_dat);
			t_dat.c_cflag |= c_c_dat[i].or_dat;
			continue;
		}
		for( i = 0; c_l_dat[i].name && safestrcasecmp( c_l_dat[i].name, arg); i++);
		if( c_l_dat[i].name ){
			DEBUG3("stty: c_lflag %s, ms 0x%x mc 0x%x",
					 c_l_dat[i].name, c_l_dat[i].or_dat, c_l_dat[i].and_dat);
			t_dat.c_lflag &= ~(c_l_dat[i].and_dat);
			t_dat.c_lflag |= c_l_dat[i].or_dat;
			continue;
		}
		for( i = 0; special[i].name && safestrcasecmp( special[i].name, arg); i++);
		if( special[i].name ){
			++count;
			option = l.list[count];
			if( option == 0 ){
				FATAL(LOG_INFO) "stty: missing parameter for %s", arg);
			}
			if( option[0] == '^' ){
				if( option[1] == '?' ){
					*special[i].cp = 0177;
				} else {
					*special[i].cp = 037 & option[1];
				}
			} else {
				*special[i].cp = option[0];
			}
			DEBUG3("stty: special %s %s", arg, option);
			continue;
		}
#ifdef USE_TERMIOX
		for( i = 0; tx_x_dat[i].name && safestrcasecmp( tx_x_dat[i].name, arg); i++);
		if( tx_x_dat[i].name ){
			DEBUG3("stty: tx_xflag %s, ms 0x%x mc 0x%x",
					 tx_x_dat[i].name, tx_x_dat[i].or_dat, tx_x_dat[i].and_dat);
			tx_dat.x_hflag &= ~(tx_x_dat[i].and_dat);
			tx_dat.x_hflag |= tx_x_dat[i].or_dat;
			continue;
		}
#endif /* USE_TERMIOX */
		FATAL(LOG_INFO) "unknown mode: %s\n", arg);
	}

	if( Read_write_DYN && (t_dat.c_lflag & ICANON) == 0 ){
		/* only do this if ICANON is off -- Martin Forssen */
		DEBUG2("setting port to read/write with unbuffered reads( MIN=1, TIME=0)");
		t_dat.c_cc[VMIN] = 1;
		t_dat.c_cc[VTIME] = 0;
	}
	DEBUG2("stty: after iflag 0x%x, oflag 0x%x, cflag 0x%x lflag 0x%x",
		(int)t_dat.c_iflag, (int)t_dat.c_oflag,
		(int)t_dat.c_cflag, (int)t_dat.c_lflag);

	if( tcsetattr( fd, TCSANOW, &t_dat) < 0 ){
		LOGERR_DIE(LOG_NOTICE) "cannot set tty parameters");
	}

#ifdef USE_TERMIOX
	if( termiox_fail == 0 && ioctl( fd, TCSETX, &tx_dat) < 0 ){
		LOGERR_DIE(LOG_NOTICE) "cannot set tty parameters (termiox)");
	}
#endif
	Free_line_list(&l);
}
#endif
