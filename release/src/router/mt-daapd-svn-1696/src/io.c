/*
 * $Id: $
 *
 * Generic IO handling for files/sockets/etc
 */


#define _CRT_SECURE_NO_WARNINGS 1601
#define _CRT_SECURE_NO_DEPRECATE 1
#define _CRT_NONSTDC_NO_DEPRECATE 1
#pragma warning(disable: 4996)

#ifdef LARGE_FILE
# define _LARGEFILE_SOURCE
# define _LARGEFILE64_SOURCE
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#ifndef WIN32
# include <netdb.h>
#else
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <ws2tcpip.h>
#endif

#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef WIN32
# include <netinet/in.h>
# include <sys/select.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
#endif
#include <sys/types.h>

#include "io-errors.h"
#include "io-plugin.h"
#include "bsd-snprintf.h"

#ifdef WIN32
# define strcasecmp stricmp
# define SHUT_RDWR SD_BOTH
# define ssize_t int

# define IO_HANDLES_START  1
# define IO_HANDLES_GROW   1
#else
# define closesocket close
#endif

#ifdef WIN32
typedef struct tag_io_waithandle {
    DWORD dwLastResult;
    DWORD dwItemCount;
    DWORD dwMaxItems;
    DWORD dwWhichEvent;
    WAITABLE_T *hWaitItems;
    WSANETWORKEVENTS wsaNetworkEvents;
    IO_PRIVHANDLE **ppHandle;
} IO_WAITHANDLE;
#else
typedef struct tag_io_waithandle {
    int max_fd;
    fd_set read_fds;
    fd_set write_fds;
    fd_set err_fds;
    fd_set result_read;
    fd_set result_write;
    fd_set result_err;
} IO_WAITHANDLE;
#endif


/* Public interface */
int io_init(void);
int io_deinit(void);
int io_isproto(IO_PRIVHANDLE *phandle, char *proto);

#define IO_WAIT_READ  1
#define IO_WAIT_WRITE 2
#define IO_WAIT_ERROR 4

#define IO_BUFFER_SIZE 1024

IO_WAITHANDLE *io_wait_new(void);
int io_wait_add(IO_WAITHANDLE *pwait, IO_PRIVHANDLE *phandle, int type);
int io_wait(IO_WAITHANDLE *pwait, uint32_t *ms);
int io_wait_status(IO_WAITHANDLE *pwait, IO_PRIVHANDLE *phandle);
int io_wait_dispose(IO_WAITHANDLE *pwait);

IO_PRIVHANDLE *io_new(void);
int io_open(IO_PRIVHANDLE *phandle, char *fmt, ...);
int io_close(IO_PRIVHANDLE *phandle);
int io_read(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len);
int io_write(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len);
int io_printf(IO_PRIVHANDLE *phandle, char *fmt, ...);
int io_size(IO_PRIVHANDLE *phandle, uint64_t *size);
int io_setpos(IO_PRIVHANDLE *phandle, uint64_t offset, int whence);
int io_getpos(IO_PRIVHANDLE *phandle, uint64_t *pos);
int io_buffer(IO_PRIVHANDLE *phandle);
int io_readline(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len);
int io_readline_timeout(IO_PRIVHANDLE *phandle, unsigned char *buf,
                      uint32_t *len, uint32_t *ms);
char *io_errstr(IO_PRIVHANDLE *phandle);
void io_dispose(IO_PRIVHANDLE *phandle);

void io_set_errhandler(void(*err_handler)(int, char*));
void io_set_lockhandler(void(*lock_handler)(int));
int io_register_handler(char *proto, IO_FNPTR *fnptr);

/* Built-ins for file and socket proto */
static int io_file_open(IO_PRIVHANDLE *phandle, char *uri);
static int io_file_close(IO_PRIVHANDLE *phandle);
static int io_file_read(IO_PRIVHANDLE *phandle, unsigned char *buf,
                        uint32_t *len);
static int io_file_write(IO_PRIVHANDLE *phandle, unsigned char *buf,
                         uint32_t *len);
static int io_file_size(IO_PRIVHANDLE *phandle, uint64_t *size);
static int io_file_setpos(IO_PRIVHANDLE *phandle, uint64_t offset, int whence);
static int io_file_getpos(IO_PRIVHANDLE *phandle, uint64_t *pos);
static char* io_file_geterrmsg(IO_PRIVHANDLE *phandle, ERR_T *code, int *is_local);
static int io_file_getwaitable(IO_PRIVHANDLE *phandle, int mode, WAITABLE_T *retval);
static int io_file_getfd(IO_PRIVHANDLE *phandle, FILE_T *pfd);


static int io_socket_open(IO_PRIVHANDLE *phandle, char *uri);
static int io_socket_close(IO_PRIVHANDLE *phandle);
static int io_socket_read(IO_PRIVHANDLE *phandle, unsigned char *buf,
                          uint32_t *len);
static int io_socket_write(IO_PRIVHANDLE *phandle, unsigned char *buf,
                           uint32_t *len);
static char* io_socket_geterrmsg(IO_PRIVHANDLE *phandle, ERR_T *code, int *is_local);
static int io_socket_getwaitable(IO_PRIVHANDLE *phandle, int mode, WAITABLE_T *retval);
static int io_socket_getsocket(IO_PRIVHANDLE *phandle, SOCKET_T *psock);

static int io_listen_open(IO_PRIVHANDLE *phandle, char *uri);
int io_listen_accept(IO_PRIVHANDLE *phandle, IO_PRIVHANDLE *pchild,
                     struct in_addr *host);
int io_udp_recvfrom(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len,
                    struct sockaddr_in *si_remote, socklen_t *si_len);
int io_udp_sendto(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len,
                  struct sockaddr_in *si_remote, socklen_t si_len);

/* Private functions */
static void io_err(IO_PRIVHANDLE *phandle, uint32_t errorcode);
static void io_err_printf(int level, char *fmt, ...);
static void io_default_errhandler(int level, char *msg);
static void io_default_lockhandler(int);
static void io_lock(void);
static void io_unlock(void);
static int io_urldecode(char *); /**< do an in-place decode */
static int io_option_add(IO_PRIVHANDLE *phandle, char *key, char *value);
static int io_option_dispose(IO_PRIVHANDLE *phandle);

static void io_file_seterr(IO_PRIVHANDLE *phandle, ERR_T errcode);
static void io_socket_seterr(IO_PRIVHANDLE *phandle, ERR_T errcode);

/* Defines */
struct tag_io_optionlist {
    char *key;
    char *value;
    struct tag_io_optionlist *next;
};

typedef struct tag_io_handler_list {
    char *proto;
    IO_FNPTR *fnptr;
    struct tag_io_handler_list *next;
} IO_HANDLER_LIST;

typedef struct tag_io_file_priv {
    FILE_T fd;
    ERR_T err;
    int local_err;
    int opened;
    int open_flags;
} IO_FILE_PRIV;

#define IO_FILE_READ      0x01
#define IO_FILE_WRITE     0x02
#define IO_FILE_APPEND    0x04
#define IO_FILE_TRUNCATE  0x08
#define IO_FILE_CREATENEW 0x10
#define IO_FILE_CREATE    0x20

typedef struct tag_io_socket_priv {
    SOCKET_T fd;
    ERR_T err;
    unsigned short port;
    int local_err;
    int opened;
    int is_udp;
    struct sockaddr_in si_remote;
#ifdef WIN32
    WAITABLE_T hEvent;
    int wait_mode;
#endif
} IO_SOCKET_PRIV;

#ifdef DEBUG
#  ifndef ASSERT
#    define ASSERT(f)         \
        if(f)                 \
            {}                \
        else                  \
            io_err_printf(0,"Assert error in %s, line %d\n",__FILE__,__LINE__)
#  endif /* ndef ASSERT */
#else /* ndef DEBUG */
#  ifndef ASSERT
#    define ASSERT(f)
#  endif
#endif


/* Globals */
static IO_HANDLER_LIST io_handler_list;
static void(*io_err_handler)(int, char*) = io_default_errhandler;
static void(*io_lock_handler)(int) = io_default_lockhandler;
static int io_initialized = 0;

static char *io_err_strings[] = {
    "Error passed from proto handler",
    "Invalid or unknown protocol",
    "IO Handle not open",
    "Unsupported IO function",
    "unknown internal error"
};

static char *io_file_err_strings[] = {
    "libc error",
    "Operation on unopened file",
    "Unknown error"
};

static char *io_socket_err_strings[] = {
    "libc error",
    "socket isn't open",
    "unknown internal error",
    "unknown or invalid hostname",
    "child socket not initialized",
    "socket doesn't support function",
    "no multicast group specified",
    "invalid parameter to function call",
    "socket/port in use"
};

static IO_FNPTR io_pfn_file = {
    io_file_open,
    io_file_close,
    io_file_read,
    io_file_write,
    io_file_size,
    io_file_setpos,
    io_file_getpos,
    io_file_geterrmsg,
    io_file_getwaitable,
    io_file_getfd,
    NULL
};

static IO_FNPTR io_pfn_socket = {
    io_socket_open,
    io_socket_close,
    io_socket_read,
    io_socket_write,
    NULL,             /* size */
    NULL,             /* setpos */
    NULL,             /* getpos */
    io_socket_geterrmsg,
    io_socket_getwaitable,
    NULL,
    io_socket_getsocket
};

static IO_FNPTR io_pfn_listen = { /* reuse most of the socket stuff */
    io_listen_open,
    io_socket_close,
    NULL,              /* read */
    NULL,              /* write */
    NULL,              /* size */
    NULL,              /* setpos */
    NULL,              /* getpos */
    io_socket_geterrmsg,
    io_socket_getwaitable,
    NULL,
    io_socket_getsocket
};

static IO_FNPTR io_pfn_udp = {
    io_socket_open,
    io_socket_close,
    io_socket_read,
    io_socket_write,
    NULL,              /* size */
    NULL,              /* setpos */
    NULL,              /* getpos */
    io_socket_geterrmsg,
    io_socket_getwaitable,
    NULL,
    io_socket_getsocket
};

static IO_FNPTR io_pfn_udplisten = {
    io_listen_open,
    io_socket_close,
    io_socket_read,
    io_socket_write,
    NULL,              /* size */
    NULL,              /* setpos */
    NULL,              /* getpos */
    io_socket_geterrmsg,
    io_socket_getwaitable,
    NULL,
    io_socket_getsocket
};


/**
 * Windows compatibility functions
 */
#ifdef WIN32

static WSADATA io_WSAData;

#undef gettimeofday /* FIXME: proper gettimeofday fixups */

int gettimeofday (struct timeval *tv, void* tz) {
  union {
    uint64_t ns100; /*time since 1 Jan 1601 in 100ns units */
    FILETIME ft;
  } now;

  GetSystemTimeAsFileTime (&now.ft);
  tv->tv_usec = (long) ((now.ns100 / 10LL) % 1000000LL);
  tv->tv_sec = (long) ((now.ns100 - 116444736000000000LL) / 10000000LL);
  return (0);
}
#endif /* WIN32 */

/**
 * initialize the io handlers
 *
 * @returns TRUE on success
 */
int io_init(void) {
#ifdef WIN32
    WORD wVersionRequested;
    int err;
#endif

    ASSERT(!io_initialized); /* only initialize once */

    io_lock();
    io_handler_list.next = NULL;

#ifdef WIN32
    wVersionRequested = MAKEWORD(2,2);
    err = WSAStartup(wVersionRequested, &io_WSAData);
    if(err) {
        io_err_printf(IO_LOG_FATAL,"Could not initialize winsock\n");
        io_unlock();
        return FALSE;
    }

    if(HIBYTE(io_WSAData.wVersion < 2)) {
        io_err_printf(IO_LOG_FATAL,"This program requires Winsock 2.0 or better\n");
        WSACleanup();
        io_unlock();
        return FALSE;

    }
#endif
    io_initialized = TRUE;
    io_unlock();

    io_register_handler("file",&io_pfn_file);
    io_register_handler("socket",&io_pfn_socket);
    io_register_handler("listen",&io_pfn_listen);
    io_register_handler("udp",&io_pfn_udp);
    io_register_handler("udplisten",&io_pfn_udplisten);
    return TRUE;
}

/**
 * urldecode a string in-place
 *
 * @param str string to decode in-place
 */
int io_urldecode(char *str) {
    char *current,*dst;
    int digit1, digit2;
    char *hexdigits = "0123456789abcdef";

    current = dst = str;
    while(*current) {
        switch(*current) {
        case '+':
            *dst++ = ' ';
            *current++;
            break;
        case '%':
            /* This is rather brute force.  Maybe sscanf? */
            if(strlen(current) < 3) {
                io_err_printf(IO_LOG_DEBUG,"urldecode: unexpected EOL\n");
                return FALSE;
            }
            current++;

            if(!strchr(hexdigits,tolower(*current))) {
                io_err_printf(IO_LOG_DEBUG,"urldecode: bad hex digit\n");
                return FALSE;
            }
            digit1 = (int)(strchr(hexdigits,tolower(*current)) - hexdigits);

            current++;
            if(!strchr(hexdigits,tolower(*current))) {
                io_err_printf(IO_LOG_DEBUG,"urldecode: bad hex digit\n");
                return FALSE;
            }
            digit2 = (int)(strchr(hexdigits,tolower(*current)) - hexdigits);
            current++;

            *dst++ = (char)(((digit1 & 0x0F) << 4) | (digit2 & 0x0F));
            break;
        default:
            *dst++ = *current++;
            break;
        }
    }

    *dst = '\0';
    return TRUE;
}

/**
 * default lock handler (no locking)
 */
void io_default_lockhandler(int lock) {
}


/**
 * handler lock
 */
void io_lock(void) {
    io_lock_handler(TRUE);
}


/**
 * handler unlock
 */
void io_unlock(void) {
    io_lock_handler(FALSE);
}


/**
 * deinitiailize anything required of the io handlers
 *
 * @returns TRUE on success
 */
int io_deinit(void) {
    IO_HANDLER_LIST *pcurrent;

    ASSERT(io_initialized); /* call io_init first */

    io_lock();
    pcurrent = io_handler_list.next;
    while(pcurrent) {
        io_handler_list.next = pcurrent->next;
        free(pcurrent->proto);
        free(pcurrent);
        pcurrent = io_handler_list.next;
    }

#ifdef WIN32
    WSACleanup();
#endif

    io_unlock();

    return TRUE;
}


/**
 * set the error handler to something other than default
 *
 * @param err_handler new error handler to use
 */
void io_set_errhandler(void(*err_handler)(int, char*)) {
    io_err_handler = err_handler;
}

void io_set_lockhandler(void(*lock_handler)(int)) {
    io_lock_handler = lock_handler;
}


/**
 * internal function to report errors
 *
 * @param level priority level, 1 - 10.
 * @param fmt printf style format
 */
void io_err_printf(int level, char *fmt, ...) {
    char errbuf[4096];
    va_list ap;

    va_start(ap,fmt);
    vsnprintf(errbuf,sizeof(errbuf),fmt,ap);
    va_end(ap);

    io_err_handler(level,errbuf);
}

/**
 * internal function for printing error messages.  Should
 * probably be overridden using io_set_errhandler
 *
 * @param level errorlevel, 1:fatal, 9:debug
 */
void io_default_errhandler(int level, char *msg) {
    fprintf(stderr,"%d: %s", level, msg);
    if(!level) { /* fatal! */
        exit(0);
    }
}

/**
 * register a handler for a particular protocol.
 *
 * @param proto protocol to handle (e.g. http)
 * @param fnptr a IO_FNPTR list of function pointers
 * @returns TRUE if successful
 */
int io_register_handler(char *proto, IO_FNPTR *fnptr) {
    IO_HANDLER_LIST *pnew;
    IO_HANDLER_LIST *last;

    pnew = (IO_HANDLER_LIST*)malloc(sizeof(IO_HANDLER_LIST));
    if(!pnew)
        io_err_printf(IO_LOG_FATAL,"Error in malloc\n");

    pnew->proto = strdup(proto);
    pnew->fnptr = fnptr;
    pnew->next = NULL;

    io_lock();

    last = &io_handler_list;
    while(last->next) {
        last = last->next;
    }

    last->next = pnew;
    io_unlock();

    return TRUE;
}

/**
 * create a new io device, and return it
 */
IO_PRIVHANDLE *io_new(void) {
    IO_PRIVHANDLE *pnew;

    ASSERT(io_initialized); /* call io_init first */

    pnew = (IO_PRIVHANDLE*)malloc(sizeof(IO_PRIVHANDLE));
    if(!pnew) {
        io_err_printf(IO_LOG_FATAL,"Malloc error in io_new\n");
        return NULL;
    }

    memset(pnew,0x00,sizeof(IO_PRIVHANDLE));
    return pnew;
}

/**
 * set up a new io object with the correct fnptrs, etc
 *
 * @param phandle handle of io device to set up
 * @param proto proto to fill ptrs with
 * @returns TRUE on success
 */
int io_assign_handle(IO_PRIVHANDLE *phandle,char *proto) {
    IO_HANDLER_LIST *plist;

    ASSERT(io_initialized);  /* call io_init first */

    if(phandle->open) {
        if(!io_close(phandle))
            return FALSE;
    }

    /* re-assigning an already assigned handle */
    if((phandle->proto) && (strcasecmp(phandle->proto,proto)==0))
        return TRUE;

    /* walk through the list of items and see what we have */
    io_lock();
    plist = io_handler_list.next;
    while(plist && (strcasecmp(proto,plist->proto) != 0))
        plist = plist->next;
    io_unlock();

    if(!plist) {
        io_err_printf(IO_LOG_DEBUG,"Couldn't find handler for '%s'\n",proto);
        io_err(phandle,IO_E_BADPROTO);
        return FALSE;
    }

    phandle->open = TRUE;
    phandle->proto = plist->proto;
    phandle->fnptr = plist->fnptr;
    return TRUE;
}

/**
 * open an io device, returning the handle necessary
 *
 * @param phandle handle of io device from io_new
 * @param uri URI to open
 * @param mode mode to open (O_RDWR, etc, as open(2))
 * @returns TRUE on success
 */
int io_open(IO_PRIVHANDLE *phandle, char *fmt, ...) {
    char *proto_part = NULL;
    char *path_part = NULL;
    char *options_part = NULL;
    char *key, *value;
    int result;
    char uri_copy[4096];
    va_list ap;
    ASSERT(io_initialized);  /* call io_init first */

    va_start(ap, fmt);
    io_util_vsnprintf(uri_copy, sizeof(uri_copy), fmt, ap);
    va_end(ap);

    io_err_printf(IO_LOG_DEBUG,"Opening %s\n", uri_copy);

    if((proto_part = strstr(uri_copy,"://"))) {
        *proto_part = '\0';
        path_part = proto_part + 3;
        proto_part = uri_copy;
    }

    if(path_part)
        io_urldecode(path_part);

    /* find the start of the options */
    options_part = strchr(path_part,'?');
    if(options_part) {
        *options_part = '\0';
        options_part++;
    }

    /* see if we can generate a list of options */
    while(options_part) {
        key = options_part;
        options_part = strchr(options_part,'&');
        if(options_part) {
            *options_part = '\0';
            options_part++;
        }
        value = strchr(key,'=');
        if(value) {
            *value = '\0';
            value++;
        }

        if((key) && (value)) {
            io_urldecode(key);
            io_urldecode(value);
            io_option_add(phandle,key,value);
        }
    }

    io_err_printf(IO_LOG_DEBUG,"Checking handler for %s\n",proto_part);
    if(!io_assign_handle(phandle,proto_part)) {
        return FALSE; /* error is already set */
    }

    phandle->open = FALSE;
    io_err_printf(IO_LOG_DEBUG,"opening %s\n",path_part);
    result = phandle->fnptr->fn_open(phandle,path_part);

    if(!result) {
        io_err(phandle,IO_E_OTHER);
        return FALSE;
    }
    phandle->open = TRUE;
    return result;
}

/**
 * close an open device
 *
 * @param phandle handle of device to close
 * @returns TRUE on success
 */
int io_close(IO_PRIVHANDLE *phandle) {
    int result=FALSE;

    ASSERT(io_initialized);   /* call io_init first */
    ASSERT(phandle);
    ASSERT(phandle->fnptr);
    ASSERT(phandle->fnptr->fn_close);

    if((!phandle) || (!phandle->fnptr)) {
        io_err(phandle,IO_E_NOTINIT);
        return FALSE;
    }

    if(!phandle->fnptr->fn_close) { /* not closeable?? */
        io_err(phandle,IO_E_BADFN);
        return FALSE;
    }

    if(phandle->open) {
        result = phandle->fnptr->fn_close(phandle);
        if(!result)
            io_err(phandle,IO_E_OTHER);
    }

    phandle->open = FALSE;
    io_option_dispose(phandle);
    return result;
}


/**
 * read from an io device with a timeout.  If the timeout expires, then
 * the function returns FALSE.  Otherwise, it returns TRUE, with the
 * _remaining_ time in ms.
 *
 * Pass NULL to ms to read without a timeout
 *
 * @param phandle handle of io device to read from
 * @param buf buffer to read into
 * @param len length of buffer (bytes to read)
 * @param ms timeout in milliseconds
 * @returns TRUE on success, FALSE with ms=0 on timeout, or FALSE
 *          on other kind of error
 */
int io_read_timeout(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len,
                    uint32_t *ms) {
    IO_WAITHANDLE *pwh;

    ASSERT(io_initialized);   /* call io_init first */
    ASSERT(phandle);
    ASSERT(phandle->open);
    ASSERT(phandle->fnptr);
    ASSERT(phandle->fnptr->fn_read);

    io_err_printf(IO_LOG_SPAM,"entering io_read_timeout\n");

    if((!phandle) || (!phandle->open) || (!phandle->fnptr)) {
        io_err(phandle,IO_E_NOTINIT);
        *len = 0;
        return FALSE;
    }

    if(!phandle->fnptr->fn_read) {
        io_err(phandle,IO_E_BADFN);
        *len = 0;
        return FALSE;
    }

    if(ms) {
        /* wait for handle to become readable */
        pwh=io_wait_new();
        if(!pwh) {
            io_err(phandle,IO_E_INTERNAL);
            return FALSE;
        }

        io_wait_add(pwh,phandle,IO_WAIT_READ | IO_WAIT_ERROR);
        if(!io_wait(pwh,ms)) {
            io_wait_dispose(pwh);
            io_err(phandle,IO_E_INTERNAL);
            return FALSE;
        }
        io_wait_dispose(pwh);
    }

    return io_read(phandle,buf,len);
}


/**
 * read from the io device, using the underlying provider
 *
 * @param phandle handle of io device
 * @param buf buffer to read into
 * @param len size of buffer
 * @returns number of bytes read, 0 on EOF, or -1 on error
 */
int io_read(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len) {
    int result;
    unsigned char *buf_ptr = buf;
    uint32_t read_size;
    uint32_t max_len = *len;

    ASSERT(io_initialized);
    ASSERT(phandle);
    ASSERT(phandle->open);
    ASSERT(phandle->fnptr);
    ASSERT(phandle->fnptr->fn_read);

    io_err_printf(IO_LOG_SPAM,"entering io_read\n");

    if((!phandle) || (!phandle->open) || (!phandle->fnptr)) {
        io_err(phandle,IO_E_NOTINIT);
        *len = 0;
        return FALSE;
    }

    if(!phandle->fnptr->fn_read) {
        io_err(phandle,IO_E_BADFN);
        *len = 0;
        return FALSE;
    }

    /* check to see if we are in buffering mode */
    if(phandle->buffering) {
        *len = 0;
        while(max_len) {
            /* fill as much as possible from buffer */
            if(phandle->buffer_offset < phandle->buffer_len) {
                io_err_printf(IO_LOG_SPAM,"Fulfilling from buffer\n");
                read_size = max_len;
                if(read_size > (phandle->buffer_len - phandle->buffer_offset))
                    read_size = phandle->buffer_len - phandle->buffer_offset;
                memcpy((void*)buf_ptr,(void*)&phandle->buffer[phandle->buffer_offset], read_size);
                *len += read_size;
                max_len -= read_size;
                buf_ptr += read_size;
                phandle->buffer_offset += read_size;
            } else {
                /* read a new block */
                io_err_printf(IO_LOG_SPAM,"reading new block\n");
                phandle->buffer_len = IO_BUFFER_SIZE;
                if(!phandle->fnptr->fn_read(phandle,phandle->buffer,&phandle->buffer_len)) {
                    io_err(phandle,IO_E_OTHER);
                    return FALSE;
                }
                phandle->buffer_offset = 0;
                if(phandle->buffer_len == 0)
                    return TRUE; /* can't read any more */
            }
        }
        return TRUE; /* filled buffer */
    }

    result = phandle->fnptr->fn_read(phandle,buf,len);
    if(!result)
        io_err(phandle,IO_E_OTHER);

    io_err_printf(IO_LOG_SPAM,"Read %d bytes\n",*len);
    return result;
}


/**
 * read a line from an io device with a timeout.  If the timeout expires,
 * then the function returns FALSE (with *ms=0).  On any other error, it
 * returns FALSE, with ms non zero.  Otherwise, it returns TRUE, with
 * ms set to the number of ms *left* in the wait.
 *
 * Pass NULL to ms to read without a timeout
 *
 * @param phandle handle of io device to read from
 * @param buf buffer to read into
 * @param len length of buffer (bytes to read)
 * @param ms timeout in milliseconds
 * @returns TRUE on success, FALSE with ms=0 on timeout, or FALSE
 *          on other kind of error
 */
int io_readline_timeout(IO_PRIVHANDLE *phandle, unsigned char *buf,
                      uint32_t *len, uint32_t *ms) {
    uint32_t numread = 0;
    uint32_t to_read;
    int ascii = 0;

    if(io_option_get(phandle,"ascii",NULL))
        ascii = 1;

    io_err_printf(IO_LOG_SPAM,"entering readline_timeout\n");

    while(numread < (*len - 1)) {
        to_read = 1;
        if(io_read_timeout(phandle, buf + numread, &to_read, ms)) {
            if(!to_read) { /* EOF */
                *len = numread;
                buf[numread] = '\0';
                return TRUE;
            }
            if((!ascii) || (to_read != '\r')) {
                if(buf[numread] == '\n') {
                    buf[numread+1] = '\0'; /* retain the CR */
                    *len = numread + 1;
                    return TRUE;
                }
                numread++;
            }
        } else {
        }
    }

    buf[numread-1] = '\0';
    *len = numread-1;

    io_err_printf(IO_LOG_LOG,"Buffer too small in io_readline_timeout()\n");
    return TRUE;
}

/**
 * read a line from an io device with a timeout.  If the timeout expires,
 * then the function returns FALSE (with *ms=0).  On any other error, it
 * returns FALSE, with ms non zero.  Otherwise, it returns TRUE, with
 * ms set to the number of ms *left* in the wait.
 *
 * Pass NULL to ms to read without a timeout
 *
 * @param phandle handle of io device to read from
 * @param buf buffer to read into
 * @param len length of buffer (bytes to read)
 * @param ms timeout in milliseconds
 * @returns TRUE on success, FALSE with ms=0 on timeout, or FALSE
 *          on other kind of error
 */
int io_readline(IO_PRIVHANDLE *phandle, unsigned char *buf,
                uint32_t *len) {

    io_err_printf(IO_LOG_SPAM,"entering io_readline\n");
    return io_readline_timeout(phandle, buf, len, NULL);
}



/**
 * write to the io device, using the underlying provider
 *
 * @param phandle handle of io device
 * @param buf buffer to read into
 * @param len size of buffer
 * @returns number of bytes read, 0 on EOF, or -1 on error
 */
int io_write(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len) {
    int result;
    int ascii=0;
    int must_free=FALSE;
    unsigned char *real_buffer;
    unsigned char *dst;

    uint32_t ascii_len;
    uint32_t index;
    uint32_t real_len;

    ASSERT(io_initialized);   /* call io_init first */
    ASSERT(phandle);
    ASSERT(phandle->open);
    ASSERT(phandle->fnptr);
    ASSERT(phandle->fnptr->fn_write);

    if((!phandle) || (!phandle->open) || (!phandle->fnptr)) {
        io_err(phandle,IO_E_NOTINIT);
        *len = 0;
        return FALSE;
    }

    if(!phandle->fnptr->fn_write) {
        io_err(phandle,IO_E_BADFN);
        *len = 0;
        return FALSE;
    }


#ifdef WIN32
    // We won't do ascii mode on non-windows platforms
    if(io_option_get(phandle,"ascii",NULL))
        ascii = 1;
#endif

    if(ascii) {
        ascii_len = *len;
        for(index = 0; index < *len; index++) {
            if(buf[index] == '\n')
                ascii_len++;
        }
        real_buffer = (unsigned char *)malloc(ascii_len);
        if(!real_buffer) {
            io_err_printf(IO_LOG_FATAL,"Could not alloc buffer in io_printf\n");
            exit(-1);
        }

        must_free = TRUE;
        dst = real_buffer;
        for(index = 0; index < *len; index++) {
            if(buf[index] == '\n')
                *dst++ = '\r';
            *dst++ = buf[index];
        }
        real_len = ascii_len;
    } else {
        real_buffer = buf; /* just write what was passed */
        real_len = *len;
    }

    result = phandle->fnptr->fn_write(phandle,real_buffer,&real_len);

    if(!result)
        io_err(phandle,IO_E_OTHER);

    if(must_free) {
        if(real_len == ascii_len) /* lie about how much we wrote */
            real_len = *len;
        free(real_buffer);
    }

    *len = real_len;
    return result;
}

/**
 * Write a formatted string to an io handle.  This deals with
 * versions of vsnprintf that return either the C99 way, or the pre-C99
 * way, by increasing the buffer until it works.
 *
 * @param phandle device to write to
 * @param fmt format string of print (compatible with printf(2))
 * @returns TRUE on success
 */
int io_printf(IO_PRIVHANDLE *phandle, char *fmt, ...) {
    char *outbuf;
    char *newbuf;
    va_list ap;
    int size=200;
    int new_size;
    uint32_t len;

    outbuf = (char*)malloc(size);
    if(!outbuf) {
        io_err_printf(IO_LOG_FATAL,"Could not alloc buffer in io_printf\n");
        exit(1);
    }

    while(1) {
        va_start(ap,fmt);
        new_size=vsnprintf(outbuf,size,fmt,ap);
        va_end(ap);

        if(new_size > -1 && new_size < size)
            break;

        if(new_size > -1)
            size = new_size + 1;
        else
            size *= 2;

        if((newbuf = realloc(outbuf,size)) == NULL) {
            free(outbuf);
            io_err_printf(IO_LOG_FATAL,"malloc error in io_printf\n");
            exit(1);
        }
        outbuf = newbuf;
    }

    len = new_size;
    if(!io_write(phandle,(unsigned char *)outbuf,&len) || (len != new_size)) {
        free(outbuf);
        return FALSE;
    }

    free(outbuf);
    return TRUE;
}

/**
 * get size (length) of io device
 *
 * @param phandle device to get size for
 * @param size returns size of file (if successful)
 * @returns TRUE on success
 */
int io_size(IO_PRIVHANDLE *phandle, uint64_t *size) {
    int result;

    ASSERT(io_initialized);    /* call io_init first */
    ASSERT(phandle);
    ASSERT(phandle->open);
    ASSERT(phandle->fnptr);

    if((!phandle) || (!phandle->open) || (!phandle->fnptr)) {
        io_err(phandle,IO_E_NOTINIT);
        return FALSE;
    }

    if(!phandle->fnptr->fn_size) {
        io_err(phandle,IO_E_BADFN);
        return FALSE;
    }

    result = phandle->fnptr->fn_size(phandle,size);
    if(!result)
        io_err(phandle,IO_E_OTHER);

    return result;
}

/**
 * set current file position
 *
 * @param phandle device to set position of
 * @param offset offset to set
 * @param whence from whence to set, as in fsetpos
 * @returns TRUE on success
 */
int io_setpos(IO_PRIVHANDLE *phandle, uint64_t offset, int whence) {
    int result;

    ASSERT(io_initialized);  /* call io_init first */
    ASSERT(phandle);
    ASSERT(phandle->open);
    ASSERT(phandle->fnptr);
    ASSERT(phandle->fnptr->fn_setpos);

    if((!phandle) || (!phandle->open) || (!phandle->fnptr)) {
        io_err(phandle,IO_E_NOTINIT);
        return FALSE;
    }

    if(!phandle->fnptr->fn_setpos) {
        io_err(phandle,IO_E_BADFN);
        return FALSE;
    }

    result = phandle->fnptr->fn_setpos(phandle, offset, whence);
    if(!result)
        io_err(phandle,IO_E_OTHER);

    return result;
}

/**
 * get current file position
 *
 * @param phandle device to get file position for
 * @param pos position variable to fill
 * @returns TRUE on success
 */
int io_getpos(IO_PRIVHANDLE *phandle, uint64_t *pos) {
    int result;

    ASSERT(io_initialized);  /* call io_init first */
    ASSERT(phandle);
    ASSERT(phandle->open);
    ASSERT(phandle->fnptr);
    ASSERT(phandle->fnptr->fn_getpos);

    if((!phandle) || (!phandle->open) || (!phandle->fnptr)) {
        io_err(phandle,IO_E_NOTINIT);
        return FALSE;
    }

    if(!phandle->fnptr->fn_getpos) {
        io_err(phandle,IO_E_BADFN);
        return FALSE;
    }

    result = phandle->fnptr->fn_getpos(phandle, pos);
    if(!result)
        io_err(phandle,IO_E_OTHER);

    return result;
}

/**
 * set up line buffering mode for the file handle
 *
 * @param phandle device to set line buffering up for
 * @returns TRUE on success
 */
int io_buffer(IO_PRIVHANDLE *phandle) {
    ASSERT(phandle);

    if(!phandle)
        return FALSE;

    if(phandle->buffer)
        return TRUE;

    phandle->buffer = (unsigned char*)malloc(IO_BUFFER_SIZE);
    if(!phandle->buffer) {
        io_err_printf(IO_LOG_FATAL,"Malloc error in io_buffer\n");
        exit(-1);
    }

    phandle->buffering=1;

    return TRUE;
}

/**
 * return the current error string for an io device
 *
 * @param phandle phandle to get error string for
 * @returns error string.  Only valid until next io call on this device
 */
char *io_errstr(IO_PRIVHANDLE *phandle) {
    ASSERT(phandle);
    ASSERT(phandle->err_str);

    if(!phandle)
        return "Invalid io handle";

    if(!phandle->err_str)
        return "Unknown error";

    return phandle->err_str;
}

/**
 * return the current error code for an io device
 *
 * @param phandle phandle to get error code for
 * @returns error code.  Only valid until next io call on this device
 */
uint32_t io_errcode(IO_PRIVHANDLE *phandle) {
    ASSERT(phandle);

    if(!phandle)
        return 0;

    return phandle->err;
}


/**
 * dispoase of an io handle
 *
 */
void io_dispose(IO_PRIVHANDLE *phandle) {
    ASSERT(phandle);

    if(!phandle)
        return;

    if(phandle->open)
        io_close(phandle);

    io_option_dispose(phandle);
    if(phandle->err_str)
        free(phandle->err_str);

    free(phandle);
}

/**
 * set up an error code and error message
 *
 * @param phandle phandle to read from
 * @param errorcode errorcode to return, or IO_E_OTHER if the
 *        errorcode should be set from the underlying provider
 */
void io_err(IO_PRIVHANDLE *phandle, uint32_t errorcode) {
    ASSERT(phandle);
    ASSERT(errorcode || phandle->fnptr);

    if((!phandle) || (!errorcode && !phandle->fnptr))
        return;

    if(phandle->err_str)
        free(phandle->err_str);

    phandle->err = errorcode;

    if(errorcode) {
        phandle->err_str = strdup(io_err_strings[errorcode & 0x00FFFFFF]);
        phandle->err = errorcode;
        phandle->is_local = TRUE;
    } else {
        /* underlying provider must provide a free-able error message */
        phandle->err_str = phandle->fnptr->fn_geterrmsg(phandle, &phandle->err, &phandle->is_local);
    }

    while((phandle->err_str[strlen(phandle->err_str) - 1] == '\n') ||
        (phandle->err_str[strlen(phandle->err_str) -1] == '\r'))
        phandle->err_str[strlen(phandle->err_str) -1] = '\0';
}

/**
 * attach a file device to an existing native file representative
 *
 * @param phandle an existing phandle to attach to
 * @param fd native file handle
 * @returns TRUE on success
 */
int io_file_attach(IO_PRIVHANDLE *phandle, FILE_T fd) {
    IO_FILE_PRIV *priv;

    ASSERT(phandle);

    if(!phandle) {
        return FALSE;
    }

    if(!io_assign_handle(phandle,"file")) {
        return FALSE; /* error already set */
    }

    priv = (IO_FILE_PRIV*)malloc(sizeof(IO_FILE_PRIV));
    if(!priv) {
        io_err_printf(IO_LOG_FATAL,"Malloc error in io_file_attach\n");
        return FALSE;
    }
    phandle->private = (void*)priv;
    memset(priv,0x00,sizeof(IO_FILE_PRIV));

    priv->fd = fd;
    priv->opened = TRUE;

    return TRUE;
}

/**
 * open an file device
 *
 * @param phandle handle of io device from io_new
 * @param uri URI to open (minus the protocol)
 * @param mode mode to open (O_RDWR, etc, as open(2))
 * @returns TRUE on success
 */
int io_file_open(IO_PRIVHANDLE *phandle, char *uri) {
    IO_FILE_PRIV *priv;
    char *mode;
    char *permissions; /* octal */
    uint32_t native_mode=0;
#ifdef WIN32
    uint32_t native_permissions=0;
    WCHAR *utf16_path; /* the real windows utf16 path */
#endif

    ASSERT(phandle);
    if(!phandle) {
        return FALSE;
    }

    priv = (IO_FILE_PRIV*)malloc(sizeof(IO_FILE_PRIV));
    if(!priv) {
        io_err_printf(IO_LOG_FATAL,"Malloc error in io_file_attach\n");
        return FALSE;
    }
    phandle->private = (void*)priv;
    memset(priv,0x00,sizeof(IO_FILE_PRIV));

    priv->open_flags = 0;
    mode = io_option_get(phandle,"mode","r");
    permissions = io_option_get(phandle,"permissions","0666");

    if(strcmp(mode,"r")==0)
        priv->open_flags = IO_FILE_READ;
    if(strcmp(mode,"r+")==0)
        priv->open_flags = IO_FILE_READ | IO_FILE_WRITE;
    if(strcmp(mode,"w")==0)
        priv->open_flags = IO_FILE_WRITE | IO_FILE_CREATE | IO_FILE_TRUNCATE;
    if(strcmp(mode,"w+")==0)
        priv->open_flags = IO_FILE_READ | IO_FILE_WRITE | IO_FILE_CREATE | IO_FILE_TRUNCATE;
    if(strcmp(mode,"a")==0)
        priv->open_flags = IO_FILE_WRITE | IO_FILE_APPEND | IO_FILE_CREATE;
    if(strcmp(mode,"a+")==0)
        priv->open_flags = IO_FILE_READ | IO_FILE_WRITE | IO_FILE_APPEND | IO_FILE_CREATE;

    /* open the file natively, and get native file handle */
#ifdef WIN32
    if(priv->open_flags & IO_FILE_READ)
        native_permissions |= GENERIC_READ;
    if(priv->open_flags & IO_FILE_WRITE)
        native_permissions |= GENERIC_WRITE;

    if(priv->open_flags & IO_FILE_CREATE)
        native_mode |= OPEN_ALWAYS;
    else
        native_mode |= OPEN_EXISTING;

    utf16_path = (WCHAR *)util_utf8toutf16_alloc(uri);
    priv->fd = CreateFileW(utf16_path,native_permissions,FILE_SHARE_READ,NULL,
        native_mode,FILE_ATTRIBUTE_NORMAL,NULL);
    free(utf16_path);
    if(priv->fd == INVALID_HANDLE_VALUE) {
        io_file_seterr(phandle,IO_E_FILE_OTHER);
        return FALSE;
    }

    if(priv->open_flags & IO_FILE_TRUNCATE)
        SetEndOfFile(priv->fd);
#else
    if((priv->open_flags & IO_FILE_READ) && (priv->open_flags & IO_FILE_WRITE))
        native_mode |= O_RDWR;
    else {
        if(priv->open_flags & IO_FILE_READ)
            native_mode |= O_RDONLY;
        if(priv->open_flags & IO_FILE_WRITE)
            native_mode |= O_WRONLY;
    }
    if(priv->open_flags & IO_FILE_TRUNCATE)
        native_mode |= O_TRUNC;
    if(priv->open_flags & IO_FILE_APPEND)
        native_mode |= O_APPEND;
    if(priv->open_flags & IO_FILE_CREATE)
        native_mode |= O_CREAT;

    priv->fd = open(uri,native_mode,strtol(permissions,(char**)NULL,8));
    if(priv->fd == -1) {
        io_file_seterr(phandle,IO_E_FILE_OTHER);
        return FALSE;
    }
#endif

    priv->opened = TRUE;
    return TRUE;
}

/**
 * close an open device
 *
 * @param phandle handle of device to close
 * @returns TRUE on success
 */
int io_file_close(IO_PRIVHANDLE *phandle) {
    IO_FILE_PRIV *priv;

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle) || (!phandle->private)) {
        return FALSE;
    }

    priv = (IO_FILE_PRIV*) phandle->private;

    if(!priv->opened) {
        io_file_seterr(phandle,IO_E_FILE_NOTOPEN);
        return FALSE;
    }

    /* close the native file handle */
#ifdef WIN32
    CloseHandle(priv->fd);
#else
    close(priv->fd);
#endif

    free(priv);
    phandle->private = NULL;

    return TRUE;
}

/**
 * read from the io device, using the underlying provider
 *
 * @param phandle handle of io device
 * @param buf buffer to read into
 * @param len size of buffer
 * @returns number of bytes read, 0 on EOF, or -1 on error
 */
int io_file_read(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len) {
    IO_FILE_PRIV *priv;
    int result;

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle) || (!phandle->private)) {
        *len = 0;
        return FALSE;
    }

    priv = (IO_FILE_PRIV*) phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        io_file_seterr(phandle,IO_E_FILE_NOTOPEN);
        *len = 0;
        return FALSE;
    }

    /* read from native file handle */
#ifdef WIN32
    result = (int) ReadFile(priv->fd,buf,*len,len,NULL);
    if(!result) {
        io_file_seterr(phandle,IO_E_FILE_OTHER);
        *len = 0;
        result = FALSE;
    } else {
        result = TRUE;
    }
#else
    while(((result = read(priv->fd, buf, *len)) == -1) &&
          (errno == EINTR));

    if(result < 0) {
        io_file_seterr(phandle,IO_E_FILE_OTHER);
        *len = 0;
        result = FALSE;
    } else {
        *len = result;
        result = TRUE;
    }
#endif

    return result;
}

/**
 * write to the io device, using the underlying provider
 *
 * @param phandle handle of io device
 * @param buf buffer to read into
 * @param len size of buffer
 * @returns number of bytes read, 0 on EOF, or -1 on error
 */
int io_file_write(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len) {
    IO_FILE_PRIV *priv;
    int result;

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle) || (!phandle->private)) {
        *len = 0;
        return FALSE;
    }

    priv = (IO_FILE_PRIV*) phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        io_file_seterr(phandle,IO_E_FILE_NOTOPEN);
        *len = 0;
        return FALSE;
    }

    /* write to native file handle */
#ifdef WIN32
    result = (int) WriteFile(priv->fd,buf,*len,len,NULL);
    if(!result) {
        io_file_seterr(phandle,IO_E_FILE_OTHER);
        *len = 0;
        result = FALSE;
    } else {
        result = TRUE;
    }

#else
    while(((result = write(priv->fd, buf, *len)) == -1) &&
          (errno == EINTR));

    if(result < 0) {
        io_file_seterr(phandle,IO_E_FILE_OTHER);
        *len = 0;
        result = FALSE;
    } else {
        *len = result;
        result = TRUE;
    }
#endif

    return result;
}

/**
 * get size (length) of io device
 *
 * @param phandle device to get size for
 * @param size returns size of file (if successful)
 * @returns TRUE on success
 */
int io_file_size(IO_PRIVHANDLE *phandle, uint64_t *size) {
    IO_FILE_PRIV *priv;
    int result=FALSE;
#ifdef WIN32
    LARGE_INTEGER liSize;
#else
    uint64_t curpos;
#endif

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle) || (!phandle->private)) {
        *size = 0;
        return FALSE;
    }

    priv = (IO_FILE_PRIV*) phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        *size = 0;
        return FALSE;
    }

#ifdef WIN32
    result = GetFileSizeEx(priv->fd,&liSize);
    *size = liSize.QuadPart;
#else
    result = FALSE;
    if(io_file_getpos(phandle,&curpos))
        if(io_file_setpos(phandle,0,SEEK_END))
            if(io_file_getpos(phandle,size))
                if(io_file_setpos(phandle, curpos, SEEK_SET))
                    result = TRUE;
#endif
    return result;
}

/**
 * set current file position
 *
 * @param phandle device to set position of
 * @param offset offset to set
 * @param whence from whence to set, as in fsetpos
 * @returns TRUE on success
 */
int io_file_setpos(IO_PRIVHANDLE *phandle, uint64_t offset, int whence) {
    IO_FILE_PRIV *priv;
    int result=FALSE;
#ifdef WIN32
    int native_position;
    LARGE_INTEGER liSize;
#endif

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle) || (!phandle->private)) {
        return FALSE;
    }

    priv = (IO_FILE_PRIV*) phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        return FALSE;
    }

#ifdef WIN32
    switch(whence) {
        case SEEK_SET:
            native_position = FILE_BEGIN;
            break;
        case SEEK_CUR:
            native_position = FILE_CURRENT;
            break;
        case SEEK_END:
            native_position = FILE_END;
            break;
    }
    liSize.QuadPart = offset;
    result = SetFilePointerEx(priv->fd,liSize,NULL,(DWORD)native_position);
#else
    result = TRUE;
    if(lseek(priv->fd, (off_t)offset, whence) == -1)
        result = FALSE;
#endif /* WIN32 */

    if(!result) {
        io_file_seterr(phandle, IO_E_FILE_OTHER);
    }

    return result;
}

/**
 * get current file position
 *
 * @param phandle device to get file position for
 * @param pos position variable to fill
 * @returns TRUE on success
 */
int io_file_getpos(IO_PRIVHANDLE *phandle, uint64_t *pos) {
    IO_FILE_PRIV *priv;
    int result=FALSE;

#ifdef WIN32
    LARGE_INTEGER liPos;
    LARGE_INTEGER liResult;
#endif

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle) || (!phandle->private)) {
        return FALSE;
    }

    priv = (IO_FILE_PRIV*) phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        return FALSE;
    }

#ifdef WIN32
    liPos.QuadPart = 0;
    result = SetFilePointerEx(priv->fd,liPos,&liResult,FILE_CURRENT);
    *pos = liResult.QuadPart;
#else
    result = TRUE;
    *pos = lseek(priv->fd, 0, SEEK_CUR);
    if((*pos) == -1)
        result = FALSE;
#endif
    if(!result)
        io_file_seterr(phandle,IO_E_FILE_OTHER);
    return result;
}

/**
 * set the file error for a particular file object
 * this should be a local error code, or IO_E_FILE_OTHER
 * if it should come from the libc error (stderr, getlasterror), etc
 *
 * @param phandle file object to set error for
 * @param errcode error code to set
 */
void io_file_seterr(IO_PRIVHANDLE *phandle, ERR_T errcode) {
    IO_FILE_PRIV *priv;

    ASSERT(phandle);

    if(!phandle)
        return;

    priv = (IO_FILE_PRIV*)(phandle->private);

    if(errcode) {
        priv->err = errcode;
        priv->local_err = TRUE;
    } else {
        priv->err = errno;
        priv->local_err = FALSE;
    }
}

/**
 * get the local error message, whatever it is.  This is only
 * guaranteed to be around until the next io call.  It may very
 * well dissappear at that point.
 *
 * @param phandle device to get error message for
 * @returns error message
 */
char *io_file_geterrmsg(IO_PRIVHANDLE *phandle, ERR_T *code, int *is_local) {
    IO_FILE_PRIV *priv;
#ifdef WIN32
    char lpErrorBuf[256];
#endif

    ASSERT(phandle);

    if(!phandle)
        return NULL;

    priv = (IO_FILE_PRIV*)(phandle->private);
    ASSERT(priv);

    if(!priv) {
        return "file not initialized";
    }

    if(code)
        *code = priv->err;

    if(priv->local_err) {
        if(is_local)
            *is_local = TRUE;
        return strdup(io_file_err_strings[priv->err & 0x00FFFFFF]);
    } else {
        if(is_local)
            *is_local=FALSE;
#ifdef WIN32
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,priv->err,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
            (LPTSTR)lpErrorBuf,sizeof(lpErrorBuf),NULL);
        return strdup(lpErrorBuf);
#else
        return strdup(strerror(priv->err));
#endif
    }
}

/**
 * get the waitable dingus for the io object.  This iwll be a
 * selectable fd for unix, or a HANDLE for win32
 *
 * @param phandle io device to get waitable object for
 * @returns waitable object
 */
int io_file_getwaitable(IO_PRIVHANDLE *phandle, int mode, WAITABLE_T *retval) {
    IO_FILE_PRIV *priv;

    ASSERT(phandle);
    ASSERT(phandle->private);
    ASSERT(retval);

    if((!phandle) || (!phandle->private)) {
        return FALSE;
    }

    if(!retval) {
        io_file_seterr(phandle,IO_E_FILE_UNKNOWN);
        return FALSE;
    }

    priv = (IO_FILE_PRIV*) phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        io_file_seterr(phandle,IO_E_FILE_NOTOPEN);
        return FALSE;
    }

    *retval = priv->fd;
    return TRUE;
}

/**
 * return the underlying file handle for a file type object
 */
int io_file_getfd(IO_PRIVHANDLE *phandle, FILE_T *pfd) {
    IO_FILE_PRIV *priv;

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle) || (!phandle->private)) {
        return FALSE;
    }

    if(!pfd) {
        io_file_seterr(phandle,IO_E_FILE_UNKNOWN);
        return FALSE;
    }

    priv = (IO_FILE_PRIV*) phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        io_file_seterr(phandle,IO_E_FILE_NOTOPEN);
        return FALSE;
    }

    *pfd = priv->fd;
    return TRUE;
}


/**
 * attach a socket device to an existing native file representative
 *
 * @param phandle an existing phandle to attach to
 * @param fd native file handle
 * @returns TRUE on success
 */
int io_socket_attach(IO_PRIVHANDLE *phandle, SOCKET_T fd) {
    IO_SOCKET_PRIV *priv;

    ASSERT(phandle);

    if(!phandle) {
        return FALSE;
    }

    if(!io_assign_handle(phandle,"socket")) {
        return FALSE; /* error already set */
    }

    priv = (IO_SOCKET_PRIV*)malloc(sizeof(IO_SOCKET_PRIV));
    if(!priv) {
        io_err_printf(IO_LOG_FATAL,"Malloc error in io_socket_attach\n");
        return FALSE;
    }
    phandle->private = (void*)priv;
    memset(priv,0x00,sizeof(IO_SOCKET_PRIV));

    priv->fd = fd;
    priv->opened = TRUE;

    return TRUE;
}

/**
 * open an socket device
 *
 * @param phandle handle of io device from io_new
 * @param uri URI to open (minus the protocol)
 * @param mode mode to open (O_RDWR, etc, as open(2))
 * @returns TRUE on success
 */
int io_socket_open(IO_PRIVHANDLE *phandle, char *uri) {
    char *port_part;
    char *server_part;
    struct hostent *phe;
    unsigned short int port = 80;
    int retval;
    int is_udp;
    IO_SOCKET_PRIV *priv;

    ASSERT(phandle);

    if(!phandle) {
        io_socket_seterr(phandle,IO_E_SOCKET_UNKNOWN);
        return FALSE;
    }

    is_udp = io_isproto(phandle,"udp");

    priv = (IO_SOCKET_PRIV*)malloc(sizeof(IO_SOCKET_PRIV));
    if(!priv) {
        io_err_printf(IO_LOG_FATAL,"Malloc error in io_socket_attach\n");
        return FALSE;
    }
    phandle->private = (void*)priv;
    memset(priv,0x00,sizeof(IO_SOCKET_PRIV));

    /* the uri should be server:port */
    server_part = uri;
    port_part = strchr(server_part,':');
    if(port_part) {
        *port_part = '\0';
        port_part++;
        port = atoi(port_part);
    }

    ASSERT(port_part);

    if(inet_addr(server_part) == INADDR_NONE) {
        phe=gethostbyname(server_part);
        if(phe == NULL) {
            io_socket_seterr(phandle,IO_E_SOCKET_BADHOST);
            return FALSE;
        }
        memcpy((char*)&priv->si_remote.sin_addr,(char*)(phe->h_addr_list[0]),
               sizeof(struct in_addr));
    } else {
        priv->si_remote.sin_addr.s_addr = inet_addr(server_part);
    }

    priv->si_remote.sin_port = htons((short)port);
    priv->si_remote.sin_family = AF_INET;

    if(is_udp) {
        priv->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    } else {
        priv->fd = socket(AF_INET, SOCK_STREAM, 0);
    }
    if(priv->fd == -1) {
        io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
        return FALSE;
    }

    if(is_udp) {
        priv->opened = TRUE;
        priv->is_udp = TRUE;
        return TRUE;
    }

    /* do the connect */
    while(1) {
        retval = connect(priv->fd,(struct sockaddr *)&priv->si_remote,
                         sizeof(priv->si_remote));
        if((retval != -1) || (errno != EINTR))
            break;
    }

    if(retval == -1) {
        closesocket(priv->fd);
        io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
        return FALSE;
    }

    priv->opened = TRUE;
    return TRUE;
}

/**
 * open a listening socket device
 *
 * @param phandle handle of io device from io_new
 * @param uri URI to open (minus the protocol)
 * @param flags (as open(2) - ignored)
 * @param mode mode to open (as open(2) - ignored)
 * @returns TRUE on success
 */
int io_listen_open(IO_PRIVHANDLE *phandle, char *uri) {
    IO_SOCKET_PRIV *priv;
    struct sockaddr_in server;
    unsigned short port;
    int retval;
    int opt;
    int backlog;
    int reuse;
    int multicast=0;
    struct ip_mreq mreq;
    char *mcast_group;

    ASSERT(phandle && uri);

    io_err_printf(IO_LOG_DEBUG, "Doing io_listen_open\n");

    if((!phandle) || (!uri)) {
        io_socket_seterr(phandle,IO_E_SOCKET_UNKNOWN);
        return FALSE;
    }

    priv = (IO_SOCKET_PRIV*)malloc(sizeof(IO_SOCKET_PRIV));
    if(!priv) {
        io_err_printf(IO_LOG_FATAL,"Malloc error in io_listen_open\n");
        return FALSE;
    }

    phandle->private=(void*)priv;
    memset(priv,0x0,sizeof(IO_SOCKET_PRIV));

    if(io_isproto(phandle, "udplisten")) {
        priv->is_udp = 1;
    }

    /* read options, get defaults */
    backlog = atoi(io_option_get(phandle,"backlog","5"));
    reuse = atoi(io_option_get(phandle,"reuseaddr","1"));

    /* the uri should be simply the port number */
    port = atoi(uri);

    if(priv->is_udp) {
        /* set up socket for multicast, if required */
        multicast = atoi(io_option_get(phandle,"multicast","0"));

        priv->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if((priv->fd != -1) && (multicast)) {
            opt = atoi(io_option_get(phandle,"mcast_ttl","4"));
            setsockopt(priv->fd, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&opt,
                       sizeof(opt));
            mcast_group = io_option_get(phandle,"mcast_group",NULL);
            if(!mcast_group) {
                io_socket_seterr(phandle,IO_E_SOCKET_NOMCAST);
                while((closesocket(priv->fd) == -1) && (errno == EINTR));
                return FALSE;
            } else {
                mreq.imr_multiaddr.s_addr = inet_addr(mcast_group);
                mreq.imr_interface.s_addr = htonl(INADDR_ANY);
                if(setsockopt(priv->fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,(char*)&mreq,
                              sizeof(mreq)) < 0) {
                    io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
                    while((closesocket(priv->fd) == -1) && (errno == EINTR));
                    return FALSE;
                }
            }
        }
    } else {
        priv->fd = socket(AF_INET, SOCK_STREAM, 0);
    }

    if(priv->fd == -1) {
        io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
        io_err_printf(IO_LOG_DEBUG,"Couldn't open socket\n");
        return FALSE;
    }

    io_err_printf(IO_LOG_DEBUG,"Socket opened\n");
    opt = reuse; /* SO_REUSEADDR */
    if(setsockopt(priv->fd,SOL_SOCKET, SO_REUSEADDR,(char*)&opt,
                  sizeof(opt)) == -1) {
        io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
        io_err_printf(IO_LOG_DEBUG,"Error setting SO_REUSEADDR\n");
        while((closesocket(priv->fd) == -1) && (errno == EINTR));
        return FALSE;
    }

    /* bind and listen */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons((short)port);

    while(((retval =
            bind(priv->fd,(struct sockaddr *)&server, sizeof(server))) == -1)
          && (errno == EINTR));

    if(retval == -1) {
        io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
        io_err_printf(IO_LOG_DEBUG,"Error binding socket\n");
        while((closesocket(priv->fd) == -1) && (errno == EINTR));
        return FALSE;
    }

    if(priv->is_udp) {
        priv->opened = TRUE;
        io_err_printf(IO_LOG_DEBUG,"Set up UDP listener successfully\n");
        return TRUE;  /* don't need to listen */
    }

    io_err_printf(IO_LOG_DEBUG,"Preparing to listen with %d backlog on %d\n",
        backlog, port);
    while(((retval = listen(priv->fd,backlog)) == -1) && (errno == EINTR));
    if(retval == -1) {
        io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
        while((closesocket(priv->fd) == -1) && (errno == EINTR));
        return FALSE;
    }

    priv->opened = TRUE;
    return TRUE;
}

/**
 * accept from a listening socket
 *
 * @param phandle handle to accept
 * @param pchild handle to initialize with child socket
 * @param host network address of remote host
 * @returns TRUE on success
 */
int io_listen_accept(IO_PRIVHANDLE *phandle, IO_PRIVHANDLE *pchild,
                     struct in_addr *host) {
    socklen_t len = sizeof(struct sockaddr);
    struct sockaddr_in client;
    SOCKET_T child_fd;
    IO_SOCKET_PRIV *priv;

    ASSERT(phandle);
    ASSERT(pchild);

    if(!phandle)
        return FALSE;

    if(!pchild) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTINIT);
        return FALSE;
    }

    if(!io_isproto(phandle,"listen")) {
        io_socket_seterr(phandle,IO_E_SOCKET_BADFN);
        return FALSE;
    }

    priv = (IO_SOCKET_PRIV *)phandle->private;

    while(((child_fd =
            accept(priv->fd,(struct sockaddr *)(&client),&len)) == -1) &&
          (errno == EINTR));

    if(child_fd == -1) {
        io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
        return FALSE;
    }

    io_err_printf(IO_LOG_DEBUG,"Got listen socket %d\n",child_fd);

    /* copy host, if passed a buffer */
    if(host)
        *host = client.sin_addr;

    /* FIXME: should return error in parent handle, not child */
    return io_socket_attach(pchild, child_fd);
}

/**
 * close an open socket device
 *
 * @param phandle handle of device to close
 * @returns TRUE on success
 */
int io_socket_close(IO_PRIVHANDLE *phandle) {
    IO_SOCKET_PRIV *priv;

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle)||(!phandle->private)) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

    priv = (IO_SOCKET_PRIV*)phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

    shutdown(priv->fd,SHUT_RDWR);
    closesocket(priv->fd);

#ifdef WIN32
    if(priv->hEvent) {
        WSACloseEvent(priv->hEvent);
        priv->hEvent = NULL;
    }
#endif

    free(priv);
    phandle->private = NULL;

    return TRUE;
}

/**
 * read from the io device, using the underlying provider
 *
 * @param phandle handle of io device
 * @param buf buffer to read into
 * @param len size of buffer
 * @returns number of bytes read, 0 on EOF, or -1 on error
 */
int io_socket_read(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len) {
    ssize_t bytes_read;
    IO_SOCKET_PRIV *priv;

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle)||(!phandle->private)) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

    priv = (IO_SOCKET_PRIV*)phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

    while(((bytes_read = recv(priv->fd, buf, *len, 0)) == -1) &&
          (errno == EINTR));

    if(bytes_read == -1) {
        io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
        return FALSE;
    }

    *len = bytes_read;
    return TRUE;
}

/**
 * write to the io device, using the underlying provider
 *
 * @param phandle handle of io device
 * @param buf buffer to read into
 * @param len size of buffer
 * @returns number of bytes read, 0 on EOF, or -1 on error
 */
int io_socket_write(IO_PRIVHANDLE *phandle, unsigned char *buf,uint32_t *len) {
    IO_SOCKET_PRIV *priv;
    int slen;

    uint32_t bytestowrite;
    ssize_t byteswritten=0;
    uint32_t totalbytes;
    unsigned char *bufp;
    long blocking = 0;

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle)||(!phandle->private)) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

    priv = (IO_SOCKET_PRIV*)phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

    for(bufp = buf, bytestowrite = *len, totalbytes=0;
        bytestowrite > 0;
        bufp += byteswritten, bytestowrite -= byteswritten) {
        if(priv->is_udp) { /* must be sending to */
            slen = sizeof(struct sockaddr_in);
            byteswritten = sendto(priv->fd, bufp, bytestowrite, 0,
                                  (struct sockaddr *)&priv->si_remote, slen);
        } else {
            byteswritten = send(priv->fd, bufp, bytestowrite, 0);
        }

        io_err_printf(IO_LOG_SPAM,"wrote %d bytes to socket %d\n",byteswritten,priv->fd);

#ifdef WIN32
        if(WSAGetLastError() == WSAEWOULDBLOCK) {
            byteswritten = 0;

            if(priv->hEvent) {
                WSAEventSelect(priv->fd,(WSAEVENT)priv->hEvent,0);
            }

            blocking = 0;
            if(ioctlsocket(priv->fd,FIONBIO,&blocking)) {
                io_err_printf(IO_LOG_LOG,"Couldn't set socket to blocking: %ld\n",WSAGetLastError());
            }
        }
#endif
        if((byteswritten == -1 ) && (errno != EINTR)) {
            io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
            *len = totalbytes;
            return FALSE;
        }

        if(byteswritten == -1)
            byteswritten = 0;
        totalbytes += byteswritten;
    }

    *len = totalbytes;
    return TRUE;
}

/**
 * get size (length) of io device
 *
 * @param phandle device to get size for
 * @param size returns size of file (if successful)
 * @returns TRUE on success
 */
int io_socket_size(IO_PRIVHANDLE *phandle, uint64_t *size) {
    return FALSE;
}

/**
 * set current file position
 *
 * @param phandle device to set position of
 * @param offset offset to set
 * @param whence from whence to set, as in fsetpos
 * @returns TRUE on success
 */
int io_socket_setpos(IO_PRIVHANDLE *phandle, uint64_t offset, int whence) {
    return FALSE;
}

/**
 * get current file position
 *
 * @param phandle device to get file position for
 * @param pos position variable to fill
 * @returns TRUE on success
 */
int io_socket_getpos(IO_PRIVHANDLE *phandle, uint64_t *pos) {
    return FALSE;
}

/**
 * set the file error for a particular file object
 * this should be a local error code, or IO_E_SOCKET_OTHER
 * if it should come from the libc error (stderr, getlasterror), etc
 *
 * @param phandle file object to set error for
 * @param errcode error code to set
 */
void io_socket_seterr(IO_PRIVHANDLE *phandle, ERR_T errcode) {
    IO_SOCKET_PRIV *priv;

    ASSERT(phandle);

    if(!phandle)
        return;

    priv = (IO_SOCKET_PRIV*)(phandle->private);

    if(errcode) {
        priv->err = errcode;
        priv->local_err = TRUE;
    } else {
        priv->local_err = FALSE;
#ifdef WIN32
        priv->err = WSAGetLastError();
        /* map error codes to exported errors */
        if(priv->err == WSAEADDRINUSE) {
            priv->err = IO_E_SOCKET_INUSE;
            priv->local_err = TRUE;
        }
#else
        priv->err = errno;
        /* map error codes to exported errors */
        if(priv->err == EADDRINUSE) {
            priv->err = IO_E_SOCKET_INUSE;
            priv->local_err = TRUE;
        }
#endif
    }
}

/**
 * get the local error message, whatever it is.  This is only
 * guaranteed to be around until the next io call.  It may very
 * well dissappear at that point.
 *
 * @param phandle device to get error message for
 * @returns error message
 */
char *io_socket_geterrmsg(IO_PRIVHANDLE *phandle, ERR_T *code, int *is_local) {
    IO_SOCKET_PRIV *priv;

#ifdef WIN32
    char lpErrorBuf[256];
#endif

    ASSERT(phandle);

    priv = (IO_SOCKET_PRIV*)(phandle->private);
    if(*code)
        *code = priv->err;

    if(priv->local_err) {
        *is_local = TRUE;
        return strdup(io_socket_err_strings[priv->err & 0x00FFFFFF]);
    } else {
        *is_local = FALSE;
#ifdef WIN32
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,priv->err,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
            (LPTSTR)lpErrorBuf,sizeof(lpErrorBuf),NULL);
        return strdup(lpErrorBuf);

#else
        return strdup(strerror(priv->err));
#endif
    }
}

/**
 * get the waitable dingus for the io object.  This iwll be a
 * selectable fd for unix, or a HANDLE for win32
 *
 * @param phandle io device to get waitable object for
 * @returns waitable object
 */
int io_socket_getwaitable(IO_PRIVHANDLE *phandle, int mode, WAITABLE_T *retval) {
    IO_SOCKET_PRIV *priv;
#ifdef WIN32
    long lEvents=0;
#endif

    ASSERT(phandle);
    ASSERT(phandle->private);
    ASSERT(retval);
    ASSERT(mode);

    if((!phandle) || (!phandle->private)) {
        io_socket_seterr(phandle,IO_E_SOCKET_UNKNOWN);
        return FALSE;
    }

    if(!retval) {
        io_socket_seterr(phandle,IO_E_SOCKET_UNKNOWN);
        return FALSE;
    }

    if(!mode) {
        io_socket_seterr(phandle,IO_E_SOCKET_INVALID);
        return FALSE;
    }

    priv = (IO_SOCKET_PRIV*) phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

#ifdef WIN32
    io_err_printf(IO_LOG_SPAM,"Building synthesized event for socket\n");
    if(priv->hEvent) {
        if(mode == priv->wait_mode) {
            *retval = priv->hEvent;
            return TRUE;
        } else {
            WSACloseEvent(priv->hEvent);
            priv->hEvent = NULL;
        }
    }

    priv->hEvent = (WAITABLE_T)WSACreateEvent();
    if(!priv->hEvent) {
        io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
        return FALSE;
    }

    if(mode & IO_WAIT_READ)
        lEvents = FD_READ | FD_OOB | FD_ACCEPT | FD_CLOSE;
    if(mode & IO_WAIT_WRITE)
        lEvents |= FD_WRITE | FD_CLOSE;
    if(mode & IO_WAIT_ERROR)
        lEvents |= FD_CLOSE;

    if(WSAEventSelect(priv->fd,(WSAEVENT)priv->hEvent,lEvents) == SOCKET_ERROR) {
        io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
        return FALSE;
    }
    *retval = priv->hEvent;
#else
    *retval = priv->fd;
#endif
    return TRUE;
}

/**
 * return the underlying socket handle for a socket type object
 */
int io_socket_getsocket(IO_PRIVHANDLE *phandle, SOCKET_T *psock) {
    IO_SOCKET_PRIV *priv;

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle) || (!phandle->private)) {
        return FALSE;
    }

    if(!psock) {
        io_file_seterr(phandle,IO_E_SOCKET_UNKNOWN);
        return FALSE;
    }

    priv = (IO_SOCKET_PRIV*) phandle->private;
    ASSERT(priv->opened);

    if(!priv->opened) {
        io_file_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

    *psock = priv->fd;
    return TRUE;
}


/**
 * udp function, similar to recvfrom(2)
 *
 * @param phandle io device (of type "udp" or "udplisten") to recv from
 * @param buf buffer to read into
 * @param len length of buffer, returns bytes read
 * @param si_remote address of remote endpoint
 * @param si_len address length
 * @returns TRUE on success
 */
int io_udp_recvfrom(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len,
                    struct sockaddr_in *si_remote, socklen_t *si_len) {
    ssize_t bytes_read;
    IO_SOCKET_PRIV *priv;


    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle)||(!phandle->private)) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

    priv = (IO_SOCKET_PRIV*)phandle->private;
    ASSERT(priv->opened);
    ASSERT(priv->is_udp);

    if(!priv->opened) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

    if(!priv->is_udp) {
        io_socket_seterr(phandle,IO_E_SOCKET_BADFN);
        return FALSE;
    }

    while(((bytes_read = recvfrom(priv->fd, buf, *len, 0,
                                  (struct sockaddr *)si_remote,
                                  si_len)) == -1) &&
          (errno == EINTR));

    if(bytes_read == -1) {
        io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
        return FALSE;
    }

    *len = bytes_read;
    return TRUE;
}

/**
 * udp function, similar to sendto(2)
 *
 * @param phandle io device (of type "udp" or "udplisten") to recv from
 * @param buf buffer holding data to write
 * @param len length of buffer, returns bytes written
 * @param si_remote address of remote endpoint
 * @param si_len remote address length
 * @returns TRUE on success
 */
int io_udp_sendto(IO_PRIVHANDLE *phandle, unsigned char *buf, uint32_t *len,
                  struct sockaddr_in *si_remote, socklen_t si_len) {
    IO_SOCKET_PRIV *priv;
    uint32_t bytestowrite;
    ssize_t byteswritten;
    uint32_t totalbytes;
    unsigned char *bufp;

    ASSERT(phandle);
    ASSERT(phandle->private);

    if((!phandle)||(!phandle->private)) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

    priv = (IO_SOCKET_PRIV*)phandle->private;
    ASSERT(priv->opened);
    ASSERT(priv->is_udp);

    if(!priv->opened) {
        io_socket_seterr(phandle,IO_E_SOCKET_NOTOPEN);
        return FALSE;
    }

    if(!priv->is_udp) {
        io_socket_seterr(phandle,IO_E_SOCKET_BADFN);
        return FALSE;
    }

    for(bufp = buf, bytestowrite = *len, totalbytes=0;
        bytestowrite > 0;
        bufp += byteswritten, bytestowrite -= byteswritten) {
        byteswritten = sendto(priv->fd, buf, *len, 0,
                              (struct sockaddr *)si_remote, si_len);

        if((byteswritten == -1 ) && (errno != EINTR)) {
            io_socket_seterr(phandle,IO_E_SOCKET_OTHER);
            return FALSE;
        }
        if(byteswritten == -1)
            byteswritten = 0;
        totalbytes += byteswritten;
    }

    *len = totalbytes;
    return TRUE;
}



/**
 * dispose of an optionlist
 *
 * @param phandle handle for which to dispose optionlist
 */
int io_option_dispose(IO_PRIVHANDLE *phandle) {
    IO_OPTIONLIST *pcurrent;

    ASSERT(phandle);

    if(!phandle)
        return TRUE;

    io_lock();
    while(phandle->pol) {
        pcurrent = phandle->pol;
        if(pcurrent->key)   free(pcurrent->key);
        if(pcurrent->value) free(pcurrent->value);
        phandle->pol = pcurrent->next;
        free(pcurrent);
    }
    io_unlock();

    return TRUE;
}


/**
 * add an option to an option list
 *
 * @param key option to add
 * @param value value to set option to
 * @returns TRUE on success
 */
int io_option_add(IO_PRIVHANDLE *phandle, char *key, char *value) {
    IO_OPTIONLIST *pnew;

    ASSERT(phandle);
    if(!phandle) return FALSE;

    pnew = (IO_OPTIONLIST*)malloc(sizeof(IO_OPTIONLIST));
    if(!pnew) {
        io_err_printf(IO_LOG_FATAL,"Malloc error in io_option_add\n");
        return FALSE;
    }

    pnew->key = strdup(key);
    pnew->value = strdup(value);

    if((!pnew->key) || (!pnew->value)) {
        io_err_printf(IO_LOG_FATAL,"Malloc error in io_option_add\n");
        if(pnew->key)   free(pnew->key);
        if(pnew->value) free(pnew->value);
        free(pnew);

        return FALSE;
    }

    /* this could (should?) be a per-handle lock */
    io_lock();
    pnew->next = phandle->pol;
    phandle->pol = pnew;
    io_unlock();

    return TRUE;
}

/**
 * fetch an option for a specific handle
 *
 * @param key option to add
 * @param value value to set option to
 * @returns TRUE on success
 */
char* io_option_get(IO_PRIVHANDLE *phandle,char *option,char *dflt) {
    IO_OPTIONLIST *pcurrent;

    ASSERT(phandle);

    if(!phandle) return dflt;

    io_lock();
    pcurrent = phandle->pol;
    while(pcurrent && (strcasecmp(pcurrent->key,option) != 0))
        pcurrent = pcurrent->next;
    io_unlock();

    if(!pcurrent)
        return dflt;

    return pcurrent->value;
}

/**
 * see if an io device is a specific proto (mostly for proto-specific
 * functions, like io_listen_accept, or io_udp_sendto
 */
int io_isproto(IO_PRIVHANDLE *phandle, char *proto) {
    ASSERT(phandle && phandle->proto);

    if(!phandle || !phandle->proto)
        return FALSE;

    if(strcasecmp(phandle->proto, proto) == 0)
        return TRUE;

    return FALSE;
}

/**
 * make a new wait object
 *
 * @returns new wait object
 */
IO_WAITHANDLE *io_wait_new(void) {
    IO_WAITHANDLE *pnew;

    pnew = (IO_WAITHANDLE*)malloc(sizeof(IO_WAITHANDLE));
    if(!pnew) {
        io_err_printf(IO_LOG_FATAL,"malloc error in io_wait_new\n");
        return NULL;
    }

    memset(pnew,0x00,sizeof(IO_WAITHANDLE));

#ifdef WIN32
    pnew->hWaitItems = (WAITABLE_T *)malloc(sizeof(WAITABLE_T) * IO_HANDLES_START);
    if(!pnew->hWaitItems) {
        free(pnew);
        return NULL;
    }
    pnew->ppHandle = (IO_PRIVHANDLE **)malloc(sizeof(IO_PRIVHANDLE*) * IO_HANDLES_START);
    if(!pnew->ppHandle) {
        free(pnew->hWaitItems);
        free(pnew);
        return NULL;
    }

    pnew->dwItemCount = 0;
    pnew->dwMaxItems = IO_HANDLES_START;
#else
    FD_ZERO(&pnew->read_fds);
    FD_ZERO(&pnew->write_fds);
    FD_ZERO(&pnew->err_fds);
#endif

    return pnew;
}

/**
 * Add a io device to be waited on.
 *
 * @param pwait wait object to add the io device to
 * @param phandle io device to add
 * @param type action to wait for (IO_WAIT_READ/WRITE/ERROR)
 * @returns TRUE if device was successfully added
 */
int io_wait_add(IO_WAITHANDLE *pwait, IO_PRIVHANDLE *phandle, int type) {
    WAITABLE_T waitable;
#ifdef WIN32
    void *ptmp;
#endif

    ASSERT(pwait);
    ASSERT(phandle && phandle->open && phandle->fnptr &&
           phandle->fnptr->fn_getwaitable);

    if(!pwait) {
        io_err_printf(IO_LOG_WARN,"io_wait_add: bad IO_WAITHANDLE\n");
        return FALSE;
    }

    if(!phandle || !phandle->open || !phandle->fnptr ||
       !phandle->fnptr->fn_getwaitable) {
        io_err_printf(IO_LOG_WARN,"io_wait_add: impossible to get waitable\n");
        return FALSE;
    }

    io_err_printf(IO_LOG_SPAM,"Getting io waitable of type %d\n",type);
    if(!phandle->fnptr->fn_getwaitable(phandle, type, &waitable)) {
        io_err_printf(IO_LOG_WARN,"io_wait_add: error getting waitable\n");
        return FALSE;
    }

#ifdef WIN32
    ASSERT(pwait->dwItemCount <= pwait->dwMaxItems);

    while (pwait->dwMaxItems <= pwait->dwItemCount) { /* must expand our handle list */
        ptmp = realloc(pwait->hWaitItems,sizeof(WAITABLE_T) * (pwait->dwMaxItems + IO_HANDLES_GROW));
        if(!ptmp) {
            return FALSE;
        }
        pwait->hWaitItems = ptmp;
        ptmp = realloc(pwait->ppHandle,sizeof(IO_PRIVHANDLE *) * (pwait->dwMaxItems + IO_HANDLES_GROW));
        if(!ptmp) {
            return FALSE;
        }
        pwait->ppHandle = ptmp;
        pwait->dwMaxItems += IO_HANDLES_GROW;
    }

    pwait->hWaitItems[pwait->dwItemCount] = waitable;
    pwait->ppHandle[pwait->dwItemCount] = phandle;

    io_err_printf(IO_LOG_SPAM,"Added event %08X with pHandle %08X as %d\n",
        waitable,phandle,pwait->dwItemCount);

    pwait->dwItemCount++;
#else
    if(waitable > pwait->max_fd)
        pwait->max_fd = waitable;

    io_err_printf(IO_LOG_SPAM,"Adding %d to waitlist\n",waitable);

    if(type & IO_WAIT_READ)
        FD_SET(waitable,&pwait->read_fds);
    if(type & IO_WAIT_WRITE)
        FD_SET(waitable,&pwait->write_fds);
    if(type & IO_WAIT_ERROR)
        FD_SET(waitable,&pwait->err_fds);
#endif

    return TRUE;
}

/**
 * Actually wait on the objects currently added.  Returns time
 * _left_ in the wait if successful.
n *
 * @param pwait wait device to wait on
 * @param ms milliseconds to wait
 * @returns TRUE on success, FALSE and ms=0 on timeout, FALSE otherwise
 */
int io_wait(IO_WAITHANDLE *pwait, uint32_t *ms) {
    struct timeval timeout;
    struct timeval start_time, end_time;
    uint32_t elapsed_ms;
    uint32_t wait_ms;
#ifdef WIN32
    SOCKET_T sock;
#endif
    int retval=0;

    ASSERT(pwait);

    if(!pwait)
        return FALSE;

    wait_ms = *ms;
    timeout.tv_sec = wait_ms/1000;
    if(!timeout.tv_sec)
        timeout.tv_sec++;
    timeout.tv_usec = 0;

    gettimeofday(&start_time,NULL);
#ifdef WIN32
    ASSERT(pwait->dwItemCount);

    io_err_printf(IO_LOG_SPAM,"Waiting on %d items for %d sec\n",
        pwait->dwItemCount,timeout.tv_sec);

    while(1) {
        pwait->dwLastResult = WaitForMultipleObjects(pwait->dwItemCount,pwait->hWaitItems,FALSE,*ms);
        if((pwait->dwLastResult == WAIT_FAILED) || (pwait->dwLastResult == WAIT_TIMEOUT))
            break;

        pwait->dwWhichEvent = pwait->dwLastResult - WAIT_OBJECT_0;
        if(pwait->dwWhichEvent >= pwait->dwItemCount) { /* error or something */
            pwait->dwWhichEvent = pwait->dwLastResult - WAIT_ABANDONED_0;
            ASSERT(pwait->dwWhichEvent < pwait->dwItemCount);
            if(pwait->dwWhichEvent < pwait->dwItemCount)
                return FALSE;
            break;
        }

        /* Was definitely a WAIT_OBJECT_x.  Make sure it's not spurious */
        io_err_printf(IO_LOG_SPAM,"Got wait on index %d\n",pwait->dwWhichEvent);
        if(!pwait->ppHandle[pwait->dwWhichEvent]->fnptr->fn_getsocket) { /* not a socket, must be a file */
            /* dummy up a result */
            pwait->wsaNetworkEvents.lNetworkEvents = FD_READ;
            break;
        }

        if(!pwait->ppHandle[pwait->dwWhichEvent]->fnptr->fn_getsocket(pwait->ppHandle[pwait->dwWhichEvent],&sock)) {
            io_err_printf(IO_LOG_SPAM,"Could not get socket handle\n");
            return FALSE;
        }

        io_err_printf(IO_LOG_SPAM,"Getting event details for wait object\n");
        WSAEnumNetworkEvents(sock,pwait->hWaitItems[pwait->dwWhichEvent],&pwait->wsaNetworkEvents);
        if(pwait->wsaNetworkEvents.lNetworkEvents != 0) {
            io_err_printf(IO_LOG_SPAM,"Got %ld\n",pwait->wsaNetworkEvents.lNetworkEvents);
            break;
        }

        io_err_printf(IO_LOG_SPAM,"Skipping spurious wakeup\n");
    }

    if(pwait->dwLastResult == WAIT_FAILED)
        return FALSE;

    /* on timeout, return FALSE, with ms=0 */
    if(WAIT_TIMEOUT == pwait->dwLastResult) {
        *ms = 0;
        return FALSE;
    }
#else
    ASSERT(pwait->max_fd);

    memcpy(&pwait->result_read, &pwait->read_fds, sizeof(pwait->read_fds));
    memcpy(&pwait->result_write, &pwait->write_fds, sizeof(pwait->write_fds));
    memcpy(&pwait->result_err, &pwait->err_fds, sizeof(pwait->err_fds));

    if(!pwait->max_fd) {
        io_err_printf(IO_LOG_WARN,"No fds being monitored in io_wait\n");
        return FALSE;
    }

    io_err_printf(IO_LOG_SPAM,"selecting on %d nfds, for %d.%d sec\n",
                  pwait->max_fd+1,timeout.tv_sec,timeout.tv_usec);

    while(((retval = select(pwait->max_fd+1,&pwait->result_read,
                            &pwait->result_write, &pwait->result_err,
                            &timeout)) == -1) &&
          (errno == EINTR));

    if(retval == -1) {
        io_err_printf(IO_LOG_WARN,"Error in select: %s\n",strerror(errno));
        return FALSE;
    }

    if(retval == 0) {
        io_err_printf(IO_LOG_WARN,"timeout in select\n");
        *ms = 0;
        return FALSE;
    }
#endif

    gettimeofday(&end_time,NULL);
    elapsed_ms = ((end_time.tv_sec - start_time.tv_sec) * 1000) +
        ((end_time.tv_usec - start_time.tv_usec)/1000);
    if(elapsed_ms > wait_ms)
        *ms = 0;
    else
        *ms -= elapsed_ms;

    return TRUE;
}

/**
 * return a flagset for what events were signalled by an io handle
 *
 * @param pwait wait object that was just io_wait'ed
 * @param phandle io device to get flagset for
 * @returns flagset (IO_WAIT_READ/WRITE/ERROR)
 */
int io_wait_status(IO_WAITHANDLE *pwait, IO_PRIVHANDLE *phandle) {
    int retval = 0;

#ifndef WIN32
    WAITABLE_T waitable;
#endif

    ASSERT(pwait);

    if(!pwait) {
        io_err_printf(IO_LOG_WARN,"io_wait_status: invalid IO_WAITHANDLE\n");
        return 0;
    }

    ASSERT(phandle && phandle->open && phandle->fnptr &&
           phandle->fnptr->fn_getwaitable);

    if(!phandle || !phandle->open || !phandle->fnptr ||
       !phandle->fnptr->fn_getwaitable) {
        io_err_printf(IO_LOG_WARN,"io_wait_status: can't get WAITABLE_T for "
                      "io device.\n");
        return 0;
    }

#ifdef WIN32
    io_err_printf(IO_LOG_DEBUG,"lnetwork: %08x\n",pwait->wsaNetworkEvents.lNetworkEvents);

    if(pwait->wsaNetworkEvents.lNetworkEvents == FD_READ) {
        retval |= IO_WAIT_READ;
        io_err_printf(IO_LOG_DEBUG,"Set: FD_READ\n");
    }
    if(pwait->wsaNetworkEvents.lNetworkEvents == FD_ACCEPT) {
        retval |= IO_WAIT_READ;
        io_err_printf(IO_LOG_DEBUG,"Set: FD_ACCEPT\n");
    }
    if(pwait->wsaNetworkEvents.lNetworkEvents == FD_OOB) {
        retval |= IO_WAIT_READ;
        io_err_printf(IO_LOG_DEBUG,"Set: FD_OOB\n");
    }
    if(pwait->wsaNetworkEvents.lNetworkEvents == FD_WRITE) {
        retval |= IO_WAIT_WRITE;
        io_err_printf(IO_LOG_DEBUG,"Set: FD_WRITE\n");
    }
    if(pwait->wsaNetworkEvents.lNetworkEvents == FD_CLOSE) {
        retval |= IO_WAIT_READ | IO_WAIT_ERROR | IO_WAIT_WRITE;
        io_err_printf(IO_LOG_DEBUG,"Set: FD_CLOSE\n");
    }

    io_err_printf(IO_LOG_DEBUG,"Wait status: %d\n",retval);
    /*
    if(wsaNetworkEvents.iErrorCode[FD_READ_BIT])
        retval |= IO_WAIT_ERROR;
    if(wsaNetworkEvents.iErrorCode[FD_ACCEPT_BIT])
        retval |= IO_WAIT_ERROR;
    if(wsaNetworkEvents.iErrorCode[FD_OOB_BIT])
        retval |= IO_WAIT_ERROR;
    if(wsaNetworkEvents.iErrorCode[FD_WRITE_BIT])
        retval |= IO_WAIT_ERROR;
    if(wsaNetworkEvents.iErrorCode[FD_CLOSE_BIT])
        retval |= IO_WAIT_ERROR;
    */
#else
    if(!phandle->fnptr->fn_getwaitable(phandle, 1, &waitable)) /* get underlying fd */
        return 0;

    io_err_printf(IO_LOG_DEBUG,"Checking status of fd %d\n",waitable);
    if(FD_ISSET(waitable,&pwait->result_read))
        retval |= IO_WAIT_READ;
    if(FD_ISSET(waitable,&pwait->result_write))
        retval |= IO_WAIT_WRITE;
    if(FD_ISSET(waitable,&pwait->result_err))
        retval |= IO_WAIT_ERROR;
    io_err_printf(IO_LOG_DEBUG,"Returning %d\n",retval);
#endif

    return retval;
}

/**
 * dispose of a wait object
 *
 * @param pwait object to dispose
 * @returns TRUE on success
 */
int io_wait_dispose(IO_WAITHANDLE *pwait) {
    ASSERT(pwait);

    if(!pwait)
        return TRUE;

#ifdef WIN32
    if(pwait->hWaitItems)
        free(pwait->hWaitItems);
    if(pwait->ppHandle)
        free(pwait->ppHandle);
#endif

    free(pwait);
    return TRUE;
}
