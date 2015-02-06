/**
 * Copyright (C) 2012-2014 Steven Barth <steven@midlink.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#include <net/if.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <arpa/inet.h>

#include "odhcp6c.h"
#include "ra.h"



static void sighandler(int signal);
static int usage(void);

static uint8_t *state_data[_STATE_MAX] = {NULL};
static size_t state_len[_STATE_MAX] = {0};

static volatile bool signal_io = false;
static volatile bool signal_usr1 = false;
static volatile bool signal_usr2 = false;
static volatile bool signal_term = false;

static int urandom_fd = -1, allow_slaac_only = 0;
static bool bound = false, release = true;
static time_t last_update = 0;

static unsigned int min_update_interval = DEFAULT_MIN_UPDATE_INTERVAL;

int main(_unused int argc, char* const argv[])
{
	// Allocate ressources
	const char *pidfile = NULL;
	const char *script = "/usr/sbin/odhcp6c-update";
	ssize_t l;
	uint8_t buf[134];
	char *optpos;
	uint16_t opttype;
	uint16_t optlen;
	enum odhcp6c_ia_mode ia_na_mode = IA_MODE_TRY;
	enum odhcp6c_ia_mode ia_pd_mode = IA_MODE_NONE;
	int ia_pd_iaid_index = 0;
	static struct in6_addr ifid = IN6ADDR_ANY_INIT;
	int sol_timeout = DHCPV6_SOL_MAX_RT;
	int verbosity = 0;


	bool help = false, daemonize = false;
	int logopt = LOG_PID;
	int c;
	unsigned int client_options = DHCPV6_CLIENT_FQDN | DHCPV6_ACCEPT_RECONFIGURE;

	while ((c = getopt(argc, argv, "S::N:V:P:FB:c:i:r:Ru:s:kt:m:hedp:fav")) != -1) {
		switch (c) {
		case 'S':
			allow_slaac_only = (optarg) ? atoi(optarg) : -1;
			break;

		case 'N':
			if (!strcmp(optarg, "force")) {
				ia_na_mode = IA_MODE_FORCE;
				allow_slaac_only = -1;
			} else if (!strcmp(optarg, "none")) {
				ia_na_mode = IA_MODE_NONE;
			} else if (!strcmp(optarg, "try")) {
				ia_na_mode = IA_MODE_TRY;
			} else{
				help = true;
			}
			break;

		case 'V':
			l = script_unhexlify(buf, sizeof(buf), optarg);
			if (!l)
				help=true;

			odhcp6c_add_state(STATE_VENDORCLASS, buf, l);

			break;
		case 'P':
			if (ia_pd_mode == IA_MODE_NONE)
				ia_pd_mode = IA_MODE_TRY;

			if (allow_slaac_only >= 0 && allow_slaac_only < 10)
				allow_slaac_only = 10;

			char *iaid_begin;
			int iaid_len = 0;

			int prefix_length = strtoul(optarg, &iaid_begin, 10);

			if (*iaid_begin != '\0' && *iaid_begin != ',' && *iaid_begin != ':') {
				syslog(LOG_ERR, "invalid argument: '%s'", optarg);
				return 1;
			}

			struct odhcp6c_request_prefix prefix = { 0, prefix_length };

			if (*iaid_begin == ',' && (iaid_len = strlen(iaid_begin)) > 1)
				memcpy(&prefix.iaid, iaid_begin + 1, iaid_len > 4 ? 4 : iaid_len);
			else if (*iaid_begin == ':')
				prefix.iaid = htonl((uint32_t)strtoul(&iaid_begin[1], NULL, 16));
			else
				prefix.iaid = htonl(++ia_pd_iaid_index);

			odhcp6c_add_state(STATE_IA_PD_INIT, &prefix, sizeof(prefix));

			break;

		case 'F':
			allow_slaac_only = -1;
			ia_pd_mode = IA_MODE_FORCE;
			break;

		case 'c':
			l = script_unhexlify(&buf[4], sizeof(buf) - 4, optarg);
			if (l > 0) {
				buf[0] = 0;
				buf[1] = DHCPV6_OPT_CLIENTID;
				buf[2] = 0;
				buf[3] = l;
				odhcp6c_add_state(STATE_CLIENT_ID, buf, l + 4);
			} else {
				help = true;
			}
			break;

		case 'i':
			if (inet_pton(AF_INET6, optarg, &ifid) != 1)
				help = true;
			break;

		case 'r':
			optpos = optarg;
			while (optpos[0]) {
				opttype = htons(strtoul(optarg, &optpos, 10));
				if (optpos == optarg)
					break;
				else if (optpos[0])
					optarg = &optpos[1];
				odhcp6c_add_state(STATE_ORO, &opttype, 2);
			}
			break;

		case 'R':
			client_options |= DHCPV6_STRICT_OPTIONS;
			break;

		case 'u':
			optlen = htons(strlen(optarg));
			odhcp6c_add_state(STATE_USERCLASS, &optlen, 2);
			odhcp6c_add_state(STATE_USERCLASS, optarg, strlen(optarg));
			break;

		case 's':
			script = optarg;
			break;

		case 'k':
			release = false;
			break;

		case 't':
			sol_timeout = atoi(optarg);
			break;

		case 'm':
			min_update_interval = atoi(optarg);
			break;

		case 'e':
			logopt |= LOG_PERROR;
			break;

		case 'd':
			daemonize = true;
			break;

		case 'p':
			pidfile = optarg;
			break;

		case 'f':
			client_options &= ~DHCPV6_CLIENT_FQDN;
			break;

		case 'a':
			client_options &= ~DHCPV6_ACCEPT_RECONFIGURE;
			break;

		case 'v':
			++verbosity;
			break;

		default:
			help = true;
			break;
		}
	}

	openlog("odhcp6c", logopt, LOG_DAEMON);
	if (!verbosity)
		setlogmask(LOG_UPTO(LOG_WARNING));

	const char *ifname = argv[optind];

	if (help || !ifname)
		return usage();

	signal(SIGIO, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGCHLD, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGUSR1, sighandler);
	signal(SIGUSR2, sighandler);

	if ((urandom_fd = open("/dev/urandom", O_CLOEXEC | O_RDONLY)) < 0 ||
			init_dhcpv6(ifname, client_options, sol_timeout) ||
			ra_init(ifname, &ifid) || script_init(script, ifname)) {
		syslog(LOG_ERR, "failed to initialize: %s", strerror(errno));
		return 3;
	}

	if (daemonize) {
		openlog("odhcp6c", LOG_PID, LOG_DAEMON); // Disable LOG_PERROR
		if (daemon(0, 0)) {
			syslog(LOG_ERR, "Failed to daemonize: %s",
					strerror(errno));
			return 4;
		}

		if (!pidfile) {
			snprintf((char*)buf, sizeof(buf), "/var/run/odhcp6c.%s.pid", ifname);
			pidfile = (char*)buf;
		}

		FILE *fp = fopen(pidfile, "w");
		if (fp) {
			fprintf(fp, "%i\n", getpid());
			fclose(fp);
		}
	}

	script_call("started");

	while (!signal_term) { // Main logic
		odhcp6c_clear_state(STATE_SERVER_ID);
		odhcp6c_clear_state(STATE_SERVER_ADDR);
		odhcp6c_clear_state(STATE_IA_NA);
		odhcp6c_clear_state(STATE_IA_PD);
		odhcp6c_clear_state(STATE_SNTP_IP);
		odhcp6c_clear_state(STATE_NTP_IP);
		odhcp6c_clear_state(STATE_NTP_FQDN);
		odhcp6c_clear_state(STATE_SIP_IP);
		odhcp6c_clear_state(STATE_SIP_FQDN);
		dhcpv6_set_ia_mode(ia_na_mode, ia_pd_mode);
		bound = false;

		syslog(LOG_NOTICE, "(re)starting transaction on %s", ifname);

		signal_usr1 = signal_usr2 = false;
		int mode = dhcpv6_request(DHCPV6_MSG_SOLICIT);
		odhcp6c_signal_process();

		if (mode < 0)
			continue;

		do {
			int res = dhcpv6_request(mode == DHCPV6_STATELESS ?
					DHCPV6_MSG_INFO_REQ : DHCPV6_MSG_REQUEST);
			bool signalled = odhcp6c_signal_process();

			if (res > 0)
				break;
			else if (signalled) {
				mode = -1;
				break;
			}

			mode = dhcpv6_promote_server_cand();
		} while (mode > DHCPV6_UNKNOWN);

		if (mode < 0)
			continue;

		switch (mode) {
		case DHCPV6_STATELESS:
			bound = true;
			syslog(LOG_NOTICE, "entering stateless-mode on %s", ifname);

			while (!signal_usr2 && !signal_term) {
				signal_usr1 = false;
				script_call("informed");

				int res = dhcpv6_poll_reconfigure();
				odhcp6c_signal_process();

				if (res > 0)
					continue;

				if (signal_usr1) {
					signal_usr1 = false; // Acknowledged
					continue;
				}
				if (signal_usr2 || signal_term)
					break;

				res = dhcpv6_request(DHCPV6_MSG_INFO_REQ);
				odhcp6c_signal_process();
				if (signal_usr1)
					continue;
				else if (res < 0)
					break;
			}
			break;

		case DHCPV6_STATEFUL:
			bound = true;
			script_call("bound");
			syslog(LOG_NOTICE, "entering stateful-mode on %s", ifname);

			while (!signal_usr2 && !signal_term) {
				// Renew Cycle
				// Wait for T1 to expire or until we get a reconfigure
				int res = dhcpv6_poll_reconfigure();
				odhcp6c_signal_process();
				if (res > 0) {
					script_call("updated");
					continue;
				}

				// Handle signal, if necessary
				if (signal_usr1)
					signal_usr1 = false; // Acknowledged
				if (signal_usr2 || signal_term)
					break; // Other signal type

				// Send renew as T1 expired
				res = dhcpv6_request(DHCPV6_MSG_RENEW);
				odhcp6c_signal_process();
				if (res > 0) { // Renew was succesfull
					// Publish updates
					script_call("updated");
					continue; // Renew was successful
				}

				odhcp6c_clear_state(STATE_SERVER_ID); // Remove binding
				odhcp6c_clear_state(STATE_SERVER_ADDR);

				size_t ia_pd_len, ia_na_len;
				odhcp6c_get_state(STATE_IA_PD, &ia_pd_len);
				odhcp6c_get_state(STATE_IA_NA, &ia_na_len);

				if (ia_pd_len == 0 && ia_na_len == 0)
					break;

				// If we have IAs, try rebind otherwise restart
				res = dhcpv6_request(DHCPV6_MSG_REBIND);
				odhcp6c_signal_process();

				if (res > 0)
					script_call("rebound");
				else {
					break;
				}
			}
			break;

		default:
			break;
		}

		odhcp6c_expire();

		size_t ia_pd_len, ia_na_len, server_id_len;
		odhcp6c_get_state(STATE_IA_PD, &ia_pd_len);
		odhcp6c_get_state(STATE_IA_NA, &ia_na_len);
		odhcp6c_get_state(STATE_SERVER_ID, &server_id_len);

		// Add all prefixes to lost prefixes
		bound = false;
		script_call("unbound");

		if (server_id_len > 0 && (ia_pd_len > 0 || ia_na_len > 0) && release)
			dhcpv6_request(DHCPV6_MSG_RELEASE);

		odhcp6c_clear_state(STATE_IA_NA);
		odhcp6c_clear_state(STATE_IA_PD);
	}

	script_call("stopped");
	return 0;
}


static int usage(void)
{
	const char buf[] =
	"Usage: odhcp6c [options] <interface>\n"
	"\nFeature options:\n"
	"	-S <time>	Wait at least <time> sec for a DHCP-server (0)\n"
	"	-N <mode>	Mode for requesting addresses [try|force|none]\n"
	"	-P <length>	Request IPv6-Prefix (0 = auto)\n"
	"	-F		Force IPv6-Prefix\n"
	"	-V <class>	Set vendor-class option (base-16 encoded)\n"
	"	-u <user-class> Set user-class option string\n"
	"	-c <clientid>	Override client-ID (base-16 encoded 16-bit type + value)\n"
	"	-i <iface-id>	Use a custom interface identifier for RA handling\n"
	"	-r <options>	Options to be requested (comma-separated)\n"
	"	-R		Do not request any options except those specified with -r\n"
	"	-s <script>	Status update script (/usr/sbin/odhcp6c-update)\n"
	"	-a		Don't send Accept Reconfigure option\n"
	"	-f		Don't send Client FQDN option\n"
	"	-k		Don't send a RELEASE when stopping\n"
	"	-t <seconds>	Maximum timeout for DHCPv6-SOLICIT (3600)\n"
	"	-m <seconds>	Minimum time between accepting updates (30)\n"
	"\nInvocation options:\n"
	"	-p <pidfile>	Set pidfile (/var/run/odhcp6c.pid)\n"
	"	-d		Daemonize\n"
	"	-e		Write logmessages to stderr\n"
	"	-v		Increase logging verbosity\n"
	"	-h		Show this help\n\n";
	write(STDERR_FILENO, buf, sizeof(buf));
	return 1;
}


// Don't want to pull-in librt and libpthread just for a monotonic clock...
uint64_t odhcp6c_get_milli_time(void)
{
	struct timespec t = {0, 0};
	syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &t);
	return ((uint64_t)t.tv_sec) * 1000 + ((uint64_t)t.tv_nsec) / 1000000;
}


static uint8_t* odhcp6c_resize_state(enum odhcp6c_state state, ssize_t len)
{
	if (len == 0)
		return state_data[state] + state_len[state];
	else if (state_len[state] + len > 1024)
		return NULL;

	uint8_t *n = realloc(state_data[state], state_len[state] + len);
	if (n || state_len[state] + len == 0) {
		state_data[state] = n;
		n += state_len[state];
		state_len[state] += len;
	}
	return n;
}


bool odhcp6c_signal_process(void)
{
	while (signal_io) {
		signal_io = false;

		bool ra_updated = ra_process();

		if (ra_link_up())
			signal_usr2 = true;

		if (ra_updated && (bound || allow_slaac_only >= 0))
			script_call("ra-updated"); // Immediate process urgent events
	}

	return signal_usr1 || signal_usr2 || signal_term;
}


void odhcp6c_clear_state(enum odhcp6c_state state)
{
	state_len[state] = 0;
}


void odhcp6c_add_state(enum odhcp6c_state state, const void *data, size_t len)
{
	uint8_t *n = odhcp6c_resize_state(state, len);
	if (n)
		memcpy(n, data, len);
}

void odhcp6c_insert_state(enum odhcp6c_state state, size_t offset, const void *data, size_t len)
{
	ssize_t len_after = state_len[state] - offset;
	if (len_after < 0)
		return;

	uint8_t *n = odhcp6c_resize_state(state, len);
	if (n) {
		uint8_t *sdata = state_data[state];

		memmove(sdata + offset + len, sdata + offset, len_after);
		memcpy(sdata + offset, data, len);
	}
}

size_t odhcp6c_remove_state(enum odhcp6c_state state, size_t offset, size_t len)
{
	uint8_t *data = state_data[state];
	ssize_t len_after = state_len[state] - (offset + len);
	if (len_after < 0)
		return state_len[state];

	memmove(data + offset, data + offset + len, len_after);
	return state_len[state] -= len;
}


void* odhcp6c_move_state(enum odhcp6c_state state, size_t *len)
{
	*len = state_len[state];
	void *data = state_data[state];

	state_len[state] = 0;
	state_data[state] = NULL;

	return data;
}


void* odhcp6c_get_state(enum odhcp6c_state state, size_t *len)
{
	*len = state_len[state];
	return state_data[state];
}


struct odhcp6c_entry* odhcp6c_find_entry(enum odhcp6c_state state, const struct odhcp6c_entry *new)
{
	size_t len, cmplen = offsetof(struct odhcp6c_entry, target) + new->length / 8;
	struct odhcp6c_entry *start = odhcp6c_get_state(state, &len);
	struct odhcp6c_entry *x = NULL;

	for (struct odhcp6c_entry *c = start; !x && c < &start[len/sizeof(*c)]; ++c)
		if (!memcmp(c, new, cmplen))
			return c;

	return NULL;
}


bool odhcp6c_update_entry(enum odhcp6c_state state, struct odhcp6c_entry *new,
		uint32_t safe, bool filterexcess)
{
	size_t len;
	struct odhcp6c_entry *x = odhcp6c_find_entry(state, new);
	struct odhcp6c_entry *start = odhcp6c_get_state(state, &len);

	if (x && x->valid > new->valid && new->valid < safe)
		new->valid = safe;

	if (new->valid > 0) {
		if (x) {
			if (filterexcess && new->valid >= x->valid &&
					new->valid != UINT32_MAX &&
					new->valid - x->valid < min_update_interval &&
					new->preferred >= x->preferred &&
					new->preferred != UINT32_MAX &&
					new->preferred - x->preferred < min_update_interval &&
					x->class == new->class)
				return false;
			x->valid = new->valid;
			x->preferred = new->preferred;
			x->t1 = new->t1;
			x->t2 = new->t2;
			x->class = new->class;
			x->iaid = new->iaid;
		} else {
			odhcp6c_add_state(state, new, sizeof(*new));
		}
	} else if (x) {
		odhcp6c_remove_state(state, (x - start) * sizeof(*x), sizeof(*x));
	}
	return true;
}


static void odhcp6c_expire_list(enum odhcp6c_state state, uint32_t elapsed)
{
	size_t len;
	struct odhcp6c_entry *start = odhcp6c_get_state(state, &len);
	for (struct odhcp6c_entry *c = start; c < &start[len / sizeof(*c)]; ++c) {
		if (c->t1 < elapsed)
			c->t1 = 0;
		else if (c->t1 != UINT32_MAX)
			c->t1 -= elapsed;

		if (c->t2 < elapsed)
			c->t2 = 0;
		else if (c->t2 != UINT32_MAX)
			c->t2 -= elapsed;

		if (c->preferred < elapsed)
			c->preferred = 0;
		else if (c->preferred != UINT32_MAX)
			c->preferred -= elapsed;

		if (c->valid < elapsed)
			c->valid = 0;
		else if (c->valid != UINT32_MAX)
			c->valid -= elapsed;

		if (!c->valid)
			odhcp6c_remove_state(state, (c - start) * sizeof(*c), sizeof(*c));
	}
}


void odhcp6c_expire(void)
{
	time_t now = odhcp6c_get_milli_time() / 1000;
	uint32_t elapsed = (last_update > 0) ? now - last_update : 0;
	last_update = now;

	odhcp6c_expire_list(STATE_RA_PREFIX, elapsed);
	odhcp6c_expire_list(STATE_RA_ROUTE, elapsed);
	odhcp6c_expire_list(STATE_RA_DNS, elapsed);
	odhcp6c_expire_list(STATE_IA_NA, elapsed);
	odhcp6c_expire_list(STATE_IA_PD, elapsed);
}


uint32_t odhcp6c_elapsed(void)
{
	return odhcp6c_get_milli_time() / 1000 - last_update;
}


void odhcp6c_random(void *buf, size_t len)
{
	read(urandom_fd, buf, len);
}

bool odhcp6c_is_bound(void)
{
	return bound;
}

static void sighandler(int signal)
{
	if (signal == SIGCHLD)
		while (waitpid(-1, NULL, WNOHANG) > 0);
	else if (signal == SIGUSR1)
		signal_usr1 = true;
	else if (signal == SIGUSR2)
		signal_usr2 = true;
	else if (signal == SIGIO)
		signal_io = true;
	else
		signal_term = true;
}
