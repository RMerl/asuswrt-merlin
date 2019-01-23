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
#include "ecdsa.h"

svr_runopts svr_opts; /* GLOBAL */

static void printhelp(const char * progname);
static void addportandaddress(const char* spec);
static void loadhostkey(const char *keyfile, int fatal_duplicate);
static void addhostkey(const char *keyfile);

static void printhelp(const char * progname) {

	fprintf(stderr, "Dropbear server v%s https://matt.ucc.asn.au/dropbear/dropbear.html\n"
					"Usage: %s [options]\n"
					"-b bannerfile	Display the contents of bannerfile"
					" before user login\n"
					"		(default: none)\n"
					"-r keyfile  Specify hostkeys (repeatable)\n"
					"		defaults: \n"
#ifdef DROPBEAR_DSS
					"		dss %s\n"
#endif
#ifdef DROPBEAR_RSA
					"		rsa %s\n"
#endif
#ifdef DROPBEAR_ECDSA
					"		ecdsa %s\n"
#endif
#ifdef DROPBEAR_DELAY_HOSTKEY
					"-R		Create hostkeys as required\n" 
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
					"-B		Allow blank password logins\n"
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
					"-K <keepalive>  (0 is never, default %d, in seconds)\n"
					"-I <idle_timeout>  (0 is never, default %d, in seconds)\n"
					"-V    Version\n"
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
#ifdef DROPBEAR_ECDSA
					ECDSA_PRIV_FILENAME,
#endif
					DROPBEAR_MAX_PORTS, DROPBEAR_DEFPORT, DROPBEAR_PIDFILE,
					DEFAULT_RECV_WINDOW, DEFAULT_KEEPALIVE, DEFAULT_IDLE_TIMEOUT);
}

void svr_getopts(int argc, char ** argv) {

	unsigned int i, j;
	char ** next = 0;
	int nextisport = 0;
	char* recv_window_arg = NULL;
	char* keepalive_arg = NULL;
	char* idle_timeout_arg = NULL;
	char* keyfile = NULL;
	char c;


	/* see printhelp() for options */
	svr_opts.bannerfile = NULL;
	svr_opts.banner = NULL;
	svr_opts.forkbg = 1;
	svr_opts.norootlogin = 0;
	svr_opts.noauthpass = 0;
	svr_opts.norootpass = 0;
	svr_opts.allowblankpass = 0;
	svr_opts.inetdmode = 0;
	svr_opts.portcount = 0;
	svr_opts.hostkey = NULL;
	svr_opts.delay_hostkey = 0;
	svr_opts.pidfile = DROPBEAR_PIDFILE;
#ifdef ENABLE_SVR_LOCALTCPFWD
	svr_opts.nolocaltcp = 0;
#endif
#ifdef ENABLE_SVR_REMOTETCPFWD
	svr_opts.noremotetcp = 0;
#endif

#ifndef DISABLE_ZLIB
#if DROPBEAR_SERVER_DELAY_ZLIB
	opts.compress_mode = DROPBEAR_COMPRESS_DELAYED;
#else
	opts.compress_mode = DROPBEAR_COMPRESS_ON;
#endif
#endif 

	/* not yet
	opts.ipv4 = 1;
	opts.ipv6 = 1;
	*/
#ifdef DO_MOTD
	svr_opts.domotd = 1;
#endif
#ifndef DISABLE_SYSLOG
	opts.usingsyslog = 1;
#endif
	opts.recv_window = DEFAULT_RECV_WINDOW;
	opts.keepalive_secs = DEFAULT_KEEPALIVE;
	opts.idle_timeout_secs = DEFAULT_IDLE_TIMEOUT;
	
#ifdef ENABLE_SVR_REMOTETCPFWD
	opts.listen_fwd_all = 0;
#endif

	for (i = 1; i < (unsigned int)argc; i++) {
		if (argv[i][0] != '-' || argv[i][1] == '\0')
			dropbear_exit("Invalid argument: %s", argv[i]);

		for (j = 1; (c = argv[i][j]) != '\0' && !next && !nextisport; j++) {
			switch (c) {
				case 'b':
					next = &svr_opts.bannerfile;
					break;
				case 'd':
				case 'r':
					next = &keyfile;
					break;
				case 'R':
					svr_opts.delay_hostkey = 1;
					break;
				case 'F':
					svr_opts.forkbg = 0;
					break;
#ifndef DISABLE_SYSLOG
				case 'E':
					opts.usingsyslog = 0;
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
				case 'B':
					svr_opts.allowblankpass = 1;
					break;
#endif
				case 'h':
					printhelp(argv[0]);
					exit(EXIT_SUCCESS);
					break;
				case 'u':
					/* backwards compatibility with old urandom option */
					break;
#ifdef DEBUG_TRACE
				case 'v':
					debug_trace = 1;
					break;
#endif
				case 'V':
					print_version();
					exit(EXIT_SUCCESS);
					break;
				default:
					fprintf(stderr, "Invalid option -%c\n", c);
					printhelp(argv[0]);
					exit(EXIT_FAILURE);
					break;
			}
		}

		if (!next && !nextisport)
			continue;

		if (c == '\0') {
			i++;
			j = 0;
			if (!argv[i]) {
				dropbear_exit("Missing argument");
			}
		}

		if (nextisport) {
			addportandaddress(&argv[i][j]);
			nextisport = 0;
		} else if (next) {
			*next = &argv[i][j];
			if (*next == NULL) {
				dropbear_exit("Invalid null argument");
			}
			next = 0x00;

			if (keyfile) {
				addhostkey(keyfile);
				keyfile = NULL;
			}
		}
	}

	/* Set up listening ports */
	if (svr_opts.portcount == 0) {
		svr_opts.ports[0] = m_strdup(DROPBEAR_DEFPORT);
		svr_opts.addresses[0] = m_strdup(DROPBEAR_DEFADDRESS);
		svr_opts.portcount = 1;
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
		unsigned int val;
		if (m_str_to_uint(keepalive_arg, &val) == DROPBEAR_FAILURE) {
			dropbear_exit("Bad keepalive '%s'", keepalive_arg);
		}
		opts.keepalive_secs = val;
	}

	if (idle_timeout_arg) {
		unsigned int val;
		if (m_str_to_uint(idle_timeout_arg, &val) == DROPBEAR_FAILURE) {
			dropbear_exit("Bad idle_timeout '%s'", idle_timeout_arg);
		}
		opts.idle_timeout_secs = val;
	}
}

static void addportandaddress(const char* spec) {
	char *spec_copy = NULL, *myspec = NULL, *port = NULL, *address = NULL;

	if (svr_opts.portcount < DROPBEAR_MAX_PORTS) {

		/* We don't free it, it becomes part of the runopt state */
		spec_copy = m_strdup(spec);
		myspec = spec_copy;

		if (myspec[0] == '[') {
			myspec++;
			port = strchr(myspec, ']');
			if (!port) {
				/* Unmatched [ -> exit */
				dropbear_exit("Bad listen address");
			}
			port[0] = '\0';
			port++;
			if (port[0] != ':') {
				/* Missing port -> exit */
				dropbear_exit("Missing port");
			}
		} else {
			/* search for ':', that separates address and port */
			port = strrchr(myspec, ':');
		}

		if (!port) {
			/* no ':' -> the whole string specifies just a port */
			port = myspec;
		} else {
			/* Split the address/port */
			port[0] = '\0'; 
			port++;
			address = myspec;
		}

		if (!address) {
			/* no address given -> fill in the default address */
			address = DROPBEAR_DEFADDRESS;
		}

		if (port[0] == '\0') {
			/* empty port -> exit */
			dropbear_exit("Bad port");
		}
		svr_opts.ports[svr_opts.portcount] = m_strdup(port);
		svr_opts.addresses[svr_opts.portcount] = m_strdup(address);
		svr_opts.portcount++;
		m_free(spec_copy);
	}
}

static void disablekey(int type) {
	int i;
	TRACE(("Disabling key type %d", type))
	for (i = 0; sshhostkey[i].name != NULL; i++) {
		if (sshhostkey[i].val == type) {
			sshhostkey[i].usable = 0;
			break;
		}
	}
}

static void loadhostkey_helper(const char *name, void** src, void** dst, int fatal_duplicate) {
	if (*dst) {
		if (fatal_duplicate) {
			dropbear_exit("Only one %s key can be specified", name);
		}
	} else {
		*dst = *src;
		*src = NULL;
	}

}

/* Must be called after syslog/etc is working */
static void loadhostkey(const char *keyfile, int fatal_duplicate) {
	sign_key * read_key = new_sign_key();
	enum signkey_type type = DROPBEAR_SIGNKEY_ANY;
	if (readhostkey(keyfile, read_key, &type) == DROPBEAR_FAILURE) {
		if (!svr_opts.delay_hostkey) {
			dropbear_log(LOG_WARNING, "Failed loading %s", keyfile);
		}
	}

#ifdef DROPBEAR_RSA
	if (type == DROPBEAR_SIGNKEY_RSA) {
		loadhostkey_helper("RSA", (void**)&read_key->rsakey, (void**)&svr_opts.hostkey->rsakey, fatal_duplicate);
	}
#endif

#ifdef DROPBEAR_DSS
	if (type == DROPBEAR_SIGNKEY_DSS) {
		loadhostkey_helper("DSS", (void**)&read_key->dsskey, (void**)&svr_opts.hostkey->dsskey, fatal_duplicate);
	}
#endif

#ifdef DROPBEAR_ECDSA
#ifdef DROPBEAR_ECC_256
	if (type == DROPBEAR_SIGNKEY_ECDSA_NISTP256) {
		loadhostkey_helper("ECDSA256", (void**)&read_key->ecckey256, (void**)&svr_opts.hostkey->ecckey256, fatal_duplicate);
	}
#endif
#ifdef DROPBEAR_ECC_384
	if (type == DROPBEAR_SIGNKEY_ECDSA_NISTP384) {
		loadhostkey_helper("ECDSA384", (void**)&read_key->ecckey384, (void**)&svr_opts.hostkey->ecckey384, fatal_duplicate);
	}
#endif
#ifdef DROPBEAR_ECC_521
	if (type == DROPBEAR_SIGNKEY_ECDSA_NISTP521) {
		loadhostkey_helper("ECDSA521", (void**)&read_key->ecckey521, (void**)&svr_opts.hostkey->ecckey521, fatal_duplicate);
	}
#endif
#endif /* DROPBEAR_ECDSA */
	sign_key_free(read_key);
	TRACE(("leave loadhostkey"))
}

static void addhostkey(const char *keyfile) {
	if (svr_opts.num_hostkey_files >= MAX_HOSTKEYS) {
		dropbear_exit("Too many hostkeys");
	}
	svr_opts.hostkey_files[svr_opts.num_hostkey_files] = m_strdup(keyfile);
	svr_opts.num_hostkey_files++;
}

void load_all_hostkeys() {
	int i;
	int disable_unset_keys = 1;
	int any_keys = 0;

	svr_opts.hostkey = new_sign_key();

	for (i = 0; i < svr_opts.num_hostkey_files; i++) {
		char *hostkey_file = svr_opts.hostkey_files[i];
		loadhostkey(hostkey_file, 1);
		m_free(hostkey_file);
	}

#ifdef DROPBEAR_RSA
	loadhostkey(RSA_PRIV_FILENAME, 0);
#endif

#ifdef DROPBEAR_DSS
	loadhostkey(DSS_PRIV_FILENAME, 0);
#endif

#ifdef DROPBEAR_ECDSA
	loadhostkey(ECDSA_PRIV_FILENAME, 0);
#endif

#ifdef DROPBEAR_DELAY_HOSTKEY
	if (svr_opts.delay_hostkey) {
		disable_unset_keys = 0;
	}
#endif

#ifdef DROPBEAR_RSA
	if (disable_unset_keys && !svr_opts.hostkey->rsakey) {
		disablekey(DROPBEAR_SIGNKEY_RSA);
	} else {
		any_keys = 1;
	}
#endif

#ifdef DROPBEAR_DSS
	if (disable_unset_keys && !svr_opts.hostkey->dsskey) {
		disablekey(DROPBEAR_SIGNKEY_DSS);
	} else {
		any_keys = 1;
	}
#endif


#ifdef DROPBEAR_ECDSA
#ifdef DROPBEAR_ECC_256
	if ((disable_unset_keys || ECDSA_DEFAULT_SIZE != 256)
		&& !svr_opts.hostkey->ecckey256) {
		disablekey(DROPBEAR_SIGNKEY_ECDSA_NISTP256);
	} else {
		any_keys = 1;
	}
#endif

#ifdef DROPBEAR_ECC_384
	if ((disable_unset_keys || ECDSA_DEFAULT_SIZE != 384)
		&& !svr_opts.hostkey->ecckey384) {
		disablekey(DROPBEAR_SIGNKEY_ECDSA_NISTP384);
	} else {
		any_keys = 1;
	}
#endif

#ifdef DROPBEAR_ECC_521
	if ((disable_unset_keys || ECDSA_DEFAULT_SIZE != 521)
		&& !svr_opts.hostkey->ecckey521) {
		disablekey(DROPBEAR_SIGNKEY_ECDSA_NISTP521);
	} else {
		any_keys = 1;
	}
#endif
#endif /* DROPBEAR_ECDSA */

	if (!any_keys) {
		dropbear_exit("No hostkeys available. 'dropbear -R' may be useful or run dropbearkey.");
	}
}
