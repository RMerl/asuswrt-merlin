
/* Jim - A small embeddable Tcl interpreter
 *
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2005 Clemens Hintze <c.hintze@gmx.net>
 * Copyright 2005 patthoyts - Pat Thoyts <patthoyts@users.sf.net>
 * Copyright 2008 oharboe - Øyvind Harboe - oyvind.harboe@zylin.com
 * Copyright 2008 Andrew Lunn <andrew@lunn.ch>
 * Copyright 2008 Duane Ellis <openocd@duaneellis.com>
 * Copyright 2008 Uwe Klein <uklein@klein-messgeraete.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE JIM TCL PROJECT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * JIM TCL PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the Jim Tcl Project.
 **/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "jim.h"
#include "jimautoconf.h"

#if defined(HAVE_SYS_SOCKET_H) && defined(HAVE_SELECT) && defined(HAVE_NETINET_IN_H) && defined(HAVE_NETDB_H) && defined(HAVE_ARPA_INET_H)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#else
#define JIM_ANSIC
#endif

#include "jim-eventloop.h"
#include "jim-subcmd.h"

#define AIO_CMD_LEN 32      /* e.g. aio.handleXXXXXX */
#define AIO_BUF_LEN 256     /* Can keep this small and rely on stdio buffering */

#define AIO_KEEPOPEN 1

#if defined(JIM_IPV6)
#define IPV6 1
#else
#define IPV6 0
#ifndef PF_INET6
#define PF_INET6 0
#endif
#endif

#if !defined(JIM_ANSIC) && !defined(JIM_BOOTSTRAP)
union sockaddr_any {
    struct sockaddr sa;
    struct sockaddr_in sin;
#if IPV6
    struct sockaddr_in6 sin6;
#endif
};

#ifndef HAVE_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, int size)
{
    if (af != PF_INET) {
        return NULL;
    }
    snprintf(dst, size, "%s", inet_ntoa(((struct sockaddr_in *)src)->sin_addr));
    return dst;
}
#endif
#endif /* JIM_BOOTSTRAP */

typedef struct AioFile
{
    FILE *fp;
    Jim_Obj *filename;
    int type;
    int OpenFlags;              /* AIO_KEEPOPEN? keep FILE* */
    int fd;
#ifdef O_NDELAY
    int flags;
#endif
    Jim_Obj *rEvent;
    Jim_Obj *wEvent;
    Jim_Obj *eEvent;
    int addr_family;
} AioFile;

static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);
static int JimMakeChannel(Jim_Interp *interp, FILE *fh, int fd, Jim_Obj *filename,
    const char *hdlfmt, int family, const char *mode);

#if !defined(JIM_ANSIC) && !defined(JIM_BOOTSTRAP)
static int JimParseIPv6Address(Jim_Interp *interp, const char *hostport, union sockaddr_any *sa, int *salen)
{
#if IPV6
    /*
     * An IPv6 addr/port looks like:
     *   [::1]
     *   [::1]:2000
     *   [fe80::223:6cff:fe95:bdc0%en1]:2000
     *   [::]:2000
     *   2000
     *
     *   Note that the "any" address is ::, which is the same as when no address is specified.
     */
     char *sthost = NULL;
     const char *stport;
     int ret = JIM_OK;
     struct addrinfo req;
     struct addrinfo *ai;

    stport = strrchr(hostport, ':');
    if (!stport) {
        /* No : so, the whole thing is the port */
        stport = hostport;
        hostport = "::";
        sthost = Jim_StrDup(hostport);
    }
    else {
        stport++;
    }

    if (*hostport == '[') {
        /* This is a numeric ipv6 address */
        char *pt = strchr(++hostport, ']');
        if (pt) {
            sthost = Jim_StrDupLen(hostport, pt - hostport);
        }
    }

    if (!sthost) {
        sthost = Jim_StrDupLen(hostport, stport - hostport - 1);
    }

    memset(&req, '\0', sizeof(req));
    req.ai_family = PF_INET6;

    if (getaddrinfo(sthost, NULL, &req, &ai)) {
        Jim_SetResultFormatted(interp, "Not a valid address: %s", hostport);
        ret = JIM_ERR;
    }
    else {
        memcpy(&sa->sin, ai->ai_addr, ai->ai_addrlen);
        *salen = ai->ai_addrlen;

        sa->sin.sin_port = htons(atoi(stport));

        freeaddrinfo(ai);
    }
    Jim_Free(sthost);

    return ret;
#else
    Jim_SetResultString(interp, "ipv6 not supported", -1);
    return JIM_ERR;
#endif
}

static int JimParseIpAddress(Jim_Interp *interp, const char *hostport, union sockaddr_any *sa, int *salen)
{
    /* An IPv4 addr/port looks like:
     *   192.168.1.5
     *   192.168.1.5:2000
     *   2000
     *
     * If the address is missing, INADDR_ANY is used.
     * If the port is missing, 0 is used (only useful for server sockets).
     */
    char *sthost = NULL;
    const char *stport;
    int ret = JIM_OK;

    stport = strrchr(hostport, ':');
    if (!stport) {
        /* No : so, the whole thing is the port */
        stport = hostport;
        sthost = Jim_StrDup("0.0.0.0");
    }
    else {
        sthost = Jim_StrDupLen(hostport, stport - hostport);
        stport++;
    }

    {
#ifdef HAVE_GETADDRINFO
        struct addrinfo req;
        struct addrinfo *ai;
        memset(&req, '\0', sizeof(req));
        req.ai_family = PF_INET;

        if (getaddrinfo(sthost, NULL, &req, &ai)) {
            ret = JIM_ERR;
        }
        else {
            memcpy(&sa->sin, ai->ai_addr, ai->ai_addrlen);
            *salen = ai->ai_addrlen;
            freeaddrinfo(ai);
        }
#else
        struct hostent *he;

        ret = JIM_ERR;

        if ((he = gethostbyname(sthost)) != NULL) {
            if (he->h_length == sizeof(sa->sin.sin_addr)) {
                *salen = sizeof(sa->sin);
                sa->sin.sin_family= he->h_addrtype;
                memcpy(&sa->sin.sin_addr, he->h_addr, he->h_length); /* set address */
                ret = JIM_OK;
            }
        }
#endif

        sa->sin.sin_port = htons(atoi(stport));
    }
    Jim_Free(sthost);

    if (ret != JIM_OK) {
        Jim_SetResultFormatted(interp, "Not a valid address: %s", hostport);
    }

    return ret;
}

#ifdef HAVE_SYS_UN_H
static int JimParseDomainAddress(Jim_Interp *interp, const char *path, struct sockaddr_un *sa)
{
    sa->sun_family = PF_UNIX;
    snprintf(sa->sun_path, sizeof(sa->sun_path), "%s", path);

    return JIM_OK;
}
#endif
#endif /* JIM_BOOTSTRAP */

static void JimAioSetError(Jim_Interp *interp, Jim_Obj *name)
{
    if (name) {
        Jim_SetResultFormatted(interp, "%#s: %s", name, strerror(errno));
    }
    else {
        Jim_SetResultString(interp, strerror(errno), -1);
    }
}

static void JimAioDelProc(Jim_Interp *interp, void *privData)
{
    AioFile *af = privData;

    JIM_NOTUSED(interp);

    Jim_DecrRefCount(interp, af->filename);

    if (!(af->OpenFlags & AIO_KEEPOPEN)) {
        fclose(af->fp);
    }
#ifdef jim_ext_eventloop
    /* remove existing EventHandlers */
    if (af->rEvent) {
        Jim_DeleteFileHandler(interp, af->fp);
    }
    if (af->wEvent) {
        Jim_DeleteFileHandler(interp, af->fp);
    }
    if (af->eEvent) {
        Jim_DeleteFileHandler(interp, af->fp);
    }
#endif
    Jim_Free(af);
}

static int aio_cmd_read(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char buf[AIO_BUF_LEN];
    Jim_Obj *objPtr;
    int nonewline = 0;
    int neededLen = -1;         /* -1 is "read as much as possible" */

    if (argc && Jim_CompareStringImmediate(interp, argv[0], "-nonewline")) {
        nonewline = 1;
        argv++;
        argc--;
    }
    if (argc == 1) {
        jim_wide wideValue;

        if (Jim_GetWide(interp, argv[0], &wideValue) != JIM_OK)
            return JIM_ERR;
        if (wideValue < 0) {
            Jim_SetResultString(interp, "invalid parameter: negative len", -1);
            return JIM_ERR;
        }
        neededLen = (int)wideValue;
    }
    else if (argc) {
        return -1;
    }
    objPtr = Jim_NewStringObj(interp, NULL, 0);
    while (neededLen != 0) {
        int retval;
        int readlen;

        if (neededLen == -1) {
            readlen = AIO_BUF_LEN;
        }
        else {
            readlen = (neededLen > AIO_BUF_LEN ? AIO_BUF_LEN : neededLen);
        }
        retval = fread(buf, 1, readlen, af->fp);
        if (retval > 0) {
            Jim_AppendString(interp, objPtr, buf, retval);
            if (neededLen != -1) {
                neededLen -= retval;
            }
        }
        if (retval != readlen)
            break;
    }
    /* Check for error conditions */
    if (ferror(af->fp)) {
        clearerr(af->fp);
        /* eof and EAGAIN are not error conditions */
        if (!feof(af->fp) && errno != EAGAIN) {
            /* I/O error */
            Jim_FreeNewObj(interp, objPtr);
            JimAioSetError(interp, af->filename);
            return JIM_ERR;
        }
    }
    if (nonewline) {
        int len;
        const char *s = Jim_GetString(objPtr, &len);

        if (len > 0 && s[len - 1] == '\n') {
            objPtr->length--;
            objPtr->bytes[objPtr->length] = '\0';
        }
    }
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

static int aio_cmd_copy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    long count = 0;
    long maxlen = LONG_MAX;
    FILE *outfh = Jim_AioFilehandle(interp, argv[0]);

    if (outfh == NULL) {
        return JIM_ERR;
    }

    if (argc == 2) {
        if (Jim_GetLong(interp, argv[1], &maxlen) != JIM_OK) {
            return JIM_ERR;
        }
    }

    while (count < maxlen) {
        int ch = fgetc(af->fp);

        if (ch == EOF || fputc(ch, outfh) == EOF) {
            break;
        }
        count++;
    }

    if (ferror(af->fp)) {
        Jim_SetResultFormatted(interp, "error while reading: %s", strerror(errno));
        clearerr(af->fp);
        return JIM_ERR;
    }

    if (ferror(outfh)) {
        Jim_SetResultFormatted(interp, "error while writing: %s", strerror(errno));
        clearerr(outfh);
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, count);

    return JIM_OK;
}

static int aio_cmd_gets(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char buf[AIO_BUF_LEN];
    Jim_Obj *objPtr;
    int len;

    errno = 0;

    objPtr = Jim_NewStringObj(interp, NULL, 0);
    while (1) {
        buf[AIO_BUF_LEN - 1] = '_';
        if (fgets(buf, AIO_BUF_LEN, af->fp) == NULL)
            break;

        if (buf[AIO_BUF_LEN - 1] == '\0' && buf[AIO_BUF_LEN - 2] != '\n') {
            Jim_AppendString(interp, objPtr, buf, AIO_BUF_LEN - 1);
        }
        else {
            len = strlen(buf);

            if (len && (buf[len - 1] == '\n')) {
                /* strip "\n" */
                len--;
            }

            Jim_AppendString(interp, objPtr, buf, len);
            break;
        }
    }
    if (ferror(af->fp) && errno != EAGAIN && errno != EINTR) {
        /* I/O error */
        Jim_FreeNewObj(interp, objPtr);
        JimAioSetError(interp, af->filename);
        clearerr(af->fp);
        return JIM_ERR;
    }

    if (argc) {
        if (Jim_SetVariable(interp, argv[0], objPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, objPtr);
            return JIM_ERR;
        }

        len = Jim_Length(objPtr);

        if (len == 0 && feof(af->fp)) {
            /* On EOF returns -1 if varName was specified */
            len = -1;
        }
        Jim_SetResultInt(interp, len);
    }
    else {
        Jim_SetResult(interp, objPtr);
    }
    return JIM_OK;
}

static int aio_cmd_puts(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int wlen;
    const char *wdata;
    Jim_Obj *strObj;

    if (argc == 2) {
        if (!Jim_CompareStringImmediate(interp, argv[0], "-nonewline")) {
            return -1;
        }
        strObj = argv[1];
    }
    else {
        strObj = argv[0];
    }

    wdata = Jim_GetString(strObj, &wlen);
    if (fwrite(wdata, 1, wlen, af->fp) == (unsigned)wlen) {
        if (argc == 2 || putc('\n', af->fp) != EOF) {
            return JIM_OK;
        }
    }
    JimAioSetError(interp, af->filename);
    return JIM_ERR;
}

#if !defined(JIM_ANSIC) && !defined(JIM_BOOTSTRAP)
static int aio_cmd_recvfrom(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char *buf;
    union sockaddr_any sa;
    long len;
    socklen_t salen = sizeof(sa);
    int rlen;

    if (Jim_GetLong(interp, argv[0], &len) != JIM_OK) {
        return JIM_ERR;
    }

    buf = Jim_Alloc(len + 1);

    rlen = recvfrom(fileno(af->fp), buf, len, 0, &sa.sa, &salen);
    if (rlen < 0) {
        Jim_Free(buf);
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    buf[rlen] = 0;
    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, rlen));

    if (argc > 1) {
        /* INET6_ADDRSTRLEN is 46. Add some for [] and port */
        char addrbuf[60];

#if IPV6
        if (sa.sa.sa_family == PF_INET6) {
            addrbuf[0] = '[';
            /* Allow 9 for []:65535\0 */
            inet_ntop(sa.sa.sa_family, &sa.sin6.sin6_addr, addrbuf + 1, sizeof(addrbuf) - 9);
            snprintf(addrbuf + strlen(addrbuf), 8, "]:%d", ntohs(sa.sin.sin_port));
        }
        else
#endif
        {
            /* Allow 7 for :65535\0 */
            inet_ntop(sa.sa.sa_family, &sa.sin.sin_addr, addrbuf, sizeof(addrbuf) - 7);
            snprintf(addrbuf + strlen(addrbuf), 7, ":%d", ntohs(sa.sin.sin_port));
        }

        if (Jim_SetVariable(interp, argv[1], Jim_NewStringObj(interp, addrbuf, -1)) != JIM_OK) {
            return JIM_ERR;
        }
    }

    return JIM_OK;
}


static int aio_cmd_sendto(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int wlen;
    int len;
    const char *wdata;
    union sockaddr_any sa;
    const char *addr = Jim_String(argv[1]);
    int salen;

    if (IPV6 && af->addr_family == PF_INET6) {
        if (JimParseIPv6Address(interp, addr, &sa, &salen) != JIM_OK) {
            return JIM_ERR;
        }
    }
    else if (JimParseIpAddress(interp, addr, &sa, &salen) != JIM_OK) {
        return JIM_ERR;
    }
    wdata = Jim_GetString(argv[0], &wlen);

    /* Note that we don't validate the socket type. Rely on sendto() failing if appropriate */
    len = sendto(fileno(af->fp), wdata, wlen, 0, &sa.sa, salen);
    if (len < 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, len);
    return JIM_OK;
}

static int aio_cmd_accept(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int sock;
    union sockaddr_any sa;
    socklen_t addrlen = sizeof(sa);

    sock = accept(af->fd, &sa.sa, &addrlen);
    if (sock < 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }

    /* Create the file command */
    return JimMakeChannel(interp, NULL, sock, Jim_NewStringObj(interp, "accept", -1),
        "aio.sockstream%ld", af->addr_family, "r+");
}

static int aio_cmd_listen(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    long backlog;

    if (Jim_GetLong(interp, argv[0], &backlog) != JIM_OK) {
        return JIM_ERR;
    }

    if (listen(af->fd, backlog)) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }

    return JIM_OK;
}
#endif /* JIM_BOOTSTRAP */

static int aio_cmd_flush(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    if (fflush(af->fp) == EOF) {
        JimAioSetError(interp, af->filename);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int aio_cmd_eof(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResultInt(interp, feof(af->fp));
    return JIM_OK;
}

static int aio_cmd_close(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_DeleteCommand(interp, Jim_String(argv[0]));
    return JIM_OK;
}

static int aio_cmd_seek(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int orig = SEEK_SET;
    long offset;

    if (argc == 2) {
        if (Jim_CompareStringImmediate(interp, argv[1], "start"))
            orig = SEEK_SET;
        else if (Jim_CompareStringImmediate(interp, argv[1], "current"))
            orig = SEEK_CUR;
        else if (Jim_CompareStringImmediate(interp, argv[1], "end"))
            orig = SEEK_END;
        else {
            return -1;
        }
    }
    if (Jim_GetLong(interp, argv[0], &offset) != JIM_OK) {
        return JIM_ERR;
    }
    if (fseek(af->fp, offset, orig) == -1) {
        JimAioSetError(interp, af->filename);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int aio_cmd_tell(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResultInt(interp, ftell(af->fp));
    return JIM_OK;
}

static int aio_cmd_filename(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResult(interp, af->filename);
    return JIM_OK;
}

#ifdef O_NDELAY
static int aio_cmd_ndelay(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    int fmode = af->flags;

    if (argc) {
        long nb;

        if (Jim_GetLong(interp, argv[0], &nb) != JIM_OK) {
            return JIM_ERR;
        }
        if (nb) {
            fmode |= O_NDELAY;
        }
        else {
            fmode &= ~O_NDELAY;
        }
        fcntl(af->fd, F_SETFL, fmode);
        af->flags = fmode;
    }
    Jim_SetResultInt(interp, (fmode & O_NONBLOCK) ? 1 : 0);
    return JIM_OK;
}
#endif

static int aio_cmd_buffering(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    static const char * const options[] = {
        "none",
        "line",
        "full",
        NULL
    };
    enum
    {
        OPT_NONE,
        OPT_LINE,
        OPT_FULL,
    };
    int option;

    if (Jim_GetEnum(interp, argv[0], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }
    switch (option) {
        case OPT_NONE:
            setvbuf(af->fp, NULL, _IONBF, 0);
            break;
        case OPT_LINE:
            setvbuf(af->fp, NULL, _IOLBF, BUFSIZ);
            break;
        case OPT_FULL:
            setvbuf(af->fp, NULL, _IOFBF, BUFSIZ);
            break;
    }
    return JIM_OK;
}

#ifdef jim_ext_eventloop
static void JimAioFileEventFinalizer(Jim_Interp *interp, void *clientData)
{
    Jim_Obj *objPtr = clientData;

    Jim_DecrRefCount(interp, objPtr);
}

static int JimAioFileEventHandler(Jim_Interp *interp, void *clientData, int mask)
{
    Jim_Obj *objPtr = clientData;

    return Jim_EvalObjBackground(interp, objPtr);
}

static int aio_eventinfo(Jim_Interp *interp, AioFile * af, unsigned mask, Jim_Obj **scriptHandlerObj,
    int argc, Jim_Obj * const *argv)
{
    int scriptlen = 0;

    if (argc == 0) {
        /* Return current script */
        if (*scriptHandlerObj) {
            Jim_SetResult(interp, *scriptHandlerObj);
        }
        return JIM_OK;
    }

    if (*scriptHandlerObj) {
        /* Delete old handler */
        Jim_DeleteFileHandler(interp, af->fp);
        *scriptHandlerObj = NULL;
    }

    /* Now possibly add the new script(s) */
    Jim_GetString(argv[0], &scriptlen);
    if (scriptlen == 0) {
        /* Empty script, so done */
        return JIM_OK;
    }

    /* A new script to add */
    Jim_IncrRefCount(argv[0]);
    *scriptHandlerObj = argv[0];

    Jim_CreateFileHandler(interp, af->fp, mask,
        JimAioFileEventHandler, *scriptHandlerObj, JimAioFileEventFinalizer);

    return JIM_OK;
}

static int aio_cmd_readable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_READABLE, &af->rEvent, argc, argv);
}

static int aio_cmd_writable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_WRITABLE, &af->wEvent, argc, argv);
}

static int aio_cmd_onexception(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_EXCEPTION, &af->wEvent, argc, argv);
}
#endif

static const jim_subcmd_type aio_command_table[] = {
    {   .cmd = "read",
        .args = "?-nonewline? ?len?",
        .function = aio_cmd_read,
        .minargs = 0,
        .maxargs = 2,
        .description = "Read and return bytes from the stream. To eof if no len."
    },
    {   .cmd = "copyto",
        .args = "handle ?size?",
        .function = aio_cmd_copy,
        .minargs = 1,
        .maxargs = 2,
        .description = "Copy up to 'size' bytes to the given filehandle, or to eof if no size."
    },
    {   .cmd = "gets",
        .args = "?var?",
        .function = aio_cmd_gets,
        .minargs = 0,
        .maxargs = 1,
        .description = "Read one line and return it or store it in the var"
    },
    {   .cmd = "puts",
        .args = "?-nonewline? str",
        .function = aio_cmd_puts,
        .minargs = 1,
        .maxargs = 2,
        .description = "Write the string, with newline unless -nonewline"
    },
#if !defined(JIM_ANSIC) && !defined(JIM_BOOTSTRAP)
    {   .cmd = "recvfrom",
        .args = "len ?addrvar?",
        .function = aio_cmd_recvfrom,
        .minargs = 1,
        .maxargs = 2,
        .description = "Receive up to 'len' bytes on the socket. Sets 'addrvar' with receive address, if set"
    },
    {   .cmd = "sendto",
        .args = "str address",
        .function = aio_cmd_sendto,
        .minargs = 2,
        .maxargs = 2,
        .description = "Send 'str' to the given address (dgram only)"
    },
    {   .cmd = "accept",
        .function = aio_cmd_accept,
        .description = "Server socket only: Accept a connection and return stream"
    },
    {   .cmd = "listen",
        .args = "backlog",
        .function = aio_cmd_listen,
        .minargs = 1,
        .maxargs = 1,
        .description = "Set the listen backlog for server socket"
    },
#endif /* JIM_BOOTSTRAP */
    {   .cmd = "flush",
        .function = aio_cmd_flush,
        .description = "Flush the stream"
    },
    {   .cmd = "eof",
        .function = aio_cmd_eof,
        .description = "Returns 1 if stream is at eof"
    },
    {   .cmd = "close",
        .flags = JIM_MODFLAG_FULLARGV,
        .function = aio_cmd_close,
        .description = "Closes the stream"
    },
    {   .cmd = "seek",
        .args = "offset ?start|current|end",
        .function = aio_cmd_seek,
        .minargs = 1,
        .maxargs = 2,
        .description = "Seeks in the stream (default 'current')"
    },
    {   .cmd = "tell",
        .function = aio_cmd_tell,
        .description = "Returns the current seek position"
    },
    {   .cmd = "filename",
        .function = aio_cmd_filename,
        .description = "Returns the original filename"
    },
#ifdef O_NDELAY
    {   .cmd = "ndelay",
        .args = "?0|1?",
        .function = aio_cmd_ndelay,
        .minargs = 0,
        .maxargs = 1,
        .description = "Set O_NDELAY (if arg). Returns current/new setting."
    },
#endif
    {   .cmd = "buffering",
        .args = "none|line|full",
        .function = aio_cmd_buffering,
        .minargs = 1,
        .maxargs = 1,
        .description = "Sets buffering"
    },
#ifdef jim_ext_eventloop
    {   .cmd = "readable",
        .args = "?readable-script?",
        .minargs = 0,
        .maxargs = 1,
        .function = aio_cmd_readable,
        .description = "Returns script, or invoke readable-script when readable, {} to remove",
    },
    {   .cmd = "writable",
        .args = "?writable-script?",
        .minargs = 0,
        .maxargs = 1,
        .function = aio_cmd_writable,
        .description = "Returns script, or invoke writable-script when writable, {} to remove",
    },
    {   .cmd = "onexception",
        .args = "?exception-script?",
        .minargs = 0,
        .maxargs = 1,
        .function = aio_cmd_onexception,
        .description = "Returns script, or invoke exception-script when oob data, {} to remove",
    },
#endif
    { 0 }
};

static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, aio_command_table, argc, argv), argc, argv);
}

static int JimAioOpenCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    FILE *fp;
    const char *hdlfmt;
    const char *mode;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "filename ?mode?");
        return JIM_ERR;
    }

    mode = (argc == 3) ? Jim_String(argv[2]) : "r";
    hdlfmt = Jim_String(argv[1]);
    if (Jim_CompareStringImmediate(interp, argv[1], "stdin")) {
        fp = stdin;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "stdout")) {
        fp = stdout;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "stderr")) {
        fp = stderr;
    }
    else {
        const char *filename = Jim_String(argv[1]);


#ifdef jim_ext_tclcompat
        /* If the filename starts with '|', use popen instead */
        if (*filename == '|') {
            Jim_Obj *evalObj[3];

            evalObj[0] = Jim_NewStringObj(interp, "popen", -1);
            evalObj[1] = Jim_NewStringObj(interp, filename + 1, -1);
            evalObj[2] = Jim_NewStringObj(interp, mode, -1);

            return Jim_EvalObjVector(interp, 3, evalObj);
        }
#endif
        hdlfmt = "aio.handle%ld";
        fp = NULL;
    }

    /* Create the file command */
    return JimMakeChannel(interp, fp, -1, argv[1], hdlfmt, 0, mode);
}

/**
 * Creates a channel for fh/fd/filename.
 *
 * If fh is not NULL, uses that as the channel (and set AIO_KEEPOPEN).
 * Otherwise, if fd is >= 0, uses that as the chanel.
 * Otherwise opens 'filename' with mode 'mode'.
 *
 * hdlfmt is a sprintf format for the filehandle. Anything with %ld at the end will do.
 * mode is used for open or fdopen.
 *
 * Creates the command and sets the name as the current result.
 */
static int JimMakeChannel(Jim_Interp *interp, FILE *fh, int fd, Jim_Obj *filename,
    const char *hdlfmt, int family, const char *mode)
{
    AioFile *af;
    char buf[AIO_CMD_LEN];
    int OpenFlags = 0;

    Jim_IncrRefCount(filename);

    if (fh == NULL) {
        if (fd < 0) {
            fh = fopen(Jim_String(filename), mode);
        }
        else {
            fh = fdopen(fd, mode);
        }
    }
    else {
        OpenFlags = AIO_KEEPOPEN;
    }

    if (fh == NULL) {
        JimAioSetError(interp, filename);
        close(fd);
        Jim_DecrRefCount(interp, filename);
        return JIM_ERR;
    }

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    memset(af, 0, sizeof(*af));
    af->fp = fh;
    af->fd = fileno(fh);
    af->filename = filename;
#ifdef FD_CLOEXEC
    if ((OpenFlags & AIO_KEEPOPEN) == 0) {
        fcntl(af->fd, F_SETFD, FD_CLOEXEC);
        af->OpenFlags = OpenFlags;
    }
#endif
#ifdef O_NDELAY
    af->flags = fcntl(af->fd, F_GETFL);
#endif
    af->addr_family = family;
    snprintf(buf, sizeof(buf), hdlfmt, Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimAioSubCmdProc, af, JimAioDelProc);

    Jim_SetResultString(interp, buf, -1);

    return JIM_OK;
}

#if !defined(JIM_ANSIC) && !defined(JIM_BOOTSTRAP)

static int JimAioSockCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *hdlfmt = "aio.unknown%ld";
    const char *socktypes[] = {
        "unix",
        "unix.server",
        "dgram",
        "dgram.server",
        "stream",
        "stream.server",
        "pipe",
        NULL
    };
    enum
    {
        SOCK_UNIX,
        SOCK_UNIX_SERVER,
        SOCK_DGRAM_CLIENT,
        SOCK_DGRAM_SERVER,
        SOCK_STREAM_CLIENT,
        SOCK_STREAM_SERVER,
        SOCK_STREAM_PIPE,
        SOCK_DGRAM6_CLIENT,
        SOCK_DGRAM6_SERVER,
        SOCK_STREAM6_CLIENT,
        SOCK_STREAM6_SERVER,
    };
    int socktype;
    int sock;
    const char *hostportarg = NULL;
    int res;
    int on = 1;
    const char *mode = "r+";
    int family = PF_INET;
    Jim_Obj *argv0 = argv[0];
    int ipv6 = 0;

    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[1], "-ipv6")) {
        if (!IPV6) {
            Jim_SetResultString(interp, "ipv6 not supported", -1);
            return JIM_ERR;
        }
        ipv6 = 1;
        family = PF_INET6;
    }
    argc -= ipv6;
    argv += ipv6;

    if (argc < 2) {
      wrongargs:
        Jim_WrongNumArgs(interp, 1, &argv0, "?-ipv6? type ?address?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], socktypes, &socktype, "socket type", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;

    Jim_SetEmptyResult(interp);

    hdlfmt = "aio.sock%ld";

    if (argc > 2) {
        hostportarg = Jim_String(argv[2]);
    }

    switch (socktype) {
        case SOCK_DGRAM_CLIENT:
            if (argc == 2) {
                /* No address, so an unconnected dgram socket */
                sock = socket(family, SOCK_DGRAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                break;
            }
            /* fall through */
        case SOCK_STREAM_CLIENT:
            {
                union sockaddr_any sa;
                int salen;

                if (argc != 3) {
                    goto wrongargs;
                }

                if (ipv6) {
                    if (JimParseIPv6Address(interp, hostportarg, &sa, &salen) != JIM_OK) {
                        return JIM_ERR;
                    }
                }
                else if (JimParseIpAddress(interp, hostportarg, &sa, &salen) != JIM_OK) {
                    return JIM_ERR;
                }
                sock = socket(family, (socktype == SOCK_DGRAM_CLIENT) ? SOCK_DGRAM : SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                res = connect(sock, &sa.sa, salen);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
            }
            break;

        case SOCK_STREAM_SERVER:
        case SOCK_DGRAM_SERVER:
            {
                union sockaddr_any sa;
                int salen;

                if (argc != 3) {
                    goto wrongargs;
                }

                if (ipv6) {
                    if (JimParseIPv6Address(interp, hostportarg, &sa, &salen) != JIM_OK) {
                        return JIM_ERR;
                    }
                }
                else if (JimParseIpAddress(interp, hostportarg, &sa, &salen) != JIM_OK) {
                    return JIM_ERR;
                }
                sock = socket(family, (socktype == SOCK_DGRAM_SERVER) ? SOCK_DGRAM : SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }

                /* Enable address reuse */
                setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on));

                res = bind(sock, &sa.sa, salen);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
                if (socktype == SOCK_STREAM_SERVER) {
                    res = listen(sock, 5);
                    if (res) {
                        JimAioSetError(interp, NULL);
                        close(sock);
                        return JIM_ERR;
                    }
                }
                hdlfmt = "aio.socksrv%ld";
            }
            break;

#ifdef HAVE_SYS_UN_H
        case SOCK_UNIX:
            {
                struct sockaddr_un sa;
                socklen_t len;

                if (argc != 3 || ipv6) {
                    goto wrongargs;
                }

                if (JimParseDomainAddress(interp, hostportarg, &sa) != JIM_OK) {
                    JimAioSetError(interp, argv[2]);
                    return JIM_ERR;
                }
                family = PF_UNIX;
                sock = socket(PF_UNIX, SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                len = strlen(sa.sun_path) + 1 + sizeof(sa.sun_family);
                res = connect(sock, (struct sockaddr *)&sa, len);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
                hdlfmt = "aio.sockunix%ld";
                break;
            }

        case SOCK_UNIX_SERVER:
            {
                struct sockaddr_un sa;
                socklen_t len;

                if (argc != 3 || ipv6) {
                    goto wrongargs;
                }

                if (JimParseDomainAddress(interp, hostportarg, &sa) != JIM_OK) {
                    JimAioSetError(interp, argv[2]);
                    return JIM_ERR;
                }
                family = PF_UNIX;
                sock = socket(PF_UNIX, SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                len = strlen(sa.sun_path) + 1 + sizeof(sa.sun_family);
                res = bind(sock, (struct sockaddr *)&sa, len);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
                res = listen(sock, 5);
                if (res) {
                    JimAioSetError(interp, NULL);
                    close(sock);
                    return JIM_ERR;
                }
                hdlfmt = "aio.sockunixsrv%ld";
                break;
            }
#endif

#ifdef HAVE_PIPE
        case SOCK_STREAM_PIPE:
            {
                int p[2];

                if (argc != 2 || ipv6) {
                    goto wrongargs;
                }

                if (pipe(p) < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }

                if (JimMakeChannel(interp, NULL, p[0], argv[1], "aio.pipe%ld", 0, "r") == JIM_OK) {
                    Jim_Obj *objPtr = Jim_NewListObj(interp, NULL, 0);
                    Jim_ListAppendElement(interp, objPtr, Jim_GetResult(interp));

                    if (JimMakeChannel(interp, NULL, p[1], argv[1], "aio.pipe%ld", 0, "w") == JIM_OK) {
                        Jim_ListAppendElement(interp, objPtr, Jim_GetResult(interp));
                        Jim_SetResult(interp, objPtr);
                        return JIM_OK;
                    }
                }
                /* Can only be here if fdopen() failed */
                close(p[0]);
                close(p[1]);
                JimAioSetError(interp, NULL);
                return JIM_ERR;
            }
            break;
#endif
        default:
            Jim_SetResultString(interp, "Unsupported socket type", -1);
            return JIM_ERR;
    }

    return JimMakeChannel(interp, NULL, sock, argv[1], hdlfmt, family, mode);
}
#endif /* JIM_BOOTSTRAP */

FILE *Jim_AioFilehandle(Jim_Interp *interp, Jim_Obj *command)
{
    Jim_Cmd *cmdPtr = Jim_GetCommand(interp, command, JIM_ERRMSG);

    if (cmdPtr && !cmdPtr->isproc && cmdPtr->u.native.cmdProc == JimAioSubCmdProc) {
        return ((AioFile *) cmdPtr->u.native.privData)->fp;
    }
    Jim_SetResultFormatted(interp, "Not a filehandle: \"%#s\"", command);
    return NULL;
}

int Jim_aioInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "aio", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "open", JimAioOpenCommand, NULL, NULL);
#ifndef JIM_ANSIC
    Jim_CreateCommand(interp, "socket", JimAioSockCommand, NULL, NULL);
#endif

    /* Takeover stdin, stdout and stderr */
    Jim_EvalGlobal(interp, "open stdin; open stdout; open stderr");

    return JIM_OK;
}
