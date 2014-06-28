/* $Id: options.c,v 1.28 2013/12/13 14:07:08 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * author: Ryan Wagoner
 * (c) 2006-2014 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include "config.h"
#include "options.h"
#include "upnppermissions.h"
#ifdef PCP_SADSCP
#include "pcplearndscp.h"
#endif /* PCP_SADSPC */
#include "upnpglobalvars.h"

#ifndef DISABLE_CONFIG_FILE
struct option * ary_options = NULL;
static char * string_repo = NULL;
unsigned int num_options = 0;

static const struct {
	enum upnpconfigoptions id;
	const char * name;
} optionids[] = {
	{ UPNPEXT_IFNAME, "ext_ifname" },
	{ UPNPEXT_IP,	"ext_ip" },
	{ UPNPLISTENING_IP, "listening_ip" },
#ifdef ENABLE_IPV6
	{ UPNPIPV6_LISTENING_IP, "ipv6_listening_ip" },
#endif /* ENABLE_IPV6 */
	{ UPNPPORT, "port" },
	{ UPNPPORT, "http_port" },	/* "port" and "http_port" are synonims */
#ifdef ENABLE_HTTPS
	{ UPNPHTTPSPORT, "https_port" },
#endif /* ENABLE_HTTPS */
	{ UPNPBITRATE_UP, "bitrate_up" },
	{ UPNPBITRATE_DOWN, "bitrate_down" },
	{ UPNPPRESENTATIONURL, "presentation_url" },
#ifdef ENABLE_MANUFACTURER_INFO_CONFIGURATION
	{ UPNPFRIENDLY_NAME, "friendly_name" },
	{ UPNPMANUFACTURER_NAME, "manufacturer_name" },
	{ UPNPMANUFACTURER_URL, "manufacturer_url" },
	{ UPNPMODEL_NAME, "model_name" },
	{ UPNPMODEL_DESCRIPTION, "model_description" },
	{ UPNPMODEL_URL, "model_url" },
#endif
	{ UPNPNOTIFY_INTERVAL, "notify_interval" },
	{ UPNPSYSTEM_UPTIME, "system_uptime" },
	{ UPNPPACKET_LOG, "packet_log" },
	{ UPNPUUID, "uuid"},
	{ UPNPSERIAL, "serial"},
	{ UPNPMODEL_NUMBER, "model_number"},
	{ UPNPCLEANTHRESHOLD, "clean_ruleset_threshold"},
	{ UPNPCLEANINTERVAL, "clean_ruleset_interval"},
#ifdef USE_NETFILTER
	{ UPNPFORWARDCHAIN, "upnp_forward_chain"},
	{ UPNPNATCHAIN, "upnp_nat_chain"},
#endif
#ifdef ENABLE_NATPMP
	{ UPNPENABLENATPMP, "enable_natpmp"},
#endif
#ifdef ENABLE_PCP
	{ UPNPPCPMINLIFETIME, "min_lifetime"},
	{ UPNPPCPMAXLIFETIME, "max_lifetime"},
	{ UPNPPCPALLOWTHIRDPARTY, "pcp_allow_thirdparty"},
#endif
	{ UPNPENABLE, "enable_upnp"},
#ifdef USE_PF
	{ UPNPANCHOR, "anchor"},
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
	unsigned int i;
	enum upnpconfigoptions id;
	size_t string_repo_len = 0;
	size_t len;
	void *tmp;

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
			/* remove spaces at the end of the line */
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

		len = strlen(name); /* length of the whole line excluding leading
		                     * and ending white spaces */
		/* check for UPnP permissions rule */
		if((len > 6) && (0 == memcmp(name, "allow", 5) || 0 == memcmp(name, "deny", 4)))
		{
			tmp = realloc(upnppermlist, sizeof(struct upnpperm) * (num_upnpperm+1));
			if(tmp == NULL)
			{
				fprintf(stderr, "memory allocation error. Permission line in file %s line %d\n",
				        fname, linenum);
			}
			else
			{
				upnppermlist = tmp;
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
			}
			continue;
		}
#ifdef PCP_SADSCP
		/* check for DSCP values configuration */
		if((len > 15) && 0 == memcmp(name, "set_learn_dscp", sizeof("set_learn_dscp")-1) )
		{
			tmp = realloc(dscp_values_list, sizeof(struct dscp_values) * (num_dscp_values+1));
			if(tmp == NULL)
			{
				fprintf(stderr, "memory allocation error. DSCP line in file %s line %d\n",
				        fname, linenum);
			}
			else
			{
				dscp_values_list = tmp;
				/* parse the rule */
				if(read_learn_dscp_line(dscp_values_list + num_dscp_values, name) >= 0)
				{
					num_dscp_values++;
				}
				else
				{
					fprintf(stderr, "parsing error file %s line %d : %s\n",
					        fname, linenum, name);
				}
			}
			continue;
		}
#endif /* PCP_SADSCP */
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
			fprintf(stderr, "invalid option in file %s line %d : %s=%s\n",
			        fname, linenum, name, value);
		}
		else
		{
			tmp = realloc(ary_options, (num_options + 1) * sizeof(struct option));
			if(tmp == NULL)
			{
				fprintf(stderr, "memory allocation error. Option in file %s line %d.\n",
				        fname, linenum);
			}
			else
			{
				ary_options = tmp;
				len = strlen(value) + 1;	/* +1 for terminating '\0' */
				tmp = realloc(string_repo, string_repo_len + len);
				if(tmp == NULL)
				{
					fprintf(stderr, "memory allocation error, Option value in file %s line %d : %s=%s\n",
					        fname, linenum, name, value);
				}
				else
				{
					string_repo = tmp;
					memcpy(string_repo + string_repo_len, value, len);
					ary_options[num_options].id = id;
					/* save the offset instead of the absolute address because realloc() could
					 * change it */
					ary_options[num_options].value = (const char *)string_repo_len;
					num_options += 1;
					string_repo_len += len;
				}
			}
		}

	}

	fclose(hfile);

	for(i = 0; i < num_options; i++)
	{
		/* add start address of string_repo to get right pointer */
		ary_options[i].value = string_repo + (size_t)ary_options[i].value;
	}

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
	if(string_repo)
	{
		free(string_repo);
		string_repo = NULL;
	}
	if(upnppermlist)
	{
		free(upnppermlist);
		upnppermlist = NULL;
		num_upnpperm = 0;
	}
#ifdef PCP_SADSCP
	if(dscp_values_list)
	{
	    unsigned int i;
		for (i = 0; i < num_dscp_values; i++) {
			if (dscp_values_list[i].app_name) {
				free(dscp_values_list[i].app_name);
				dscp_values_list[i].app_name = NULL;
			}
		}
		free(dscp_values_list);
		dscp_values_list = NULL;
		num_dscp_values = 0;
	}
#endif /* PCP_SADSCP */
}

#endif /* DISABLE_CONFIG_FILE */
