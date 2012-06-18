/*
 * System-dependent procedures for pppd under SunOS 4.
 *
 * Copyright (c) 1994 The Australian National University.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  This software is provided without any
 * warranty, express or implied. The Australian National University
 * makes no representations about the suitability of this software for
 * any purpose.
 *
 * IN NO EVENT SHALL THE AUSTRALIAN NATIONAL UNIVERSITY BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
 * THE AUSTRALIAN NATIONAL UNIVERSITY HAVE BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * THE AUSTRALIAN NATIONAL UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUSTRALIAN NATIONAL UNIVERSITY HAS NO
 * OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
 * OR MODIFICATIONS.
 */

#define RCSID	"$Id: sys-sunos4.c,v 1.1.1.4 2003/10/14 08:09:53 sparq Exp $"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <malloc.h>
#include <utmp.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/nit_if.h>
#include <net/route.h>
#include <net/ppp_defs.h>
#include <net/pppio.h>
#include <netinet/in.h>

#include "pppd.h"

#if defined(sun) && defined(sparc)
#include <alloca.h>
#ifndef __GNUC__
extern void *alloca();
#endif
#endif /*sparc*/

static const char rcsid[] = RCSID;

static int	pppfd;
static int	fdmuxid = -1;
static int	iffd;
static int	sockfd;

static int	restore_term;
static struct termios inittermios;
static struct winsize wsinfo;	/* Initial window size info */
static pid_t	parent_pid;	/* PID of our parent */

extern u_char	inpacket_buf[];	/* borrowed from main.c */

#define MAX_POLLFDS	32
static struct pollfd pollfds[MAX_POLLFDS];
static int n_pollfds;

static int	link_mtu, link_mru;

#define NMODULES	32
static int	tty_nmodules;
static char	tty_modules[NMODULES][FMNAMESZ+1];

static int	if_is_up;	/* Interface has been marked up */
static u_int32_t ifaddrs[2];	/* local and remote addresses */
static u_int32_t default_route_gateway;	/* Gateway for default route added */
static u_int32_t proxy_arp_addr;	/* Addr for proxy arp entry added */

/* Prototypes for procedures local to this file. */
static int translate_speed __P((int));
static int baud_rate_of __P((int));
static int get_ether_addr __P((u_int32_t, struct sockaddr *));
static int strioctl __P((int, int, void *, int, int));


/*
 * sys_init - System-dependent initialization.
 */
void
sys_init()
{
    int x;

    /* Get an internet socket for doing socket ioctl's on. */
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	fatal("Couldn't create IP socket: %m");

    /*
     * We may want to send a SIGHUP to the session leader associated
     * with our controlling terminal later.  Because SunOS doesn't
     * have getsid(), we make do with sending the signal to our
     * parent process.
     */
    parent_pid = getppid();

    /*
     * Open the ppp device.
     */
    pppfd = open("/dev/ppp", O_RDWR | O_NONBLOCK, 0);
    if (pppfd < 0)
	fatal("Can't open /dev/ppp: %m");
    if (kdebugflag) {
	x = PPPDBG_LOG + PPPDBG_DRIVER;
	strioctl(pppfd, PPPIO_DEBUG, &x, sizeof(int), 0);
    }

    /* Assign a new PPA and get its unit number. */
    if (strioctl(pppfd, PPPIO_NEWPPA, &ifunit, 0, sizeof(int)) < 0)
	fatal("Can't create new PPP interface: %m");

    /*
     * Open the ppp device again and push the if_ppp module on it.
     */
    iffd = open("/dev/ppp", O_RDWR, 0);
    if (iffd < 0)
	fatal("Can't open /dev/ppp (2): %m");
    if (kdebugflag) {
	x = PPPDBG_LOG + PPPDBG_DRIVER;
	strioctl(iffd, PPPIO_DEBUG, &x, sizeof(int), 0);
    }
    if (strioctl(iffd, PPPIO_ATTACH, &ifunit, sizeof(int), 0) < 0)
	fatal("Couldn't attach ppp interface to device: %m");
    if (ioctl(iffd, I_PUSH, "if_ppp") < 0)
	fatal("Can't push ppp interface module: %m");
    if (kdebugflag) {
	x = PPPDBG_LOG + PPPDBG_IF;
	strioctl(iffd, PPPIO_DEBUG, &x, sizeof(int), 0);
    }
    if (strioctl(iffd, PPPIO_NEWPPA, &ifunit, sizeof(int), 0) < 0)
	fatal("Couldn't create ppp interface unit: %m");
    x = PPP_IP;
    if (strioctl(iffd, PPPIO_BIND, &x, sizeof(int), 0) < 0)
	fatal("Couldn't bind ppp interface to IP SAP: %m");

    n_pollfds = 0;
}

/*
 * sys_cleanup - restore any system state we modified before exiting:
 * mark the interface down, delete default route and/or proxy arp entry.
 * This shouldn't call die() because it's called from die().
 */
void
sys_cleanup()
{
    if (if_is_up)
	sifdown(0);
    if (ifaddrs[0])
	cifaddr(0, ifaddrs[0], ifaddrs[1]);
    if (default_route_gateway)
	cifdefaultroute(0, 0, default_route_gateway);
    if (proxy_arp_addr)
	cifproxyarp(0, proxy_arp_addr);
}

/*
 * sys_close - Clean up in a child process before execing.
 */
void
sys_close()
{
    close(iffd);
    close(pppfd);
    close(sockfd);
}

/*
 * sys_check_options - check the options that the user specified
 */
int
sys_check_options()
{
    return 1;
}


/*
 * ppp_available - check whether the system has any ppp interfaces
 */
int
ppp_available()
{
    struct stat buf;

    return stat("/dev/ppp", &buf) >= 0;
}

/*
 * tty_establish_ppp - Turn the serial port into a ppp interface.
 */
int
tty_establish_ppp(fd)
    int fd;
{
    int i;

    /* Pop any existing modules off the tty stream. */
    for (i = 0;; ++i)
	if (ioctl(fd, I_LOOK, tty_modules[i]) < 0
	    || ioctl(fd, I_POP, 0) < 0)
	    break;
    tty_nmodules = i;

    /* Push the async hdlc module and the compressor module. */
    if (ioctl(fd, I_PUSH, "ppp_ahdl") < 0)
	fatal("Couldn't push PPP Async HDLC module: %m");
    if (ioctl(fd, I_PUSH, "ppp_comp") < 0)
	error("Couldn't push PPP compression module: %m");

    /* Link the serial port under the PPP multiplexor. */
    if ((fdmuxid = ioctl(pppfd, I_LINK, fd)) < 0)
	fatal("Can't link tty to PPP mux: %m");

    return pppfd;
}

/*
 * disestablish_ppp - Restore the serial port to normal operation.
 * It attempts to reconstruct the stream with the previously popped
 * modules.  This shouldn't call die() because it's called from die().
 */
void
tty_disestablish_ppp(fd)
    int fd;
{
    int i;

    if (fdmuxid >= 0) {
	if (ioctl(pppfd, I_UNLINK, fdmuxid) < 0) {
	    if (!hungup)
		error("Can't unlink tty from PPP mux: %m");
	}
	fdmuxid = -1;

	if (!hungup) {
	    while (ioctl(fd, I_POP, 0) >= 0)
		;
	    for (i = tty_nmodules - 1; i >= 0; --i)
		if (ioctl(fd, I_PUSH, tty_modules[i]) < 0)
		    error("Couldn't restore tty module %s: %m",
			   tty_modules[i]);
	}
	if (hungup && default_device && parent_pid > 0) {
	    /*
	     * If we have received a hangup, we need to send a SIGHUP
	     * to the terminal's controlling process.  The reason is
	     * that the original stream head for the terminal hasn't
	     * seen the M_HANGUP message (it went up through the ppp
	     * driver to the stream head for our fd to /dev/ppp).
	     * Actually we send the signal to the process that invoked
	     * pppd, since SunOS doesn't have getsid().
	     */
	    kill(parent_pid, SIGHUP);
	}
    }
}

/*
 * Check whether the link seems not to be 8-bit clean.
 */
void
clean_check()
{
    int x;
    char *s;

    if (strioctl(pppfd, PPPIO_GCLEAN, &x, 0, sizeof(x)) < 0)
	return;
    s = NULL;
    switch (~x) {
    case RCV_B7_0:
	s = "bit 7 set to 1";
	break;
    case RCV_B7_1:
	s = "bit 7 set to 0";
	break;
    case RCV_EVNP:
	s = "odd parity";
	break;
    case RCV_ODDP:
	s = "even parity";
	break;
    }
    if (s != NULL) {
	warn("Serial link is not 8-bit clean:");
	warn("All received characters had %s", s);
    }
}

/*
 * List of valid speeds.
 */
struct speed {
    int speed_int, speed_val;
} speeds[] = {
#ifdef B50
    { 50, B50 },
#endif
#ifdef B75
    { 75, B75 },
#endif
#ifdef B110
    { 110, B110 },
#endif
#ifdef B134
    { 134, B134 },
#endif
#ifdef B150
    { 150, B150 },
#endif
#ifdef B200
    { 200, B200 },
#endif
#ifdef B300
    { 300, B300 },
#endif
#ifdef B600
    { 600, B600 },
#endif
#ifdef B1200
    { 1200, B1200 },
#endif
#ifdef B1800
    { 1800, B1800 },
#endif
#ifdef B2000
    { 2000, B2000 },
#endif
#ifdef B2400
    { 2400, B2400 },
#endif
#ifdef B3600
    { 3600, B3600 },
#endif
#ifdef B4800
    { 4800, B4800 },
#endif
#ifdef B7200
    { 7200, B7200 },
#endif
#ifdef B9600
    { 9600, B9600 },
#endif
#ifdef B19200
    { 19200, B19200 },
#endif
#ifdef B38400
    { 38400, B38400 },
#endif
#ifdef EXTA
    { 19200, EXTA },
#endif
#ifdef EXTB
    { 38400, EXTB },
#endif
#ifdef B57600
    { 57600, B57600 },
#endif
#ifdef B115200
    { 115200, B115200 },
#endif
    { 0, 0 }
};

/*
 * Translate from bits/second to a speed_t.
 */
static int
translate_speed(bps)
    int bps;
{
    struct speed *speedp;

    if (bps == 0)
	return 0;
    for (speedp = speeds; speedp->speed_int; speedp++)
	if (bps == speedp->speed_int)
	    return speedp->speed_val;
    warn("speed %d not supported", bps);
    return 0;
}

/*
 * Translate from a speed_t to bits/second.
 */
static int
baud_rate_of(speed)
    int speed;
{
    struct speed *speedp;

    if (speed == 0)
	return 0;
    for (speedp = speeds; speedp->speed_int; speedp++)
	if (speed == speedp->speed_val)
	    return speedp->speed_int;
    return 0;
}

/*
 * set_up_tty: Set up the serial port on `fd' for 8 bits, no parity,
 * at the requested speed, etc.  If `local' is true, set CLOCAL
 * regardless of whether the modem option was specified.
 */
void
set_up_tty(fd, local)
    int fd, local;
{
    int speed;
    struct termios tios;

    if (tcgetattr(fd, &tios) < 0)
	fatal("tcgetattr: %m");

    if (!restore_term) {
	inittermios = tios;
	ioctl(fd, TIOCGWINSZ, &wsinfo);
    }

    tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CLOCAL);
    if (crtscts > 0)
	tios.c_cflag |= CRTSCTS;
    else if (crtscts < 0)
	tios.c_cflag &= ~CRTSCTS;

    tios.c_cflag |= CS8 | CREAD | HUPCL;
    if (local || !modem)
	tios.c_cflag |= CLOCAL;
    tios.c_iflag = IGNBRK | IGNPAR;
    tios.c_oflag = 0;
    tios.c_lflag = 0;
    tios.c_cc[VMIN] = 1;
    tios.c_cc[VTIME] = 0;

    if (crtscts == -2) {
	tios.c_iflag |= IXON | IXOFF;
	tios.c_cc[VSTOP] = 0x13;	/* DC3 = XOFF = ^S */
	tios.c_cc[VSTART] = 0x11;	/* DC1 = XON  = ^Q */
    }

    speed = translate_speed(inspeed);
    if (speed) {
	cfsetospeed(&tios, speed);
	cfsetispeed(&tios, speed);
    } else {
	speed = cfgetospeed(&tios);
	/*
	 * We can't proceed if the serial port speed is 0,
	 * since that implies that the serial port is disabled.
	 */
	if (speed == B0)
	    fatal("Baud rate for %s is 0; need explicit baud rate", devnam);
    }

    if (tcsetattr(fd, TCSAFLUSH, &tios) < 0)
	fatal("tcsetattr: %m");

    baud_rate = inspeed = baud_rate_of(speed);
    restore_term = 1;
}

/*
 * restore_tty - restore the terminal to the saved settings.
 */
void
restore_tty(fd)
    int fd;
{
    if (restore_term) {
	if (!default_device) {
	    /*
	     * Turn off echoing, because otherwise we can get into
	     * a loop with the tty and the modem echoing to each other.
	     * We presume we are the sole user of this tty device, so
	     * when we close it, it will revert to its defaults anyway.
	     */
	    inittermios.c_lflag &= ~(ECHO | ECHONL);
	}
	if (tcsetattr(fd, TCSAFLUSH, &inittermios) < 0)
	    if (!hungup && errno != ENXIO)
		warn("tcsetattr: %m");
	ioctl(fd, TIOCSWINSZ, &wsinfo);
	restore_term = 0;
    }
}

/*
 * setdtr - control the DTR line on the serial port.
 * This is called from die(), so it shouldn't call die().
 */
void
setdtr(fd, on)
int fd, on;
{
    int modembits = TIOCM_DTR;

    ioctl(fd, (on? TIOCMBIS: TIOCMBIC), &modembits);
}

/*
 * open_loopback - open the device we use for getting packets
 * in demand mode.  Under SunOS, we use our existing fd
 * to the ppp driver.
 */
int
open_ppp_loopback()
{
    return pppfd;
}

/*
 * output - Output PPP packet.
 */
void
output(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    struct strbuf data;
    int retries;
    struct pollfd pfd;

    if (debug)
	dbglog("sent %P", p, len);

    data.len = len;
    data.buf = (caddr_t) p;
    retries = 4;
    while (putmsg(pppfd, NULL, &data, 0) < 0) {
	if (--retries < 0 || (errno != EWOULDBLOCK && errno != EAGAIN)) {
	    if (errno != ENXIO)
		error("Couldn't send packet: %m");
	    break;
	}
	pfd.fd = pppfd;
	pfd.events = POLLOUT;
	poll(&pfd, 1, 250);	/* wait for up to 0.25 seconds */
    }
}


/*
 * wait_input - wait until there is data available,
 * for the length of time specified by *timo (indefinite
 * if timo is NULL).
 */
void
wait_input(timo)
    struct timeval *timo;
{
    int t;

    t = timo == NULL? -1: timo->tv_sec * 1000 + timo->tv_usec / 1000;
    if (poll(pollfds, n_pollfds, t) < 0 && errno != EINTR) {
	if (errno != EAGAIN)
	    fatal("poll: %m");
	/* we can get EAGAIN on a heavily loaded system,
	 * just wait a short time and try again. */
	usleep(50000);
    }
}

/*
 * add_fd - add an fd to the set that wait_input waits for.
 */
void add_fd(fd)
    int fd;
{
    int n;

    for (n = 0; n < n_pollfds; ++n)
	if (pollfds[n].fd == fd)
	    return;
    if (n_pollfds < MAX_POLLFDS) {
	pollfds[n_pollfds].fd = fd;
	pollfds[n_pollfds].events = POLLIN | POLLPRI | POLLHUP;
	++n_pollfds;
    } else
	error("Too many inputs!");
}

/*
 * remove_fd - remove an fd from the set that wait_input waits for.
 */
void remove_fd(fd)
    int fd;
{
    int n;

    for (n = 0; n < n_pollfds; ++n) {
	if (pollfds[n].fd == fd) {
	    while (++n < n_pollfds)
		pollfds[n-1] = pollfds[n];
	    --n_pollfds;
	    break;
	}
    }
}


/*
 * read_packet - get a PPP packet from the serial device.
 */
int
read_packet(buf)
    u_char *buf;
{
    struct strbuf ctrl, data;
    int flags, len;
    unsigned char ctrlbuf[64];

    for (;;) {
	data.maxlen = PPP_MRU + PPP_HDRLEN;
	data.buf = (caddr_t) buf;
	ctrl.maxlen = sizeof(ctrlbuf);
	ctrl.buf = (caddr_t) ctrlbuf;
	flags = 0;
	len = getmsg(pppfd, &ctrl, &data, &flags);
	if (len < 0) {
	    if (errno == EAGAIN || errno == EINTR)
		return -1;
	    fatal("Error reading packet: %m");
	}

	if (ctrl.len <= 0)
	    return data.len;

	/*
	 * Got a M_PROTO or M_PCPROTO message.  Huh?
	 */
	if (debug)
	    dbglog("got ctrl msg len=%d", ctrl.len);

    }
}

/*
 * get_loop_output - get outgoing packets from the ppp device,
 * and detect when we want to bring the real link up.
 * Return value is 1 if we need to bring up the link, 0 otherwise.
 */
int
get_loop_output()
{
    int len;
    int rv = 0;

    while ((len = read_packet(inpacket_buf)) > 0) {
	if (loop_frame(inpacket_buf, len))
	    rv = 1;
    }
    return rv;
}

/*
 * ppp_send_config - configure the transmit characteristics of
 * the ppp interface.
 */
void
ppp_send_config(unit, mtu, asyncmap, pcomp, accomp)
    int unit, mtu;
    u_int32_t asyncmap;
    int pcomp, accomp;
{
    int cf[2];
    struct ifreq ifr;

    link_mtu = mtu;
    if (strioctl(pppfd, PPPIO_MTU, &mtu, sizeof(mtu), 0) < 0) {
	if (hungup && errno == ENXIO)
	    return;
	error("Couldn't set MTU: %m");
    }
    if (strioctl(pppfd, PPPIO_XACCM, &asyncmap, sizeof(asyncmap), 0) < 0) {
	error("Couldn't set transmit ACCM: %m");
    }
    cf[0] = (pcomp? COMP_PROT: 0) + (accomp? COMP_AC: 0);
    cf[1] = COMP_PROT | COMP_AC;
    if (strioctl(pppfd, PPPIO_CFLAGS, cf, sizeof(cf), sizeof(int)) < 0) {
	error("Couldn't set prot/AC compression: %m");
    }

    /* set mtu for ip as well */
    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    ifr.ifr_metric = link_mtu;
    if (ioctl(sockfd, SIOCSIFMTU, &ifr) < 0) {
	error("Couldn't set IP MTU: %m");
    }
}

/*
 * ppp_set_xaccm - set the extended transmit ACCM for the interface.
 */
void
ppp_set_xaccm(unit, accm)
    int unit;
    ext_accm accm;
{
    if (strioctl(pppfd, PPPIO_XACCM, accm, sizeof(ext_accm), 0) < 0) {
	if (!hungup || errno != ENXIO)
	    warn("Couldn't set extended ACCM: %m");
    }
}

/*
 * ppp_recv_config - configure the receive-side characteristics of
 * the ppp interface.
 */
void
ppp_recv_config(unit, mru, asyncmap, pcomp, accomp)
    int unit, mru;
    u_int32_t asyncmap;
    int pcomp, accomp;
{
    int cf[2];

    link_mru = mru;
    if (strioctl(pppfd, PPPIO_MRU, &mru, sizeof(mru), 0) < 0) {
	if (hungup && errno == ENXIO)
	    return;
	error("Couldn't set MRU: %m");
    }
    if (strioctl(pppfd, PPPIO_RACCM, &asyncmap, sizeof(asyncmap), 0) < 0) {
	error("Couldn't set receive ACCM: %m");
    }
    cf[0] = (pcomp? DECOMP_PROT: 0) + (accomp? DECOMP_AC: 0);
    cf[1] = DECOMP_PROT | DECOMP_AC;
    if (strioctl(pppfd, PPPIO_CFLAGS, cf, sizeof(cf), sizeof(int)) < 0) {
	error("Couldn't set prot/AC decompression: %m");
    }
}

/*
 * ccp_test - ask kernel whether a given compression method
 * is acceptable for use.
 */
int
ccp_test(unit, opt_ptr, opt_len, for_transmit)
    int unit, opt_len, for_transmit;
    u_char *opt_ptr;
{
    if (strioctl(pppfd, (for_transmit? PPPIO_XCOMP: PPPIO_RCOMP),
		 opt_ptr, opt_len, 0) >= 0)
	return 1;
    return (errno == ENOSR)? 0: -1;
}

/*
 * ccp_flags_set - inform kernel about the current state of CCP.
 */
void
ccp_flags_set(unit, isopen, isup)
    int unit, isopen, isup;
{
    int cf[2];

    cf[0] = (isopen? CCP_ISOPEN: 0) + (isup? CCP_ISUP: 0);
    cf[1] = CCP_ISOPEN | CCP_ISUP | CCP_ERROR | CCP_FATALERROR;
    if (strioctl(pppfd, PPPIO_CFLAGS, cf, sizeof(cf), sizeof(int)) < 0) {
	if (!hungup || errno != ENXIO)
	    error("Couldn't set kernel CCP state: %m");
    }
}

/*
 * get_idle_time - return how long the link has been idle.
 */
int
get_idle_time(u, ip)
    int u;
    struct ppp_idle *ip;
{
    return strioctl(pppfd, PPPIO_GIDLE, ip, 0, sizeof(struct ppp_idle)) >= 0;
}

/*
 * get_ppp_stats - return statistics for the link.
 */
int
get_ppp_stats(u, stats)
    int u;
    struct pppd_stats *stats;
{
    struct ppp_stats s;

    if (strioctl(pppfd, PPPIO_GETSTAT, &s, 0, sizeof(s)) < 0) {
	error("Couldn't get link statistics: %m");
	return 0;
    }
    stats->bytes_in = s.p.ppp_ibytes;
    stats->bytes_out = s.p.ppp_obytes;
    return 1;
}


/*
 * ccp_fatal_error - returns 1 if decompression was disabled as a
 * result of an error detected after decompression of a packet,
 * 0 otherwise.  This is necessary because of patent nonsense.
 */
int
ccp_fatal_error(unit)
    int unit;
{
    int cf[2];

    cf[0] = cf[1] = 0;
    if (strioctl(pppfd, PPPIO_CFLAGS, cf, sizeof(cf), sizeof(int)) < 0) {
	if (errno != ENXIO && errno != EINVAL)
	    error("Couldn't get compression flags: %m");
	return 0;
    }
    return cf[0] & CCP_FATALERROR;
}

/*
 * sifvjcomp - config tcp header compression
 */
int
sifvjcomp(u, vjcomp, xcidcomp, xmaxcid)
    int u, vjcomp, xcidcomp, xmaxcid;
{
    int cf[2];
    char maxcid[2];

    if (vjcomp) {
	maxcid[0] = xcidcomp;
	maxcid[1] = 15;		
	if (strioctl(pppfd, PPPIO_VJINIT, maxcid, sizeof(maxcid), 0) < 0) {
	    error("Couldn't initialize VJ compression: %m");
	}
    }

    cf[0] = (vjcomp? COMP_VJC + DECOMP_VJC: 0)	
	+ (xcidcomp? COMP_VJCCID + DECOMP_VJCCID: 0);
    cf[1] = COMP_VJC + DECOMP_VJC + COMP_VJCCID + DECOMP_VJCCID;
    if (strioctl(pppfd, PPPIO_CFLAGS, cf, sizeof(cf), sizeof(int)) < 0) {
	if (vjcomp)
	    error("Couldn't enable VJ compression: %m");
    }

    return 1;
}

/*
 * sifup - Config the interface up and enable IP packets to pass.
 */
int
sifup(u)
    int u;
{
    struct ifreq ifr;

    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
	error("Couldn't mark interface up (get): %m");
	return 0;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
	error("Couldn't mark interface up (set): %m");
	return 0;
    }
    if_is_up = 1;
    return 1;
}

/*
 * sifdown - Config the interface down and disable IP.
 */
int
sifdown(u)
    int u;
{
    struct ifreq ifr;

    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
	error("Couldn't mark interface down (get): %m");
	return 0;
    }
    if ((ifr.ifr_flags & IFF_UP) != 0) {
	ifr.ifr_flags &= ~IFF_UP;
	if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
	    error("Couldn't mark interface down (set): %m");
	    return 0;
	}
    }
    if_is_up = 0;
    return 1;
}

/*
 * sifnpmode - Set the mode for handling packets for a given NP.
 */
int
sifnpmode(u, proto, mode)
    int u;
    int proto;
    enum NPmode mode;
{
    int npi[2];

    npi[0] = proto;
    npi[1] = (int) mode;
    if (strioctl(pppfd, PPPIO_NPMODE, npi, 2 * sizeof(int), 0) < 0) {
	error("ioctl(set NP %d mode to %d): %m", proto, mode);
	return 0;
    }
    return 1;
}

#define INET_ADDR(x)	(((struct sockaddr_in *) &(x))->sin_addr.s_addr)

/*
 * sifaddr - Config the interface IP addresses and netmask.
 */
int
sifaddr(u, o, h, m)
    int u;
    u_int32_t o, h, m;
{
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    ifr.ifr_addr.sa_family = AF_INET;
    INET_ADDR(ifr.ifr_addr) = m;
    if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
	error("Couldn't set IP netmask: %m");
    }
    ifr.ifr_addr.sa_family = AF_INET;
    INET_ADDR(ifr.ifr_addr) = o;
    if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) {
	error("Couldn't set local IP address: %m");
    }
    ifr.ifr_dstaddr.sa_family = AF_INET;
    INET_ADDR(ifr.ifr_dstaddr) = h;
    if (ioctl(sockfd, SIOCSIFDSTADDR, &ifr) < 0) {
	error("Couldn't set remote IP address: %m");
    }
    ifaddrs[0] = o;
    ifaddrs[1] = h;

    return 1;
}

/*
 * cifaddr - Clear the interface IP addresses, and delete routes
 * through the interface if possible.
 */
int
cifaddr(u, o, h)
    int u;
    u_int32_t o, h;
{
    struct rtentry rt;

    bzero(&rt, sizeof(rt));
    rt.rt_dst.sa_family = AF_INET;
    INET_ADDR(rt.rt_dst) = h;
    rt.rt_gateway.sa_family = AF_INET;
    INET_ADDR(rt.rt_gateway) = o;
    rt.rt_flags = RTF_HOST;
    if (ioctl(sockfd, SIOCDELRT, &rt) < 0)
	error("Couldn't delete route through interface: %m");
    ifaddrs[0] = 0;
    return 1;
}

/*
 * sifdefaultroute - assign a default route through the address given.
 */
int
sifdefaultroute(u, l, g)
    int u;
    u_int32_t l, g;
{
    struct rtentry rt;

    bzero(&rt, sizeof(rt));
    rt.rt_dst.sa_family = AF_INET;
    INET_ADDR(rt.rt_dst) = 0;
    rt.rt_gateway.sa_family = AF_INET;
    INET_ADDR(rt.rt_gateway) = g;
    rt.rt_flags = RTF_GATEWAY;

    if (ioctl(sockfd, SIOCADDRT, &rt) < 0) {
	error("Can't add default route: %m");
	return 0;
    }

    default_route_gateway = g;
    return 1;
}

/*
 * cifdefaultroute - delete a default route through the address given.
 */
int
cifdefaultroute(u, l, g)
    int u;
    u_int32_t l, g;
{
    struct rtentry rt;

    bzero(&rt, sizeof(rt));
    rt.rt_dst.sa_family = AF_INET;
    INET_ADDR(rt.rt_dst) = 0;
    rt.rt_gateway.sa_family = AF_INET;
    INET_ADDR(rt.rt_gateway) = g;
    rt.rt_flags = RTF_GATEWAY;

    if (ioctl(sockfd, SIOCDELRT, &rt) < 0) {
	error("Can't delete default route: %m");
	return 0;
    }

    default_route_gateway = 0;
    return 1;
}

/*
 * sifproxyarp - Make a proxy ARP entry for the peer.
 */
int
sifproxyarp(unit, hisaddr)
    int unit;
    u_int32_t hisaddr;
{
    struct arpreq arpreq;

    bzero(&arpreq, sizeof(arpreq));
    if (!get_ether_addr(hisaddr, &arpreq.arp_ha))
	return 0;

    arpreq.arp_pa.sa_family = AF_INET;
    INET_ADDR(arpreq.arp_pa) = hisaddr;
    arpreq.arp_flags = ATF_PERM | ATF_PUBL;
    if (ioctl(sockfd, SIOCSARP, (caddr_t) &arpreq) < 0) {
	error("Couldn't set proxy ARP entry: %m");
	return 0;
    }

    proxy_arp_addr = hisaddr;
    return 1;
}

/*
 * cifproxyarp - Delete the proxy ARP entry for the peer.
 */
int
cifproxyarp(unit, hisaddr)
    int unit;
    u_int32_t hisaddr;
{
    struct arpreq arpreq;

    bzero(&arpreq, sizeof(arpreq));
    arpreq.arp_pa.sa_family = AF_INET;
    INET_ADDR(arpreq.arp_pa) = hisaddr;
    if (ioctl(sockfd, SIOCDARP, (caddr_t)&arpreq) < 0) {
	error("Couldn't delete proxy ARP entry: %m");
	return 0;
    }

    proxy_arp_addr = 0;
    return 1;
}

/*
 * get_ether_addr - get the hardware address of an interface on the
 * the same subnet as ipaddr.
 */
#define MAX_IFS		32

static int
get_ether_addr(ipaddr, hwaddr)
    u_int32_t ipaddr;
    struct sockaddr *hwaddr;
{
    struct ifreq *ifr, *ifend;
    u_int32_t ina, mask;
    struct ifreq ifreq;
    struct ifconf ifc;
    struct ifreq ifs[MAX_IFS];
    int nit_fd;

    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_req = ifs;
    if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
	error("ioctl(SIOCGIFCONF): %m");
	return 0;
    }

    /*
     * Scan through looking for an interface with an Internet
     * address on the same subnet as `ipaddr'.
     */
    ifend = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifr = ifc.ifc_req; ifr < ifend; ifr = (struct ifreq *)
	    ((char *)&ifr->ifr_addr + sizeof(struct sockaddr))) {
        if (ifr->ifr_addr.sa_family == AF_INET) {

            /*
             * Check that the interface is up, and not point-to-point
             * or loopback.
             */
            strlcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
            if (ioctl(sockfd, SIOCGIFFLAGS, &ifreq) < 0)
                continue;
            if ((ifreq.ifr_flags &
                 (IFF_UP|IFF_BROADCAST|IFF_POINTOPOINT|IFF_LOOPBACK|IFF_NOARP))
                 != (IFF_UP|IFF_BROADCAST))
                continue;

            /*
             * Get its netmask and check that it's on the right subnet.
             */
            if (ioctl(sockfd, SIOCGIFNETMASK, &ifreq) < 0)
                continue;
            ina = ((struct sockaddr_in *) &ifr->ifr_addr)->sin_addr.s_addr;
            mask = ((struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr.s_addr;
            if ((ipaddr & mask) != (ina & mask))
                continue;

            break;
        }
    }

    if (ifr >= ifend)
	return 0;
    info("found interface %s for proxy arp", ifr->ifr_name);

    /*
     * Grab the physical address for this interface.
     */
    if ((nit_fd = open("/dev/nit", O_RDONLY)) < 0) {
	error("Couldn't open /dev/nit: %m");
	return 0;
    }
    strlcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
    if (ioctl(nit_fd, NIOCBIND, &ifreq) < 0
	|| ioctl(nit_fd, SIOCGIFADDR, &ifreq) < 0) {
	error("Couldn't get hardware address for %s: %m",
	       ifreq.ifr_name);
	close(nit_fd);
	return 0;
    }

    hwaddr->sa_family = AF_UNSPEC;
    memcpy(hwaddr->sa_data, ifreq.ifr_addr.sa_data, 6);
    close(nit_fd);
    return 1;
}

/*
 * have_route_to - determine if the system has any route to
 * a given IP address.
 * For demand mode to work properly, we have to ignore routes
 * through our own interface.
 */
int have_route_to(addr)
    u_int32_t addr;
{
    return -1;
}

#define	WTMPFILE	"/usr/adm/wtmp"

void
logwtmp(line, name, host)
    const char *line, *name, *host;
{
    int fd;
    struct stat buf;
    struct utmp ut;

    if ((fd = open(WTMPFILE, O_WRONLY|O_APPEND, 0)) < 0)
	return;
    if (!fstat(fd, &buf)) {
	strncpy(ut.ut_line, line, sizeof(ut.ut_line));
	strncpy(ut.ut_name, name, sizeof(ut.ut_name));
	strncpy(ut.ut_host, host, sizeof(ut.ut_host));
	(void)time(&ut.ut_time);
	if (write(fd, (char *)&ut, sizeof(struct utmp)) != sizeof(struct utmp))
	    (void)ftruncate(fd, buf.st_size);
    }
    close(fd);
}

/*
 * Return user specified netmask, modified by any mask we might determine
 * for address `addr' (in network byte order).
 * Here we scan through the system's list of interfaces, looking for
 * any non-point-to-point interfaces which might appear to be on the same
 * network as `addr'.  If we find any, we OR in their netmask to the
 * user-specified netmask.
 */
u_int32_t
GetMask(addr)
    u_int32_t addr;
{
    u_int32_t mask, nmask, ina;
    struct ifreq *ifr, *ifend, ifreq;
    struct ifconf ifc;

    addr = ntohl(addr);
    if (IN_CLASSA(addr))	/* determine network mask for address class */
	nmask = IN_CLASSA_NET;
    else if (IN_CLASSB(addr))
	nmask = IN_CLASSB_NET;
    else
	nmask = IN_CLASSC_NET;
    /* class D nets are disallowed by bad_ip_adrs */
    mask = netmask | htonl(nmask);

    /*
     * Scan through the system's network interfaces.
     */
    ifc.ifc_len = MAX_IFS * sizeof(struct ifreq);
    ifc.ifc_req = alloca(ifc.ifc_len);
    if (ifc.ifc_req == 0)
	return mask;
    if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
	warn("Couldn't get system interface list: %m");
	return mask;
    }
    ifend = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifr = ifc.ifc_req; ifr < ifend; ++ifr) {
	/*
	 * Check the interface's internet address.
	 */
	if (ifr->ifr_addr.sa_family != AF_INET)
	    continue;
	ina = INET_ADDR(ifr->ifr_addr);
	if ((ntohl(ina) & nmask) != (addr & nmask))
	    continue;
	/*
	 * Check that the interface is up, and not point-to-point or loopback.
	 */
	strlcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
	if (ioctl(sockfd, SIOCGIFFLAGS, &ifreq) < 0)
	    continue;
	if ((ifreq.ifr_flags & (IFF_UP|IFF_POINTOPOINT|IFF_LOOPBACK))
	    != IFF_UP)
	    continue;
	/*
	 * Get its netmask and OR it into our mask.
	 */
	if (ioctl(sockfd, SIOCGIFNETMASK, &ifreq) < 0)
	    continue;
	mask |= INET_ADDR(ifreq.ifr_addr);
    }

    return mask;
}

static int
strioctl(fd, cmd, ptr, ilen, olen)
    int fd, cmd, ilen, olen;
    void *ptr;
{
    struct strioctl str;

    str.ic_cmd = cmd;
    str.ic_timout = 0;
    str.ic_len = ilen;
    str.ic_dp = ptr;
    if (ioctl(fd, I_STR, &str) == -1)
	return -1;
    if (str.ic_len != olen)
	dbglog("strioctl: expected %d bytes, got %d for cmd %x\n",
	       olen, str.ic_len, cmd);
    return 0;
}

/*
 * Use the hostid as part of the random number seed.
 */
int
get_host_seed()
{
    return gethostid();
}


/*
 * get_pty - get a pty master/slave pair and chown the slave side
 * to the uid given.  Assumes slave_name points to >= 12 bytes of space.
 */
int
get_pty(master_fdp, slave_fdp, slave_name, uid)
    int *master_fdp;
    int *slave_fdp;
    char *slave_name;
    int uid;
{
    int i, mfd, sfd;
    char pty_name[12];
    struct termios tios;

    sfd = -1;
    for (i = 0; i < 64; ++i) {
	slprintf(pty_name, sizeof(pty_name), "/dev/pty%c%x",
		 'p' + i / 16, i % 16);
	mfd = open(pty_name, O_RDWR, 0);
	if (mfd >= 0) {
	    pty_name[5] = 't';
	    sfd = open(pty_name, O_RDWR | O_NOCTTY, 0);
	    if (sfd >= 0)
		break;
	    close(mfd);
	}
    }
    if (sfd < 0)
	return 0;

    strlcpy(slave_name, pty_name, 12);
    *master_fdp = mfd;
    *slave_fdp = sfd;
    fchown(sfd, uid, -1);
    fchmod(sfd, S_IRUSR | S_IWUSR);
    if (tcgetattr(sfd, &tios) == 0) {
	tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
	tios.c_cflag |= CS8 | CREAD;
	tios.c_iflag  = IGNPAR | CLOCAL;
	tios.c_oflag  = 0;
	tios.c_lflag  = 0;
	if (tcsetattr(sfd, TCSAFLUSH, &tios) < 0)
	    warn("couldn't set attributes on pty: %m");
    } else
	warn("couldn't get attributes on pty: %m");

    return 1;
}

/*
 * SunOS doesn't have strtoul :-(
 */
unsigned long
strtoul(str, ptr, base)
    char *str, **ptr;
    int base;
{
    return (unsigned long) strtol(str, ptr, base);
}

/*
 * Or strerror :-(
 */
extern char *sys_errlist[];
extern int sys_nerr;

char *
strerror(n)
    int n;
{
    static char unknown[32];

    if (n > 0 && n < sys_nerr)
	return sys_errlist[n];
    slprintf(unknown, sizeof(unknown), "Error %d", n);
    return unknown;
}
