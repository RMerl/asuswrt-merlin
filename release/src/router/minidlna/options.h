/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * author: Ryan Wagoner
 *
 * Copyright (c) 2006, Thomas Bernard
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
#ifndef __OPTIONS_H__
#define __OPTIONS_H__

#include "config.h"

/* enum of option available in the miniupnpd.conf */
enum upnpconfigoptions {
	UPNP_INVALID = 0,
	UPNPIFNAME = 1,			/* ext_ifname */
	UPNPLISTENING_IP,		/* listening_ip */
	UPNPPORT,			/* port */
	UPNPPRESENTATIONURL,		/* presentation_url */
	UPNPNOTIFY_INTERVAL,		/* notify_interval */
	UPNPUUID,			/* uuid */
	UPNPSERIAL,			/* serial */
	UPNPMODEL_NAME,			/* model_name */
	UPNPMODEL_NUMBER,		/* model_number */
	UPNPFRIENDLYNAME,		/* how the system should show up to DLNA clients */
	UPNPMEDIADIR,			/* directory to search for UPnP-A/V content */
	UPNPALBUMART_NAMES,		/* list of '/'-delimited file names to check for album art */
	UPNPINOTIFY,			/* enable inotify on the media directories */
	UPNPDBDIR,			/* base directory to store the database and album art cache */
	UPNPLOGDIR,			/* base directory to store the log file */
	UPNPLOGLEVEL,			/* logging verbosity */
	UPNPMINISSDPDSOCKET,		/* minissdpdsocket */
	ENABLE_TIVO,			/* enable support for streaming images and music to TiVo */
	ENABLE_DLNA_STRICT,		/* strictly adhere to DLNA specs */
	ROOT_CONTAINER,			/* root ObjectID (instead of "0") */
	USER_ACCOUNT			/* user account to run as */
};

/* readoptionsfile()
 * parse and store the option file values
 * returns: 0 success, -1 failure */
int
readoptionsfile(const char * fname);

/* freeoptions() 
 * frees memory allocated to option values */
void
freeoptions(void);

#define MAX_OPTION_VALUE_LEN (200)
struct option
{
	enum upnpconfigoptions id;
	char value[MAX_OPTION_VALUE_LEN];
};

extern struct option * ary_options;
extern int num_options;

#endif

