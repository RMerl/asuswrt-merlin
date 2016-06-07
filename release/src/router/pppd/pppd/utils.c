/*
 * utils.c - various utility functions used in pppd.
 *
 * Copyright (c) 1999-2002 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 3. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define RCSID	"$Id: utils.c,v 1.25 2008/06/03 12:06:37 paulus Exp $"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <time.h>
#include <utmp.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef SVR4
#include <sys/mkdev.h>
#endif

#include "pppd.h"
#include "fsm.h"
#include "lcp.h"

static const char rcsid[] = RCSID;

#if defined(SUNOS4)
extern char *strerror();
#endif

static void logit __P((int, char *, va_list));
static void log_write __P((int, char *));
static void vslp_printer __P((void *, char *, ...));
static void format_packet __P((u_char *, int, printer_func, void *));

struct buffer_info {
    char *ptr;
    int len;
};

/*
 * strlcpy - like strcpy/strncpy, doesn't overflow destination buffer,
 * always leaves destination null-terminated (for len > 0).
 */
size_t
strlcpy(dest, src, len)
    char *dest;
    const char *src;
    size_t len;
{
    size_t ret = strlen(src);

    if (len != 0) {
	if (ret < len)
	    strcpy(dest, src);
	else {
	    strncpy(dest, src, len - 1);
	    dest[len-1] = 0;
	}
    }
    return ret;
}

/*
 * strlcat - like strcat/strncat, doesn't overflow destination buffer,
 * always leaves destination null-terminated (for len > 0).
 */
size_t
strlcat(dest, src, len)
    char *dest;
    const char *src;
    size_t len;
{
    size_t dlen = strlen(dest);

    return dlen + strlcpy(dest + dlen, src, (len > dlen? len - dlen: 0));
}


/*
 * slprintf - format a message into a buffer.  Like sprintf except we
 * also specify the length of the output buffer, and we handle
 * %m (error message), %v (visible string),
 * %q (quoted string), %t (current time) and %I (IP address) formats.
 * Doesn't do floating-point formats.
 * Returns the number of chars put into buf.
 */
int
slprintf __V((char *buf, int buflen, char *fmt, ...))
{
    va_list args;
    int n;

#if defined(__STDC__)
    va_start(args, fmt);
#else
    char *buf;
    int buflen;
    char *fmt;
    va_start(args);
    buf = va_arg(args, char *);
    buflen = va_arg(args, int);
    fmt = va_arg(args, char *);
#endif
    n = vslprintf(buf, buflen, fmt, args);
    va_end(args);
    return n;
}

/*
 * vslprintf - like slprintf, takes a va_list instead of a list of args.
 */
#define OUTCHAR(c)	(buflen > 0? (--buflen, *buf++ = (c)): 0)

int
vslprintf(buf, buflen, fmt, args)
    char *buf;
    int buflen;
    char *fmt;
    va_list args;
{
    int c, i, n;
    int width, prec, fillch;
    int base, len, neg, quoted;
    unsigned long val = 0;
    char *str, *f, *buf0;
    unsigned char *p;
    char num[32];
    time_t t;
    u_int32_t ip;
    static char hexchars[] = "0123456789abcdef";
    struct buffer_info bufinfo;

    buf0 = buf;
    --buflen;
    while (buflen > 0) {
	for (f = fmt; *f != '%' && *f != 0; ++f)
	    ;
	if (f > fmt) {
	    len = f - fmt;
	    if (len > buflen)
		len = buflen;
	    memcpy(buf, fmt, len);
	    buf += len;
	    buflen -= len;
	    fmt = f;
	}
	if (*fmt == 0)
	    break;
	c = *++fmt;
	width = 0;
	prec = -1;
	fillch = ' ';
	if (c == '0') {
	    fillch = '0';
	    c = *++fmt;
	}
	if (c == '*') {
	    width = va_arg(args, int);
	    c = *++fmt;
	} else {
	    while (isdigit(c)) {
		width = width * 10 + c - '0';
		c = *++fmt;
	    }
	}
	if (c == '.') {
	    c = *++fmt;
	    if (c == '*') {
		prec = va_arg(args, int);
		c = *++fmt;
	    } else {
		prec = 0;
		while (isdigit(c)) {
		    prec = prec * 10 + c - '0';
		    c = *++fmt;
		}
	    }
	}
	str = 0;
	base = 0;
	neg = 0;
	++fmt;
	switch (c) {
	case 'l':
	    c = *fmt++;
	    switch (c) {
	    case 'd':
		val = va_arg(args, long);
		if (val < 0) {
		    neg = 1;
		    val = -val;
		}
		base = 10;
		break;
	    case 'u':
		val = va_arg(args, unsigned long);
		base = 10;
		break;
	    default:
		OUTCHAR('%');
		OUTCHAR('l');
		--fmt;		/* so %lz outputs %lz etc. */
		continue;
	    }
	    break;
	case 'd':
	    i = va_arg(args, int);
	    if (i < 0) {
		neg = 1;
		val = -i;
	    } else
		val = i;
	    base = 10;
	    break;
	case 'u':
	    val = va_arg(args, unsigned int);
	    base = 10;
	    break;
	case 'o':
	    val = va_arg(args, unsigned int);
	    base = 8;
	    break;
	case 'x':
	case 'X':
	    val = va_arg(args, unsigned int);
	    base = 16;
	    break;
	case 'p':
	    val = (unsigned long) va_arg(args, void *);
	    base = 16;
	    neg = 2;
	    break;
	case 's':
	    str = va_arg(args, char *);
	    break;
	case 'c':
	    num[0] = va_arg(args, int);
	    num[1] = 0;
	    str = num;
	    break;
	case 'm':
	    str = strerror(errno);
	    break;
	case 'I':
	    ip = va_arg(args, u_int32_t);
	    ip = ntohl(ip);
	    slprintf(num, sizeof(num), "%d.%d.%d.%d", (ip >> 24) & 0xff,
		     (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
	    str = num;
	    break;
	case 't':
	    time(&t);
	    str = ctime(&t);
	    str += 4;		/* chop off the day name */
	    str[15] = 0;	/* chop off year and newline */
	    break;
	case 'v':		/* "visible" string */
	case 'q':		/* quoted string */
	    quoted = c == 'q';
	    p = va_arg(args, unsigned char *);
	    if (p == NULL)
		    p = (unsigned char *)"<NULL>";
	    if (fillch == '0' && prec >= 0) {
		n = prec;
	    } else {
		n = strlen((char *)p);
		if (prec >= 0 && n > prec)
		    n = prec;
	    }
	    while (n > 0 && buflen > 0) {
		c = *p++;
		--n;
		if (!quoted && c >= 0x80) {
		    OUTCHAR('M');
		    OUTCHAR('-');
		    c -= 0x80;
		}
		if (quoted && (c == '"' || c == '\\'))
		    OUTCHAR('\\');
		if (c < 0x20 || (0x7f <= c && c < 0xa0)) {
		    if (quoted) {
			OUTCHAR('\\');
			switch (c) {
			case '\t':	OUTCHAR('t');	break;
			case '\n':	OUTCHAR('n');	break;
			case '\b':	OUTCHAR('b');	break;
			case '\f':	OUTCHAR('f');	break;
			default:
			    OUTCHAR('x');
			    OUTCHAR(hexchars[c >> 4]);
			    OUTCHAR(hexchars[c & 0xf]);
			}
		    } else {
			if (c == '\t')
			    OUTCHAR(c);
			else {
			    OUTCHAR('^');
			    OUTCHAR(c ^ 0x40);
			}
		    }
		} else
		    OUTCHAR(c);
	    }
	    continue;
	case 'P':		/* print PPP packet */
	    bufinfo.ptr = buf;
	    bufinfo.len = buflen + 1;
	    p = va_arg(args, unsigned char *);
	    n = va_arg(args, int);
	    format_packet(p, n, vslp_printer, &bufinfo);
	    buf = bufinfo.ptr;
	    buflen = bufinfo.len - 1;
	    continue;
	case 'B':
	    p = va_arg(args, unsigned char *);
	    for (n = prec; n > 0; --n) {
		c = *p++;
		if (fillch == ' ')
		    OUTCHAR(' ');
		OUTCHAR(hexchars[(c >> 4) & 0xf]);
		OUTCHAR(hexchars[c & 0xf]);
	    }
	    continue;
	default:
	    *buf++ = '%';
	    if (c != '%')
		--fmt;		/* so %z outputs %z etc. */
	    --buflen;
	    continue;
	}
	if (base != 0) {
	    str = num + sizeof(num);
	    *--str = 0;
	    while (str > num + neg) {
		*--str = hexchars[val % base];
		val = val / base;
		if (--prec <= 0 && val == 0)
		    break;
	    }
	    switch (neg) {
	    case 1:
		*--str = '-';
		break;
	    case 2:
		*--str = 'x';
		*--str = '0';
		break;
	    }
	    len = num + sizeof(num) - 1 - str;
	} else {
	    len = strlen(str);
	    if (prec >= 0 && len > prec)
		len = prec;
	}
	if (width > 0) {
	    if (width > buflen)
		width = buflen;
	    if ((n = width - len) > 0) {
		buflen -= n;
		for (; n > 0; --n)
		    *buf++ = fillch;
	    }
	}
	if (len > buflen)
	    len = buflen;
	memcpy(buf, str, len);
	buf += len;
	buflen -= len;
    }
    *buf = 0;
    return buf - buf0;
}

/*
 * vslp_printer - used in processing a %P format
 */
static void
vslp_printer __V((void *arg, char *fmt, ...))
{
    int n;
    va_list pvar;
    struct buffer_info *bi;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    void *arg;
    char *fmt;
    va_start(pvar);
    arg = va_arg(pvar, void *);
    fmt = va_arg(pvar, char *);
#endif

    bi = (struct buffer_info *) arg;
    n = vslprintf(bi->ptr, bi->len, fmt, pvar);
    va_end(pvar);

    bi->ptr += n;
    bi->len -= n;
}

#ifdef unused
/*
 * log_packet - format a packet and log it.
 */

void
log_packet(p, len, prefix, level)
    u_char *p;
    int len;
    char *prefix;
    int level;
{
	init_pr_log(prefix, level);
	format_packet(p, len, pr_log, &level);
	end_pr_log();
}
#endif /* unused */

/*
 * format_packet - make a readable representation of a packet,
 * calling `printer(arg, format, ...)' to output it.
 */
static void
format_packet(p, len, printer, arg)
    u_char *p;
    int len;
    printer_func printer;
    void *arg;
{
    int i, n;
    u_short proto;
    struct protent *protp;

    if (len >= PPP_HDRLEN && p[0] == PPP_ALLSTATIONS && p[1] == PPP_UI) {
	p += 2;
	GETSHORT(proto, p);
	len -= PPP_HDRLEN;
	for (i = 0; (protp = protocols[i]) != NULL; ++i)
	    if (proto == protp->protocol)
		break;
	if (protp != NULL) {
	    printer(arg, "[%s", protp->name);
	    n = (*protp->printpkt)(p, len, printer, arg);
	    printer(arg, "]");
	    p += n;
	    len -= n;
	} else {
	    for (i = 0; (protp = protocols[i]) != NULL; ++i)
		if (proto == (protp->protocol & ~0x8000))
		    break;
	    if (protp != 0 && protp->data_name != 0) {
		printer(arg, "[%s data]", protp->data_name);
		if (len > 8)
		    printer(arg, "%.8B ...", p);
		else
		    printer(arg, "%.*B", len, p);
		len = 0;
	    } else
		printer(arg, "[proto=0x%x]", proto);
	}
    }

    if (len > 32)
	printer(arg, "%.32B ...", p);
    else
	printer(arg, "%.*B", len, p);
}

/*
 * init_pr_log, end_pr_log - initialize and finish use of pr_log.
 */

static char line[256];		/* line to be logged accumulated here */
static char *linep;		/* current pointer within line */
static int llevel;		/* level for logging */

void
init_pr_log(prefix, level)
     const char *prefix;
     int level;
{
	linep = line;
	if (prefix != NULL) {
		strlcpy(line, prefix, sizeof(line));
		linep = line + strlen(line);
	}
	llevel = level;
}

void
end_pr_log()
{
	if (linep != line) {
		*linep = 0;
		log_write(llevel, line);
	}
}

/*
 * pr_log - printer routine for outputting to syslog
 */
void
pr_log __V((void *arg, char *fmt, ...))
{
	int l, n;
	va_list pvar;
	char *p, *eol;
	char buf[256];

#if defined(__STDC__)
	va_start(pvar, fmt);
#else
	void *arg;
	char *fmt;
	va_start(pvar);
	arg = va_arg(pvar, void *);
	fmt = va_arg(pvar, char *);
#endif

	n = vslprintf(buf, sizeof(buf), fmt, pvar);
	va_end(pvar);

	p = buf;
	eol = strchr(buf, '\n');
	if (linep != line) {
		l = (eol == NULL)? n: eol - buf;
		if (linep + l < line + sizeof(line)) {
			if (l > 0) {
				memcpy(linep, buf, l);
				linep += l;
			}
			if (eol == NULL)
				return;
			p = eol + 1;
			eol = strchr(p, '\n');
		}
		*linep = 0;
		log_write(llevel, line);
		linep = line;
	}

	while (eol != NULL) {
		*eol = 0;
		log_write(llevel, p);
		p = eol + 1;
		eol = strchr(p, '\n');
	}

	/* assumes sizeof(buf) <= sizeof(line) */
	l = buf + n - p;
	if (l > 0) {
		memcpy(line, p, n);
		linep = line + l;
	}
}

/*
 * print_string - print a readable representation of a string using
 * printer.
 */
void
print_string(p, len, printer, arg)
    char *p;
    int len;
    printer_func printer;
    void *arg;
{
    int c;

    printer(arg, "\"");
    for (; len > 0; --len) {
	c = *p++;
	if (' ' <= c && c <= '~') {
	    if (c == '\\' || c == '"')
		printer(arg, "\\");
	    printer(arg, "%c", c);
	} else {
	    switch (c) {
	    case '\n':
		printer(arg, "\\n");
		break;
	    case '\r':
		printer(arg, "\\r");
		break;
	    case '\t':
		printer(arg, "\\t");
		break;
	    default:
		printer(arg, "\\%.3o", c);
	    }
	}
    }
    printer(arg, "\"");
}

/*
 * logit - does the hard work for fatal et al.
 */
static void
logit(level, fmt, args)
    int level;
    char *fmt;
    va_list args;
{
    char buf[1024];

    vslprintf(buf, sizeof(buf), fmt, args);
    log_write(level, buf);
}

static void
log_write(level, buf)
    int level;
    char *buf;
{
    syslog(level, "%s", buf);
    if (log_to_fd >= 0 && (level != LOG_DEBUG || debug)) {
	int n = strlen(buf);

	if (n > 0 && buf[n-1] == '\n')
	    --n;
	if (write(log_to_fd, buf, n) != n
	    || write(log_to_fd, "\n", 1) != 1)
	    log_to_fd = -1;
    }
}

/*
 * fatal - log an error message and die horribly.
 */
void
fatal __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_ERR, fmt, pvar);
    va_end(pvar);

    die(1);			/* as promised */
}

/*
 * error - log an error message.
 */
void
error __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_ERR, fmt, pvar);
    va_end(pvar);
    ++error_count;
}

/*
 * warn - log a warning message.
 */
void
warn __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_WARNING, fmt, pvar);
    va_end(pvar);
}

/*
 * notice - log a notice-level message.
 */
void
notice __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_NOTICE, fmt, pvar);
    va_end(pvar);
}

/*
 * info - log an informational message.
 */
void
info __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_INFO, fmt, pvar);
    va_end(pvar);
}

/*
 * dbglog - log a debug message.
 */
void
dbglog __V((char *fmt, ...))
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    logit(LOG_DEBUG, fmt, pvar);
    va_end(pvar);
}

/*
 * dump_packet - print out a packet in readable form if it is interesting.
 * Assumes len >= PPP_HDRLEN.
 */
void
dump_packet(const char *tag, unsigned char *p, int len)
{
    int proto;

    if (!debug)
	return;

    /*
     * don't print LCP echo request/reply packets if debug <= 1
     * and the link is up.
     */
    proto = (p[2] << 8) + p[3];
    if (debug <= 1 && unsuccess == 0 && proto == PPP_LCP
	&& len >= PPP_HDRLEN + HEADERLEN) {
	unsigned char *lcp = p + PPP_HDRLEN;
	int l = (lcp[2] << 8) + lcp[3];

	if ((lcp[0] == ECHOREQ || lcp[0] == ECHOREP)
	    && l >= HEADERLEN && l <= len - PPP_HDRLEN)
	    return;
    }

    dbglog("%s %P", tag, p, len);
}

/*
 * complete_read - read a full `count' bytes from fd,
 * unless end-of-file or an error other than EINTR is encountered.
 */
ssize_t
complete_read(int fd, void *buf, size_t count)
{
	size_t done;
	ssize_t nb;
	char *ptr = buf;

	for (done = 0; done < count; ) {
		nb = read(fd, ptr, count - done);
		if (nb < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (nb == 0)
			break;
		done += nb;
		ptr += nb;
	}
	return done;
}

/* Procedures for locking the serial device using a lock file. */
#ifndef LOCK_DIR
#ifdef __linux__
#define LOCK_DIR	"/var/lock"
#else
#ifdef SVR4
#define LOCK_DIR	"/var/spool/locks"
#else
#define LOCK_DIR	"/var/spool/lock"
#endif
#endif
#endif /* LOCK_DIR */

static char lock_file[MAXPATHLEN];

/*
 * lock - create a lock file for the named device
 */
int
lock(dev)
    char *dev;
{
#ifdef LOCKLIB
    int result;

    result = mklock (dev, (void *) 0);
    if (result == 0) {
	strlcpy(lock_file, dev, sizeof(lock_file));
	return 0;
    }

    if (result > 0)
        notice("Device %s is locked by pid %d", dev, result);
    else
	error("Can't create lock file %s", lock_file);
    return -1;

#else /* LOCKLIB */

    char lock_buffer[12];
    int fd, pid, n;

#ifdef SVR4
    struct stat sbuf;

    if (stat(dev, &sbuf) < 0) {
	error("Can't get device number for %s: %m", dev);
	return -1;
    }
    if ((sbuf.st_mode & S_IFMT) != S_IFCHR) {
	error("Can't lock %s: not a character device", dev);
	return -1;
    }
    slprintf(lock_file, sizeof(lock_file), "%s/LK.%03d.%03d.%03d",
	     LOCK_DIR, major(sbuf.st_dev),
	     major(sbuf.st_rdev), minor(sbuf.st_rdev));
#else
    char *p;
    char lockdev[MAXPATHLEN];

    if ((p = strstr(dev, "dev/")) != NULL) {
	dev = p + 4;
	strncpy(lockdev, dev, MAXPATHLEN-1);
	lockdev[MAXPATHLEN-1] = 0;
	while ((p = strrchr(lockdev, '/')) != NULL) {
	    *p = '_';
	}
	dev = lockdev;
    } else
	if ((p = strrchr(dev, '/')) != NULL)
	    dev = p + 1;

    slprintf(lock_file, sizeof(lock_file), "%s/LCK..%s", LOCK_DIR, dev);
#endif

    while ((fd = open(lock_file, O_EXCL | O_CREAT | O_RDWR, 0644)) < 0) {
	if (errno != EEXIST) {
	    error("Can't create lock file %s: %m", lock_file);
	    break;
	}

	/* Read the lock file to find out who has the device locked. */
	fd = open(lock_file, O_RDONLY, 0);
	if (fd < 0) {
	    if (errno == ENOENT) /* This is just a timing problem. */
		continue;
	    error("Can't open existing lock file %s: %m", lock_file);
	    break;
	}
#ifndef LOCK_BINARY
	n = read(fd, lock_buffer, 11);
#else
	n = read(fd, &pid, sizeof(pid));
#endif /* LOCK_BINARY */
	close(fd);
	fd = -1;
	if (n <= 0) {
	    error("Can't read pid from lock file %s", lock_file);
	    break;
	}

	/* See if the process still exists. */
#ifndef LOCK_BINARY
	lock_buffer[n] = 0;
	pid = atoi(lock_buffer);
#endif /* LOCK_BINARY */
	if (pid == getpid())
	    return 1;		/* somebody else locked it for us */
	if (pid == 0
	    || (kill(pid, 0) == -1 && errno == ESRCH)) {
	    if (unlink (lock_file) == 0) {
		notice("Removed stale lock on %s (pid %d)", dev, pid);
		continue;
	    }
	    warn("Couldn't remove stale lock on %s", dev);
	} else
	    notice("Device %s is locked by pid %d", dev, pid);
	break;
    }

    if (fd < 0) {
	lock_file[0] = 0;
	return -1;
    }

    pid = getpid();
#ifndef LOCK_BINARY
    slprintf(lock_buffer, sizeof(lock_buffer), "%10d\n", pid);
    write (fd, lock_buffer, 11);
#else
    write(fd, &pid, sizeof (pid));
#endif
    close(fd);
    return 0;

#endif
}

/*
 * relock - called to update our lockfile when we are about to detach,
 * thus changing our pid (we fork, the child carries on, and the parent dies).
 * Note that this is called by the parent, with pid equal to the pid
 * of the child.  This avoids a potential race which would exist if
 * we had the child rewrite the lockfile (the parent might die first,
 * and another process could think the lock was stale if it checked
 * between when the parent died and the child rewrote the lockfile).
 */
int
relock(pid)
    int pid;
{
#ifdef LOCKLIB
    /* XXX is there a way to do this? */
    return -1;
#else /* LOCKLIB */

    int fd;
    char lock_buffer[12];

    if (lock_file[0] == 0)
	return -1;
    fd = open(lock_file, O_WRONLY, 0);
    if (fd < 0) {
	error("Couldn't reopen lock file %s: %m", lock_file);
	lock_file[0] = 0;
	return -1;
    }

#ifndef LOCK_BINARY
    slprintf(lock_buffer, sizeof(lock_buffer), "%10d\n", pid);
    write (fd, lock_buffer, 11);
#else
    write(fd, &pid, sizeof(pid));
#endif /* LOCK_BINARY */
    close(fd);
    return 0;

#endif /* LOCKLIB */
}

/*
 * unlock - remove our lockfile
 */
void
unlock()
{
    if (lock_file[0]) {
#ifdef LOCKLIB
	(void) rmlock(lock_file, (void *) 0);
#else
	unlink(lock_file);
#endif
	lock_file[0] = 0;
    }
}

/* JYWeng 20031216: add to wanstatus.log */
static void
create_status(statusname, status)
    char *statusname;
    char *status;
{
    FILE *fp;

    if ((fp = fopen(statusname, "w")) != NULL) {
	fprintf(fp, "%s\n", status ? : "");
	(void) fclose(fp);
    } else
	error("Failed to create status file %s: %m", statusname);
}

void
save_wanstatus(status)
    char *status;
{
    char statusname[MAXPATHLEN];

    /* only save with status description */
    if (!status || status[0] == 0)
	return;

    /* ifname.status is unused
    slprintf(statusname, sizeof(statusname), "%s%s.status",
	     _PATH_VARRUN, ifname);
    create_status(statusname, status);
    */

    if (linkname[0]) {
	slprintf(statusname, sizeof(statusname), "%sppp-%s.status",
		 _PATH_VARRUN, linkname);
	create_status(statusname, status);
    }
}
