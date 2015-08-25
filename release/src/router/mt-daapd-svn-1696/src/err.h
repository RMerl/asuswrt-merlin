/*
 * $Id: err.h 1363 2006-08-25 03:37:03Z rpedde $
 * Error related routines
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
 * \file err.h
 *
 * Header file for err.c
 */

#ifndef __ERR_H__
#define __ERR_H__

/** @anchor log_dests */
#define LOGDEST_STDERR       1  /**< Log to stderr */
#define LOGDEST_SYSLOG       2  /**< Log to syslog/eventviewer */
#define LOGDEST_LOGFILE      4  /**< Log to logfile */

/** @anchor log_levels */
#define E_SPAM         10   /**< Logorrhea! */
#define E_DBG           9   /**< Way too verbose */
#define E_INF           5   /**< Good info, not too much spam */
#define E_WARN          2   /**< Goes in text log, but not syslog */
#define E_LOG           1   /**< Something that should go in a log file */
#define E_FATAL         0   /**< Log and force an exit */

/** @anchor log_categories */
#define L_CONF    0x00000001 /**< configuration - configfile.c */
#define L_WS      0x00000002 /**< webserver - webserver.c */
#define L_DB      0x00000004 /**< database - db-gdbm.c, db-memory.c */
#define L_SCAN    0x00000008 /**< scanner - mp3-scanner.c */
#define L_QRY     0x00000010 /**< query - query.c */
#define L_IND     0x00000020 /**< index - daap.c */
#define L_BROW    0x00000040 /**< browse - daap.c, query.c */
#define L_PL      0x00000080 /**< playlists - playlist.c, lexer.l, parser.y */
#define L_ART     0x00000100 /**< cover art - dynamic-art.c */
#define L_DAAP    0x00000200 /**< Generally daap related - main.c, daap.c, query.c */
#define L_MAIN    0x00000400 /**< setup, teardown, signals - main.c */
#define L_REND    0x00000800 /**< rendezvous -- rend-unix.c, rend-posix.c, etc */
#define L_XML     0x00001000 /**< xml - xml-rpc.c */
#define L_PARSE   0x00002000 /**< smart playlist parser */
#define L_PLUG    0x00004000 /**< plugins */
#define L_LOCK    0x00008000 /**< semaphore locks */
#define L_MISC    0x80000000 /**< anything else */

#ifndef TRUE
# define TRUE 1
# define FALSE 0
#endif

extern void err_log(int level, unsigned int cat, char *fmt, ...);
extern void err_reopen(void); /** rotate logfile */
extern void err_setdest(int destination);
extern int err_getdest(void);
extern void err_setlevel(int level);
extern int err_getlevel(void);
extern int err_setdebugmask(char *list);
extern int err_setlogfile(char *file);
extern int err_settruncate(int truncate);

/**
 * Print a debugging or log message
 */

#define DPRINTF err_log

#ifdef ERR_LEAN
# define os_syslog(a,b)
# define os_opensyslog(a)
# define os_closesyslog(a)
#endif

#endif /* __ERR_H__ */
