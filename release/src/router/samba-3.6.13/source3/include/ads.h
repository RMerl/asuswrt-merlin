#ifndef _INCLUDE_ADS_H_
#define _INCLUDE_ADS_H_
/*
  header for ads (active directory) library routines

  basically this is a wrapper around ldap
*/

#include "libads/ads_status.h"
#include "smb_ldap.h"

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
		int ldap_page_size;
	} config;

	/* info about the current LDAP connection */
#ifdef HAVE_LDAP
	struct {
		LDAP *ld;
		struct sockaddr_storage ss; /* the ip of the active connection, if any */
		time_t last_attempt; /* last attempt to reconnect, monotonic clock */
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

#ifdef HAVE_ADS
typedef LDAPMod **ADS_MODLIST;
#else
typedef void **ADS_MODLIST;
#endif

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

/* ldap bitwise searches */
#define ADS_LDAP_MATCHING_RULE_BIT_AND	"1.2.840.113556.1.4.803"
#define ADS_LDAP_MATCHING_RULE_BIT_OR	"1.2.840.113556.1.4.804"

#define ADS_PINGS          0x0000FFFF  /* Ping response */

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

#include "libads/ads_proto.h"

#ifdef HAVE_LDAP
#include "libads/ads_ldap_protos.h"
#endif

#include "libads/kerberos_proto.h"

#endif	/* _INCLUDE_ADS_H_ */
