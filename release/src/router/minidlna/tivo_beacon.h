/* TiVo discovery
 *
 * Project : minidlna
 * Website : http://sourceforge.net/projects/minidlna/
 * Author  : Justin Maggard
 *
 * MiniDLNA media server
 * Copyright (C) 2009  Justin Maggard
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#ifdef TIVO_SUPPORT
/*
 *  * A saved copy of a beacon from another tivo or another server
 *   */
struct aBeacon
{
#ifdef DEBUG
   time_t lastSeen;
#endif
   char * machine;
   char * identity;
   struct aBeacon *next;
};

uint32_t
getBcastAddress();

int
OpenAndConfTivoBeaconSocket();

void
sendBeaconMessage(int fd, struct sockaddr_in * client, int len, int broadcast);

void
ProcessTiVoBeacon(int fd);
#endif
