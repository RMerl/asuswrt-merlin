/*
*
* radrealms.c
*
* A pppd plugin which is stacked on top of radius.so.  This plugin
* allows selection of alternate set of servers based on the user's realm.
*
* Author: Ben McKeegan  ben@netservers.co.uk
*
* Copyright (C) 2002 Netservers
*
* This plugin may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
*/

static char const RCSID[] =
    "$Id: radrealms.c,v 1.2 2004/11/14 07:26:26 paulus Exp $";

#include "pppd.h"
#include "radiusclient.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char pppd_version[] = VERSION;

char radrealms_config[MAXPATHLEN] = "/etc/radiusclient/realms";

static option_t Options[] = {
    { "realms-config-file", o_string, &radrealms_config },
    { NULL }
};

extern void (*radius_pre_auth_hook)(char const *user,
				    SERVER **authserver,
				    SERVER **acctserver);

static void
lookup_realm(char const *user,
	     SERVER **authserver,
	     SERVER **acctserver)
{
    char *realm;
    FILE *fd;
    SERVER *accts, *auths, *s;
    char buffer[512], *p;
    int line = 0;
    
    auths = (SERVER *) malloc(sizeof(SERVER));
    auths->max = 0;
    accts = (SERVER *) malloc(sizeof(SERVER));
    accts->max = 0;
    
    realm = strrchr(user, '@');
    
    if (realm) {
	info("Looking up servers for realm '%s'", realm);
    } else {
	info("Looking up servers for DEFAULT realm");
    }
    if (realm) {
	if (*(++realm) == '\0') {
	    realm = NULL;
	}
    }
    
    if ((fd = fopen(radrealms_config, "r")) == NULL) {
	option_error("cannot open %s", radrealms_config);
	return;
    } 
    info("Reading %s", radrealms_config);
    
    while ((fgets(buffer, sizeof(buffer), fd) != NULL)) {
	line++;

	if ((*buffer == '\n') || (*buffer == '#') || (*buffer == '\0'))
	    continue;

	buffer[strlen(buffer)-1] = '\0';

	p = strtok(buffer, "\t ");

	if (p == NULL || (strcmp(p, "authserver") !=0
	    && strcmp(p, "acctserver"))) {
	    fclose(fd);
	    option_error("%s: invalid line %d: %s", radrealms_config,
			 line, buffer);
	    return;
	}
	info("Parsing '%s' entry:", p);
	s = auths;
	if (p[1] == 'c') {
	    s = accts;
	}
	if (s->max >= SERVER_MAX)
	    continue;

	if ((p = strtok(NULL, "\t ")) == NULL) {
	    fclose(fd);
	    option_error("%s: realm name missing on line %d: %s",
			 radrealms_config, line, buffer);
	    return;
	}

	if ((realm != NULL && strcmp(p, realm) == 0) ||
	    (realm == NULL && strcmp(p, "DEFAULT") == 0) ) {
	    info(" - Matched realm %s", p);
	    if ((p = strtok(NULL, ":")) == NULL) {
		fclose(fd);
		option_error("%s: server address missing on line %d: %s",
			     radrealms_config, line, buffer);
		return;
	    }
	    s->name[s->max] = strdup(p);
	    info(" - Address is '%s'",p);
	    if ((p = strtok(NULL, "\t ")) == NULL) {
		fclose(fd);
		option_error("%s: server port missing on line %d:  %s",
			     radrealms_config, line, buffer);
		return;
	    }
	    s->port[s->max] = atoi(p);
	    info(" - Port is '%d'", s->port[s->max]);
	    s->max++;
	} else 
	    info(" - Skipping realm '%s'", p);
    }
    fclose(fd);

    if (accts->max)
	*acctserver = accts;

    if (auths->max)
	*authserver = auths;

    return;
}

void
plugin_init(void)
{
    radius_pre_auth_hook = lookup_realm;

    add_options(Options);
    info("RADIUS Realms plugin initialized.");
}
