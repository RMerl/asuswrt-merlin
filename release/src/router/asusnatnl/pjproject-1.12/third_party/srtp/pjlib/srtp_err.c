/* $Id: srtp_err.c 1907 2008-04-03 22:03:14Z bennylp $ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#include "err.h"
#include <pj/log.h>

/* Redirect libsrtp error to PJ_LOG */

err_reporting_level_t err_level = err_level_none;

err_status_t
err_reporting_init(char *ident) {
    return err_status_ok;
}

void
err_report(int priority, char *format, ...) {
  va_list args;

#if PJ_LOG_MAX_LEVEL >= 1
  if (priority <= err_level) {

    va_start(args, format);
    pj_log("libsrtp", priority, format, args);
    va_end(args);
  }
#endif
}

void
err_reporting_set_level(err_reporting_level_t lvl) { 
  err_level = lvl;
}

