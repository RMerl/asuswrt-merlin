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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "options.h"
#include "upnpglobalvars.h"

struct option * ary_options = NULL;
int num_options = 0;

static const struct {
	enum upnpconfigoptions id;
	const char * name;
} optionids[] = {
	{ UPNPIFNAME, "network_interface" },
	{ UPNPLISTENING_IP, "listening_ip" },
	{ UPNPPORT, "port" },
	{ UPNPPRESENTATIONURL, "presentation_url" },
	{ UPNPNOTIFY_INTERVAL, "notify_interval" },
	{ UPNPSYSTEM_UPTIME, "system_uptime" },
	{ UPNPUUID, "uuid"},
	{ UPNPSERIAL, "serial"},
	{ UPNPMODEL_NAME, "model_name"},
	{ UPNPMODEL_NUMBER, "model_number"},
	{ UPNPFRIENDLYNAME, "friendly_name"},
	{ UPNPMEDIADIR, "media_dir"},
	{ UPNPALBUMART_NAMES, "album_art_names"},
	{ UPNPINOTIFY, "inotify" },
	{ UPNPDBDIR, "db_dir" },
	{ UPNPLOGDIR, "log_dir" },
	{ UPNPLOGLEVEL, "log_level" },
	{ UPNPMINISSDPDSOCKET, "minissdpdsocket"},
	{ ENABLE_TIVO, "enable_tivo" },
	{ ENABLE_DLNA_STRICT, "strict_dlna" },
	{ ROOT_CONTAINER, "root_container" }
};

int
readoptionsfile(const char * fname)
{
	FILE *hfile = NULL;
	char buffer[1024];
	char *equals;
	char *name;
	char *value;
	char *t;
	int linenum = 0;
	int i;
	enum upnpconfigoptions id;

	if(!fname || (strlen(fname) == 0))
		return -1;

	memset(buffer, 0, sizeof(buffer));

#ifdef DEBUG
	printf("Reading configuration from file %s\n", fname);
#endif

	if(!(hfile = fopen(fname, "r")))
		return -1;

	if(ary_options != NULL)
	{
		free(ary_options);
		num_options = 0;
	}

	while(fgets(buffer, sizeof(buffer), hfile))
	{
		linenum++;
		t = strchr(buffer, '\n'); 
		if(t)
		{
			*t = '\0';
			t--;
			while((t >= buffer) && isspace(*t))
			{
				*t = '\0';
				t--;
			}
		}
       
		/* skip leading whitespaces */
		name = buffer;
		while(isspace(*name))
			name++;

		/* check for comments or empty lines */
		if(name[0] == '#' || name[0] == '\0') continue;

		if(!(equals = strchr(name, '=')))
		{
			fprintf(stderr, "parsing error file %s line %d : %s\n",
			        fname, linenum, name);
			continue;
		}

		/* remove ending whitespaces */
		for(t=equals-1; t>name && isspace(*t); t--)
			*t = '\0';

		*equals = '\0';
		value = equals+1;

		/* skip leading whitespaces */
		while(isspace(*value))
			value++;

		id = UPNP_INVALID;
		for(i=0; i<sizeof(optionids)/sizeof(optionids[0]); i++)
		{
			/*printf("%2d %2d %s %s\n", i, optionids[i].id, name,
			       optionids[i].name); */

			if(0 == strcmp(name, optionids[i].name))
			{
				id = optionids[i].id;
				break;
			}
		}

		if(id == UPNP_INVALID)
		{
			fprintf(stderr, "parsing error file %s line %d : %s=%s\n",
			        fname, linenum, name, value);
		}
		else
		{
			num_options += 1;
			ary_options = (struct option *) realloc(ary_options, num_options * sizeof(struct option));

			ary_options[num_options-1].id = id;
			strncpy(ary_options[num_options-1].value, value, MAX_OPTION_VALUE_LEN);
		}

	}
	
	fclose(hfile);
	
	return 0;
}

void
freeoptions(void)
{
	if(ary_options)
	{
		free(ary_options);
		ary_options = NULL;
		num_options = 0;
	}
}

