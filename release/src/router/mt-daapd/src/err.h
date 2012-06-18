/*
 * $Id: err.h,v 1.1 2009-06-30 02:31:08 steven Exp $
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
#define LOGDEST_STDERR       0  /**< Log to stderr */
#define LOGDEST_SYSLOG       1  /**< Log to syslog */
#define LOGDEST_LOGFILE      2  /**< Log to logfile */

/** @anchor log_levels */
#define E_SPAM         10   /**< Logorrhea! */
#define E_DBG           9   /**< Way too verbose */
#define E_INF           5   /**< Good info, not too much spam */
#define E_WARN          2   /**< Reasonably important, but not enough to log */
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

#define L_MISC    0x80000000 /**< anything else */

extern int err_debuglevel;

extern void err_log(int level, unsigned int cat, char *fmt, ...);
extern void err_setdest(char *app, int destination);
extern int err_setdebugmask(char *list);
/**
 * Print a debugging or log message
 */
#ifdef DEBUG
# define DPRINTF(level, cat, fmt, arg...)				\
    { err_log(level,cat,"%s, %d: ",__FILE__,__LINE__); err_log(level,cat,fmt,##arg); }
#else
# define DPRINTF(level, cat, fmt, arg...)	\
    { err_log(level,cat,fmt,##arg); }
#endif

#ifdef DEBUG_MEMORY
# ifndef __IN_ERR__
#  define malloc(x) err_malloc(__FILE__,__LINE__,x)
#  define strdup(x) err_strdup(__FILE__,__LINE__,x)
#  define free(x) err_free(__FILE__,__LINE__,x)
# endif /* __IN_ERR__ */

# define MEMNOTIFY(x) err_notify(__FILE__,__LINE__,x)

extern void *err_malloc(char *file, int line, size_t size);
extern char *err_strdup(char *file, int line, const char *str);
extern void err_free(char *file, int line, void *ptr);
extern void err_notify(char *file, int line, void *ptr);
extern void err_leakcheck(void);
#else
/**
 * Notify the leak checking system of externally allocated memory.
 */
# define MEMNOTIFY(x)
#endif /* DEBUG_MEMORY */
#endif /* __ERR_H__ */
