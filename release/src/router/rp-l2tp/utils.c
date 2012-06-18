/***********************************************************************
*
* utils.c
*
* Utility functions for l2tp
*
* Copyright (C) 2002 Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: utils.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include "l2tp.h"

#define MAX_ERRMSG_LEN 512

static int random_fd = -1;
static char errmsg[MAX_ERRMSG_LEN];

struct sd_handler {
    l2tp_shutdown_func f;
    void *data;
};

static struct sd_handler shutdown_handlers[16];

static int n_shutdown_handlers = 0;

int
l2tp_register_shutdown_handler(l2tp_shutdown_func f, void *data)
{
    if (n_shutdown_handlers == 16) return -1;
    shutdown_handlers[n_shutdown_handlers].f = f;
    shutdown_handlers[n_shutdown_handlers].data = data;
    n_shutdown_handlers++;
    return n_shutdown_handlers;
}

void
l2tp_cleanup(void)
{
    int i;
    for (i=0; i<n_shutdown_handlers; i++) {
	shutdown_handlers[i].f(shutdown_handlers[i].data);
    }
}

char const *
l2tp_get_errmsg(void)
{
    return errmsg;
}

/**********************************************************************
* %FUNCTION: set_errmsg
* %ARGUMENTS:
*  fmt -- printf format
*  ... -- format args
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Sets static errmsg string
***********************************************************************/
void
l2tp_set_errmsg(char const *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(errmsg, MAX_ERRMSG_LEN, fmt, ap);
    va_end(ap);
    errmsg[MAX_ERRMSG_LEN-1] = 0;
    fprintf(stderr, "Error: %s\n", errmsg);
    
    vsyslog(LOG_ERR, fmt, ap);
}

/**********************************************************************
* %FUNCTION: random_init
* %ARGUMENTS:
*  None
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Sets up random-number generator
***********************************************************************/
void
l2tp_random_init(void)
{
    /* Prefer /dev/urandom; fall back on rand() */
    random_fd = open("/dev/urandom", O_RDONLY);
    if (random_fd < 0) {
	srand(time(NULL) + getpid()*getppid());
    }

}

/**********************************************************************
* %FUNCTION: bad random_fill
* %ARGUMENTS:
*  ptr -- pointer to a buffer
*  size -- size of buffer
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Fills buffer with "size" random bytes.  This function is not
*  cryptographically strong; it's used as a fallback for systems
*  without /dev/urandom.
***********************************************************************/
static void
bad_random_fill(void *ptr, size_t size)
{
    unsigned char *buf = ptr;
    while(size--) {
	*buf++ = rand() & 0xFF;
    }
}

/**********************************************************************
* %FUNCTION: random_fill
* %ARGUMENTS:
*  ptr -- pointer to a buffer
*  size -- size of buffer
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Fills buffer with "size" random bytes.
***********************************************************************/
void
l2tp_random_fill(void *ptr, size_t size)
{
    int n;
    int ndone = 0;
    int nleft = size;
    unsigned char *buf = ptr;

    if (random_fd < 0) {
	bad_random_fill(ptr, size);
	return;
    }

    while(nleft) {
	n = read(random_fd, buf+ndone, nleft);
	if (n <= 0) {
	    close(random_fd);
	    random_fd = -1;
	    bad_random_fill(buf+ndone, nleft);
	    return;
	}
	nleft -= n;
	ndone += n;
    }
}

void l2tp_die(void)
{
    fprintf(stderr, "FATAL: %s\n", errmsg);
    l2tp_cleanup();
    exit(1);
}

/**********************************************************************
* %FUNCTION: load_handler
* %ARGUMENTS:
*  fname -- filename to load
* %RETURNS:
*  -1 on error, 0 if OK
* %DESCRIPTION:
*  Dynamically-loads a handler and initializes it.  If fname is not
*  an absolute path name, we load the handler from /usr/lib/l2tp/plugins
***********************************************************************/
int
l2tp_load_handler(EventSelector *es,
		  char const *fname)
{
    char buf[1024];
    void *handle;
    void *init;
    void (*init_fn)(EventSelector *);

    if (*fname == '/') {
	handle = dlopen(fname, RTLD_NOW);
    } else {
	/* ASUS snprintf(buf, sizeof(buf), PREFIX"/lib/l2tp/plugins/%s", fname); */
	snprintf(buf, sizeof(buf), PREFIX"/lib/l2tp/%s", fname);
	buf[sizeof(buf)-1] = 0;
	handle = dlopen(buf, RTLD_NOW);
    }

    if (!handle) {
	l2tp_set_errmsg("Could not dload %s: %s",
			fname, dlerror());
	return -1;
    }

    init = dlsym(handle, "handler_init");
    if (!init) {
	dlclose(handle);
	l2tp_set_errmsg("No handler_init found in %s", fname);
	return -1;
    }
    init_fn = (void (*)(EventSelector *)) init;
    init_fn(es);
    return 0;
}
