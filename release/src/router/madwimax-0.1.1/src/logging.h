/*
 * This is a reverse-engineered driver for mobile WiMAX (802.16e) devices
 * based on Samsung CMC-730 chip.
 * Copyright (C) 2008-2009 Alexander Gordeev <lasaine@lvk.cs.msu.su>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _LOGGING_H
#define _LOGGING_H

#include <stdio.h>

/* get printable madwimax version */
const char* get_madwimax_version();

/* set wmlog level to the desired value */
void set_wmlog_level(int level);

/* increase wmlog level by 1 */
void inc_wmlog_level();

/* set logger */
#define WMLOGGER_FILE	0
#define WMLOGGER_SYSLOG	1
void set_wmlogger(const char *progname, int logger, FILE *file);

/* print wmlog message. */
void wmlog_msg(int level, const char *fmt, ...);

/* dump message and len bytes from buf in hexadecimal and ASCII. */
void wmlog_dumphexasc(int level, const void *buf, int len, const char *fmt, ...);

#endif /* _LOGGING_H */

