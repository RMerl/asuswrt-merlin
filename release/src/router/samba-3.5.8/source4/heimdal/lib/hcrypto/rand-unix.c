/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <rand.h>
#include <heim_threads.h>

#include <roken.h>

#include "randi.h"

static int random_fd = -1;
static HEIMDAL_MUTEX random_mutex = HEIMDAL_MUTEX_INITIALIZER;

/*
 * Unix /dev/random
 */

static int
get_device_fd(int flags)
{
    static const char *rnd_devices[] = {
	"/dev/urandom",
	"/dev/random",
	"/dev/srandom",
	"/dev/arandom",
	NULL
    };
    const char **p;

    for(p = rnd_devices; *p; p++) {
	int fd = open(*p, flags | O_NDELAY);
	if(fd >= 0) {
	    rk_cloexec(fd);
	    return fd;
	}
    }
    return -1;
}

static void
unix_seed(const void *indata, int size)
{
    int fd;

    if (size <= 0)
	return;

    fd = get_device_fd(O_WRONLY);
    if (fd < 0)
	return;

    write(fd, indata, size);
    close(fd);

}


static int
unix_bytes(unsigned char *outdata, int size)
{
    ssize_t count;
    int once = 0;

    if (size <= 0)
	return 0;

    HEIMDAL_MUTEX_lock(&random_mutex);
    if (random_fd == -1) {
    retry:
	random_fd = get_device_fd(O_RDONLY);
	if (random_fd < 0) {
	    HEIMDAL_MUTEX_unlock(&random_mutex);
	    return 0;
	}
    }

    while (size > 0) {
	HEIMDAL_MUTEX_unlock(&random_mutex);
	count = read (random_fd, outdata, size);
	HEIMDAL_MUTEX_lock(&random_mutex);
	if (random_fd < 0) {
	    if (errno == EINTR)
		continue;
	    else if (errno == EBADF && once++ == 0) {
		close(random_fd);
		random_fd = -1;
		goto retry;
	    }
	    return 0;
	} else if (count <= 0) {
	    HEIMDAL_MUTEX_unlock(&random_mutex);
	    return 0;
	}
	outdata += count;
	size -= count;
    }
    HEIMDAL_MUTEX_unlock(&random_mutex);

    return 1;
}

static void
unix_cleanup(void)
{
}

static void
unix_add(const void *indata, int size, double entropi)
{
    unix_seed(indata, size);
}

static int
unix_pseudorand(unsigned char *outdata, int size)
{
    return unix_bytes(outdata, size);
}

static int
unix_status(void)
{
    int fd;

    fd = get_device_fd(O_RDONLY);
    if (fd < 0)
	return 0;
    close(fd);

    return 1;
}

const RAND_METHOD hc_rand_unix_method = {
    unix_seed,
    unix_bytes,
    unix_cleanup,
    unix_add,
    unix_pseudorand,
    unix_status
};

const RAND_METHOD *
RAND_unix_method(void)
{
    return &hc_rand_unix_method;
}
