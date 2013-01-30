/* $Id: options.c,v 1.20 2008/10/06 13:22:02 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * author: Ryan Wagoner
 * (c) 2006 Thomas Bernard 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include "options.h"
#include "upnppermissions.h"
#include "upnpglobalvars.h"

struct option * ary_options = NULL;
int num_options = 0;

static const struct {
	enum upnpconfigoptions id;
	const char * name;
} optionids[] = {
	{ UPNPEXT_IFNAME, "ext_ifname" },
	{ UPNPEXT_IP,	"ext_ip" },
	{ UPNPLISTENING_IP, "listening_ip" },
	{ UPNPPORT, "port" },
	{ UPNPBITRATE_UP, "bitrate_up" },
	{ UPNPBITRATE_DOWN, "bitrate_down" },
	{ UPNPPRESENTATIONURL, "presentation_url" },
	{ UPNPNOTIFY_INTERVAL, "notify_interval" },
	{ UPNPSYSTEM_UPTIME, "system_uptime" },
	{ UPNPPACKET_LOG, "packet_log" },
	{ UPNPUUID, "uuid"},
	{ UPNPSERIAL, "serial"},
	{ UPNPMODEL_NUMBER, "model_number"},
	{ UPNPFRIENDLYNAME, "friendly_name"},
	{ UPNPCLEANTHRESHOLD, "clean_ruleset_threshold"},
	{ UPNPCLEANINTERVAL, "clean_ruleset_interval"},
#ifdef USE_NETFILTER
	{ UPNPFORWARDCHAIN, "upnp_forward_chain"},
	{ UPNPNATCHAIN, "upnp_nat_chain"},
#endif
#ifdef ENABLE_NATPMP
	{ UPNPENABLENATPMP, "enable_natpmp"},
#endif
	{ UPNPENABLE, "enable_upnp"},
#ifdef USE_PF
	{ UPNPQUEUE, "queue"},
	{ UPNPTAG, "tag"},
#endif
#ifdef PF_ENABLE_FILTER_RULES
	{ UPNPQUICKRULES, "quickrules" },
#endif
#ifdef ENABLE_LEASEFILE
	{ UPNPLEASEFILE, "lease_file"},
#endif
	{ UPNPMINISSDPDSOCKET, "minissdpdsocket"},
	{ UPNPSECUREMODE, "secure_mode"}
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

		/* check for UPnP permissions rule */
		if(0 == memcmp(name, "allow", 5) || 0 == memcmp(name, "deny", 4))
		{
			upnppermlist = realloc(upnppermlist,
			                       sizeof(struct upnpperm) * (num_upnpperm+1));
			/* parse the rule */
			if(read_permission_line(upnppermlist + num_upnpperm, name) >= 0)
			{
				num_upnpperm++;
			}
			else
			{
				fprintf(stderr, "parsing error file %s line %d : %s\n",
				        fname, linenum, name);
			}
			continue;
		}
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

