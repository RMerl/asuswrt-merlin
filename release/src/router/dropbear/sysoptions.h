/*******************************************************************
 * You shouldn't edit this file unless you know you need to. 
 * This file is only included from options.h
 *******************************************************************/

#ifndef DROPBEAR_VERSION
#define DROPBEAR_VERSION "0.52"
#endif

#define LOCAL_IDENT "SSH-2.0-dropbear_" DROPBEAR_VERSION
#define PROGNAME "dropbear"

/* Spec recommends after one hour or 1 gigabyte of data. One hour
 * is a bit too verbose, so we try 8 hours */
#ifndef KEX_REKEY_TIMEOUT
#define KEX_REKEY_TIMEOUT (3600 * 8)
#endif
#ifndef KEX_REKEY_DATA
#define KEX_REKEY_DATA (1<<30) /* 2^30 == 1GB, this value must be < INT_MAX */
#endif
/* Close connections to clients which haven't authorised after AUTH_TIMEOUT */
#ifndef AUTH_TIMEOUT
#define AUTH_TIMEOUT 300 /* we choose 5 minutes */
#endif

/* Minimum key sizes for DSS and RSA */
#ifndef MIN_DSS_KEYLEN
#define MIN_DSS_KEYLEN 512
#endif
#ifndef MIN_RSA_KEYLEN
#define MIN_RSA_KEYLEN 512
#endif

#define MAX_BANNER_SIZE 2000 /* this is 25*80 chars, any more is foolish */
#define MAX_BANNER_LINES 20 /* How many lines the client will display */

/* the number of NAME=VALUE pairs to malloc for environ, if we don't have
 * the clearenv() function */
#define ENV_SIZE 100

#define MAX_CMD_LEN 1024 /* max length of a command */
#define MAX_TERM_LEN 200 /* max length of TERM name */

#define MAX_HOST_LEN 254 /* max hostname len for tcp fwding */
#define MAX_IP_LEN 15 /* strlen("255.255.255.255") == 15 */

#define DROPBEAR_MAX_PORTS 10 /* max number of ports which can be specified,
								 ipv4 and ipv6 don't count twice */

/* Each port might have at least a v4 and a v6 address */
#define MAX_LISTEN_ADDR (DROPBEAR_MAX_PORTS*3)

#define _PATH_TTY "/dev/tty"

#define _PATH_CP "/bin/cp"

/* success/failure defines */
#define DROPBEAR_SUCCESS 0
#define DROPBEAR_FAILURE -1

/* various algorithm identifiers */
#define DROPBEAR_KEX_DH_GROUP1 0

#define DROPBEAR_SIGNKEY_ANY 0
#define DROPBEAR_SIGNKEY_RSA 1
#define DROPBEAR_SIGNKEY_DSS 2
#define DROPBEAR_SIGNKEY_NONE 3

#define DROPBEAR_COMP_NONE 0
#define DROPBEAR_COMP_ZLIB 1
#define DROPBEAR_COMP_ZLIB_DELAY 2

/* Required for pubkey auth */
#if defined(ENABLE_SVR_PUBKEY_AUTH) || defined(DROPBEAR_CLIENT)
#define DROPBEAR_SIGNKEY_VERIFY
#endif

/* SHA1 is 20 bytes == 160 bits */
#define SHA1_HASH_SIZE 20
/* SHA512 is 64 bytes == 512 bits */
#define SHA512_HASH_SIZE 64
/* MD5 is 16 bytes = 128 bits */
#define MD5_HASH_SIZE 16

/* largest of MD5 and SHA1 */
#define MAX_MAC_LEN SHA1_HASH_SIZE


#define MAX_KEY_LEN 32 /* 256 bits for aes256 etc */
#define MAX_IV_LEN 20 /* must be same as max blocksize, 
						 and >= SHA1_HASH_SIZE */
#define MAX_MAC_KEY 20

#define MAX_NAME_LEN 64 /* maximum length of a protocol name, isn't
						   explicitly specified for all protocols (just
						   for algos) but seems valid */

#define MAX_PROPOSED_ALGO 20

/* size/count limits */
#define MIN_PACKET_LEN 16

#define RECV_MAX_PACKET_LEN (MAX(35000, ((RECV_MAX_PAYLOAD_LEN)+100)))

/* for channel code */
#define TRANS_MAX_WINDOW 500000000 /* 500MB is sufficient, stopping overflow */
#define TRANS_MAX_WIN_INCR 500000000 /* overflow prevention */

#define RECV_WINDOWEXTEND (opts.recv_window / 3) /* We send a "window extend" every
								RECV_WINDOWEXTEND bytes */
#define MAX_RECV_WINDOW (1024*1024) /* 1 MB should be enough */

#define MAX_CHANNELS 100 /* simple mem restriction, includes each tcp/x11
							connection, so can't be _too_ small */

#define MAX_STRING_LEN 1400 /* ~= MAX_PROPOSED_ALGO * MAX_NAME_LEN, also
							   is the max length for a password etc */

/* For a 4096 bit DSS key, empirically determined */
#define MAX_PUBKEY_SIZE 1700
/* For a 4096 bit DSS key, empirically determined */
#define MAX_PRIVKEY_SIZE 1700

/* The maximum size of the bignum portion of the kexhash buffer */
/* Sect. 8 of the transport draft, K_S + e + f + K */
#define KEXHASHBUF_MAX_INTS (1700 + 130 + 130 + 130)

#define DROPBEAR_MAX_SOCKS 2 /* IPv4, IPv6 are all we'll get for now. Revisit
								in a few years time.... */

#define DROPBEAR_MAX_CLI_PASS 1024

#define DROPBEAR_MAX_CLI_INTERACT_PROMPTS 80 /* The number of prompts we'll 
												accept for keyb-interactive
												auth */

#if defined(DROPBEAR_AES256) || defined(DROPBEAR_AES128)
#define DROPBEAR_AES
#endif

#if defined(DROPBEAR_TWOFISH256) || defined(DROPBEAR_TWOFISH128)
#define DROPBEAR_TWOFISH
#endif

#ifndef ENABLE_X11FWD
#define DISABLE_X11FWD
#endif

#ifndef ENABLE_AGENTFWD
#define DISABLE_AGENTFWD
#endif

#if defined(ENABLE_CLI_REMOTETCPFWD) || defined(ENABLE_CLI_LOCALTCPFWD)
#define ENABLE_CLI_ANYTCPFWD 
#endif

#if defined(ENABLE_CLI_LOCALTCPFWD) || defined(ENABLE_SVR_REMOTETCPFWD)
#define DROPBEAR_TCP_ACCEPT
#endif

#if defined(ENABLE_CLI_REMOTETCPFWD) || defined(ENABLE_CLI_LOCALTCPFWD) || \
	defined(ENABLE_SVR_REMOTETCPFWD) || defined(ENABLE_SVR_LOCALTCPFWD) || \
	defined(ENABLE_AGENTFWD) || defined(ENABLE_X11FWD)
#define USING_LISTENERS
#endif

#if defined(ENABLE_CLI_NETCAT) && defined(ENABLE_CLI_PROXYCMD)
#define ENABLE_CLI_MULTIHOP
#endif

#if defined(DROPBEAR_CLIENT) || defined(ENABLE_SVR_PUBKEY_AUTH)
#define DROPBEAR_KEY_LINES /* ie we're using authorized_keys or known_hosts */
#endif

#if defined(ENABLE_SVR_PASSWORD_AUTH) && defined(ENABLE_SVR_PAM_AUTH)
#error "You can't turn on PASSWORD and PAM auth both at once. Fix it in options.h"
#endif

#if defined(DROPBEAR_RANDOM_DEV) && defined(DROPBEAR_PRNGD_SOCKET)
#error "You can't turn on DROPBEAR_PRNGD_SOCKET and DROPBEAR_RANDOM_DEV at once"
#endif

#if !defined(DROPBEAR_RANDOM_DEV) && !defined(DROPBEAR_PRNGD_SOCKET)
#error "You must choose one of DROPBEAR_PRNGD_SOCKET or DROPBEAR_RANDOM_DEV in options.h"
#endif

/* We use dropbear_client and dropbear_server as shortcuts to avoid redundant
 * code, if we're just compiling as client or server */
#if defined(DROPBEAR_SERVER) && defined(DROPBEAR_CLIENT)

#define IS_DROPBEAR_SERVER (ses.isserver == 1)
#define IS_DROPBEAR_CLIENT (ses.isserver == 0)

#elif defined(DROPBEAR_SERVER)

#define IS_DROPBEAR_SERVER 1
#define IS_DROPBEAR_CLIENT 0

#elif defined(DROPBEAR_CLIENT)

#define IS_DROPBEAR_SERVER 0
#define IS_DROPBEAR_CLIENT 1

#else
#error You must compiled with either DROPBEAR_CLIENT or DROPBEAR_SERVER selected
#endif
