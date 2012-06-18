/* 
   Unix SMB/CIFS implementation.
   Parameter loading functions
   Copyright (C) Karl Auer 1993-1998

   Largely re-written by Andrew Tridgell, September 1994

   Copyright (C) Simo Sorce 2001
   Copyright (C) Alexander Bokovoy 2002
   Copyright (C) Stefan (metze) Metzmacher 2002
   Copyright (C) Jim McDonough (jmcd@us.ibm.com)  2003.
   Copyright (C) James Myers 2003 <myersjj@samba.org>
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007

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

/*
 *  Load parameters.
 *
 *  This module provides suitable callback functions for the params
 *  module. It builds the internal table of service details which is
 *  then used by the rest of the server.
 *
 * To add a parameter:
 *
 * 1) add it to the global or service structure definition
 * 2) add it to the parm_table
 * 3) add it to the list of available functions (eg: using FN_GLOBAL_STRING())
 * 4) If it's a global then initialise it in init_globals. If a local
 *    (ie. service) parameter then initialise it in the sDefault structure
 *
 *
 * Notes:
 *   The configuration file is processed sequentially for speed. It is NOT
 *   accessed randomly as happens in 'real' Windows. For this reason, there
 *   is a fair bit of sequence-dependent code here - ie., code which assumes
 *   that certain things happen before others. In particular, the code which
 *   happens at the boundary between sections is delicately poised, so be
 *   careful!
 *
 */

#include "includes.h"
#include "version.h"
#include "dynconfig/dynconfig.h"
#include "system/time.h"
#include "system/locale.h"
#include "system/network.h" /* needed for TCP_NODELAY */
#include "smb_server/smb_server.h"
#include "libcli/raw/signing.h"
#include "../lib/util/dlinklist.h"
#include "param/param.h"
#include "param/loadparm.h"
#include "libcli/raw/libcliraw.h"
#include "rpc_server/common/common.h"
#include "lib/socket/socket.h"
#include "auth/gensec/gensec.h"

#define standard_sub_basic talloc_strdup

static bool do_parameter(const char *, const char *, void *);
static bool defaults_saved = false;

/**
 * This structure describes global (ie., server-wide) parameters.
 */
struct loadparm_global
{
	enum server_role server_role;

	const char **smb_ports;
	char *ncalrpc_dir;
	char *dos_charset;
	char *unix_charset;
	char *display_charset;
	char *szLockDir;
	char *szModulesDir;
	char *szPidDir;
	char *szSetupDir;
	char *szServerString;
	char *szAutoServices;
	char *szPasswdChat;
	char *szShareBackend;
	char *szSAM_URL;
	char *szIDMAP_URL;
	char *szSECRETS_URL;
	char *szSPOOLSS_URL;
	char *szWINS_CONFIG_URL;
	char *szWINS_URL;
	char *szPrivateDir;
	const char **szPasswordServers;
	char *szSocketOptions;
	char *szRealm;
	const char **szWINSservers;
	const char **szInterfaces;
	char *szSocketAddress;
	char *szAnnounceVersion;	/* This is initialised in init_globals */
	char *szWorkgroup;
	char *szNetbiosName;
	const char **szNetbiosAliases;
	char *szNetbiosScope;
	char *szDomainOtherSIDs;
	const char **szNameResolveOrder;
	const char **dcerpc_ep_servers;
	const char **server_services;
	char *ntptr_providor;
	char *szWinbindSeparator;
	char *szWinbinddPrivilegedSocketDirectory;
	char *szWinbinddSocketDirectory;
	char *szTemplateShell;
	char *szTemplateHomedir;
	int bWinbindSealedPipes;
	int bIdmapTrustedOnly;
	char *swat_directory;
	int tls_enabled;
	char *tls_keyfile;
	char *tls_certfile;
	char *tls_cafile;
	char *tls_crlfile;
	char *tls_dhpfile;
	char *logfile;
	char *panic_action;
	int max_mux;
	int debuglevel;
	int max_xmit;
	int pwordlevel;
	int srv_maxprotocol;
	int srv_minprotocol;
	int cli_maxprotocol;
	int cli_minprotocol;
	int security;
	int paranoid_server_security;
	int max_wins_ttl;
	int min_wins_ttl;
	int announce_as;	/* This is initialised in init_globals */
	int nbt_port;
	int dgram_port;
	int cldap_port;
	int krb5_port;
	int kpasswd_port;
	int web_port;
	char *socket_options;
	int bWINSsupport;
	int bWINSdnsProxy;
	char *szWINSHook;
	int bLocalMaster;
	int bPreferredMaster;
	int bEncryptPasswords;
	int bNullPasswords;
	int bObeyPamRestrictions;
	int bLargeReadwrite;
	int bReadRaw;
	int bWriteRaw;
	int bTimeServer;
	int bBindInterfacesOnly;
	int bNTSmbSupport;
	int bNTStatusSupport;
	int bLanmanAuth;
	int bNTLMAuth;
	int bUseSpnego;
	int server_signing;
	int client_signing;
	int bClientPlaintextAuth;
	int bClientLanManAuth;
	int bClientNTLMv2Auth;
	int client_use_spnego_principal;
	int bHostMSDfs;
	int bUnicode;
	int bUnixExtensions;
	int bDisableNetbios;
	int bRpcBigEndian;
	char *szNTPSignDSocketDirectory;
	struct parmlist_entry *param_opt;
};


/**
 * This structure describes a single service.
 */
struct loadparm_service
{
	char *szService;
	char *szPath;
	char *szCopy;
	char *szInclude;
	char *szPrintername;
	char **szHostsallow;
	char **szHostsdeny;
	char *comment;
	char *volume;
	char *fstype;
	char **ntvfs_handler;
	int iMaxPrintJobs;
	int iMaxConnections;
	int iCSCPolicy;
	int bAvailable;
	int bBrowseable;
	int bRead_only;
	int bPrint_ok;
	int bMap_system;
	int bMap_hidden;
	int bMap_archive;
	int bStrictLocking;
	int bOplocks;
	int iCreate_mask;
	int iCreate_force_mode;
	int iDir_mask;
	int iDir_force_mode;
	int *copymap;
	int bMSDfsRoot;
	int bStrictSync;
	int bCIFileSystem;
	struct parmlist_entry *param_opt;

	char dummy[3];		/* for alignment */
};


#define NUMPARAMETERS (sizeof(parm_table) / sizeof(struct parm_struct))


/* prototypes for the special type handlers */
static bool handle_include(struct loadparm_context *lp_ctx,
			   const char *pszParmValue, char **ptr);
static bool handle_copy(struct loadparm_context *lp_ctx,
			const char *pszParmValue, char **ptr);
static bool handle_debuglevel(struct loadparm_context *lp_ctx,
			      const char *pszParmValue, char **ptr);
static bool handle_logfile(struct loadparm_context *lp_ctx,
			   const char *pszParmValue, char **ptr);

static const struct enum_list enum_protocol[] = {
	{PROTOCOL_SMB2, "SMB2"},
	{PROTOCOL_NT1, "NT1"},
	{PROTOCOL_LANMAN2, "LANMAN2"},
	{PROTOCOL_LANMAN1, "LANMAN1"},
	{PROTOCOL_CORE, "CORE"},
	{PROTOCOL_COREPLUS, "COREPLUS"},
	{PROTOCOL_COREPLUS, "CORE+"},
	{-1, NULL}
};

static const struct enum_list enum_security[] = {
	{SEC_SHARE, "SHARE"},
	{SEC_USER, "USER"},
	{-1, NULL}
};

static const struct enum_list enum_announce_as[] = {
	{ANNOUNCE_AS_NT_SERVER, "NT"},
	{ANNOUNCE_AS_NT_SERVER, "NT Server"},
	{ANNOUNCE_AS_NT_WORKSTATION, "NT Workstation"},
	{ANNOUNCE_AS_WIN95, "win95"},
	{ANNOUNCE_AS_WFW, "WfW"},
	{-1, NULL}
};

static const struct enum_list enum_bool_auto[] = {
	{false, "No"},
	{false, "False"},
	{false, "0"},
	{true, "Yes"},
	{true, "True"},
	{true, "1"},
	{Auto, "Auto"},
	{-1, NULL}
};

/* Client-side offline caching policy types */
enum csc_policy {
	CSC_POLICY_MANUAL=0,
	CSC_POLICY_DOCUMENTS=1,
	CSC_POLICY_PROGRAMS=2,
	CSC_POLICY_DISABLE=3
};

static const struct enum_list enum_csc_policy[] = {
	{CSC_POLICY_MANUAL, "manual"},
	{CSC_POLICY_DOCUMENTS, "documents"},
	{CSC_POLICY_PROGRAMS, "programs"},
	{CSC_POLICY_DISABLE, "disable"},
	{-1, NULL}
};

/* SMB signing types. */
static const struct enum_list enum_smb_signing_vals[] = {
	{SMB_SIGNING_OFF, "No"},
	{SMB_SIGNING_OFF, "False"},
	{SMB_SIGNING_OFF, "0"},
	{SMB_SIGNING_OFF, "Off"},
	{SMB_SIGNING_OFF, "disabled"},
	{SMB_SIGNING_SUPPORTED, "Yes"},
	{SMB_SIGNING_SUPPORTED, "True"},
	{SMB_SIGNING_SUPPORTED, "1"},
	{SMB_SIGNING_SUPPORTED, "On"},
	{SMB_SIGNING_SUPPORTED, "enabled"},
	{SMB_SIGNING_REQUIRED, "required"},
	{SMB_SIGNING_REQUIRED, "mandatory"},
	{SMB_SIGNING_REQUIRED, "force"},
	{SMB_SIGNING_REQUIRED, "forced"},
	{SMB_SIGNING_REQUIRED, "enforced"},
	{SMB_SIGNING_AUTO, "auto"},
	{-1, NULL}
};

static const struct enum_list enum_server_role[] = {
	{ROLE_STANDALONE, "standalone"},
	{ROLE_DOMAIN_MEMBER, "member server"},
	{ROLE_DOMAIN_MEMBER, "member"},
	{ROLE_DOMAIN_CONTROLLER, "domain controller"},
	{ROLE_DOMAIN_CONTROLLER, "dc"},
	{-1, NULL}
};


#define GLOBAL_VAR(name) offsetof(struct loadparm_global, name)
#define LOCAL_VAR(name) offsetof(struct loadparm_service, name)

static struct parm_struct parm_table[] = {
	{"server role", P_ENUM, P_GLOBAL, GLOBAL_VAR(server_role), NULL, enum_server_role},

	{"dos charset", P_STRING, P_GLOBAL, GLOBAL_VAR(dos_charset), NULL, NULL},
	{"unix charset", P_STRING, P_GLOBAL, GLOBAL_VAR(unix_charset), NULL, NULL},
	{"ncalrpc dir", P_STRING, P_GLOBAL, GLOBAL_VAR(ncalrpc_dir), NULL, NULL},
	{"display charset", P_STRING, P_GLOBAL, GLOBAL_VAR(display_charset), NULL, NULL},
	{"comment", P_STRING, P_LOCAL, LOCAL_VAR(comment), NULL, NULL},
	{"path", P_STRING, P_LOCAL, LOCAL_VAR(szPath), NULL, NULL},
	{"directory", P_STRING, P_LOCAL, LOCAL_VAR(szPath), NULL, NULL},
	{"workgroup", P_USTRING, P_GLOBAL, GLOBAL_VAR(szWorkgroup), NULL, NULL},
	{"realm", P_STRING, P_GLOBAL, GLOBAL_VAR(szRealm), NULL, NULL},
	{"netbios name", P_USTRING, P_GLOBAL, GLOBAL_VAR(szNetbiosName), NULL, NULL},
	{"netbios aliases", P_LIST, P_GLOBAL, GLOBAL_VAR(szNetbiosAliases), NULL, NULL},
	{"netbios scope", P_USTRING, P_GLOBAL, GLOBAL_VAR(szNetbiosScope), NULL, NULL},
	{"server string", P_STRING, P_GLOBAL, GLOBAL_VAR(szServerString), NULL, NULL},
	{"interfaces", P_LIST, P_GLOBAL, GLOBAL_VAR(szInterfaces), NULL, NULL},
	{"bind interfaces only", P_BOOL, P_GLOBAL, GLOBAL_VAR(bBindInterfacesOnly), NULL, NULL},
	{"ntvfs handler", P_LIST, P_LOCAL, LOCAL_VAR(ntvfs_handler), NULL, NULL},
	{"ntptr providor", P_STRING, P_GLOBAL, GLOBAL_VAR(ntptr_providor), NULL, NULL},
	{"dcerpc endpoint servers", P_LIST, P_GLOBAL, GLOBAL_VAR(dcerpc_ep_servers), NULL, NULL},
	{"server services", P_LIST, P_GLOBAL, GLOBAL_VAR(server_services), NULL, NULL},

	{"security", P_ENUM, P_GLOBAL, GLOBAL_VAR(security), NULL, enum_security},
	{"encrypt passwords", P_BOOL, P_GLOBAL, GLOBAL_VAR(bEncryptPasswords), NULL, NULL},
	{"null passwords", P_BOOL, P_GLOBAL, GLOBAL_VAR(bNullPasswords), NULL, NULL},
	{"obey pam restrictions", P_BOOL, P_GLOBAL, GLOBAL_VAR(bObeyPamRestrictions), NULL, NULL},
	{"password server", P_LIST, P_GLOBAL, GLOBAL_VAR(szPasswordServers), NULL, NULL},
	{"sam database", P_STRING, P_GLOBAL, GLOBAL_VAR(szSAM_URL), NULL, NULL},
	{"idmap database", P_STRING, P_GLOBAL, GLOBAL_VAR(szIDMAP_URL), NULL, NULL},
	{"secrets database", P_STRING, P_GLOBAL, GLOBAL_VAR(szSECRETS_URL), NULL, NULL},
	{"spoolss database", P_STRING, P_GLOBAL, GLOBAL_VAR(szSPOOLSS_URL), NULL, NULL},
	{"wins config database", P_STRING, P_GLOBAL, GLOBAL_VAR(szWINS_CONFIG_URL), NULL, NULL},
	{"wins database", P_STRING, P_GLOBAL, GLOBAL_VAR(szWINS_URL), NULL, NULL},
	{"private dir", P_STRING, P_GLOBAL, GLOBAL_VAR(szPrivateDir), NULL, NULL},
	{"passwd chat", P_STRING, P_GLOBAL, GLOBAL_VAR(szPasswdChat), NULL, NULL},
	{"password level", P_INTEGER, P_GLOBAL, GLOBAL_VAR(pwordlevel), NULL, NULL},
	{"lanman auth", P_BOOL, P_GLOBAL, GLOBAL_VAR(bLanmanAuth), NULL, NULL},
	{"ntlm auth", P_BOOL, P_GLOBAL, GLOBAL_VAR(bNTLMAuth), NULL, NULL},
	{"client NTLMv2 auth", P_BOOL, P_GLOBAL, GLOBAL_VAR(bClientNTLMv2Auth), NULL, NULL},
	{"client lanman auth", P_BOOL, P_GLOBAL, GLOBAL_VAR(bClientLanManAuth), NULL, NULL},
	{"client plaintext auth", P_BOOL, P_GLOBAL, GLOBAL_VAR(bClientPlaintextAuth), NULL, NULL},
	{"client use spnego principal", P_BOOL, P_GLOBAL, GLOBAL_VAR(client_use_spnego_principal), NULL, NULL},

	{"read only", P_BOOL, P_LOCAL, LOCAL_VAR(bRead_only), NULL, NULL},

	{"create mask", P_OCTAL, P_LOCAL, LOCAL_VAR(iCreate_mask), NULL, NULL},
	{"force create mode", P_OCTAL, P_LOCAL, LOCAL_VAR(iCreate_force_mode), NULL, NULL}, 
	{"directory mask", P_OCTAL, P_LOCAL, LOCAL_VAR(iDir_mask), NULL, NULL},
	{"force directory mode", P_OCTAL, P_LOCAL, LOCAL_VAR(iDir_force_mode), NULL, NULL}, 

	{"hosts allow", P_LIST, P_LOCAL, LOCAL_VAR(szHostsallow), NULL, NULL},
	{"hosts deny", P_LIST, P_LOCAL, LOCAL_VAR(szHostsdeny), NULL, NULL},

	{"log level", P_INTEGER, P_GLOBAL, GLOBAL_VAR(debuglevel), handle_debuglevel, NULL},
	{"debuglevel", P_INTEGER, P_GLOBAL, GLOBAL_VAR(debuglevel), handle_debuglevel, NULL},
	{"log file", P_STRING, P_GLOBAL, GLOBAL_VAR(logfile), handle_logfile, NULL},

	{"smb ports", P_LIST, P_GLOBAL, GLOBAL_VAR(smb_ports), NULL, NULL},
	{"nbt port", P_INTEGER, P_GLOBAL, GLOBAL_VAR(nbt_port), NULL, NULL},
	{"dgram port", P_INTEGER, P_GLOBAL, GLOBAL_VAR(dgram_port), NULL, NULL},
	{"cldap port", P_INTEGER, P_GLOBAL, GLOBAL_VAR(cldap_port), NULL, NULL},
	{"krb5 port", P_INTEGER, P_GLOBAL, GLOBAL_VAR(krb5_port), NULL, NULL},
	{"kpasswd port", P_INTEGER, P_GLOBAL, GLOBAL_VAR(kpasswd_port), NULL, NULL},
	{"web port", P_INTEGER, P_GLOBAL, GLOBAL_VAR(web_port), NULL, NULL},
	{"tls enabled", P_BOOL, P_GLOBAL, GLOBAL_VAR(tls_enabled), NULL, NULL},
	{"tls keyfile", P_STRING, P_GLOBAL, GLOBAL_VAR(tls_keyfile), NULL, NULL},
	{"tls certfile", P_STRING, P_GLOBAL, GLOBAL_VAR(tls_certfile), NULL, NULL},
	{"tls cafile", P_STRING, P_GLOBAL, GLOBAL_VAR(tls_cafile), NULL, NULL},
	{"tls crlfile", P_STRING, P_GLOBAL, GLOBAL_VAR(tls_crlfile), NULL, NULL},
	{"tls dh params file", P_STRING, P_GLOBAL, GLOBAL_VAR(tls_dhpfile), NULL, NULL},
	{"swat directory", P_STRING, P_GLOBAL, GLOBAL_VAR(swat_directory), NULL, NULL},
	{"large readwrite", P_BOOL, P_GLOBAL, GLOBAL_VAR(bLargeReadwrite), NULL, NULL},
	{"server max protocol", P_ENUM, P_GLOBAL, GLOBAL_VAR(srv_maxprotocol), NULL, enum_protocol},
	{"server min protocol", P_ENUM, P_GLOBAL, GLOBAL_VAR(srv_minprotocol), NULL, enum_protocol},
	{"client max protocol", P_ENUM, P_GLOBAL, GLOBAL_VAR(cli_maxprotocol), NULL, enum_protocol},
	{"client min protocol", P_ENUM, P_GLOBAL, GLOBAL_VAR(cli_minprotocol), NULL, enum_protocol},
	{"unicode", P_BOOL, P_GLOBAL, GLOBAL_VAR(bUnicode), NULL, NULL},
	{"read raw", P_BOOL, P_GLOBAL, GLOBAL_VAR(bReadRaw), NULL, NULL},
	{"write raw", P_BOOL, P_GLOBAL, GLOBAL_VAR(bWriteRaw), NULL, NULL},
	{"disable netbios", P_BOOL, P_GLOBAL, GLOBAL_VAR(bDisableNetbios), NULL, NULL},

	{"nt status support", P_BOOL, P_GLOBAL, GLOBAL_VAR(bNTStatusSupport), NULL, NULL},

	{"announce version", P_STRING, P_GLOBAL, GLOBAL_VAR(szAnnounceVersion), NULL, NULL},
	{"announce as", P_ENUM, P_GLOBAL, GLOBAL_VAR(announce_as), NULL, enum_announce_as},
	{"max mux", P_INTEGER, P_GLOBAL, GLOBAL_VAR(max_mux), NULL, NULL},
	{"max xmit", P_BYTES, P_GLOBAL, GLOBAL_VAR(max_xmit), NULL, NULL},

	{"name resolve order", P_LIST, P_GLOBAL, GLOBAL_VAR(szNameResolveOrder), NULL, NULL},
	{"max wins ttl", P_INTEGER, P_GLOBAL, GLOBAL_VAR(max_wins_ttl), NULL, NULL},
	{"min wins ttl", P_INTEGER, P_GLOBAL, GLOBAL_VAR(min_wins_ttl), NULL, NULL},
	{"time server", P_BOOL, P_GLOBAL, GLOBAL_VAR(bTimeServer), NULL, NULL},
	{"unix extensions", P_BOOL, P_GLOBAL, GLOBAL_VAR(bUnixExtensions), NULL, NULL},
	{"use spnego", P_BOOL, P_GLOBAL, GLOBAL_VAR(bUseSpnego), NULL, NULL},
	{"server signing", P_ENUM, P_GLOBAL, GLOBAL_VAR(server_signing), NULL, enum_smb_signing_vals}, 
	{"client signing", P_ENUM, P_GLOBAL, GLOBAL_VAR(client_signing), NULL, enum_smb_signing_vals}, 
	{"rpc big endian", P_BOOL, P_GLOBAL, GLOBAL_VAR(bRpcBigEndian), NULL, NULL},

	{"max connections", P_INTEGER, P_LOCAL, LOCAL_VAR(iMaxConnections), NULL, NULL},
	{"paranoid server security", P_BOOL, P_GLOBAL, GLOBAL_VAR(paranoid_server_security), NULL, NULL},
	{"socket options", P_STRING, P_GLOBAL, GLOBAL_VAR(socket_options), NULL, NULL},

	{"strict sync", P_BOOL, P_LOCAL, LOCAL_VAR(bStrictSync), NULL, NULL},
	{"case insensitive filesystem", P_BOOL, P_LOCAL, LOCAL_VAR(bCIFileSystem), NULL, NULL}, 

	{"max print jobs", P_INTEGER, P_LOCAL, LOCAL_VAR(iMaxPrintJobs), NULL, NULL},
	{"printable", P_BOOL, P_LOCAL, LOCAL_VAR(bPrint_ok), NULL, NULL},
	{"print ok", P_BOOL, P_LOCAL, LOCAL_VAR(bPrint_ok), NULL, NULL},

	{"printer name", P_STRING, P_LOCAL, LOCAL_VAR(szPrintername), NULL, NULL},
	{"printer", P_STRING, P_LOCAL, LOCAL_VAR(szPrintername), NULL, NULL},

	{"map system", P_BOOL, P_LOCAL, LOCAL_VAR(bMap_system), NULL, NULL},
	{"map hidden", P_BOOL, P_LOCAL, LOCAL_VAR(bMap_hidden), NULL, NULL},
	{"map archive", P_BOOL, P_LOCAL, LOCAL_VAR(bMap_archive), NULL, NULL},

	{"preferred master", P_ENUM, P_GLOBAL, GLOBAL_VAR(bPreferredMaster), NULL, enum_bool_auto},
	{"prefered master", P_ENUM, P_GLOBAL, GLOBAL_VAR(bPreferredMaster), NULL, enum_bool_auto},
	{"local master", P_BOOL, P_GLOBAL, GLOBAL_VAR(bLocalMaster), NULL, NULL},
	{"browseable", P_BOOL, P_LOCAL, LOCAL_VAR(bBrowseable), NULL, NULL},
	{"browsable", P_BOOL, P_LOCAL, LOCAL_VAR(bBrowseable), NULL, NULL},

	{"wins server", P_LIST, P_GLOBAL, GLOBAL_VAR(szWINSservers), NULL, NULL},
	{"wins support", P_BOOL, P_GLOBAL, GLOBAL_VAR(bWINSsupport), NULL, NULL},
	{"dns proxy", P_BOOL, P_GLOBAL, GLOBAL_VAR(bWINSdnsProxy), NULL, NULL},
	{"wins hook", P_STRING, P_GLOBAL, GLOBAL_VAR(szWINSHook), NULL, NULL}, 

	{"csc policy", P_ENUM, P_LOCAL, LOCAL_VAR(iCSCPolicy), NULL, enum_csc_policy},

	{"strict locking", P_BOOL, P_LOCAL, LOCAL_VAR(bStrictLocking), NULL, NULL},
	{"oplocks", P_BOOL, P_LOCAL, LOCAL_VAR(bOplocks), NULL, NULL},

	{"share backend", P_STRING, P_GLOBAL, GLOBAL_VAR(szShareBackend), NULL, NULL},
	{"preload", P_STRING, P_GLOBAL, GLOBAL_VAR(szAutoServices), NULL, NULL},
	{"auto services", P_STRING, P_GLOBAL, GLOBAL_VAR(szAutoServices), NULL, NULL},
	{"lock dir", P_STRING, P_GLOBAL, GLOBAL_VAR(szLockDir), NULL, NULL}, 
	{"lock directory", P_STRING, P_GLOBAL, GLOBAL_VAR(szLockDir), NULL, NULL},
	{"modules dir", P_STRING, P_GLOBAL, GLOBAL_VAR(szModulesDir), NULL, NULL},
	{"pid directory", P_STRING, P_GLOBAL, GLOBAL_VAR(szPidDir), NULL, NULL}, 
	{"setup directory", P_STRING, P_GLOBAL, GLOBAL_VAR(szSetupDir), NULL, NULL},

	{"socket address", P_STRING, P_GLOBAL, GLOBAL_VAR(szSocketAddress), NULL, NULL},
	{"copy", P_STRING, P_LOCAL, LOCAL_VAR(szCopy), handle_copy, NULL},
	{"include", P_STRING, P_LOCAL, LOCAL_VAR(szInclude), handle_include, NULL},

	{"available", P_BOOL, P_LOCAL, LOCAL_VAR(bAvailable), NULL, NULL},
	{"volume", P_STRING, P_LOCAL, LOCAL_VAR(volume), NULL, NULL },
	{"fstype", P_STRING, P_LOCAL, LOCAL_VAR(fstype), NULL, NULL},

	{"panic action", P_STRING, P_GLOBAL, GLOBAL_VAR(panic_action), NULL, NULL},

	{"msdfs root", P_BOOL, P_LOCAL, LOCAL_VAR(bMSDfsRoot), NULL, NULL},
	{"host msdfs", P_BOOL, P_GLOBAL, GLOBAL_VAR(bHostMSDfs), NULL, NULL},
	{"winbind separator", P_STRING, P_GLOBAL, GLOBAL_VAR(szWinbindSeparator), NULL, NULL },
	{"winbindd socket directory", P_STRING, P_GLOBAL, GLOBAL_VAR(szWinbinddSocketDirectory), NULL, NULL },
	{"winbindd privileged socket directory", P_STRING, P_GLOBAL, GLOBAL_VAR(szWinbinddPrivilegedSocketDirectory), NULL, NULL },
	{"winbind sealed pipes", P_BOOL, P_GLOBAL, GLOBAL_VAR(bWinbindSealedPipes), NULL, NULL },
	{"template shell", P_STRING, P_GLOBAL, GLOBAL_VAR(szTemplateShell), NULL, NULL },
	{"template homedir", P_STRING, P_GLOBAL, GLOBAL_VAR(szTemplateHomedir), NULL, NULL },
	{"idmap trusted only", P_BOOL, P_GLOBAL, GLOBAL_VAR(bIdmapTrustedOnly), NULL, NULL},

	{"ntp signd socket directory", P_STRING, P_GLOBAL, GLOBAL_VAR(szNTPSignDSocketDirectory), NULL, NULL },

	{NULL, P_BOOL, P_NONE, 0, NULL, NULL}
};


/* local variables */
struct loadparm_context {
	const char *szConfigFile;
	struct loadparm_global *globals;
	struct loadparm_service **services;
	struct loadparm_service *sDefault;
	int iNumServices;
	struct loadparm_service *currentService;
	bool bInGlobalSection;
	struct file_lists {
		struct file_lists *next;
		char *name;
		char *subfname;
		time_t modtime;
	} *file_lists;
	unsigned int flags[NUMPARAMETERS];
	struct smb_iconv_convenience *iconv_convenience;
};


struct loadparm_service *lp_default_service(struct loadparm_context *lp_ctx)
{
	return lp_ctx->sDefault;
}

/*
  return the parameter table
*/
struct parm_struct *lp_parm_table(void)
{
	return parm_table;
}

/**
 * Convenience routine to grab string parameters into temporary memory
 * and run standard_sub_basic on them.
 *
 * The buffers can be written to by
 * callers without affecting the source string.
 */

static const char *lp_string(const char *s)
{
#if 0  /* until REWRITE done to make thread-safe */
	size_t len = s ? strlen(s) : 0;
	char *ret;
#endif

	/* The follow debug is useful for tracking down memory problems
	   especially if you have an inner loop that is calling a lp_*()
	   function that returns a string.  Perhaps this debug should be
	   present all the time? */

#if 0
	DEBUG(10, ("lp_string(%s)\n", s));
#endif

#if 0  /* until REWRITE done to make thread-safe */
	if (!lp_talloc)
		lp_talloc = talloc_init("lp_talloc");

	ret = talloc_array(lp_talloc, char, len + 100);	/* leave room for substitution */

	if (!ret)
		return NULL;

	if (!s)
		*ret = 0;
	else
		strlcpy(ret, s, len);

	if (trim_string(ret, "\"", "\"")) {
		if (strchr(ret,'"') != NULL)
			strlcpy(ret, s, len);
	}

	standard_sub_basic(ret,len+100);
	return (ret);
#endif
	return s;
}

/*
   In this section all the functions that are used to access the
   parameters from the rest of the program are defined
*/

#define FN_GLOBAL_STRING(fn_name,var_name) \
 const char *fn_name(struct loadparm_context *lp_ctx) {if (lp_ctx == NULL) return NULL; return lp_ctx->globals->var_name ? lp_string(lp_ctx->globals->var_name) : "";}
#define FN_GLOBAL_CONST_STRING(fn_name,var_name) \
 const char *fn_name(struct loadparm_context *lp_ctx) {if (lp_ctx == NULL) return NULL; return lp_ctx->globals->var_name ? lp_ctx->globals->var_name : "";}
#define FN_GLOBAL_LIST(fn_name,var_name) \
 const char **fn_name(struct loadparm_context *lp_ctx) {if (lp_ctx == NULL) return NULL; return lp_ctx->globals->var_name;}
#define FN_GLOBAL_BOOL(fn_name,var_name) \
 bool fn_name(struct loadparm_context *lp_ctx) {if (lp_ctx == NULL) return false; return lp_ctx->globals->var_name;}
#if 0 /* unused */
#define FN_GLOBAL_CHAR(fn_name,ptr) \
 char fn_name(void) {return(*(char *)(ptr));}
#endif
#define FN_GLOBAL_INTEGER(fn_name,var_name) \
 int fn_name(struct loadparm_context *lp_ctx) {return lp_ctx->globals->var_name;}

#define FN_LOCAL_STRING(fn_name,val) \
 const char *fn_name(struct loadparm_service *service, struct loadparm_service *sDefault) {return(lp_string((const char *)((service != NULL && service->val != NULL) ? service->val : sDefault->val)));}
#define FN_LOCAL_LIST(fn_name,val) \
 const char **fn_name(struct loadparm_service *service, struct loadparm_service *sDefault) {return(const char **)(service != NULL && service->val != NULL? service->val : sDefault->val);}
#define FN_LOCAL_BOOL(fn_name,val) \
 bool fn_name(struct loadparm_service *service, struct loadparm_service *sDefault) {return((service != NULL)? service->val : sDefault->val);}
#define FN_LOCAL_INTEGER(fn_name,val) \
 int fn_name(struct loadparm_service *service, struct loadparm_service *sDefault) {return((service != NULL)? service->val : sDefault->val);}

_PUBLIC_ FN_GLOBAL_INTEGER(lp_server_role, server_role)
_PUBLIC_ FN_GLOBAL_LIST(lp_smb_ports, smb_ports)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_nbt_port, nbt_port)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_dgram_port, dgram_port)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_cldap_port, cldap_port)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_krb5_port, krb5_port)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_kpasswd_port, kpasswd_port)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_web_port, web_port)
_PUBLIC_ FN_GLOBAL_STRING(lp_swat_directory, swat_directory)
_PUBLIC_ FN_GLOBAL_BOOL(lp_tls_enabled, tls_enabled)
_PUBLIC_ FN_GLOBAL_STRING(lp_share_backend, szShareBackend)
_PUBLIC_ FN_GLOBAL_STRING(lp_sam_url, szSAM_URL)
_PUBLIC_ FN_GLOBAL_STRING(lp_idmap_url, szIDMAP_URL)
_PUBLIC_ FN_GLOBAL_STRING(lp_secrets_url, szSECRETS_URL)
_PUBLIC_ FN_GLOBAL_STRING(lp_spoolss_url, szSPOOLSS_URL)
_PUBLIC_ FN_GLOBAL_STRING(lp_wins_config_url, szWINS_CONFIG_URL)
_PUBLIC_ FN_GLOBAL_STRING(lp_wins_url, szWINS_URL)
_PUBLIC_ FN_GLOBAL_CONST_STRING(lp_winbind_separator, szWinbindSeparator)
_PUBLIC_ FN_GLOBAL_CONST_STRING(lp_winbindd_socket_directory, szWinbinddSocketDirectory)
_PUBLIC_ FN_GLOBAL_CONST_STRING(lp_winbindd_privileged_socket_directory, szWinbinddPrivilegedSocketDirectory)
_PUBLIC_ FN_GLOBAL_CONST_STRING(lp_template_shell, szTemplateShell)
_PUBLIC_ FN_GLOBAL_CONST_STRING(lp_template_homedir, szTemplateHomedir)
_PUBLIC_ FN_GLOBAL_BOOL(lp_winbind_sealed_pipes, bWinbindSealedPipes)
_PUBLIC_ FN_GLOBAL_BOOL(lp_idmap_trusted_only, bIdmapTrustedOnly)
_PUBLIC_ FN_GLOBAL_STRING(lp_private_dir, szPrivateDir)
_PUBLIC_ FN_GLOBAL_STRING(lp_serverstring, szServerString)
_PUBLIC_ FN_GLOBAL_STRING(lp_lockdir, szLockDir)
_PUBLIC_ FN_GLOBAL_STRING(lp_modulesdir, szModulesDir)
_PUBLIC_ FN_GLOBAL_STRING(lp_setupdir, szSetupDir)
_PUBLIC_ FN_GLOBAL_STRING(lp_ncalrpc_dir, ncalrpc_dir)
_PUBLIC_ FN_GLOBAL_STRING(lp_dos_charset, dos_charset)
_PUBLIC_ FN_GLOBAL_STRING(lp_unix_charset, unix_charset)
_PUBLIC_ FN_GLOBAL_STRING(lp_display_charset, display_charset)
_PUBLIC_ FN_GLOBAL_STRING(lp_piddir, szPidDir)
_PUBLIC_ FN_GLOBAL_LIST(lp_dcerpc_endpoint_servers, dcerpc_ep_servers)
_PUBLIC_ FN_GLOBAL_LIST(lp_server_services, server_services)
_PUBLIC_ FN_GLOBAL_STRING(lp_ntptr_providor, ntptr_providor)
_PUBLIC_ FN_GLOBAL_STRING(lp_auto_services, szAutoServices)
_PUBLIC_ FN_GLOBAL_STRING(lp_passwd_chat, szPasswdChat)
_PUBLIC_ FN_GLOBAL_LIST(lp_passwordserver, szPasswordServers)
_PUBLIC_ FN_GLOBAL_LIST(lp_name_resolve_order, szNameResolveOrder)
_PUBLIC_ FN_GLOBAL_STRING(lp_realm, szRealm)
_PUBLIC_ FN_GLOBAL_STRING(lp_socket_options, socket_options)
_PUBLIC_ FN_GLOBAL_STRING(lp_workgroup, szWorkgroup)
_PUBLIC_ FN_GLOBAL_STRING(lp_netbios_name, szNetbiosName)
_PUBLIC_ FN_GLOBAL_STRING(lp_netbios_scope, szNetbiosScope)
_PUBLIC_ FN_GLOBAL_LIST(lp_wins_server_list, szWINSservers)
_PUBLIC_ FN_GLOBAL_LIST(lp_interfaces, szInterfaces)
_PUBLIC_ FN_GLOBAL_STRING(lp_socket_address, szSocketAddress)
_PUBLIC_ FN_GLOBAL_LIST(lp_netbios_aliases, szNetbiosAliases)

_PUBLIC_ FN_GLOBAL_BOOL(lp_disable_netbios, bDisableNetbios)
_PUBLIC_ FN_GLOBAL_BOOL(lp_wins_support, bWINSsupport)
_PUBLIC_ FN_GLOBAL_BOOL(lp_wins_dns_proxy, bWINSdnsProxy)
_PUBLIC_ FN_GLOBAL_STRING(lp_wins_hook, szWINSHook)
_PUBLIC_ FN_GLOBAL_BOOL(lp_local_master, bLocalMaster)
_PUBLIC_ FN_GLOBAL_BOOL(lp_readraw, bReadRaw)
_PUBLIC_ FN_GLOBAL_BOOL(lp_large_readwrite, bLargeReadwrite)
_PUBLIC_ FN_GLOBAL_BOOL(lp_writeraw, bWriteRaw)
_PUBLIC_ FN_GLOBAL_BOOL(lp_null_passwords, bNullPasswords)
_PUBLIC_ FN_GLOBAL_BOOL(lp_obey_pam_restrictions, bObeyPamRestrictions)
_PUBLIC_ FN_GLOBAL_BOOL(lp_encrypted_passwords, bEncryptPasswords)
_PUBLIC_ FN_GLOBAL_BOOL(lp_time_server, bTimeServer)
_PUBLIC_ FN_GLOBAL_BOOL(lp_bind_interfaces_only, bBindInterfacesOnly)
_PUBLIC_ FN_GLOBAL_BOOL(lp_unicode, bUnicode)
_PUBLIC_ FN_GLOBAL_BOOL(lp_nt_status_support, bNTStatusSupport)
_PUBLIC_ FN_GLOBAL_BOOL(lp_lanman_auth, bLanmanAuth)
_PUBLIC_ FN_GLOBAL_BOOL(lp_ntlm_auth, bNTLMAuth)
_PUBLIC_ FN_GLOBAL_BOOL(lp_client_plaintext_auth, bClientPlaintextAuth)
_PUBLIC_ FN_GLOBAL_BOOL(lp_client_lanman_auth, bClientLanManAuth)
_PUBLIC_ FN_GLOBAL_BOOL(lp_client_ntlmv2_auth, bClientNTLMv2Auth)
_PUBLIC_ FN_GLOBAL_BOOL(lp_client_use_spnego_principal, client_use_spnego_principal)
_PUBLIC_ FN_GLOBAL_BOOL(lp_host_msdfs, bHostMSDfs)
_PUBLIC_ FN_GLOBAL_BOOL(lp_unix_extensions, bUnixExtensions)
_PUBLIC_ FN_GLOBAL_BOOL(lp_use_spnego, bUseSpnego)
_PUBLIC_ FN_GLOBAL_BOOL(lp_rpc_big_endian, bRpcBigEndian)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_max_wins_ttl, max_wins_ttl)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_min_wins_ttl, min_wins_ttl)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_maxmux, max_mux)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_max_xmit, max_xmit)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_passwordlevel, pwordlevel)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_srv_maxprotocol, srv_maxprotocol)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_srv_minprotocol, srv_minprotocol)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_cli_maxprotocol, cli_maxprotocol)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_cli_minprotocol, cli_minprotocol)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_security, security)
_PUBLIC_ FN_GLOBAL_BOOL(lp_paranoid_server_security, paranoid_server_security)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_announce_as, announce_as)
const char *lp_servicename(const struct loadparm_service *service)
{
	return lp_string((const char *)service->szService);
}

_PUBLIC_ FN_LOCAL_STRING(lp_pathname, szPath)
static FN_LOCAL_STRING(_lp_printername, szPrintername)
_PUBLIC_ FN_LOCAL_LIST(lp_hostsallow, szHostsallow)
_PUBLIC_ FN_LOCAL_LIST(lp_hostsdeny, szHostsdeny)
_PUBLIC_ FN_LOCAL_STRING(lp_comment, comment)
_PUBLIC_ FN_LOCAL_STRING(lp_fstype, fstype)
static FN_LOCAL_STRING(lp_volume, volume)
_PUBLIC_ FN_LOCAL_LIST(lp_ntvfs_handler, ntvfs_handler)
_PUBLIC_ FN_LOCAL_BOOL(lp_msdfs_root, bMSDfsRoot)
_PUBLIC_ FN_LOCAL_BOOL(lp_browseable, bBrowseable)
_PUBLIC_ FN_LOCAL_BOOL(lp_readonly, bRead_only)
_PUBLIC_ FN_LOCAL_BOOL(lp_print_ok, bPrint_ok)
_PUBLIC_ FN_LOCAL_BOOL(lp_map_hidden, bMap_hidden)
_PUBLIC_ FN_LOCAL_BOOL(lp_map_archive, bMap_archive)
_PUBLIC_ FN_LOCAL_BOOL(lp_strict_locking, bStrictLocking)
_PUBLIC_ FN_LOCAL_BOOL(lp_oplocks, bOplocks)
_PUBLIC_ FN_LOCAL_BOOL(lp_strict_sync, bStrictSync)
_PUBLIC_ FN_LOCAL_BOOL(lp_ci_filesystem, bCIFileSystem)
_PUBLIC_ FN_LOCAL_BOOL(lp_map_system, bMap_system)
_PUBLIC_ FN_LOCAL_INTEGER(lp_max_connections, iMaxConnections)
_PUBLIC_ FN_LOCAL_INTEGER(lp_csc_policy, iCSCPolicy)
_PUBLIC_ FN_LOCAL_INTEGER(lp_create_mask, iCreate_mask)
_PUBLIC_ FN_LOCAL_INTEGER(lp_force_create_mode, iCreate_force_mode)
_PUBLIC_ FN_LOCAL_INTEGER(lp_dir_mask, iDir_mask)
_PUBLIC_ FN_LOCAL_INTEGER(lp_force_dir_mode, iDir_force_mode)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_server_signing, server_signing)
_PUBLIC_ FN_GLOBAL_INTEGER(lp_client_signing, client_signing)

_PUBLIC_ FN_GLOBAL_CONST_STRING(lp_ntp_signd_socket_directory, szNTPSignDSocketDirectory)

/* local prototypes */
static int map_parameter(const char *pszParmName);
static struct loadparm_service *getservicebyname(struct loadparm_context *lp_ctx, 
					const char *pszServiceName);
static void copy_service(struct loadparm_service *pserviceDest,
			 struct loadparm_service *pserviceSource,
			 int *pcopymapDest);
static bool service_ok(struct loadparm_service *service);
static bool do_section(const char *pszSectionName, void *);
static void init_copymap(struct loadparm_service *pservice);

/* This is a helper function for parametrical options support. */
/* It returns a pointer to parametrical option value if it exists or NULL otherwise */
/* Actual parametrical functions are quite simple */
const char *lp_get_parametric(struct loadparm_context *lp_ctx,
			      struct loadparm_service *service,
			      const char *type, const char *option)
{
	char *vfskey;
        struct parmlist_entry *data;

	if (lp_ctx == NULL)
		return NULL;

	data = (service == NULL ? lp_ctx->globals->param_opt : service->param_opt);

	asprintf(&vfskey, "%s:%s", type, option);
	strlower(vfskey);

	while (data) {
		if (strcmp(data->key, vfskey) == 0) {
			free(vfskey);
			return data->value;
		}
		data = data->next;
	}

	if (service != NULL) {
		/* Try to fetch the same option but from globals */
		/* but only if we are not already working with globals */
		for (data = lp_ctx->globals->param_opt; data;
		     data = data->next) {
			if (strcmp(data->key, vfskey) == 0) {
				free(vfskey);
				return data->value;
			}
		}
	}

	free(vfskey);

	return NULL;
}


/**
 * convenience routine to return int parameters.
 */
static int lp_int(const char *s)
{

	if (!s) {
		DEBUG(0,("lp_int(%s): is called with NULL!\n",s));
		return -1;
	}

	return strtol(s, NULL, 0);
}

/**
 * convenience routine to return unsigned long parameters.
 */
static int lp_ulong(const char *s)
{

	if (!s) {
		DEBUG(0,("lp_int(%s): is called with NULL!\n",s));
		return -1;
	}

	return strtoul(s, NULL, 0);
}

/**
 * convenience routine to return unsigned long parameters.
 */
static double lp_double(const char *s)
{

	if (!s) {
		DEBUG(0,("lp_double(%s): is called with NULL!\n",s));
		return -1;
	}

	return strtod(s, NULL);
}

/**
 * convenience routine to return boolean parameters.
 */
static bool lp_bool(const char *s)
{
	bool ret = false;

	if (!s) {
		DEBUG(0,("lp_bool(%s): is called with NULL!\n",s));
		return false;
	}

	if (!set_boolean(s, &ret)) {
		DEBUG(0,("lp_bool(%s): value is not boolean!\n",s));
		return false;
	}

	return ret;
}


/**
 * Return parametric option from a given service. Type is a part of option before ':'
 * Parametric option has following syntax: 'Type: option = value'
 * Returned value is allocated in 'lp_talloc' context
 */

const char *lp_parm_string(struct loadparm_context *lp_ctx,
			   struct loadparm_service *service, const char *type,
			   const char *option)
{
	const char *value = lp_get_parametric(lp_ctx, service, type, option);

	if (value)
		return lp_string(value);

	return NULL;
}

/**
 * Return parametric option from a given service. Type is a part of option before ':'
 * Parametric option has following syntax: 'Type: option = value'
 * Returned value is allocated in 'lp_talloc' context
 */

const char **lp_parm_string_list(TALLOC_CTX *mem_ctx,
				 struct loadparm_context *lp_ctx,
				 struct loadparm_service *service,
				 const char *type,
				 const char *option, const char *separator)
{
	const char *value = lp_get_parametric(lp_ctx, service, type, option);

	if (value != NULL)
		return (const char **)str_list_make(mem_ctx, value, separator);

	return NULL;
}

/**
 * Return parametric option from a given service. Type is a part of option before ':'
 * Parametric option has following syntax: 'Type: option = value'
 */

int lp_parm_int(struct loadparm_context *lp_ctx,
		struct loadparm_service *service, const char *type,
		const char *option, int default_v)
{
	const char *value = lp_get_parametric(lp_ctx, service, type, option);

	if (value)
		return lp_int(value);

	return default_v;
}

/**
 * Return parametric option from a given service. Type is a part of
 * option before ':'.
 * Parametric option has following syntax: 'Type: option = value'.
 */

int lp_parm_bytes(struct loadparm_context *lp_ctx,
		  struct loadparm_service *service, const char *type,
		  const char *option, int default_v)
{
	uint64_t bval;

	const char *value = lp_get_parametric(lp_ctx, service, type, option);

	if (value && conv_str_size(value, &bval)) {
		if (bval <= INT_MAX) {
			return (int)bval;
		}
	}

	return default_v;
}

/**
 * Return parametric option from a given service.
 * Type is a part of option before ':'
 * Parametric option has following syntax: 'Type: option = value'
 */
unsigned long lp_parm_ulong(struct loadparm_context *lp_ctx,
			    struct loadparm_service *service, const char *type,
			    const char *option, unsigned long default_v)
{
	const char *value = lp_get_parametric(lp_ctx, service, type, option);

	if (value)
		return lp_ulong(value);

	return default_v;
}


double lp_parm_double(struct loadparm_context *lp_ctx,
		      struct loadparm_service *service, const char *type,
		      const char *option, double default_v)
{
	const char *value = lp_get_parametric(lp_ctx, service, type, option);

	if (value != NULL)
		return lp_double(value);

	return default_v;
}

/**
 * Return parametric option from a given service. Type is a part of option before ':'
 * Parametric option has following syntax: 'Type: option = value'
 */

bool lp_parm_bool(struct loadparm_context *lp_ctx,
		  struct loadparm_service *service, const char *type,
		  const char *option, bool default_v)
{
	const char *value = lp_get_parametric(lp_ctx, service, type, option);

	if (value != NULL)
		return lp_bool(value);

	return default_v;
}


/**
 * Initialise a service to the defaults.
 */

static struct loadparm_service *init_service(TALLOC_CTX *mem_ctx, struct loadparm_service *sDefault)
{
	struct loadparm_service *pservice =
		talloc_zero(mem_ctx, struct loadparm_service);
	copy_service(pservice, sDefault, NULL);
	return pservice;
}

/**
 * Set a string value, deallocating any existing space, and allocing the space
 * for the string
 */
static bool string_set(TALLOC_CTX *mem_ctx, char **dest, const char *src)
{
	talloc_free(*dest);

	if (src == NULL)
		src = "";

	*dest = talloc_strdup(mem_ctx, src);
	if ((*dest) == NULL) {
		DEBUG(0,("Out of memory in string_init\n"));
		return false;
	}

	return true;
}



/**
 * Add a new service to the services array initialising it with the given
 * service.
 */

struct loadparm_service *lp_add_service(struct loadparm_context *lp_ctx,
				     const struct loadparm_service *pservice,
				     const char *name)
{
	int i;
	struct loadparm_service tservice;
	int num_to_alloc = lp_ctx->iNumServices + 1;
	struct parmlist_entry *data, *pdata;

	tservice = *pservice;

	/* it might already exist */
	if (name) {
		struct loadparm_service *service = getservicebyname(lp_ctx,
								    name);
		if (service != NULL) {
			/* Clean all parametric options for service */
			/* They will be added during parsing again */
			data = service->param_opt;
			while (data) {
				pdata = data->next;
				talloc_free(data);
				data = pdata;
			}
			service->param_opt = NULL;
			return service;
		}
	}

	/* find an invalid one */
	for (i = 0; i < lp_ctx->iNumServices; i++)
		if (lp_ctx->services[i] == NULL)
			break;

	/* if not, then create one */
	if (i == lp_ctx->iNumServices) {
		struct loadparm_service **tsp;

		tsp = talloc_realloc(lp_ctx, lp_ctx->services, struct loadparm_service *, num_to_alloc);

		if (!tsp) {
			DEBUG(0,("lp_add_service: failed to enlarge services!\n"));
			return NULL;
		} else {
			lp_ctx->services = tsp;
			lp_ctx->services[lp_ctx->iNumServices] = NULL;
		}

		lp_ctx->iNumServices++;
	}

	lp_ctx->services[i] = init_service(lp_ctx->services, lp_ctx->sDefault);
	if (lp_ctx->services[i] == NULL) {
		DEBUG(0,("lp_add_service: out of memory!\n"));
		return NULL;
	}
	copy_service(lp_ctx->services[i], &tservice, NULL);
	if (name != NULL)
		string_set(lp_ctx->services[i], &lp_ctx->services[i]->szService, name);
	return lp_ctx->services[i];
}

/**
 * Add a new home service, with the specified home directory, defaults coming
 * from service ifrom.
 */

bool lp_add_home(struct loadparm_context *lp_ctx,
		 const char *pszHomename,
		 struct loadparm_service *default_service,
		 const char *user, const char *pszHomedir)
{
	struct loadparm_service *service;

	service = lp_add_service(lp_ctx, default_service, pszHomename);

	if (service == NULL)
		return false;

	if (!(*(default_service->szPath))
	    || strequal(default_service->szPath, lp_ctx->sDefault->szPath)) {
		service->szPath = talloc_strdup(service, pszHomedir);
	} else {
		service->szPath = string_sub_talloc(service, lp_pathname(default_service, lp_ctx->sDefault), "%H", pszHomedir); 
	}

	if (!(*(service->comment))) {
		service->comment = talloc_asprintf(service, "Home directory of %s", user);
	}
	service->bAvailable = default_service->bAvailable;
	service->bBrowseable = default_service->bBrowseable;

	DEBUG(3, ("adding home's share [%s] for user '%s' at '%s'\n",
		  pszHomename, user, service->szPath));

	return true;
}

/**
 * Add the IPC service.
 */

static bool lp_add_hidden(struct loadparm_context *lp_ctx, const char *name,
			  const char *fstype)
{
	struct loadparm_service *service = lp_add_service(lp_ctx, lp_ctx->sDefault, name);

	if (service == NULL)
		return false;

	string_set(service, &service->szPath, tmpdir());

	service->comment = talloc_asprintf(service, "%s Service (%s)",
				fstype, lp_ctx->globals->szServerString);
	string_set(service, &service->fstype, fstype);
	service->iMaxConnections = -1;
	service->bAvailable = true;
	service->bRead_only = true;
	service->bPrint_ok = false;
	service->bBrowseable = false;

	if (strcasecmp(fstype, "IPC") == 0) {
		lp_do_service_parameter(lp_ctx, service, "ntvfs handler",
					"default");
	}

	DEBUG(3, ("adding hidden service %s\n", name));

	return true;
}

/**
 * Add a new printer service, with defaults coming from service iFrom.
 */

bool lp_add_printer(struct loadparm_context *lp_ctx,
		    const char *pszPrintername,
		    struct loadparm_service *default_service)
{
	const char *comment = "From Printcap";
	struct loadparm_service *service;
	service = lp_add_service(lp_ctx, default_service, pszPrintername);

	if (service == NULL)
		return false;

	/* note that we do NOT default the availability flag to True - */
	/* we take it from the default service passed. This allows all */
	/* dynamic printers to be disabled by disabling the [printers] */
	/* entry (if/when the 'available' keyword is implemented!).    */

	/* the printer name is set to the service name. */
	string_set(service, &service->szPrintername, pszPrintername);
	string_set(service, &service->comment, comment);
	service->bBrowseable = default_service->bBrowseable;
	/* Printers cannot be read_only. */
	service->bRead_only = false;
	/* Printer services must be printable. */
	service->bPrint_ok = true;

	DEBUG(3, ("adding printer service %s\n", pszPrintername));

	return true;
}

/**
 * Map a parameter's string representation to something we can use.
 * Returns False if the parameter string is not recognised, else TRUE.
 */

static int map_parameter(const char *pszParmName)
{
	int iIndex;

	if (*pszParmName == '-')
		return -1;

	for (iIndex = 0; parm_table[iIndex].label; iIndex++)
		if (strwicmp(parm_table[iIndex].label, pszParmName) == 0)
			return iIndex;

	/* Warn only if it isn't parametric option */
	if (strchr(pszParmName, ':') == NULL)
		DEBUG(0, ("Unknown parameter encountered: \"%s\"\n", pszParmName));
	/* We do return 'fail' for parametric options as well because they are
	   stored in different storage
	 */
	return -1;
}


/**
  return the parameter structure for a parameter
*/
struct parm_struct *lp_parm_struct(const char *name)
{
	int parmnum = map_parameter(name);
	if (parmnum == -1) return NULL;
	return &parm_table[parmnum];
}

/**
  return the parameter pointer for a parameter
*/
void *lp_parm_ptr(struct loadparm_context *lp_ctx,
		  struct loadparm_service *service, struct parm_struct *parm)
{
	if (service == NULL) {
		if (parm->pclass == P_LOCAL)
			return ((char *)lp_ctx->sDefault)+parm->offset;
		else if (parm->pclass == P_GLOBAL)
			return ((char *)lp_ctx->globals)+parm->offset;
		else return NULL;
	} else {
		return ((char *)service) + parm->offset;
	}
}

/**
 * Find a service by name. Otherwise works like get_service.
 */

static struct loadparm_service *getservicebyname(struct loadparm_context *lp_ctx, 
					const char *pszServiceName)
{
	int iService;

	for (iService = lp_ctx->iNumServices - 1; iService >= 0; iService--)
		if (lp_ctx->services[iService] != NULL &&
		    strwicmp(lp_ctx->services[iService]->szService, pszServiceName) == 0) {
			return lp_ctx->services[iService];
		}

	return NULL;
}

/**
 * Copy a service structure to another.
 * If pcopymapDest is NULL then copy all fields
 */

static void copy_service(struct loadparm_service *pserviceDest,
			 struct loadparm_service *pserviceSource,
			 int *pcopymapDest)
{
	int i;
	bool bcopyall = (pcopymapDest == NULL);
	struct parmlist_entry *data, *pdata, *paramo;
	bool not_added;

	for (i = 0; parm_table[i].label; i++)
		if (parm_table[i].offset != -1 && parm_table[i].pclass == P_LOCAL &&
		    (bcopyall || pcopymapDest[i])) {
			void *src_ptr =
				((char *)pserviceSource) + parm_table[i].offset;
			void *dest_ptr =
				((char *)pserviceDest) + parm_table[i].offset;

			switch (parm_table[i].type) {
				case P_BOOL:
					*(int *)dest_ptr = *(int *)src_ptr;
					break;

				case P_INTEGER:
				case P_OCTAL:
				case P_ENUM:
					*(int *)dest_ptr = *(int *)src_ptr;
					break;

				case P_STRING:
					string_set(pserviceDest,
						   (char **)dest_ptr,
						   *(char **)src_ptr);
					break;

				case P_USTRING:
					string_set(pserviceDest,
						   (char **)dest_ptr,
						   *(char **)src_ptr);
					strupper(*(char **)dest_ptr);
					break;
				case P_LIST:
					*(const char ***)dest_ptr = (const char **)str_list_copy(pserviceDest, 
										  *(const char ***)src_ptr);
					break;
				default:
					break;
			}
		}

	if (bcopyall) {
		init_copymap(pserviceDest);
		if (pserviceSource->copymap)
			memcpy((void *)pserviceDest->copymap,
			       (void *)pserviceSource->copymap,
			       sizeof(int) * NUMPARAMETERS);
	}

	data = pserviceSource->param_opt;
	while (data) {
		not_added = true;
		pdata = pserviceDest->param_opt;
		/* Traverse destination */
		while (pdata) {
			/* If we already have same option, override it */
			if (strcmp(pdata->key, data->key) == 0) {
				talloc_free(pdata->value);
				pdata->value = talloc_reference(pdata,
							     data->value);
				not_added = false;
				break;
			}
			pdata = pdata->next;
		}
		if (not_added) {
			paramo = talloc(pserviceDest, struct parmlist_entry);
			if (paramo == NULL)
				smb_panic("OOM");
			paramo->key = talloc_reference(paramo, data->key);
			paramo->value = talloc_reference(paramo, data->value);
			DLIST_ADD(pserviceDest->param_opt, paramo);
		}
		data = data->next;
	}
}

/**
 * Check a service for consistency. Return False if the service is in any way
 * incomplete or faulty, else True.
 */
static bool service_ok(struct loadparm_service *service)
{
	bool bRetval;

	bRetval = true;
	if (service->szService[0] == '\0') {
		DEBUG(0, ("The following message indicates an internal error:\n"));
		DEBUG(0, ("No service name in service entry.\n"));
		bRetval = false;
	}

	/* The [printers] entry MUST be printable. I'm all for flexibility, but */
	/* I can't see why you'd want a non-printable printer service...        */
	if (strwicmp(service->szService, PRINTERS_NAME) == 0) {
		if (!service->bPrint_ok) {
			DEBUG(0, ("WARNING: [%s] service MUST be printable!\n",
			       service->szService));
			service->bPrint_ok = true;
		}
		/* [printers] service must also be non-browsable. */
		if (service->bBrowseable)
			service->bBrowseable = false;
	}

	/* If a service is flagged unavailable, log the fact at level 0. */
	if (!service->bAvailable)
		DEBUG(1, ("NOTE: Service %s is flagged unavailable.\n",
			  service->szService));

	return bRetval;
}


/*******************************************************************
 Keep a linked list of all config files so we know when one has changed
 it's date and needs to be reloaded.
********************************************************************/

static void add_to_file_list(struct loadparm_context *lp_ctx,
			     const char *fname, const char *subfname)
{
	struct file_lists *f = lp_ctx->file_lists;

	while (f) {
		if (f->name && !strcmp(f->name, fname))
			break;
		f = f->next;
	}

	if (!f) {
		f = talloc(lp_ctx, struct file_lists);
		if (!f)
			return;
		f->next = lp_ctx->file_lists;
		f->name = talloc_strdup(f, fname);
		if (!f->name) {
			talloc_free(f);
			return;
		}
		f->subfname = talloc_strdup(f, subfname);
		if (!f->subfname) {
			talloc_free(f);
			return;
		}
		lp_ctx->file_lists = f;
		f->modtime = file_modtime(subfname);
	} else {
		time_t t = file_modtime(subfname);
		if (t)
			f->modtime = t;
	}
}

/*******************************************************************
 Check if a config file has changed date.
********************************************************************/
bool lp_file_list_changed(struct loadparm_context *lp_ctx)
{
	struct file_lists *f;
	DEBUG(6, ("lp_file_list_changed()\n"));

	for (f = lp_ctx->file_lists; f != NULL; f = f->next) {
		char *n2;
		time_t mod_time;

		n2 = standard_sub_basic(lp_ctx, f->name);

		DEBUGADD(6, ("file %s -> %s  last mod_time: %s\n",
			     f->name, n2, ctime(&f->modtime)));

		mod_time = file_modtime(n2);

		if (mod_time && ((f->modtime != mod_time) || (f->subfname == NULL) || (strcmp(n2, f->subfname) != 0))) {
			DEBUGADD(6, ("file %s modified: %s\n", n2,
				  ctime(&mod_time)));
			f->modtime = mod_time;
			talloc_free(f->subfname);
			f->subfname = talloc_strdup(f, n2);
			return true;
		}
	}
	return false;
}

/***************************************************************************
 Handle the include operation.
***************************************************************************/

static bool handle_include(struct loadparm_context *lp_ctx,
			   const char *pszParmValue, char **ptr)
{
	char *fname = standard_sub_basic(lp_ctx, pszParmValue);

	add_to_file_list(lp_ctx, pszParmValue, fname);

	string_set(lp_ctx, ptr, fname);

	if (file_exist(fname))
		return pm_process(fname, do_section, do_parameter, lp_ctx);

	DEBUG(2, ("Can't find include file %s\n", fname));

	return false;
}

/***************************************************************************
 Handle the interpretation of the copy parameter.
***************************************************************************/

static bool handle_copy(struct loadparm_context *lp_ctx,
			const char *pszParmValue, char **ptr)
{
	bool bRetval;
	struct loadparm_service *serviceTemp;

	string_set(lp_ctx, ptr, pszParmValue);

	bRetval = false;

	DEBUG(3, ("Copying service from service %s\n", pszParmValue));

	if ((serviceTemp = getservicebyname(lp_ctx, pszParmValue)) != NULL) {
		if (serviceTemp == lp_ctx->currentService) {
			DEBUG(0, ("Can't copy service %s - unable to copy self!\n", pszParmValue));
		} else {
			copy_service(lp_ctx->currentService,
				     serviceTemp,
				     lp_ctx->currentService->copymap);
			bRetval = true;
		}
	} else {
		DEBUG(0, ("Unable to copy service - source not found: %s\n",
			  pszParmValue));
		bRetval = false;
	}

	return bRetval;
}

static bool handle_debuglevel(struct loadparm_context *lp_ctx,
			const char *pszParmValue, char **ptr)
{
	DEBUGLEVEL = atoi(pszParmValue);

	return true;
}

static bool handle_logfile(struct loadparm_context *lp_ctx,
			const char *pszParmValue, char **ptr)
{
	logfile = pszParmValue;
	return true;
}

/***************************************************************************
 Initialise a copymap.
***************************************************************************/

static void init_copymap(struct loadparm_service *pservice)
{
	int i;
	talloc_free(pservice->copymap);
	pservice->copymap = talloc_array(pservice, int, NUMPARAMETERS);
	if (pservice->copymap == NULL) {
		DEBUG(0,
		      ("Couldn't allocate copymap!! (size %d)\n",
		       (int)NUMPARAMETERS));
		return;
	}
	for (i = 0; i < NUMPARAMETERS; i++)
		pservice->copymap[i] = true;
}

/**
 * Process a parametric option
 */
static bool lp_do_parameter_parametric(struct loadparm_context *lp_ctx,
				       struct loadparm_service *service,
				       const char *pszParmName,
				       const char *pszParmValue, int flags)
{
	struct parmlist_entry *paramo, *data;
	char *name;
	TALLOC_CTX *mem_ctx;

	while (isspace((unsigned char)*pszParmName)) {
		pszParmName++;
	}

	name = strdup(pszParmName);
	if (!name) return false;

	strlower(name);

	if (service == NULL) {
		data = lp_ctx->globals->param_opt;
		mem_ctx = lp_ctx->globals;
	} else {
		data = service->param_opt;
		mem_ctx = service;
	}

	/* Traverse destination */
	for (paramo=data; paramo; paramo=paramo->next) {
		/* If we already have the option set, override it unless
		   it was a command line option and the new one isn't */
		if (strcmp(paramo->key, name) == 0) {
			if ((paramo->priority & FLAG_CMDLINE) &&
			    !(flags & FLAG_CMDLINE)) {
				return true;
			}

			talloc_free(paramo->value);
			paramo->value = talloc_strdup(paramo, pszParmValue);
			paramo->priority = flags;
			free(name);
			return true;
		}
	}

	paramo = talloc(mem_ctx, struct parmlist_entry);
	if (!paramo)
		smb_panic("OOM");
	paramo->key = talloc_strdup(paramo, name);
	paramo->value = talloc_strdup(paramo, pszParmValue);
	paramo->priority = flags;
	if (service == NULL) {
		DLIST_ADD(lp_ctx->globals->param_opt, paramo);
	} else {
		DLIST_ADD(service->param_opt, paramo);
	}

	free(name);

	return true;
}

static bool set_variable(TALLOC_CTX *mem_ctx, int parmnum, void *parm_ptr,
			 const char *pszParmName, const char *pszParmValue,
			 struct loadparm_context *lp_ctx)
{
	int i;
	/* if it is a special case then go ahead */
	if (parm_table[parmnum].special) {
		parm_table[parmnum].special(lp_ctx, pszParmValue,
					    (char **)parm_ptr);
		return true;
	}

	/* now switch on the type of variable it is */
	switch (parm_table[parmnum].type)
	{
		case P_BOOL: {
			bool b;
			if (!set_boolean(pszParmValue, &b)) {
				DEBUG(0,("lp_do_parameter(%s): value is not boolean!\n", pszParmValue));
				return false;
			}
			*(int *)parm_ptr = b;
			}
			break;

		case P_INTEGER:
			*(int *)parm_ptr = atoi(pszParmValue);
			break;

		case P_OCTAL:
			*(int *)parm_ptr = strtol(pszParmValue, NULL, 8);
			break;

		case P_BYTES:
		{
			uint64_t val;
			if (conv_str_size(pszParmValue, &val)) {
				if (val <= INT_MAX) {
					*(int *)parm_ptr = (int)val;
					break;
				}
			}

			DEBUG(0,("lp_do_parameter(%s): value is not "
			    "a valid size specifier!\n", pszParmValue));
			return false;
		}

		case P_LIST:
			*(const char ***)parm_ptr = (const char **)str_list_make(mem_ctx,
								  pszParmValue, NULL);
			break;

		case P_STRING:
			string_set(mem_ctx, (char **)parm_ptr, pszParmValue);
			break;

		case P_USTRING:
			string_set(mem_ctx, (char **)parm_ptr, pszParmValue);
			strupper(*(char **)parm_ptr);
			break;

		case P_ENUM:
			for (i = 0; parm_table[parmnum].enum_list[i].name; i++) {
				if (strequal
				    (pszParmValue,
				     parm_table[parmnum].enum_list[i].name)) {
					*(int *)parm_ptr =
						parm_table[parmnum].
						enum_list[i].value;
					break;
				}
			}
			if (!parm_table[parmnum].enum_list[i].name) {
				DEBUG(0,("Unknown enumerated value '%s' for '%s'\n", 
					 pszParmValue, pszParmName));
				return false;
			}
			break;
	}

	if (lp_ctx->flags[parmnum] & FLAG_DEFAULT) {
		lp_ctx->flags[parmnum] &= ~FLAG_DEFAULT;
		/* we have to also unset FLAG_DEFAULT on aliases */
		for (i=parmnum-1;i>=0 && parm_table[i].offset == parm_table[parmnum].offset;i--) {
			lp_ctx->flags[i] &= ~FLAG_DEFAULT;
		}
		for (i=parmnum+1;i<NUMPARAMETERS && parm_table[i].offset == parm_table[parmnum].offset;i++) {
			lp_ctx->flags[i] &= ~FLAG_DEFAULT;
		}
	}
	return true;
}


bool lp_do_global_parameter(struct loadparm_context *lp_ctx,
			    const char *pszParmName, const char *pszParmValue)
{
	int parmnum = map_parameter(pszParmName);
	void *parm_ptr;

	if (parmnum < 0) {
		if (strchr(pszParmName, ':')) {
			return lp_do_parameter_parametric(lp_ctx, NULL, pszParmName, pszParmValue, 0);
		}
		DEBUG(0, ("Ignoring unknown parameter \"%s\"\n", pszParmName));
		return true;
	}

	/* if the flag has been set on the command line, then don't allow override,
	   but don't report an error */
	if (lp_ctx->flags[parmnum] & FLAG_CMDLINE) {
		return true;
	}

	parm_ptr = lp_parm_ptr(lp_ctx, NULL, &parm_table[parmnum]);

	return set_variable(lp_ctx, parmnum, parm_ptr,
			    pszParmName, pszParmValue, lp_ctx);
}

bool lp_do_service_parameter(struct loadparm_context *lp_ctx,
			     struct loadparm_service *service,
			     const char *pszParmName, const char *pszParmValue)
{
	void *parm_ptr;
	int i;
	int parmnum = map_parameter(pszParmName);

	if (parmnum < 0) {
		if (strchr(pszParmName, ':')) {
			return lp_do_parameter_parametric(lp_ctx, service, pszParmName, pszParmValue, 0);
		}
		DEBUG(0, ("Ignoring unknown parameter \"%s\"\n", pszParmName));
		return true;
	}

	/* if the flag has been set on the command line, then don't allow override,
	   but don't report an error */
	if (lp_ctx->flags[parmnum] & FLAG_CMDLINE) {
		return true;
	}

	if (parm_table[parmnum].pclass == P_GLOBAL) {
		DEBUG(0,
		      ("Global parameter %s found in service section!\n",
		       pszParmName));
		return true;
	}
	parm_ptr = ((char *)service) + parm_table[parmnum].offset;

	if (!service->copymap)
		init_copymap(service);

	/* this handles the aliases - set the copymap for other
	 * entries with the same data pointer */
	for (i = 0; parm_table[i].label; i++)
		if (parm_table[i].offset == parm_table[parmnum].offset &&
		    parm_table[i].pclass == parm_table[parmnum].pclass)
			service->copymap[i] = false;

	return set_variable(service, parmnum, parm_ptr, pszParmName,
			    pszParmValue, lp_ctx);
}

/**
 * Process a parameter.
 */

static bool do_parameter(const char *pszParmName, const char *pszParmValue,
			 void *userdata)
{
	struct loadparm_context *lp_ctx = (struct loadparm_context *)userdata;

	if (lp_ctx->bInGlobalSection)
		return lp_do_global_parameter(lp_ctx, pszParmName,
					      pszParmValue);
	else
		return lp_do_service_parameter(lp_ctx, lp_ctx->currentService,
					       pszParmName, pszParmValue);
}

/*
  variable argument do parameter
*/
bool lp_do_global_parameter_var(struct loadparm_context *lp_ctx, const char *pszParmName, const char *fmt, ...) PRINTF_ATTRIBUTE(3, 4);
bool lp_do_global_parameter_var(struct loadparm_context *lp_ctx,
				const char *pszParmName, const char *fmt, ...)
{
	char *s;
	bool ret;
	va_list ap;

	va_start(ap, fmt);
	s = talloc_vasprintf(NULL, fmt, ap);
	va_end(ap);
	ret = lp_do_global_parameter(lp_ctx, pszParmName, s);
	talloc_free(s);
	return ret;
}


/*
  set a parameter from the commandline - this is called from command line parameter
  parsing code. It sets the parameter then marks the parameter as unable to be modified
  by smb.conf processing
*/
bool lp_set_cmdline(struct loadparm_context *lp_ctx, const char *pszParmName,
		    const char *pszParmValue)
{
	int parmnum = map_parameter(pszParmName);
	int i;

	while (isspace((unsigned char)*pszParmValue)) pszParmValue++;


	if (parmnum < 0 && strchr(pszParmName, ':')) {
		/* set a parametric option */
		return lp_do_parameter_parametric(lp_ctx, NULL, pszParmName,
						  pszParmValue, FLAG_CMDLINE);
	}

	if (parmnum < 0) {
		DEBUG(0,("Unknown option '%s'\n", pszParmName));
		return false;
	}

	/* reset the CMDLINE flag in case this has been called before */
	lp_ctx->flags[parmnum] &= ~FLAG_CMDLINE;

	if (!lp_do_global_parameter(lp_ctx, pszParmName, pszParmValue)) {
		return false;
	}

	lp_ctx->flags[parmnum] |= FLAG_CMDLINE;

	/* we have to also set FLAG_CMDLINE on aliases */
	for (i=parmnum-1;i>=0 && parm_table[i].offset == parm_table[parmnum].offset;i--) {
		lp_ctx->flags[i] |= FLAG_CMDLINE;
	}
	for (i=parmnum+1;i<NUMPARAMETERS && parm_table[i].offset == parm_table[parmnum].offset;i++) {
		lp_ctx->flags[i] |= FLAG_CMDLINE;
	}

	return true;
}

/*
  set a option from the commandline in 'a=b' format. Use to support --option
*/
bool lp_set_option(struct loadparm_context *lp_ctx, const char *option)
{
	char *p, *s;
	bool ret;

	s = strdup(option);
	if (!s) {
		return false;
	}

	p = strchr(s, '=');
	if (!p) {
		free(s);
		return false;
	}

	*p = 0;

	ret = lp_set_cmdline(lp_ctx, s, p+1);
	free(s);
	return ret;
}


#define BOOLSTR(b) ((b) ? "Yes" : "No")

/**
 * Print a parameter of the specified type.
 */

static void print_parameter(struct parm_struct *p, void *ptr, FILE * f)
{
	int i;
	switch (p->type)
	{
		case P_ENUM:
			for (i = 0; p->enum_list[i].name; i++) {
				if (*(int *)ptr == p->enum_list[i].value) {
					fprintf(f, "%s",
						p->enum_list[i].name);
					break;
				}
			}
			break;

		case P_BOOL:
			fprintf(f, "%s", BOOLSTR((bool)*(int *)ptr));
			break;

		case P_INTEGER:
		case P_BYTES:
			fprintf(f, "%d", *(int *)ptr);
			break;

		case P_OCTAL:
			fprintf(f, "0%o", *(int *)ptr);
			break;

		case P_LIST:
			if ((char ***)ptr && *(char ***)ptr) {
				char **list = *(char ***)ptr;

				for (; *list; list++)
					fprintf(f, "%s%s", *list,
						((*(list+1))?", ":""));
			}
			break;

		case P_STRING:
		case P_USTRING:
			if (*(char **)ptr) {
				fprintf(f, "%s", *(char **)ptr);
			}
			break;
	}
}

/**
 * Check if two parameters are equal.
 */

static bool equal_parameter(parm_type type, void *ptr1, void *ptr2)
{
	switch (type) {
		case P_BOOL:
			return (*((int *)ptr1) == *((int *)ptr2));

		case P_INTEGER:
		case P_OCTAL:
		case P_BYTES:
		case P_ENUM:
			return (*((int *)ptr1) == *((int *)ptr2));

		case P_LIST:
			return str_list_equal((const char **)(*(char ***)ptr1),
					      (const char **)(*(char ***)ptr2));

		case P_STRING:
		case P_USTRING:
		{
			char *p1 = *(char **)ptr1, *p2 = *(char **)ptr2;
			if (p1 && !*p1)
				p1 = NULL;
			if (p2 && !*p2)
				p2 = NULL;
			return (p1 == p2 || strequal(p1, p2));
		}
	}
	return false;
}

/**
 * Process a new section (service).
 *
 * At this stage all sections are services.
 * Later we'll have special sections that permit server parameters to be set.
 * Returns True on success, False on failure.
 */

static bool do_section(const char *pszSectionName, void *userdata)
{
	struct loadparm_context *lp_ctx = (struct loadparm_context *)userdata;
	bool bRetval;
	bool isglobal = ((strwicmp(pszSectionName, GLOBAL_NAME) == 0) ||
			 (strwicmp(pszSectionName, GLOBAL_NAME2) == 0));
	bRetval = false;

	/* if we've just struck a global section, note the fact. */
	lp_ctx->bInGlobalSection = isglobal;

	/* check for multiple global sections */
	if (lp_ctx->bInGlobalSection) {
		DEBUG(3, ("Processing section \"[%s]\"\n", pszSectionName));
		return true;
	}

	/* if we have a current service, tidy it up before moving on */
	bRetval = true;

	if (lp_ctx->currentService != NULL)
		bRetval = service_ok(lp_ctx->currentService);

	/* if all is still well, move to the next record in the services array */
	if (bRetval) {
		/* We put this here to avoid an odd message order if messages are */
		/* issued by the post-processing of a previous section. */
		DEBUG(2, ("Processing section \"[%s]\"\n", pszSectionName));

		if ((lp_ctx->currentService = lp_add_service(lp_ctx, lp_ctx->sDefault,
							     pszSectionName))
		    == NULL) {
			DEBUG(0, ("Failed to add a new service\n"));
			return false;
		}
	}

	return bRetval;
}


/**
 * Determine if a particular base parameter is currently set to the default value.
 */

static bool is_default(struct loadparm_service *sDefault, int i)
{
	void *def_ptr = ((char *)sDefault) + parm_table[i].offset;
	if (!defaults_saved)
		return false;
	switch (parm_table[i].type) {
		case P_LIST:
			return str_list_equal((const char **)parm_table[i].def.lvalue, 
					      (const char **)def_ptr);
		case P_STRING:
		case P_USTRING:
			return strequal(parm_table[i].def.svalue,
					*(char **)def_ptr);
		case P_BOOL:
			return parm_table[i].def.bvalue ==
				*(int *)def_ptr;
		case P_INTEGER:
		case P_OCTAL:
		case P_BYTES:
		case P_ENUM:
			return parm_table[i].def.ivalue ==
				*(int *)def_ptr;
	}
	return false;
}

/**
 *Display the contents of the global structure.
 */

static void dump_globals(struct loadparm_context *lp_ctx, FILE *f,
			 bool show_defaults)
{
	int i;
	struct parmlist_entry *data;

	fprintf(f, "# Global parameters\n[global]\n");

	for (i = 0; parm_table[i].label; i++)
		if (parm_table[i].pclass == P_GLOBAL &&
		    parm_table[i].offset != -1 &&
		    (i == 0 || (parm_table[i].offset != parm_table[i - 1].offset))) {
			if (!show_defaults && (lp_ctx->flags[i] & FLAG_DEFAULT)) 
				continue;
			fprintf(f, "\t%s = ", parm_table[i].label);
			print_parameter(&parm_table[i], lp_parm_ptr(lp_ctx, NULL, &parm_table[i]), f);
			fprintf(f, "\n");
	}
	if (lp_ctx->globals->param_opt != NULL) {
		for (data = lp_ctx->globals->param_opt; data;
		     data = data->next) {
			fprintf(f, "\t%s = %s\n", data->key, data->value);
		}
        }

}

/**
 * Display the contents of a single services record.
 */

static void dump_a_service(struct loadparm_service * pService, struct loadparm_service *sDefault, FILE * f)
{
	int i;
	struct parmlist_entry *data;

	if (pService != sDefault)
		fprintf(f, "\n[%s]\n", pService->szService);

	for (i = 0; parm_table[i].label; i++) {
		if (parm_table[i].pclass == P_LOCAL &&
		    parm_table[i].offset != -1 &&
		    (*parm_table[i].label != '-') &&
		    (i == 0 || (parm_table[i].offset != parm_table[i - 1].offset)))
		{
			if (pService == sDefault) {
				if (defaults_saved && is_default(sDefault, i))
					continue;
			} else {
				if (equal_parameter(parm_table[i].type,
						    ((char *)pService) +
						    parm_table[i].offset,
						    ((char *)sDefault) +
						    parm_table[i].offset))
					continue;
			}

			fprintf(f, "\t%s = ", parm_table[i].label);
			print_parameter(&parm_table[i],
					((char *)pService) + parm_table[i].offset, f);
			fprintf(f, "\n");
		}
	}
	if (pService->param_opt != NULL) {
		for (data = pService->param_opt; data; data = data->next) {
			fprintf(f, "\t%s = %s\n", data->key, data->value);
		}
        }
}

bool lp_dump_a_parameter(struct loadparm_context *lp_ctx,
			 struct loadparm_service *service,
			 const char *parm_name, FILE * f)
{
	struct parm_struct *parm;
	void *ptr;

	parm = lp_parm_struct(parm_name);
	if (!parm) {
		return false;
	}

	ptr = lp_parm_ptr(lp_ctx, service,parm);

	print_parameter(parm, ptr, f);
	fprintf(f, "\n");
	return true;
}

/**
 * Return info about the next parameter in a service.
 * snum==-1 gives the globals.
 * Return NULL when out of parameters.
 */

struct parm_struct *lp_next_parameter(struct loadparm_context *lp_ctx, int snum, int *i, 
				      int allparameters)
{
	if (snum == -1) {
		/* do the globals */
		for (; parm_table[*i].label; (*i)++) {
			if (parm_table[*i].offset == -1
			    || (*parm_table[*i].label == '-'))
				continue;

			if ((*i) > 0
			    && (parm_table[*i].offset ==
				parm_table[(*i) - 1].offset))
				continue;

			return &parm_table[(*i)++];
		}
	} else {
		struct loadparm_service *pService = lp_ctx->services[snum];

		for (; parm_table[*i].label; (*i)++) {
			if (parm_table[*i].pclass == P_LOCAL &&
			    parm_table[*i].offset != -1 &&
			    (*parm_table[*i].label != '-') &&
			    ((*i) == 0 ||
			     (parm_table[*i].offset !=
			      parm_table[(*i) - 1].offset)))
			{
				if (allparameters ||
				    !equal_parameter(parm_table[*i].type,
						     ((char *)pService) +
						     parm_table[*i].offset,
						     ((char *)lp_ctx->sDefault) +
						     parm_table[*i].offset))
				{
					return &parm_table[(*i)++];
				}
			}
		}
	}

	return NULL;
}


/**
 * Auto-load some home services.
 */
static void lp_add_auto_services(struct loadparm_context *lp_ctx,
				 const char *str)
{
	return;
}


/**
 * Unload unused services.
 */

void lp_killunused(struct loadparm_context *lp_ctx,
		   struct smbsrv_connection *smb,
		   bool (*snumused) (struct smbsrv_connection *, int))
{
	int i;
	for (i = 0; i < lp_ctx->iNumServices; i++) {
		if (lp_ctx->services[i] == NULL)
			continue;

		if (!snumused || !snumused(smb, i)) {
			talloc_free(lp_ctx->services[i]);
			lp_ctx->services[i] = NULL;
		}
	}
}


static int lp_destructor(struct loadparm_context *lp_ctx)
{
	struct parmlist_entry *data;

	if (lp_ctx->globals->param_opt != NULL) {
		struct parmlist_entry *next;
		for (data = lp_ctx->globals->param_opt; data; data=next) {
			next = data->next;
			if (data->priority & FLAG_CMDLINE) continue;
			DLIST_REMOVE(lp_ctx->globals->param_opt, data);
			talloc_free(data);
		}
	}

	return 0;
}

/**
 * Initialise the global parameter structure.
 */
struct loadparm_context *loadparm_init(TALLOC_CTX *mem_ctx)
{
	int i;
	char *myname;
	struct loadparm_context *lp_ctx;

	lp_ctx = talloc_zero(mem_ctx, struct loadparm_context);
	if (lp_ctx == NULL)
		return NULL;

	talloc_set_destructor(lp_ctx, lp_destructor);
	lp_ctx->bInGlobalSection = true;
	lp_ctx->globals = talloc_zero(lp_ctx, struct loadparm_global);
	lp_ctx->sDefault = talloc_zero(lp_ctx, struct loadparm_service);

	lp_ctx->sDefault->iMaxPrintJobs = 1000;
	lp_ctx->sDefault->bAvailable = true;
	lp_ctx->sDefault->bBrowseable = true;
	lp_ctx->sDefault->bRead_only = true;
	lp_ctx->sDefault->bMap_archive = true;
	lp_ctx->sDefault->bStrictLocking = true;
	lp_ctx->sDefault->bOplocks = true;
	lp_ctx->sDefault->iCreate_mask = 0744;
	lp_ctx->sDefault->iCreate_force_mode = 0000;
	lp_ctx->sDefault->iDir_mask = 0755;
	lp_ctx->sDefault->iDir_force_mode = 0000;

	DEBUG(3, ("Initialising global parameters\n"));

	for (i = 0; parm_table[i].label; i++) {
		if ((parm_table[i].type == P_STRING ||
		     parm_table[i].type == P_USTRING) &&
		    parm_table[i].offset != -1 &&
		    !(lp_ctx->flags[i] & FLAG_CMDLINE)) {
			char **r;
			if (parm_table[i].pclass == P_LOCAL) {
				r = (char **)(((char *)lp_ctx->sDefault) + parm_table[i].offset);
			} else {
				r = (char **)(((char *)lp_ctx->globals) + parm_table[i].offset);
			}
			*r = talloc_strdup(lp_ctx, "");
		}
	}

	lp_do_global_parameter(lp_ctx, "share backend", "classic");

	lp_do_global_parameter(lp_ctx, "server role", "standalone");

	/* options that can be set on the command line must be initialised via
	   the slower lp_do_global_parameter() to ensure that FLAG_CMDLINE is obeyed */
#ifdef TCP_NODELAY
	lp_do_global_parameter(lp_ctx, "socket options", "TCP_NODELAY");
#endif
	lp_do_global_parameter(lp_ctx, "workgroup", DEFAULT_WORKGROUP);
	myname = get_myname(lp_ctx);
	lp_do_global_parameter(lp_ctx, "netbios name", myname);
	talloc_free(myname);
	lp_do_global_parameter(lp_ctx, "name resolve order", "wins host bcast");

	lp_do_global_parameter(lp_ctx, "fstype", "NTFS");

	lp_do_global_parameter(lp_ctx, "ntvfs handler", "unixuid default");
	lp_do_global_parameter(lp_ctx, "max connections", "-1");

	lp_do_global_parameter(lp_ctx, "dcerpc endpoint servers", "epmapper srvsvc wkssvc rpcecho samr netlogon lsarpc spoolss drsuapi winreg dssetup unixinfo browser");
	lp_do_global_parameter(lp_ctx, "server services", "smb rpc nbt wrepl ldap cldap kdc drepl winbind ntp_signd kcc");
	lp_do_global_parameter(lp_ctx, "ntptr providor", "simple_ldb");
	lp_do_global_parameter(lp_ctx, "auth methods:domain controller", "anonymous sam_ignoredomain");
	lp_do_global_parameter(lp_ctx, "auth methods:member server", "anonymous sam winbind");
	lp_do_global_parameter(lp_ctx, "auth methods:standalone", "anonymous sam_ignoredomain");
	lp_do_global_parameter(lp_ctx, "private dir", dyn_PRIVATE_DIR);
	lp_do_global_parameter(lp_ctx, "sam database", "sam.ldb");
	lp_do_global_parameter(lp_ctx, "idmap database", "idmap.ldb");
	lp_do_global_parameter(lp_ctx, "secrets database", "secrets.ldb");
	lp_do_global_parameter(lp_ctx, "spoolss database", "spoolss.ldb");
	lp_do_global_parameter(lp_ctx, "wins config database", "wins_config.ldb");
	lp_do_global_parameter(lp_ctx, "wins database", "wins.ldb");
	lp_do_global_parameter(lp_ctx, "registry:HKEY_LOCAL_MACHINE", "hklm.ldb");

	/* This hive should be dynamically generated by Samba using
	   data from the sam, but for the moment leave it in a tdb to
	   keep regedt32 from popping up an annoying dialog. */
	lp_do_global_parameter(lp_ctx, "registry:HKEY_USERS", "hku.ldb");

	/* using UTF8 by default allows us to support all chars */
	lp_do_global_parameter(lp_ctx, "unix charset", "UTF8");

	/* Use codepage 850 as a default for the dos character set */
	lp_do_global_parameter(lp_ctx, "dos charset", "CP850");

	/*
	 * Allow the default PASSWD_CHAT to be overridden in local.h.
	 */
	lp_do_global_parameter(lp_ctx, "passwd chat", DEFAULT_PASSWD_CHAT);

	lp_do_global_parameter(lp_ctx, "pid directory", dyn_PIDDIR);
	lp_do_global_parameter(lp_ctx, "lock dir", dyn_LOCKDIR);
	lp_do_global_parameter(lp_ctx, "modules dir", dyn_MODULESDIR);
	lp_do_global_parameter(lp_ctx, "ncalrpc dir", dyn_NCALRPCDIR);

	lp_do_global_parameter(lp_ctx, "socket address", "0.0.0.0");
	lp_do_global_parameter_var(lp_ctx, "server string",
				   "Samba %s", SAMBA_VERSION_STRING);

	lp_do_global_parameter_var(lp_ctx, "announce version", "%d.%d",
			 DEFAULT_MAJOR_VERSION,
			 DEFAULT_MINOR_VERSION);

	lp_do_global_parameter(lp_ctx, "password server", "*");

	lp_do_global_parameter(lp_ctx, "max mux", "50");
	lp_do_global_parameter(lp_ctx, "max xmit", "12288");
	lp_do_global_parameter(lp_ctx, "password level", "0");
	lp_do_global_parameter(lp_ctx, "LargeReadwrite", "True");
	lp_do_global_parameter(lp_ctx, "server min protocol", "CORE");
	lp_do_global_parameter(lp_ctx, "server max protocol", "NT1");
	lp_do_global_parameter(lp_ctx, "client min protocol", "CORE");
	lp_do_global_parameter(lp_ctx, "client max protocol", "NT1");
	lp_do_global_parameter(lp_ctx, "security", "USER");
	lp_do_global_parameter(lp_ctx, "paranoid server security", "True");
	lp_do_global_parameter(lp_ctx, "EncryptPasswords", "True");
	lp_do_global_parameter(lp_ctx, "ReadRaw", "True");
	lp_do_global_parameter(lp_ctx, "WriteRaw", "True");
	lp_do_global_parameter(lp_ctx, "NullPasswords", "False");
	lp_do_global_parameter(lp_ctx, "ObeyPamRestrictions", "False");
	lp_do_global_parameter(lp_ctx, "announce as", "NT SERVER");

	lp_do_global_parameter(lp_ctx, "TimeServer", "False");
	lp_do_global_parameter(lp_ctx, "BindInterfacesOnly", "False");
	lp_do_global_parameter(lp_ctx, "Unicode", "True");
	lp_do_global_parameter(lp_ctx, "ClientLanManAuth", "False");
	lp_do_global_parameter(lp_ctx, "LanmanAuth", "False");
	lp_do_global_parameter(lp_ctx, "NTLMAuth", "True");
	lp_do_global_parameter(lp_ctx, "client use spnego principal", "False");

	lp_do_global_parameter(lp_ctx, "UnixExtensions", "False");

	lp_do_global_parameter(lp_ctx, "PreferredMaster", "Auto");
	lp_do_global_parameter(lp_ctx, "LocalMaster", "True");

	lp_do_global_parameter(lp_ctx, "wins support", "False");
	lp_do_global_parameter(lp_ctx, "dns proxy", "True");

	lp_do_global_parameter(lp_ctx, "winbind separator", "\\");
	lp_do_global_parameter(lp_ctx, "winbind sealed pipes", "True");
	lp_do_global_parameter(lp_ctx, "winbindd socket directory", dyn_WINBINDD_SOCKET_DIR);
	lp_do_global_parameter(lp_ctx, "winbindd privileged socket directory", dyn_WINBINDD_PRIVILEGED_SOCKET_DIR);
	lp_do_global_parameter(lp_ctx, "template shell", "/bin/false");
	lp_do_global_parameter(lp_ctx, "template homedir", "/home/%WORKGROUP%/%ACCOUNTNAME%");
	lp_do_global_parameter(lp_ctx, "idmap trusted only", "False");

	lp_do_global_parameter(lp_ctx, "client signing", "Yes");
	lp_do_global_parameter(lp_ctx, "server signing", "auto");

	lp_do_global_parameter(lp_ctx, "use spnego", "True");

	lp_do_global_parameter(lp_ctx, "smb ports", "445 139");
	lp_do_global_parameter(lp_ctx, "nbt port", "137");
	lp_do_global_parameter(lp_ctx, "dgram port", "138");
	lp_do_global_parameter(lp_ctx, "cldap port", "389");
	lp_do_global_parameter(lp_ctx, "krb5 port", "88");
	lp_do_global_parameter(lp_ctx, "kpasswd port", "464");
	lp_do_global_parameter(lp_ctx, "web port", "901");
	lp_do_global_parameter(lp_ctx, "swat directory", dyn_SWATDIR);

	lp_do_global_parameter(lp_ctx, "nt status support", "True");

	lp_do_global_parameter(lp_ctx, "max wins ttl", "518400"); /* 6 days */
	lp_do_global_parameter(lp_ctx, "min wins ttl", "10");

	lp_do_global_parameter(lp_ctx, "tls enabled", "True");
	lp_do_global_parameter(lp_ctx, "tls keyfile", "tls/key.pem");
	lp_do_global_parameter(lp_ctx, "tls certfile", "tls/cert.pem");
	lp_do_global_parameter(lp_ctx, "tls cafile", "tls/ca.pem");
	lp_do_global_parameter_var(lp_ctx, "setup directory", "%s",
				   dyn_SETUPDIR);

	lp_do_global_parameter(lp_ctx, "prefork children:smb", "4");

	lp_do_global_parameter(lp_ctx, "ntp signd socket directory", dyn_NTP_SIGND_SOCKET_DIR);

	for (i = 0; parm_table[i].label; i++) {
		if (!(lp_ctx->flags[i] & FLAG_CMDLINE)) {
			lp_ctx->flags[i] |= FLAG_DEFAULT;
		}
	}

	return lp_ctx;
}

const char *lp_configfile(struct loadparm_context *lp_ctx)
{
	return lp_ctx->szConfigFile;
}

const char *lp_default_path(void)
{
    if (getenv("SMB_CONF_PATH"))
        return getenv("SMB_CONF_PATH");
    else
        return dyn_CONFIGFILE;
}

/**
 * Update the internal state of a loadparm context after settings 
 * have changed.
 */
static bool lp_update(struct loadparm_context *lp_ctx)
{
	lp_add_auto_services(lp_ctx, lp_auto_services(lp_ctx));

	lp_add_hidden(lp_ctx, "IPC$", "IPC");
	lp_add_hidden(lp_ctx, "ADMIN$", "DISK");

	if (!lp_ctx->globals->szWINSservers && lp_ctx->globals->bWINSsupport) {
		lp_do_global_parameter(lp_ctx, "wins server", "127.0.0.1");
	}

	panic_action = lp_ctx->globals->panic_action;

	reload_charcnv(lp_ctx);

	/* FIXME: ntstatus_check_dos_mapping = lp_nt_status_support(lp_ctx); */

	/* FIXME: This is a bit of a hack, but we can't use a global, since 
	 * not everything that uses lp also uses the socket library */
	if (lp_parm_bool(lp_ctx, NULL, "socket", "testnonblock", false)) {
		setenv("SOCKET_TESTNONBLOCK", "1", 1);
	} else {
		unsetenv("SOCKET_TESTNONBLOCK");
	}

	/* FIXME: Check locale in environment for this: */
	if (strcmp(lp_display_charset(lp_ctx), lp_unix_charset(lp_ctx)) != 0)
		d_set_iconv(smb_iconv_open(lp_display_charset(lp_ctx), lp_unix_charset(lp_ctx)));
	else
		d_set_iconv((smb_iconv_t)-1);

	return true;
}

bool lp_load_default(struct loadparm_context *lp_ctx)
{
    const char *path;

    path = lp_default_path();

    if (!file_exist(path)) {
	    /* We allow the default smb.conf file to not exist, 
	     * basically the equivalent of an empty file. */
	    return lp_update(lp_ctx);
    }

    return lp_load(lp_ctx, path);
}

/**
 * Load the services array from the services file.
 *
 * Return True on success, False on failure.
 */
bool lp_load(struct loadparm_context *lp_ctx, const char *filename)
{
	char *n2;
	bool bRetval;

	filename = talloc_strdup(lp_ctx, filename);

	lp_ctx->szConfigFile = filename;

	lp_ctx->bInGlobalSection = true;
	n2 = standard_sub_basic(lp_ctx, lp_ctx->szConfigFile);
	DEBUG(2, ("lp_load: refreshing parameters from %s\n", n2));

	add_to_file_list(lp_ctx, lp_ctx->szConfigFile, n2);

	/* We get sections first, so have to start 'behind' to make up */
	lp_ctx->currentService = NULL;
	bRetval = pm_process(n2, do_section, do_parameter, lp_ctx);

	/* finish up the last section */
	DEBUG(4, ("pm_process() returned %s\n", BOOLSTR(bRetval)));
	if (bRetval)
		if (lp_ctx->currentService != NULL)
			bRetval = service_ok(lp_ctx->currentService);

	bRetval = bRetval && lp_update(lp_ctx);

	return bRetval;
}

/**
 * Return the max number of services.
 */

int lp_numservices(struct loadparm_context *lp_ctx)
{
	return lp_ctx->iNumServices;
}

/**
 * Display the contents of the services array in human-readable form.
 */

void lp_dump(struct loadparm_context *lp_ctx, FILE *f, bool show_defaults,
	     int maxtoprint)
{
	int iService;

	if (show_defaults)
		defaults_saved = false;

	dump_globals(lp_ctx, f, show_defaults);

	dump_a_service(lp_ctx->sDefault, lp_ctx->sDefault, f);

	for (iService = 0; iService < maxtoprint; iService++)
		lp_dump_one(f, show_defaults, lp_ctx->services[iService], lp_ctx->sDefault);
}

/**
 * Display the contents of one service in human-readable form.
 */
void lp_dump_one(FILE *f, bool show_defaults, struct loadparm_service *service, struct loadparm_service *sDefault)
{
	if (service != NULL) {
		if (service->szService[0] == '\0')
			return;
		dump_a_service(service, sDefault, f);
	}
}

struct loadparm_service *lp_servicebynum(struct loadparm_context *lp_ctx,
					 int snum)
{
	return lp_ctx->services[snum];
}

struct loadparm_service *lp_service(struct loadparm_context *lp_ctx,
				    const char *service_name)
{
	int iService;
        char *serviceName;

	for (iService = lp_ctx->iNumServices - 1; iService >= 0; iService--) {
		if (lp_ctx->services[iService] &&
		    lp_ctx->services[iService]->szService) {
			/*
			 * The substitution here is used to support %U is
			 * service names
			 */
			serviceName = standard_sub_basic(
					lp_ctx->services[iService],
					lp_ctx->services[iService]->szService);
			if (strequal(serviceName, service_name))
				return lp_ctx->services[iService];
		}
	}

	DEBUG(7,("lp_servicenumber: couldn't find %s\n", service_name));
	return NULL;
}


/**
 * A useful volume label function.
 */
const char *volume_label(struct loadparm_service *service, struct loadparm_service *sDefault)
{
	const char *ret = lp_volume(service, sDefault);
	if (!*ret)
		return lp_servicename(service);
	return ret;
}


/**
 * If we are PDC then prefer us as DMB
 */
const char *lp_printername(struct loadparm_service *service, struct loadparm_service *sDefault)
{
	const char *ret = _lp_printername(service, sDefault);
	if (ret == NULL || (ret != NULL && *ret == '\0'))
		ret = lp_servicename(service);

	return ret;
}


/**
 * Return the max print jobs per queue.
 */
int lp_maxprintjobs(struct loadparm_service *service, struct loadparm_service *sDefault)
{
	int maxjobs = (service != NULL) ? service->iMaxPrintJobs : sDefault->iMaxPrintJobs;
	if (maxjobs <= 0 || maxjobs >= PRINT_MAX_JOBID)
		maxjobs = PRINT_MAX_JOBID - 1;

	return maxjobs;
}

struct smb_iconv_convenience *lp_iconv_convenience(struct loadparm_context *lp_ctx)
{
	if (lp_ctx == NULL) {
		static struct smb_iconv_convenience *fallback_ic = NULL;
		if (fallback_ic == NULL)
			fallback_ic = smb_iconv_convenience_init(talloc_autofree_context(), 
						  "CP850", "UTF8", true);
		return fallback_ic;
	}
	return lp_ctx->iconv_convenience;
}

_PUBLIC_ void reload_charcnv(struct loadparm_context *lp_ctx)
{
	talloc_unlink(lp_ctx, lp_ctx->iconv_convenience);
	global_iconv_convenience = lp_ctx->iconv_convenience = smb_iconv_convenience_init_lp(lp_ctx, lp_ctx);
}

void lp_smbcli_options(struct loadparm_context *lp_ctx,
			 struct smbcli_options *options)
{
	options->max_xmit = lp_max_xmit(lp_ctx);
	options->max_mux = lp_maxmux(lp_ctx);
	options->use_spnego = lp_nt_status_support(lp_ctx) && lp_use_spnego(lp_ctx); 
	options->signing = lp_client_signing(lp_ctx);
	options->request_timeout = SMB_REQUEST_TIMEOUT;
	options->ntstatus_support = lp_nt_status_support(lp_ctx);
	options->max_protocol = lp_cli_maxprotocol(lp_ctx);
	options->unicode = lp_unicode(lp_ctx);
	options->use_oplocks = true;
	options->use_level2_oplocks = true;
}

void lp_smbcli_session_options(struct loadparm_context *lp_ctx,
				 struct smbcli_session_options *options)
{
	options->lanman_auth = lp_client_lanman_auth(lp_ctx);
	options->ntlmv2_auth = lp_client_ntlmv2_auth(lp_ctx);
	options->plaintext_auth = lp_client_plaintext_auth(lp_ctx);
}

_PUBLIC_ char *lp_tls_keyfile(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx)
{
	return private_path(mem_ctx, lp_ctx, lp_ctx->globals->tls_keyfile);
}

_PUBLIC_ char *lp_tls_certfile(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx)
{
	return private_path(mem_ctx, lp_ctx, lp_ctx->globals->tls_certfile);
}

_PUBLIC_ char *lp_tls_cafile(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx)
{
	return private_path(mem_ctx, lp_ctx, lp_ctx->globals->tls_cafile);
}

_PUBLIC_ char *lp_tls_crlfile(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx)
{
	return private_path(mem_ctx, lp_ctx, lp_ctx->globals->tls_crlfile);
}

_PUBLIC_ char *lp_tls_dhpfile(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx)
{
	return private_path(mem_ctx, lp_ctx, lp_ctx->globals->tls_dhpfile);
}

_PUBLIC_ struct dcerpc_server_info *lp_dcerpc_server_info(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx)
{
	struct dcerpc_server_info *ret = talloc_zero(mem_ctx, struct dcerpc_server_info);

	ret->domain_name = talloc_reference(mem_ctx, lp_workgroup(lp_ctx));
	ret->version_major = lp_parm_int(lp_ctx, NULL, "server_info", "version_major", 5);
	ret->version_minor = lp_parm_int(lp_ctx, NULL, "server_info", "version_minor", 2);
	ret->version_build = lp_parm_int(lp_ctx, NULL, "server_info", "version_build", 3790);

	return ret;
}

struct gensec_settings *lp_gensec_settings(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx)
{
	struct gensec_settings *settings = talloc(mem_ctx, struct gensec_settings);
	if (settings == NULL)
		return NULL;
	SMB_ASSERT(lp_ctx != NULL);
	settings->lp_ctx = talloc_reference(settings, lp_ctx);
	settings->iconv_convenience = lp_iconv_convenience(lp_ctx);
	settings->target_hostname = lp_parm_string(lp_ctx, NULL, "gensec", "target_hostname");
	return settings;
}

