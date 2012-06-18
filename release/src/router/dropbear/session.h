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

#ifndef _SESSION_H_
#define _SESSION_H_

#include "includes.h"
#include "options.h"
#include "buffer.h"
#include "signkey.h"
#include "kex.h"
#include "auth.h"
#include "channel.h"
#include "queue.h"
#include "listener.h"
#include "packet.h"
#include "tcpfwd.h"
#include "chansession.h"

extern int sessinitdone; /* Is set to 0 somewhere */
extern int exitflag;

void common_session_init(int sock_in, int sock_out, char* remotehost);
void session_loop(void(*loophandler)());
void common_session_cleanup();
void session_identification();
void send_msg_ignore();

const char* get_user_shell();
void fill_passwd(const char* username);

/* Server */
void svr_session(int sock, int childpipe, char *remotehost, char *addrstring);
void svr_dropbear_exit(int exitcode, const char* format, va_list param);
void svr_dropbear_log(int priority, const char* format, va_list param);

/* Client */
void cli_session(int sock_in, int sock_out, char *remotehost);
void cli_session_cleanup();
void cleantext(unsigned char* dirtytext);

struct key_context {

	const struct dropbear_cipher *recv_algo_crypt; /* NULL for none */
	const struct dropbear_cipher *trans_algo_crypt; /* NULL for none */
	const struct dropbear_cipher_mode *recv_crypt_mode;
	const struct dropbear_cipher_mode *trans_crypt_mode;
	const struct dropbear_hash *recv_algo_mac; /* NULL for none */
	const struct dropbear_hash *trans_algo_mac; /* NULL for none */
	char algo_kex;
	char algo_hostkey;

	char recv_algo_comp; /* compression */
	char trans_algo_comp;
	int allow_compress; /* whether compression has started (useful in 
							zlib@openssh.com delayed compression case) */
#ifndef DISABLE_ZLIB
	z_streamp recv_zstream;
	z_streamp trans_zstream;
#endif

	/* actual keys */
	union {
		symmetric_CBC cbc;
#ifdef DROPBEAR_ENABLE_CTR_MODE
		symmetric_CTR ctr;
#endif
	} recv_cipher_state;
	union {
		symmetric_CBC cbc;
#ifdef DROPBEAR_ENABLE_CTR_MODE
		symmetric_CTR ctr;
#endif
	} trans_cipher_state;
	unsigned char recvmackey[MAX_MAC_KEY];
	unsigned char transmackey[MAX_MAC_KEY];

};

struct packetlist;
struct packetlist {
	struct packetlist *next;
	buffer * payload;
};

struct sshsession {

	/* Is it a client or server? */
	unsigned char isserver;

	time_t connect_time; /* time the connection was established
							(cleared after auth once we're not
							respecting AUTH_TIMEOUT any more) */

	int sock_in;
	int sock_out;

	unsigned char *remotehost; /* the peer hostname */

	unsigned char *remoteident;

	int maxfd; /* the maximum file descriptor to check with select() */


	/* Packet buffers/values etc */
	buffer *writepayload; /* Unencrypted payload to write - this is used
							 throughout the code, as handlers fill out this
							 buffer with the packet to send. */
	struct Queue writequeue; /* A queue of encrypted packets to send */
	buffer *readbuf; /* Encrypted */
	buffer *decryptreadbuf; /* Post-decryption */
	buffer *payload; /* Post-decompression, the actual SSH packet */
	unsigned int transseq, recvseq; /* Sequence IDs */

	/* Packet-handling flags */
	const packettype * packettypes; /* Packet handler mappings for this
										session, see process-packet.c */

	unsigned dataallowed : 1; /* whether we can send data packets or we are in
								 the middle of a KEX or something */

	unsigned char requirenext; /* byte indicating what packet we require next, 
								or 0x00 for any */

	unsigned char ignorenext; /* whether to ignore the next packet,
								 used for kex_follows stuff */

	unsigned char lastpacket; /* What the last received packet type was */
	
	int signal_pipe[2]; /* stores endpoints of a self-pipe used for
						   race-free signal handling */
						
	time_t last_trx_packet_time; /* time of the last packet transmission, for
							keepalive purposes */

	time_t last_packet_time; /* time of the last packet transmission or receive, for
								idle timeout purposes */


	/* KEX/encryption related */
	struct KEXState kexstate;
	struct key_context *keys;
	struct key_context *newkeys;
	unsigned char *session_id; /* this is the hash from the first kex */
	/* The below are used temorarily during kex, are freed after use */
	mp_int * dh_K; /* SSH_MSG_KEXDH_REPLY and sending SSH_MSH_NEWKEYS */
	unsigned char hash[SHA1_HASH_SIZE]; /* the hash*/
	buffer* kexhashbuf; /* session hash buffer calculated from various packets*/
	buffer* transkexinit; /* the kexinit packet we send should be kept so we
							 can add it to the hash when generating keys */
							
	/* a list of queued replies that should be sent after a KEX has
	   concluded (ie, while dataallowed was unset)*/
	struct packetlist *reply_queue_head, *reply_queue_tail;

	algo_type*(*buf_match_algo)(buffer*buf, algo_type localalgos[],
			int *goodguess); /* The function to use to choose which algorithm
								to use from the ones presented by the remote
								side. Is specific to the client/server mode,
								hence the function-pointer callback.*/

	void(*remoteclosed)(); /* A callback to handle closure of the
									  remote connection */


	struct AuthState authstate; /* Common amongst client and server, since most
								   struct elements are common */

	/* Channel related */
	struct Channel ** channels; /* these pointers may be null */
	unsigned int chansize; /* the number of Channel*s allocated for channels */
	unsigned int chancount; /* the number of Channel*s in use */
	const struct ChanType **chantypes; /* The valid channel types */

	
	/* TCP forwarding - where manage listeners */
	struct Listener ** listeners;
	unsigned int listensize;

	/* Whether to allow binding to privileged ports (<1024). This doesn't
	 * really belong here, but nowhere else fits nicely */
	int allowprivport;

};

struct serversession {

	/* Server specific options */
	int childpipe; /* kept open until we successfully authenticate */
	/* userauth */

	struct ChildPid * childpids; /* array of mappings childpid<->channel */
	unsigned int childpidsize;

	/* Used to avoid a race in the exit returncode handling - see
	 * svr-chansession.c for details */
	struct exitinfo lastexit;

	/* The numeric address they connected from, used for logging */
	char * addrstring;

};

typedef enum {
	KEX_NOTHING,
	KEXINIT_RCVD,
	KEXDH_INIT_SENT,
	KEXDONE
} cli_kex_state;

typedef enum {
	STATE_NOTHING,
	SERVICE_AUTH_REQ_SENT,
	SERVICE_AUTH_ACCEPT_RCVD,
	SERVICE_CONN_REQ_SENT,
	SERVICE_CONN_ACCEPT_RCVD,
	USERAUTH_REQ_SENT,
	USERAUTH_FAIL_RCVD,
	USERAUTH_SUCCESS_RCVD,
	SESSION_RUNNING
} cli_state;

struct clientsession {

	mp_int *dh_e, *dh_x; /* Used during KEX */
	cli_kex_state kex_state; /* Used for progressing KEX */
	cli_state state; /* Used to progress auth/channelsession etc */
	unsigned donefirstkex : 1; /* Set when we set sentnewkeys, never reset */

	int tty_raw_mode; /* Whether we're in raw mode (and have to clean up) */
	struct termios saved_tio;
	int stdincopy;
	int stdinflags;
	int stdoutcopy;
	int stdoutflags;
	int stderrcopy;
	int stderrflags;

	int winchange; /* Set to 1 when a windowchange signal happens */

	int lastauthtype; /* either AUTH_TYPE_PUBKEY or AUTH_TYPE_PASSWORD,
						 for the last type of auth we tried */
#ifdef ENABLE_CLI_INTERACT_AUTH
	int auth_interact_failed; /* flag whether interactive auth can still
								 be used */
	int interact_request_received; /* flag whether we've received an 
									  info request from the server for
									  interactive auth.*/
#endif
	struct SignKeyList *lastprivkey;

	int retval; /* What the command exit status was - we emulate it */
#if 0
	TODO
	struct AgentkeyList *agentkeys; /* Keys to use for public-key auth */
#endif

};

/* Global structs storing the state */
extern struct sshsession ses;

#ifdef DROPBEAR_SERVER
extern struct serversession svr_ses;
#endif /* DROPBEAR_SERVER */

#ifdef DROPBEAR_CLIENT
extern struct clientsession cli_ses;
#endif /* DROPBEAR_CLIENT */

#endif /* _SESSION_H_ */
