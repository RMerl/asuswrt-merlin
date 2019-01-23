/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single TCP/UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2017 OpenVPN Technologies, Inc. <sales@openvpn.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef STATUS_H
#define STATUS_H

#include "interval.h"

//Sam.B	2013/10/31
#define TYPEDEF_BOOL	//will skip in typedefs.h
#include <unistd.h>
#include <bcmnvram.h>
#include "shared.h"

#define EXIT_GOOD		0
#define EXIT_ERROR		1
#define ADDR_CONFLICTED		2
#define ROUTE_CONFLICTED	3
#define RUNNING			4
#define SSLPARAM_ERROR	5
#define SSLPARAM_DH_ERROR	6
#define RCV_AUTH_FAILED_ERROR	7

#define ST_EXIT			0
#define ST_INIT			1
#define ST_RUNNING		2
#define ST_ERROR		-1

#define ERRNO_DEFAULT		0
#define ERRNO_IP		1
#define ERRNO_ROUTE		2
#define ERRNO_SSL		4
#define ERRNO_DH		5
#define ERRNO_AUTH		6

void update_nvram_status(int flag);
int current_addr(in_addr_t addr);
int current_route(in_addr_t network, in_addr_t netmask);
//Sam.E	2013/10/31

/*
 * virtual function interface for status output
 */
struct virtual_output {
    void *arg;
    unsigned int flags_default;
    void (*func) (void *arg, const unsigned int flags, const char *str);
};

static inline void
virtual_output_print(const struct virtual_output *vo, const unsigned int flags, const char *str)
{
    (*vo->func)(vo->arg, flags, str);
}

/*
 * printf-style interface for inputting/outputting status info
 */

struct status_output
{
#define STATUS_OUTPUT_READ  (1<<0)
#define STATUS_OUTPUT_WRITE (1<<1)
    unsigned int flags;

    char *filename;
    int fd;
    int msglevel;
    const struct virtual_output *vout;

    struct buffer read_buf;

    struct event_timeout et;

    bool errors;
};

struct status_output *status_open(const char *filename,
                                  const int refresh_freq,
                                  const int msglevel,
                                  const struct virtual_output *vout,
                                  const unsigned int flags);

bool status_trigger_tv(struct status_output *so, struct timeval *tv);

bool status_trigger(struct status_output *so);

void status_reset(struct status_output *so);

void status_flush(struct status_output *so);

bool status_close(struct status_output *so);

void status_printf(struct status_output *so, const char *format, ...)
#ifdef __GNUC__
#if __USE_MINGW_ANSI_STDIO
__attribute__ ((format(gnu_printf, 2, 3)))
#else
__attribute__ ((format(__printf__, 2, 3)))
#endif
#endif
;

bool status_read(struct status_output *so, struct buffer *buf);

static inline unsigned int
status_rw_flags(const struct status_output *so)
{
    if (so)
    {
        return so->flags;
    }
    else
    {
        return 0;
    }
}

#endif /* ifndef STATUS_H */
