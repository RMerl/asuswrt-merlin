/*
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *
 *  Copyright (C) Gerald Carter              2002-2005
 *  Copyright (C) Volker Lendecke            2006
 *  Copyright (C) Michael Adam               2006-2010
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _REGISTRY_H
#define _REGISTRY_H

#include "../librpc/gen_ndr/winreg.h"

struct registry_value {
	enum winreg_Type type;
	DATA_BLOB data;
};

/* forward declarations. definitions in reg_objects.c */
struct regval_ctr;
struct regsubkey_ctr;

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
				     const struct security_token *token );
	WERROR (*get_secdesc)(TALLOC_CTX *mem_ctx, const char *key,
			      struct security_descriptor **psecdesc);
	WERROR (*set_secdesc)(const char *key,
			      struct security_descriptor *sec_desc);
	bool	(*subkeys_need_update)(struct regsubkey_ctr *subkeys);
	bool	(*values_need_update)(struct regval_ctr *values);
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
	struct security_token *token;
};


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

#endif /* _REGISTRY_H */
