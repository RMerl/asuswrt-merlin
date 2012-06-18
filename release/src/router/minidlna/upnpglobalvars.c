/* MiniDLNA project
 *
 * http://sourceforge.net/projects/minidlna/
 *
 * MiniDLNA media server
 * Copyright (C) 2008-2009  Justin Maggard
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
 *
 * Portions of the code from the MiniUPnP project:
 *
 * Copyright (c) 2006-2007, Thomas Bernard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <linux/limits.h>

#include "config.h"
#include "upnpglobalvars.h"
#include "upnpdescstrings.h"

/* LAN address */
/*const char * listen_addr = 0;*/

/* startup time */
time_t startup_time = 0;

struct runtime_vars_s runtime_vars;
uint32_t runtime_flags = INOTIFY_MASK;

const char * pidfilename = "/var/run/minidlna.pid";

char uuidvalue[] = "uuid:00000000-0000-0000-0000-000000000000";
char modelname[MODELNAME_MAX_LEN] = ROOTDEV_MODELNAME;
char modelnumber[MODELNUMBER_MAX_LEN] = MINIDLNA_VERSION;
char serialnumber[SERIALNUMBER_MAX_LEN] = "00000000";
#if PNPX
char pnpx_hwid[] = "VEN_0000&amp;DEV_0000&amp;REV_01 VEN_0033&amp;DEV_0001&amp;REV_01";
#endif

/* presentation url :
 * http://nnn.nnn.nnn.nnn:ppppp/  => max 30 bytes including terminating 0 */
char presentationurl[PRESENTATIONURL_MAX_LEN];

int n_lan_addr = 0;
struct lan_addr_s lan_addr[MAX_LAN_ADDR];

/* Path of the Unix socket used to communicate with MiniSSDPd */
const char * minissdpdsocketpath = "/var/run/minissdpd.sock";

/* UPnP-A/V [DLNA] */
sqlite3 * db;
char friendly_name[FRIENDLYNAME_MAX_LEN];
char db_path[PATH_MAX] = {'\0'};
char log_path[PATH_MAX] = {'\0'};
struct media_dir_s * media_dirs = NULL;
struct album_art_name_s * album_art_names = NULL;
struct client_cache_s clients[CLIENT_CACHE_SLOTS];
short int scanning = 0;
volatile short int quitting = 0;
volatile uint32_t updateID = 0;
