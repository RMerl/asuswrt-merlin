/*
   Samba Unix/Linux SMB client library
   net time command
   Copyright (C) 2001 Andrew Tridgell (tridge@samba.org)

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
#include "includes.h"
#include "utils/net.h"
#include "libsmb/nmblib.h"
#include "libsmb/libsmb.h"

/*
  return the time on a server. This does not require any authentication
*/
static time_t cli_servertime(const char *host, struct sockaddr_storage *pss, int *zone)
{
	struct nmb_name calling, called;
	time_t ret = 0;
	struct cli_state *cli = NULL;
	NTSTATUS status;

	cli = cli_initialise();
	if (!cli) {
		goto done;
	}

	status = cli_connect(cli, host, pss);
	if (!NT_STATUS_IS_OK(status)) {
		fprintf(stderr, _("Can't contact server %s. Error %s\n"),
			host, nt_errstr(status));
		goto done;
	}

	make_nmb_name(&calling, global_myname(), 0x0);
	if (host) {
		make_nmb_name(&called, host, 0x20);
	} else {
		make_nmb_name(&called, "*SMBSERVER", 0x20);
	}

	if (!cli_session_request(cli, &calling, &called)) {
		fprintf(stderr, _("Session request failed\n"));
		goto done;
	}
	status = cli_negprot(cli);
	if (!NT_STATUS_IS_OK(status)) {
		fprintf(stderr, _("Protocol negotiation failed: %s\n"),
			nt_errstr(status));
		goto done;
	}

	ret = cli->servertime;
	if (zone) *zone = cli->serverzone;

done:
	if (cli) {
		cli_shutdown(cli);
	}
	return ret;
}

/* find the servers time on the opt_host host */
static time_t nettime(struct net_context *c, int *zone)
{
	return cli_servertime(c->opt_host,
			      c->opt_have_ip? &c->opt_dest_ip : NULL, zone);
}

/* return a time as a string ready to be passed to /bin/date */
static const char *systime(time_t t)
{
	struct tm *tm;

	tm = localtime(&t);
	if (!tm) {
		return "unknown";
	}

	return talloc_asprintf(talloc_tos(), "%02d%02d%02d%02d%04d.%02d",
			       tm->tm_mon+1, tm->tm_mday, tm->tm_hour,
			       tm->tm_min, tm->tm_year + 1900, tm->tm_sec);
}

int net_time_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
"net time\n\tdisplays time on a server\n\n"
"net time system\n\tdisplays time on a server in a format ready for /bin/date\n\n"
"net time set\n\truns /bin/date with the time from the server\n\n"
"net time zone\n\tdisplays the timezone in hours from GMT on the remote computer\n\n"
"\n"));
	net_common_flags_usage(c, argc, argv);
	return -1;
}

/* try to set the system clock */
static int net_time_set(struct net_context *c, int argc, const char **argv)
{
	struct timeval tv;
	int result;

	tv.tv_sec = nettime(c, NULL);
	tv.tv_usec=0;

	if (tv.tv_sec == 0) return -1;

	result = settimeofday(&tv,NULL);

	if (result)
		d_fprintf(stderr, _("setting system clock failed.  Error was (%s)\n"),
			strerror(errno));

	return result;
}

/* display the time on a remote box in a format ready for /bin/date */
static int net_time_system(struct net_context *c, int argc, const char **argv)
{
	time_t t;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net time system\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Output remote time server time in a format "
			   "ready for /bin/date"));
		return 0;
	}

	t = nettime(c, NULL);
	if (t == 0) return -1;

	printf("%s\n", systime(t));

	return 0;
}

/* display the remote time server's offset to UTC */
static int net_time_zone(struct net_context *c, int argc, const char **argv)
{
	int zone = 0;
	int hours, mins;
	char zsign;
	time_t t;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net time zone\n"
			   "   %s\n",
			 _("Usage:"),
			 _("Display the remote time server's offset to "
			   "UTC"));
		return 0;
	}

	t = nettime(c, &zone);

	if (t == 0) return -1;

	zsign = (zone > 0) ? '-' : '+';
	if (zone < 0) zone = -zone;

	zone /= 60;
	hours = zone / 60;
	mins = zone % 60;

	printf("%c%02d%02d\n", zsign, hours, mins);

	return 0;
}

/* display or set the time on a host */
int net_time(struct net_context *c, int argc, const char **argv)
{
	time_t t;
	struct functable func[] = {
		{
			"system",
			net_time_system,
			NET_TRANSPORT_LOCAL,
			N_("Display time ready for /bin/date"),
			N_("net time system\n"
			   "    Display time ready for /bin/date")
		},
		{
			"set",
			net_time_set,
			NET_TRANSPORT_LOCAL,
			N_("Set the system time from time server"),
			N_("net time set\n"
			   "    Set the system time from time server")
		},
		{
			"zone",
			net_time_zone,
			NET_TRANSPORT_LOCAL,
			N_("Display timezone offset from UTC"),
			N_("net time zone\n"
			   "    Display timezone offset from UTC")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (argc != 0) {
		return net_run_function(c, argc, argv, "net time", func);
	}

	if (c->display_usage) {
		d_printf(  "%s\n"
		           "net time\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Display the remote time server's time"));
		net_display_usage_from_functable(func);
		return 0;
	}

	if (!c->opt_host && !c->opt_have_ip &&
	    !find_master_ip(c->opt_target_workgroup, &c->opt_dest_ip)) {
		d_fprintf(stderr, _("Could not locate a time server.  Try "
				    "specifying a target host.\n"));
		net_time_usage(c, argc,argv);
		return -1;
	}

	/* default - print the time */
	t = cli_servertime(c->opt_host, c->opt_have_ip? &c->opt_dest_ip : NULL,
			   NULL);
	if (t == 0) return -1;

	d_printf("%s", ctime(&t));
	return 0;
}
