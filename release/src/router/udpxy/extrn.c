/* @(#) external common resources (variables and constants)
 *
 * Copyright 2008-2011 Pavel V. Cherenkov (pcherenkov@gmail.com)
 *
 *  This file is part of udpxy.
 *
 *  udpxy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  udpxy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with udpxy.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>

/* server commands
 */
const char  CMD_UDP[]            = "udp";
const char  CMD_STATUS[]         = "status";
const char  CMD_RESTART[]        = "restart";
const char  CMD_RTP[]            = "rtp";

const size_t CMD_UDP_LEN         = sizeof(CMD_UDP);
const size_t CMD_STATUS_LEN      = sizeof(CMD_STATUS);
const size_t CMD_RESTART_LEN     = sizeof(CMD_RESTART);
const size_t CMD_RTP_LEN         = sizeof(CMD_RTP);

const char UDPXY_COPYRIGHT_NOTICE[] =
    "udpxy and udpxrec are Copyright (C) 2008-2013 Pavel V. Cherenkov and licensed under GNU GPLv3";
const char UDPXY_CONTACT[] =
    "Contact: www.udpxy.com/forum; support@udpxy.com";

#ifndef TRACE_MODULE
  const char COMPILE_MODE[] = "lean";
#else
  const char COMPILE_MODE[] = "standard";
#endif

const char   g_udpxy_app[]  = "udpxy";

#ifdef UDPXREC_MOD
const char   g_udpxrec_app[] = "udpxrec";
#endif

const char IPv4_ALL[] = "0.0.0.0";

const char VERSION[] =
#include "VERSION"
;

const int BUILDNUM =
#include "BUILD"
;

const int PATCH =
#include "PATCH"
;

const char BUILD_TYPE[] =
#include "BLDTYPE"
;

/* application log */
FILE*  g_flog = NULL;

/* signal-handler set quit flag */
volatile sig_atomic_t g_quit = 0;


/* __EOF__ */

