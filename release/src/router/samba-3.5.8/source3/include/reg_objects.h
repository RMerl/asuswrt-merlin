/* 
   Samba's Internal Registry objects
   
   SMB parameters and setup
   Copyright (C) Gerald Carter                   2002-2006.
   
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

#ifndef _REG_OBJECTS_H /* _REG_OBJECTS_H */
#define _REG_OBJECTS_H

/* low level structure to contain registry values */

struct regval_blob {
	fstring		valuename;
	uint16		type;
	/* this should be encapsulated in an RPC_DATA_BLOB */
	uint32		size;	/* in bytes */
	uint8           *data_p;
};

/*
 * A REG_SZ string is not necessarily NULL terminated. When retrieving it from
 * the net, we guarantee this however. A server might want to push it without
 * the terminator though.
 */

struct registry_string {
	size_t len;
	char *str;
};

struct registry_value {
	enum winreg_Type type;
	union {
		uint32 dword;
		uint64 qword;
		struct registry_string sz;
		struct {
			uint32 num_strings;
			char **strings;
		} multi_sz;
		DATA_BLOB binary;
	} v;
};

/* container for registry values */

struct regval_ctr {
	uint32          num_values;
	struct regval_blob **values;
	int seqnum;
};

/* container for registry subkey names */

struct regsubkey_ctr;

/*
 *
 * Macros that used to reside in rpc_reg.h
 *
 */
 
#define HKEY_CLASSES_ROOT	0x80000000
#define HKEY_CURRENT_USER	0x80000001
#define HKEY_LOCAL_MACHINE 	0x80000002
#define HKEY_USERS         	0x80000003
#define HKEY_PERFORMANCE_DATA	0x80000004

#define KEY_HKLM		"HKLM"
#define KEY_HKU			"HKU"
#define KEY_HKCC		"HKCC"
#define KEY_HKCR		"HKCR"
#define KEY_HKPD		"HKPD"
#define KEY_HKPT		"HKPT"
#define KEY_HKPN		"HKPN"
#define KEY_HKCU		"HKCU"
#define KEY_HKDD		"HKDD"
#define KEY_SERVICES		"HKLM\\SYSTEM\\CurrentControlSet\\Services"
#define KEY_EVENTLOG 		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\Eventlog"
#define KEY_SHARES		"HKLM\\SYSTEM\\CurrentControlSet\\Services\\LanmanServer\\Shares"
#define KEY_NETLOGON_PARAMS	"HKLM\\SYSTEM\\CurrentControlSet\\Services\\Netlogon\\Parameters"
#define KEY_TCPIP_PARAMS	"HKLM\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#define KEY_PROD_OPTIONS	"HKLM\\SYSTEM\\CurrentControlSet\\Control\\ProductOptions"
#define KEY_PRINTING 		"HKLM\\SYSTEM\\CurrentControlSet\\Control\\Print"
#define KEY_PRINTING_2K		"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Print\\Printers"
#define KEY_PRINTING_PORTS	"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Ports"
#define KEY_CURRENT_VERSION	"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"
#define KEY_PERFLIB		"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Perflib"
#define KEY_PERFLIB_009		"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Perflib\\009"
#define KEY_GROUP_POLICY	"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Group Policy"
#define KEY_WINLOGON		"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon"
#define KEY_SMBCONF		"HKLM\\SOFTWARE\\Samba\\smbconf"
#define KEY_SAMBA_GROUP_POLICY	"HKLM\\SOFTWARE\\Samba\\Group Policy"
#define KEY_TREE_ROOT		""

#define KEY_GP_MACHINE_POLICY		"HKLM\\Software\\Policies"
#define KEY_GP_MACHINE_WIN_POLICY	"HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies"
#define KEY_GP_USER_POLICY		"HKCU\\Software\\Policies"
#define KEY_GP_USER_WIN_POLICY		"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies"
/*
 * Registry key types
 *	Most keys are going to be GENERIC -- may need a better name?
 *	HKPD and HKPT are used by reg_perfcount.c
 *		they are special keys that contain performance data
 */
#define REG_KEY_GENERIC		0
#define REG_KEY_HKPD		1
#define REG_KEY_HKPT		2

/* 
 * container for function pointers to enumeration routines
 * for virtual registry view 
 */ 
 
struct registry_ops {
	/* functions for enumerating subkeys and values */	
	int 	(*fetch_subkeys)( const char *key, struct regsubkey_ctr *subkeys);
	int 	(*fetch_values) ( const char *key, struct regval_ctr *val );
	bool 	(*store_subkeys)( const char *key, struct regsubkey_ctr *subkeys );
	WERROR	(*create_subkey)(const char *key, const char *subkey);
	WERROR	(*delete_subkey)(const char *key, const char *subkey);
	bool 	(*store_values)( const char *key, struct regval_ctr *val );
	bool	(*reg_access_check)( const char *keyname, uint32 requested,
				     uint32 *granted,
				     const NT_USER_TOKEN *token );
	WERROR (*get_secdesc)(TALLOC_CTX *mem_ctx, const char *key,
			      struct security_descriptor **psecdesc);
	WERROR (*set_secdesc)(const char *key,
			      struct security_descriptor *sec_desc);
	bool	(*subkeys_need_update)(struct regsubkey_ctr *subkeys);
	bool	(*values_need_update)(struct regval_ctr *values);
};

struct registry_hook {
	const char	*keyname;	/* full path to name of key */
	struct registry_ops	*ops;	/* registry function hooks */
};


/* structure to store the registry handles */

struct registry_key_handle {
	uint32		type;
	char		*name; 		/* full name of registry key */
	uint32 		access_granted;
	struct registry_ops	*ops;
};

struct registry_key {
	struct registry_key_handle *key;
	struct regsubkey_ctr *subkeys;
	struct regval_ctr *values;
	struct nt_user_token *token;
};

#endif /* _REG_OBJECTS_H */
