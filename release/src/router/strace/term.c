/*
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
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

#ifdef LINUX
/*
 * The C library's definition of struct termios might differ from
 * the kernel one, and we need to use the kernel layout.
 */
#include <linux/termios.h>
#else

#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif /* HAVE_TERMIO_H */

#include <termios.h>
#endif

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

static const struct xlat tcxonc_options[] = {
	{ TCOOFF,	"TCOOFF"	},
	{ TCOON,	"TCOON"		},
	{ TCIOFF,	"TCIOFF"	},
	{ TCION,	"TCION"		},
	{ 0,		NULL		},
};

#ifdef TCLFLSH
static const struct xlat tcflsh_options[] = {
	{ TCIFLUSH,	"TCIFLUSH"	},
	{ TCOFLUSH,	"TCOFLUSH"	},
	{ TCIOFLUSH,	"TCIOFLUSH"	},
	{ 0,		NULL		},
};
#endif

static const struct xlat baud_options[] = {
	{ B0,		"B0"		},
	{ B50,		"B50"		},
	{ B75,		"B75"		},
	{ B110,		"B110"		},
	{ B134,		"B134"		},
	{ B150,		"B150"		},
	{ B200,		"B200"		},
	{ B300,		"B300"		},
	{ B600,		"B600"		},
	{ B1200,	"B1200"		},
	{ B1800,	"B1800"		},
	{ B2400,	"B2400"		},
	{ B4800,	"B4800"		},
	{ B9600,	"B9600"		},
#ifdef B19200
	{ B19200,	"B19200"	},
#endif
#ifdef B38400
	{ B38400,	"B38400"	},
#endif
#ifdef B57600
	{ B57600,	"B57600"	},
#endif
#ifdef B115200
	{ B115200,	"B115200"	},
#endif
#ifdef B230400
	{ B230400,	"B230400"	},
#endif
#ifdef B460800
	{ B460800,	"B460800"	},
#endif
#ifdef B500000
	{ B500000,	"B500000"	},
#endif
#ifdef B576000
	{ B576000,	"B576000"	},
#endif
#ifdef B921600
	{ B921600,	"B921600"	},
#endif
#ifdef B1000000
	{ B1000000,	"B1000000"	},
#endif
#ifdef B1152000
	{ B1152000,	"B1152000"	},
#endif
#ifdef B1500000
	{ B1500000,	"B1500000"	},
#endif
#ifdef B2000000
	{ B2000000,	"B2000000"	},
#endif
#ifdef B2500000
	{ B2500000,	"B2500000"	},
#endif
#ifdef B3000000
	{ B3000000,	"B3000000"	},
#endif
#ifdef B3500000
	{ B3500000,	"B3500000"	},
#endif
#ifdef B4000000
	{ B4000000,	"B4000000"	},
#endif
#ifdef EXTA
	{ EXTA,		"EXTA"		},
#endif
#ifdef EXTB
	{ EXTB,		"EXTB"		},
#endif
	{ 0,		NULL		},
};

static const struct xlat modem_flags[] = {
#ifdef TIOCM_LE
	{ TIOCM_LE,	"TIOCM_LE",	},
#endif
#ifdef TIOCM_DTR
	{ TIOCM_DTR,	"TIOCM_DTR",	},
#endif
#ifdef TIOCM_RTS
	{ TIOCM_RTS,	"TIOCM_RTS",	},
#endif
#ifdef TIOCM_ST
	{ TIOCM_ST,	"TIOCM_ST",	},
#endif
#ifdef TIOCM_SR
	{ TIOCM_SR,	"TIOCM_SR",	},
#endif
#ifdef TIOCM_CTS
	{ TIOCM_CTS,	"TIOCM_CTS",	},
#endif
#ifdef TIOCM_CAR
	{ TIOCM_CAR,	"TIOCM_CAR",	},
#endif
#ifdef TIOCM_CD
	{ TIOCM_CD,	"TIOCM_CD",	},
#endif
#ifdef TIOCM_RNG
	{ TIOCM_RNG,	"TIOCM_RNG",	},
#endif
#ifdef TIOCM_RI
	{ TIOCM_RI,	"TIOCM_RI",	},
#endif
#ifdef TIOCM_DSR
	{ TIOCM_DSR,	"TIOCM_DSR",	},
#endif
	{ 0,		NULL,		},
};


int term_ioctl(struct tcb *tcp, long code, long arg)
{
	struct termios tios;
#ifndef FREEBSD
	struct termio tio;
#else
	#define TCGETS	TIOCGETA
	#define TCSETS	TIOCSETA
	#define TCSETSW	TIOCSETAW
	#define TCSETSF	TIOCSETAF
#endif
	struct winsize ws;
#ifdef TIOCGSIZE
	struct  ttysize ts;
#endif
	int i;

	if (entering(tcp))
		return 0;

	switch (code) {

	/* ioctls with termios or termio args */

#ifdef TCGETS
	case TCGETS:
		if (syserror(tcp))
			return 0;
	case TCSETS:
	case TCSETSW:
	case TCSETSF:
		if (!verbose(tcp) || umove(tcp, arg, &tios) < 0)
			return 0;
		if (abbrev(tcp)) {
			tprintf(", {");
#ifndef FREEBSD
			printxval(baud_options, tios.c_cflag & CBAUD, "B???");
#else
			printxval(baud_options, tios.c_ispeed, "B???");
			if (tios.c_ispeed != tios.c_ospeed) {
				tprintf(" (in)");
				printxval(baud_options, tios.c_ospeed, "B???");
				tprintf(" (out)");
			}
#endif
			tprintf(" %sopost %sisig %sicanon %secho ...}",
				(tios.c_oflag & OPOST) ? "" : "-",
				(tios.c_lflag & ISIG) ? "" : "-",
				(tios.c_lflag & ICANON) ? "" : "-",
				(tios.c_lflag & ECHO) ? "" : "-");
			return 1;
		}
		tprintf(", {c_iflags=%#lx, c_oflags=%#lx, ",
			(long) tios.c_iflag, (long) tios.c_oflag);
		tprintf("c_cflags=%#lx, c_lflags=%#lx, ",
			(long) tios.c_cflag, (long) tios.c_lflag);
#if !defined(SVR4) && !defined(FREEBSD)
		tprintf("c_line=%u, ", tios.c_line);
#endif
		if (!(tios.c_lflag & ICANON))
			tprintf("c_cc[VMIN]=%d, c_cc[VTIME]=%d, ",
				tios.c_cc[VMIN], tios.c_cc[VTIME]);
		tprintf("c_cc=\"");
		for (i = 0; i < NCCS; i++)
			tprintf("\\x%02x", tios.c_cc[i]);
		tprintf("\"}");
		return 1;
#endif /* TCGETS */

#ifdef TCGETA
	case TCGETA:
		if (syserror(tcp))
			return 0;
	case TCSETA:
	case TCSETAW:
	case TCSETAF:
		if (!verbose(tcp) || umove(tcp, arg, &tio) < 0)
			return 0;
		if (abbrev(tcp)) {
			tprintf(", {");
			printxval(baud_options, tio.c_cflag & CBAUD, "B???");
			tprintf(" %sopost %sisig %sicanon %secho ...}",
				(tio.c_oflag & OPOST) ? "" : "-",
				(tio.c_lflag & ISIG) ? "" : "-",
				(tio.c_lflag & ICANON) ? "" : "-",
				(tio.c_lflag & ECHO) ? "" : "-");
			return 1;
		}
		tprintf(", {c_iflags=%#lx, c_oflags=%#lx, ",
			(long) tio.c_iflag, (long) tio.c_oflag);
		tprintf("c_cflags=%#lx, c_lflags=%#lx, ",
			(long) tio.c_cflag, (long) tio.c_lflag);
		tprintf("c_line=%u, ", tio.c_line);
#ifdef _VMIN
		if (!(tio.c_lflag & ICANON))
			tprintf("c_cc[_VMIN]=%d, c_cc[_VTIME]=%d, ",
				tio.c_cc[_VMIN], tio.c_cc[_VTIME]);
#else /* !_VMIN */
		if (!(tio.c_lflag & ICANON))
			tprintf("c_cc[VMIN]=%d, c_cc[VTIME]=%d, ",
				tio.c_cc[VMIN], tio.c_cc[VTIME]);
#endif /* !_VMIN */
		tprintf("c_cc=\"");
		for (i = 0; i < NCC; i++)
			tprintf("\\x%02x", tio.c_cc[i]);
		tprintf("\"}");
		return 1;
#endif /* TCGETA */

	/* ioctls with winsize or ttysize args */

#ifdef TIOCGWINSZ
	case TIOCGWINSZ:
		if (syserror(tcp))
			return 0;
	case TIOCSWINSZ:
		if (!verbose(tcp) || umove(tcp, arg, &ws) < 0)
			return 0;
		tprintf(", {ws_row=%d, ws_col=%d, ws_xpixel=%d, ws_ypixel=%d}",
			ws.ws_row, ws.ws_col, ws.ws_xpixel, ws.ws_ypixel);
		return 1;
#endif /* TIOCGWINSZ */

#ifdef TIOCGSIZE
	case TIOCGSIZE:
		if (syserror(tcp))
			return 0;
	case TIOCSSIZE:
		if (!verbose(tcp) || umove(tcp, arg, &ts) < 0)
			return 0;
		tprintf(", {ts_lines=%d, ts_cols=%d}",
			ts.ts_lines, ts.ts_cols);
		return 1;
#endif

	/* ioctls with a direct decodable arg */
#ifdef TCXONC
	case TCXONC:
		tprintf(", ");
		printxval(tcxonc_options, arg, "TC???");
		return 1;
#endif
#ifdef TCLFLSH
	case TCFLSH:
		tprintf(", ");
		printxval(tcflsh_options, arg, "TC???");
		return 1;
#endif

	/* ioctls with an indirect parameter displayed as modem flags */

#ifdef TIOCMGET
	case TIOCMGET:
	case TIOCMBIS:
	case TIOCMBIC:
	case TIOCMSET:
		if (umove(tcp, arg, &i) < 0)
			return 0;
		tprintf(", [");
		printflags(modem_flags, i, "TIOCM_???");
		tprintf("]");
		return 1;
#endif /* TIOCMGET */

	/* ioctls with an indirect parameter displayed in decimal */

	case TIOCSPGRP:
	case TIOCGPGRP:
#ifdef TIOCGETPGRP
	case TIOCGETPGRP:
#endif
#ifdef TIOCSETPGRP
	case TIOCSETPGRP:
#endif
#ifdef FIONREAD
	case FIONREAD:
#endif
	case TIOCOUTQ:
#ifdef FIONBIO
	case FIONBIO:
#endif
#ifdef FIOASYNC
	case FIOASYNC:
#endif
#ifdef FIOGETOWN
	case FIOGETOWN:
#endif
#ifdef FIOSETOWN
	case FIOSETOWN:
#endif
#ifdef TIOCGETD
	case TIOCGETD:
#endif
#ifdef TIOCSETD
	case TIOCSETD:
#endif
#ifdef TIOCPKT
	case TIOCPKT:
#endif
#ifdef TIOCREMOTE
	case TIOCREMOTE:
#endif
#ifdef TIOCUCNTL
	case TIOCUCNTL:
#endif
#ifdef TIOCTCNTL
	case TIOCTCNTL:
#endif
#ifdef TIOCSIGNAL
	case TIOCSIGNAL:
#endif
#ifdef TIOCSSOFTCAR
	case TIOCSSOFTCAR:
#endif
#ifdef TIOCGSOFTCAR
	case TIOCGSOFTCAR:
#endif
#ifdef TIOCISPACE
	case TIOCISPACE:
#endif
#ifdef TIOCISIZE
	case TIOCISIZE:
#endif
#ifdef TIOCSINTR
	case TIOCSINTR:
#endif
#ifdef TIOCSPTLCK
	case TIOCSPTLCK:
#endif
#ifdef TIOCGPTN
	case TIOCGPTN:
#endif
		tprintf(", ");
		printnum_int(tcp, arg, "%d");
		return 1;

	/* ioctls with an indirect parameter displayed as a char */

#ifdef TIOCSTI
	case TIOCSTI:
#endif
		tprintf(", ");
		printstr(tcp, arg, 1);
		return 1;

	/* ioctls with no parameters */

#ifdef TIOCSCTTY
	case TIOCSCTTY:
#endif
#ifdef TIOCNOTTY
	case TIOCNOTTY:
#endif
#ifdef FIOCLEX
	case FIOCLEX:
#endif
#ifdef FIONCLEX
	case FIONCLEX:
#endif
#ifdef TIOCCONS
	case TIOCCONS:
#endif
		return 1;

	/* ioctls which are unknown */

	default:
		return 0;
	}
}
