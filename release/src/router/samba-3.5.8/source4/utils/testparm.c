/* 
   Unix SMB/CIFS implementation.
   Test validity of smb.conf
   Copyright (C) Karl Auer 1993, 1994-1998

   Extensively modified by Andrew Tridgell, 1995
   Converted to popt by Jelmer Vernooij (jelmer@nl.linux.org), 2002
   Updated for Samba4 by Andrew Bartlett <abartlet@samba.org> 2006
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Testbed for loadparm.c/params.c
 *
 * This module simply loads a specified configuration file and
 * if successful, dumps it's contents to stdout. Note that the
 * operation is performed with DEBUGLEVEL at 3.
 *
 * Useful for a quick 'syntax check' of a configuration file.
 *
 */

#include "includes.h"
#include "system/filesys.h"
#include "lib/cmdline/popt_common.h"
#include "lib/socket/socket.h"
#include "param/param.h"
#include "param/loadparm.h"


/***********************************************
 Here we do a set of 'hard coded' checks for bad
 configuration settings.
************************************************/

static int do_global_checks(struct loadparm_context *lp_ctx)
{
	int ret = 0;

	if (!directory_exist(lp_lockdir(lp_ctx))) {
		fprintf(stderr, "ERROR: lock directory %s does not exist\n",
		       lp_lockdir(lp_ctx));
		ret = 1;
	}

	if (!directory_exist(lp_piddir(lp_ctx))) {
		fprintf(stderr, "ERROR: pid directory %s does not exist\n",
		       lp_piddir(lp_ctx));
		ret = 1;
	}

	if (strlen(lp_winbind_separator(lp_ctx)) != 1) {
		fprintf(stderr,"ERROR: the 'winbind separator' parameter must be a single character.\n");
		ret = 1;
	}

	if (*lp_winbind_separator(lp_ctx) == '+') {
		fprintf(stderr,"'winbind separator = +' might cause problems with group membership.\n");
	}

	return ret;
}   


static int do_share_checks(struct loadparm_context *lp_ctx, const char *cname, const char *caddr, bool silent_mode,
			   bool show_defaults, const char *section_name, const char *parameter_name)
{
	int ret = 0;
	int s;

	for (s=0;s<lp_numservices(lp_ctx);s++) {
		struct loadparm_service *service = lp_servicebynum(lp_ctx, s);
		if (service != NULL)
			if (strlen(lp_servicename(lp_servicebynum(lp_ctx, s))) > 12) {
				fprintf(stderr, "WARNING: You have some share names that are longer than 12 characters.\n" );
				fprintf(stderr, "These may not be accessible to some older clients.\n" );
				fprintf(stderr, "(Eg. Windows9x, WindowsMe, and not listed in smbclient in Samba 3.0.)\n" );
				break;
			}
	}

	for (s=0;s<lp_numservices(lp_ctx);s++) {
		struct loadparm_service *service = lp_servicebynum(lp_ctx, s);
		if (service != NULL) {
			const char **deny_list = lp_hostsdeny(service, lp_default_service(lp_ctx));
			const char **allow_list = lp_hostsallow(service, lp_default_service(lp_ctx));
			int i;
			if(deny_list) {
				for (i=0; deny_list[i]; i++) {
					char *hasstar = strchr_m(deny_list[i], '*');
					char *hasquery = strchr_m(deny_list[i], '?');
					if(hasstar || hasquery) {
						fprintf(stderr,"Invalid character %c in hosts deny list (%s) for service %s.\n",
							   hasstar ? *hasstar : *hasquery, deny_list[i], lp_servicename(service) );
					}
				}
			}

			if(allow_list) {
				for (i=0; allow_list[i]; i++) {
					char *hasstar = strchr_m(allow_list[i], '*');
					char *hasquery = strchr_m(allow_list[i], '?');
					if(hasstar || hasquery) {
						fprintf(stderr,"Invalid character %c in hosts allow list (%s) for service %s.\n",
							   hasstar ? *hasstar : *hasquery, allow_list[i], lp_servicename(service) );
					}
				}
			}
		}
	}


	if (!cname) {
		if (!silent_mode) {
			fprintf(stderr,"Press enter to see a dump of your service definitions\n");
			fflush(stdout);
			getc(stdin);
		}
		if (section_name != NULL || parameter_name != NULL) {
			struct loadparm_service *service = NULL;
			if (!section_name) {
				section_name = GLOBAL_NAME;
				service = NULL;
			} else if ((!strwicmp(section_name, GLOBAL_NAME)) == 0 &&
				 (service=lp_service(lp_ctx, section_name)) == NULL) {
					fprintf(stderr,"Unknown section %s\n",
						section_name);
					return(1);
			}
			if (!parameter_name) {
				lp_dump_one(stdout, show_defaults, service, lp_default_service(lp_ctx));
			} else {
				ret = !lp_dump_a_parameter(lp_ctx, service, parameter_name, stdout);
			}
		} else {
			lp_dump(lp_ctx, stdout, show_defaults, lp_numservices(lp_ctx));
		}
		return(ret);
	}

	if(cname && caddr){
		/* this is totally ugly, a real `quick' hack */
		for (s=0;s<lp_numservices(lp_ctx);s++) {
			struct loadparm_service *service = lp_servicebynum(lp_ctx, s);
			if (service != NULL) {
				if (allow_access(NULL, lp_hostsdeny(NULL, lp_default_service(lp_ctx)), lp_hostsallow(NULL, lp_default_service(lp_ctx)), cname, caddr)
				    && allow_access(NULL, lp_hostsdeny(service, lp_default_service(lp_ctx)), lp_hostsallow(service, lp_default_service(lp_ctx)), cname, caddr)) {
					fprintf(stderr,"Allow connection from %s (%s) to %s\n",
						   cname,caddr,lp_servicename(service));
				} else {
					fprintf(stderr,"Deny connection from %s (%s) to %s\n",
						   cname,caddr,lp_servicename(service));
				}
			}
		}
	}

	return ret;
}


 int main(int argc, const char *argv[])
{
	int ret = 0;
	poptContext pc;
/*
	static int show_all_parameters = 0;
	static char *new_local_machine = NULL;
*/
	static const char *section_name = NULL;
	static char *parameter_name = NULL;
	static const char *cname;
	static const char *caddr;
	static int silent_mode = false;
	static int show_defaults = false;  /* This must be an 'int',
					    * as we take it as we pass
					    * it's address as an int
					    * pointer  */
	struct loadparm_context *lp_ctx;

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"suppress-prompt", 0, POPT_ARG_NONE, &silent_mode, true, "Suppress prompt for enter"},
		{"verbose", 'v', POPT_ARG_NONE, &show_defaults, true, "Show default options too"},
/*
  We need support for smb.conf macros before this will work again 
		{"server", 'L',POPT_ARG_STRING, &new_local_machine, 0, "Set %%L macro to servername\n"},
*/
/*
  These are harder to do with the new code structure
		{"show-all-parameters", '\0', POPT_ARG_NONE, &show_all_parameters, 1, "Show the parameters, type, possible values" },
*/
		{"section-name", '\0', POPT_ARG_STRING, &section_name, 0, "Limit testparm to a named section" },
		{"parameter-name", '\0', POPT_ARG_STRING, &parameter_name, 0, "Limit testparm to a named parameter" },
		{"client-name", '\0', POPT_ARG_STRING, &cname, 0, "Client DNS name for 'hosts allow' checking (should match reverse lookup)"},
		{"client-ip", '\0', POPT_ARG_STRING, &caddr, 0, "Client IP address for 'hosts allow' checking"},
		POPT_COMMON_SAMBA
		POPT_COMMON_VERSION
		{ NULL }
	};

	setup_logging(NULL, DEBUG_STDERR);

	pc = poptGetContext(NULL, argc, argv, long_options, 
			    POPT_CONTEXT_KEEP_FIRST);
	poptSetOtherOptionHelp(pc, "[OPTION...] [host-name] [host-ip]");

	while(poptGetNextOpt(pc) != -1);

/* 
	if (show_all_parameters) {
		show_parameter_list();
		exit(0);
	}
*/

	if ( cname && ! caddr ) {
		printf ( "ERROR: For 'hosts allow' check you must specify both a DNS name and an IP address.\n" );
		return(1);
	}
/*
  We need support for smb.conf macros before this will work again 

	if (new_local_machine) {
		set_local_machine_name(new_local_machine, True);
	}
*/

	lp_ctx = cmdline_lp_ctx;
	
	/* We need this to force the output */
	lp_set_cmdline(lp_ctx, "log level", "2");

	fprintf(stderr, "Loaded smb config files from %s\n", lp_configfile(lp_ctx));

	if (!lp_load(lp_ctx, lp_configfile(lp_ctx))) {
		fprintf(stderr,"Error loading services.\n");
		return(1);
	}

	fprintf(stderr,"Loaded services file OK.\n");

	ret = do_global_checks(lp_ctx);
	ret |= do_share_checks(lp_ctx, cname, caddr, silent_mode, show_defaults, section_name, parameter_name);

	return(ret);
}

