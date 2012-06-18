#ifndef _INCLUDE_ADS_H_
#define _INCLUDE_ADS_H_
/*
  header for ads (active directory) library routines

  basically this is a wrapper around ldap
*/

#include "../libds/common/flags.h"

/*
 * This should be under the HAVE_KRB5 flag but since they're used
 * in lp_kerberos_method(), they ned to be always available
 */
#define KERBEROS_VERIFY_SECRETS 0
#define KERBEROS_VERIFY_SYSTEM_KEYTAB 1
#define KERBEROS_VERIFY_DEDICATED_KEYTAB 2
#define KERBEROS_VERIFY_SECRETS_AND_KEYTAB 3

/*
 * If you add any entries to the above, please modify the below expressions
 * so they remain accurate.
 */
#define USE_KERBEROS_KEYTAB (KERBEROS_VERIFY_SECRETS != lp_kerberos_method())
#define USE_SYSTEM_KEYTAB \
    ((KERBEROS_VERIFY_SECRETS_AND_KEYTAB == lp_kerberos_method()) || \
     (KERBEROS_VERIFY_SYSTEM_KEYTAB == lp_kerberos_method()))

#define TOK_ID_KRB_AP_REQ	((const uint8_t *)"\x01\x00")
#define TOK_ID_KRB_AP_REP	((const uint8_t *)"\x02\x00")
#define TOK_ID_KRB_ERROR	((const uint8_t *)"\x03\x00")
#define TOK_ID_GSS_GETMIC	((const uint8_t *)"\x01\x01")
#define TOK_ID_GSS_WRAP		((const uint8_t *)"\x02\x01")

enum wb_posix_mapping {
	WB_POSIX_MAP_UNKNOWN    = -1,
	WB_POSIX_MAP_TEMPLATE 	= 0, 
	WB_POSIX_MAP_SFU 	= 1, 
	WB_POSIX_MAP_SFU20 	= 2, 
	WB_POSIX_MAP_RFC2307 	= 3,
	WB_POSIX_MAP_UNIXINFO	= 4
};

/* there are 5 possible types of errors the ads subsystem can produce */
enum ads_error_type {ENUM_ADS_ERROR_KRB5, ENUM_ADS_ERROR_GSS, 
		     ENUM_ADS_ERROR_LDAP, ENUM_ADS_ERROR_SYSTEM, ENUM_ADS_ERROR_NT};

typedef struct {
	enum ads_error_type error_type;
	union err_state{		
		int rc;
		NTSTATUS nt_status;
	} err;
	/* For error_type = ENUM_ADS_ERROR_GSS minor_status describe GSS API error */
	/* Where rc represents major_status of GSS API error */
	int minor_status;
} ADS_STATUS;

struct ads_struct;

struct ads_saslwrap_ops {
	const char *name;
	ADS_STATUS (*wrap)(struct ads_struct *, uint8 *buf, uint32 len);
	ADS_STATUS (*unwrap)(struct ads_struct *);
	void (*disconnect)(struct ads_struct *);
};

enum ads_saslwrap_type {
	ADS_SASLWRAP_TYPE_PLAIN = 1,
	ADS_SASLWRAP_TYPE_SIGN = 2,
	ADS_SASLWRAP_TYPE_SEAL = 4
};

typedef struct ads_struct {
	int is_mine;	/* do I own this structure's memory? */
	
	/* info needed to find the server */
	struct {
		char *realm;
		char *workgroup;
		char *ldap_server;
		int foreign; /* set to 1 if connecting to a foreign
			      * realm */
		bool gc;     /* Is this a global catalog server? */
	} server;

	/* info needed to authenticate */
	struct {
		char *realm;
		char *password;
		char *user_name;
		char *kdc_server;
		unsigned flags;
		int time_offset;
		time_t tgt_expire;
		time_t tgs_expire;
		time_t renewable;
	} auth;

	/* info derived from the servers config */
	struct {
		uint32 flags; /* cldap flags identifying the services. */
		char *realm;
		char *bind_path;
		char *ldap_server_name;
		char *server_site_name;
		char *client_site_name;
		time_t current_time;
		char *schema_path;
		char *config_path;
	} config;

	/* info about the current LDAP connection */
#ifdef HAVE_LDAP
	struct {
		LDAP *ld;
		struct sockaddr_storage ss; /* the ip of the active connection, if any */
		time_t last_attempt; /* last attempt to reconnect */
		int port;

		enum ads_saslwrap_type wrap_type;

#ifdef HAVE_LDAP_SASL_WRAPPING
		Sockbuf_IO_Desc *sbiod; /* lowlevel state for LDAP wrapping */
#endif /* HAVE_LDAP_SASL_WRAPPING */
		TALLOC_CTX *mem_ctx;
		const struct ads_saslwrap_ops *wrap_ops;
		void *wrap_private_data;
		struct {
			uint32 ofs;
			uint32 needed;
			uint32 left;
#define        ADS_SASL_WRAPPING_IN_MAX_WRAPPED        0x0FFFFFFF
			uint32 max_wrapped;
			uint32 min_wrapped;
			uint32 size;
			uint8 *buf;
		} in;
		struct {
			uint32 ofs;
			uint32 left;
#define        ADS_SASL_WRAPPING_OUT_MAX_WRAPPED       0x00A00000
			uint32 max_unwrapped;
			uint32 sig_size;
			uint32 size;
			uint8 *buf;
		} out;
	} ldap;
#endif /* HAVE_LDAP */
} ADS_STRUCT;

/* used to remember the names of the posix attributes in AD */
/* see the rfc2307 & sfu nss backends */

struct posix_schema {
	char *posix_homedir_attr;
	char *posix_shell_attr;
	char *posix_uidnumber_attr;
	char *posix_gidnumber_attr;
	char *posix_gecos_attr;
	char *posix_uid_attr;
};



#ifdef HAVE_ADS
typedef LDAPMod **ADS_MODLIST;
#else
typedef void **ADS_MODLIST;
#endif

/* macros to simplify error returning */
#define ADS_ERROR(rc) ADS_ERROR_LDAP(rc)
#define ADS_ERROR_LDAP(rc) ads_build_error(ENUM_ADS_ERROR_LDAP, rc, 0)
#define ADS_ERROR_SYSTEM(rc) ads_build_error(ENUM_ADS_ERROR_SYSTEM, rc?rc:EINVAL, 0)
#define ADS_ERROR_KRB5(rc) ads_build_error(ENUM_ADS_ERROR_KRB5, rc, 0)
#define ADS_ERROR_GSS(rc, minor) ads_build_error(ENUM_ADS_ERROR_GSS, rc, minor)
#define ADS_ERROR_NT(rc) ads_build_nt_error(ENUM_ADS_ERROR_NT,rc)

#define ADS_ERR_OK(status) ((status.error_type == ENUM_ADS_ERROR_NT) ? NT_STATUS_IS_OK(status.err.nt_status):(status.err.rc == 0))
#define ADS_SUCCESS ADS_ERROR(0)

#define ADS_ERROR_HAVE_NO_MEMORY(x) do { \
        if (!(x)) {\
                return ADS_ERROR(LDAP_NO_MEMORY);\
        }\
} while (0)


/* time between reconnect attempts */
#define ADS_RECONNECT_TIME 5

/* ldap control oids */
#define ADS_PAGE_CTL_OID 	"1.2.840.113556.1.4.319"
#define ADS_NO_REFERRALS_OID 	"1.2.840.113556.1.4.1339"
#define ADS_SERVER_SORT_OID 	"1.2.840.113556.1.4.473"
#define ADS_PERMIT_MODIFY_OID 	"1.2.840.113556.1.4.1413"
#define ADS_ASQ_OID		"1.2.840.113556.1.4.1504"
#define ADS_EXTENDED_DN_OID	"1.2.840.113556.1.4.529"
#define ADS_SD_FLAGS_OID	"1.2.840.113556.1.4.801"

/* ldap attribute oids (Services for Unix 3.0, 3.5) */
#define ADS_ATTR_SFU_UIDNUMBER_OID 	"1.2.840.113556.1.6.18.1.310"
#define ADS_ATTR_SFU_GIDNUMBER_OID 	"1.2.840.113556.1.6.18.1.311"
#define ADS_ATTR_SFU_HOMEDIR_OID 	"1.2.840.113556.1.6.18.1.344"
#define ADS_ATTR_SFU_SHELL_OID 		"1.2.840.113556.1.6.18.1.312"
#define ADS_ATTR_SFU_GECOS_OID 		"1.2.840.113556.1.6.18.1.337"
#define ADS_ATTR_SFU_UID_OID            "1.2.840.113556.1.6.18.1.309"

/* ldap attribute oids (Services for Unix 2.0) */
#define ADS_ATTR_SFU20_UIDNUMBER_OID	"1.2.840.113556.1.4.7000.187.70"
#define ADS_ATTR_SFU20_GIDNUMBER_OID	"1.2.840.113556.1.4.7000.187.71"
#define ADS_ATTR_SFU20_HOMEDIR_OID	"1.2.840.113556.1.4.7000.187.106"
#define ADS_ATTR_SFU20_SHELL_OID	"1.2.840.113556.1.4.7000.187.72"
#define ADS_ATTR_SFU20_GECOS_OID 	"1.2.840.113556.1.4.7000.187.97"
#define ADS_ATTR_SFU20_UID_OID          "1.2.840.113556.1.4.7000.187.102"


/* ldap attribute oids (RFC2307) */
#define ADS_ATTR_RFC2307_UIDNUMBER_OID	"1.3.6.1.1.1.1.0"
#define ADS_ATTR_RFC2307_GIDNUMBER_OID	"1.3.6.1.1.1.1.1"
#define ADS_ATTR_RFC2307_HOMEDIR_OID	"1.3.6.1.1.1.1.3"
#define ADS_ATTR_RFC2307_SHELL_OID	"1.3.6.1.1.1.1.4"
#define ADS_ATTR_RFC2307_GECOS_OID	"1.3.6.1.1.1.1.2"
#define ADS_ATTR_RFC2307_UID_OID        "0.9.2342.19200300.100.1.1"

/* ldap bitwise searches */
#define ADS_LDAP_MATCHING_RULE_BIT_AND	"1.2.840.113556.1.4.803"
#define ADS_LDAP_MATCHING_RULE_BIT_OR	"1.2.840.113556.1.4.804"

#define ADS_PINGS          0x0000FFFF  /* Ping response */
#define ADS_DNS_CONTROLLER 0x20000000  /* DomainControllerName is a DNS name*/
#define ADS_DNS_DOMAIN     0x40000000  /* DomainName is a DNS name */
#define ADS_DNS_FOREST     0x80000000  /* DnsForestName is a DNS name */

/* ads auth control flags */
#define ADS_AUTH_DISABLE_KERBEROS 0x0001
#define ADS_AUTH_NO_BIND          0x0002
#define ADS_AUTH_ANON_BIND        0x0004
#define ADS_AUTH_SIMPLE_BIND      0x0008
#define ADS_AUTH_ALLOW_NTLMSSP    0x0010
#define ADS_AUTH_SASL_SIGN        0x0020
#define ADS_AUTH_SASL_SEAL        0x0040
#define ADS_AUTH_SASL_FORCE       0x0080
#define ADS_AUTH_USER_CREDS       0x0100

/* Kerberos environment variable names */
#define KRB5_ENV_CCNAME "KRB5CCNAME"

#define WELL_KNOWN_GUID_COMPUTERS	"AA312825768811D1ADED00C04FD8D5CD" 
#define WELL_KNOWN_GUID_USERS		"A9D1CA15768811D1ADED00C04FD8D5CD"

enum ads_extended_dn_flags {
	ADS_EXTENDED_DN_HEX_STRING	= 0,
	ADS_EXTENDED_DN_STRING		= 1 /* not supported on win2k */
};

/* this is probably not very well suited to pass other controls generically but
 * is good enough for the extended dn control where it is only used for atm */

typedef struct {
	const char *control;
	int val;
	int critical;
} ads_control;

#define ADS_IGNORE_PRINCIPAL "not_defined_in_RFC4178@please_ignore"

#endif	/* _INCLUDE_ADS_H_ */
