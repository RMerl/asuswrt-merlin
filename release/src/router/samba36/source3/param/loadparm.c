/* 
   Unix SMB/CIFS implementation.
   Parameter loading functions
   Copyright (C) Karl Auer 1993-1998

   Largely re-written by Andrew Tridgell, September 1994

   Copyright (C) Simo Sorce 2001
   Copyright (C) Alexander Bokovoy 2002
   Copyright (C) Stefan (metze) Metzmacher 2002
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2003
   Copyright (C) Michael Adam 2008

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
#include "system/filesys.h"
#include "util_tdb.h"
#include "printing.h"
#include "lib/smbconf/smbconf.h"
#include "lib/smbconf/smbconf_init.h"

#include "ads.h"
#include "../librpc/gen_ndr/svcctl.h"
#include "intl.h"
#include "smb_signing.h"
#include "dbwrap.h"
#include "smbldap.h"
#include "../lib/util/bitmap.h"

#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif

#ifdef HAVE_HTTPCONNECTENCRYPT
#include <cups/http.h>
#endif

bool bLoaded = False;

extern userdom_struct current_user_info;

#ifndef GLOBAL_NAME
#define GLOBAL_NAME "global"
#endif

#ifndef PRINTERS_NAME
#define PRINTERS_NAME "printers"
#endif

#ifndef HOMES_NAME
#define HOMES_NAME "homes"
#endif

/* the special value for the include parameter
 * to be interpreted not as a file name but to
 * trigger loading of the global smb.conf options
 * from registry. */
#ifndef INCLUDE_REGISTRY_NAME
#define INCLUDE_REGISTRY_NAME "registry"
#endif

static bool in_client = False;		/* Not in the client by default */
static struct smbconf_csn conf_last_csn;

#define CONFIG_BACKEND_FILE 0
#define CONFIG_BACKEND_REGISTRY 1

static int config_backend = CONFIG_BACKEND_FILE;

/* some helpful bits */
#define LP_SNUM_OK(i) (((i) >= 0) && ((i) < iNumServices) && (ServicePtrs != NULL) && ServicePtrs[(i)]->valid)
#define VALID(i) (ServicePtrs != NULL && ServicePtrs[i]->valid)

#define USERSHARE_VALID 1
#define USERSHARE_PENDING_DELETE 2

static bool defaults_saved = False;

struct param_opt_struct {
	struct param_opt_struct *prev, *next;
	char *key;
	char *value;
	char **list;
	unsigned flags;
};

/*
 * This structure describes global (ie., server-wide) parameters.
 */
struct global {
	int ConfigBackend;
	char *smb_ports;
	char *dos_charset;
	char *unix_charset;
	char *display_charset;
	char *szPrintcapname;
	char *szAddPortCommand;
	char *szEnumPortsCommand;
	char *szAddPrinterCommand;
	char *szDeletePrinterCommand;
	char *szOs2DriverMap;
	char *szLockDir;
	char *szStateDir;
	char *szCacheDir;
	char *szPidDir;
	char *szRootdir;
	char *szDefaultService;
	char *szGetQuota;
	char *szSetQuota;
	char *szMsgCommand;
	char *szServerString;
	char *szAutoServices;
	char *szPasswdProgram;
	char *szPasswdChat;
	char *szLogFile;
	char *szConfigFile;
	char *szSMBPasswdFile;
	char *szPrivateDir;
	char *szPassdbBackend;
	char **szPreloadModules;
	char *szPasswordServer;
	char *szSocketOptions;
	char *szRealm;
	char *szAfsUsernameMap;
	int iAfsTokenLifetime;
	char *szLogNtTokenCommand;
	char *szUsernameMap;
	char *szLogonScript;
	char *szLogonPath;
	char *szLogonDrive;
	char *szLogonHome;
	char **szWINSservers;
	char **szInterfaces;
	char *szRemoteAnnounce;
	char *szRemoteBrowseSync;
	char *szSocketAddress;
	bool bNmbdBindExplicitBroadcast;
	char *szNISHomeMapName;
	char *szAnnounceVersion;	/* This is initialised in init_globals */
	char *szWorkgroup;
	char *szNetbiosName;
	char **szNetbiosAliases;
	char *szNetbiosScope;
	char *szNameResolveOrder;
	char *szPanicAction;
	char *szAddUserScript;
	char *szRenameUserScript;
	char *szDelUserScript;
	char *szAddGroupScript;
	char *szDelGroupScript;
	char *szAddUserToGroupScript;
	char *szDelUserFromGroupScript;
	char *szSetPrimaryGroupScript;
	char *szAddMachineScript;
	char *szShutdownScript;
	char *szAbortShutdownScript;
	char *szUsernameMapScript;
	int iUsernameMapCacheTime;
	char *szCheckPasswordScript;
	char *szWINSHook;
	char *szUtmpDir;
	char *szWtmpDir;
	bool bUtmp;
	char *szIdmapUID;
	char *szIdmapGID;
	bool bPassdbExpandExplicit;
	int AlgorithmicRidBase;
	char *szTemplateHomedir;
	char *szTemplateShell;
	char *szWinbindSeparator;
	bool bWinbindEnumUsers;
	bool bWinbindEnumGroups;
	bool bWinbindUseDefaultDomain;
	bool bWinbindTrustedDomainsOnly;
	bool bWinbindNestedGroups;
	int  winbind_expand_groups;
	bool bWinbindRefreshTickets;
	bool bWinbindOfflineLogon;
	bool bWinbindSealedPipes;
	bool bWinbindNormalizeNames;
	bool bWinbindRpcOnly;
	bool bCreateKrb5Conf;
	int winbindMaxDomainConnections;
	char *szIdmapBackend;
	bool bIdmapReadOnly;
	char *szAddShareCommand;
	char *szChangeShareCommand;
	char *szDeleteShareCommand;
	char **szEventLogs;
	char *szGuestaccount;
	char *szManglingMethod;
	char **szServicesList;
	char *szUsersharePath;
	char *szUsershareTemplateShare;
	char **szUsersharePrefixAllowList;
	char **szUsersharePrefixDenyList;
	int mangle_prefix;
	int max_log_size;
	char *szLogLevel;
	int max_xmit;
	int max_mux;
	int max_open_files;
	int open_files_db_hash_size;
	int pwordlevel;
	int unamelevel;
	int deadtime;
	bool getwd_cache;
	int maxprotocol;
	int minprotocol;
	int security;
	char **AuthMethods;
	bool paranoid_server_security;
	int maxdisksize;
	int lpqcachetime;
	int iMaxSmbdProcesses;
	bool bDisableSpoolss;
	int syslog;
	int os_level;
	bool enhanced_browsing;
	int max_ttl;
	int max_wins_ttl;
	int min_wins_ttl;
	int lm_announce;
	int lm_interval;
	int announce_as;	/* This is initialised in init_globals */
	int machine_password_timeout;
	int map_to_guest;
	int oplock_break_wait_time;
	int winbind_cache_time;
	int winbind_reconnect_delay;
	int winbind_max_clients;
	char **szWinbindNssInfo;
	int iLockSpinTime;
	char *szLdapMachineSuffix;
	char *szLdapUserSuffix;
	char *szLdapIdmapSuffix;
	char *szLdapGroupSuffix;
	int ldap_ssl;
	bool ldap_ssl_ads;
	int ldap_deref;
	int ldap_follow_referral;
	char *szLdapSuffix;
	char *szLdapAdminDn;
	int ldap_debug_level;
	int ldap_debug_threshold;
	int iAclCompat;
	char *szCupsServer;
	int CupsEncrypt;
	char *szIPrintServer;
	char *ctdbdSocket;
	char **szClusterAddresses;
	bool clustering;
	int ctdb_timeout;
	int ctdb_locktime_warn_threshold;
	int ldap_passwd_sync;
	int ldap_replication_sleep;
	int ldap_timeout; /* This is initialised in init_globals */
	int ldap_connection_timeout;
	int ldap_page_size;
	bool ldap_delete_dn;
	bool bMsAddPrinterWizard;
	bool bDNSproxy;
	bool bWINSsupport;
	bool bWINSproxy;
	bool bLocalMaster;
	int  iPreferredMaster;
	int iDomainMaster;
	bool bDomainLogons;
	char **szInitLogonDelayedHosts;
	int InitLogonDelay;
	bool bEncryptPasswords;
	bool bUpdateEncrypt;
	int  clientSchannel;
	int  serverSchannel;
	bool bNullPasswords;
	bool bObeyPamRestrictions;
	bool bLoadPrinters;
	int PrintcapCacheTime;
	bool bLargeReadwrite;
	bool bReadRaw;
	bool bWriteRaw;
	bool bSyslogOnly;
	bool bBrowseList;
	bool bNISHomeMap;
	bool bTimeServer;
	bool bBindInterfacesOnly;
	bool bPamPasswordChange;
	bool bUnixPasswdSync;
	bool bPasswdChatDebug;
	int iPasswdChatTimeout;
	bool bTimestampLogs;
	bool bNTSmbSupport;
	bool bNTPipeSupport;
	bool bNTStatusSupport;
	bool bStatCache;
	int iMaxStatCacheSize;
	bool bKernelOplocks;
	bool bAllowTrustedDomains;
	bool bLanmanAuth;
	bool bNTLMAuth;
	bool bRawNTLMv2Auth;
	bool bUseSpnego;
	bool bClientLanManAuth;
	bool bClientNTLMv2Auth;
	bool bClientPlaintextAuth;
	bool bClientUseSpnego;
	bool client_use_spnego_principal;
	bool send_spnego_principal;
	bool bDebugPrefixTimestamp;
	bool bDebugHiresTimestamp;
	bool bDebugPid;
	bool bDebugUid;
	bool bDebugClass;
	bool bEnableCoreFiles;
	bool bHostMSDfs;
	bool bUseMmap;
	bool bHostnameLookups;
	bool bUnixExtensions;
	bool bAllowDcerpcAuthLevelConnect;
	bool bDisableNetbios;
	char * szDedicatedKeytabFile;
	int  iKerberosMethod;
	bool bDeferSharingViolations;
	bool bEnablePrivileges;
	bool bASUSupport;
	bool bUsershareOwnerOnly;
	bool bUsershareAllowGuests;
	bool bRegistryShares;
	int restrict_anonymous;
	int name_cache_timeout;
	int client_signing;
	int client_ipc_signing;
	int server_signing;
	int client_ldap_sasl_wrapping;
	int iUsershareMaxShares;
	int iIdmapCacheTime;
	int iIdmapNegativeCacheTime;
	bool bResetOnZeroVC;
	bool bLogWriteableFilesOnExit;
	int iKeepalive;
	int iminreceivefile;
	struct param_opt_struct *param_opt;
	int cups_connection_timeout;
	char *szSMBPerfcountModule;
	bool bMapUntrustedToDomain;
	bool bAsyncSMBEchoHandler;
	bool bMulticastDnsRegister;
	bool bAllowInsecureWidelinks;
	int ismb2_max_read;
	int ismb2_max_write;
	int ismb2_max_trans;
	int ismb2_max_credits;
	char *ncalrpc_dir;
};

static struct global Globals;

/*
 * This structure describes a single service.
 */
struct service {
	bool valid;
	bool autoloaded;
	int usershare;
	struct timespec usershare_last_mod;
	char *szService;
	char *szPath;
	char *szUsername;
	char **szInvalidUsers;
	char **szValidUsers;
	char **szAdminUsers;
	char *szCopy;
	char *szInclude;
	char *szPreExec;
	char *szPostExec;
	char *szRootPreExec;
	char *szRootPostExec;
	char *szCupsOptions;
	char *szPrintcommand;
	char *szLpqcommand;
	char *szLprmcommand;
	char *szLppausecommand;
	char *szLpresumecommand;
	char *szQueuepausecommand;
	char *szQueueresumecommand;
	char *szPrintername;
	char *szPrintjobUsername;
	char *szDontdescend;
	char **szHostsallow;
	char **szHostsdeny;
	char *szMagicScript;
	char *szMagicOutput;
	char *szVetoFiles;
	char *szHideFiles;
	char *szVetoOplockFiles;
	char *comment;
	char *force_user;
	char *force_group;
	char **readlist;
	char **writelist;
	char **printer_admin;
	char *volume;
	char *fstype;
	char **szVfsObjects;
	char *szMSDfsProxy;
	char *szAioWriteBehind;
	char *szDfree;
	int iMinPrintSpace;
	int iMaxPrintJobs;
	int iMaxReportedPrintJobs;
	int iWriteCacheSize;
	int iCreate_mask;
	int iCreate_force_mode;
	int iSecurity_mask;
	int iSecurity_force_mode;
	int iDir_mask;
	int iDir_force_mode;
	int iDir_Security_mask;
	int iDir_Security_force_mode;
	int iMaxConnections;
	int iDefaultCase;
	int iPrinting;
	int iOplockContentionLimit;
	int iCSCPolicy;
	int iBlock_size;
	int iDfreeCacheTime;
	bool bPreexecClose;
	bool bRootpreexecClose;
	int  iCaseSensitive;
	bool bCasePreserve;
	bool bShortCasePreserve;
	bool bHideDotFiles;
	bool bHideSpecialFiles;
	bool bHideUnReadable;
	bool bHideUnWriteableFiles;
	bool bBrowseable;
	bool bAccessBasedShareEnum;
	bool bAvailable;
	bool bRead_only;
	bool bNo_set_dir;
	bool bGuest_only;
	bool bAdministrative_share;
	bool bGuest_ok;
	bool bPrint_ok;
	bool bPrintNotifyBackchannel;
	bool bMap_system;
	bool bMap_hidden;
	bool bMap_archive;
	bool bStoreDosAttributes;
	bool bDmapiSupport;
	bool bLocking;
	int iStrictLocking;
	bool bPosixLocking;
	bool bShareModes;
	bool bOpLocks;
	bool bLevel2OpLocks;
	bool bOnlyUser;
	bool bMangledNames;
	bool bWidelinks;
	bool bSymlinks;
	bool bSyncAlways;
	bool bStrictAllocate;
	bool bStrictSync;
	char magic_char;
	struct bitmap *copymap;
	bool bDeleteReadonly;
	bool bFakeOplocks;
	bool bDeleteVetoFiles;
	bool bDosFilemode;
	bool bDosFiletimes;
	bool bDosFiletimeResolution;
	bool bFakeDirCreateTimes;
	bool bBlockingLocks;
	bool bInheritPerms;
	bool bInheritACLS;
	bool bInheritOwner;
	bool bMSDfsRoot;
	bool bUseClientDriver;
	bool bDefaultDevmode;
	bool bForcePrintername;
	bool bNTAclSupport;
	bool bForceUnknownAclUser;
	bool bUseSendfile;
	bool bProfileAcls;
	bool bMap_acl_inherit;
	bool bAfs_Share;
	bool bEASupport;
	bool bAclCheckPermissions;
	bool bAclMapFullControl;
	bool bAclGroupControl;
	bool bChangeNotify;
	bool bKernelChangeNotify;
	int iallocation_roundup_size;
	int iAioReadSize;
	int iAioWriteSize;
	int iMap_readonly;
	int iDirectoryNameCacheSize;
	int ismb_encrypt;
	struct param_opt_struct *param_opt;

	char dummy[3];		/* for alignment */
};


/* This is a default service used to prime a services structure */
static struct service sDefault = {
	True,			/* valid */
	False,			/* not autoloaded */
	0,			/* not a usershare */
	{0, },                  /* No last mod time */
	NULL,			/* szService */
	NULL,			/* szPath */
	NULL,			/* szUsername */
	NULL,			/* szInvalidUsers */
	NULL,			/* szValidUsers */
	NULL,			/* szAdminUsers */
	NULL,			/* szCopy */
	NULL,			/* szInclude */
	NULL,			/* szPreExec */
	NULL,			/* szPostExec */
	NULL,			/* szRootPreExec */
	NULL,			/* szRootPostExec */
	NULL,			/* szCupsOptions */
	NULL,			/* szPrintcommand */
	NULL,			/* szLpqcommand */
	NULL,			/* szLprmcommand */
	NULL,			/* szLppausecommand */
	NULL,			/* szLpresumecommand */
	NULL,			/* szQueuepausecommand */
	NULL,			/* szQueueresumecommand */
	NULL,			/* szPrintername */
	NULL,			/* szPrintjobUsername */
	NULL,			/* szDontdescend */
	NULL,			/* szHostsallow */
	NULL,			/* szHostsdeny */
	NULL,			/* szMagicScript */
	NULL,			/* szMagicOutput */
	NULL,			/* szVetoFiles */
	NULL,			/* szHideFiles */
	NULL,			/* szVetoOplockFiles */
	NULL,			/* comment */
	NULL,			/* force user */
	NULL,			/* force group */
	NULL,			/* readlist */
	NULL,			/* writelist */
	NULL,			/* printer admin */
	NULL,			/* volume */
	NULL,			/* fstype */
	NULL,			/* vfs objects */
	NULL,                   /* szMSDfsProxy */
	NULL,			/* szAioWriteBehind */
	NULL,			/* szDfree */
	0,			/* iMinPrintSpace */
	1000,			/* iMaxPrintJobs */
	0,			/* iMaxReportedPrintJobs */
	0,			/* iWriteCacheSize */
	0744,			/* iCreate_mask */
	0000,			/* iCreate_force_mode */
	0777,			/* iSecurity_mask */
	0,			/* iSecurity_force_mode */
	0755,			/* iDir_mask */
	0000,			/* iDir_force_mode */
	0777,			/* iDir_Security_mask */
	0,			/* iDir_Security_force_mode */
	0,			/* iMaxConnections */
	CASE_LOWER,		/* iDefaultCase */
	DEFAULT_PRINTING,	/* iPrinting */
	2,			/* iOplockContentionLimit */
	0,			/* iCSCPolicy */
	1024,           	/* iBlock_size */
	0,			/* iDfreeCacheTime */
	False,			/* bPreexecClose */
	False,			/* bRootpreexecClose */
	Auto,			/* case sensitive */
	True,			/* case preserve */
	True,			/* short case preserve */
	True,			/* bHideDotFiles */
	False,			/* bHideSpecialFiles */
	False,			/* bHideUnReadable */
	False,			/* bHideUnWriteableFiles */
	True,			/* bBrowseable */
	False,			/* bAccessBasedShareEnum */
	True,			/* bAvailable */
	True,			/* bRead_only */
	True,			/* bNo_set_dir */
	False,			/* bGuest_only */
	False,			/* bAdministrative_share */
	False,			/* bGuest_ok */
	False,			/* bPrint_ok */
	True,			/* bPrintNotifyBackchannel */
	False,			/* bMap_system */
	False,			/* bMap_hidden */
	True,			/* bMap_archive */
	False,			/* bStoreDosAttributes */
	False,			/* bDmapiSupport */
	True,			/* bLocking */
	Auto,			/* iStrictLocking */
	True,			/* bPosixLocking */
	True,			/* bShareModes */
	True,			/* bOpLocks */
	True,			/* bLevel2OpLocks */
	False,			/* bOnlyUser */
	True,			/* bMangledNames */
	false,			/* bWidelinks */
	True,			/* bSymlinks */
	False,			/* bSyncAlways */
	False,			/* bStrictAllocate */
	False,			/* bStrictSync */
	'~',			/* magic char */
	NULL,			/* copymap */
	False,			/* bDeleteReadonly */
	False,			/* bFakeOplocks */
	False,			/* bDeleteVetoFiles */
	False,			/* bDosFilemode */
	True,			/* bDosFiletimes */
	False,			/* bDosFiletimeResolution */
	False,			/* bFakeDirCreateTimes */
	True,			/* bBlockingLocks */
	False,			/* bInheritPerms */
	False,			/* bInheritACLS */
	False,			/* bInheritOwner */
	False,			/* bMSDfsRoot */
	False,			/* bUseClientDriver */
	True,			/* bDefaultDevmode */
	False,			/* bForcePrintername */
	True,			/* bNTAclSupport */
	False,                  /* bForceUnknownAclUser */
	False,			/* bUseSendfile */
	False,			/* bProfileAcls */
	False,			/* bMap_acl_inherit */
	False,			/* bAfs_Share */
	False,			/* bEASupport */
	True,			/* bAclCheckPermissions */
	True,			/* bAclMapFullControl */
	False,			/* bAclGroupControl */
	True,			/* bChangeNotify */
	True,			/* bKernelChangeNotify */
	SMB_ROUNDUP_ALLOCATION_SIZE,		/* iallocation_roundup_size */
	0,			/* iAioReadSize */
	0,			/* iAioWriteSize */
	MAP_READONLY_YES,	/* iMap_readonly */
#ifdef BROKEN_DIRECTORY_HANDLING
	0,			/* iDirectoryNameCacheSize */
#else
	100,			/* iDirectoryNameCacheSize */
#endif
	Auto,			/* ismb_encrypt */
	NULL,			/* Parametric options */

	""			/* dummy */
};

/* local variables */
static struct service **ServicePtrs = NULL;
static int iNumServices = 0;
static int iServiceIndex = 0;
static struct db_context *ServiceHash;
static int *invalid_services = NULL;
static int num_invalid_services = 0;
static bool bInGlobalSection = True;
static bool bGlobalOnly = False;
static int default_server_announce;

#define NUMPARAMETERS (sizeof(parm_table) / sizeof(struct parm_struct))

/* prototypes for the special type handlers */
static bool handle_include( int snum, const char *pszParmValue, char **ptr);
static bool handle_copy( int snum, const char *pszParmValue, char **ptr);
static bool handle_netbios_name( int snum, const char *pszParmValue, char **ptr);
static bool handle_idmap_backend(int snum, const char *pszParmValue, char **ptr);
static bool handle_idmap_uid( int snum, const char *pszParmValue, char **ptr);
static bool handle_idmap_gid( int snum, const char *pszParmValue, char **ptr);
static bool handle_debug_list( int snum, const char *pszParmValue, char **ptr );
static bool handle_workgroup( int snum, const char *pszParmValue, char **ptr );
static bool handle_netbios_aliases( int snum, const char *pszParmValue, char **ptr );
static bool handle_netbios_scope( int snum, const char *pszParmValue, char **ptr );
static bool handle_charset( int snum, const char *pszParmValue, char **ptr );
static bool handle_dos_charset( int snum, const char *pszParmValue, char **ptr );
static bool handle_printing( int snum, const char *pszParmValue, char **ptr);
static bool handle_ldap_debug_level( int snum, const char *pszParmValue, char **ptr);

static void set_default_server_announce_type(void);
static void set_allowed_client_auth(void);

static void *lp_local_ptr(struct service *service, void *ptr);

static void add_to_file_list(const char *fname, const char *subfname);
static bool lp_set_cmdline_helper(const char *pszParmName, const char *pszParmValue, bool store_values);

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
	{SEC_SERVER, "SERVER"},
	{SEC_DOMAIN, "DOMAIN"},
#ifdef HAVE_ADS
	{SEC_ADS, "ADS"},
#endif
	{-1, NULL}
};

static const struct enum_list enum_printing[] = {
	{PRINT_SYSV, "sysv"},
	{PRINT_AIX, "aix"},
	{PRINT_HPUX, "hpux"},
	{PRINT_BSD, "bsd"},
	{PRINT_QNX, "qnx"},
	{PRINT_PLP, "plp"},
	{PRINT_LPRNG, "lprng"},
	{PRINT_CUPS, "cups"},
	{PRINT_IPRINT, "iprint"},
	{PRINT_LPRNT, "nt"},
	{PRINT_LPROS2, "os2"},
#if defined(DEVELOPER) || defined(ENABLE_BUILD_FARM_HACKS)
	{PRINT_TEST, "test"},
	{PRINT_VLP, "vlp"},
#endif /* DEVELOPER */
	{-1, NULL}
};

static const struct enum_list enum_ldap_sasl_wrapping[] = {
	{0, "plain"},
	{ADS_AUTH_SASL_SIGN, "sign"},
	{ADS_AUTH_SASL_SEAL, "seal"},
	{-1, NULL}
};

static const struct enum_list enum_ldap_ssl[] = {
	{LDAP_SSL_OFF, "no"},
	{LDAP_SSL_OFF, "off"},
	{LDAP_SSL_START_TLS, "start tls"},
	{LDAP_SSL_START_TLS, "start_tls"},
	{-1, NULL}
};

/* LDAP Dereferencing Alias types */
#define SAMBA_LDAP_DEREF_NEVER		0
#define SAMBA_LDAP_DEREF_SEARCHING	1
#define SAMBA_LDAP_DEREF_FINDING	2
#define SAMBA_LDAP_DEREF_ALWAYS		3

static const struct enum_list enum_ldap_deref[] = {
	{SAMBA_LDAP_DEREF_NEVER, "never"},
	{SAMBA_LDAP_DEREF_SEARCHING, "searching"},
	{SAMBA_LDAP_DEREF_FINDING, "finding"},
	{SAMBA_LDAP_DEREF_ALWAYS, "always"},
	{-1, "auto"}
};

static const struct enum_list enum_ldap_passwd_sync[] = {
	{LDAP_PASSWD_SYNC_OFF, "no"},
	{LDAP_PASSWD_SYNC_OFF, "off"},
	{LDAP_PASSWD_SYNC_ON, "yes"},
	{LDAP_PASSWD_SYNC_ON, "on"},
	{LDAP_PASSWD_SYNC_ONLY, "only"},
	{-1, NULL}
};

/* Types of machine we can announce as. */
#define ANNOUNCE_AS_NT_SERVER 1
#define ANNOUNCE_AS_WIN95 2
#define ANNOUNCE_AS_WFW 3
#define ANNOUNCE_AS_NT_WORKSTATION 4

static const struct enum_list enum_announce_as[] = {
	{ANNOUNCE_AS_NT_SERVER, "NT"},
	{ANNOUNCE_AS_NT_SERVER, "NT Server"},
	{ANNOUNCE_AS_NT_WORKSTATION, "NT Workstation"},
	{ANNOUNCE_AS_WIN95, "win95"},
	{ANNOUNCE_AS_WFW, "WfW"},
	{-1, NULL}
};

static const struct enum_list enum_map_readonly[] = {
	{MAP_READONLY_NO, "no"},
	{MAP_READONLY_NO, "false"},
	{MAP_READONLY_NO, "0"},
	{MAP_READONLY_YES, "yes"},
	{MAP_READONLY_YES, "true"},
	{MAP_READONLY_YES, "1"},
	{MAP_READONLY_PERMISSIONS, "permissions"},
	{MAP_READONLY_PERMISSIONS, "perms"},
	{-1, NULL}
};

static const struct enum_list enum_case[] = {
	{CASE_LOWER, "lower"},
	{CASE_UPPER, "upper"},
	{-1, NULL}
};



static const struct enum_list enum_bool_auto[] = {
	{False, "No"},
	{False, "False"},
	{False, "0"},
	{True, "Yes"},
	{True, "True"},
	{True, "1"},
	{Auto, "Auto"},
	{-1, NULL}
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
	{False, "No"},
	{False, "False"},
	{False, "0"},
	{False, "Off"},
	{False, "disabled"},
	{True, "Yes"},
	{True, "True"},
	{True, "1"},
	{True, "On"},
	{True, "enabled"},
	{Auto, "auto"},
	{Required, "required"},
	{Required, "mandatory"},
	{Required, "force"},
	{Required, "forced"},
	{Required, "enforced"},
	{-1, NULL}
};

/* ACL compatibility options. */
static const struct enum_list enum_acl_compat_vals[] = {
    { ACL_COMPAT_AUTO, "auto" },
    { ACL_COMPAT_WINNT, "winnt" },
    { ACL_COMPAT_WIN2K, "win2k" },
    { -1, NULL}
};

/* 
   Do you want session setups at user level security with a invalid
   password to be rejected or allowed in as guest? WinNT rejects them
   but it can be a pain as it means "net view" needs to use a password

   You have 3 choices in the setting of map_to_guest:

   "Never" means session setups with an invalid password
   are rejected. This is the default.

   "Bad User" means session setups with an invalid password
   are rejected, unless the username does not exist, in which case it
   is treated as a guest login

   "Bad Password" means session setups with an invalid password
   are treated as a guest login

   Note that map_to_guest only has an effect in user or server
   level security.
*/

static const struct enum_list enum_map_to_guest[] = {
	{NEVER_MAP_TO_GUEST, "Never"},
	{MAP_TO_GUEST_ON_BAD_USER, "Bad User"},
	{MAP_TO_GUEST_ON_BAD_PASSWORD, "Bad Password"},
        {MAP_TO_GUEST_ON_BAD_UID, "Bad Uid"},
	{-1, NULL}
};

/* Config backend options */

static const struct enum_list enum_config_backend[] = {
	{CONFIG_BACKEND_FILE, "file"},
	{CONFIG_BACKEND_REGISTRY, "registry"},
	{-1, NULL}
};

/* ADS kerberos ticket verification options */

static const struct enum_list enum_kerberos_method[] = {
	{KERBEROS_VERIFY_SECRETS, "default"},
	{KERBEROS_VERIFY_SECRETS, "secrets only"},
	{KERBEROS_VERIFY_SYSTEM_KEYTAB, "system keytab"},
	{KERBEROS_VERIFY_DEDICATED_KEYTAB, "dedicated keytab"},
	{KERBEROS_VERIFY_SECRETS_AND_KEYTAB, "secrets and keytab"},
	{-1, NULL}
};

/* Note: We do not initialise the defaults union - it is not allowed in ANSI C
 *
 * The FLAG_HIDE is explicit. Parameters set this way do NOT appear in any edit
 * screen in SWAT. This is used to exclude parameters as well as to squash all
 * parameters that have been duplicated by pseudonyms.
 *
 * NOTE: To display a parameter in BASIC view set FLAG_BASIC
 *       Any parameter that does NOT have FLAG_ADVANCED will not disply at all
 *	 Set FLAG_SHARE and FLAG_PRINT to specifically display parameters in
 *        respective views.
 *
 * NOTE2: Handling of duplicated (synonym) parameters:
 *	Only the first occurance of a parameter should be enabled by FLAG_BASIC
 *	and/or FLAG_ADVANCED. All duplicates following the first mention should be
 *	set to FLAG_HIDE. ie: Make you must place the parameter that has the preferred
 *	name first, and all synonyms must follow it with the FLAG_HIDE attribute.
 */

static struct parm_struct parm_table[] = {
	{N_("Base Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "dos charset",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.dos_charset,
		.special	= handle_dos_charset,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED
	},
	{
		.label		= "unix charset",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.unix_charset,
		.special	= handle_charset,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED
	},
	{
		.label		= "display charset",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.display_charset,
		.special	= handle_charset,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED
	},
	{
		.label		= "comment",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.comment,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT
	},
	{
		.label		= "path",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szPath,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT,
	},
	{
		.label		= "directory",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szPath,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "workgroup",
		.type		= P_USTRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szWorkgroup,
		.special	= handle_workgroup,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_WIZARD,
	},
#ifdef WITH_ADS
	{
		.label		= "realm",
		.type		= P_USTRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szRealm,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_WIZARD,
	},
#endif
	{
		.label		= "netbios name",
		.type		= P_USTRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szNetbiosName,
		.special	= handle_netbios_name,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_WIZARD,
	},
	{
		.label		= "netbios aliases",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szNetbiosAliases,
		.special	= handle_netbios_aliases,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "netbios scope",
		.type		= P_USTRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szNetbiosScope,
		.special	= handle_netbios_scope,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "server string",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szServerString,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED,
	},
	{
		.label		= "interfaces",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szInterfaces,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_WIZARD,
	},
	{
		.label		= "bind interfaces only",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bBindInterfacesOnly,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_WIZARD,
	},
	{
		.label		= "config backend",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ConfigBackend,
		.special	= NULL,
		.enum_list	= enum_config_backend,
		.flags		= FLAG_HIDE|FLAG_ADVANCED|FLAG_META,
	},

	{N_("Security Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "security",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.security,
		.special	= NULL,
		.enum_list	= enum_security,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_WIZARD,
	},
	{
		.label		= "auth methods",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.AuthMethods,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "encrypt passwords",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bEncryptPasswords,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_WIZARD,
	},
	{
		.label		= "client schannel",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.clientSchannel,
		.special	= NULL,
		.enum_list	= enum_bool_auto,
		.flags		= FLAG_BASIC | FLAG_ADVANCED,
	},
	{
		.label		= "server schannel",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.serverSchannel,
		.special	= NULL,
		.enum_list	= enum_bool_auto,
		.flags		= FLAG_BASIC | FLAG_ADVANCED,
	},
	{
		.label		= "allow trusted domains",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bAllowTrustedDomains,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "map to guest",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.map_to_guest,
		.special	= NULL,
		.enum_list	= enum_map_to_guest,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "null passwords",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bNullPasswords,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_DEPRECATED,
	},
	{
		.label		= "obey pam restrictions",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bObeyPamRestrictions,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "password server",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szPasswordServer,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_WIZARD,
	},
	{
		.label		= "smb passwd file",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szSMBPasswdFile,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "private dir",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szPrivateDir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "passdb backend",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szPassdbBackend,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_WIZARD,
	},
	{
		.label		= "algorithmic rid base",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.AlgorithmicRidBase,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "root directory",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szRootdir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "root dir",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szRootdir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "root",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szRootdir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "guest account",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szGuestaccount,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED,
	},
	{
		.label		= "enable privileges",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bEnablePrivileges,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_DEPRECATED,
	},

	{
		.label		= "pam password change",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bPamPasswordChange,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "passwd program",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szPasswdProgram,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "passwd chat",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szPasswdChat,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "passwd chat debug",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bPasswdChatDebug,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "passwd chat timeout",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iPasswdChatTimeout,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "check password script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szCheckPasswordScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "username map",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szUsernameMap,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "password level",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.pwordlevel,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_DEPRECATED,
	},
	{
		.label		= "username level",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.unamelevel,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "unix password sync",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bUnixPasswdSync,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "restrict anonymous",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.restrict_anonymous,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "lanman auth",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bLanmanAuth,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ntlm auth",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bNTLMAuth,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "raw NTLMv2 auth",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bRawNTLMv2Auth,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "client NTLMv2 auth",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bClientNTLMv2Auth,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "client lanman auth",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bClientLanManAuth,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "client plaintext auth",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bClientPlaintextAuth,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "client use spnego principal",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.client_use_spnego_principal,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "send spnego principal",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.send_spnego_principal,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "username",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szUsername,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE | FLAG_DEPRECATED,
	},
	{
		.label		= "user",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szUsername,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "users",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szUsername,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "invalid users",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szInvalidUsers,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "valid users",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szValidUsers,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "admin users",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szAdminUsers,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "read list",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.readlist,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "write list",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.writelist,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "printer admin",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.printer_admin,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_PRINT | FLAG_DEPRECATED,
	},
	{
		.label		= "force user",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.force_user,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "force group",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.force_group,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "group",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.force_group,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "read only",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bRead_only,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "write ok",
		.type		= P_BOOLREV,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bRead_only,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "writeable",
		.type		= P_BOOLREV,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bRead_only,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "writable",
		.type		= P_BOOLREV,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bRead_only,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "acl check permissions",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bAclCheckPermissions,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "acl group control",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bAclGroupControl,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "acl map full control",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bAclMapFullControl,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "create mask",
		.type		= P_OCTAL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iCreate_mask,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "create mode",
		.type		= P_OCTAL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iCreate_mask,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "force create mode",
		.type		= P_OCTAL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iCreate_force_mode,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "security mask",
		.type		= P_OCTAL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iSecurity_mask,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "force security mode",
		.type		= P_OCTAL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iSecurity_force_mode,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "directory mask",
		.type		= P_OCTAL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iDir_mask,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "directory mode",
		.type		= P_OCTAL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iDir_mask,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "force directory mode",
		.type		= P_OCTAL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iDir_force_mode,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "directory security mask",
		.type		= P_OCTAL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iDir_Security_mask,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "force directory security mode",
		.type		= P_OCTAL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iDir_Security_force_mode,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "force unknown acl user",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bForceUnknownAclUser,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "inherit permissions",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bInheritPerms,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "inherit acls",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bInheritACLS,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "inherit owner",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bInheritOwner,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "guest only",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bGuest_only,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "only guest",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bGuest_only,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "administrative share",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bAdministrative_share,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT,
	},

	{
		.label		= "guest ok",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bGuest_ok,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT,
	},
	{
		.label		= "public",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bGuest_ok,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "only user",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bOnlyUser,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_DEPRECATED,
	},
	{
		.label		= "hosts allow",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szHostsallow,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_GLOBAL | FLAG_BASIC | FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT,
	},
	{
		.label		= "allow hosts",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szHostsallow,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "hosts deny",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szHostsdeny,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_GLOBAL | FLAG_BASIC | FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT,
	},
	{
		.label		= "deny hosts",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szHostsdeny,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "preload modules",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szPreloadModules,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "dedicated keytab file",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szDedicatedKeytabFile,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "kerberos method",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iKerberosMethod,
		.special	= NULL,
		.enum_list	= enum_kerberos_method,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "map untrusted to domain",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bMapUntrustedToDomain,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},


	{N_("Logging Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "log level",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLogLevel,
		.special	= handle_debug_list,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "debuglevel",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLogLevel,
		.special	= handle_debug_list,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "syslog",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.syslog,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "syslog only",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bSyslogOnly,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "log file",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLogFile,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "max log size",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.max_log_size,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "debug timestamp",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bTimestampLogs,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "timestamp logs",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bTimestampLogs,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "debug prefix timestamp",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bDebugPrefixTimestamp,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "debug hires timestamp",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bDebugHiresTimestamp,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "debug pid",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bDebugPid,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "debug uid",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bDebugUid,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "debug class",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bDebugClass,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "enable core files",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bEnableCoreFiles,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{N_("Protocol Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "allocation roundup size",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iallocation_roundup_size,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "aio read size",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iAioReadSize,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "aio write size",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iAioWriteSize,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "aio write behind",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szAioWriteBehind,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "smb ports",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.smb_ports,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "large readwrite",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bLargeReadwrite,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "max protocol",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.maxprotocol,
		.special	= NULL,
		.enum_list	= enum_protocol,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "protocol",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.maxprotocol,
		.special	= NULL,
		.enum_list	= enum_protocol,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "min protocol",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.minprotocol,
		.special	= NULL,
		.enum_list	= enum_protocol,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "min receivefile size",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iminreceivefile,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "read raw",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bReadRaw,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "write raw",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWriteRaw,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "disable netbios",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bDisableNetbios,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "reset on zero vc",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bResetOnZeroVC,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "log writeable files on exit",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bLogWriteableFilesOnExit,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "acl compatibility",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iAclCompat,
		.special	= NULL,
		.enum_list	= enum_acl_compat_vals,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "defer sharing violations",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bDeferSharingViolations,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "ea support",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bEASupport,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "nt acl support",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bNTAclSupport,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "nt pipe support",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bNTPipeSupport,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "nt status support",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bNTStatusSupport,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "profile acls",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bProfileAcls,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},
	{
		.label		= "announce version",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAnnounceVersion,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "announce as",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.announce_as,
		.special	= NULL,
		.enum_list	= enum_announce_as,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "map acl inherit",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bMap_acl_inherit,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "afs share",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bAfs_Share,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "max mux",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.max_mux,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "max xmit",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.max_xmit,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "name resolve order",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szNameResolveOrder,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_WIZARD,
	},
	{
		.label		= "max ttl",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.max_ttl,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "max wins ttl",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.max_wins_ttl,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "min wins ttl",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.min_wins_ttl,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "time server",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bTimeServer,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "unix extensions",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bUnixExtensions,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "allow dcerpc auth level connect",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bAllowDcerpcAuthLevelConnect,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "use spnego",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bUseSpnego,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_DEPRECATED,
	},
	{
		.label		= "client signing",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.client_signing,
		.special	= NULL,
		.enum_list	= enum_smb_signing_vals,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "client ipc signing",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.client_ipc_signing,
		.special	= NULL,
		.enum_list	= enum_smb_signing_vals,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "server signing",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.server_signing,
		.special	= NULL,
		.enum_list	= enum_smb_signing_vals,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "smb encrypt",
		.type		= P_ENUM,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.ismb_encrypt,
		.special	= NULL,
		.enum_list	= enum_smb_signing_vals,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "client use spnego",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bClientUseSpnego,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "client ldap sasl wrapping",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.client_ldap_sasl_wrapping,
		.special	= NULL,
		.enum_list	= enum_ldap_sasl_wrapping,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "enable asu support",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bASUSupport,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "svcctl list",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szServicesList,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{N_("Tuning Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "block size",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iBlock_size,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "deadtime",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.deadtime,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "getwd cache",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.getwd_cache,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "keepalive",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iKeepalive,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "change notify",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bChangeNotify,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "directory name cache size",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iDirectoryNameCacheSize,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "kernel change notify",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bKernelChangeNotify,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "lpq cache time",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.lpqcachetime,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "max smbd processes",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iMaxSmbdProcesses,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "max connections",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iMaxConnections,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "paranoid server security",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.paranoid_server_security,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "max disk size",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.maxdisksize,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "max open files",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.max_open_files,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "min print space",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iMinPrintSpace,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},
	{
		.label		= "socket options",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szSocketOptions,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "strict allocate",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bStrictAllocate,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "strict sync",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bStrictSync,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "sync always",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bSyncAlways,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "use mmap",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bUseMmap,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "use sendfile",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bUseSendfile,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "hostname lookups",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bHostnameLookups,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "write cache size",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iWriteCacheSize,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "name cache timeout",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.name_cache_timeout,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ctdbd socket",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ctdbdSocket,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "cluster addresses",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szClusterAddresses,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "clustering",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.clustering,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "ctdb timeout",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ctdb_timeout,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "ctdb locktime warn threshold",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ctdb_locktime_warn_threshold,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "smb2 max read",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ismb2_max_read,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "smb2 max write",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ismb2_max_write,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "smb2 max trans",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ismb2_max_trans,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "smb2 max credits",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ismb2_max_credits,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{N_("Printing Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "max reported print jobs",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iMaxReportedPrintJobs,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},
	{
		.label		= "max print jobs",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iMaxPrintJobs,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},
	{
		.label		= "load printers",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bLoadPrinters,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},
	{
		.label		= "printcap cache time",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.PrintcapCacheTime,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},
	{
		.label		= "printcap name",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szPrintcapname,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},
	{
		.label		= "printcap",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szPrintcapname,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "printable",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bPrint_ok,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},
	{
		.label		= "print notify backchannel",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bPrintNotifyBackchannel,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "print ok",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bPrint_ok,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "printing",
		.type		= P_ENUM,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iPrinting,
		.special	= handle_printing,
		.enum_list	= enum_printing,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label		= "cups options",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szCupsOptions,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label		= "cups server",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szCupsServer,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label          = "cups encrypt",
		.type           = P_ENUM,
		.p_class        = P_GLOBAL,
		.ptr            = &Globals.CupsEncrypt,
		.special        = NULL,
		.enum_list      = enum_bool_auto,
		.flags          = FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{

		.label		= "cups connection timeout",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.cups_connection_timeout,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "iprint server",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szIPrintServer,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label		= "print command",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szPrintcommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label		= "disable spoolss",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bDisableSpoolss,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label		= "enable spoolss",
		.type		= P_BOOLREV,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bDisableSpoolss,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "lpq command",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szLpqcommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label		= "lprm command",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szLprmcommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label		= "lppause command",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szLppausecommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label		= "lpresume command",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szLpresumecommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label		= "queuepause command",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szQueuepausecommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label		= "queueresume command",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szQueueresumecommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT | FLAG_GLOBAL,
	},
	{
		.label		= "addport command",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAddPortCommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "enumports command",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szEnumPortsCommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "addprinter command",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAddPrinterCommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "deleteprinter command",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szDeletePrinterCommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "show add printer wizard",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bMsAddPrinterWizard,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "os2 driver map",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szOs2DriverMap,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{
		.label		= "printer name",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szPrintername,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},
	{
		.label		= "printer",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szPrintername,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "use client driver",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bUseClientDriver,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},
	{
		.label		= "default devmode",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bDefaultDevmode,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},
	{
		.label		= "force printername",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bForcePrintername,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},
	{
		.label		= "printjob username",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szPrintjobUsername,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_PRINT,
	},

	{N_("Filename Handling"), P_SEP, P_SEPARATOR},

	{
		.label		= "mangling method",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szManglingMethod,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "mangle prefix",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.mangle_prefix,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{
		.label		= "default case",
		.type		= P_ENUM,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iDefaultCase,
		.special	= NULL,
		.enum_list	= enum_case,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "case sensitive",
		.type		= P_ENUM,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iCaseSensitive,
		.special	= NULL,
		.enum_list	= enum_bool_auto,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "casesignames",
		.type		= P_ENUM,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iCaseSensitive,
		.special	= NULL,
		.enum_list	= enum_bool_auto,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL | FLAG_HIDE,
	},
	{
		.label		= "preserve case",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bCasePreserve,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "short preserve case",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bShortCasePreserve,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "mangling char",
		.type		= P_CHAR,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.magic_char,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "hide dot files",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bHideDotFiles,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "hide special files",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bHideSpecialFiles,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "hide unreadable",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bHideUnReadable,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "hide unwriteable files",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bHideUnWriteableFiles,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "delete veto files",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bDeleteVetoFiles,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "veto files",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szVetoFiles,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "hide files",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szHideFiles,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "veto oplock files",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szVetoOplockFiles,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "map archive",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bMap_archive,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "map hidden",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bMap_hidden,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "map system",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bMap_system,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "map readonly",
		.type		= P_ENUM,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iMap_readonly,
		.special	= NULL,
		.enum_list	= enum_map_readonly,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "mangled names",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bMangledNames,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "max stat cache size",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iMaxStatCacheSize,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "stat cache",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bStatCache,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "store dos attributes",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bStoreDosAttributes,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "dmapi support",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bDmapiSupport,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},


	{N_("Domain Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "machine password timeout",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.machine_password_timeout,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_WIZARD,
	},

	{N_("Logon Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "add user script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAddUserScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "rename user script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szRenameUserScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "delete user script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szDelUserScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "add group script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAddGroupScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "delete group script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szDelGroupScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "add user to group script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAddUserToGroupScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "delete user from group script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szDelUserFromGroupScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "set primary group script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szSetPrimaryGroupScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "add machine script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAddMachineScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "shutdown script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szShutdownScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "abort shutdown script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAbortShutdownScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "username map script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szUsernameMapScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "username map cache time",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iUsernameMapCacheTime,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "logon script",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLogonScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "logon path",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLogonPath,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "logon drive",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLogonDrive,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "logon home",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLogonHome,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "domain logons",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bDomainLogons,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{
		.label		= "init logon delayed hosts",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szInitLogonDelayedHosts,
		.special        = NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{
		.label		= "init logon delay",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.InitLogonDelay,
		.special        = NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,

	},

	{N_("Browse Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "os level",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.os_level,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED,
	},
	{
		.label		= "lm announce",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.lm_announce,
		.special	= NULL,
		.enum_list	= enum_bool_auto,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "lm interval",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.lm_interval,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "preferred master",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iPreferredMaster,
		.special	= NULL,
		.enum_list	= enum_bool_auto,
		.flags		= FLAG_BASIC | FLAG_ADVANCED,
	},
	{
		.label		= "prefered master",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iPreferredMaster,
		.special	= NULL,
		.enum_list	= enum_bool_auto,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "local master",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bLocalMaster,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED,
	},
	{
		.label		= "domain master",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iDomainMaster,
		.special	= NULL,
		.enum_list	= enum_bool_auto,
		.flags		= FLAG_BASIC | FLAG_ADVANCED,
	},
	{
		.label		= "browse list",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bBrowseList,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "browseable",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bBrowseable,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT,
	},
	{
		.label		= "browsable",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bBrowseable,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "access based share enum",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bAccessBasedShareEnum,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_SHARE
	},
	{
		.label		= "enhanced browsing",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.enhanced_browsing,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{N_("WINS Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "dns proxy",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bDNSproxy,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "wins proxy",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWINSproxy,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "wins server",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szWINSservers,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_WIZARD,
	},
	{
		.label		= "wins support",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWINSsupport,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_WIZARD,
	},
	{
		.label		= "wins hook",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szWINSHook,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{N_("Locking Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "blocking locks",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bBlockingLocks,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "csc policy",
		.type		= P_ENUM,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iCSCPolicy,
		.special	= NULL,
		.enum_list	= enum_csc_policy,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "fake oplocks",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bFakeOplocks,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "kernel oplocks",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bKernelOplocks,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "locking",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bLocking,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "lock spin time",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iLockSpinTime,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "oplocks",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bOpLocks,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "level2 oplocks",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bLevel2OpLocks,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "oplock break wait time",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.oplock_break_wait_time,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "oplock contention limit",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iOplockContentionLimit,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "posix locking",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bPosixLocking,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "strict locking",
		.type		= P_ENUM,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iStrictLocking,
		.special	= NULL,
		.enum_list	= enum_bool_auto,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "share modes",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bShareModes,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL | FLAG_DEPRECATED,
	},

	{N_("Ldap Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "ldap admin dn",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLdapAdminDn,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap delete dn",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_delete_dn,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap group suffix",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLdapGroupSuffix,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap idmap suffix",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLdapIdmapSuffix,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap machine suffix",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLdapMachineSuffix,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap passwd sync",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_passwd_sync,
		.special	= NULL,
		.enum_list	= enum_ldap_passwd_sync,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap password sync",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_passwd_sync,
		.special	= NULL,
		.enum_list	= enum_ldap_passwd_sync,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "ldap replication sleep",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_replication_sleep,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap suffix",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLdapSuffix,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap ssl",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_ssl,
		.special	= NULL,
		.enum_list	= enum_ldap_ssl,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap ssl ads",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_ssl_ads,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap deref",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_deref,
		.special	= NULL,
		.enum_list	= enum_ldap_deref,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap follow referral",
		.type		= P_ENUM,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_follow_referral,
		.special	= NULL,
		.enum_list	= enum_bool_auto,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap timeout",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_timeout,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap connection timeout",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_connection_timeout,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap page size",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_page_size,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap user suffix",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLdapUserSuffix,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap debug level",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_debug_level,
		.special	= handle_ldap_debug_level,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ldap debug threshold",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ldap_debug_threshold,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{N_("EventLog Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "eventlog list",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szEventLogs,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL | FLAG_SHARE,
	},

	{N_("Miscellaneous Options"), P_SEP, P_SEPARATOR},

	{
		.label		= "add share command",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAddShareCommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "change share command",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szChangeShareCommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "delete share command",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szDeleteShareCommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "config file",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szConfigFile,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE|FLAG_META,
	},
	{
		.label		= "preload",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAutoServices,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "auto services",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAutoServices,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "lock directory",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLockDir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "lock dir",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLockDir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "state directory",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szStateDir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "cache directory",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szCacheDir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "pid directory",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szPidDir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
#ifdef WITH_UTMP
	{
		.label		= "utmp directory",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szUtmpDir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "wtmp directory",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szWtmpDir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "utmp",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bUtmp,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
#endif
	{
		.label		= "default service",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szDefaultService,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "default",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szDefaultService,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "message command",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szMsgCommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "dfree cache time",
		.type		= P_INTEGER,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.iDfreeCacheTime,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "dfree command",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szDfree,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "get quota command",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szGetQuota,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "set quota command",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szSetQuota,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "remote announce",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szRemoteAnnounce,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "remote browse sync",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szRemoteBrowseSync,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "socket address",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szSocketAddress,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "nmbd bind explicit broadcast",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bNmbdBindExplicitBroadcast,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "homedir map",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szNISHomeMapName,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "afs username map",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szAfsUsernameMap,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "afs token lifetime",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iAfsTokenLifetime,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "log nt token command",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szLogNtTokenCommand,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "time offset",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &extra_time_offset,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_DEPRECATED,
	},
	{
		.label		= "NIS homedir",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bNISHomeMap,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "-valid",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.valid,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "copy",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szCopy,
		.special	= handle_copy,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "include",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szInclude,
		.special	= handle_include,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE|FLAG_META,
	},
	{
		.label		= "preexec",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szPreExec,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT,
	},
	{
		.label		= "exec",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szPreExec,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "preexec close",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bPreexecClose,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "postexec",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szPostExec,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT,
	},
	{
		.label		= "root preexec",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szRootPreExec,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT,
	},
	{
		.label		= "root preexec close",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bRootpreexecClose,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "root postexec",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szRootPostExec,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT,
	},
	{
		.label		= "available",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bAvailable,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_BASIC | FLAG_ADVANCED | FLAG_SHARE | FLAG_PRINT,
	},
	{
		.label		= "registry shares",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bRegistryShares,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "usershare allow guests",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bUsershareAllowGuests,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "usershare max shares",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iUsershareMaxShares,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "usershare owner only",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bUsershareOwnerOnly,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "usershare path",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szUsersharePath,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "usershare prefix allow list",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szUsersharePrefixAllowList,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "usershare prefix deny list",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szUsersharePrefixDenyList,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "usershare template share",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szUsershareTemplateShare,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "volume",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.volume,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "fstype",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.fstype,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "set directory",
		.type		= P_BOOLREV,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bNo_set_dir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "allow insecure wide links",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bAllowInsecureWidelinks,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "wide links",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bWidelinks,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "follow symlinks",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bSymlinks,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "dont descend",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szDontdescend,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "magic script",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szMagicScript,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "magic output",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szMagicOutput,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "delete readonly",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bDeleteReadonly,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "dos filemode",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bDosFilemode,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "dos filetimes",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bDosFiletimes,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "dos filetime resolution",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bDosFiletimeResolution,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE | FLAG_GLOBAL,
	},
	{
		.label		= "fake directory create times",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bFakeDirCreateTimes,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "async smb echo handler",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bAsyncSMBEchoHandler,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "multicast dns register",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bMulticastDnsRegister,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_GLOBAL,
	},
	{
		.label		= "panic action",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szPanicAction,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "perfcount module",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szSMBPerfcountModule,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{N_("VFS module options"), P_SEP, P_SEPARATOR},

	{
		.label		= "vfs objects",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szVfsObjects,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "vfs object",
		.type		= P_LIST,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szVfsObjects,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},


	{N_("MSDFS options"), P_SEP, P_SEPARATOR},

	{
		.label		= "msdfs root",
		.type		= P_BOOL,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.bMSDfsRoot,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "msdfs proxy",
		.type		= P_STRING,
		.p_class	= P_LOCAL,
		.ptr		= &sDefault.szMSDfsProxy,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_SHARE,
	},
	{
		.label		= "host msdfs",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bHostMSDfs,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{N_("Winbind options"), P_SEP, P_SEPARATOR},

	{
		.label		= "passdb expand explicit",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bPassdbExpandExplicit,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "idmap backend",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szIdmapBackend,
		.special	= handle_idmap_backend,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_DEPRECATED,
	},
	{
		.label		= "idmap cache time",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iIdmapCacheTime,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "idmap negative cache time",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.iIdmapNegativeCacheTime,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "idmap uid",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szIdmapUID,
		.special	= handle_idmap_uid,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_DEPRECATED,
	},
	{
		.label		= "winbind uid",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szIdmapUID,
		.special	= handle_idmap_uid,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "idmap gid",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szIdmapGID,
		.special	= handle_idmap_gid,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED | FLAG_DEPRECATED,
	},
	{
		.label		= "winbind gid",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szIdmapGID,
		.special	= handle_idmap_gid,
		.enum_list	= NULL,
		.flags		= FLAG_HIDE,
	},
	{
		.label		= "template homedir",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szTemplateHomedir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "template shell",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szTemplateShell,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind separator",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szWinbindSeparator,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind cache time",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.winbind_cache_time,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind reconnect delay",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.winbind_reconnect_delay,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind max clients",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.winbind_max_clients,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind enum users",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWinbindEnumUsers,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind enum groups",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWinbindEnumGroups,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind use default domain",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWinbindUseDefaultDomain,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind trusted domains only",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWinbindTrustedDomainsOnly,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind nested groups",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWinbindNestedGroups,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind expand groups",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.winbind_expand_groups,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind nss info",
		.type		= P_LIST,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.szWinbindNssInfo,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind refresh tickets",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWinbindRefreshTickets,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind offline logon",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWinbindOfflineLogon,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind sealed pipes",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWinbindSealedPipes,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind normalize names",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWinbindNormalizeNames,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind rpc only",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bWinbindRpcOnly,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "create krb5 conf",
		.type		= P_BOOL,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.bCreateKrb5Conf,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "ncalrpc dir",
		.type		= P_STRING,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.ncalrpc_dir,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},
	{
		.label		= "winbind max domain connections",
		.type		= P_INTEGER,
		.p_class	= P_GLOBAL,
		.ptr		= &Globals.winbindMaxDomainConnections,
		.special	= NULL,
		.enum_list	= NULL,
		.flags		= FLAG_ADVANCED,
	},

	{NULL,  P_BOOL,  P_NONE,  NULL,  NULL,  NULL,  0}
};

/***************************************************************************
 Initialise the sDefault parameter structure for the printer values.
***************************************************************************/

static void init_printer_values(struct service *pService)
{
	/* choose defaults depending on the type of printing */
	switch (pService->iPrinting) {
		case PRINT_BSD:
		case PRINT_AIX:
		case PRINT_LPRNT:
		case PRINT_LPROS2:
			string_set(&pService->szLpqcommand, "lpq -P'%p'");
			string_set(&pService->szLprmcommand, "lprm -P'%p' %j");
			string_set(&pService->szPrintcommand, "lpr -r -P'%p' %s");
			break;

		case PRINT_LPRNG:
		case PRINT_PLP:
			string_set(&pService->szLpqcommand, "lpq -P'%p'");
			string_set(&pService->szLprmcommand, "lprm -P'%p' %j");
			string_set(&pService->szPrintcommand, "lpr -r -P'%p' %s");
			string_set(&pService->szQueuepausecommand, "lpc stop '%p'");
			string_set(&pService->szQueueresumecommand, "lpc start '%p'");
			string_set(&pService->szLppausecommand, "lpc hold '%p' %j");
			string_set(&pService->szLpresumecommand, "lpc release '%p' %j");
			break;

		case PRINT_CUPS:
		case PRINT_IPRINT:
#ifdef HAVE_CUPS
			/* set the lpq command to contain the destination printer
			   name only.  This is used by cups_queue_get() */
			string_set(&pService->szLpqcommand, "%p");
			string_set(&pService->szLprmcommand, "");
			string_set(&pService->szPrintcommand, "");
			string_set(&pService->szLppausecommand, "");
			string_set(&pService->szLpresumecommand, "");
			string_set(&pService->szQueuepausecommand, "");
			string_set(&pService->szQueueresumecommand, "");
#else
			string_set(&pService->szLpqcommand, "lpq -P'%p'");
			string_set(&pService->szLprmcommand, "lprm -P'%p' %j");
			string_set(&pService->szPrintcommand, "lpr -P'%p' %s; rm %s");
			string_set(&pService->szLppausecommand, "lp -i '%p-%j' -H hold");
			string_set(&pService->szLpresumecommand, "lp -i '%p-%j' -H resume");
			string_set(&pService->szQueuepausecommand, "disable '%p'");
			string_set(&pService->szQueueresumecommand, "enable '%p'");
#endif /* HAVE_CUPS */
			break;

		case PRINT_SYSV:
		case PRINT_HPUX:
			string_set(&pService->szLpqcommand, "lpstat -o%p");
			string_set(&pService->szLprmcommand, "cancel %p-%j");
			string_set(&pService->szPrintcommand, "lp -c -d%p %s; rm %s");
			string_set(&pService->szQueuepausecommand, "disable %p");
			string_set(&pService->szQueueresumecommand, "enable %p");
#ifndef HPUX
			string_set(&pService->szLppausecommand, "lp -i %p-%j -H hold");
			string_set(&pService->szLpresumecommand, "lp -i %p-%j -H resume");
#endif /* HPUX */
			break;

		case PRINT_QNX:
			string_set(&pService->szLpqcommand, "lpq -P%p");
			string_set(&pService->szLprmcommand, "lprm -P%p %j");
			string_set(&pService->szPrintcommand, "lp -r -P%p %s");
			break;

#if defined(DEVELOPER) || defined(ENABLE_BUILD_FARM_HACKS)

	case PRINT_TEST:
	case PRINT_VLP: {
		const char *tdbfile;
		char *tmp;

		tdbfile = talloc_asprintf(
			talloc_tos(), "tdbfile=%s",
			lp_parm_const_string(-1, "vlp", "tdbfile",
					     "/tmp/vlp.tdb"));
		if (tdbfile == NULL) {
			tdbfile="tdbfile=/tmp/vlp.tdb";
		}

		tmp = talloc_asprintf(talloc_tos(), "vlp %s print %%p %%s",
				      tdbfile);
		string_set(&pService->szPrintcommand,
			   tmp ? tmp : "vlp print %p %s");
		TALLOC_FREE(tmp);

		tmp = talloc_asprintf(talloc_tos(), "vlp %s lpq %%p",
				      tdbfile);
		string_set(&pService->szLpqcommand,
			   tmp ? tmp : "vlp lpq %p");
		TALLOC_FREE(tmp);

		tmp = talloc_asprintf(talloc_tos(), "vlp %s lprm %%p %%j",
				      tdbfile);
		string_set(&pService->szLprmcommand,
			   tmp ? tmp : "vlp lprm %p %j");
		TALLOC_FREE(tmp);

		tmp = talloc_asprintf(talloc_tos(), "vlp %s lppause %%p %%j",
				      tdbfile);
		string_set(&pService->szLppausecommand,
			   tmp ? tmp : "vlp lppause %p %j");
		TALLOC_FREE(tmp);

		tmp = talloc_asprintf(talloc_tos(), "vlp %s lpresume %%p %%j",
				      tdbfile);
		string_set(&pService->szLpresumecommand,
			   tmp ? tmp : "vlp lpresume %p %j");
		TALLOC_FREE(tmp);

		tmp = talloc_asprintf(talloc_tos(), "vlp %s queuepause %%p",
				      tdbfile);
		string_set(&pService->szQueuepausecommand,
			   tmp ? tmp : "vlp queuepause %p");
		TALLOC_FREE(tmp);

		tmp = talloc_asprintf(talloc_tos(), "vlp %s queueresume %%p",
				      tdbfile);
		string_set(&pService->szQueueresumecommand,
			   tmp ? tmp : "vlp queueresume %p");
		TALLOC_FREE(tmp);

		break;
	}
#endif /* DEVELOPER */

	}
}
/**
 *  Function to return the default value for the maximum number of open
 *  file descriptors permitted.  This function tries to consult the
 *  kernel-level (sysctl) and ulimit (getrlimit()) values and goes
 *  the smaller of those.
 */
static int max_open_files(void)
{
	int sysctl_max = MAX_OPEN_FILES;
	int rlimit_max = MAX_OPEN_FILES;

#ifdef HAVE_SYSCTLBYNAME
	{
		size_t size = sizeof(sysctl_max);
		sysctlbyname("kern.maxfilesperproc", &sysctl_max, &size, NULL,
			     0);
	}
#endif

#if (defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE))
	{
		struct rlimit rl;

		ZERO_STRUCT(rl);

		if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
			rlimit_max = rl.rlim_cur;

#if defined(RLIM_INFINITY)
		if(rl.rlim_cur == RLIM_INFINITY)
			rlimit_max = MAX_OPEN_FILES;
#endif
	}
#endif

	if (sysctl_max < MIN_OPEN_FILES_WINDOWS) {
		DEBUG(2,("max_open_files: increasing sysctl_max (%d) to "
			"minimum Windows limit (%d)\n",
			sysctl_max,
			MIN_OPEN_FILES_WINDOWS));
		sysctl_max = MIN_OPEN_FILES_WINDOWS;
	}

	if (rlimit_max < MIN_OPEN_FILES_WINDOWS) {
		DEBUG(2,("rlimit_max: increasing rlimit_max (%d) to "
			"minimum Windows limit (%d)\n",
			rlimit_max,
			MIN_OPEN_FILES_WINDOWS));
		rlimit_max = MIN_OPEN_FILES_WINDOWS;
	}

	return MIN(sysctl_max, rlimit_max);
}

/**
 * Common part of freeing allocated data for one parameter.
 */
static void free_one_parameter_common(void *parm_ptr,
				      struct parm_struct parm)
{
	if ((parm.type == P_STRING) ||
	    (parm.type == P_USTRING))
	{
		string_free((char**)parm_ptr);
	} else if (parm.type == P_LIST) {
		TALLOC_FREE(*((char***)parm_ptr));
	}
}

/**
 * Free the allocated data for one parameter for a share
 * given as a service struct.
 */
static void free_one_parameter(struct service *service,
			       struct parm_struct parm)
{
	void *parm_ptr;

	if (parm.p_class != P_LOCAL) {
		return;
	}

	parm_ptr = lp_local_ptr(service, parm.ptr);

	free_one_parameter_common(parm_ptr, parm);
}

/**
 * Free the allocated parameter data of a share given
 * as a service struct.
 */
static void free_parameters(struct service *service)
{
	uint32_t i;

	for (i=0; parm_table[i].label; i++) {
		free_one_parameter(service, parm_table[i]);
	}
}

/**
 * Free the allocated data for one parameter for a given share
 * specified by an snum.
 */
static void free_one_parameter_by_snum(int snum, struct parm_struct parm)
{
	void *parm_ptr;

	if (parm.ptr == NULL) {
		return;
	}

	if (snum < 0) {
		parm_ptr = parm.ptr;
	} else if (parm.p_class != P_LOCAL) {
		return;
	} else {
		parm_ptr = lp_local_ptr_by_snum(snum, parm.ptr);
	}

	free_one_parameter_common(parm_ptr, parm);
}

/**
 * Free the allocated parameter data for a share specified
 * by an snum.
 */
static void free_parameters_by_snum(int snum)
{
	uint32_t i;

	for (i=0; parm_table[i].label; i++) {
		free_one_parameter_by_snum(snum, parm_table[i]);
	}
}

/**
 * Free the allocated global parameters.
 */
static void free_global_parameters(void)
{
	free_parameters_by_snum(GLOBAL_SECTION_SNUM);
}

static int map_parameter(const char *pszParmName);

struct lp_stored_option {
	struct lp_stored_option *prev, *next;
	const char *label;
	const char *value;
};

static struct lp_stored_option *stored_options;

/*
  save options set by lp_set_cmdline() into a list. This list is
  re-applied when we do a globals reset, so that cmdline set options
  are sticky across reloads of smb.conf
 */
static bool store_lp_set_cmdline(const char *pszParmName, const char *pszParmValue)
{
	struct lp_stored_option *entry, *entry_next;
	for (entry = stored_options; entry != NULL; entry = entry_next) {
		entry_next = entry->next;
		if (strcmp(pszParmName, entry->label) == 0) {
			DLIST_REMOVE(stored_options, entry);
			talloc_free(entry);
			break;
		}
	}

	entry = talloc(NULL, struct lp_stored_option);
	if (!entry) {
		return false;
	}

	entry->label = talloc_strdup(entry, pszParmName);
	if (!entry->label) {
		talloc_free(entry);
		return false;
	}

	entry->value = talloc_strdup(entry, pszParmValue);
	if (!entry->value) {
		talloc_free(entry);
		return false;
	}

	DLIST_ADD_END(stored_options, entry, struct lp_stored_option);

	return true;
}

static bool apply_lp_set_cmdline(void)
{
	struct lp_stored_option *entry = NULL;
	for (entry = stored_options; entry != NULL; entry = entry->next) {
		if (!lp_set_cmdline_helper(entry->label, entry->value, false)) {
			DEBUG(0, ("Failed to re-apply cmdline parameter %s = %s\n",
				  entry->label, entry->value));
			return false;
		}
	}
	return true;
}

/***************************************************************************
 Initialise the global parameter structure.
***************************************************************************/

static void init_globals(bool reinit_globals)
{
	static bool done_init = False;
	char *s = NULL;
	int i;

        /* If requested to initialize only once and we've already done it... */
        if (!reinit_globals && done_init) {
                /* ... then we have nothing more to do */
                return;
        }

	if (!done_init) {
		/* The logfile can be set before this is invoked. Free it if so. */
		if (Globals.szLogFile != NULL) {
			string_free(&Globals.szLogFile);
			Globals.szLogFile = NULL;
		}
		done_init = True;
	} else {
		free_global_parameters();
	}

	/* This memset and the free_global_parameters() above will
	 * wipe out smb.conf options set with lp_set_cmdline().  The
	 * apply_lp_set_cmdline() call puts these values back in the
	 * table once the defaults are set */
	memset((void *)&Globals, '\0', sizeof(Globals));

	for (i = 0; parm_table[i].label; i++) {
		if ((parm_table[i].type == P_STRING ||
		     parm_table[i].type == P_USTRING) &&
		    parm_table[i].ptr)
		{
			string_set((char **)parm_table[i].ptr, "");
		}
	}

	string_set(&sDefault.fstype, FSTYPE_STRING);
	string_set(&sDefault.szPrintjobUsername, "%U");

	init_printer_values(&sDefault);


	DEBUG(3, ("Initialising global parameters\n"));

	string_set(&Globals.szSMBPasswdFile, get_dyn_SMB_PASSWD_FILE());
	string_set(&Globals.szPrivateDir, get_dyn_PRIVATE_DIR());

	/* use the new 'hash2' method by default, with a prefix of 1 */
	string_set(&Globals.szManglingMethod, "hash2");
	Globals.mangle_prefix = 1;

	string_set(&Globals.szGuestaccount, GUEST_ACCOUNT);

	/* using UTF8 by default allows us to support all chars */
	string_set(&Globals.unix_charset, DEFAULT_UNIX_CHARSET);

#if defined(HAVE_NL_LANGINFO) && defined(CODESET)
	/* If the system supports nl_langinfo(), try to grab the value
	   from the user's locale */
	string_set(&Globals.display_charset, "LOCALE");
#else
	string_set(&Globals.display_charset, DEFAULT_DISPLAY_CHARSET);
#endif

	/* Use codepage 850 as a default for the dos character set */
	string_set(&Globals.dos_charset, DEFAULT_DOS_CHARSET);

	/*
	 * Allow the default PASSWD_CHAT to be overridden in local.h.
	 */
	string_set(&Globals.szPasswdChat, DEFAULT_PASSWD_CHAT);

	set_global_myname(myhostname());
	string_set(&Globals.szNetbiosName,global_myname());

	set_global_myworkgroup(WORKGROUP);
	string_set(&Globals.szWorkgroup, lp_workgroup());

	string_set(&Globals.szPasswdProgram, "");
	string_set(&Globals.szLockDir, get_dyn_LOCKDIR());
	string_set(&Globals.szStateDir, get_dyn_STATEDIR());
	string_set(&Globals.szCacheDir, get_dyn_CACHEDIR());
	string_set(&Globals.szPidDir, get_dyn_PIDDIR());
	string_set(&Globals.szSocketAddress, "0.0.0.0");
	/*
	 * By default support explicit binding to broadcast
 	 * addresses.
 	 */
	Globals.bNmbdBindExplicitBroadcast = true;

	if (asprintf(&s, "Samba %s", samba_version_string()) < 0) {
		smb_panic("init_globals: ENOMEM");
	}
	string_set(&Globals.szServerString, s);
	SAFE_FREE(s);
	if (asprintf(&s, "%d.%d", DEFAULT_MAJOR_VERSION,
			DEFAULT_MINOR_VERSION) < 0) {
		smb_panic("init_globals: ENOMEM");
	}
	string_set(&Globals.szAnnounceVersion, s);
	SAFE_FREE(s);
#ifdef DEVELOPER
	string_set(&Globals.szPanicAction, "/bin/sleep 999999999");
#endif

	string_set(&Globals.szSocketOptions, DEFAULT_SOCKET_OPTIONS);

	string_set(&Globals.szLogonDrive, "");
	/* %N is the NIS auto.home server if -DAUTOHOME is used, else same as %L */
	string_set(&Globals.szLogonHome, "\\\\%N\\%U");
	string_set(&Globals.szLogonPath, "\\\\%N\\%U\\profile");

	string_set(&Globals.szNameResolveOrder, "lmhosts wins host bcast");
	string_set(&Globals.szPasswordServer, "*");

	Globals.AlgorithmicRidBase = BASE_RID;

	Globals.bLoadPrinters = True;
	Globals.PrintcapCacheTime = 750; 	/* 12.5 minutes */

	Globals.ConfigBackend = config_backend;

	/* Was 65535 (0xFFFF). 0x4101 matches W2K and causes major speed improvements... */
	/* Discovered by 2 days of pain by Don McCall @ HP :-). */
	Globals.max_xmit = 0x4104;
	Globals.max_mux = 50;	/* This is *needed* for profile support. */
	Globals.lpqcachetime = 30;	/* changed to handle large print servers better -- jerry */
	Globals.bDisableSpoolss = False;
	Globals.iMaxSmbdProcesses = 0;/* no limit specified */
	Globals.pwordlevel = 0;
	Globals.unamelevel = 0;
	Globals.deadtime = 0;
	Globals.getwd_cache = true;
	Globals.bLargeReadwrite = True;
	Globals.max_log_size = 5000;
	Globals.max_open_files = max_open_files();
	Globals.open_files_db_hash_size = SMB_OPEN_DATABASE_TDB_HASH_SIZE;
	Globals.maxprotocol = PROTOCOL_NT1;
	Globals.minprotocol = PROTOCOL_CORE;
	Globals.security = SEC_USER;
	Globals.paranoid_server_security = True;
	Globals.bEncryptPasswords = True;
	Globals.bUpdateEncrypt = False;
	Globals.clientSchannel = Auto;
	Globals.serverSchannel = Auto;
	Globals.bReadRaw = True;
	Globals.bWriteRaw = True;
	Globals.bNullPasswords = False;
	Globals.bObeyPamRestrictions = False;
	Globals.syslog = 1;
	Globals.bSyslogOnly = False;
	Globals.bTimestampLogs = True;
	string_set(&Globals.szLogLevel, "0");
	Globals.bDebugPrefixTimestamp = False;
	Globals.bDebugHiresTimestamp = true;
	Globals.bDebugPid = False;
	Globals.bDebugUid = False;
	Globals.bDebugClass = False;
	Globals.bEnableCoreFiles = True;
	Globals.max_ttl = 60 * 60 * 24 * 3;	/* 3 days default. */
	Globals.max_wins_ttl = 60 * 60 * 24 * 6;	/* 6 days default. */
	Globals.min_wins_ttl = 60 * 60 * 6;	/* 6 hours default. */
	Globals.machine_password_timeout = 60 * 60 * 24 * 7;	/* 7 days default. */
	Globals.lm_announce = 2;	/* = Auto: send only if LM clients found */
	Globals.lm_interval = 60;
	Globals.announce_as = ANNOUNCE_AS_NT_SERVER;
#if (defined(HAVE_NETGROUP) && defined(WITH_AUTOMOUNT))
	Globals.bNISHomeMap = False;
#ifdef WITH_NISPLUS_HOME
	string_set(&Globals.szNISHomeMapName, "auto_home.org_dir");
#else
	string_set(&Globals.szNISHomeMapName, "auto.home");
#endif
#endif
	Globals.bTimeServer = False;
	Globals.bBindInterfacesOnly = False;
	Globals.bUnixPasswdSync = False;
	Globals.bPamPasswordChange = False;
	Globals.bPasswdChatDebug = False;
	Globals.iPasswdChatTimeout = 2; /* 2 second default. */
	Globals.bNTPipeSupport = True;	/* Do NT pipes by default. */
	Globals.bNTStatusSupport = True; /* Use NT status by default. */
	Globals.bStatCache = True;	/* use stat cache by default */
	Globals.iMaxStatCacheSize = 256; /* 256k by default */
	Globals.restrict_anonymous = 0;
	Globals.bClientLanManAuth = False;	/* Do NOT use the LanMan hash if it is available */
	Globals.bClientPlaintextAuth = False;	/* Do NOT use a plaintext password even if is requested by the server */
	Globals.bLanmanAuth = False;	/* Do NOT use the LanMan hash, even if it is supplied */
	Globals.bNTLMAuth = True;	/* Do use NTLMv1 if it is supplied by the client (otherwise NTLMv2) */
	Globals.bRawNTLMv2Auth = false;	/* Allow NTLMv2 without NTLMSSP */
	Globals.bClientNTLMv2Auth = True; /* Client should always use use NTLMv2, as we can't tell that the server supports it, but most modern servers do */
	/* Note, that we will also use NTLM2 session security (which is different), if it is available */

	Globals.bAllowDcerpcAuthLevelConnect = false; /* we don't allow this by default */

	Globals.map_to_guest = 0;	/* By Default, "Never" */
	Globals.oplock_break_wait_time = 0;	/* By Default, 0 msecs. */
	Globals.enhanced_browsing = true;
	Globals.iLockSpinTime = WINDOWS_MINIMUM_LOCK_TIMEOUT_MS; /* msec. */
#ifdef MMAP_BLACKLIST
	Globals.bUseMmap = False;
#else
	Globals.bUseMmap = True;
#endif
	Globals.bUnixExtensions = True;
	Globals.bResetOnZeroVC = False;
	Globals.bLogWriteableFilesOnExit = False;
	Globals.bCreateKrb5Conf = true;
	Globals.winbindMaxDomainConnections = 1;

	/* hostname lookups can be very expensive and are broken on
	   a large number of sites (tridge) */
	Globals.bHostnameLookups = False;

	string_set(&Globals.szPassdbBackend, "tdbsam");
	string_set(&Globals.szLdapSuffix, "");
	string_set(&Globals.szLdapMachineSuffix, "");
	string_set(&Globals.szLdapUserSuffix, "");
	string_set(&Globals.szLdapGroupSuffix, "");
	string_set(&Globals.szLdapIdmapSuffix, "");

	string_set(&Globals.szLdapAdminDn, "");
	Globals.ldap_ssl = LDAP_SSL_START_TLS;
	Globals.ldap_ssl_ads = False;
	Globals.ldap_deref = -1;
	Globals.ldap_passwd_sync = LDAP_PASSWD_SYNC_OFF;
	Globals.ldap_delete_dn = False;
	Globals.ldap_replication_sleep = 1000; /* wait 1 sec for replication */
	Globals.ldap_follow_referral = Auto;
	Globals.ldap_timeout = LDAP_DEFAULT_TIMEOUT;
	Globals.ldap_connection_timeout = LDAP_CONNECTION_DEFAULT_TIMEOUT;
	Globals.ldap_page_size = LDAP_PAGE_SIZE;

	Globals.ldap_debug_level = 0;
	Globals.ldap_debug_threshold = 10;

	Globals.client_ldap_sasl_wrapping = ADS_AUTH_SASL_SIGN;

	/* This is what we tell the afs client. in reality we set the token 
	 * to never expire, though, when this runs out the afs client will 
	 * forget the token. Set to 0 to get NEVERDATE.*/
	Globals.iAfsTokenLifetime = 604800;
	Globals.cups_connection_timeout = CUPS_DEFAULT_CONNECTION_TIMEOUT;

/* these parameters are set to defaults that are more appropriate
   for the increasing samba install base:

   as a member of the workgroup, that will possibly become a
   _local_ master browser (lm = True).  this is opposed to a forced
   local master browser startup (pm = True).

   doesn't provide WINS server service by default (wsupp = False),
   and doesn't provide domain master browser services by default, either.

*/

	Globals.bMsAddPrinterWizard = True;
	Globals.os_level = 20;
	Globals.bLocalMaster = True;
	Globals.iDomainMaster = Auto;	/* depending on bDomainLogons */
	Globals.bDomainLogons = False;
	Globals.bBrowseList = True;
	Globals.bWINSsupport = False;
	Globals.bWINSproxy = False;

	TALLOC_FREE(Globals.szInitLogonDelayedHosts);
	Globals.InitLogonDelay = 100; /* 100 ms default delay */

	Globals.bDNSproxy = True;

	/* this just means to use them if they exist */
	Globals.bKernelOplocks = True;

	Globals.bAllowTrustedDomains = True;
	string_set(&Globals.szIdmapBackend, "tdb");
	Globals.bIdmapReadOnly = false;

	string_set(&Globals.szTemplateShell, "/bin/false");
	string_set(&Globals.szTemplateHomedir, "/home/%D/%U");
	string_set(&Globals.szWinbindSeparator, "\\");

	string_set(&Globals.szCupsServer, "");
	string_set(&Globals.szIPrintServer, "");

	string_set(&Globals.ctdbdSocket, "");
	Globals.szClusterAddresses = NULL;
	Globals.clustering = False;
	Globals.ctdb_timeout = 0;
	Globals.ctdb_locktime_warn_threshold = 0;

	Globals.winbind_cache_time = 300;	/* 5 minutes */
	Globals.winbind_reconnect_delay = 30;	/* 30 seconds */
	Globals.winbind_max_clients = 200;
	Globals.bWinbindEnumUsers = False;
	Globals.bWinbindEnumGroups = False;
	Globals.bWinbindUseDefaultDomain = False;
	Globals.bWinbindTrustedDomainsOnly = False;
	Globals.bWinbindNestedGroups = True;
	Globals.winbind_expand_groups = 1;
	Globals.szWinbindNssInfo = str_list_make_v3(NULL, "template", NULL);
	Globals.bWinbindRefreshTickets = False;
	Globals.bWinbindOfflineLogon = False;
	Globals.bWinbindSealedPipes = True;

	Globals.iIdmapCacheTime = 86400 * 7; /* a week by default */
	Globals.iIdmapNegativeCacheTime = 120; /* 2 minutes by default */

	Globals.bPassdbExpandExplicit = False;

	Globals.name_cache_timeout = 660; /* In seconds */

	Globals.bUseSpnego = True;
	Globals.bClientUseSpnego = True;

	Globals.client_signing = Auto;
	Globals.client_ipc_signing = Required;
	Globals.server_signing = False;

	Globals.bDeferSharingViolations = True;
	string_set(&Globals.smb_ports, SMB_PORTS);

	Globals.bEnablePrivileges = True;
	Globals.bHostMSDfs        = True;
	Globals.bASUSupport       = False;

	/* User defined shares. */
	if (asprintf(&s, "%s/usershares", get_dyn_STATEDIR()) < 0) {
		smb_panic("init_globals: ENOMEM");
	}
	string_set(&Globals.szUsersharePath, s);
	SAFE_FREE(s);
	string_set(&Globals.szUsershareTemplateShare, "");
	Globals.iUsershareMaxShares = 0;
	/* By default disallow sharing of directories not owned by the sharer. */
	Globals.bUsershareOwnerOnly = True;
	/* By default disallow guest access to usershares. */
	Globals.bUsershareAllowGuests = False;

	Globals.iKeepalive = DEFAULT_KEEPALIVE;

	/* By default no shares out of the registry */
	Globals.bRegistryShares = False;

	Globals.iminreceivefile = 0;

	Globals.bMapUntrustedToDomain = false;
	Globals.bMulticastDnsRegister = true;

	Globals.ismb2_max_read = DEFAULT_SMB2_MAX_READ;
	Globals.ismb2_max_write = DEFAULT_SMB2_MAX_WRITE;
	Globals.ismb2_max_trans = DEFAULT_SMB2_MAX_TRANSACT;
	Globals.ismb2_max_credits = DEFAULT_SMB2_MAX_CREDITS;

	string_set(&Globals.ncalrpc_dir, get_dyn_NCALRPCDIR());

	/* Now put back the settings that were set with lp_set_cmdline() */
	apply_lp_set_cmdline();
}

/*******************************************************************
 Convenience routine to grab string parameters into temporary memory
 and run standard_sub_basic on them. The buffers can be written to by
 callers without affecting the source string.
********************************************************************/

static char *lp_string(const char *s)
{
	char *ret;
	TALLOC_CTX *ctx = talloc_tos();

	/* The follow debug is useful for tracking down memory problems
	   especially if you have an inner loop that is calling a lp_*()
	   function that returns a string.  Perhaps this debug should be
	   present all the time? */

#if 0
	DEBUG(10, ("lp_string(%s)\n", s));
#endif
	if (!s) {
		return NULL;
	}

	ret = talloc_sub_basic(ctx,
			get_current_username(),
			current_user_info.domain,
			s);
	if (trim_char(ret, '\"', '\"')) {
		if (strchr(ret,'\"') != NULL) {
			TALLOC_FREE(ret);
			ret = talloc_sub_basic(ctx,
					get_current_username(),
					current_user_info.domain,
					s);
		}
	}
	return ret;
}

/*
   In this section all the functions that are used to access the
   parameters from the rest of the program are defined
*/

#define FN_GLOBAL_STRING(fn_name,ptr) \
 char *fn_name(void) {return(lp_string(*(char **)(ptr) ? *(char **)(ptr) : ""));}
#define FN_GLOBAL_CONST_STRING(fn_name,ptr) \
 const char *fn_name(void) {return(*(const char **)(ptr) ? *(const char **)(ptr) : "");}
#define FN_GLOBAL_LIST(fn_name,ptr) \
 const char **fn_name(void) {return(*(const char ***)(ptr));}
#define FN_GLOBAL_BOOL(fn_name,ptr) \
 bool fn_name(void) {return(*(bool *)(ptr));}
#define FN_GLOBAL_CHAR(fn_name,ptr) \
 char fn_name(void) {return(*(char *)(ptr));}
#define FN_GLOBAL_INTEGER(fn_name,ptr) \
 int fn_name(void) {return(*(int *)(ptr));}

#define FN_LOCAL_STRING(fn_name,val) \
 char *fn_name(int i) {return(lp_string((LP_SNUM_OK(i) && ServicePtrs[(i)]->val) ? ServicePtrs[(i)]->val : sDefault.val));}
#define FN_LOCAL_CONST_STRING(fn_name,val) \
 const char *fn_name(int i) {return (const char *)((LP_SNUM_OK(i) && ServicePtrs[(i)]->val) ? ServicePtrs[(i)]->val : sDefault.val);}
#define FN_LOCAL_LIST(fn_name,val) \
 const char **fn_name(int i) {return(const char **)(LP_SNUM_OK(i)? ServicePtrs[(i)]->val : sDefault.val);}
#define FN_LOCAL_BOOL(fn_name,val) \
 bool fn_name(int i) {return(bool)(LP_SNUM_OK(i)? ServicePtrs[(i)]->val : sDefault.val);}
#define FN_LOCAL_INTEGER(fn_name,val) \
 int fn_name(int i) {return(LP_SNUM_OK(i)? ServicePtrs[(i)]->val : sDefault.val);}

#define FN_LOCAL_PARM_BOOL(fn_name,val) \
 bool fn_name(const struct share_params *p) {return(bool)(LP_SNUM_OK(p->service)? ServicePtrs[(p->service)]->val : sDefault.val);}
#define FN_LOCAL_PARM_INTEGER(fn_name,val) \
 int fn_name(const struct share_params *p) {return(LP_SNUM_OK(p->service)? ServicePtrs[(p->service)]->val : sDefault.val);}
#define FN_LOCAL_CHAR(fn_name,val) \
 char fn_name(const struct share_params *p) {return(LP_SNUM_OK(p->service)? ServicePtrs[(p->service)]->val : sDefault.val);}

FN_GLOBAL_STRING(lp_smb_ports, &Globals.smb_ports)
FN_GLOBAL_CONST_STRING(lp_dos_charset, &Globals.dos_charset)
FN_GLOBAL_CONST_STRING(lp_unix_charset, &Globals.unix_charset)
FN_GLOBAL_CONST_STRING(lp_display_charset, &Globals.display_charset)
FN_GLOBAL_STRING(lp_logfile, &Globals.szLogFile)
FN_GLOBAL_STRING(lp_configfile, &Globals.szConfigFile)
FN_GLOBAL_STRING(lp_smb_passwd_file, &Globals.szSMBPasswdFile)
FN_GLOBAL_STRING(lp_private_dir, &Globals.szPrivateDir)
FN_GLOBAL_STRING(lp_serverstring, &Globals.szServerString)
FN_GLOBAL_INTEGER(lp_printcap_cache_time, &Globals.PrintcapCacheTime)
FN_GLOBAL_STRING(lp_addport_cmd, &Globals.szAddPortCommand)
FN_GLOBAL_STRING(lp_enumports_cmd, &Globals.szEnumPortsCommand)
FN_GLOBAL_STRING(lp_addprinter_cmd, &Globals.szAddPrinterCommand)
FN_GLOBAL_STRING(lp_deleteprinter_cmd, &Globals.szDeletePrinterCommand)
FN_GLOBAL_STRING(lp_os2_driver_map, &Globals.szOs2DriverMap)
FN_GLOBAL_STRING(lp_lockdir, &Globals.szLockDir)
/* If lp_statedir() and lp_cachedir() are explicitely set during the
 * build process or in smb.conf, we use that value.  Otherwise they
 * default to the value of lp_lockdir(). */
char *lp_statedir(void) {
	if ((strcmp(get_dyn_STATEDIR(), get_dyn_LOCKDIR()) != 0) ||
	    (strcmp(get_dyn_STATEDIR(), Globals.szStateDir) != 0))
		return(lp_string(*(char **)(&Globals.szStateDir) ?
		    *(char **)(&Globals.szStateDir) : ""));
	else
		return(lp_string(*(char **)(&Globals.szLockDir) ?
		    *(char **)(&Globals.szLockDir) : ""));
}
char *lp_cachedir(void) {
	if ((strcmp(get_dyn_CACHEDIR(), get_dyn_LOCKDIR()) != 0) ||
	    (strcmp(get_dyn_CACHEDIR(), Globals.szCacheDir) != 0))
		return(lp_string(*(char **)(&Globals.szCacheDir) ?
		    *(char **)(&Globals.szCacheDir) : ""));
	else
		return(lp_string(*(char **)(&Globals.szLockDir) ?
		    *(char **)(&Globals.szLockDir) : ""));
}
FN_GLOBAL_STRING(lp_piddir, &Globals.szPidDir)
FN_GLOBAL_STRING(lp_mangling_method, &Globals.szManglingMethod)
FN_GLOBAL_INTEGER(lp_mangle_prefix, &Globals.mangle_prefix)
FN_GLOBAL_STRING(lp_utmpdir, &Globals.szUtmpDir)
FN_GLOBAL_STRING(lp_wtmpdir, &Globals.szWtmpDir)
FN_GLOBAL_BOOL(lp_utmp, &Globals.bUtmp)
FN_GLOBAL_STRING(lp_rootdir, &Globals.szRootdir)
FN_GLOBAL_STRING(lp_perfcount_module, &Globals.szSMBPerfcountModule)
FN_GLOBAL_STRING(lp_defaultservice, &Globals.szDefaultService)
FN_GLOBAL_STRING(lp_msg_command, &Globals.szMsgCommand)
FN_GLOBAL_STRING(lp_get_quota_command, &Globals.szGetQuota)
FN_GLOBAL_STRING(lp_set_quota_command, &Globals.szSetQuota)
FN_GLOBAL_STRING(lp_auto_services, &Globals.szAutoServices)
FN_GLOBAL_STRING(lp_passwd_program, &Globals.szPasswdProgram)
FN_GLOBAL_STRING(lp_passwd_chat, &Globals.szPasswdChat)
FN_GLOBAL_STRING(lp_passwordserver, &Globals.szPasswordServer)
FN_GLOBAL_STRING(lp_name_resolve_order, &Globals.szNameResolveOrder)
FN_GLOBAL_STRING(lp_realm, &Globals.szRealm)
FN_GLOBAL_CONST_STRING(lp_afs_username_map, &Globals.szAfsUsernameMap)
FN_GLOBAL_INTEGER(lp_afs_token_lifetime, &Globals.iAfsTokenLifetime)
FN_GLOBAL_STRING(lp_log_nt_token_command, &Globals.szLogNtTokenCommand)
FN_GLOBAL_STRING(lp_username_map, &Globals.szUsernameMap)
FN_GLOBAL_CONST_STRING(lp_logon_script, &Globals.szLogonScript)
FN_GLOBAL_CONST_STRING(lp_logon_path, &Globals.szLogonPath)
FN_GLOBAL_CONST_STRING(lp_logon_drive, &Globals.szLogonDrive)
FN_GLOBAL_CONST_STRING(lp_logon_home, &Globals.szLogonHome)
FN_GLOBAL_STRING(lp_remote_announce, &Globals.szRemoteAnnounce)
FN_GLOBAL_STRING(lp_remote_browse_sync, &Globals.szRemoteBrowseSync)
FN_GLOBAL_BOOL(lp_nmbd_bind_explicit_broadcast, &Globals.bNmbdBindExplicitBroadcast)
FN_GLOBAL_LIST(lp_wins_server_list, &Globals.szWINSservers)
FN_GLOBAL_LIST(lp_interfaces, &Globals.szInterfaces)
FN_GLOBAL_STRING(lp_nis_home_map_name, &Globals.szNISHomeMapName)
static FN_GLOBAL_STRING(lp_announce_version, &Globals.szAnnounceVersion)
FN_GLOBAL_LIST(lp_netbios_aliases, &Globals.szNetbiosAliases)
/* FN_GLOBAL_STRING(lp_passdb_backend, &Globals.szPassdbBackend)
 * lp_passdb_backend() should be replace by the this macro again after
 * some releases.
 * */
const char *lp_passdb_backend(void)
{
	char *delim, *quote;

	delim = strchr( Globals.szPassdbBackend, ' ');
	/* no space at all */
	if (delim == NULL) {
		goto out;
	}

	quote = strchr(Globals.szPassdbBackend, '"');
	/* no quote char or non in the first part */
	if (quote == NULL || quote > delim) {
		*delim = '\0';
		goto warn;
	}

	quote = strchr(quote+1, '"');
	if (quote == NULL) {
		DEBUG(0, ("WARNING: Your 'passdb backend' configuration is invalid due to a missing second \" char.\n"));
		goto out;
	} else if (*(quote+1) == '\0') {
		/* space, fitting quote char, and one backend only */
		goto out;
	} else {
		/* terminate string after the fitting quote char */
		*(quote+1) = '\0';
	}

warn:
	DEBUG(0, ("WARNING: Your 'passdb backend' configuration includes multiple backends.  This\n"
		"is deprecated since Samba 3.0.23.  Please check WHATSNEW.txt or the section 'Passdb\n"
		"Changes' from the ChangeNotes as part of the Samba HOWTO collection.  Only the first\n"
		"backend (%s) is used.  The rest is ignored.\n", Globals.szPassdbBackend));

out:
	return Globals.szPassdbBackend;
}
FN_GLOBAL_LIST(lp_preload_modules, &Globals.szPreloadModules)
FN_GLOBAL_STRING(lp_panic_action, &Globals.szPanicAction)
FN_GLOBAL_STRING(lp_adduser_script, &Globals.szAddUserScript)
FN_GLOBAL_STRING(lp_renameuser_script, &Globals.szRenameUserScript)
FN_GLOBAL_STRING(lp_deluser_script, &Globals.szDelUserScript)

FN_GLOBAL_CONST_STRING(lp_guestaccount, &Globals.szGuestaccount)
FN_GLOBAL_STRING(lp_addgroup_script, &Globals.szAddGroupScript)
FN_GLOBAL_STRING(lp_delgroup_script, &Globals.szDelGroupScript)
FN_GLOBAL_STRING(lp_addusertogroup_script, &Globals.szAddUserToGroupScript)
FN_GLOBAL_STRING(lp_deluserfromgroup_script, &Globals.szDelUserFromGroupScript)
FN_GLOBAL_STRING(lp_setprimarygroup_script, &Globals.szSetPrimaryGroupScript)

FN_GLOBAL_STRING(lp_addmachine_script, &Globals.szAddMachineScript)

FN_GLOBAL_STRING(lp_shutdown_script, &Globals.szShutdownScript)
FN_GLOBAL_STRING(lp_abort_shutdown_script, &Globals.szAbortShutdownScript)
FN_GLOBAL_STRING(lp_username_map_script, &Globals.szUsernameMapScript)
FN_GLOBAL_INTEGER(lp_username_map_cache_time, &Globals.iUsernameMapCacheTime)

FN_GLOBAL_STRING(lp_check_password_script, &Globals.szCheckPasswordScript)

FN_GLOBAL_BOOL(lp_allow_dcerpc_auth_level_connect, &Globals.bAllowDcerpcAuthLevelConnect)
FN_GLOBAL_STRING(lp_wins_hook, &Globals.szWINSHook)
FN_GLOBAL_CONST_STRING(lp_template_homedir, &Globals.szTemplateHomedir)
FN_GLOBAL_CONST_STRING(lp_template_shell, &Globals.szTemplateShell)
FN_GLOBAL_CONST_STRING(lp_winbind_separator, &Globals.szWinbindSeparator)
FN_GLOBAL_INTEGER(lp_acl_compatibility, &Globals.iAclCompat)
FN_GLOBAL_BOOL(lp_winbind_enum_users, &Globals.bWinbindEnumUsers)
FN_GLOBAL_BOOL(lp_winbind_enum_groups, &Globals.bWinbindEnumGroups)
FN_GLOBAL_BOOL(lp_winbind_use_default_domain, &Globals.bWinbindUseDefaultDomain)
FN_GLOBAL_BOOL(lp_winbind_trusted_domains_only, &Globals.bWinbindTrustedDomainsOnly)
FN_GLOBAL_BOOL(lp_winbind_nested_groups, &Globals.bWinbindNestedGroups)
FN_GLOBAL_INTEGER(lp_winbind_expand_groups, &Globals.winbind_expand_groups)
FN_GLOBAL_BOOL(lp_winbind_refresh_tickets, &Globals.bWinbindRefreshTickets)
FN_GLOBAL_BOOL(lp_winbind_offline_logon, &Globals.bWinbindOfflineLogon)
FN_GLOBAL_BOOL(lp_winbind_sealed_pipes, &Globals.bWinbindSealedPipes)
FN_GLOBAL_BOOL(lp_winbind_normalize_names, &Globals.bWinbindNormalizeNames)
FN_GLOBAL_BOOL(lp_winbind_rpc_only, &Globals.bWinbindRpcOnly)
FN_GLOBAL_BOOL(lp_create_krb5_conf, &Globals.bCreateKrb5Conf)
static FN_GLOBAL_INTEGER(lp_winbind_max_domain_connections_int,
		  &Globals.winbindMaxDomainConnections)

int lp_winbind_max_domain_connections(void)
{
	if (lp_winbind_offline_logon() &&
	    lp_winbind_max_domain_connections_int() > 1) {
		DEBUG(1, ("offline logons active, restricting max domain "
			  "connections to 1\n"));
		return 1;
	}
	return MAX(1, lp_winbind_max_domain_connections_int());
}

FN_GLOBAL_CONST_STRING(lp_idmap_backend, &Globals.szIdmapBackend)
FN_GLOBAL_INTEGER(lp_idmap_cache_time, &Globals.iIdmapCacheTime)
FN_GLOBAL_INTEGER(lp_idmap_negative_cache_time, &Globals.iIdmapNegativeCacheTime)
FN_GLOBAL_INTEGER(lp_keepalive, &Globals.iKeepalive)
FN_GLOBAL_BOOL(lp_passdb_expand_explicit, &Globals.bPassdbExpandExplicit)

FN_GLOBAL_STRING(lp_ldap_suffix, &Globals.szLdapSuffix)
FN_GLOBAL_STRING(lp_ldap_admin_dn, &Globals.szLdapAdminDn)
FN_GLOBAL_INTEGER(lp_ldap_ssl, &Globals.ldap_ssl)
FN_GLOBAL_BOOL(lp_ldap_ssl_ads, &Globals.ldap_ssl_ads)
FN_GLOBAL_INTEGER(lp_ldap_deref, &Globals.ldap_deref)
FN_GLOBAL_INTEGER(lp_ldap_follow_referral, &Globals.ldap_follow_referral)
FN_GLOBAL_INTEGER(lp_ldap_passwd_sync, &Globals.ldap_passwd_sync)
FN_GLOBAL_BOOL(lp_ldap_delete_dn, &Globals.ldap_delete_dn)
FN_GLOBAL_INTEGER(lp_ldap_replication_sleep, &Globals.ldap_replication_sleep)
FN_GLOBAL_INTEGER(lp_ldap_timeout, &Globals.ldap_timeout)
FN_GLOBAL_INTEGER(lp_ldap_connection_timeout, &Globals.ldap_connection_timeout)
FN_GLOBAL_INTEGER(lp_ldap_page_size, &Globals.ldap_page_size)
FN_GLOBAL_INTEGER(lp_ldap_debug_level, &Globals.ldap_debug_level)
FN_GLOBAL_INTEGER(lp_ldap_debug_threshold, &Globals.ldap_debug_threshold)
FN_GLOBAL_STRING(lp_add_share_cmd, &Globals.szAddShareCommand)
FN_GLOBAL_STRING(lp_change_share_cmd, &Globals.szChangeShareCommand)
FN_GLOBAL_STRING(lp_delete_share_cmd, &Globals.szDeleteShareCommand)
FN_GLOBAL_STRING(lp_usershare_path, &Globals.szUsersharePath)
FN_GLOBAL_LIST(lp_usershare_prefix_allow_list, &Globals.szUsersharePrefixAllowList)
FN_GLOBAL_LIST(lp_usershare_prefix_deny_list, &Globals.szUsersharePrefixDenyList)

FN_GLOBAL_LIST(lp_eventlog_list, &Globals.szEventLogs)

FN_GLOBAL_BOOL(lp_registry_shares, &Globals.bRegistryShares)
FN_GLOBAL_BOOL(lp_usershare_allow_guests, &Globals.bUsershareAllowGuests)
FN_GLOBAL_BOOL(lp_usershare_owner_only, &Globals.bUsershareOwnerOnly)
FN_GLOBAL_BOOL(lp_disable_netbios, &Globals.bDisableNetbios)
FN_GLOBAL_BOOL(lp_reset_on_zero_vc, &Globals.bResetOnZeroVC)
FN_GLOBAL_BOOL(lp_log_writeable_files_on_exit,
	       &Globals.bLogWriteableFilesOnExit)
FN_GLOBAL_BOOL(lp_ms_add_printer_wizard, &Globals.bMsAddPrinterWizard)
FN_GLOBAL_BOOL(lp_dns_proxy, &Globals.bDNSproxy)
FN_GLOBAL_BOOL(lp_wins_support, &Globals.bWINSsupport)
FN_GLOBAL_BOOL(lp_we_are_a_wins_server, &Globals.bWINSsupport)
FN_GLOBAL_BOOL(lp_wins_proxy, &Globals.bWINSproxy)
FN_GLOBAL_BOOL(lp_local_master, &Globals.bLocalMaster)
FN_GLOBAL_BOOL(lp_domain_logons, &Globals.bDomainLogons)
FN_GLOBAL_LIST(lp_init_logon_delayed_hosts, &Globals.szInitLogonDelayedHosts)
FN_GLOBAL_INTEGER(lp_init_logon_delay, &Globals.InitLogonDelay)
FN_GLOBAL_BOOL(lp_load_printers, &Globals.bLoadPrinters)
FN_GLOBAL_BOOL(_lp_readraw, &Globals.bReadRaw)
FN_GLOBAL_BOOL(lp_large_readwrite, &Globals.bLargeReadwrite)
FN_GLOBAL_BOOL(_lp_writeraw, &Globals.bWriteRaw)
FN_GLOBAL_BOOL(lp_null_passwords, &Globals.bNullPasswords)
FN_GLOBAL_BOOL(lp_obey_pam_restrictions, &Globals.bObeyPamRestrictions)
FN_GLOBAL_BOOL(lp_encrypted_passwords, &Globals.bEncryptPasswords)
FN_GLOBAL_INTEGER(lp_client_schannel, &Globals.clientSchannel)
FN_GLOBAL_INTEGER(lp_server_schannel, &Globals.serverSchannel)
FN_GLOBAL_BOOL(lp_syslog_only, &Globals.bSyslogOnly)
FN_GLOBAL_BOOL(lp_timestamp_logs, &Globals.bTimestampLogs)
FN_GLOBAL_BOOL(lp_debug_prefix_timestamp, &Globals.bDebugPrefixTimestamp)
FN_GLOBAL_BOOL(lp_debug_hires_timestamp, &Globals.bDebugHiresTimestamp)
FN_GLOBAL_BOOL(lp_debug_pid, &Globals.bDebugPid)
FN_GLOBAL_BOOL(lp_debug_uid, &Globals.bDebugUid)
FN_GLOBAL_BOOL(lp_debug_class, &Globals.bDebugClass)
FN_GLOBAL_BOOL(lp_enable_core_files, &Globals.bEnableCoreFiles)
FN_GLOBAL_BOOL(lp_browse_list, &Globals.bBrowseList)
FN_GLOBAL_BOOL(lp_nis_home_map, &Globals.bNISHomeMap)
static FN_GLOBAL_BOOL(lp_time_server, &Globals.bTimeServer)
FN_GLOBAL_BOOL(lp_bind_interfaces_only, &Globals.bBindInterfacesOnly)
FN_GLOBAL_BOOL(lp_pam_password_change, &Globals.bPamPasswordChange)
FN_GLOBAL_BOOL(lp_unix_password_sync, &Globals.bUnixPasswdSync)
FN_GLOBAL_BOOL(lp_passwd_chat_debug, &Globals.bPasswdChatDebug)
FN_GLOBAL_INTEGER(lp_passwd_chat_timeout, &Globals.iPasswdChatTimeout)
FN_GLOBAL_BOOL(lp_nt_pipe_support, &Globals.bNTPipeSupport)
FN_GLOBAL_BOOL(lp_nt_status_support, &Globals.bNTStatusSupport)
FN_GLOBAL_BOOL(lp_stat_cache, &Globals.bStatCache)
FN_GLOBAL_INTEGER(lp_max_stat_cache_size, &Globals.iMaxStatCacheSize)
FN_GLOBAL_BOOL(lp_allow_trusted_domains, &Globals.bAllowTrustedDomains)
FN_GLOBAL_BOOL(lp_map_untrusted_to_domain, &Globals.bMapUntrustedToDomain)
FN_GLOBAL_INTEGER(lp_restrict_anonymous, &Globals.restrict_anonymous)
FN_GLOBAL_BOOL(lp_lanman_auth, &Globals.bLanmanAuth)
FN_GLOBAL_BOOL(lp_ntlm_auth, &Globals.bNTLMAuth)
FN_GLOBAL_BOOL(lp_raw_ntlmv2_auth, &Globals.bRawNTLMv2Auth)
FN_GLOBAL_BOOL(lp_client_plaintext_auth, &Globals.bClientPlaintextAuth)
FN_GLOBAL_BOOL(lp_client_lanman_auth, &Globals.bClientLanManAuth)
FN_GLOBAL_BOOL(lp_client_ntlmv2_auth, &Globals.bClientNTLMv2Auth)
FN_GLOBAL_BOOL(lp_host_msdfs, &Globals.bHostMSDfs)
FN_GLOBAL_BOOL(lp_kernel_oplocks, &Globals.bKernelOplocks)
FN_GLOBAL_BOOL(lp_enhanced_browsing, &Globals.enhanced_browsing)
FN_GLOBAL_BOOL(lp_use_mmap, &Globals.bUseMmap)
FN_GLOBAL_BOOL(lp_unix_extensions, &Globals.bUnixExtensions)
FN_GLOBAL_BOOL(lp_use_spnego, &Globals.bUseSpnego)
FN_GLOBAL_BOOL(lp_client_use_spnego, &Globals.bClientUseSpnego)
FN_GLOBAL_BOOL(lp_client_use_spnego_principal, &Globals.client_use_spnego_principal)
FN_GLOBAL_BOOL(lp_send_spnego_principal, &Globals.send_spnego_principal)
FN_GLOBAL_BOOL(lp_hostname_lookups, &Globals.bHostnameLookups)
FN_LOCAL_PARM_BOOL(lp_change_notify, bChangeNotify)
FN_LOCAL_PARM_BOOL(lp_kernel_change_notify, bKernelChangeNotify)
FN_GLOBAL_STRING(lp_dedicated_keytab_file, &Globals.szDedicatedKeytabFile)
FN_GLOBAL_INTEGER(lp_kerberos_method, &Globals.iKerberosMethod)
FN_GLOBAL_BOOL(lp_defer_sharing_violations, &Globals.bDeferSharingViolations)
FN_GLOBAL_BOOL(lp_enable_privileges, &Globals.bEnablePrivileges)
FN_GLOBAL_BOOL(lp_enable_asu_support, &Globals.bASUSupport)
FN_GLOBAL_INTEGER(lp_os_level, &Globals.os_level)
FN_GLOBAL_INTEGER(lp_max_ttl, &Globals.max_ttl)
FN_GLOBAL_INTEGER(lp_max_wins_ttl, &Globals.max_wins_ttl)
FN_GLOBAL_INTEGER(lp_min_wins_ttl, &Globals.min_wins_ttl)
FN_GLOBAL_INTEGER(lp_max_log_size, &Globals.max_log_size)
FN_GLOBAL_INTEGER(lp_max_open_files, &Globals.max_open_files)
FN_GLOBAL_INTEGER(lp_open_files_db_hash_size, &Globals.open_files_db_hash_size)
FN_GLOBAL_INTEGER(lp_maxxmit, &Globals.max_xmit)
FN_GLOBAL_INTEGER(lp_maxmux, &Globals.max_mux)
FN_GLOBAL_INTEGER(lp_passwordlevel, &Globals.pwordlevel)
FN_GLOBAL_INTEGER(lp_usernamelevel, &Globals.unamelevel)
FN_GLOBAL_INTEGER(lp_deadtime, &Globals.deadtime)
FN_GLOBAL_BOOL(lp_getwd_cache, &Globals.getwd_cache)
static FN_GLOBAL_INTEGER(_lp_maxprotocol, &Globals.maxprotocol)
int lp_maxprotocol(void)
{
	int ret = _lp_maxprotocol();
	if ((ret == PROTOCOL_SMB2) && (lp_security() == SEC_SHARE)) {
		DEBUG(2,("WARNING!!: \"security = share\" is incompatible "
			"with the SMB2 protocol. Resetting to SMB1.\n" ));
			lp_do_parameter(-1, "max protocol", "NT1");
		return PROTOCOL_NT1;
	}
	return ret;
}
FN_GLOBAL_INTEGER(lp_minprotocol, &Globals.minprotocol)
FN_GLOBAL_INTEGER(lp_security, &Globals.security)
FN_GLOBAL_LIST(lp_auth_methods, &Globals.AuthMethods)
FN_GLOBAL_BOOL(lp_paranoid_server_security, &Globals.paranoid_server_security)
FN_GLOBAL_INTEGER(lp_maxdisksize, &Globals.maxdisksize)
FN_GLOBAL_INTEGER(lp_lpqcachetime, &Globals.lpqcachetime)
FN_GLOBAL_INTEGER(lp_max_smbd_processes, &Globals.iMaxSmbdProcesses)
FN_GLOBAL_BOOL(_lp_disable_spoolss, &Globals.bDisableSpoolss)
FN_GLOBAL_INTEGER(lp_syslog, &Globals.syslog)
static FN_GLOBAL_INTEGER(lp_announce_as, &Globals.announce_as)
FN_GLOBAL_INTEGER(lp_lm_announce, &Globals.lm_announce)
FN_GLOBAL_INTEGER(lp_lm_interval, &Globals.lm_interval)
FN_GLOBAL_INTEGER(lp_machine_password_timeout, &Globals.machine_password_timeout)
FN_GLOBAL_INTEGER(lp_map_to_guest, &Globals.map_to_guest)
FN_GLOBAL_INTEGER(lp_oplock_break_wait_time, &Globals.oplock_break_wait_time)
FN_GLOBAL_INTEGER(lp_lock_spin_time, &Globals.iLockSpinTime)
FN_GLOBAL_INTEGER(lp_usershare_max_shares, &Globals.iUsershareMaxShares)
FN_GLOBAL_CONST_STRING(lp_socket_options, &Globals.szSocketOptions)
FN_GLOBAL_INTEGER(lp_config_backend, &Globals.ConfigBackend)
FN_GLOBAL_INTEGER(lp_smb2_max_read, &Globals.ismb2_max_read)
FN_GLOBAL_INTEGER(lp_smb2_max_write, &Globals.ismb2_max_write)
FN_GLOBAL_INTEGER(lp_smb2_max_trans, &Globals.ismb2_max_trans)
int lp_smb2_max_credits(void)
{
	if (Globals.ismb2_max_credits == 0) {
		Globals.ismb2_max_credits = DEFAULT_SMB2_MAX_CREDITS;
	}
	return Globals.ismb2_max_credits;
}
FN_LOCAL_STRING(lp_preexec, szPreExec)
FN_LOCAL_STRING(lp_postexec, szPostExec)
FN_LOCAL_STRING(lp_rootpreexec, szRootPreExec)
FN_LOCAL_STRING(lp_rootpostexec, szRootPostExec)
FN_LOCAL_STRING(lp_servicename, szService)
FN_LOCAL_CONST_STRING(lp_const_servicename, szService)
FN_LOCAL_STRING(lp_pathname, szPath)
FN_LOCAL_STRING(lp_dontdescend, szDontdescend)
FN_LOCAL_STRING(lp_username, szUsername)
FN_LOCAL_LIST(lp_invalid_users, szInvalidUsers)
FN_LOCAL_LIST(lp_valid_users, szValidUsers)
FN_LOCAL_LIST(lp_admin_users, szAdminUsers)
FN_GLOBAL_LIST(lp_svcctl_list, &Globals.szServicesList)
FN_LOCAL_STRING(lp_cups_options, szCupsOptions)
FN_GLOBAL_STRING(lp_cups_server, &Globals.szCupsServer)
int lp_cups_encrypt(void)
{
	int result = 0;
#ifdef HAVE_HTTPCONNECTENCRYPT
	switch (Globals.CupsEncrypt) {
		case Auto:
			result = HTTP_ENCRYPT_REQUIRED;
			break;
		case True:
			result = HTTP_ENCRYPT_ALWAYS;
			break;
		case False:
			result = HTTP_ENCRYPT_NEVER;
			break;
	}
#endif
	return result;
}
FN_GLOBAL_STRING(lp_iprint_server, &Globals.szIPrintServer)
FN_GLOBAL_INTEGER(lp_cups_connection_timeout, &Globals.cups_connection_timeout)
FN_GLOBAL_CONST_STRING(lp_ctdbd_socket, &Globals.ctdbdSocket)
FN_GLOBAL_LIST(lp_cluster_addresses, &Globals.szClusterAddresses)
FN_GLOBAL_BOOL(lp_clustering, &Globals.clustering)
FN_GLOBAL_INTEGER(lp_ctdb_timeout, &Globals.ctdb_timeout)
FN_GLOBAL_INTEGER(lp_ctdb_locktime_warn_threshold, &Globals.ctdb_locktime_warn_threshold)
FN_LOCAL_STRING(lp_printcommand, szPrintcommand)
FN_LOCAL_STRING(lp_lpqcommand, szLpqcommand)
FN_LOCAL_STRING(lp_lprmcommand, szLprmcommand)
FN_LOCAL_STRING(lp_lppausecommand, szLppausecommand)
FN_LOCAL_STRING(lp_lpresumecommand, szLpresumecommand)
FN_LOCAL_STRING(lp_queuepausecommand, szQueuepausecommand)
FN_LOCAL_STRING(lp_queueresumecommand, szQueueresumecommand)
static FN_LOCAL_STRING(_lp_printername, szPrintername)
FN_LOCAL_CONST_STRING(lp_printjob_username, szPrintjobUsername)
FN_LOCAL_LIST(lp_hostsallow, szHostsallow)
FN_LOCAL_LIST(lp_hostsdeny, szHostsdeny)
FN_LOCAL_STRING(lp_magicscript, szMagicScript)
FN_LOCAL_STRING(lp_magicoutput, szMagicOutput)
FN_LOCAL_STRING(lp_comment, comment)
FN_LOCAL_STRING(lp_force_user, force_user)
FN_LOCAL_STRING(lp_force_group, force_group)
FN_LOCAL_LIST(lp_readlist, readlist)
FN_LOCAL_LIST(lp_writelist, writelist)
FN_LOCAL_LIST(lp_printer_admin, printer_admin)
FN_LOCAL_STRING(lp_fstype, fstype)
FN_LOCAL_LIST(lp_vfs_objects, szVfsObjects)
FN_LOCAL_STRING(lp_msdfs_proxy, szMSDfsProxy)
static FN_LOCAL_STRING(lp_volume, volume)
FN_LOCAL_STRING(lp_veto_files, szVetoFiles)
FN_LOCAL_STRING(lp_hide_files, szHideFiles)
FN_LOCAL_STRING(lp_veto_oplocks, szVetoOplockFiles)
FN_LOCAL_BOOL(lp_msdfs_root, bMSDfsRoot)
FN_LOCAL_STRING(lp_aio_write_behind, szAioWriteBehind)
FN_LOCAL_STRING(lp_dfree_command, szDfree)
FN_LOCAL_BOOL(lp_autoloaded, autoloaded)
FN_LOCAL_BOOL(lp_preexec_close, bPreexecClose)
FN_LOCAL_BOOL(lp_rootpreexec_close, bRootpreexecClose)
FN_LOCAL_INTEGER(lp_casesensitive, iCaseSensitive)
FN_LOCAL_BOOL(lp_preservecase, bCasePreserve)
FN_LOCAL_BOOL(lp_shortpreservecase, bShortCasePreserve)
FN_LOCAL_BOOL(lp_hide_dot_files, bHideDotFiles)
FN_LOCAL_BOOL(lp_hide_special_files, bHideSpecialFiles)
FN_LOCAL_BOOL(lp_hideunreadable, bHideUnReadable)
FN_LOCAL_BOOL(lp_hideunwriteable_files, bHideUnWriteableFiles)
FN_LOCAL_BOOL(lp_browseable, bBrowseable)
FN_LOCAL_BOOL(lp_access_based_share_enum, bAccessBasedShareEnum)
FN_LOCAL_BOOL(lp_readonly, bRead_only)
FN_LOCAL_BOOL(lp_no_set_dir, bNo_set_dir)
FN_LOCAL_BOOL(lp_guest_ok, bGuest_ok)
FN_LOCAL_BOOL(lp_guest_only, bGuest_only)
FN_LOCAL_BOOL(lp_administrative_share, bAdministrative_share)
FN_LOCAL_BOOL(lp_print_ok, bPrint_ok)
FN_LOCAL_BOOL(lp_print_notify_backchannel, bPrintNotifyBackchannel)
FN_LOCAL_BOOL(lp_map_hidden, bMap_hidden)
FN_LOCAL_BOOL(lp_map_archive, bMap_archive)
FN_LOCAL_BOOL(lp_store_dos_attributes, bStoreDosAttributes)
FN_LOCAL_BOOL(lp_dmapi_support, bDmapiSupport)
FN_LOCAL_PARM_BOOL(lp_locking, bLocking)
FN_LOCAL_PARM_INTEGER(lp_strict_locking, iStrictLocking)
FN_LOCAL_PARM_BOOL(lp_posix_locking, bPosixLocking)
FN_LOCAL_BOOL(lp_share_modes, bShareModes)
FN_LOCAL_BOOL(lp_oplocks, bOpLocks)
FN_LOCAL_BOOL(lp_level2_oplocks, bLevel2OpLocks)
FN_LOCAL_BOOL(lp_onlyuser, bOnlyUser)
FN_LOCAL_PARM_BOOL(lp_manglednames, bMangledNames)
FN_LOCAL_BOOL(lp_symlinks, bSymlinks)
FN_LOCAL_BOOL(lp_syncalways, bSyncAlways)
FN_LOCAL_BOOL(lp_strict_allocate, bStrictAllocate)
FN_LOCAL_BOOL(lp_strict_sync, bStrictSync)
FN_LOCAL_BOOL(lp_map_system, bMap_system)
FN_LOCAL_BOOL(lp_delete_readonly, bDeleteReadonly)
FN_LOCAL_BOOL(lp_fake_oplocks, bFakeOplocks)
FN_LOCAL_BOOL(lp_recursive_veto_delete, bDeleteVetoFiles)
FN_LOCAL_BOOL(lp_dos_filemode, bDosFilemode)
FN_LOCAL_BOOL(lp_dos_filetimes, bDosFiletimes)
FN_LOCAL_BOOL(lp_dos_filetime_resolution, bDosFiletimeResolution)
FN_LOCAL_BOOL(lp_fake_dir_create_times, bFakeDirCreateTimes)
FN_GLOBAL_BOOL(lp_async_smb_echo_handler, &Globals.bAsyncSMBEchoHandler)
FN_GLOBAL_BOOL(lp_multicast_dns_register, &Globals.bMulticastDnsRegister)
FN_GLOBAL_BOOL(lp_allow_insecure_widelinks, &Globals.bAllowInsecureWidelinks)
FN_LOCAL_BOOL(lp_blocking_locks, bBlockingLocks)
FN_LOCAL_BOOL(lp_inherit_perms, bInheritPerms)
FN_LOCAL_BOOL(lp_inherit_acls, bInheritACLS)
FN_LOCAL_BOOL(lp_inherit_owner, bInheritOwner)
FN_LOCAL_BOOL(lp_use_client_driver, bUseClientDriver)
FN_LOCAL_BOOL(lp_default_devmode, bDefaultDevmode)
FN_LOCAL_BOOL(lp_force_printername, bForcePrintername)
FN_LOCAL_BOOL(lp_nt_acl_support, bNTAclSupport)
FN_LOCAL_BOOL(lp_force_unknown_acl_user, bForceUnknownAclUser)
FN_LOCAL_BOOL(lp_ea_support, bEASupport)
FN_LOCAL_BOOL(_lp_use_sendfile, bUseSendfile)
FN_LOCAL_BOOL(lp_profile_acls, bProfileAcls)
FN_LOCAL_BOOL(lp_map_acl_inherit, bMap_acl_inherit)
FN_LOCAL_BOOL(lp_afs_share, bAfs_Share)
FN_LOCAL_BOOL(lp_acl_check_permissions, bAclCheckPermissions)
FN_LOCAL_BOOL(lp_acl_group_control, bAclGroupControl)
FN_LOCAL_BOOL(lp_acl_map_full_control, bAclMapFullControl)
FN_LOCAL_INTEGER(lp_create_mask, iCreate_mask)
FN_LOCAL_INTEGER(lp_force_create_mode, iCreate_force_mode)
FN_LOCAL_INTEGER(lp_security_mask, iSecurity_mask)
FN_LOCAL_INTEGER(lp_force_security_mode, iSecurity_force_mode)
FN_LOCAL_INTEGER(lp_dir_mask, iDir_mask)
FN_LOCAL_INTEGER(lp_force_dir_mode, iDir_force_mode)
FN_LOCAL_INTEGER(lp_dir_security_mask, iDir_Security_mask)
FN_LOCAL_INTEGER(lp_force_dir_security_mode, iDir_Security_force_mode)
FN_LOCAL_INTEGER(lp_max_connections, iMaxConnections)
FN_LOCAL_INTEGER(lp_defaultcase, iDefaultCase)
FN_LOCAL_INTEGER(lp_minprintspace, iMinPrintSpace)
FN_LOCAL_INTEGER(lp_printing, iPrinting)
FN_LOCAL_INTEGER(lp_max_reported_jobs, iMaxReportedPrintJobs)
FN_LOCAL_INTEGER(lp_oplock_contention_limit, iOplockContentionLimit)
FN_LOCAL_INTEGER(lp_csc_policy, iCSCPolicy)
FN_LOCAL_INTEGER(lp_write_cache_size, iWriteCacheSize)
FN_LOCAL_INTEGER(lp_block_size, iBlock_size)
FN_LOCAL_INTEGER(lp_dfree_cache_time, iDfreeCacheTime)
FN_LOCAL_INTEGER(lp_allocation_roundup_size, iallocation_roundup_size)
FN_LOCAL_INTEGER(lp_aio_read_size, iAioReadSize)
FN_LOCAL_INTEGER(lp_aio_write_size, iAioWriteSize)
FN_LOCAL_INTEGER(lp_map_readonly, iMap_readonly)
FN_LOCAL_INTEGER(lp_directory_name_cache_size, iDirectoryNameCacheSize)
FN_LOCAL_INTEGER(lp_smb_encrypt, ismb_encrypt)
FN_LOCAL_CHAR(lp_magicchar, magic_char)
FN_GLOBAL_INTEGER(lp_winbind_cache_time, &Globals.winbind_cache_time)
FN_GLOBAL_INTEGER(lp_winbind_reconnect_delay, &Globals.winbind_reconnect_delay)
FN_GLOBAL_INTEGER(lp_winbind_max_clients, &Globals.winbind_max_clients)
FN_GLOBAL_LIST(lp_winbind_nss_info, &Globals.szWinbindNssInfo)
FN_GLOBAL_INTEGER(lp_algorithmic_rid_base, &Globals.AlgorithmicRidBase)
FN_GLOBAL_INTEGER(lp_name_cache_timeout, &Globals.name_cache_timeout)
FN_GLOBAL_INTEGER(lp_client_signing, &Globals.client_signing)
FN_GLOBAL_INTEGER(lp_client_ipc_signing, &Globals.client_ipc_signing)
FN_GLOBAL_INTEGER(lp_server_signing, &Globals.server_signing)
FN_GLOBAL_INTEGER(lp_client_ldap_sasl_wrapping, &Globals.client_ldap_sasl_wrapping)

FN_GLOBAL_STRING(lp_ncalrpc_dir, &Globals.ncalrpc_dir)

/* local prototypes */

static int map_parameter_canonical(const char *pszParmName, bool *inverse);
static const char *get_boolean(bool bool_value);
static int getservicebyname(const char *pszServiceName,
			    struct service *pserviceDest);
static void copy_service(struct service *pserviceDest,
			 struct service *pserviceSource,
			 struct bitmap *pcopymapDest);
static bool do_parameter(const char *pszParmName, const char *pszParmValue,
			 void *userdata);
static bool do_section(const char *pszSectionName, void *userdata);
static void init_copymap(struct service *pservice);
static bool hash_a_service(const char *name, int number);
static void free_service_byindex(int iService);
static void free_param_opts(struct param_opt_struct **popts);
static void show_parameter(int parmIndex);
static bool is_synonym_of(int parm1, int parm2, bool *inverse);

/*
 * This is a helper function for parametrical options support.  It returns a
 * pointer to parametrical option value if it exists or NULL otherwise. Actual
 * parametrical functions are quite simple
 */
static struct param_opt_struct *get_parametrics(int snum, const char *type,
						const char *option)
{
	bool global_section = False;
	char* param_key;
        struct param_opt_struct *data;

	if (snum >= iNumServices) return NULL;

	if (snum < 0) { 
		data = Globals.param_opt;
		global_section = True;
	} else {
		data = ServicePtrs[snum]->param_opt;
	}

	if (asprintf(&param_key, "%s:%s", type, option) == -1) {
		DEBUG(0,("asprintf failed!\n"));
		return NULL;
	}

	while (data) {
		if (strwicmp(data->key, param_key) == 0) {
			string_free(&param_key);
			return data;
		}
		data = data->next;
	}

	if (!global_section) {
		/* Try to fetch the same option but from globals */
		/* but only if we are not already working with Globals */
		data = Globals.param_opt;
		while (data) {
		        if (strwicmp(data->key, param_key) == 0) {
			        string_free(&param_key);
				return data;
			}
			data = data->next;
		}
	}

	string_free(&param_key);

	return NULL;
}


#define MISSING_PARAMETER(name) \
    DEBUG(0, ("%s(): value is NULL or empty!\n", #name))

/*******************************************************************
convenience routine to return int parameters.
********************************************************************/
static int lp_int(const char *s)
{

	if (!s || !*s) {
		MISSING_PARAMETER(lp_int);
		return (-1);
	}

	return (int)strtol(s, NULL, 0);
}

/*******************************************************************
convenience routine to return unsigned long parameters.
********************************************************************/
static unsigned long lp_ulong(const char *s)
{

	if (!s || !*s) {
		MISSING_PARAMETER(lp_ulong);
		return (0);
	}

	return strtoul(s, NULL, 0);
}

/*******************************************************************
convenience routine to return boolean parameters.
********************************************************************/
static bool lp_bool(const char *s)
{
	bool ret = False;

	if (!s || !*s) {
		MISSING_PARAMETER(lp_bool);
		return False;
	}

	if (!set_boolean(s, &ret)) {
		DEBUG(0,("lp_bool(%s): value is not boolean!\n",s));
		return False;
	}

	return ret;
}

/*******************************************************************
convenience routine to return enum parameters.
********************************************************************/
static int lp_enum(const char *s,const struct enum_list *_enum)
{
	int i;

	if (!s || !*s || !_enum) {
		MISSING_PARAMETER(lp_enum);
		return (-1);
	}

	for (i=0; _enum[i].name; i++) {
		if (strequal(_enum[i].name,s))
			return _enum[i].value;
	}

	DEBUG(0,("lp_enum(%s,enum): value is not in enum_list!\n",s));
	return (-1);
}

#undef MISSING_PARAMETER

/* DO NOT USE lp_parm_string ANYMORE!!!!
 * use lp_parm_const_string or lp_parm_talloc_string
 *
 * lp_parm_string is only used to let old modules find this symbol
 */
#undef lp_parm_string
 char *lp_parm_string(const char *servicename, const char *type, const char *option);
 char *lp_parm_string(const char *servicename, const char *type, const char *option)
{
	return lp_parm_talloc_string(lp_servicenumber(servicename), type, option, NULL);
}

/* Return parametric option from a given service. Type is a part of option before ':' */
/* Parametric option has following syntax: 'Type: option = value' */
/* the returned value is talloced on the talloc_tos() */
char *lp_parm_talloc_string(int snum, const char *type, const char *option, const char *def)
{
	struct param_opt_struct *data = get_parametrics(snum, type, option);

	if (data == NULL||data->value==NULL) {
		if (def) {
			return lp_string(def);
		} else {
			return NULL;
		}
	}

	return lp_string(data->value);
}

/* Return parametric option from a given service. Type is a part of option before ':' */
/* Parametric option has following syntax: 'Type: option = value' */
const char *lp_parm_const_string(int snum, const char *type, const char *option, const char *def)
{
	struct param_opt_struct *data = get_parametrics(snum, type, option);

	if (data == NULL||data->value==NULL)
		return def;

	return data->value;
}

/* Return parametric option from a given service. Type is a part of option before ':' */
/* Parametric option has following syntax: 'Type: option = value' */

const char **lp_parm_string_list(int snum, const char *type, const char *option, const char **def)
{
	struct param_opt_struct *data = get_parametrics(snum, type, option);

	if (data == NULL||data->value==NULL)
		return (const char **)def;

	if (data->list==NULL) {
		data->list = str_list_make_v3(NULL, data->value, NULL);
	}

	return (const char **)data->list;
}

/* Return parametric option from a given service. Type is a part of option before ':' */
/* Parametric option has following syntax: 'Type: option = value' */

int lp_parm_int(int snum, const char *type, const char *option, int def)
{
	struct param_opt_struct *data = get_parametrics(snum, type, option);

	if (data && data->value && *data->value)
		return lp_int(data->value);

	return def;
}

/* Return parametric option from a given service. Type is a part of option before ':' */
/* Parametric option has following syntax: 'Type: option = value' */

unsigned long lp_parm_ulong(int snum, const char *type, const char *option, unsigned long def)
{
	struct param_opt_struct *data = get_parametrics(snum, type, option);

	if (data && data->value && *data->value)
		return lp_ulong(data->value);

	return def;
}

/* Return parametric option from a given service. Type is a part of option before ':' */
/* Parametric option has following syntax: 'Type: option = value' */

bool lp_parm_bool(int snum, const char *type, const char *option, bool def)
{
	struct param_opt_struct *data = get_parametrics(snum, type, option);

	if (data && data->value && *data->value)
		return lp_bool(data->value);

	return def;
}

/* Return parametric option from a given service. Type is a part of option before ':' */
/* Parametric option has following syntax: 'Type: option = value' */

int lp_parm_enum(int snum, const char *type, const char *option,
		 const struct enum_list *_enum, int def)
{
	struct param_opt_struct *data = get_parametrics(snum, type, option);

	if (data && data->value && *data->value && _enum)
		return lp_enum(data->value, _enum);

	return def;
}


/***************************************************************************
 Initialise a service to the defaults.
***************************************************************************/

static void init_service(struct service *pservice)
{
	memset((char *)pservice, '\0', sizeof(struct service));
	copy_service(pservice, &sDefault, NULL);
}


/**
 * free a param_opts structure.
 * param_opts handling should be moved to talloc;
 * then this whole functions reduces to a TALLOC_FREE().
 */

static void free_param_opts(struct param_opt_struct **popts)
{
	struct param_opt_struct *opt, *next_opt;

	if (popts == NULL) {
		return;
	}

	if (*popts != NULL) {
		DEBUG(5, ("Freeing parametrics:\n"));
	}
	opt = *popts;
	while (opt != NULL) {
		string_free(&opt->key);
		string_free(&opt->value);
		TALLOC_FREE(opt->list);
		next_opt = opt->next;
		SAFE_FREE(opt);
		opt = next_opt;
	}
	*popts = NULL;
}

/***************************************************************************
 Free the dynamically allocated parts of a service struct.
***************************************************************************/

static void free_service(struct service *pservice)
{
	if (!pservice)
		return;

	if (pservice->szService)
		DEBUG(5, ("free_service: Freeing service %s\n",
		       pservice->szService));

	free_parameters(pservice);

	string_free(&pservice->szService);
	TALLOC_FREE(pservice->copymap);

	free_param_opts(&pservice->param_opt);

	ZERO_STRUCTP(pservice);
}


/***************************************************************************
 remove a service indexed in the ServicePtrs array from the ServiceHash
 and free the dynamically allocated parts
***************************************************************************/

static void free_service_byindex(int idx)
{
	if ( !LP_SNUM_OK(idx) ) 
		return;

	ServicePtrs[idx]->valid = False;
	invalid_services[num_invalid_services++] = idx;

	/* we have to cleanup the hash record */

	if (ServicePtrs[idx]->szService) {
		char *canon_name = canonicalize_servicename(
			talloc_tos(),
			ServicePtrs[idx]->szService );

		dbwrap_delete_bystring(ServiceHash, canon_name );
		TALLOC_FREE(canon_name);
	}

	free_service(ServicePtrs[idx]);
}

/***************************************************************************
 Add a new service to the services array initialising it with the given 
 service. 
***************************************************************************/

static int add_a_service(const struct service *pservice, const char *name)
{
	int i;
	struct service tservice;
	int num_to_alloc = iNumServices + 1;

	tservice = *pservice;

	/* it might already exist */
	if (name) {
		i = getservicebyname(name, NULL);
		if (i >= 0) {
			return (i);
		}
	}

	/* find an invalid one */
	i = iNumServices;
	if (num_invalid_services > 0) {
		i = invalid_services[--num_invalid_services];
	}

	/* if not, then create one */
	if (i == iNumServices) {
		struct service **tsp;
		int *tinvalid;

		tsp = SMB_REALLOC_ARRAY_KEEP_OLD_ON_ERROR(ServicePtrs, struct service *, num_to_alloc);
		if (tsp == NULL) {
			DEBUG(0,("add_a_service: failed to enlarge ServicePtrs!\n"));
			return (-1);
		}
		ServicePtrs = tsp;
		ServicePtrs[iNumServices] = SMB_MALLOC_P(struct service);
		if (!ServicePtrs[iNumServices]) {
			DEBUG(0,("add_a_service: out of memory!\n"));
			return (-1);
		}
		iNumServices++;

		/* enlarge invalid_services here for now... */
		tinvalid = SMB_REALLOC_ARRAY_KEEP_OLD_ON_ERROR(invalid_services, int,
					     num_to_alloc);
		if (tinvalid == NULL) {
			DEBUG(0,("add_a_service: failed to enlarge "
				 "invalid_services!\n"));
			return (-1);
		}
		invalid_services = tinvalid;
	} else {
		free_service_byindex(i);
	}

	ServicePtrs[i]->valid = True;

	init_service(ServicePtrs[i]);
	copy_service(ServicePtrs[i], &tservice, NULL);
	if (name)
		string_set(&ServicePtrs[i]->szService, name);

	DEBUG(8,("add_a_service: Creating snum = %d for %s\n", 
		i, ServicePtrs[i]->szService));

	if (!hash_a_service(ServicePtrs[i]->szService, i)) {
		return (-1);
	}

	return (i);
}

/***************************************************************************
  Convert a string to uppercase and remove whitespaces.
***************************************************************************/

char *canonicalize_servicename(TALLOC_CTX *ctx, const char *src)
{
	char *result;

	if ( !src ) {
		DEBUG(0,("canonicalize_servicename: NULL source name!\n"));
		return NULL;
	}

	result = talloc_strdup(ctx, src);
	SMB_ASSERT(result != NULL);

	strlower_m(result);
	return result;
}

/***************************************************************************
  Add a name/index pair for the services array to the hash table.
***************************************************************************/

static bool hash_a_service(const char *name, int idx)
{
	char *canon_name;

	if ( !ServiceHash ) {
		DEBUG(10,("hash_a_service: creating servicehash\n"));
		ServiceHash = db_open_rbt(NULL);
		if ( !ServiceHash ) {
			DEBUG(0,("hash_a_service: open tdb servicehash failed!\n"));
			return False;
		}
	}

	DEBUG(10,("hash_a_service: hashing index %d for service name %s\n",
		idx, name));

	canon_name = canonicalize_servicename(talloc_tos(), name );

	dbwrap_store_bystring(ServiceHash, canon_name,
			      make_tdb_data((uint8 *)&idx, sizeof(idx)),
			      TDB_REPLACE);

	TALLOC_FREE(canon_name);

	return True;
}

/***************************************************************************
 Add a new home service, with the specified home directory, defaults coming
 from service ifrom.
***************************************************************************/

bool lp_add_home(const char *pszHomename, int iDefaultService,
		 const char *user, const char *pszHomedir)
{
	int i;

	if (pszHomename == NULL || user == NULL || pszHomedir == NULL ||
			pszHomedir[0] == '\0') {
		return false;
	}

	i = add_a_service(ServicePtrs[iDefaultService], pszHomename);

	if (i < 0)
		return (False);

	if (!(*(ServicePtrs[iDefaultService]->szPath))
	    || strequal(ServicePtrs[iDefaultService]->szPath, lp_pathname(GLOBAL_SECTION_SNUM))) {
		string_set(&ServicePtrs[i]->szPath, pszHomedir);
	}

	if (!(*(ServicePtrs[i]->comment))) {
		char *comment = NULL;
		if (asprintf(&comment, "Home directory of %s", user) < 0) {
			return false;
		}
		string_set(&ServicePtrs[i]->comment, comment);
		SAFE_FREE(comment);
	}

	/* set the browseable flag from the global default */

	ServicePtrs[i]->bBrowseable = sDefault.bBrowseable;
	ServicePtrs[i]->bAccessBasedShareEnum = sDefault.bAccessBasedShareEnum;

	ServicePtrs[i]->autoloaded = True;

	DEBUG(3, ("adding home's share [%s] for user '%s' at '%s'\n", pszHomename, 
	       user, ServicePtrs[i]->szPath ));

	return (True);
}

/***************************************************************************
 Add a new service, based on an old one.
***************************************************************************/

int lp_add_service(const char *pszService, int iDefaultService)
{
	if (iDefaultService < 0) {
		return add_a_service(&sDefault, pszService);
	}

	return (add_a_service(ServicePtrs[iDefaultService], pszService));
}

/***************************************************************************
 Add the IPC service.
***************************************************************************/

static bool lp_add_ipc(const char *ipc_name, bool guest_ok)
{
	char *comment = NULL;
	int i = add_a_service(&sDefault, ipc_name);

	if (i < 0)
		return (False);

	if (asprintf(&comment, "IPC Service (%s)",
				Globals.szServerString) < 0) {
		return (False);
	}

	string_set(&ServicePtrs[i]->szPath, tmpdir());
	string_set(&ServicePtrs[i]->szUsername, "");
	string_set(&ServicePtrs[i]->comment, comment);
	string_set(&ServicePtrs[i]->fstype, "IPC");
	ServicePtrs[i]->iMaxConnections = 0;
	ServicePtrs[i]->bAvailable = True;
	ServicePtrs[i]->bRead_only = True;
	ServicePtrs[i]->bGuest_only = False;
	ServicePtrs[i]->bAdministrative_share = True;
	ServicePtrs[i]->bGuest_ok = guest_ok;
	ServicePtrs[i]->bPrint_ok = False;
	ServicePtrs[i]->bBrowseable = sDefault.bBrowseable;

	DEBUG(3, ("adding IPC service\n"));

	SAFE_FREE(comment);
	return (True);
}

/***************************************************************************
 Add a new printer service, with defaults coming from service iFrom.
***************************************************************************/

bool lp_add_printer(const char *pszPrintername, int iDefaultService)
{
	const char *comment = "From Printcap";
	int i = add_a_service(ServicePtrs[iDefaultService], pszPrintername);

	if (i < 0)
		return (False);

	/* note that we do NOT default the availability flag to True - */
	/* we take it from the default service passed. This allows all */
	/* dynamic printers to be disabled by disabling the [printers] */
	/* entry (if/when the 'available' keyword is implemented!).    */

	/* the printer name is set to the service name. */
	string_set(&ServicePtrs[i]->szPrintername, pszPrintername);
	string_set(&ServicePtrs[i]->comment, comment);

	/* set the browseable flag from the gloabl default */
	ServicePtrs[i]->bBrowseable = sDefault.bBrowseable;

	/* Printers cannot be read_only. */
	ServicePtrs[i]->bRead_only = False;
	/* No share modes on printer services. */
	ServicePtrs[i]->bShareModes = False;
	/* No oplocks on printer services. */
	ServicePtrs[i]->bOpLocks = False;
	/* Printer services must be printable. */
	ServicePtrs[i]->bPrint_ok = True;

	DEBUG(3, ("adding printer service %s\n", pszPrintername));

	return (True);
}


/***************************************************************************
 Check whether the given parameter name is valid.
 Parametric options (names containing a colon) are considered valid.
***************************************************************************/

bool lp_parameter_is_valid(const char *pszParmName)
{
	return ((map_parameter(pszParmName) != -1) ||
		(strchr(pszParmName, ':') != NULL));
}

/***************************************************************************
 Check whether the given name is the name of a global parameter.
 Returns True for strings belonging to parameters of class
 P_GLOBAL, False for all other strings, also for parametric options
 and strings not belonging to any option.
***************************************************************************/

bool lp_parameter_is_global(const char *pszParmName)
{
	int num = map_parameter(pszParmName);

	if (num >= 0) {
		return (parm_table[num].p_class == P_GLOBAL);
	}

	return False;
}

/**************************************************************************
 Check whether the given name is the canonical name of a parameter.
 Returns False if it is not a valid parameter Name.
 For parametric options, True is returned.
**************************************************************************/

bool lp_parameter_is_canonical(const char *parm_name)
{
	if (!lp_parameter_is_valid(parm_name)) {
		return False;
	}

	return (map_parameter(parm_name) ==
		map_parameter_canonical(parm_name, NULL));
}

/**************************************************************************
 Determine the canonical name for a parameter.
 Indicate when it is an inverse (boolean) synonym instead of a
 "usual" synonym.
**************************************************************************/

bool lp_canonicalize_parameter(const char *parm_name, const char **canon_parm,
			       bool *inverse)
{
	int num;

	if (!lp_parameter_is_valid(parm_name)) {
		*canon_parm = NULL;
		return False;
	}

	num = map_parameter_canonical(parm_name, inverse);
	if (num < 0) {
		/* parametric option */
		*canon_parm = parm_name;
	} else {
		*canon_parm = parm_table[num].label;
	}

	return True;

}

/**************************************************************************
 Determine the canonical name for a parameter.
 Turn the value given into the inverse boolean expression when
 the synonym is an invers boolean synonym.

 Return True if parm_name is a valid parameter name and
 in case it is an invers boolean synonym, if the val string could
 successfully be converted to the reverse bool.
 Return false in all other cases.
**************************************************************************/

bool lp_canonicalize_parameter_with_value(const char *parm_name,
					  const char *val,
					  const char **canon_parm,
					  const char **canon_val)
{
	int num;
	bool inverse;

	if (!lp_parameter_is_valid(parm_name)) {
		*canon_parm = NULL;
		*canon_val = NULL;
		return False;
	}

	num = map_parameter_canonical(parm_name, &inverse);
	if (num < 0) {
		/* parametric option */
		*canon_parm = parm_name;
		*canon_val = val;
	} else {
		*canon_parm = parm_table[num].label;
		if (inverse) {
			if (!lp_invert_boolean(val, canon_val)) {
				*canon_val = NULL;
				return False;
			}
		} else {
			*canon_val = val;
		}
	}

	return True;
}

/***************************************************************************
 Map a parameter's string representation to something we can use. 
 Returns False if the parameter string is not recognised, else TRUE.
***************************************************************************/

static int map_parameter(const char *pszParmName)
{
	int iIndex;

	if (*pszParmName == '-' && !strequal(pszParmName, "-valid"))
		return (-1);

	for (iIndex = 0; parm_table[iIndex].label; iIndex++)
		if (strwicmp(parm_table[iIndex].label, pszParmName) == 0)
			return (iIndex);

	/* Warn only if it isn't parametric option */
	if (strchr(pszParmName, ':') == NULL)
		DEBUG(1, ("Unknown parameter encountered: \"%s\"\n", pszParmName));
	/* We do return 'fail' for parametric options as well because they are
	   stored in different storage
	 */
	return (-1);
}

/***************************************************************************
 Map a parameter's string representation to the index of the canonical
 form of the parameter (it might be a synonym).
 Returns -1 if the parameter string is not recognised.
***************************************************************************/

static int map_parameter_canonical(const char *pszParmName, bool *inverse)
{
	int parm_num, canon_num;
	bool loc_inverse = False;

	parm_num = map_parameter(pszParmName);
	if ((parm_num < 0) || !(parm_table[parm_num].flags & FLAG_HIDE)) {
		/* invalid, parametric or no canidate for synonyms ... */
		goto done;
	}

	for (canon_num = 0; parm_table[canon_num].label; canon_num++) {
		if (is_synonym_of(parm_num, canon_num, &loc_inverse)) {
			parm_num = canon_num;
			goto done;
		}
	}

done:
	if (inverse != NULL) {
		*inverse = loc_inverse;
	}
	return parm_num;
}

/***************************************************************************
 return true if parameter number parm1 is a synonym of parameter
 number parm2 (parm2 being the principal name).
 set inverse to True if parm1 is P_BOOLREV and parm2 is P_BOOL,
 False otherwise.
***************************************************************************/

static bool is_synonym_of(int parm1, int parm2, bool *inverse)
{
	if ((parm_table[parm1].ptr == parm_table[parm2].ptr) &&
	    (parm_table[parm1].flags & FLAG_HIDE) &&
	    !(parm_table[parm2].flags & FLAG_HIDE))
	{
		if (inverse != NULL) {
			if ((parm_table[parm1].type == P_BOOLREV) &&
			    (parm_table[parm2].type == P_BOOL))
			{
				*inverse = True;
			} else {
				*inverse = False;
			}
		}
		return True;
	}
	return False;
}

/***************************************************************************
 Show one parameter's name, type, [values,] and flags.
 (helper functions for show_parameter_list)
***************************************************************************/

static void show_parameter(int parmIndex)
{
	int enumIndex, flagIndex;
	int parmIndex2;
	bool hadFlag;
	bool hadSyn;
	bool inverse;
	const char *type[] = { "P_BOOL", "P_BOOLREV", "P_CHAR", "P_INTEGER",
		"P_OCTAL", "P_LIST", "P_STRING", "P_USTRING",
		"P_ENUM", "P_SEP"};
	unsigned flags[] = { FLAG_BASIC, FLAG_SHARE, FLAG_PRINT, FLAG_GLOBAL,
		FLAG_WIZARD, FLAG_ADVANCED, FLAG_DEVELOPER, FLAG_DEPRECATED,
		FLAG_HIDE, FLAG_DOS_STRING};
	const char *flag_names[] = { "FLAG_BASIC", "FLAG_SHARE", "FLAG_PRINT",
		"FLAG_GLOBAL", "FLAG_WIZARD", "FLAG_ADVANCED", "FLAG_DEVELOPER",
		"FLAG_DEPRECATED", "FLAG_HIDE", "FLAG_DOS_STRING", NULL};

	printf("%s=%s", parm_table[parmIndex].label,
	       type[parm_table[parmIndex].type]);
	if (parm_table[parmIndex].type == P_ENUM) {
		printf(",");
		for (enumIndex=0;
		     parm_table[parmIndex].enum_list[enumIndex].name;
		     enumIndex++)
		{
			printf("%s%s",
			       enumIndex ? "|" : "",
			       parm_table[parmIndex].enum_list[enumIndex].name);
		}
	}
	printf(",");
	hadFlag = False;
	for (flagIndex=0; flag_names[flagIndex]; flagIndex++) {
		if (parm_table[parmIndex].flags & flags[flagIndex]) {
			printf("%s%s",
				hadFlag ? "|" : "",
				flag_names[flagIndex]);
			hadFlag = True;
		}
	}

	/* output synonyms */
	hadSyn = False;
	for (parmIndex2=0; parm_table[parmIndex2].label; parmIndex2++) {
		if (is_synonym_of(parmIndex, parmIndex2, &inverse)) {
			printf(" (%ssynonym of %s)", inverse ? "inverse " : "",
			       parm_table[parmIndex2].label);
		} else if (is_synonym_of(parmIndex2, parmIndex, &inverse)) {
			if (!hadSyn) {
				printf(" (synonyms: ");
				hadSyn = True;
			} else {
				printf(", ");
			}
			printf("%s%s", parm_table[parmIndex2].label,
			       inverse ? "[i]" : "");
		}
	}
	if (hadSyn) {
		printf(")");
	}

	printf("\n");
}

/***************************************************************************
 Show all parameter's name, type, [values,] and flags.
***************************************************************************/

void show_parameter_list(void)
{
	int classIndex, parmIndex;
	const char *section_names[] = { "local", "global", NULL};

	for (classIndex=0; section_names[classIndex]; classIndex++) {
		printf("[%s]\n", section_names[classIndex]);
		for (parmIndex = 0; parm_table[parmIndex].label; parmIndex++) {
			if (parm_table[parmIndex].p_class == classIndex) {
				show_parameter(parmIndex);
			}
		}
	}
}

/***************************************************************************
 Check if a given string correctly represents a boolean value.
***************************************************************************/

bool lp_string_is_valid_boolean(const char *parm_value)
{
	return set_boolean(parm_value, NULL);
}

/***************************************************************************
 Get the standard string representation of a boolean value ("yes" or "no")
***************************************************************************/

static const char *get_boolean(bool bool_value)
{
	static const char *yes_str = "yes";
	static const char *no_str = "no";

	return (bool_value ? yes_str : no_str);
}

/***************************************************************************
 Provide the string of the negated boolean value associated to the boolean
 given as a string. Returns False if the passed string does not correctly
 represent a boolean.
***************************************************************************/

bool lp_invert_boolean(const char *str, const char **inverse_str)
{
	bool val;

	if (!set_boolean(str, &val)) {
		return False;
	}

	*inverse_str = get_boolean(!val);
	return True;
}

/***************************************************************************
 Provide the canonical string representation of a boolean value given
 as a string. Return True on success, False if the string given does
 not correctly represent a boolean.
***************************************************************************/

bool lp_canonicalize_boolean(const char *str, const char**canon_str)
{
	bool val;

	if (!set_boolean(str, &val)) {
		return False;
	}

	*canon_str = get_boolean(val);
	return True;
}

/***************************************************************************
Find a service by name. Otherwise works like get_service.
***************************************************************************/

static int getservicebyname(const char *pszServiceName, struct service *pserviceDest)
{
	int iService = -1;
	char *canon_name;
	TDB_DATA data;

	if (ServiceHash == NULL) {
		return -1;
	}

	canon_name = canonicalize_servicename(talloc_tos(), pszServiceName);

	data = dbwrap_fetch_bystring(ServiceHash, canon_name, canon_name);

	if ((data.dptr != NULL) && (data.dsize == sizeof(iService))) {
		iService = *(int *)data.dptr;
	}

	TALLOC_FREE(canon_name);

	if ((iService != -1) && (LP_SNUM_OK(iService))
	    && (pserviceDest != NULL)) {
		copy_service(pserviceDest, ServicePtrs[iService], NULL);
	}

	return (iService);
}

/***************************************************************************
 Copy a service structure to another.
 If pcopymapDest is NULL then copy all fields
***************************************************************************/

/**
 * Add a parametric option to a param_opt_struct,
 * replacing old value, if already present.
 */
static void set_param_opt(struct param_opt_struct **opt_list,
			  const char *opt_name,
			  const char *opt_value,
			  unsigned flags)
{
	struct param_opt_struct *new_opt, *opt;
	bool not_added;

	if (opt_list == NULL) {
		return;
	}

	opt = *opt_list;
	not_added = true;

	/* Traverse destination */
	while (opt) {
		/* If we already have same option, override it */
		if (strwicmp(opt->key, opt_name) == 0) {
			if ((opt->flags & FLAG_CMDLINE) &&
			    !(flags & FLAG_CMDLINE)) {
				/* it's been marked as not to be
				   overridden */
				return;
			}
			string_free(&opt->value);
			TALLOC_FREE(opt->list);
			opt->value = SMB_STRDUP(opt_value);
			opt->flags = flags;
			not_added = false;
			break;
		}
		opt = opt->next;
	}
	if (not_added) {
	    new_opt = SMB_XMALLOC_P(struct param_opt_struct);
	    new_opt->key = SMB_STRDUP(opt_name);
	    new_opt->value = SMB_STRDUP(opt_value);
	    new_opt->list = NULL;
	    new_opt->flags = flags;
	    DLIST_ADD(*opt_list, new_opt);
	}
}

static void copy_service(struct service *pserviceDest, struct service *pserviceSource,
			 struct bitmap *pcopymapDest)
{
	int i;
	bool bcopyall = (pcopymapDest == NULL);
	struct param_opt_struct *data;

	for (i = 0; parm_table[i].label; i++)
		if (parm_table[i].ptr && parm_table[i].p_class == P_LOCAL &&
		    (bcopyall || bitmap_query(pcopymapDest,i))) {
			void *def_ptr = parm_table[i].ptr;
			void *src_ptr =
				((char *)pserviceSource) + PTR_DIFF(def_ptr,
								    &sDefault);
			void *dest_ptr =
				((char *)pserviceDest) + PTR_DIFF(def_ptr,
								  &sDefault);

			switch (parm_table[i].type) {
				case P_BOOL:
				case P_BOOLREV:
					*(bool *)dest_ptr = *(bool *)src_ptr;
					break;

				case P_INTEGER:
				case P_ENUM:
				case P_OCTAL:
					*(int *)dest_ptr = *(int *)src_ptr;
					break;

				case P_CHAR:
					*(char *)dest_ptr = *(char *)src_ptr;
					break;

				case P_STRING:
					string_set((char **)dest_ptr,
						   *(char **)src_ptr);
					break;

				case P_USTRING:
					string_set((char **)dest_ptr,
						   *(char **)src_ptr);
					strupper_m(*(char **)dest_ptr);
					break;
				case P_LIST:
					TALLOC_FREE(*((char ***)dest_ptr));
					*((char ***)dest_ptr) = str_list_copy(NULL, 
						      *(const char ***)src_ptr);
					break;
				default:
					break;
			}
		}

	if (bcopyall) {
		init_copymap(pserviceDest);
		if (pserviceSource->copymap)
			bitmap_copy(pserviceDest->copymap,
				    pserviceSource->copymap);
	}

	data = pserviceSource->param_opt;
	while (data) {
		set_param_opt(&pserviceDest->param_opt, data->key, data->value, data->flags);
		data = data->next;
	}
}

/***************************************************************************
Check a service for consistency. Return False if the service is in any way
incomplete or faulty, else True.
***************************************************************************/

bool service_ok(int iService)
{
	bool bRetval;

	bRetval = True;
	if (ServicePtrs[iService]->szService[0] == '\0') {
		DEBUG(0, ("The following message indicates an internal error:\n"));
		DEBUG(0, ("No service name in service entry.\n"));
		bRetval = False;
	}

	/* The [printers] entry MUST be printable. I'm all for flexibility, but */
	/* I can't see why you'd want a non-printable printer service...        */
	if (strwicmp(ServicePtrs[iService]->szService, PRINTERS_NAME) == 0) {
		if (!ServicePtrs[iService]->bPrint_ok) {
			DEBUG(0, ("WARNING: [%s] service MUST be printable!\n",
			       ServicePtrs[iService]->szService));
			ServicePtrs[iService]->bPrint_ok = True;
		}
		/* [printers] service must also be non-browsable. */
		if (ServicePtrs[iService]->bBrowseable)
			ServicePtrs[iService]->bBrowseable = False;
	}

	if (ServicePtrs[iService]->szPath[0] == '\0' &&
	    strwicmp(ServicePtrs[iService]->szService, HOMES_NAME) != 0 &&
	    ServicePtrs[iService]->szMSDfsProxy[0] == '\0'
	    ) {
		DEBUG(0, ("WARNING: No path in service %s - making it unavailable!\n",
			ServicePtrs[iService]->szService));
		ServicePtrs[iService]->bAvailable = False;
	}

	/* If a service is flagged unavailable, log the fact at level 1. */
	if (!ServicePtrs[iService]->bAvailable)
		DEBUG(1, ("NOTE: Service %s is flagged unavailable.\n",
			  ServicePtrs[iService]->szService));

	return (bRetval);
}

static struct smbconf_ctx *lp_smbconf_ctx(void)
{
	sbcErr err;
	static struct smbconf_ctx *conf_ctx = NULL;

	if (conf_ctx == NULL) {
		err = smbconf_init(NULL, &conf_ctx, "registry:");
		if (!SBC_ERROR_IS_OK(err)) {
			DEBUG(1, ("error initializing registry configuration: "
				  "%s\n", sbcErrorString(err)));
			conf_ctx = NULL;
		}
	}

	return conf_ctx;
}

static bool process_smbconf_service(struct smbconf_service *service)
{
	uint32_t count;
	bool ret;

	if (service == NULL) {
		return false;
	}

	ret = do_section(service->name, NULL);
	if (ret != true) {
		return false;
	}
	for (count = 0; count < service->num_params; count++) {
		ret = do_parameter(service->param_names[count],
				   service->param_values[count],
				   NULL);
		if (ret != true) {
			return false;
		}
	}
	if (iServiceIndex >= 0) {
		return service_ok(iServiceIndex);
	}
	return true;
}

/**
 * load a service from registry and activate it
 */
bool process_registry_service(const char *service_name)
{
	sbcErr err;
	struct smbconf_service *service = NULL;
	TALLOC_CTX *mem_ctx = talloc_stackframe();
	struct smbconf_ctx *conf_ctx = lp_smbconf_ctx();
	bool ret = false;

	if (conf_ctx == NULL) {
		goto done;
	}

	DEBUG(5, ("process_registry_service: service name %s\n", service_name));

	if (!smbconf_share_exists(conf_ctx, service_name)) {
		/*
		 * Registry does not contain data for this service (yet),
		 * but make sure lp_load doesn't return false.
		 */
		ret = true;
		goto done;
	}

	err = smbconf_get_share(conf_ctx, mem_ctx, service_name, &service);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

	ret = process_smbconf_service(service);
	if (!ret) {
		goto done;
	}

	/* store the csn */
	smbconf_changed(conf_ctx, &conf_last_csn, NULL, NULL);

done:
	TALLOC_FREE(mem_ctx);
	return ret;
}

/*
 * process_registry_globals
 */
static bool process_registry_globals(void)
{
	bool ret;

	add_to_file_list(INCLUDE_REGISTRY_NAME, INCLUDE_REGISTRY_NAME);

	ret = do_parameter("registry shares", "yes", NULL);
	if (!ret) {
		return ret;
	}

	return process_registry_service(GLOBAL_NAME);
}

bool process_registry_shares(void)
{
	sbcErr err;
	uint32_t count;
	struct smbconf_service **service = NULL;
	uint32_t num_shares = 0;
	TALLOC_CTX *mem_ctx = talloc_stackframe();
	struct smbconf_ctx *conf_ctx = lp_smbconf_ctx();
	bool ret = false;

	if (conf_ctx == NULL) {
		goto done;
	}

	err = smbconf_get_config(conf_ctx, mem_ctx, &num_shares, &service);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

	ret = true;

	for (count = 0; count < num_shares; count++) {
		if (strequal(service[count]->name, GLOBAL_NAME)) {
			continue;
		}
		ret = process_smbconf_service(service[count]);
		if (!ret) {
			goto done;
		}
	}

	/* store the csn */
	smbconf_changed(conf_ctx, &conf_last_csn, NULL, NULL);

done:
	TALLOC_FREE(mem_ctx);
	return ret;
}

/**
 * reload those shares from registry that are already
 * activated in the services array.
 */
static bool reload_registry_shares(void)
{
	int i;
	bool ret = true;

	for (i = 0; i < iNumServices; i++) {
		if (!VALID(i)) {
			continue;
		}

		if (ServicePtrs[i]->usershare == USERSHARE_VALID) {
			continue;
		}

		ret = process_registry_service(ServicePtrs[i]->szService);
		if (!ret) {
			goto done;
		}
	}

done:
	return ret;
}


#define MAX_INCLUDE_DEPTH 100

static uint8_t include_depth;

static struct file_lists {
	struct file_lists *next;
	char *name;
	char *subfname;
	time_t modtime;
} *file_lists = NULL;

/*******************************************************************
 Keep a linked list of all config files so we know when one has changed 
 it's date and needs to be reloaded.
********************************************************************/

static void add_to_file_list(const char *fname, const char *subfname)
{
	struct file_lists *f = file_lists;

	while (f) {
		if (f->name && !strcmp(f->name, fname))
			break;
		f = f->next;
	}

	if (!f) {
		f = SMB_MALLOC_P(struct file_lists);
		if (!f)
			return;
		f->next = file_lists;
		f->name = SMB_STRDUP(fname);
		if (!f->name) {
			SAFE_FREE(f);
			return;
		}
		f->subfname = SMB_STRDUP(subfname);
		if (!f->subfname) {
			SAFE_FREE(f->name);
			SAFE_FREE(f);
			return;
		}
		file_lists = f;
		f->modtime = file_modtime(subfname);
	} else {
		time_t t = file_modtime(subfname);
		if (t)
			f->modtime = t;
	}
	return;
}

/**
 * Free the file lists
 */
static void free_file_list(void)
{
	struct file_lists *f;
	struct file_lists *next;

	f = file_lists;
	while( f ) {
		next = f->next;
		SAFE_FREE( f->name );
		SAFE_FREE( f->subfname );
		SAFE_FREE( f );
		f = next;
	}
	file_lists = NULL;
}


/**
 * Utility function for outsiders to check if we're running on registry.
 */
bool lp_config_backend_is_registry(void)
{
	return (lp_config_backend() == CONFIG_BACKEND_REGISTRY);
}

/**
 * Utility function to check if the config backend is FILE.
 */
bool lp_config_backend_is_file(void)
{
	return (lp_config_backend() == CONFIG_BACKEND_FILE);
}

/*******************************************************************
 Check if a config file has changed date.
********************************************************************/

bool lp_file_list_changed(void)
{
	struct file_lists *f = file_lists;

 	DEBUG(6, ("lp_file_list_changed()\n"));

	while (f) {
		time_t mod_time;

		if (strequal(f->name, INCLUDE_REGISTRY_NAME)) {
			struct smbconf_ctx *conf_ctx = lp_smbconf_ctx();

			if (conf_ctx == NULL) {
				return false;
			}
			if (smbconf_changed(conf_ctx, &conf_last_csn, NULL,
					    NULL))
			{
				DEBUGADD(6, ("registry config changed\n"));
				return true;
			}
		} else {
			char *n2 = NULL;
			n2 = talloc_sub_basic(talloc_tos(),
					      get_current_username(),
					      current_user_info.domain,
					      f->name);
			if (!n2) {
				return false;
			}
			DEBUGADD(6, ("file %s -> %s  last mod_time: %s\n",
				     f->name, n2, ctime(&f->modtime)));

			mod_time = file_modtime(n2);

			if (mod_time &&
			    ((f->modtime != mod_time) ||
			     (f->subfname == NULL) ||
			     (strcmp(n2, f->subfname) != 0)))
			{
				DEBUGADD(6,
					 ("file %s modified: %s\n", n2,
					  ctime(&mod_time)));
				f->modtime = mod_time;
				SAFE_FREE(f->subfname);
				f->subfname = SMB_STRDUP(n2);
				TALLOC_FREE(n2);
				return true;
			}
			TALLOC_FREE(n2);
		}
		f = f->next;
	}
	return (False);
}


/***************************************************************************
 Run standard_sub_basic on netbios name... needed because global_myname
 is not accessed through any lp_ macro.
 Note: We must *NOT* use string_set() here as ptr points to global_myname.
***************************************************************************/

static bool handle_netbios_name(int snum, const char *pszParmValue, char **ptr)
{
	bool ret;
	char *netbios_name = talloc_sub_basic(
		talloc_tos(), get_current_username(), current_user_info.domain,
		pszParmValue);

	ret = set_global_myname(netbios_name);
	TALLOC_FREE(netbios_name);
	string_set(&Globals.szNetbiosName,global_myname());

	DEBUG(4, ("handle_netbios_name: set global_myname to: %s\n",
	       global_myname()));

	return ret;
}

static bool handle_charset(int snum, const char *pszParmValue, char **ptr)
{
	if (strcmp(*ptr, pszParmValue) != 0) {
		string_set(ptr, pszParmValue);
		init_iconv();
	}
	return True;
}

static bool handle_dos_charset(int snum, const char *pszParmValue, char **ptr)
{
	bool is_utf8 = false;
	size_t len = strlen(pszParmValue);

	if (len == 4 || len == 5) {
		/* Don't use StrCaseCmp here as we don't want to
		   initialize iconv. */
		if ((toupper_m(pszParmValue[0]) == 'U') &&
		    (toupper_m(pszParmValue[1]) == 'T') &&
		    (toupper_m(pszParmValue[2]) == 'F')) {
			if (len == 4) {
				if (pszParmValue[3] == '8') {
					is_utf8 = true;
				}
			} else {
				if (pszParmValue[3] == '-' &&
				    pszParmValue[4] == '8') {
					is_utf8 = true;
				}
			}
		}
	}

	if (strcmp(*ptr, pszParmValue) != 0) {
		if (is_utf8) {
			DEBUG(0,("ERROR: invalid DOS charset: 'dos charset' must not "
				"be UTF8, using (default value) %s instead.\n",
				DEFAULT_DOS_CHARSET));
			pszParmValue = DEFAULT_DOS_CHARSET;
		}
		string_set(ptr, pszParmValue);
		init_iconv();
	}
	return True;
}



static bool handle_workgroup(int snum, const char *pszParmValue, char **ptr)
{
	bool ret;

	ret = set_global_myworkgroup(pszParmValue);
	string_set(&Globals.szWorkgroup,lp_workgroup());

	return ret;
}

static bool handle_netbios_scope(int snum, const char *pszParmValue, char **ptr)
{
	bool ret;

	ret = set_global_scope(pszParmValue);
	string_set(&Globals.szNetbiosScope,global_scope());

	return ret;
}

static bool handle_netbios_aliases(int snum, const char *pszParmValue, char **ptr)
{
	TALLOC_FREE(Globals.szNetbiosAliases);
	Globals.szNetbiosAliases = str_list_make_v3(NULL, pszParmValue, NULL);
	return set_netbios_aliases((const char **)Globals.szNetbiosAliases);
}

/***************************************************************************
 Handle the include operation.
***************************************************************************/
static bool bAllowIncludeRegistry = true;

static bool handle_include(int snum, const char *pszParmValue, char **ptr)
{
	char *fname;

	if (include_depth >= MAX_INCLUDE_DEPTH) {
		DEBUG(0, ("Error: Maximum include depth (%u) exceeded!\n",
			  include_depth));
		return false;
	}

	if (strequal(pszParmValue, INCLUDE_REGISTRY_NAME)) {
		if (!bAllowIncludeRegistry) {
			return true;
		}
		if (bInGlobalSection) {
			bool ret;
			include_depth++;
			ret = process_registry_globals();
			include_depth--;
			return ret;
		} else {
			DEBUG(1, ("\"include = registry\" only effective "
				  "in %s section\n", GLOBAL_NAME));
			return false;
		}
	}

	fname = talloc_sub_basic(talloc_tos(), get_current_username(),
				 current_user_info.domain,
				 pszParmValue);

	add_to_file_list(pszParmValue, fname);

	string_set(ptr, fname);

	if (file_exist(fname)) {
		bool ret;
		include_depth++;
		ret = pm_process(fname, do_section, do_parameter, NULL);
		include_depth--;
		TALLOC_FREE(fname);
		return ret;
	}

	DEBUG(2, ("Can't find include file %s\n", fname));
	TALLOC_FREE(fname);
	return true;
}

/***************************************************************************
 Handle the interpretation of the copy parameter.
***************************************************************************/

static bool handle_copy(int snum, const char *pszParmValue, char **ptr)
{
	bool bRetval;
	int iTemp;
	struct service serviceTemp;

	string_set(ptr, pszParmValue);

	init_service(&serviceTemp);

	bRetval = False;

	DEBUG(3, ("Copying service from service %s\n", pszParmValue));

	if ((iTemp = getservicebyname(pszParmValue, &serviceTemp)) >= 0) {
		if (iTemp == iServiceIndex) {
			DEBUG(0, ("Can't copy service %s - unable to copy self!\n", pszParmValue));
		} else {
			copy_service(ServicePtrs[iServiceIndex],
				     &serviceTemp,
				     ServicePtrs[iServiceIndex]->copymap);
			bRetval = True;
		}
	} else {
		DEBUG(0, ("Unable to copy service - source not found: %s\n", pszParmValue));
		bRetval = False;
	}

	free_service(&serviceTemp);
	return (bRetval);
}

static bool handle_ldap_debug_level(int snum, const char *pszParmValue, char **ptr)
{
	Globals.ldap_debug_level = lp_int(pszParmValue);
	init_ldap_debugging();
	return true;
}

/***************************************************************************
 Handle idmap/non unix account uid and gid allocation parameters.  The format of these
 parameters is:

 [global]

        idmap uid = 1000-1999
        idmap gid = 700-899

 We only do simple parsing checks here.  The strings are parsed into useful
 structures in the idmap daemon code.

***************************************************************************/

/* Some lp_ routines to return idmap [ug]id information */

static uid_t idmap_uid_low, idmap_uid_high;
static gid_t idmap_gid_low, idmap_gid_high;

bool lp_idmap_uid(uid_t *low, uid_t *high)
{
        if (idmap_uid_low == 0 || idmap_uid_high == 0)
                return False;

        if (low)
                *low = idmap_uid_low;

        if (high)
                *high = idmap_uid_high;

        return True;
}

bool lp_idmap_gid(gid_t *low, gid_t *high)
{
        if (idmap_gid_low == 0 || idmap_gid_high == 0)
                return False;

        if (low)
                *low = idmap_gid_low;

        if (high)
                *high = idmap_gid_high;

        return True;
}

static bool handle_idmap_backend(int snum, const char *pszParmValue, char **ptr)
{
	lp_do_parameter(snum, "idmap config * : backend", pszParmValue);

	return true;
}

/* Do some simple checks on "idmap [ug]id" parameter values */

static bool handle_idmap_uid(int snum, const char *pszParmValue, char **ptr)
{
	lp_do_parameter(snum, "idmap config * : range", pszParmValue);

	return True;
}

static bool handle_idmap_gid(int snum, const char *pszParmValue, char **ptr)
{
	lp_do_parameter(snum, "idmap config * : range", pszParmValue);

	return True;
}

/***************************************************************************
 Handle the DEBUG level list.
***************************************************************************/

static bool handle_debug_list( int snum, const char *pszParmValueIn, char **ptr )
{
	string_set(ptr, pszParmValueIn);
	return debug_parse_levels(pszParmValueIn);
}

/***************************************************************************
 Handle ldap suffixes - default to ldapsuffix if sub-suffixes are not defined.
***************************************************************************/

static const char *append_ldap_suffix( const char *str )
{
	const char *suffix_string;


	suffix_string = talloc_asprintf(talloc_tos(), "%s,%s", str,
					Globals.szLdapSuffix );
	if ( !suffix_string ) {
		DEBUG(0,("append_ldap_suffix: talloc_asprintf() failed!\n"));
		return "";
	}

	return suffix_string;
}

const char *lp_ldap_machine_suffix(void)
{
	if (Globals.szLdapMachineSuffix[0])
		return append_ldap_suffix(Globals.szLdapMachineSuffix);

	return lp_string(Globals.szLdapSuffix);
}

const char *lp_ldap_user_suffix(void)
{
	if (Globals.szLdapUserSuffix[0])
		return append_ldap_suffix(Globals.szLdapUserSuffix);

	return lp_string(Globals.szLdapSuffix);
}

const char *lp_ldap_group_suffix(void)
{
	if (Globals.szLdapGroupSuffix[0])
		return append_ldap_suffix(Globals.szLdapGroupSuffix);

	return lp_string(Globals.szLdapSuffix);
}

const char *lp_ldap_idmap_suffix(void)
{
	if (Globals.szLdapIdmapSuffix[0])
		return append_ldap_suffix(Globals.szLdapIdmapSuffix);

	return lp_string(Globals.szLdapSuffix);
}

/****************************************************************************
 set the value for a P_ENUM
 ***************************************************************************/

static void lp_set_enum_parm( struct parm_struct *parm, const char *pszParmValue,
                              int *ptr )
{
	int i;

	for (i = 0; parm->enum_list[i].name; i++) {
		if ( strequal(pszParmValue, parm->enum_list[i].name)) {
			*ptr = parm->enum_list[i].value;
			return;
		}
	}
	DEBUG(0, ("WARNING: Ignoring invalid value '%s' for parameter '%s'\n",
		  pszParmValue, parm->label));
}

/***************************************************************************
***************************************************************************/

static bool handle_printing(int snum, const char *pszParmValue, char **ptr)
{
	static int parm_num = -1;
	struct service *s;

	if ( parm_num == -1 )
		parm_num = map_parameter( "printing" );

	lp_set_enum_parm( &parm_table[parm_num], pszParmValue, (int*)ptr );

	if ( snum < 0 )
		s = &sDefault;
	else
		s = ServicePtrs[snum];

	init_printer_values( s );

	return True;
}


/***************************************************************************
 Initialise a copymap.
***************************************************************************/

static void init_copymap(struct service *pservice)
{
	int i;

	TALLOC_FREE(pservice->copymap);

	pservice->copymap = bitmap_talloc(NULL, NUMPARAMETERS);
	if (!pservice->copymap)
		DEBUG(0,
		      ("Couldn't allocate copymap!! (size %d)\n",
		       (int)NUMPARAMETERS));
	else
		for (i = 0; i < NUMPARAMETERS; i++)
			bitmap_set(pservice->copymap, i);
}

/***************************************************************************
 Return the local pointer to a parameter given a service struct and the
 pointer into the default structure.
***************************************************************************/

static void *lp_local_ptr(struct service *service, void *ptr)
{
	return (void *)(((char *)service) + PTR_DIFF(ptr, &sDefault));
}

/***************************************************************************
 Return the local pointer to a parameter given the service number and the 
 pointer into the default structure.
***************************************************************************/

void *lp_local_ptr_by_snum(int snum, void *ptr)
{
	return lp_local_ptr(ServicePtrs[snum], ptr);
}

/***************************************************************************
 Process a parameter for a particular service number. If snum < 0
 then assume we are in the globals.
***************************************************************************/

bool lp_do_parameter(int snum, const char *pszParmName, const char *pszParmValue)
{
	int parmnum, i;
	void *parm_ptr = NULL;	/* where we are going to store the result */
	void *def_ptr = NULL;
	struct param_opt_struct **opt_list;

	parmnum = map_parameter(pszParmName);

	if (parmnum < 0) {
		if (strchr(pszParmName, ':') == NULL) {
			DEBUG(0, ("Ignoring unknown parameter \"%s\"\n",
				  pszParmName));
			return (True);
		}

		/*
		 * We've got a parametric option
		 */

		opt_list = (snum < 0)
			? &Globals.param_opt : &ServicePtrs[snum]->param_opt;
		set_param_opt(opt_list, pszParmName, pszParmValue, 0);

		return (True);
	}

	/* if it's already been set by the command line, then we don't
	   override here */
	if (parm_table[parmnum].flags & FLAG_CMDLINE) {
		return true;
	}

	if (parm_table[parmnum].flags & FLAG_DEPRECATED) {
		DEBUG(1, ("WARNING: The \"%s\" option is deprecated\n",
			  pszParmName));
	}

	def_ptr = parm_table[parmnum].ptr;

	/* we might point at a service, the default service or a global */
	if (snum < 0) {
		parm_ptr = def_ptr;
	} else {
		if (parm_table[parmnum].p_class == P_GLOBAL) {
			DEBUG(0,
			      ("Global parameter %s found in service section!\n",
			       pszParmName));
			return (True);
		}
		parm_ptr = lp_local_ptr_by_snum(snum, def_ptr);
	}

	if (snum >= 0) {
		if (!ServicePtrs[snum]->copymap)
			init_copymap(ServicePtrs[snum]);

		/* this handles the aliases - set the copymap for other entries with
		   the same data pointer */
		for (i = 0; parm_table[i].label; i++)
			if (parm_table[i].ptr == parm_table[parmnum].ptr)
				bitmap_clear(ServicePtrs[snum]->copymap, i);
	}

	/* if it is a special case then go ahead */
	if (parm_table[parmnum].special) {
		return parm_table[parmnum].special(snum, pszParmValue,
						   (char **)parm_ptr);
	}

	/* now switch on the type of variable it is */
	switch (parm_table[parmnum].type)
	{
		case P_BOOL:
			*(bool *)parm_ptr = lp_bool(pszParmValue);
			break;

		case P_BOOLREV:
			*(bool *)parm_ptr = !lp_bool(pszParmValue);
			break;

		case P_INTEGER:
			*(int *)parm_ptr = lp_int(pszParmValue);
			break;

		case P_CHAR:
			*(char *)parm_ptr = *pszParmValue;
			break;

		case P_OCTAL:
			i = sscanf(pszParmValue, "%o", (int *)parm_ptr);
			if ( i != 1 ) {
			    DEBUG ( 0, ("Invalid octal number %s\n", pszParmName ));
			}
			break;

		case P_LIST:
			TALLOC_FREE(*((char ***)parm_ptr));
			*(char ***)parm_ptr = str_list_make_v3(
				NULL, pszParmValue, NULL);
			break;

		case P_STRING:
			string_set((char **)parm_ptr, pszParmValue);
			break;

		case P_USTRING:
			string_set((char **)parm_ptr, pszParmValue);
			strupper_m(*(char **)parm_ptr);
			break;

		case P_ENUM:
			lp_set_enum_parm( &parm_table[parmnum], pszParmValue, (int*)parm_ptr );
			break;
		case P_SEP:
			break;
	}

	return (True);
}

/***************************************************************************
set a parameter, marking it with FLAG_CMDLINE. Parameters marked as
FLAG_CMDLINE won't be overridden by loads from smb.conf.
***************************************************************************/

static bool lp_set_cmdline_helper(const char *pszParmName, const char *pszParmValue, bool store_values)
{
	int parmnum, i;
	parmnum = map_parameter(pszParmName);
	if (parmnum >= 0) {
		parm_table[parmnum].flags &= ~FLAG_CMDLINE;
		if (!lp_do_parameter(-1, pszParmName, pszParmValue)) {
			return false;
		}
		parm_table[parmnum].flags |= FLAG_CMDLINE;

		/* we have to also set FLAG_CMDLINE on aliases.  Aliases must
		 * be grouped in the table, so we don't have to search the
		 * whole table */
		for (i=parmnum-1;i>=0 && parm_table[i].ptr == parm_table[parmnum].ptr;i--) {
			parm_table[i].flags |= FLAG_CMDLINE;
		}
		for (i=parmnum+1;i<NUMPARAMETERS && parm_table[i].ptr == parm_table[parmnum].ptr;i++) {
			parm_table[i].flags |= FLAG_CMDLINE;
		}

		if (store_values) {
			store_lp_set_cmdline(pszParmName, pszParmValue);
		}
		return true;
	}

	/* it might be parametric */
	if (strchr(pszParmName, ':') != NULL) {
		set_param_opt(&Globals.param_opt, pszParmName, pszParmValue, FLAG_CMDLINE);
		if (store_values) {
			store_lp_set_cmdline(pszParmName, pszParmValue);
		}
		return true;
	}

	DEBUG(0, ("Ignoring unknown parameter \"%s\"\n",  pszParmName));
	return true;
}

bool lp_set_cmdline(const char *pszParmName, const char *pszParmValue)
{
	return lp_set_cmdline_helper(pszParmName, pszParmValue, true);
}

/***************************************************************************
 Process a parameter.
***************************************************************************/

static bool do_parameter(const char *pszParmName, const char *pszParmValue,
			 void *userdata)
{
	if (!bInGlobalSection && bGlobalOnly)
		return (True);

	DEBUGADD(4, ("doing parameter %s = %s\n", pszParmName, pszParmValue));

	return (lp_do_parameter(bInGlobalSection ? -2 : iServiceIndex,
				pszParmName, pszParmValue));
}

/*
  set a option from the commandline in 'a=b' format. Use to support --option
*/
bool lp_set_option(const char *option)
{
	char *p, *s;
	bool ret;

	s = talloc_strdup(NULL, option);
	if (!s) {
		return false;
	}

	p = strchr(s, '=');
	if (!p) {
		talloc_free(s);
		return false;
	}

	*p = 0;

	/* skip white spaces after the = sign */
	do {
		p++;
	} while (*p == ' ');

	ret = lp_set_cmdline(s, p);
	talloc_free(s);
	return ret;
}

/**************************************************************************
 Print a parameter of the specified type.
***************************************************************************/

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
			fprintf(f, "%s", BOOLSTR(*(bool *)ptr));
			break;

		case P_BOOLREV:
			fprintf(f, "%s", BOOLSTR(!*(bool *)ptr));
			break;

		case P_INTEGER:
			fprintf(f, "%d", *(int *)ptr);
			break;

		case P_CHAR:
			fprintf(f, "%c", *(char *)ptr);
			break;

		case P_OCTAL: {
			char *o = octal_string(*(int *)ptr);
			fprintf(f, "%s", o);
			TALLOC_FREE(o);
			break;
		}

		case P_LIST:
			if ((char ***)ptr && *(char ***)ptr) {
				char **list = *(char ***)ptr;
				for (; *list; list++) {
					/* surround strings with whitespace in double quotes */
					if ( strchr_m( *list, ' ' ) )
						fprintf(f, "\"%s\"%s", *list, ((*(list+1))?", ":""));
					else
						fprintf(f, "%s%s", *list, ((*(list+1))?", ":""));
				}
			}
			break;

		case P_STRING:
		case P_USTRING:
			if (*(char **)ptr) {
				fprintf(f, "%s", *(char **)ptr);
			}
			break;
		case P_SEP:
			break;
	}
}

/***************************************************************************
 Check if two parameters are equal.
***************************************************************************/

static bool equal_parameter(parm_type type, void *ptr1, void *ptr2)
{
	switch (type) {
		case P_BOOL:
		case P_BOOLREV:
			return (*((bool *)ptr1) == *((bool *)ptr2));

		case P_INTEGER:
		case P_ENUM:
		case P_OCTAL:
			return (*((int *)ptr1) == *((int *)ptr2));

		case P_CHAR:
			return (*((char *)ptr1) == *((char *)ptr2));

		case P_LIST:
			return str_list_equal(*(const char ***)ptr1, *(const char ***)ptr2);

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
		case P_SEP:
			break;
	}
	return (False);
}

/***************************************************************************
 Initialize any local varients in the sDefault table.
***************************************************************************/

void init_locals(void)
{
	/* None as yet. */
}

/***************************************************************************
 Process a new section (service). At this stage all sections are services.
 Later we'll have special sections that permit server parameters to be set.
 Returns True on success, False on failure. 
***************************************************************************/

static bool do_section(const char *pszSectionName, void *userdata)
{
	bool bRetval;
	bool isglobal = ((strwicmp(pszSectionName, GLOBAL_NAME) == 0) ||
			 (strwicmp(pszSectionName, GLOBAL_NAME2) == 0));
	bRetval = False;

	/* if we were in a global section then do the local inits */
	if (bInGlobalSection && !isglobal)
		init_locals();

	/* if we've just struck a global section, note the fact. */
	bInGlobalSection = isglobal;

	/* check for multiple global sections */
	if (bInGlobalSection) {
		DEBUG(3, ("Processing section \"[%s]\"\n", pszSectionName));
		return (True);
	}

	if (!bInGlobalSection && bGlobalOnly)
		return (True);

	/* if we have a current service, tidy it up before moving on */
	bRetval = True;

	if (iServiceIndex >= 0)
		bRetval = service_ok(iServiceIndex);

	/* if all is still well, move to the next record in the services array */
	if (bRetval) {
		/* We put this here to avoid an odd message order if messages are */
		/* issued by the post-processing of a previous section. */
		DEBUG(2, ("Processing section \"[%s]\"\n", pszSectionName));

		if ((iServiceIndex = add_a_service(&sDefault, pszSectionName))
		    < 0) {
			DEBUG(0, ("Failed to add a new service\n"));
			return (False);
		}
		/* Clean all parametric options for service */
		/* They will be added during parsing again */
		free_param_opts(&ServicePtrs[iServiceIndex]->param_opt);
	}

	return (bRetval);
}


/***************************************************************************
 Determine if a partcular base parameter is currentl set to the default value.
***************************************************************************/

static bool is_default(int i)
{
	if (!defaults_saved)
		return False;
	switch (parm_table[i].type) {
		case P_LIST:
			return str_list_equal((const char **)parm_table[i].def.lvalue, 
						*(const char ***)parm_table[i].ptr);
		case P_STRING:
		case P_USTRING:
			return strequal(parm_table[i].def.svalue,
					*(char **)parm_table[i].ptr);
		case P_BOOL:
		case P_BOOLREV:
			return parm_table[i].def.bvalue ==
				*(bool *)parm_table[i].ptr;
		case P_CHAR:
			return parm_table[i].def.cvalue ==
				*(char *)parm_table[i].ptr;
		case P_INTEGER:
		case P_OCTAL:
		case P_ENUM:
			return parm_table[i].def.ivalue ==
				*(int *)parm_table[i].ptr;
		case P_SEP:
			break;
	}
	return False;
}

/***************************************************************************
Display the contents of the global structure.
***************************************************************************/

static void dump_globals(FILE *f)
{
	int i;
	struct param_opt_struct *data;

	fprintf(f, "[global]\n");

	for (i = 0; parm_table[i].label; i++)
		if (parm_table[i].p_class == P_GLOBAL &&
		    !(parm_table[i].flags & FLAG_META) &&
		    parm_table[i].ptr &&
		    (i == 0 || (parm_table[i].ptr != parm_table[i - 1].ptr))) {
			if (defaults_saved && is_default(i))
				continue;
			fprintf(f, "\t%s = ", parm_table[i].label);
			print_parameter(&parm_table[i], parm_table[i].ptr, f);
			fprintf(f, "\n");
	}
	if (Globals.param_opt != NULL) {
		data = Globals.param_opt;
		while(data) {
			fprintf(f, "\t%s = %s\n", data->key, data->value);
			data = data->next;
		}
        }

}

/***************************************************************************
 Return True if a local parameter is currently set to the global default.
***************************************************************************/

bool lp_is_default(int snum, struct parm_struct *parm)
{
	int pdiff = PTR_DIFF(parm->ptr, &sDefault);

	return equal_parameter(parm->type,
			       ((char *)ServicePtrs[snum]) + pdiff,
			       ((char *)&sDefault) + pdiff);
}

/***************************************************************************
 Display the contents of a single services record.
***************************************************************************/

static void dump_a_service(struct service *pService, FILE * f)
{
	int i;
	struct param_opt_struct *data;

	if (pService != &sDefault)
		fprintf(f, "[%s]\n", pService->szService);

	for (i = 0; parm_table[i].label; i++) {

		if (parm_table[i].p_class == P_LOCAL &&
		    !(parm_table[i].flags & FLAG_META) &&
		    parm_table[i].ptr &&
		    (*parm_table[i].label != '-') &&
		    (i == 0 || (parm_table[i].ptr != parm_table[i - 1].ptr))) 
		{
			int pdiff = PTR_DIFF(parm_table[i].ptr, &sDefault);

			if (pService == &sDefault) {
				if (defaults_saved && is_default(i))
					continue;
			} else {
				if (equal_parameter(parm_table[i].type,
						    ((char *)pService) +
						    pdiff,
						    ((char *)&sDefault) +
						    pdiff))
					continue;
			}

			fprintf(f, "\t%s = ", parm_table[i].label);
			print_parameter(&parm_table[i],
					((char *)pService) + pdiff, f);
			fprintf(f, "\n");
		}
	}

		if (pService->param_opt != NULL) {
			data = pService->param_opt;
			while(data) {
				fprintf(f, "\t%s = %s\n", data->key, data->value);
				data = data->next;
			}
        	}
}

/***************************************************************************
 Display the contents of a parameter of a single services record.
***************************************************************************/

bool dump_a_parameter(int snum, char *parm_name, FILE * f, bool isGlobal)
{
	int i;
	bool result = False;
	parm_class p_class;
	unsigned flag = 0;
	fstring local_parm_name;
	char *parm_opt;
	const char *parm_opt_value;

	/* check for parametrical option */
	fstrcpy( local_parm_name, parm_name);
	parm_opt = strchr( local_parm_name, ':');

	if (parm_opt) {
		*parm_opt = '\0';
		parm_opt++;
		if (strlen(parm_opt)) {
			parm_opt_value = lp_parm_const_string( snum,
				local_parm_name, parm_opt, NULL);
			if (parm_opt_value) {
				printf( "%s\n", parm_opt_value);
				result = True;
			}
		}
		return result;
	}

	/* check for a key and print the value */
	if (isGlobal) {
		p_class = P_GLOBAL;
		flag = FLAG_GLOBAL;
	} else
		p_class = P_LOCAL;

	for (i = 0; parm_table[i].label; i++) {
		if (strwicmp(parm_table[i].label, parm_name) == 0 &&
		    !(parm_table[i].flags & FLAG_META) &&
		    (parm_table[i].p_class == p_class || parm_table[i].flags & flag) &&
		    parm_table[i].ptr &&
		    (*parm_table[i].label != '-') &&
		    (i == 0 || (parm_table[i].ptr != parm_table[i - 1].ptr))) 
		{
			void *ptr;

			if (isGlobal) {
				ptr = parm_table[i].ptr;
			} else {
				struct service *pService = ServicePtrs[snum];
				ptr = ((char *)pService) +
					PTR_DIFF(parm_table[i].ptr, &sDefault);
			}

			print_parameter(&parm_table[i],
					ptr, f);
			fprintf(f, "\n");
			result = True;
			break;
		}
	}

	return result;
}

/***************************************************************************
 Return info about the requested parameter (given as a string).
 Return NULL when the string is not a valid parameter name.
***************************************************************************/

struct parm_struct *lp_get_parameter(const char *param_name)
{
	int num = map_parameter(param_name);

	if (num < 0) {
		return NULL;
	}

	return &parm_table[num];
}

/***************************************************************************
 Return info about the next parameter in a service.
 snum==GLOBAL_SECTION_SNUM gives the globals.
 Return NULL when out of parameters.
***************************************************************************/

struct parm_struct *lp_next_parameter(int snum, int *i, int allparameters)
{
	if (snum < 0) {
		/* do the globals */
		for (; parm_table[*i].label; (*i)++) {
			if (parm_table[*i].p_class == P_SEPARATOR)
				return &parm_table[(*i)++];

			if (!parm_table[*i].ptr
			    || (*parm_table[*i].label == '-'))
				continue;

			if ((*i) > 0
			    && (parm_table[*i].ptr ==
				parm_table[(*i) - 1].ptr))
				continue;

			if (is_default(*i) && !allparameters)
				continue;

			return &parm_table[(*i)++];
		}
	} else {
		struct service *pService = ServicePtrs[snum];

		for (; parm_table[*i].label; (*i)++) {
			if (parm_table[*i].p_class == P_SEPARATOR)
				return &parm_table[(*i)++];

			if (parm_table[*i].p_class == P_LOCAL &&
			    parm_table[*i].ptr &&
			    (*parm_table[*i].label != '-') &&
			    ((*i) == 0 ||
			     (parm_table[*i].ptr !=
			      parm_table[(*i) - 1].ptr)))
			{
				int pdiff =
					PTR_DIFF(parm_table[*i].ptr,
						 &sDefault);

				if (allparameters ||
				    !equal_parameter(parm_table[*i].type,
						     ((char *)pService) +
						     pdiff,
						     ((char *)&sDefault) +
						     pdiff))
				{
					return &parm_table[(*i)++];
				}
			}
		}
	}

	return NULL;
}


#if 0
/***************************************************************************
 Display the contents of a single copy structure.
***************************************************************************/
static void dump_copy_map(bool *pcopymap)
{
	int i;
	if (!pcopymap)
		return;

	printf("\n\tNon-Copied parameters:\n");

	for (i = 0; parm_table[i].label; i++)
		if (parm_table[i].p_class == P_LOCAL &&
		    parm_table[i].ptr && !pcopymap[i] &&
		    (i == 0 || (parm_table[i].ptr != parm_table[i - 1].ptr)))
		{
			printf("\t\t%s\n", parm_table[i].label);
		}
}
#endif

/***************************************************************************
 Return TRUE if the passed service number is within range.
***************************************************************************/

bool lp_snum_ok(int iService)
{
	return (LP_SNUM_OK(iService) && ServicePtrs[iService]->bAvailable);
}

/***************************************************************************
 Auto-load some home services.
***************************************************************************/

static void lp_add_auto_services(char *str)
{
	char *s;
	char *p;
	int homes;
	char *saveptr;

	if (!str)
		return;

	s = SMB_STRDUP(str);
	if (!s)
		return;

	homes = lp_servicenumber(HOMES_NAME);

	for (p = strtok_r(s, LIST_SEP, &saveptr); p;
	     p = strtok_r(NULL, LIST_SEP, &saveptr)) {
		char *home;

		if (lp_servicenumber(p) >= 0)
			continue;

		home = get_user_home_dir(talloc_tos(), p);

		if (home && home[0] && homes >= 0)
			lp_add_home(p, homes, p, home);

		TALLOC_FREE(home);
	}
	SAFE_FREE(s);
}

/***************************************************************************
 Auto-load one printer.
***************************************************************************/

void lp_add_one_printer(const char *name, const char *comment,
			const char *location, void *pdata)
{
	int printers = lp_servicenumber(PRINTERS_NAME);
	int i;

	if (lp_servicenumber(name) < 0) {
		lp_add_printer(name, printers);
		if ((i = lp_servicenumber(name)) >= 0) {
			string_set(&ServicePtrs[i]->comment, comment);
			ServicePtrs[i]->autoloaded = True;
		}
	}
}

/***************************************************************************
 Have we loaded a services file yet?
***************************************************************************/

bool lp_loaded(void)
{
	return (bLoaded);
}

/***************************************************************************
 Unload unused services.
***************************************************************************/

void lp_killunused(bool (*snumused) (int))
{
	int i;
	for (i = 0; i < iNumServices; i++) {
		if (!VALID(i))
			continue;

		/* don't kill autoloaded or usershare services */
		if ( ServicePtrs[i]->autoloaded ||
				ServicePtrs[i]->usershare == USERSHARE_VALID) {
			continue;
		}

		if (!snumused || !snumused(i)) {
			free_service_byindex(i);
		}
	}
}

/**
 * Kill all except autoloaded and usershare services - convenience wrapper
 */
void lp_kill_all_services(void)
{
	lp_killunused(NULL);
}

/***************************************************************************
 Unload a service.
***************************************************************************/

void lp_killservice(int iServiceIn)
{
	if (VALID(iServiceIn)) {
		free_service_byindex(iServiceIn);
	}
}

/***************************************************************************
 Save the curent values of all global and sDefault parameters into the 
 defaults union. This allows swat and testparm to show only the
 changed (ie. non-default) parameters.
***************************************************************************/

static void lp_save_defaults(void)
{
	int i;
	for (i = 0; parm_table[i].label; i++) {
		if (i > 0 && parm_table[i].ptr == parm_table[i - 1].ptr)
			continue;
		switch (parm_table[i].type) {
			case P_LIST:
				parm_table[i].def.lvalue = str_list_copy(
					NULL, *(const char ***)parm_table[i].ptr);
				break;
			case P_STRING:
			case P_USTRING:
				if (parm_table[i].ptr) {
					parm_table[i].def.svalue = SMB_STRDUP(*(char **)parm_table[i].ptr);
				} else {
					parm_table[i].def.svalue = NULL;
				}
				break;
			case P_BOOL:
			case P_BOOLREV:
				parm_table[i].def.bvalue =
					*(bool *)parm_table[i].ptr;
				break;
			case P_CHAR:
				parm_table[i].def.cvalue =
					*(char *)parm_table[i].ptr;
				break;
			case P_INTEGER:
			case P_OCTAL:
			case P_ENUM:
				parm_table[i].def.ivalue =
					*(int *)parm_table[i].ptr;
				break;
			case P_SEP:
				break;
		}
	}
	defaults_saved = True;
}

/***********************************************************
 If we should send plaintext/LANMAN passwords in the clinet
************************************************************/

static void set_allowed_client_auth(void)
{
	if (Globals.bClientNTLMv2Auth) {
		Globals.bClientLanManAuth = False;
	}
	if (!Globals.bClientLanManAuth) {
		Globals.bClientPlaintextAuth = False;
	}
}

/***************************************************************************
 JRA.
 The following code allows smbd to read a user defined share file.
 Yes, this is my intent. Yes, I'm comfortable with that...

 THE FOLLOWING IS SECURITY CRITICAL CODE.

 It washes your clothes, it cleans your house, it guards you while you sleep...
 Do not f%^k with it....
***************************************************************************/

#define MAX_USERSHARE_FILE_SIZE (10*1024)

/***************************************************************************
 Check allowed stat state of a usershare file.
 Ensure we print out who is dicking with us so the admin can
 get their sorry ass fired.
***************************************************************************/

static bool check_usershare_stat(const char *fname,
				 const SMB_STRUCT_STAT *psbuf)
{
	if (!S_ISREG(psbuf->st_ex_mode)) {
		DEBUG(0,("check_usershare_stat: file %s owned by uid %u is "
			"not a regular file\n",
			fname, (unsigned int)psbuf->st_ex_uid ));
		return False;
	}

	/* Ensure this doesn't have the other write bit set. */
	if (psbuf->st_ex_mode & S_IWOTH) {
		DEBUG(0,("check_usershare_stat: file %s owned by uid %u allows "
			"public write. Refusing to allow as a usershare file.\n",
			fname, (unsigned int)psbuf->st_ex_uid ));
		return False;
	}

	/* Should be 10k or less. */
	if (psbuf->st_ex_size > MAX_USERSHARE_FILE_SIZE) {
		DEBUG(0,("check_usershare_stat: file %s owned by uid %u is "
			"too large (%u) to be a user share file.\n",
			fname, (unsigned int)psbuf->st_ex_uid,
			(unsigned int)psbuf->st_ex_size ));
		return False;
	}

	return True;
}

/***************************************************************************
 Parse the contents of a usershare file.
***************************************************************************/

enum usershare_err parse_usershare_file(TALLOC_CTX *ctx,
			SMB_STRUCT_STAT *psbuf,
			const char *servicename,
			int snum,
			char **lines,
			int numlines,
			char **pp_sharepath,
			char **pp_comment,
			char **pp_cp_servicename,
			struct security_descriptor **ppsd,
			bool *pallow_guest)
{
	const char **prefixallowlist = lp_usershare_prefix_allow_list();
	const char **prefixdenylist = lp_usershare_prefix_deny_list();
	int us_vers;
	SMB_STRUCT_DIR *dp;
	SMB_STRUCT_STAT sbuf;
	char *sharepath = NULL;
	char *comment = NULL;

	*pp_sharepath = NULL;
	*pp_comment = NULL;

	*pallow_guest = False;

	if (numlines < 4) {
		return USERSHARE_MALFORMED_FILE;
	}

	if (strcmp(lines[0], "#VERSION 1") == 0) {
		us_vers = 1;
	} else if (strcmp(lines[0], "#VERSION 2") == 0) {
		us_vers = 2;
		if (numlines < 5) {
			return USERSHARE_MALFORMED_FILE;
		}
	} else {
		return USERSHARE_BAD_VERSION;
	}

	if (strncmp(lines[1], "path=", 5) != 0) {
		return USERSHARE_MALFORMED_PATH;
	}

	sharepath = talloc_strdup(ctx, &lines[1][5]);
	if (!sharepath) {
		return USERSHARE_POSIX_ERR;
	}
	trim_string(sharepath, " ", " ");

	if (strncmp(lines[2], "comment=", 8) != 0) {
		return USERSHARE_MALFORMED_COMMENT_DEF;
	}

	comment = talloc_strdup(ctx, &lines[2][8]);
	if (!comment) {
		return USERSHARE_POSIX_ERR;
	}
	trim_string(comment, " ", " ");
	trim_char(comment, '"', '"');

	if (strncmp(lines[3], "usershare_acl=", 14) != 0) {
		return USERSHARE_MALFORMED_ACL_DEF;
	}

	if (!parse_usershare_acl(ctx, &lines[3][14], ppsd)) {
		return USERSHARE_ACL_ERR;
	}

	if (us_vers == 2) {
		if (strncmp(lines[4], "guest_ok=", 9) != 0) {
			return USERSHARE_MALFORMED_ACL_DEF;
		}
		if (lines[4][9] == 'y') {
			*pallow_guest = True;
		}

		/* Backwards compatible extension to file version #2. */
		if (numlines > 5) {
			if (strncmp(lines[5], "sharename=", 10) != 0) {
				return USERSHARE_MALFORMED_SHARENAME_DEF;
			}
			if (!strequal(&lines[5][10], servicename)) {
				return USERSHARE_BAD_SHARENAME;
			}
			*pp_cp_servicename = talloc_strdup(ctx, &lines[5][10]);
			if (!*pp_cp_servicename) {
				return USERSHARE_POSIX_ERR;
			}
		}
	}

	if (*pp_cp_servicename == NULL) {
		*pp_cp_servicename = talloc_strdup(ctx, servicename);
		if (!*pp_cp_servicename) {
			return USERSHARE_POSIX_ERR;
		}
	}

	if (snum != -1 && (strcmp(sharepath, ServicePtrs[snum]->szPath) == 0)) {
		/* Path didn't change, no checks needed. */
		*pp_sharepath = sharepath;
		*pp_comment = comment;
		return USERSHARE_OK;
	}

	/* The path *must* be absolute. */
	if (sharepath[0] != '/') {
		DEBUG(2,("parse_usershare_file: share %s: path %s is not an absolute path.\n",
			servicename, sharepath));
		return USERSHARE_PATH_NOT_ABSOLUTE;
	}

	/* If there is a usershare prefix deny list ensure one of these paths
	   doesn't match the start of the user given path. */
	if (prefixdenylist) {
		int i;
		for ( i=0; prefixdenylist[i]; i++ ) {
			DEBUG(10,("parse_usershare_file: share %s : checking prefixdenylist[%d]='%s' against %s\n",
				servicename, i, prefixdenylist[i], sharepath ));
			if (memcmp( sharepath, prefixdenylist[i], strlen(prefixdenylist[i])) == 0) {
				DEBUG(2,("parse_usershare_file: share %s path %s starts with one of the "
					"usershare prefix deny list entries.\n",
					servicename, sharepath));
				return USERSHARE_PATH_IS_DENIED;
			}
		}
	}

	/* If there is a usershare prefix allow list ensure one of these paths
	   does match the start of the user given path. */

	if (prefixallowlist) {
		int i;
		for ( i=0; prefixallowlist[i]; i++ ) {
			DEBUG(10,("parse_usershare_file: share %s checking prefixallowlist[%d]='%s' against %s\n",
				servicename, i, prefixallowlist[i], sharepath ));
			if (memcmp( sharepath, prefixallowlist[i], strlen(prefixallowlist[i])) == 0) {
				break;
			}
		}
		if (prefixallowlist[i] == NULL) {
			DEBUG(2,("parse_usershare_file: share %s path %s doesn't start with one of the "
				"usershare prefix allow list entries.\n",
				servicename, sharepath));
			return USERSHARE_PATH_NOT_ALLOWED;
		}
        }

	/* Ensure this is pointing to a directory. */
	dp = sys_opendir(sharepath);

	if (!dp) {
		DEBUG(2,("parse_usershare_file: share %s path %s is not a directory.\n",
			servicename, sharepath));
		return USERSHARE_PATH_NOT_DIRECTORY;
	}

	/* Ensure the owner of the usershare file has permission to share
	   this directory. */

	if (sys_stat(sharepath, &sbuf, false) == -1) {
		DEBUG(2,("parse_usershare_file: share %s : stat failed on path %s. %s\n",
			servicename, sharepath, strerror(errno) ));
		sys_closedir(dp);
		return USERSHARE_POSIX_ERR;
	}

	sys_closedir(dp);

	if (!S_ISDIR(sbuf.st_ex_mode)) {
		DEBUG(2,("parse_usershare_file: share %s path %s is not a directory.\n",
			servicename, sharepath ));
		return USERSHARE_PATH_NOT_DIRECTORY;
	}

	/* Check if sharing is restricted to owner-only. */
	/* psbuf is the stat of the usershare definition file,
	   sbuf is the stat of the target directory to be shared. */

	if (lp_usershare_owner_only()) {
		/* root can share anything. */
		if ((psbuf->st_ex_uid != 0) && (sbuf.st_ex_uid != psbuf->st_ex_uid)) {
			return USERSHARE_PATH_NOT_ALLOWED;
		}
	}

	*pp_sharepath = sharepath;
	*pp_comment = comment;
	return USERSHARE_OK;
}

/***************************************************************************
 Deal with a usershare file.
 Returns:
	>= 0 - snum
	-1 - Bad name, invalid contents.
	   - service name already existed and not a usershare, problem
	    with permissions to share directory etc.
***************************************************************************/

static int process_usershare_file(const char *dir_name, const char *file_name, int snum_template)
{
	SMB_STRUCT_STAT sbuf;
	SMB_STRUCT_STAT lsbuf;
	char *fname = NULL;
	char *sharepath = NULL;
	char *comment = NULL;
	char *cp_service_name = NULL;
	char **lines = NULL;
	int numlines = 0;
	int fd = -1;
	int iService = -1;
	TALLOC_CTX *ctx = talloc_stackframe();
	struct security_descriptor *psd = NULL;
	bool guest_ok = False;
	char *canon_name = NULL;
	bool added_service = false;
	int ret = -1;

	/* Ensure share name doesn't contain invalid characters. */
	if (!validate_net_name(file_name, INVALID_SHARENAME_CHARS, strlen(file_name))) {
		DEBUG(0,("process_usershare_file: share name %s contains "
			"invalid characters (any of %s)\n",
			file_name, INVALID_SHARENAME_CHARS ));
		goto out;
	}

	canon_name = canonicalize_servicename(ctx, file_name);
	if (!canon_name) {
		goto out;
	}

	fname = talloc_asprintf(ctx, "%s/%s", dir_name, file_name);
	if (!fname) {
		goto out;
	}

	/* Minimize the race condition by doing an lstat before we
	   open and fstat. Ensure this isn't a symlink link. */

	if (sys_lstat(fname, &lsbuf, false) != 0) {
		DEBUG(0,("process_usershare_file: stat of %s failed. %s\n",
			fname, strerror(errno) ));
		goto out;
	}

	/* This must be a regular file, not a symlink, directory or
	   other strange filetype. */
	if (!check_usershare_stat(fname, &lsbuf)) {
		goto out;
	}

	{
		TDB_DATA data = dbwrap_fetch_bystring(
			ServiceHash, canon_name, canon_name);

		iService = -1;

		if ((data.dptr != NULL) && (data.dsize == sizeof(iService))) {
			iService = *(int *)data.dptr;
		}
	}

	if (iService != -1 &&
	    timespec_compare(&ServicePtrs[iService]->usershare_last_mod,
			     &lsbuf.st_ex_mtime) == 0) {
		/* Nothing changed - Mark valid and return. */
		DEBUG(10,("process_usershare_file: service %s not changed.\n",
			canon_name ));
		ServicePtrs[iService]->usershare = USERSHARE_VALID;
		ret = iService;
		goto out;
	}

	/* Try and open the file read only - no symlinks allowed. */
#ifdef O_NOFOLLOW
	fd = sys_open(fname, O_RDONLY|O_NOFOLLOW, 0);
#else
	fd = sys_open(fname, O_RDONLY, 0);
#endif

	if (fd == -1) {
		DEBUG(0,("process_usershare_file: unable to open %s. %s\n",
			fname, strerror(errno) ));
		goto out;
	}

	/* Now fstat to be *SURE* it's a regular file. */
	if (sys_fstat(fd, &sbuf, false) != 0) {
		close(fd);
		DEBUG(0,("process_usershare_file: fstat of %s failed. %s\n",
			fname, strerror(errno) ));
		goto out;
	}

	/* Is it the same dev/inode as was lstated ? */
	if (lsbuf.st_ex_dev != sbuf.st_ex_dev || lsbuf.st_ex_ino != sbuf.st_ex_ino) {
		close(fd);
		DEBUG(0,("process_usershare_file: fstat of %s is a different file from lstat. "
			"Symlink spoofing going on ?\n", fname ));
		goto out;
	}

	/* This must be a regular file, not a symlink, directory or
	   other strange filetype. */
	if (!check_usershare_stat(fname, &sbuf)) {
		goto out;
	}

	lines = fd_lines_load(fd, &numlines, MAX_USERSHARE_FILE_SIZE, NULL);

	close(fd);
	if (lines == NULL) {
		DEBUG(0,("process_usershare_file: loading file %s owned by %u failed.\n",
			fname, (unsigned int)sbuf.st_ex_uid ));
		goto out;
	}

	if (parse_usershare_file(ctx, &sbuf, file_name,
			iService, lines, numlines, &sharepath,
			&comment, &cp_service_name,
			&psd, &guest_ok) != USERSHARE_OK) {
		goto out;
	}

	/* Everything ok - add the service possibly using a template. */
	if (iService < 0) {
		const struct service *sp = &sDefault;
		if (snum_template != -1) {
			sp = ServicePtrs[snum_template];
		}

		if ((iService = add_a_service(sp, cp_service_name)) < 0) {
			DEBUG(0, ("process_usershare_file: Failed to add "
				"new service %s\n", cp_service_name));
			goto out;
		}

		added_service = true;

		/* Read only is controlled by usershare ACL below. */
		ServicePtrs[iService]->bRead_only = False;
	}

	/* Write the ACL of the new/modified share. */
	if (!set_share_security(canon_name, psd)) {
		 DEBUG(0, ("process_usershare_file: Failed to set share "
			"security for user share %s\n",
			canon_name ));
		goto out;
	}

	/* If from a template it may be marked invalid. */
	ServicePtrs[iService]->valid = True;

	/* Set the service as a valid usershare. */
	ServicePtrs[iService]->usershare = USERSHARE_VALID;

	/* Set guest access. */
	if (lp_usershare_allow_guests()) {
		ServicePtrs[iService]->bGuest_ok = guest_ok;
	}

	/* And note when it was loaded. */
	ServicePtrs[iService]->usershare_last_mod = sbuf.st_ex_mtime;
	string_set(&ServicePtrs[iService]->szPath, sharepath);
	string_set(&ServicePtrs[iService]->comment, comment);

	ret = iService;

  out:

	if (ret == -1 && iService != -1 && added_service) {
		lp_remove_service(iService);
	}

	TALLOC_FREE(lines);
	TALLOC_FREE(ctx);
	return ret;
}

/***************************************************************************
 Checks if a usershare entry has been modified since last load.
***************************************************************************/

static bool usershare_exists(int iService, struct timespec *last_mod)
{
	SMB_STRUCT_STAT lsbuf;
	const char *usersharepath = Globals.szUsersharePath;
	char *fname;

	if (asprintf(&fname, "%s/%s",
				usersharepath,
				ServicePtrs[iService]->szService) < 0) {
		return false;
	}

	if (sys_lstat(fname, &lsbuf, false) != 0) {
		SAFE_FREE(fname);
		return false;
	}

	if (!S_ISREG(lsbuf.st_ex_mode)) {
		SAFE_FREE(fname);
		return false;
	}

	SAFE_FREE(fname);
	*last_mod = lsbuf.st_ex_mtime;
	return true;
}

/***************************************************************************
 Load a usershare service by name. Returns a valid servicenumber or -1.
***************************************************************************/

int load_usershare_service(const char *servicename)
{
	SMB_STRUCT_STAT sbuf;
	const char *usersharepath = Globals.szUsersharePath;
	int max_user_shares = Globals.iUsershareMaxShares;
	int snum_template = -1;

	if (*usersharepath == 0 ||  max_user_shares == 0) {
		return -1;
	}

	if (sys_stat(usersharepath, &sbuf, false) != 0) {
		DEBUG(0,("load_usershare_service: stat of %s failed. %s\n",
			usersharepath, strerror(errno) ));
		return -1;
	}

	if (!S_ISDIR(sbuf.st_ex_mode)) {
		DEBUG(0,("load_usershare_service: %s is not a directory.\n",
			usersharepath ));
		return -1;
	}

	/*
	 * This directory must be owned by root, and have the 't' bit set.
	 * It also must not be writable by "other".
	 */

#ifdef S_ISVTX
	if (sbuf.st_ex_uid != 0 || !(sbuf.st_ex_mode & S_ISVTX) || (sbuf.st_ex_mode & S_IWOTH)) {
#else
	if (sbuf.st_ex_uid != 0 || (sbuf.st_ex_mode & S_IWOTH)) {
#endif
		DEBUG(0,("load_usershare_service: directory %s is not owned by root "
			"or does not have the sticky bit 't' set or is writable by anyone.\n",
			usersharepath ));
		return -1;
	}

	/* Ensure the template share exists if it's set. */
	if (Globals.szUsershareTemplateShare[0]) {
		/* We can't use lp_servicenumber here as we are recommending that
		   template shares have -valid=False set. */
		for (snum_template = iNumServices - 1; snum_template >= 0; snum_template--) {
			if (ServicePtrs[snum_template]->szService &&
					strequal(ServicePtrs[snum_template]->szService,
						Globals.szUsershareTemplateShare)) {
				break;
			}
		}

		if (snum_template == -1) {
			DEBUG(0,("load_usershare_service: usershare template share %s "
				"does not exist.\n",
				Globals.szUsershareTemplateShare ));
			return -1;
		}
	}

	return process_usershare_file(usersharepath, servicename, snum_template);
}

/***************************************************************************
 Load all user defined shares from the user share directory.
 We only do this if we're enumerating the share list.
 This is the function that can delete usershares that have
 been removed.
***************************************************************************/

int load_usershare_shares(void)
{
	SMB_STRUCT_DIR *dp;
	SMB_STRUCT_STAT sbuf;
	SMB_STRUCT_DIRENT *de;
	int num_usershares = 0;
	int max_user_shares = Globals.iUsershareMaxShares;
	unsigned int num_dir_entries, num_bad_dir_entries, num_tmp_dir_entries;
	unsigned int allowed_bad_entries = ((2*max_user_shares)/10);
	unsigned int allowed_tmp_entries = ((2*max_user_shares)/10);
	int iService;
	int snum_template = -1;
	const char *usersharepath = Globals.szUsersharePath;
	int ret = lp_numservices();

	if (max_user_shares == 0 || *usersharepath == '\0') {
		return lp_numservices();
	}

	if (sys_stat(usersharepath, &sbuf, false) != 0) {
		DEBUG(0,("load_usershare_shares: stat of %s failed. %s\n",
			usersharepath, strerror(errno) ));
		return ret;
	}

	/*
	 * This directory must be owned by root, and have the 't' bit set.
	 * It also must not be writable by "other".
	 */

#ifdef S_ISVTX
	if (sbuf.st_ex_uid != 0 || !(sbuf.st_ex_mode & S_ISVTX) || (sbuf.st_ex_mode & S_IWOTH)) {
#else
	if (sbuf.st_ex_uid != 0 || (sbuf.st_ex_mode & S_IWOTH)) {
#endif
		DEBUG(0,("load_usershare_shares: directory %s is not owned by root "
			"or does not have the sticky bit 't' set or is writable by anyone.\n",
			usersharepath ));
		return ret;
	}

	/* Ensure the template share exists if it's set. */
	if (Globals.szUsershareTemplateShare[0]) {
		/* We can't use lp_servicenumber here as we are recommending that
		   template shares have -valid=False set. */
		for (snum_template = iNumServices - 1; snum_template >= 0; snum_template--) {
			if (ServicePtrs[snum_template]->szService &&
					strequal(ServicePtrs[snum_template]->szService,
						Globals.szUsershareTemplateShare)) {
				break;
			}
		}

		if (snum_template == -1) {
			DEBUG(0,("load_usershare_shares: usershare template share %s "
				"does not exist.\n",
				Globals.szUsershareTemplateShare ));
			return ret;
		}
	}

	/* Mark all existing usershares as pending delete. */
	for (iService = iNumServices - 1; iService >= 0; iService--) {
		if (VALID(iService) && ServicePtrs[iService]->usershare) {
			ServicePtrs[iService]->usershare = USERSHARE_PENDING_DELETE;
		}
	}

	dp = sys_opendir(usersharepath);
	if (!dp) {
		DEBUG(0,("load_usershare_shares:: failed to open directory %s. %s\n",
			usersharepath, strerror(errno) ));
		return ret;
	}

	for (num_dir_entries = 0, num_bad_dir_entries = 0, num_tmp_dir_entries = 0;
			(de = sys_readdir(dp));
			num_dir_entries++ ) {
		int r;
		const char *n = de->d_name;

		/* Ignore . and .. */
		if (*n == '.') {
			if ((n[1] == '\0') || (n[1] == '.' && n[2] == '\0')) {
				continue;
			}
		}

		if (n[0] == ':') {
			/* Temporary file used when creating a share. */
			num_tmp_dir_entries++;
		}

		/* Allow 20% tmp entries. */
		if (num_tmp_dir_entries > allowed_tmp_entries) {
			DEBUG(0,("load_usershare_shares: too many temp entries (%u) "
				"in directory %s\n",
				num_tmp_dir_entries, usersharepath));
			break;
		}

		r = process_usershare_file(usersharepath, n, snum_template);
		if (r == 0) {
			/* Update the services count. */
			num_usershares++;
			if (num_usershares >= max_user_shares) {
				DEBUG(0,("load_usershare_shares: max user shares reached "
					"on file %s in directory %s\n",
					n, usersharepath ));
				break;
			}
		} else if (r == -1) {
			num_bad_dir_entries++;
		}

		/* Allow 20% bad entries. */
		if (num_bad_dir_entries > allowed_bad_entries) {
			DEBUG(0,("load_usershare_shares: too many bad entries (%u) "
				"in directory %s\n",
				num_bad_dir_entries, usersharepath));
			break;
		}

		/* Allow 20% bad entries. */
		if (num_dir_entries > max_user_shares + allowed_bad_entries) {
			DEBUG(0,("load_usershare_shares: too many total entries (%u) "
			"in directory %s\n",
			num_dir_entries, usersharepath));
			break;
		}
	}

	sys_closedir(dp);

	/* Sweep through and delete any non-refreshed usershares that are
	   not currently in use. */
	for (iService = iNumServices - 1; iService >= 0; iService--) {
		if (VALID(iService) && (ServicePtrs[iService]->usershare == USERSHARE_PENDING_DELETE)) {
			if (conn_snum_used(iService)) {
				continue;
			}
			/* Remove from the share ACL db. */
			DEBUG(10,("load_usershare_shares: Removing deleted usershare %s\n",
				lp_servicename(iService) ));
			delete_share_security(lp_servicename(iService));
			free_service_byindex(iService);
		}
	}

	return lp_numservices();
}

/********************************************************
 Destroy global resources allocated in this file
********************************************************/

void gfree_loadparm(void)
{
	int i;

	free_file_list();

	/* Free resources allocated to services */

	for ( i = 0; i < iNumServices; i++ ) {
		if ( VALID(i) ) {
			free_service_byindex(i);
		}
	}

	SAFE_FREE( ServicePtrs );
	iNumServices = 0;

	/* Now release all resources allocated to global
	   parameters and the default service */

	free_global_parameters();
}


/***************************************************************************
 Allow client apps to specify that they are a client
***************************************************************************/
void lp_set_in_client(bool b)
{
    in_client = b;
}


/***************************************************************************
 Determine if we're running in a client app
***************************************************************************/
bool lp_is_in_client(void)
{
    return in_client;
}

/***************************************************************************
 Load the services array from the services file. Return True on success, 
 False on failure.
***************************************************************************/

static bool lp_load_ex(const char *pszFname,
		       bool global_only,
		       bool save_defaults,
		       bool add_ipc,
		       bool initialize_globals,
		       bool allow_include_registry,
		       bool allow_registry_shares)
{
	char *n2 = NULL;
	bool bRetval;

	bRetval = False;

	DEBUG(3, ("lp_load_ex: refreshing parameters\n"));

	bInGlobalSection = True;
	bGlobalOnly = global_only;
	bAllowIncludeRegistry = allow_include_registry;

	init_globals(initialize_globals);

	free_file_list();

	if (save_defaults) {
		init_locals();
		lp_save_defaults();
	}

	free_param_opts(&Globals.param_opt);

	lp_do_parameter(-1, "idmap config * : backend", Globals.szIdmapBackend);

	/* We get sections first, so have to start 'behind' to make up */
	iServiceIndex = -1;

	if (lp_config_backend_is_file()) {
		n2 = talloc_sub_basic(talloc_tos(), get_current_username(),
					current_user_info.domain,
					pszFname);
		if (!n2) {
			smb_panic("lp_load_ex: out of memory");
		}

		add_to_file_list(pszFname, n2);

		bRetval = pm_process(n2, do_section, do_parameter, NULL);
		TALLOC_FREE(n2);

		/* finish up the last section */
		DEBUG(4, ("pm_process() returned %s\n", BOOLSTR(bRetval)));
		if (bRetval) {
			if (iServiceIndex >= 0) {
				bRetval = service_ok(iServiceIndex);
			}
		}

		if (lp_config_backend_is_registry()) {
			/* config backend changed to registry in config file */
			/*
			 * We need to use this extra global variable here to
			 * survive restart: init_globals uses this as a default
			 * for ConfigBackend. Otherwise, init_globals would
			 *  send us into an endless loop here.
			 */
			config_backend = CONFIG_BACKEND_REGISTRY;
			/* start over */
			DEBUG(1, ("lp_load_ex: changing to config backend "
				  "registry\n"));
			init_globals(true);
			lp_kill_all_services();
			return lp_load_ex(pszFname, global_only, save_defaults,
					  add_ipc, initialize_globals,
					  allow_include_registry,
					  allow_registry_shares);
		}
	} else if (lp_config_backend_is_registry()) {
		bRetval = process_registry_globals();
	} else {
		DEBUG(0, ("Illegal config  backend given: %d\n",
			  lp_config_backend()));
		bRetval = false;
	}

	if (bRetval && lp_registry_shares()) {
		if (allow_registry_shares) {
			bRetval = process_registry_shares();
		} else {
			bRetval = reload_registry_shares();
		}
	}

	{
		char *serv = lp_auto_services();
		lp_add_auto_services(serv);
		TALLOC_FREE(serv);
	}

	if (add_ipc) {
		/* When 'restrict anonymous = 2' guest connections to ipc$
		   are denied */
		lp_add_ipc("IPC$", (lp_restrict_anonymous() < 2));
		if ( lp_enable_asu_support() ) {
			lp_add_ipc("ADMIN$", false);
		}
	}

	set_server_role();
	set_default_server_announce_type();
	set_allowed_client_auth();

	if (lp_security() == SEC_SHARE) {
		DEBUG(1, ("WARNING: The security=share option is deprecated\n"));
	} else if (lp_security() == SEC_SERVER) {
		DEBUG(1, ("WARNING: The security=server option is deprecated\n"));
	}

	if (lp_security() == SEC_ADS && strchr(lp_passwordserver(), ':')) {
		DEBUG(1, ("WARNING: The optional ':port' in password server = %s is deprecated\n",
			  lp_passwordserver()));
	}

	bLoaded = True;

	/* Now we check bWINSsupport and set szWINSserver to 127.0.0.1 */
	/* if bWINSsupport is true and we are in the client            */
	if (lp_is_in_client() && Globals.bWINSsupport) {
		lp_do_parameter(GLOBAL_SECTION_SNUM, "wins server", "127.0.0.1");
	}

	if (!lp_is_in_client()) {
		switch (lp_client_ipc_signing()) {
		case Required:
			lp_set_cmdline("client signing", "mandatory");
			break;
		case Auto:
			lp_set_cmdline("client signing", "auto");
			break;
		case False:
			lp_set_cmdline("client signing", "disabled");
			break;
		}
	}

	init_iconv();

	bAllowIncludeRegistry = true;

	return (bRetval);
}

bool lp_load(const char *pszFname,
	     bool global_only,
	     bool save_defaults,
	     bool add_ipc,
	     bool initialize_globals)
{
	return lp_load_ex(pszFname,
			  global_only,
			  save_defaults,
			  add_ipc,
			  initialize_globals,
			  true,   /* allow_include_registry */
			  false); /* allow_registry_shares*/
}

bool lp_load_initial_only(const char *pszFname)
{
	return lp_load_ex(pszFname,
			  true,   /* global only */
			  false,  /* save_defaults */
			  false,  /* add_ipc */
			  true,   /* initialize_globals */
			  false,  /* allow_include_registry */
			  false); /* allow_registry_shares*/
}

bool lp_load_with_registry_shares(const char *pszFname,
				  bool global_only,
				  bool save_defaults,
				  bool add_ipc,
				  bool initialize_globals)
{
	return lp_load_ex(pszFname,
			  global_only,
			  save_defaults,
			  add_ipc,
			  initialize_globals,
			  true,  /* allow_include_registry */
			  true); /* allow_registry_shares*/
}

/***************************************************************************
 Return the max number of services.
***************************************************************************/

int lp_numservices(void)
{
	return (iNumServices);
}

/***************************************************************************
Display the contents of the services array in human-readable form.
***************************************************************************/

void lp_dump(FILE *f, bool show_defaults, int maxtoprint)
{
	int iService;

	if (show_defaults)
		defaults_saved = False;

	dump_globals(f);

	dump_a_service(&sDefault, f);

	for (iService = 0; iService < maxtoprint; iService++) {
		fprintf(f,"\n");
		lp_dump_one(f, show_defaults, iService);
	}
}

/***************************************************************************
Display the contents of one service in human-readable form.
***************************************************************************/

void lp_dump_one(FILE * f, bool show_defaults, int snum)
{
	if (VALID(snum)) {
		if (ServicePtrs[snum]->szService[0] == '\0')
			return;
		dump_a_service(ServicePtrs[snum], f);
	}
}

/***************************************************************************
Return the number of the service with the given name, or -1 if it doesn't
exist. Note that this is a DIFFERENT ANIMAL from the internal function
getservicebyname()! This works ONLY if all services have been loaded, and
does not copy the found service.
***************************************************************************/

int lp_servicenumber(const char *pszServiceName)
{
	int iService;
        fstring serviceName;

        if (!pszServiceName) {
        	return GLOBAL_SECTION_SNUM;
	}

	for (iService = iNumServices - 1; iService >= 0; iService--) {
		if (VALID(iService) && ServicePtrs[iService]->szService) {
			/*
			 * The substitution here is used to support %U is
			 * service names
			 */
			fstrcpy(serviceName, ServicePtrs[iService]->szService);
			standard_sub_basic(get_current_username(),
					   current_user_info.domain,
					   serviceName,sizeof(serviceName));
			if (strequal(serviceName, pszServiceName)) {
				break;
			}
		}
	}

	if (iService >= 0 && ServicePtrs[iService]->usershare == USERSHARE_VALID) {
		struct timespec last_mod;

		if (!usershare_exists(iService, &last_mod)) {
			/* Remove the share security tdb entry for it. */
			delete_share_security(lp_servicename(iService));
			/* Remove it from the array. */
			free_service_byindex(iService);
			/* Doesn't exist anymore. */
			return GLOBAL_SECTION_SNUM;
		}

		/* Has it been modified ? If so delete and reload. */
		if (timespec_compare(&ServicePtrs[iService]->usershare_last_mod,
				     &last_mod) < 0) {
			/* Remove it from the array. */
			free_service_byindex(iService);
			/* and now reload it. */
			iService = load_usershare_service(pszServiceName);
		}
	}

	if (iService < 0) {
		DEBUG(7,("lp_servicenumber: couldn't find %s\n", pszServiceName));
		return GLOBAL_SECTION_SNUM;
	}

	return (iService);
}

bool share_defined(const char *service_name)
{
	return (lp_servicenumber(service_name) != -1);
}

struct share_params *get_share_params(TALLOC_CTX *mem_ctx,
				      const char *sharename)
{
	struct share_params *result;
	char *sname = NULL;
	int snum;

	snum = find_service(mem_ctx, sharename, &sname);
	if (snum < 0 || sname == NULL) {
		return NULL;
	}

	if (!(result = TALLOC_P(mem_ctx, struct share_params))) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	result->service = snum;
	return result;
}

struct share_iterator *share_list_all(TALLOC_CTX *mem_ctx)
{
	struct share_iterator *result;

	if (!(result = TALLOC_P(mem_ctx, struct share_iterator))) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	result->next_id = 0;
	return result;
}

struct share_params *next_share(struct share_iterator *list)
{
	struct share_params *result;

	while (!lp_snum_ok(list->next_id) &&
	       (list->next_id < lp_numservices())) {
		list->next_id += 1;
	}

	if (list->next_id >= lp_numservices()) {
		return NULL;
	}

	if (!(result = TALLOC_P(list, struct share_params))) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	result->service = list->next_id;
	list->next_id += 1;
	return result;
}

struct share_params *next_printer(struct share_iterator *list)
{
	struct share_params *result;

	while ((result = next_share(list)) != NULL) {
		if (lp_print_ok(result->service)) {
			break;
		}
	}
	return result;
}

/*
 * This is a hack for a transition period until we transformed all code from
 * service numbers to struct share_params.
 */

struct share_params *snum2params_static(int snum)
{
	static struct share_params result;
	result.service = snum;
	return &result;
}

/*******************************************************************
 A useful volume label function. 
********************************************************************/

const char *volume_label(int snum)
{
	char *ret;
	const char *label = lp_volume(snum);
	if (!*label) {
		label = lp_servicename(snum);
	}

	/* This returns a 33 byte guarenteed null terminated string. */
	ret = talloc_strndup(talloc_tos(), label, 32);
	if (!ret) {
		return "";
	}		
	return ret;
}

/*******************************************************************
 Set the server type we will announce as via nmbd.
********************************************************************/

static void set_default_server_announce_type(void)
{
	default_server_announce = 0;
	default_server_announce |= SV_TYPE_WORKSTATION;
	default_server_announce |= SV_TYPE_SERVER;
	default_server_announce |= SV_TYPE_SERVER_UNIX;

	/* note that the flag should be set only if we have a 
	   printer service but nmbd doesn't actually load the 
	   services so we can't tell   --jerry */

	default_server_announce |= SV_TYPE_PRINTQ_SERVER;

	switch (lp_announce_as()) {
		case ANNOUNCE_AS_NT_SERVER:
			default_server_announce |= SV_TYPE_SERVER_NT;
			/* fall through... */
		case ANNOUNCE_AS_NT_WORKSTATION:
			default_server_announce |= SV_TYPE_NT;
			break;
		case ANNOUNCE_AS_WIN95:
			default_server_announce |= SV_TYPE_WIN95_PLUS;
			break;
		case ANNOUNCE_AS_WFW:
			default_server_announce |= SV_TYPE_WFW;
			break;
		default:
			break;
	}

	switch (lp_server_role()) {
		case ROLE_DOMAIN_MEMBER:
			default_server_announce |= SV_TYPE_DOMAIN_MEMBER;
			break;
		case ROLE_DOMAIN_PDC:
			default_server_announce |= SV_TYPE_DOMAIN_CTRL;
			break;
		case ROLE_DOMAIN_BDC:
			default_server_announce |= SV_TYPE_DOMAIN_BAKCTRL;
			break;
		case ROLE_STANDALONE:
		default:
			break;
	}
	if (lp_time_server())
		default_server_announce |= SV_TYPE_TIME_SOURCE;

	if (lp_host_msdfs())
		default_server_announce |= SV_TYPE_DFS_SERVER;
}

/***********************************************************
 If we are PDC then prefer us as DMB
************************************************************/

bool lp_domain_master(void)
{
	if (Globals.iDomainMaster == Auto)
		return (lp_server_role() == ROLE_DOMAIN_PDC);

	return (bool)Globals.iDomainMaster;
}

/***********************************************************
 If we are PDC then prefer us as DMB
************************************************************/

bool lp_domain_master_true_or_auto(void)
{
	if (Globals.iDomainMaster) /* auto or yes */
		return true;

	return false;
}

/***********************************************************
 If we are DMB then prefer us as LMB
************************************************************/

bool lp_preferred_master(void)
{
	if (Globals.iPreferredMaster == Auto)
		return (lp_local_master() && lp_domain_master());

	return (bool)Globals.iPreferredMaster;
}

/*******************************************************************
 Remove a service.
********************************************************************/

void lp_remove_service(int snum)
{
	ServicePtrs[snum]->valid = False;
	invalid_services[num_invalid_services++] = snum;
}

/*******************************************************************
 Copy a service.
********************************************************************/

void lp_copy_service(int snum, const char *new_name)
{
	do_section(new_name, NULL);
	if (snum >= 0) {
		snum = lp_servicenumber(new_name);
		if (snum >= 0)
			lp_do_parameter(snum, "copy", lp_servicename(snum));
	}
}


/*******************************************************************
 Get the default server type we will announce as via nmbd.
********************************************************************/

int lp_default_server_announce(void)
{
	return default_server_announce;
}

/*******************************************************************
 Split the announce version into major and minor numbers.
********************************************************************/

int lp_major_announce_version(void)
{
	static bool got_major = False;
	static int major_version = DEFAULT_MAJOR_VERSION;
	char *vers;
	char *p;

	if (got_major)
		return major_version;

	got_major = True;
	if ((vers = lp_announce_version()) == NULL)
		return major_version;

	if ((p = strchr_m(vers, '.')) == 0)
		return major_version;

	*p = '\0';
	major_version = atoi(vers);
	return major_version;
}

int lp_minor_announce_version(void)
{
	static bool got_minor = False;
	static int minor_version = DEFAULT_MINOR_VERSION;
	char *vers;
	char *p;

	if (got_minor)
		return minor_version;

	got_minor = True;
	if ((vers = lp_announce_version()) == NULL)
		return minor_version;

	if ((p = strchr_m(vers, '.')) == 0)
		return minor_version;

	p++;
	minor_version = atoi(p);
	return minor_version;
}

/***********************************************************
 Set the global name resolution order (used in smbclient).
************************************************************/

void lp_set_name_resolve_order(const char *new_order)
{
	string_set(&Globals.szNameResolveOrder, new_order);
}

const char *lp_printername(int snum)
{
	const char *ret = _lp_printername(snum);
	if (ret == NULL || (ret != NULL && *ret == '\0'))
		ret = lp_const_servicename(snum);

	return ret;
}


/***********************************************************
 Allow daemons such as winbindd to fix their logfile name.
************************************************************/

void lp_set_logfile(const char *name)
{
	string_set(&Globals.szLogFile, name);
	debug_set_logfile(name);
}

/*******************************************************************
 Return the max print jobs per queue.
********************************************************************/

int lp_maxprintjobs(int snum)
{
	int maxjobs = LP_SNUM_OK(snum) ? ServicePtrs[snum]->iMaxPrintJobs : sDefault.iMaxPrintJobs;
	if (maxjobs <= 0 || maxjobs >= PRINT_MAX_JOBID)
		maxjobs = PRINT_MAX_JOBID - 1;

	return maxjobs;
}

const char *lp_printcapname(void)
{
	if ((Globals.szPrintcapname != NULL) &&
	    (Globals.szPrintcapname[0] != '\0'))
		return Globals.szPrintcapname;

	if (sDefault.iPrinting == PRINT_CUPS) {
#ifdef HAVE_CUPS
		return "cups";
#else
		return "lpstat";
#endif
	}

	if (sDefault.iPrinting == PRINT_BSD)
		return "/etc/printcap";

	return PRINTCAP_NAME;
}

static uint32 spoolss_state;

bool lp_disable_spoolss( void )
{
	if ( spoolss_state == SVCCTL_STATE_UNKNOWN )
		spoolss_state = _lp_disable_spoolss() ? SVCCTL_STOPPED : SVCCTL_RUNNING;

	return spoolss_state == SVCCTL_STOPPED ? True : False;
}

void lp_set_spoolss_state( uint32 state )
{
	SMB_ASSERT( (state == SVCCTL_STOPPED) || (state == SVCCTL_RUNNING) );

	spoolss_state = state;
}

uint32 lp_get_spoolss_state( void )
{
	return lp_disable_spoolss() ? SVCCTL_STOPPED : SVCCTL_RUNNING;
}

/*******************************************************************
 Ensure we don't use sendfile if server smb signing is active.
********************************************************************/

bool lp_use_sendfile(int snum, struct smb_signing_state *signing_state)
{
	bool sign_active = false;

	/* Using sendfile blows the brains out of any DOS or Win9x TCP stack... JRA. */
	if (get_Protocol() < PROTOCOL_NT1) {
		return false;
	}
	if (signing_state) {
		sign_active = smb_signing_is_active(signing_state);
	}
	return (_lp_use_sendfile(snum) &&
			(get_remote_arch() != RA_WIN95) &&
			!sign_active);
}

/*******************************************************************
 Turn off sendfile if we find the underlying OS doesn't support it.
********************************************************************/

void set_use_sendfile(int snum, bool val)
{
	if (LP_SNUM_OK(snum))
		ServicePtrs[snum]->bUseSendfile = val;
	else
		sDefault.bUseSendfile = val;
}

/*******************************************************************
 Turn off storing DOS attributes if this share doesn't support it.
********************************************************************/

void set_store_dos_attributes(int snum, bool val)
{
	if (!LP_SNUM_OK(snum))
		return;
	ServicePtrs[(snum)]->bStoreDosAttributes = val;
}

void lp_set_mangling_method(const char *new_method)
{
	string_set(&Globals.szManglingMethod, new_method);
}

/*******************************************************************
 Global state for POSIX pathname processing.
********************************************************************/

static bool posix_pathnames;

bool lp_posix_pathnames(void)
{
	return posix_pathnames;
}

/*******************************************************************
 Change everything needed to ensure POSIX pathname processing (currently
 not much).
********************************************************************/

void lp_set_posix_pathnames(void)
{
	posix_pathnames = True;
}

/*******************************************************************
 Global state for POSIX lock processing - CIFS unix extensions.
********************************************************************/

bool posix_default_lock_was_set;
static enum brl_flavour posix_cifsx_locktype; /* By default 0 == WINDOWS_LOCK */

enum brl_flavour lp_posix_cifsu_locktype(files_struct *fsp)
{
	if (posix_default_lock_was_set) {
		return posix_cifsx_locktype;
	} else {
		return fsp->posix_open ? POSIX_LOCK : WINDOWS_LOCK;
	}
}

/*******************************************************************
********************************************************************/

void lp_set_posix_default_cifsx_readwrite_locktype(enum brl_flavour val)
{
	posix_default_lock_was_set = True;
	posix_cifsx_locktype = val;
}

int lp_min_receive_file_size(void)
{
	if (Globals.iminreceivefile < 0) {
		return 0;
	}
	return MIN(Globals.iminreceivefile, BUFFER_SIZE);
}

/*******************************************************************
 If socket address is an empty character string, it is necessary to 
 define it as "0.0.0.0". 
********************************************************************/

const char *lp_socket_address(void)
{
	char *sock_addr = Globals.szSocketAddress;

	if (sock_addr[0] == '\0'){
		string_set(&Globals.szSocketAddress, "0.0.0.0");
	}
	return  Globals.szSocketAddress;
}

void lp_set_passdb_backend(const char *backend)
{
	string_set(&Globals.szPassdbBackend, backend);
}

/*******************************************************************
 Safe wide links checks.
 This helper function always verify the validity of wide links,
 even after a configuration file reload.
********************************************************************/

static bool lp_widelinks_internal(int snum)
{
	return (bool)(LP_SNUM_OK(snum)? ServicePtrs[(snum)]->bWidelinks :
			sDefault.bWidelinks);
}

void widelinks_warning(int snum)
{
	if (lp_allow_insecure_widelinks()) {
		return;
	}

	if (lp_unix_extensions() && lp_widelinks_internal(snum)) {
		DEBUG(0,("Share '%s' has wide links and unix extensions enabled. "
			"These parameters are incompatible. "
			"Wide links will be disabled for this share.\n",
			lp_servicename(snum) ));
	}
}

bool lp_widelinks(int snum)
{
	/* wide links is always incompatible with unix extensions */
	if (lp_unix_extensions()) {
		/*
		 * Unless we have "allow insecure widelinks"
		 * turned on.
		 */
		if (!lp_allow_insecure_widelinks()) {
			return false;
		}
	}

	return lp_widelinks_internal(snum);
}

bool lp_writeraw(void)
{
	if (lp_async_smb_echo_handler()) {
		return false;
	}
	return _lp_writeraw();
}

bool lp_readraw(void)
{
	if (lp_async_smb_echo_handler()) {
		return false;
	}
	return _lp_readraw();
}
