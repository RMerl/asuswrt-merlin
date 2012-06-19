/* Dropbear SSH
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved. See LICENSE for the license. */

#ifndef _OPTIONS_H_
#define _OPTIONS_H_

/******************************************************************
 * Define compile-time options below - the "#ifndef DROPBEAR_XXX .... #endif"
 * parts are to allow for commandline -DDROPBEAR_XXX options etc.
 ******************************************************************/

#ifndef DROPBEAR_DEFPORT
#define DROPBEAR_DEFPORT "22"
#endif

#ifndef DROPBEAR_DEFADDRESS
/* Listen on all interfaces */
#define DROPBEAR_DEFADDRESS ""
#endif

/* Default hostkey paths - these can be specified on the command line */
#ifndef DSS_PRIV_FILENAME
#define DSS_PRIV_FILENAME "/etc/dropbear/dropbear_dss_host_key"
#endif
#ifndef RSA_PRIV_FILENAME
#define RSA_PRIV_FILENAME "/etc/dropbear/dropbear_rsa_host_key"
#endif

/* Set NON_INETD_MODE if you require daemon functionality (ie Dropbear listens
 * on chosen ports and keeps accepting connections. This is the default.
 *
 * Set INETD_MODE if you want to be able to run Dropbear with inetd (or
 * similar), where it will use stdin/stdout for connections, and each process
 * lasts for a single connection. Dropbear should be invoked with the -i flag
 * for inetd, and can only accept IPv4 connections.
 *
 * Both of these flags can be defined at once, don't compile without at least
 * one of them. */
#define NON_INETD_MODE
#define INETD_MODE

/* Setting this disables the fast exptmod bignum code. It saves ~5kB, but is
 * perhaps 20% slower for pubkey operations (it is probably worth experimenting
 * if you want to use this) */
/*#define NO_FAST_EXPTMOD*/

/* Set this if you want to use the DROPBEAR_SMALL_CODE option. This can save
several kB in binary size however will make the symmetrical ciphers and hashes
slower, perhaps by 50%. Recommended for small systems that aren't doing
much traffic. */
/*#define DROPBEAR_SMALL_CODE*/

/* Enable X11 Forwarding - server only */
#define ENABLE_X11FWD

/* Enable TCP Fowarding */
/* 'Local' is "-L" style (client listening port forwarded via server)
 * 'Remote' is "-R" style (server listening port forwarded via client) */

#define ENABLE_CLI_LOCALTCPFWD
#define ENABLE_CLI_REMOTETCPFWD

#define ENABLE_SVR_LOCALTCPFWD
#define ENABLE_SVR_REMOTETCPFWD

/* Enable Authentication Agent Forwarding */
#define ENABLE_SVR_AGENTFWD
#define ENABLE_CLI_AGENTFWD


/* Note: Both ENABLE_CLI_PROXYCMD and ENABLE_CLI_NETCAT must be set to
 * allow multihop dbclient connections */

/* Allow using -J <proxycommand> to run the connection through a 
   pipe to a program, rather the normal TCP connection */
#define ENABLE_CLI_PROXYCMD

/* Enable "Netcat mode" option. This will forward standard input/output
 * to a remote TCP-forwarded connection */
#define ENABLE_CLI_NETCAT

/* Encryption - at least one required.
 * Protocol RFC requires 3DES and recommends AES128 for interoperability.
 * Including multiple keysize variants the same cipher 
 * (eg AES256 as well as AES128) will result in a minimal size increase.*/
#define DROPBEAR_AES128
#define DROPBEAR_3DES
#define DROPBEAR_AES256
/* Compiling in Blowfish will add ~6kB to runtime heap memory usage */
/*#define DROPBEAR_BLOWFISH*/
#define DROPBEAR_TWOFISH256
#define DROPBEAR_TWOFISH128

/* Enable "Counter Mode" for ciphers. This is more secure than normal
 * CBC mode against certain attacks. This adds around 1kB to binary 
 * size and is recommended for most cases */
#define DROPBEAR_ENABLE_CTR_MODE

/* Message Integrity - at least one required.
 * Protocol RFC requires sha1 and recommends sha1-96.
 * sha1-96 may be of use for slow links, as it has a smaller overhead.
 *
 * Note: there's no point disabling sha1 to save space, since it's used
 * for the random number generator and public-key cryptography anyway.
 * Disabling it here will just stop it from being used as the integrity portion
 * of the ssh protocol.
 *
 * These hashes are also used for public key fingerprints in logs.
 * If you disable MD5, Dropbear will fall back to SHA1 fingerprints,
 * which are not the standard form. */
#define DROPBEAR_SHA1_HMAC
#define DROPBEAR_SHA1_96_HMAC
#define DROPBEAR_MD5_HMAC

/* Hostkey/public key algorithms - at least one required, these are used
 * for hostkey as well as for verifying signatures with pubkey auth.
 * Removing either of these won't save very much space.
 * SSH2 RFC Draft requires dss, recommends rsa */
#define DROPBEAR_RSA
#define DROPBEAR_DSS

/* RSA can be vulnerable to timing attacks which use the time required for
 * signing to guess the private key. Blinding avoids this attack, though makes
 * signing operations slightly slower. */
#define RSA_BLINDING

/* Define DSS_PROTOK to use PuTTY's method of generating the value k for dss,
 * rather than just from the random byte source. Undefining this will save you
 * ~4k in binary size with static uclibc, but your DSS hostkey could be exposed
 * if the random number source isn't good. It happened to Sony. 
 * On systems with a decent random source this isn't required. */
/* #define DSS_PROTOK */

/* Control the memory/performance/compression tradeoff for zlib.
 * Set windowBits=8 for least memory usage, see your system's
 * zlib.h for full details.
 * Default settings (windowBits=15) will use 256kB for compression
 * windowBits=8 will use 129kB for compression.
 * Both modes will use ~35kB for decompression (using windowBits=15 for
 * interoperability) */
#ifndef DROPBEAR_ZLIB_WINDOW_BITS
#define DROPBEAR_ZLIB_WINDOW_BITS 15 
#endif

/* Whether to do reverse DNS lookups. */
#define DO_HOST_LOOKUP

/* Whether to print the message of the day (MOTD). This doesn't add much code
 * size */
#define DO_MOTD

/* The MOTD file path */
#ifndef MOTD_FILENAME
#define MOTD_FILENAME "/etc/motd"
#endif

/* Authentication Types - at least one required.
   RFC Draft requires pubkey auth, and recommends password */

/* Note: PAM auth is quite simple and only works for PAM modules which just do
 * a simple "Login: " "Password: " (you can edit the strings in svr-authpam.c).
 * It's useful for systems like OS X where standard password crypts don't work
 * but there's an interface via a PAM module. It won't work for more complex
 * PAM challenge/response.
 * You can't enable both PASSWORD and PAM. */

#define ENABLE_SVR_PASSWORD_AUTH
/* PAM requires ./configure --enable-pam */
/*#define ENABLE_SVR_PAM_AUTH*/
#define ENABLE_SVR_PUBKEY_AUTH

/* Whether to take public key options in 
 * authorized_keys file into account */
#ifdef ENABLE_SVR_PUBKEY_AUTH
#define ENABLE_SVR_PUBKEY_OPTIONS
#endif

/* Define this to allow logging in to accounts that have no password specified.
 * Public key logins are allowed for blank-password accounts regardless of this
 * setting.  PAM is not affected by this setting, it uses the normal pam.d
 * settings ('nullok' option) */
/* #define ALLOW_BLANK_PASSWORD */

#define ENABLE_CLI_PASSWORD_AUTH
#define ENABLE_CLI_PUBKEY_AUTH
#define ENABLE_CLI_INTERACT_AUTH

/* This variable can be used to set a password for client
 * authentication on the commandline. Beware of platforms
 * that don't protect environment variables of processes etc. Also
 * note that it will be provided for all "hidden" client-interactive
 * style prompts - if you want something more sophisticated, use 
 * SSH_ASKPASS instead. Comment out this var to remove this functionality.*/
#define DROPBEAR_PASSWORD_ENV "DROPBEAR_PASSWORD"

/* Define this (as well as ENABLE_CLI_PASSWORD_AUTH) to allow the use of
 * a helper program for the ssh client. The helper program should be
 * specified in the SSH_ASKPASS environment variable, and dbclient
 * should be run with DISPLAY set and no tty. The program should
 * return the password on standard output */
/*#define ENABLE_CLI_ASKPASS_HELPER*/

/* Random device to use - define either DROPBEAR_RANDOM_DEV or
 * DROPBEAR_PRNGD_SOCKET.
 * DROPBEAR_RANDOM_DEV is recommended on hosts with a good /dev/(u)random,
 * otherwise use run prngd (or egd if you want), specifying the socket. 
 * The device will be queried for a few dozen bytes of seed a couple of times
 * per session (or more for very long-lived sessions). */

/* We'll use /dev/urandom by default, since /dev/random is too much hassle.
 * If system developers aren't keeping seeds between boots nor getting
 * any entropy from somewhere it's their own fault. */
#define DROPBEAR_RANDOM_DEV "/dev/urandom"

/* prngd must be manually set up to produce output */
/*#define DROPBEAR_PRNGD_SOCKET "/var/run/dropbear-rng"*/

/* Specify the number of clients we will allow to be connected but
 * not yet authenticated. After this limit, connections are rejected */
/* The first setting is per-IP, to avoid denial of service */
#ifndef MAX_UNAUTH_PER_IP
#define MAX_UNAUTH_PER_IP 5
#endif

/* And then a global limit to avoid chewing memory if connections 
 * come from many IPs */
#ifndef MAX_UNAUTH_CLIENTS
#define MAX_UNAUTH_CLIENTS 30
#endif

/* Maximum number of failed authentication tries (server option) */
#ifndef MAX_AUTH_TRIES
#define MAX_AUTH_TRIES 10
#endif

/* The default file to store the daemon's process ID, for shutdown
   scripts etc. This can be overridden with the -P flag */
#ifndef DROPBEAR_PIDFILE
#define DROPBEAR_PIDFILE "/var/run/dropbear.pid"
#endif

/* The command to invoke for xauth when using X11 forwarding.
 * "-q" for quiet */
#ifndef XAUTH_COMMAND
#define XAUTH_COMMAND "/usr/bin/X11/xauth -q"
#endif

/* if you want to enable running an sftp server (such as the one included with
 * OpenSSH), set the path below. If the path isn't defined, sftp will not
 * be enabled */
#ifndef SFTPSERVER_PATH
#define SFTPSERVER_PATH "/usr/libexec/sftp-server"
#endif

/* This is used by the scp binary when used as a client binary. If you're
 * not using the Dropbear client, you'll need to change it */
#define _PATH_SSH_PROGRAM "/usr/bin/dbclient"

/* Whether to log commands executed by a client. This only logs the 
 * (single) command sent to the server, not what a user did in a 
 * shell/sftp session etc. */
/* #define LOG_COMMANDS */

/* Window size limits. These tend to be a trade-off between memory
   usage and network performance: */
/* Size of the network receive window. This amount of memory is allocated
   as a per-channel receive buffer. Increasing this value can make a
   significant difference to network performance. 24kB was empirically
   chosen for a 100mbit ethernet network. The value can be altered at
   runtime with the -W argument. */
#ifndef DEFAULT_RECV_WINDOW
#define DEFAULT_RECV_WINDOW 24576
#endif
/* Maximum size of a received SSH data packet - this _MUST_ be >= 32768
   in order to interoperate with other implementations */
#ifndef RECV_MAX_PAYLOAD_LEN
#define RECV_MAX_PAYLOAD_LEN 32768
#endif
/* Maximum size of a transmitted data packet - this can be any value,
   though increasing it may not make a significant difference. */
#ifndef TRANS_MAX_PAYLOAD_LEN
#define TRANS_MAX_PAYLOAD_LEN 16384
#endif

/* Ensure that data is transmitted every KEEPALIVE seconds. This can
be overridden at runtime with -K. 0 disables keepalives */
#define DEFAULT_KEEPALIVE 0

/* Ensure that data is received within IDLE_TIMEOUT seconds. This can
be overridden at runtime with -I. 0 disables idle timeouts */
#define DEFAULT_IDLE_TIMEOUT 0

/* The default path. This will often get replaced by the shell */
#define DEFAULT_PATH "/usr/bin:/bin"

/* Some other defines (that mostly should be left alone) are defined
 * in sysoptions.h */
#include "sysoptions.h"

#endif /* _OPTIONS_H_ */
