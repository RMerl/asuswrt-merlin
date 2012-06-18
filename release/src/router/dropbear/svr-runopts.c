/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"
#include "runopts.h"
#include "signkey.h"
#include "buffer.h"
#include "dbutil.h"
#include "algo.h"

svr_runopts svr_opts; /* GLOBAL */

static void printhelp(const char * progname);
static void addportandaddress(char* spec);

static void printhelp(const char * progname) {

	fprintf(stderr, "Dropbear sshd v%s\n"
					"Usage: %s [options]\n"
					"Options are:\n"
					"-b bannerfile	Display the contents of bannerfile"
					" before user login\n"
					"		(default: none)\n"
#ifdef DROPBEAR_DSS
					"-d dsskeyfile	Use dsskeyfile for the dss host key\n"
					"		(default: %s)\n"
#endif
#ifdef DROPBEAR_RSA
					"-r rsakeyfile	Use rsakeyfile for the rsa host key\n"
					"		(default: %s)\n"
#endif
					"-F		Don't fork into background\n"
#ifdef DISABLE_SYSLOG
					"(Syslog support not compiled in, using stderr)\n"
#else
					"-E		Log to stderr rather than syslog\n"
#endif
#ifdef DO_MOTD
					"-m		Don't display the motd on login\n"
#endif
					"-w		Disallow root logins\n"
#if defined(ENABLE_SVR_PASSWORD_AUTH) || defined(ENABLE_SVR_PAM_AUTH)
					"-s		Disable password logins\n"
					"-g		Disable password logins for root\n"
#endif
#ifdef ENABLE_SVR_LOCALTCPFWD
					"-j		Disable local port forwarding\n"
#endif
#ifdef ENABLE_SVR_REMOTETCPFWD
					"-k		Disable remote port forwarding\n"
					"-a		Allow connections to forwarded ports from any host\n"
#endif
					"-p [address:]port\n"
					"		Listen on specified tcp port (and optionally address),\n"
					"		up to %d can be specified\n"
					"		(default port is %s if none specified)\n"
					"-P PidFile	Create pid file PidFile\n"
					"		(default %s)\n"
#ifdef INETD_MODE
					"-i		Start for inetd\n"
#endif
					"-W <receive_window_buffer> (default %d, larger may be faster, max 1MB)\n"
					"-K <keepalive>  (0 is never, default %d)\n"
					"-I <idle_timeout>  (0 is never, default %d)\n"
#ifdef DEBUG_TRACE
					"-v		verbose (compiled with DEBUG_TRACE)\n"
#endif
					,DROPBEAR_VERSION, progname,
#ifdef DROPBEAR_DSS
					DSS_PRIV_FILENAME,
#endif
#ifdef DROPBEAR_RSA
					RSA_PRIV_FILENAME,
#endif
					DROPBEAR_MAX_PORTS, DROPBEAR_DEFPORT, DROPBEAR_PIDFILE,
					DEFAULT_RECV_WINDOW, DEFAULT_KEEPALIVE, DEFAULT_IDLE_TIMEOUT);
}

void svr_getopts(int argc, char ** argv) {

	unsigned int i;
	char ** next = 0;
	int nextisport = 0;
	char* recv_window_arg = NULL;
	char* keepalive_arg = NULL;
	char* idle_timeout_arg = NULL;

	/* see printhelp() for options */
	svr_opts.rsakeyfile = NULL;
	svr_opts.dsskeyfile = NULL;
	svr_opts.bannerfile = NULL;
	svr_opts.banner = NULL;
	svr_opts.forkbg = 1;
	svr_opts.norootlogin = 0;
	svr_opts.noauthpass = 0;
	svr_opts.norootpass = 0;
	svr_opts.inetdmode = 0;
	svr_opts.portcount = 0;
	svr_opts.hostkey = NULL;
	svr_opts.pidfile = DROPBEAR_PIDFILE;
#ifdef ENABLE_SVR_LOCALTCPFWD
	svr_opts.nolocaltcp = 0;
#endif
#ifdef ENABLE_SVR_REMOTETCPFWD
	svr_opts.noremotetcp = 0;
#endif
	/* not yet
	opts.ipv4 = 1;
	opts.ipv6 = 1;
	*/
#ifdef DO_MOTD
	svr_opts.domotd = 1;
#endif
#ifndef DISABLE_SYSLOG
	svr_opts.usingsyslog = 1;
#endif
	opts.recv_window = DEFAULT_RECV_WINDOW;
	opts.keepalive_secs = DEFAULT_KEEPALIVE;
	opts.idle_timeout_secs = DEFAULT_IDLE_TIMEOUT;
	
#ifdef ENABLE_SVR_REMOTETCPFWD
	opts.listen_fwd_all = 0;
#endif

	for (i = 1; i < (unsigned int)argc; i++) {
		if (nextisport) {
			addportandaddress(argv[i]);
			nextisport = 0;
			continue;
		}
	  
		if (next) {
			*next = argv[i];
			if (*next == NULL) {
				dropbear_exit("Invalid null argument");
			}
			next = 0x00;
			continue;
		}

		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'b':
					next = &svr_opts.bannerfile;
					break;
#ifdef DROPBEAR_DSS
				case 'd':
					next = &svr_opts.dsskeyfile;
					break;
#endif
#ifdef DROPBEAR_RSA
				case 'r':
					next = &svr_opts.rsakeyfile;
					break;
#endif
				case 'F':
					svr_opts.forkbg = 0;
					break;
#ifndef DISABLE_SYSLOG
				case 'E':
					svr_opts.usingsyslog = 0;
					break;
#endif
#ifdef ENABLE_SVR_LOCALTCPFWD
				case 'j':
					svr_opts.nolocaltcp = 1;
					break;
#endif
#ifdef ENABLE_SVR_REMOTETCPFWD
				case 'k':
					svr_opts.noremotetcp = 1;
					break;
				case 'a':
					opts.listen_fwd_all = 1;
					break;
#endif
#ifdef INETD_MODE
				case 'i':
					svr_opts.inetdmode = 1;
					break;
#endif
				case 'p':
				  nextisport = 1;
				  break;
				case 'P':
					next = &svr_opts.pidfile;
					break;
#ifdef DO_MOTD
				/* motd is displayed by default, -m turns it off */
				case 'm':
					svr_opts.domotd = 0;
					break;
#endif
				case 'w':
					svr_opts.norootlogin = 1;
					break;
				case 'W':
					next = &recv_window_arg;
					break;
				case 'K':
					next = &keepalive_arg;
					break;
				case 'I':
					next = &idle_timeout_arg;
					break;
#if defined(ENABLE_SVR_PASSWORD_AUTH) || defined(ENABLE_SVR_PAM_AUTH)
				case 's':
					svr_opts.noauthpass = 1;
					break;
				case 'g':
					svr_opts.norootpass = 1;
					break;
#endif
				case 'h':
					printhelp(argv[0]);
					exit(EXIT_FAILURE);
					break;
				case 'u':
					/* backwards compatibility with old urandom option */
					break;
#ifdef DEBUG_TRACE
				case 'v':
					debug_trace = 1;
					break;
#endif
				default:
					fprintf(stderr, "Unknown argument %s\n", argv[i]);
					printhelp(argv[0]);
					exit(EXIT_FAILURE);
					break;
			}
		}
	}

	/* Set up listening ports */
	if (svr_opts.portcount == 0) {
		svr_opts.ports[0] = m_strdup(DROPBEAR_DEFPORT);
		svr_opts.addresses[0] = m_strdup(DROPBEAR_DEFADDRESS);
		svr_opts.portcount = 1;
	}

	if (svr_opts.dsskeyfile == NULL) {
		svr_opts.dsskeyfile = DSS_PRIV_FILENAME;
	}
	if (svr_opts.rsakeyfile == NULL) {
		svr_opts.rsakeyfile = RSA_PRIV_FILENAME;
	}

	if (svr_opts.bannerfile) {
		struct stat buf;
		if (stat(svr_opts.bannerfile, &buf) != 0) {
			dropbear_exit("Error opening banner file '%s'",
					svr_opts.bannerfile);
		}
		
		if (buf.st_size > MAX_BANNER_SIZE) {
			dropbear_exit("Banner file too large, max is %d bytes",
					MAX_BANNER_SIZE);
		}

		svr_opts.banner = buf_new(buf.st_size);
		if (buf_readfile(svr_opts.banner, svr_opts.bannerfile)!=DROPBEAR_SUCCESS) {
			dropbear_exit("Error reading banner file '%s'",
					svr_opts.bannerfile);
		}
		buf_setpos(svr_opts.banner, 0);

	}
	
	if (recv_window_arg) {
		opts.recv_window = atol(recv_window_arg);
		if (opts.recv_window == 0 || opts.recv_window > MAX_RECV_WINDOW) {
			dropbear_exit("Bad recv window '%s'", recv_window_arg);
		}
	}
	
	if (keepalive_arg) {
		if (m_str_to_uint(keepalive_arg, &opts.keepalive_secs) == DROPBEAR_FAILURE) {
			dropbear_exit("Bad keepalive '%s'", keepalive_arg);
		}
	}

	if (idle_timeout_arg) {
		if (m_str_to_uint(idle_timeout_arg, &opts.idle_timeout_secs) == DROPBEAR_FAILURE) {
			dropbear_exit("Bad idle_timeout '%s'", idle_timeout_arg);
		}
	}
}

static void addportandaddress(char* spec) {

	char *myspec = NULL;

	if (svr_opts.portcount < DROPBEAR_MAX_PORTS) {

		/* We don't free it, it becomes part of the runopt state */
		myspec = m_strdup(spec);

		/* search for ':', that separates address and port */
		svr_opts.ports[svr_opts.portcount] = strchr(myspec, ':');

		if (svr_opts.ports[svr_opts.portcount] == NULL) {
			/* no ':' -> the whole string specifies just a port */
			svr_opts.ports[svr_opts.portcount] = myspec;
		} else {
			/* Split the address/port */
			svr_opts.ports[svr_opts.portcount][0] = '\0'; 
			svr_opts.ports[svr_opts.portcount]++;
			svr_opts.addresses[svr_opts.portcount] = myspec;
		}

		if (svr_opts.addresses[svr_opts.portcount] == NULL) {
			/* no address given -> fill in the default address */
			svr_opts.addresses[svr_opts.portcount] = m_strdup(DROPBEAR_DEFADDRESS);
		}

		if (svr_opts.ports[svr_opts.portcount][0] == '\0') {
			/* empty port -> exit */
			dropbear_exit("Bad port");
		}

		svr_opts.portcount++;
	}
}

static void disablekey(int type, const char* filename) {

	int i;

	for (i = 0; sshhostkey[i].name != NULL; i++) {
		if (sshhostkey[i].val == type) {
			sshhostkey[i].usable = 0;
			break;
		}
	}
	dropbear_log(LOG_WARNING, "Failed reading '%s', disabling %s", filename,
			type == DROPBEAR_SIGNKEY_DSS ? "DSS" : "RSA");
}

/* Must be called after syslog/etc is working */
void loadhostkeys() {

	int ret;
	int type;

	TRACE(("enter loadhostkeys"))

	svr_opts.hostkey = new_sign_key();

#ifdef DROPBEAR_RSA
	type = DROPBEAR_SIGNKEY_RSA;
	ret = readhostkey(svr_opts.rsakeyfile, svr_opts.hostkey, &type);
	if (ret == DROPBEAR_FAILURE) {
		disablekey(DROPBEAR_SIGNKEY_RSA, svr_opts.rsakeyfile);
	}
#endif
#ifdef DROPBEAR_DSS
	type = DROPBEAR_SIGNKEY_DSS;
	ret = readhostkey(svr_opts.dsskeyfile, svr_opts.hostkey, &type);
	if (ret == DROPBEAR_FAILURE) {
		disablekey(DROPBEAR_SIGNKEY_DSS, svr_opts.dsskeyfile);
	}
#endif

	if ( 1
#ifdef DROPBEAR_DSS
		&& svr_opts.hostkey->dsskey == NULL
#endif
#ifdef DROPBEAR_RSA
		&& svr_opts.hostkey->rsakey == NULL
#endif
		) {
		dropbear_exit("No hostkeys available");
	}

	TRACE(("leave loadhostkeys"))
}
