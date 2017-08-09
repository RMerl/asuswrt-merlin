/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1999
   Copyright (C) Luke Kenneth Casson Leighton 1996 - 1999
   
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

#include "includes.h"
#include "libcli/security/security.h"
#include "librpc/ndr/libndr.h"
#include "libcli/security/display_sec.h"

/****************************************************************************
convert a security permissions into a string
****************************************************************************/

char *get_sec_mask_str(TALLOC_CTX *ctx, uint32_t type)
{
	char *typestr = talloc_strdup(ctx, "");

	if (!typestr) {
		return NULL;
	}

	if (type & SEC_GENERIC_ALL) {
		typestr = talloc_asprintf_append(typestr,
				"Generic all access ");
		if (!typestr) {
			return NULL;
		}
	}
	if (type & SEC_GENERIC_EXECUTE) {
		typestr = talloc_asprintf_append(typestr,
				"Generic execute access");
		if (!typestr) {
			return NULL;
		}
	}
	if (type & SEC_GENERIC_WRITE) {
		typestr = talloc_asprintf_append(typestr,
				"Generic write access ");
		if (!typestr) {
			return NULL;
		}
	}
	if (type & SEC_GENERIC_READ) {
		typestr = talloc_asprintf_append(typestr,
				"Generic read access ");
		if (!typestr) {
			return NULL;
		}
	}
	if (type & SEC_FLAG_MAXIMUM_ALLOWED) {
		typestr = talloc_asprintf_append(typestr,
				"MAXIMUM_ALLOWED_ACCESS ");
		if (!typestr) {
			return NULL;
		}
	}
	if (type & SEC_FLAG_SYSTEM_SECURITY) {
		typestr = talloc_asprintf_append(typestr,
				"SYSTEM_SECURITY_ACCESS ");
		if (!typestr) {
			return NULL;
		}
	}
	if (type & SEC_STD_SYNCHRONIZE) {
		typestr = talloc_asprintf_append(typestr,
				"SYNCHRONIZE_ACCESS ");
		if (!typestr) {
			return NULL;
		}
	}
	if (type & SEC_STD_WRITE_OWNER) {
		typestr = talloc_asprintf_append(typestr,
				"WRITE_OWNER_ACCESS ");
		if (!typestr) {
			return NULL;
		}
	}
	if (type & SEC_STD_WRITE_DAC) {
		typestr = talloc_asprintf_append(typestr,
				"WRITE_DAC_ACCESS ");
		if (!typestr) {
			return NULL;
		}
	}
	if (type & SEC_STD_READ_CONTROL) {
		typestr = talloc_asprintf_append(typestr,
				"READ_CONTROL_ACCESS ");
		if (!typestr) {
			return NULL;
		}
	}
	if (type & SEC_STD_DELETE) {
		typestr = talloc_asprintf_append(typestr,
				"DELETE_ACCESS ");
		if (!typestr) {
			return NULL;
		}
	}

	printf("\t\tSpecific bits: 0x%lx\n", (unsigned long)type&SEC_MASK_SPECIFIC);

	return typestr;
}

/****************************************************************************
 display sec_access structure
 ****************************************************************************/
void display_sec_access(uint32_t *info)
{
	char *mask_str = get_sec_mask_str(NULL, *info);
	printf("\t\tPermissions: 0x%x: %s\n", *info, mask_str ? mask_str : "");
	talloc_free(mask_str);
}

/****************************************************************************
 display sec_ace flags
 ****************************************************************************/
void display_sec_ace_flags(uint8_t flags)
{
	if (flags & SEC_ACE_FLAG_OBJECT_INHERIT)
		printf("SEC_ACE_FLAG_OBJECT_INHERIT ");
	if (flags & SEC_ACE_FLAG_CONTAINER_INHERIT)
		printf(" SEC_ACE_FLAG_CONTAINER_INHERIT ");
	if (flags & SEC_ACE_FLAG_NO_PROPAGATE_INHERIT)
		printf("SEC_ACE_FLAG_NO_PROPAGATE_INHERIT ");
	if (flags & SEC_ACE_FLAG_INHERIT_ONLY)
		printf("SEC_ACE_FLAG_INHERIT_ONLY ");
	if (flags & SEC_ACE_FLAG_INHERITED_ACE)
		printf("SEC_ACE_FLAG_INHERITED_ACE ");
/*	if (flags & SEC_ACE_FLAG_VALID_INHERIT)
		printf("SEC_ACE_FLAG_VALID_INHERIT "); */
	if (flags & SEC_ACE_FLAG_SUCCESSFUL_ACCESS)
		printf("SEC_ACE_FLAG_SUCCESSFUL_ACCESS ");
	if (flags & SEC_ACE_FLAG_FAILED_ACCESS)
		printf("SEC_ACE_FLAG_FAILED_ACCESS ");

	printf("\n");
}

/****************************************************************************
 display sec_ace object
 ****************************************************************************/
static void disp_sec_ace_object(struct security_ace_object *object)
{
	char *str;
	if (object->flags & SEC_ACE_OBJECT_TYPE_PRESENT) {
		str = GUID_string(NULL, &object->type.type);
		if (str == NULL) return;
		printf("Object type: SEC_ACE_OBJECT_TYPE_PRESENT\n");
		printf("Object GUID: %s\n", str);
		talloc_free(str);
	}
	if (object->flags & SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT) {
		str = GUID_string(NULL, &object->inherited_type.inherited_type);
		if (str == NULL) return;
		printf("Object type: SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT\n");
		printf("Object GUID: %s\n", str);
		talloc_free(str);
	}
}

/****************************************************************************
 display sec_ace structure
 ****************************************************************************/
void display_sec_ace(struct security_ace *ace)
{
	char *sid_str;

	printf("\tACE\n\t\ttype: ");
	switch (ace->type) {
		case SEC_ACE_TYPE_ACCESS_ALLOWED:
			printf("ACCESS ALLOWED");
			break;
		case SEC_ACE_TYPE_ACCESS_DENIED:
			printf("ACCESS DENIED");
			break;
		case SEC_ACE_TYPE_SYSTEM_AUDIT:
			printf("SYSTEM AUDIT");
			break;
		case SEC_ACE_TYPE_SYSTEM_ALARM:
			printf("SYSTEM ALARM");
			break;
		case SEC_ACE_TYPE_ALLOWED_COMPOUND:
			printf("SEC_ACE_TYPE_ALLOWED_COMPOUND");
			break;
		case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT:
			printf("SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT");
			break;
		case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
			printf("SEC_ACE_TYPE_ACCESS_DENIED_OBJECT");
			break;
		case SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT:
			printf("SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT");
			break;
		case SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT:
			printf("SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT");
			break;
		default:
			printf("????");
			break;
	}

	printf(" (%d) flags: 0x%02x ", ace->type, ace->flags);
	display_sec_ace_flags(ace->flags);
	display_sec_access(&ace->access_mask);
	sid_str = dom_sid_string(NULL, &ace->trustee);
	printf("\t\tSID: %s\n\n", sid_str);
	talloc_free(sid_str);

	if (sec_ace_object(ace->type)) {
		disp_sec_ace_object(&ace->object.object);
	}

}

/****************************************************************************
 display sec_acl structure
 ****************************************************************************/
void display_sec_acl(struct security_acl *sec_acl)
{
	uint32_t i;

	printf("\tACL\tNum ACEs:\t%u\trevision:\t%x\n",
			 sec_acl->num_aces, sec_acl->revision); 
	printf("\t---\n");

	if (sec_acl->size != 0 && sec_acl->num_aces != 0) {
		for (i = 0; i < sec_acl->num_aces; i++) {
			display_sec_ace(&sec_acl->aces[i]);
		}
	}
}

void display_acl_type(uint16_t type)
{
	printf("type: 0x%04x: ", type);

	if (type & SEC_DESC_OWNER_DEFAULTED)	/* 0x0001 */
		printf("SEC_DESC_OWNER_DEFAULTED ");
	if (type & SEC_DESC_GROUP_DEFAULTED)	/* 0x0002 */
		printf("SEC_DESC_GROUP_DEFAULTED ");
	if (type & SEC_DESC_DACL_PRESENT) 	/* 0x0004 */
		printf("SEC_DESC_DACL_PRESENT ");
	if (type & SEC_DESC_DACL_DEFAULTED)	/* 0x0008 */
		printf("SEC_DESC_DACL_DEFAULTED ");
	if (type & SEC_DESC_SACL_PRESENT)	/* 0x0010 */
		printf("SEC_DESC_SACL_PRESENT ");
	if (type & SEC_DESC_SACL_DEFAULTED)	/* 0x0020 */
		printf("SEC_DESC_SACL_DEFAULTED ");
	if (type & SEC_DESC_DACL_TRUSTED)	/* 0x0040 */
		printf("SEC_DESC_DACL_TRUSTED ");
	if (type & SEC_DESC_SERVER_SECURITY)	/* 0x0080 */
		printf("SEC_DESC_SERVER_SECURITY ");
	if (type & SEC_DESC_DACL_AUTO_INHERIT_REQ) /* 0x0100 */
		printf("SEC_DESC_DACL_AUTO_INHERIT_REQ ");
	if (type & SEC_DESC_SACL_AUTO_INHERIT_REQ) /* 0x0200 */
		printf("SEC_DESC_SACL_AUTO_INHERIT_REQ ");
	if (type & SEC_DESC_DACL_AUTO_INHERITED) /* 0x0400 */
		printf("SEC_DESC_DACL_AUTO_INHERITED ");
	if (type & SEC_DESC_SACL_AUTO_INHERITED) /* 0x0800 */
		printf("SEC_DESC_SACL_AUTO_INHERITED ");
	if (type & SEC_DESC_DACL_PROTECTED)	/* 0x1000 */
		printf("SEC_DESC_DACL_PROTECTED ");
	if (type & SEC_DESC_SACL_PROTECTED)	/* 0x2000 */
		printf("SEC_DESC_SACL_PROTECTED ");
	if (type & SEC_DESC_RM_CONTROL_VALID)	/* 0x4000 */
		printf("SEC_DESC_RM_CONTROL_VALID ");
	if (type & SEC_DESC_SELF_RELATIVE)	/* 0x8000 */
		printf("SEC_DESC_SELF_RELATIVE ");
	
	printf("\n");
}

/****************************************************************************
 display sec_desc structure
 ****************************************************************************/
void display_sec_desc(struct security_descriptor *sec)
{
	char *sid_str;

	if (!sec) {
		printf("NULL\n");
		return;
	}

	printf("revision: %d\n", sec->revision);
	display_acl_type(sec->type);

	if (sec->sacl) {
		printf("SACL\n");
		display_sec_acl(sec->sacl);
	}

	if (sec->dacl) {
		printf("DACL\n");
		display_sec_acl(sec->dacl);
	}

	if (sec->owner_sid) {
		sid_str = dom_sid_string(NULL, sec->owner_sid);
		printf("\tOwner SID:\t%s\n", sid_str);
		talloc_free(sid_str);
	}

	if (sec->group_sid) {
		sid_str = dom_sid_string(NULL, sec->group_sid);
		printf("\tGroup SID:\t%s\n", sid_str);
		talloc_free(sid_str);
	}
}
