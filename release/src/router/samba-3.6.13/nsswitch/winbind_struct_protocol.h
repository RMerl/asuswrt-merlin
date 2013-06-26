/*
   Unix SMB/CIFS implementation.

   Winbind daemon for ntdom nss module

   Copyright (C) Tim Potter 2000
   Copyright (C) Gerald Carter 2006

   You are free to use this interface definition in any way you see
   fit, including without restriction, using this header in your own
   products. You do not need to give any attribution.
*/

#ifndef SAFE_FREE
#define SAFE_FREE(x) do { if(x) {free(x); x=NULL;} } while(0)
#endif

#ifndef FSTRING_LEN
#define FSTRING_LEN 256
typedef char fstring[FSTRING_LEN];
#endif

#ifndef _WINBINDD_NTDOM_H
#define _WINBINDD_NTDOM_H

#define WINBINDD_SOCKET_NAME "pipe"            /* Name of PF_UNIX socket */

/* Let the build environment override the public winbindd socket location. This
 * is needed for launchd support -- jpeach.
 */
#ifndef WINBINDD_SOCKET_DIR
#define WINBINDD_SOCKET_DIR  "/tmp/.winbindd"  /* Name of PF_UNIX dir */
#endif

/*
 * when compiled with socket_wrapper support
 * the location of the WINBINDD_SOCKET_DIR
 * can be overwritten via an environment variable
 */
#define WINBINDD_SOCKET_DIR_ENVVAR "WINBINDD_SOCKET_DIR"

#define WINBINDD_PRIV_SOCKET_SUBDIR "winbindd_privileged" /* name of subdirectory of lp_lockdir() to hold the 'privileged' pipe */
#define WINBINDD_DOMAIN_ENV  "WINBINDD_DOMAIN" /* Environment variables */
#define WINBINDD_DONT_ENV    "_NO_WINBINDD"
#define WINBINDD_LOCATOR_KDC_ADDRESS "WINBINDD_LOCATOR_KDC_ADDRESS"

/* Update this when you change the interface.
 * 21: added WINBINDD_GETPWSID
 *     added WINBINDD_GETSIDALIASES
 * 22: added WINBINDD_PING_DC
 * 23: added session_key to ccache_ntlm_auth response
 *     added WINBINDD_CCACHE_SAVE
 * 24: Fill in num_entries WINBINDD_LIST_USERS and WINBINDD_LIST_GROUPS
 * 25: removed WINBINDD_SET_HWM
 *     removed WINBINDD_SET_MAPPING
 *     removed WINBINDD_REMOVE_MAPPING
 * 26: added WINBINDD_DC_INFO
 * 27: added WINBINDD_LOOKUPSIDS
 */
#define WINBIND_INTERFACE_VERSION 27

/* Have to deal with time_t being 4 or 8 bytes due to structure alignment.
   On a 64bit Linux box, we have to support a constant structure size
   between /lib/libnss_winbind.so.2 and /lib64/libnss_winbind.so.2.
   The easiest way to do this is to always use 8byte values for time_t. */

#define SMB_TIME_T int64_t

/* Socket commands */

enum winbindd_cmd {

	WINBINDD_INTERFACE_VERSION,    /* Always a well known value */

	/* Get users and groups */

	WINBINDD_GETPWNAM,
	WINBINDD_GETPWUID,
	WINBINDD_GETPWSID,
	WINBINDD_GETGRNAM,
	WINBINDD_GETGRGID,
	WINBINDD_GETGROUPS,

	/* Enumerate users and groups */

	WINBINDD_SETPWENT,
	WINBINDD_ENDPWENT,
	WINBINDD_GETPWENT,
	WINBINDD_SETGRENT,
	WINBINDD_ENDGRENT,
	WINBINDD_GETGRENT,

	/* PAM authenticate and password change */

	WINBINDD_PAM_AUTH,
	WINBINDD_PAM_AUTH_CRAP,
	WINBINDD_PAM_CHAUTHTOK,
	WINBINDD_PAM_LOGOFF,
	WINBINDD_PAM_CHNG_PSWD_AUTH_CRAP,

	/* List various things */

	WINBINDD_LIST_USERS,         /* List w/o rid->id mapping */
	WINBINDD_LIST_GROUPS,        /* Ditto */
	WINBINDD_LIST_TRUSTDOM,

	/* SID conversion */

	WINBINDD_LOOKUPSID,
	WINBINDD_LOOKUPNAME,
	WINBINDD_LOOKUPRIDS,
	WINBINDD_LOOKUPSIDS,

	/* Lookup functions */

	WINBINDD_SID_TO_UID,
	WINBINDD_SID_TO_GID,
	WINBINDD_SIDS_TO_XIDS,
	WINBINDD_UID_TO_SID,
	WINBINDD_GID_TO_SID,

	WINBINDD_ALLOCATE_UID,
	WINBINDD_ALLOCATE_GID,

	/* Miscellaneous other stuff */

	WINBINDD_CHECK_MACHACC,     /* Check machine account pw works */
	WINBINDD_CHANGE_MACHACC,    /* Change machine account pw */
	WINBINDD_PING_DC,     	    /* Ping the DC through NETLOGON */
	WINBINDD_PING,              /* Just tell me winbind is running */
	WINBINDD_INFO,              /* Various bit of info.  Currently just tidbits */
	WINBINDD_DOMAIN_NAME,       /* The domain this winbind server is a member of (lp_workgroup()) */

	WINBINDD_DOMAIN_INFO,	/* Most of what we know from
				   struct winbindd_domain */
	WINBINDD_GETDCNAME,	/* Issue a GetDCName Request */
	WINBINDD_DSGETDCNAME,	/* Issue a DsGetDCName Request */
	WINBINDD_DC_INFO,	/* Which DC are we connected to? */

	WINBINDD_SHOW_SEQUENCE, /* display sequence numbers of domains */

	/* WINS commands */

	WINBINDD_WINS_BYIP,
	WINBINDD_WINS_BYNAME,

	/* this is like GETGRENT but gives an empty group list */
	WINBINDD_GETGRLST,

	WINBINDD_NETBIOS_NAME,       /* The netbios name of the server */

	/* find the location of our privileged pipe */
	WINBINDD_PRIV_PIPE_DIR,

	/* return a list of group sids for a user sid */
	WINBINDD_GETUSERSIDS,

	/* Various group queries */
	WINBINDD_GETUSERDOMGROUPS,

	/* lookup local groups */
	WINBINDD_GETSIDALIASES,

	/* Initialize connection in a child */
	WINBINDD_INIT_CONNECTION,

	/* Blocking calls that are not allowed on the main winbind pipe, only
	 * between parent and children */
	WINBINDD_DUAL_SID2UID,
	WINBINDD_DUAL_SID2GID,
	WINBINDD_DUAL_SIDS2XIDS,
	WINBINDD_DUAL_UID2SID,
	WINBINDD_DUAL_GID2SID,

	/* Wrapper around possibly blocking unix nss calls */
	WINBINDD_DUAL_USERINFO,
	WINBINDD_DUAL_GETSIDALIASES,

	WINBINDD_DUAL_NDRCMD,

	/* Complete the challenge phase of the NTLM authentication
	   protocol using cached password. */
	WINBINDD_CCACHE_NTLMAUTH,
	WINBINDD_CCACHE_SAVE,

	WINBINDD_NUM_CMDS
};

typedef struct winbindd_pw {
	fstring pw_name;
	fstring pw_passwd;
	uid_t pw_uid;
	gid_t pw_gid;
	fstring pw_gecos;
	fstring pw_dir;
	fstring pw_shell;
} WINBINDD_PW;


typedef struct winbindd_gr {
	fstring gr_name;
	fstring gr_passwd;
	gid_t gr_gid;
	uint32_t num_gr_mem;
	uint32_t gr_mem_ofs;   /* offset to group membership */
} WINBINDD_GR;

/* PAM specific request flags */
#define WBFLAG_PAM_INFO3_NDR		0x00000001
#define WBFLAG_PAM_INFO3_TEXT		0x00000002
#define WBFLAG_PAM_USER_SESSION_KEY	0x00000004
#define WBFLAG_PAM_LMKEY		0x00000008
#define WBFLAG_PAM_CONTACT_TRUSTDOM	0x00000010
#define WBFLAG_PAM_UNIX_NAME		0x00000080
#define WBFLAG_PAM_AFS_TOKEN		0x00000100
#define WBFLAG_PAM_NT_STATUS_SQUASH	0x00000200
#define WBFLAG_PAM_KRB5			0x00001000
#define WBFLAG_PAM_FALLBACK_AFTER_KRB5	0x00002000
#define WBFLAG_PAM_CACHED_LOGIN		0x00004000
#define WBFLAG_PAM_GET_PWD_POLICY	0x00008000

/* generic request flags */
#define WBFLAG_QUERY_ONLY		0x00000020	/* not used */
/* This is a flag that can only be sent from parent to child */
#define WBFLAG_IS_PRIVILEGED		0x00000400	/* not used */
/* Flag to say this is a winbindd internal send - don't recurse. */
#define WBFLAG_RECURSE			0x00000800
/* Flag to tell winbind the NTLMv2 blob is too big for the struct and is in the
 * extra_data field */
#define WBFLAG_BIG_NTLMV2_BLOB		0x00010000

#define WINBINDD_MAX_EXTRA_DATA (128*1024)

/* Winbind request structure */

/*******************************************************************************
 * This structure MUST be the same size in the 32bit and 64bit builds
 * for compatibility between /lib64/libnss_winbind.so and /lib/libnss_winbind.so
 *
 * DO NOT CHANGE THIS STRUCTURE WITHOUT TESTING THE 32BIT NSS LIB AGAINST
 * A 64BIT WINBINDD    --jerry
 ******************************************************************************/

struct winbindd_request {
	uint32_t length;
	enum winbindd_cmd cmd;   /* Winbindd command to execute */
	enum winbindd_cmd original_cmd;   /* Original Winbindd command
					     issued to parent process */
	pid_t pid;               /* pid of calling process */
	uint32_t wb_flags;       /* generic flags */
	uint32_t flags;          /* flags relevant *only* to a given request */
	fstring domain_name;	/* name of domain for which the request applies */

	union {
		fstring winsreq;     /* WINS request */
		fstring username;    /* getpwnam */
		fstring groupname;   /* getgrnam */
		uid_t uid;           /* getpwuid, uid_to_sid */
		gid_t gid;           /* getgrgid, gid_to_sid */
		uint32_t ndrcmd;
		struct {
			/* We deliberatedly don't split into domain/user to
                           avoid having the client know what the separator
                           character is. */
			fstring user;
			fstring pass;
			char require_membership_of_sid[1024];
			fstring krb5_cc_type;
			uid_t uid;
		} auth;              /* pam_winbind auth module */
                struct {
                        uint8_t chal[8];
			uint32_t logon_parameters;
                        fstring user;
                        fstring domain;
                        fstring lm_resp;
                        uint32_t lm_resp_len;
                        fstring nt_resp;
                        uint32_t nt_resp_len;
			fstring workstation;
		        fstring require_membership_of_sid;
                } auth_crap;
                struct {
                    fstring user;
                    fstring oldpass;
                    fstring newpass;
                } chauthtok;         /* pam_winbind passwd module */
		struct {
			fstring user;
			fstring domain;
			uint8_t new_nt_pswd[516];
			uint16_t new_nt_pswd_len;
			uint8_t old_nt_hash_enc[16];
			uint16_t old_nt_hash_enc_len;
			uint8_t new_lm_pswd[516];
			uint16_t new_lm_pswd_len;
			uint8_t old_lm_hash_enc[16];
			uint16_t old_lm_hash_enc_len;
		} chng_pswd_auth_crap;/* pam_winbind passwd module */
		struct {
			fstring user;
			fstring krb5ccname;
			uid_t uid;
		} logoff;              /* pam_winbind session module */
		fstring sid;         /* lookupsid, sid_to_[ug]id */
		struct {
			fstring dom_name;       /* lookupname */
			fstring name;
		} name;
		uint32_t num_entries;  /* getpwent, getgrent */
		struct {
			fstring username;
			fstring groupname;
		} acct_mgt;
		struct {
			bool is_primary;
			fstring dcname;
		} init_conn;
		struct {
			fstring sid;
			fstring name;
		} dual_sid2id;
		struct {
			fstring sid;
			uint32_t type;
			uint32_t id;
		} dual_idmapset;
		bool list_all_domains;

		struct {
			uid_t uid;
			fstring user;
			/* the effective uid of the client, must be the uid for 'user'.
			   This is checked by the main daemon, trusted by children. */
			/* if the blobs are length zero, then this doesn't
			   produce an actual challenge response. It merely
			   succeeds if there are cached credentials available
			   that could be used. */
			uint32_t initial_blob_len; /* blobs in extra_data */
			uint32_t challenge_blob_len;
		} ccache_ntlm_auth;
		struct {
			uid_t uid;
			fstring user;
			fstring pass;
		} ccache_save;
		struct {
			fstring domain_name;
			fstring domain_guid;
			fstring site_name;
			uint32_t flags;
		} dsgetdcname;

		/* padding -- needed to fix alignment between 32bit and 64bit libs.
		   The size is the sizeof the union without the padding aligned on
		   an 8 byte boundary.   --jerry */

		char padding[1800];
	} data;
	union {
		SMB_TIME_T padding;
		char *data;
	} extra_data;
	uint32_t extra_len;
	char null_term;
};

/* Response values */

enum winbindd_result {
	WINBINDD_ERROR,
	WINBINDD_PENDING,
	WINBINDD_OK
};

/* Winbind response structure */

/*******************************************************************************
 * This structure MUST be the same size in the 32bit and 64bit builds
 * for compatibility between /lib64/libnss_winbind.so and /lib/libnss_winbind.so
 *
 * DO NOT CHANGE THIS STRUCTURE WITHOUT TESTING THE 32BIT NSS LIB AGAINST
 * A 64BIT WINBINDD    --jerry
 ******************************************************************************/

struct winbindd_response {

	/* Header information */

	uint32_t length;                      /* Length of response */
	enum winbindd_result result;          /* Result code */

	/* Fixed length return data */

	union {
		int interface_version;  /* Try to ensure this is always in the same spot... */

		fstring winsresp;		/* WINS response */

		/* getpwnam, getpwuid */

		struct winbindd_pw pw;

		/* getgrnam, getgrgid */

		struct winbindd_gr gr;

		uint32_t num_entries; /* getpwent, getgrent */
		struct winbindd_sid {
			fstring sid;        /* lookupname, [ug]id_to_sid */
			int type;
		} sid;
		struct winbindd_name {
			fstring dom_name;       /* lookupsid */
			fstring name;
			int type;
		} name;
		uid_t uid;          /* sid_to_uid */
		gid_t gid;          /* sid_to_gid */
		struct winbindd_info {
			char winbind_separator;
			fstring samba_version;
		} info;
		fstring domain_name;
		fstring netbios_name;
		fstring dc_name;

		struct auth_reply {
			uint32_t nt_status;
			fstring nt_status_string;
			fstring error_string;
			int pam_error;
			char user_session_key[16];
			char first_8_lm_hash[8];
			fstring krb5ccname;
			uint32_t reject_reason;
			uint32_t padding;
			struct policy_settings {
				uint32_t min_length_password;
				uint32_t password_history;
				uint32_t password_properties;
				uint32_t padding;
				SMB_TIME_T expire;
				SMB_TIME_T min_passwordage;
			} policy;
			struct info3_text {
				SMB_TIME_T logon_time;
				SMB_TIME_T logoff_time;
				SMB_TIME_T kickoff_time;
				SMB_TIME_T pass_last_set_time;
				SMB_TIME_T pass_can_change_time;
				SMB_TIME_T pass_must_change_time;
				uint32_t logon_count;
				uint32_t bad_pw_count;
				uint32_t user_rid;
				uint32_t group_rid;
				uint32_t num_groups;
				uint32_t user_flgs;
				uint32_t acct_flags;
				uint32_t num_other_sids;
				fstring dom_sid;
				fstring user_name;
				fstring full_name;
				fstring logon_script;
				fstring profile_path;
				fstring home_dir;
				fstring dir_drive;
				fstring logon_srv;
				fstring logon_dom;
			} info3;
			fstring unix_username;
		} auth;
		struct {
			fstring name;
			fstring alt_name;
			fstring sid;
			bool native_mode;
			bool active_directory;
			bool primary;
		} domain_info;
		uint32_t sequence_number;
		struct {
			fstring acct_name;
			fstring full_name;
			fstring homedir;
			fstring shell;
			uint32_t primary_gid;
			uint32_t group_rid;
		} user_info;
		struct {
			uint8_t session_key[16];
			uint32_t auth_blob_len; /* blob in extra_data */
		} ccache_ntlm_auth;
		struct {
			fstring dc_unc;
			fstring dc_address;
			uint32_t dc_address_type;
			fstring domain_guid;
			fstring domain_name;
			fstring forest_name;
			uint32_t dc_flags;
			fstring dc_site_name;
			fstring client_site_name;
		} dsgetdcname;
	} data;

	/* Variable length return data */

	union {
		SMB_TIME_T padding;
		void *data;
	} extra_data;
};

#endif
