/*
 * $Id: daapd.h 1629 2007-08-09 06:33:41Z rpedde $
 * Header info for daapd server
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file daapd.h
 *
 * header file for main.c.  Why it isn't main.h, I don't know.
 * In fact...
 *
 * \todo make daapd.h into main.h
 */

#ifndef _DAAPD_H_
#define _DAAPD_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#ifdef DEBUG
#  ifndef ASSERT
#    define ASSERT(f)         \
        if(f)                 \
            {}                \
        else                  \
            err_log(0,"Assert error in %s, line %d\n",__FILE__,__LINE__)
#  endif /* ndef ASSERT */
#else /* ndef DEBUG */
#  ifndef ASSERT
#    define ASSERT(f)
#  endif
#endif

#include <time.h>
#include "memdebug.h"  /* redefine free/malloc/etc */
#include "compat.h"
#include "webserver.h"

/** Simple struct for holding stat info.
 * \todo wire up the tag_stats#bytes_served stuff into r_write() in restart.c
 */
typedef struct tag_stats {
    time_t start_time;          /**< When the server was started */
    int songs_served;           /**< How many songs have been served */

    unsigned int gb_served;     /**< How many gigs of data have been served (unused) */
    unsigned int bytes_served;  /**< How many bytes of data served (unused) */
} STATS;

/** Global config struct */
typedef struct tag_config {
    int use_mdns;         /**< Should we do rendezvous advertisements? */
    int stop;             /**< Time to exit? */
    int reload;           /**< Time to reload and/or rescan the database? */
    int foreground;       /**< Whether or not we are running in foreground */
    int full_reload;      /**< Whether the reload should be a full one */

    STATS stats;          /**< Stats structure (see above) */
    WSHANDLE server;      /**< webserver handle */
} CONFIG;

extern CONFIG config;

#endif /* _DAAPD_H_ */
