/*
  Copyright (c) 2004 The Regents of the University of Michigan.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the University nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _RPC_GSSD_H_
#define _RPC_GSSD_H_

#include <sys/types.h>
#include <sys/queue.h>
#include <gssapi/gssapi.h>

#define MAX_FILE_NAMELEN	32
#define FD_ALLOC_BLOCK		256
#ifndef GSSD_PIPEFS_DIR
#define GSSD_PIPEFS_DIR		"/var/lib/nfs/rpc_pipefs"
#endif
#define INFO			"info"
#define KRB5			"krb5"
#define DNOTIFY_SIGNAL		(SIGRTMIN + 3)

#define GSSD_DEFAULT_CRED_DIR			"/tmp"
#define GSSD_DEFAULT_CRED_PREFIX		"krb5cc_"
#define GSSD_DEFAULT_MACHINE_CRED_SUFFIX	"machine"
#define GSSD_DEFAULT_KEYTAB_FILE		"/etc/krb5.keytab"
#define GSSD_SERVICE_NAME			"nfs"
#define GSSD_SERVICE_NAME_LEN			3
#define GSSD_MAX_CCACHE_SEARCH			16

/*
 * The gss mechanisms that we can handle
 */
enum {AUTHTYPE_KRB5, AUTHTYPE_SPKM3, AUTHTYPE_LIPKEY};



extern char			pipefs_dir[PATH_MAX];
extern char			pipefs_nfsdir[PATH_MAX];
extern char			keytabfile[PATH_MAX];
extern char			*ccachesearch[];
extern int			use_memcache;
extern int			root_uses_machine_creds;
extern unsigned int 		context_timeout;
extern char			*preferred_realm;

TAILQ_HEAD(clnt_list_head, clnt_info) clnt_list;

struct clnt_info {
	TAILQ_ENTRY(clnt_info)	list;
	char			*dirname;
	int			dir_fd;
	char			*servicename;
	char			*servername;
	int			prog;
	int			vers;
	char			*protocol;
	int			krb5_fd;
	int			krb5_poll_index;
	int			spkm3_fd;
	int			spkm3_poll_index;
	struct sockaddr_storage addr;
};

void init_client_list(void);
int update_client_list(void);
void handle_krb5_upcall(struct clnt_info *clp);
void handle_spkm3_upcall(struct clnt_info *clp);
int gssd_acquire_cred(char *server_name);
void gssd_run(void);


#endif /* _RPC_GSSD_H_ */
