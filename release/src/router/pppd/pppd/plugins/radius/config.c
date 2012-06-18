/*
 * $Id: config.c,v 1.1 2004/11/14 07:26:26 paulus Exp $
 *
 * Copyright (C) 1995,1996,1997 Lars Fenneberg
 *
 * Copyright 1992 Livingston Enterprises, Inc.
 *
 * Copyright 1992,1993, 1994,1995 The Regents of the University of Michigan
 * and Merit Network, Inc. All Rights Reserved
 *
 * See the file COPYRIGHT for the respective terms and conditions.
 * If the file is missing contact me at lf@elemental.net
 * and I'll send you a copy.
 *
 */

#include <includes.h>
#include <radiusclient.h>
#include <options.h>

static int test_config(char *);

/*
 * Function: find_option
 *
 * Purpose: find an option in the option list
 *
 * Returns: pointer to option on success, NULL otherwise
 */

static OPTION *find_option(char *optname, unsigned int type)
{
	int i;

	/* there're so few options that a binary search seems not necessary */
	for (i = 0; i < num_options; i++) {
		if (!strcmp(config_options[i].name, optname) &&
		    (config_options[i].type & type))
			return &config_options[i];
	}

	return NULL;
}

/*
 * Function: set_option_...
 *
 * Purpose: set a specific option doing type conversions
 *
 * Returns: 0 on success, -1 on failure
 */

static int set_option_str(char *filename, int line, OPTION *option, char *p)
{
	if (p)
		option->val = (void *) strdup(p);
	else
		option->val = NULL;

	return 0;
}

static int set_option_int(char *filename, int line, OPTION *option, char *p)
{
	int *iptr;

	if (p == NULL) {
		error("%s: line %d: bogus option value", filename, line);
		return (-1);
	}

	if ((iptr = (int *) malloc(sizeof(iptr))) == NULL) {
		novm("read_config");
		return (-1);
	}

	*iptr = atoi(p);
	option->val = (void *) iptr;

	return 0;
}

static int set_option_srv(char *filename, int line, OPTION *option, char *p)
{
	SERVER *serv;
	char *q;
	struct servent *svp;
	int i;

	if (p == NULL) {
		error("%s: line %d: bogus option value", filename, line);
		return (-1);
	}

	serv = (SERVER *) option->val;

	for (i = 0; i < serv->max; i++) {
		free(serv->name[i]);
	}
	serv->max = 0;

	while ((p = strtok(p, ", \t")) != NULL) {

		if ((q = strchr(p,':')) != NULL) {
			*q = '\0';
			q++;
			serv->port[serv->max] = atoi(q);
		} else {
			if (!strcmp(option->name,"authserver"))
				if ((svp = getservbyname ("radius", "udp")) == NULL)
					serv->port[serv->max] = PW_AUTH_UDP_PORT;
				else
					serv->port[serv->max] = ntohs ((unsigned int) svp->s_port);
			else if (!strcmp(option->name, "acctserver"))
				if ((svp = getservbyname ("radacct", "udp")) == NULL)
					serv->port[serv->max] = PW_ACCT_UDP_PORT;
				else
					serv->port[serv->max] = ntohs ((unsigned int) svp->s_port);
			else {
				error("%s: line %d: no default port for %s", filename, line, option->name);
				return (-1);
			}
		}

		serv->name[serv->max++] = strdup(p);

		p = NULL;
	}

	return 0;
}

static int set_option_auo(char *filename, int line, OPTION *option, char *p)
{
	int *iptr;

	if (p == NULL) {
		warn("%s: line %d: bogus option value", filename, line);
		return (-1);
	}

	if ((iptr = (int *) malloc(sizeof(iptr))) == NULL) {
			novm("read_config");
			return (-1);
	}

	*iptr = 0;
	p = strtok(p, ", \t");

	if (!strncmp(p, "local", 5))
			*iptr = AUTH_LOCAL_FST;
	else if (!strncmp(p, "radius", 6))
			*iptr = AUTH_RADIUS_FST;
	else {
		error("%s: auth_order: unknown keyword: %s", filename, p);
		return (-1);
	}

	p = strtok(NULL, ", \t");

	if (p && (*p != '\0')) {
		if ((*iptr & AUTH_RADIUS_FST) && !strcmp(p, "local"))
			*iptr = (*iptr) | AUTH_LOCAL_SND;
		else if ((*iptr & AUTH_LOCAL_FST) && !strcmp(p, "radius"))
			*iptr = (*iptr) | AUTH_RADIUS_SND;
		else {
			error("%s: auth_order: unknown or unexpected keyword: %s", filename, p);
			return (-1);
		}
	}

	option->val = (void *) iptr;

	return 0;
}


/*
 * Function: rc_read_config
 *
 * Purpose: read the global config file
 *
 * Returns: 0 on success, -1 when failure
 */

int rc_read_config(char *filename)
{
	FILE *configfd;
	char buffer[512], *p;
	OPTION *option;
	int line, pos;

	if ((configfd = fopen(filename,"r")) == NULL)
	{
		error("rc_read_config: can't open %s: %m", filename);
		return (-1);
	}

	line = 0;
	while ((fgets(buffer, sizeof(buffer), configfd) != NULL))
	{
		line++;
		p = buffer;

		if ((*p == '\n') || (*p == '#') || (*p == '\0'))
			continue;

		p[strlen(p)-1] = '\0';


		if ((pos = strcspn(p, "\t ")) == 0) {
			error("%s: line %d: bogus format: %s", filename, line, p);
			return (-1);
		}

		p[pos] = '\0';

		if ((option = find_option(p, OT_ANY)) == NULL) {
			error("%s: line %d: unrecognized keyword: %s", filename, line, p);
			return (-1);
		}

		if (option->status != ST_UNDEF) {
			error("%s: line %d: duplicate option line: %s", filename, line, p);
			return (-1);
		}

		p += pos+1;
		while (isspace(*p))
			p++;

		switch (option->type) {
			case OT_STR:
				 if (set_option_str(filename, line, option, p) < 0)
					return (-1);
				break;
			case OT_INT:
				 if (set_option_int(filename, line, option, p) < 0)
					return (-1);
				break;
			case OT_SRV:
				 if (set_option_srv(filename, line, option, p) < 0)
					return (-1);
				break;
			case OT_AUO:
				 if (set_option_auo(filename, line, option, p) < 0)
					return (-1);
				break;
			default:
				fatal("rc_read_config: impossible case branch!");
				abort();
		}
	}
	fclose(configfd);

	return test_config(filename);
}

/*
 * Function: rc_conf_str, rc_conf_int, rc_conf_src
 *
 * Purpose: get the value of a config option
 *
 * Returns: config option value
 */

char *rc_conf_str(char *optname)
{
	OPTION *option;

	option = find_option(optname, OT_STR);

	if (option == NULL)
		fatal("rc_conf_str: unkown config option requested: %s", optname);
		return (char *)option->val;
}

int rc_conf_int(char *optname)
{
	OPTION *option;

	option = find_option(optname, OT_INT|OT_AUO);

	if (option == NULL)
		fatal("rc_conf_int: unkown config option requested: %s", optname);
	return *((int *)option->val);
}

SERVER *rc_conf_srv(char *optname)
{
	OPTION *option;

	option = find_option(optname, OT_SRV);

	if (option == NULL)
		fatal("rc_conf_srv: unkown config option requested: %s", optname);
	return (SERVER *)option->val;
}

/*
 * Function: test_config
 *
 * Purpose: test the configuration the user supplied
 *
 * Returns: 0 on success, -1 when failure
 */

static int test_config(char *filename)
{
#if 0
	struct stat st;
	char	    *file;
#endif

	if (!(rc_conf_srv("authserver")->max))
	{
		error("%s: no authserver specified", filename);
		return (-1);
	}
	if (!(rc_conf_srv("acctserver")->max))
	{
		error("%s: no acctserver specified", filename);
		return (-1);
	}
	if (!rc_conf_str("servers"))
	{
		error("%s: no servers file specified", filename);
		return (-1);
	}
	if (!rc_conf_str("dictionary"))
	{
		error("%s: no dictionary specified", filename);
		return (-1);
	}

	if (rc_conf_int("radius_timeout") <= 0)
	{
		error("%s: radius_timeout <= 0 is illegal", filename);
		return (-1);
	}
	if (rc_conf_int("radius_retries") <= 0)
	{
		error("%s: radius_retries <= 0 is illegal", filename);
		return (-1);
	}

#if 0
	file = rc_conf_str("login_local");
	if (stat(file, &st) == 0)
	{
		if (!S_ISREG(st.st_mode)) {
			error("%s: not a regular file: %s", filename, file);
			return (-1);
		}
	} else {
		error("%s: file not found: %s", filename, file);
		return (-1);
	}
	file = rc_conf_str("login_radius");
	if (stat(file, &st) == 0)
	{
		if (!S_ISREG(st.st_mode)) {
			error("%s: not a regular file: %s", filename, file);
			return (-1);
		}
	} else {
		error("%s: file not found: %s", filename, file);
		return (-1);
	}
#endif

	if (rc_conf_int("login_tries") <= 0)
	{
		error("%s: login_tries <= 0 is illegal", filename);
		return (-1);
	}
	if (rc_conf_str("seqfile") == NULL)
	{
		error("%s: seqfile not specified", filename);
		return (-1);
	}
	if (rc_conf_int("login_timeout") <= 0)
	{
		error("%s: login_timeout <= 0 is illegal", filename);
		return (-1);
	}
	if (rc_conf_str("mapfile") == NULL)
	{
		error("%s: mapfile not specified", filename);
		return (-1);
	}
	if (rc_conf_str("nologin") == NULL)
	{
		error("%s: nologin not specified", filename);
		return (-1);
	}

	return 0;
}

/*
 * Function: rc_find_match
 *
 * Purpose: see if ip_addr is one of the ip addresses of hostname
 *
 * Returns: 0 on success, -1 when failure
 *
 */

static int find_match (UINT4 *ip_addr, char *hostname)
{
	UINT4           addr;
	char          **paddr;
	struct hostent *hp;

	if (rc_good_ipaddr (hostname) == 0)
	{
		if (*ip_addr == ntohl(inet_addr (hostname)))
		{
			return (0);
		}
	}
	else
	{
		if ((hp = gethostbyname (hostname)) == (struct hostent *) NULL)
		{
			return (-1);
		}
		for (paddr = hp->h_addr_list; *paddr; paddr++)
		{
			addr = ** (UINT4 **) paddr;
			if (ntohl(addr) == *ip_addr)
			{
				return (0);
			}
		}
	}
	return (-1);
}

/*
 * Function: rc_find_server
 *
 * Purpose: search a server in the servers file
 *
 * Returns: 0 on success, -1 on failure
 *
 */

int rc_find_server (char *server_name, UINT4 *ip_addr, char *secret)
{
	UINT4	myipaddr = 0;
	int             len;
	int             result;
	FILE           *clientfd;
	char           *h;
	char           *s;
	char           *host2;
	char            buffer[128];
	char            hostnm[AUTH_ID_LEN + 1];

	/* Get the IP address of the authentication server */
	if ((*ip_addr = rc_get_ipaddr (server_name)) == (UINT4) 0)
		return (-1);

	if ((clientfd = fopen (rc_conf_str("servers"), "r")) == (FILE *) NULL)
	{
		error("rc_find_server: couldn't open file: %m: %s", rc_conf_str("servers"));
		return (-1);
	}

	myipaddr = rc_own_ipaddress();

	result = 0;
	while (fgets (buffer, sizeof (buffer), clientfd) != (char *) NULL)
	{
		if (*buffer == '#')
			continue;

		if ((h = strtok (buffer, " \t\n")) == NULL) /* first hostname */
			continue;

		memset (hostnm, '\0', AUTH_ID_LEN);
		len = strlen (h);
		if (len > AUTH_ID_LEN)
		{
			len = AUTH_ID_LEN;
		}
		strncpy (hostnm, h, (size_t) len);
		hostnm[AUTH_ID_LEN] = '\0';

		if ((s = strtok (NULL, " \t\n")) == NULL) /* and secret field */
			continue;

		memset (secret, '\0', MAX_SECRET_LENGTH);
		len = strlen (s);
		if (len > MAX_SECRET_LENGTH)
		{
			len = MAX_SECRET_LENGTH;
		}
		strncpy (secret, s, (size_t) len);
		secret[MAX_SECRET_LENGTH] = '\0';

		if (!strchr (hostnm, '/')) /* If single name form */
		{
			if (find_match (ip_addr, hostnm) == 0)
			{
				result++;
				break;
			}
		}
		else /* <name1>/<name2> "paired" form */
		{
			strtok (hostnm, "/");
			if (find_match (&myipaddr, hostnm) == 0)
			{	     /* If we're the 1st name, target is 2nd */
				host2 = strtok (NULL, " ");
				if (find_match (ip_addr, host2) == 0)
				{
					result++;
					break;
				}
			}
			else	/* If we were 2nd name, target is 1st name */
			{
				if (find_match (ip_addr, hostnm) == 0)
				{
					result++;
					break;
				}
			}
		}
	}
	fclose (clientfd);
	if (result == 0)
	{
		memset (buffer, '\0', sizeof (buffer));
		memset (secret, '\0', sizeof (secret));
		error("rc_find_server: couldn't find RADIUS server %s in %s",
		      server_name, rc_conf_str("servers"));
		return (-1);
	}
	return 0;
}
