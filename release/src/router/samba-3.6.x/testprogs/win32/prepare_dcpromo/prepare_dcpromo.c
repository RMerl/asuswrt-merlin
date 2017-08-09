/*
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2010

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
   Published to the public domain
 */

/*
 * This tool can set the DOMAIN-SID and nextRid counter in
 * the local SAM on windows servers (tested with w2k8r2)
 *
 * dcpromo will use this values for the ad domain it creates.
 *
 * This might be useful for upgrades from a Samba3 domain.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* Convert a binary SID to a character string */
static DWORD SidToString(const SID *sid,
			 char **string)
{
	DWORD id_auth;
	int i, ofs, maxlen;
	char *result;

	if (!sid) {
		return ERROR_INVALID_SID;
	}

	maxlen = sid->SubAuthorityCount * 11 + 25;

	result = (char *)malloc(maxlen);
	if (result == NULL) {
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	/*
	 * BIG NOTE: this function only does SIDS where the identauth is not
	 * >= ^32 in a range of 2^48.
	 */

	id_auth = sid->IdentifierAuthority.Value[5] +
		(sid->IdentifierAuthority.Value[4] << 8) +
		(sid->IdentifierAuthority.Value[3] << 16) +
		(sid->IdentifierAuthority.Value[2] << 24);

	ofs = snprintf(result, maxlen, "S-%u-%lu",
		       (unsigned int)sid->Revision, (unsigned long)id_auth);

	for (i = 0; i < sid->SubAuthorityCount; i++) {
		ofs += snprintf(result + ofs, maxlen - ofs, "-%lu",
				(unsigned long)sid->SubAuthority[i]);
	}

	*string = result;
	return ERROR_SUCCESS;
}

static DWORD StringToSid(const char *str,
			 SID *sid)
{
	const char *p;
	char *q;
	DWORD x;

	if (!sid) {
		return ERROR_INVALID_PARAMETER;
	}

	/* Sanity check for either "S-" or "s-" */

	if (!str
	    || (str[0]!='S' && str[0]!='s')
	    || (str[1]!='-'))
	{
		return ERROR_INVALID_PARAMETER;
	}

	/* Get the SID revision number */

	p = str+2;
	x = (DWORD)strtol(p, &q, 10);
	if (x==0 || !q || *q!='-') {
		return ERROR_INVALID_SID;
	}
	sid->Revision = (BYTE)x;

	/* Next the Identifier Authority.  This is stored in big-endian
	   in a 6 byte array. */

	p = q+1;
	x = (DWORD)strtol(p, &q, 10);
	if (!q || *q!='-') {
		return ERROR_INVALID_SID;
	}
	sid->IdentifierAuthority.Value[5] = (x & 0x000000ff);
	sid->IdentifierAuthority.Value[4] = (x & 0x0000ff00) >> 8;
	sid->IdentifierAuthority.Value[3] = (x & 0x00ff0000) >> 16;
	sid->IdentifierAuthority.Value[2] = (x & 0xff000000) >> 24;
	sid->IdentifierAuthority.Value[1] = 0;
	sid->IdentifierAuthority.Value[0] = 0;

	/* now read the the subauthorities */

	p = q +1;
	sid->SubAuthorityCount = 0;
	while (sid->SubAuthorityCount < 6) {
		x=(DWORD)strtoul(p, &q, 10);
		if (p == q)
			break;
		if (q == NULL) {
			return ERROR_INVALID_SID;
		}
		sid->SubAuthority[sid->SubAuthorityCount++] = x;

		if ((*q!='-') || (*q=='\0'))
			break;
		p = q + 1;
	}

	/* IF we ended early, then the SID could not be converted */

	if (q && *q!='\0') {
		return ERROR_INVALID_SID;
	}

	return ERROR_SUCCESS;
}

#define MIN(a,b) ((a)<(b)?(a):(b))
static void print_asc(const unsigned char *buf,int len)
{
        int i;
        for (i=0;i<len;i++)
                printf("%c", isprint(buf[i])?buf[i]:'.');
}

static void dump_data(const unsigned char *buf1,int len)
{
        const unsigned char *buf = (const unsigned char *)buf1;
        int i=0;
        if (len<=0) return;

        printf("[%03X] ",i);
        for (i=0;i<len;) {
                printf("%02X ",(int)buf[i]);
                i++;
                if (i%8 == 0) printf(" ");
                if (i%16 == 0) {
                        print_asc(&buf[i-16],8); printf(" ");
                        print_asc(&buf[i-8],8); printf("\n");
                        if (i<len) printf("[%03X] ",i);
                }
        }
        if (i%16) {
                int n;
                n = 16 - (i%16);
                printf(" ");
                if (n>8) printf(" ");
                while (n--) printf("   ");
                n = MIN(8,i%16);
                print_asc(&buf[i-(i%16)],n); printf( " " );
                n = (i%16) - n;
                if (n>0) print_asc(&buf[i-n],n);
                printf("\n");
        }
}

static DWORD calc_tmp_HKLM_SECURITY_SD(SECURITY_DESCRIPTOR *old_sd,
				       SID *current_user_sid,
				       SECURITY_DESCRIPTOR **_old_parent_sd,
				       SECURITY_DESCRIPTOR **_old_child_sd,
				       SECURITY_DESCRIPTOR **_new_parent_sd,
				       SECURITY_DESCRIPTOR **_new_child_sd)
{
	LONG status;
	DWORD cbSecurityDescriptor = 0;
	SECURITY_DESCRIPTOR *old_parent_sd = NULL;
	SECURITY_DESCRIPTOR *old_child_sd = NULL;
	SECURITY_DESCRIPTOR *new_parent_sd = NULL;
	SECURITY_DESCRIPTOR *new_child_sd = NULL;
	BOOL ok;
	ACL *old_Dacl = NULL;
	ACL *new_Dacl = NULL;
	ACL_SIZE_INFORMATION dacl_info;
	DWORD i = 0;
	SECURITY_DESCRIPTOR *AbsoluteSD = NULL;
	DWORD dwAbsoluteSDSize = 0;
	DWORD dwRelativeSDSize = 0;
	DWORD dwDaclSize = 0;
	ACL *Sacl = NULL;
	DWORD dwSaclSize = 0;
	SID *Owner = NULL;
	DWORD dwOwnerSize = 0;
	SID *PrimaryGroup = NULL;
	DWORD dwPrimaryGroupSize = 0;
	ACCESS_ALLOWED_ACE *ace = NULL;

	ok = MakeAbsoluteSD(old_sd,
			    NULL,
			    &dwAbsoluteSDSize,
			    NULL,
			    &dwDaclSize,
			    NULL,
			    &dwSaclSize,
			    NULL,
			    &dwOwnerSize,
			    NULL,
			    &dwPrimaryGroupSize);
	if (!ok) {
		status = GetLastError();
	}
	if (status != ERROR_INSUFFICIENT_BUFFER) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	AbsoluteSD = (SECURITY_DESCRIPTOR *)malloc(dwAbsoluteSDSize+1024);
	if (AbsoluteSD == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	old_Dacl = (ACL *)malloc(dwDaclSize + 1024);
	if (old_Dacl == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	Sacl = (ACL *)malloc(dwSaclSize);
	if (Sacl == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	Owner = (SID *)malloc(dwOwnerSize);
	if (Owner == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	PrimaryGroup = (SID *)malloc(dwPrimaryGroupSize);
	if (PrimaryGroup == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	ok = MakeAbsoluteSD(old_sd,
			    AbsoluteSD,
			    &dwAbsoluteSDSize,
			    old_Dacl,
			    &dwDaclSize,
			    Sacl,
			    &dwSaclSize,
			    Owner,
			    &dwOwnerSize,
			    PrimaryGroup,
			    &dwPrimaryGroupSize);
	if (!ok) {
		status = GetLastError();
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	AbsoluteSD->Control |= SE_DACL_AUTO_INHERITED | SE_DACL_AUTO_INHERIT_REQ | SE_DACL_PROTECTED;
	dwRelativeSDSize = 0;
	ok = MakeSelfRelativeSD(AbsoluteSD,
				NULL,
				&dwRelativeSDSize);
	if (!ok) {
		status = GetLastError();
	}
	if (status != ERROR_INSUFFICIENT_BUFFER) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	old_parent_sd = (SECURITY_DESCRIPTOR *)malloc(dwRelativeSDSize);
	if (old_parent_sd == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	ok = MakeSelfRelativeSD(AbsoluteSD,
				old_parent_sd,
				&dwRelativeSDSize);
	if (!ok) {
		status = GetLastError();
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	ok = GetAclInformation(old_Dacl,
			       &dacl_info,
			       sizeof(dacl_info),
			       AclSizeInformation);
	if (!ok) {
		status = GetLastError();
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	new_Dacl = (ACL *)calloc(dacl_info.AclBytesInUse + 1024, 1);
	if (new_Dacl == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	InitializeAcl(new_Dacl, dacl_info.AclBytesInUse + 1024, ACL_REVISION);

	ok = AddAccessAllowedAce(new_Dacl, ACL_REVISION,
				 KEY_ALL_ACCESS, current_user_sid);
	if (!ok) {
		status = GetLastError();
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	ok = GetAce(new_Dacl, 0, (LPVOID *)&ace);
	if (!ok) {
		status = GetLastError();
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	ace->Header.AceFlags |= CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;

	for (i=0; i < dacl_info.AceCount; i++) {
		ok = GetAce(old_Dacl, i, (LPVOID *)&ace);
		if (!ok) {
			status = GetLastError();
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

		ok = AddAce(new_Dacl, ACL_REVISION, MAXDWORD,
			    ace, ace->Header.AceSize);
		if (!ok) {
			status = GetLastError();
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}
	}

	AbsoluteSD->Dacl = new_Dacl;
	dwRelativeSDSize = 0;
	ok = MakeSelfRelativeSD(AbsoluteSD,
				NULL,
				&dwRelativeSDSize);
	if (!ok) {
		status = GetLastError();
	}
	if (status != ERROR_INSUFFICIENT_BUFFER) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	new_parent_sd = (SECURITY_DESCRIPTOR *)malloc(dwRelativeSDSize);
	if (new_parent_sd == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	ok = MakeSelfRelativeSD(AbsoluteSD,
				new_parent_sd,
				&dwRelativeSDSize);
	if (!ok) {
		status = GetLastError();
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	for (i=0; i < dacl_info.AceCount; i++) {
		ok = GetAce(old_Dacl, i, (LPVOID *)&ace);
		if (!ok) {
			status = GetLastError();
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

		ace->Header.AceFlags |= INHERITED_ACE;
	}

	AbsoluteSD->Control &= ~SE_DACL_PROTECTED;
	AbsoluteSD->Dacl = old_Dacl;
	dwRelativeSDSize = 0;
	ok = MakeSelfRelativeSD(AbsoluteSD,
				NULL,
				&dwRelativeSDSize);
	if (!ok) {
		status = GetLastError();
	}
	if (status != ERROR_INSUFFICIENT_BUFFER) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	old_child_sd = (SECURITY_DESCRIPTOR *)malloc(dwRelativeSDSize);
	if (old_child_sd == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	ok = MakeSelfRelativeSD(AbsoluteSD,
				old_child_sd,
				&dwRelativeSDSize);
	if (!ok) {
		status = GetLastError();
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	for (i=0; i < dacl_info.AceCount + 1; i++) {
		ok = GetAce(new_Dacl, i, (LPVOID *)&ace);
		if (!ok) {
			status = GetLastError();
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

		ace->Header.AceFlags |= INHERITED_ACE;
	}

	AbsoluteSD->Dacl = new_Dacl;
	dwRelativeSDSize = 0;
	ok = MakeSelfRelativeSD(AbsoluteSD,
				NULL,
				&dwRelativeSDSize);
	if (!ok) {
		status = GetLastError();
	}
	if (status != ERROR_INSUFFICIENT_BUFFER) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	new_child_sd = (SECURITY_DESCRIPTOR *)malloc(dwRelativeSDSize);
	if (new_child_sd == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	ok = MakeSelfRelativeSD(AbsoluteSD,
				new_child_sd,
				&dwRelativeSDSize);
	if (!ok) {
		status = GetLastError();
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	*_old_parent_sd = old_parent_sd;
	*_old_child_sd = old_child_sd;
	*_new_parent_sd = new_parent_sd;
	*_new_child_sd = new_child_sd;
	return ERROR_SUCCESS;
}

static DWORD inherit_SD(HKEY parent_hk,
			char *current_key,
			BOOL reset,
			SECURITY_DESCRIPTOR *current_sd,
			SECURITY_DESCRIPTOR *child_sd)
{
	DWORD status;
	DWORD i = 0;
	HKEY current_hk;

	if (!reset) {
		status = RegOpenKeyEx(parent_hk,
				      current_key,
				      0, /* options */
				      WRITE_DAC, /* samDesired */
				      &current_hk);
		if (status != ERROR_SUCCESS) {
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

		status = RegSetKeySecurity(current_hk,
					   DACL_SECURITY_INFORMATION |
					   PROTECTED_DACL_SECURITY_INFORMATION |
					   UNPROTECTED_DACL_SECURITY_INFORMATION |
					   UNPROTECTED_SACL_SECURITY_INFORMATION,
					   current_sd /* pSecurityDescriptor */
					   );
		if (status != ERROR_SUCCESS) {
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

		RegCloseKey(current_hk);
	}

	status = RegOpenKeyEx(parent_hk,
			      current_key,
			      0, /* options */
			      KEY_ENUMERATE_SUB_KEYS, /* samDesired */
			      &current_hk);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	for (i=0; ; i++) {
		char subkey[10240];
		HKEY hk_child;

		memset(subkey, 0, sizeof(subkey));
		status = RegEnumKey(current_hk, i, subkey, sizeof(subkey));
		if (status == ERROR_NO_MORE_ITEMS) {
			break;
		}
		if (status != ERROR_SUCCESS) {
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

#if 0
		printf("subkey: %s\n", subkey);
#endif

		status = inherit_SD(current_hk, subkey, reset,
				    child_sd, child_sd);
		if (status != ERROR_SUCCESS) {
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}
	}

	RegCloseKey(current_hk);

	if (reset) {
		status = RegOpenKeyEx(parent_hk,
				      current_key,
				      0, /* options */
				      WRITE_DAC, /* samDesired */
				      &current_hk);
		if (status != ERROR_SUCCESS) {
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

		status = RegSetKeySecurity(current_hk,
					   DACL_SECURITY_INFORMATION |
					   PROTECTED_DACL_SECURITY_INFORMATION |
					   UNPROTECTED_DACL_SECURITY_INFORMATION |
					   UNPROTECTED_SACL_SECURITY_INFORMATION,
					   current_sd /* pSecurityDescriptor */
					   );
		if (status != ERROR_SUCCESS) {
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

		RegCloseKey(current_hk);
	}

	return ERROR_SUCCESS;
}

static DWORD replaceSIDBuffer(BYTE *buf, DWORD len,
			      SID *oldDomainSid,
			      SID *newDomainSid)
{
	DWORD ret = 0;
	BYTE *oldb = ((BYTE *)oldDomainSid)+2;
	BYTE *newb = ((BYTE *)newDomainSid)+2;
	int cmp;

#if 0
	printf("replaceSIDBuffer: %u\n", len);
	dump_data(buf, len);
#endif

	if (len < 24) {
		return 0;
	}

	if (buf[0] != SID_REVISION) {
		return 0;
	}

	switch (buf[1]) {
	case 4:
		ret = 24;
		break;
	case 5:
		if (len < 28) {
			return 0;
		}
		ret = 28;
		break;
	default:
		return 0;
	}

#if 0
	printf("oldb:\n");
	dump_data(oldb, 22);
#endif
	cmp = memcmp(&buf[2], oldb, 22);
	if (cmp != 0) {
		return 0;
	}

	memcpy(&buf[2], newb, 22);

	return ret;
}

static DWORD replaceSID(HKEY parent_hk,
			const char *parent_path,
			const char *current_key,
			SID *oldDomainSid,
			SID *newDomainSid)
{
	DWORD status;
	DWORD i = 0;
	HKEY current_hk;
	char current_path[10240];

	snprintf(current_path, sizeof(current_path), "%s\\%s",
		 parent_path, current_key);

	status = RegOpenKeyEx(parent_hk,
			      current_key,
			      0, /* options */
			      KEY_ALL_ACCESS, /* samDesired */
			      &current_hk);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	for (i=0; ; i++) {
		char subkey[10240];
		HKEY hk_child;

		memset(subkey, 0, sizeof(subkey));
		status = RegEnumKey(current_hk, i, subkey, sizeof(subkey));
		if (status == ERROR_NO_MORE_ITEMS) {
			break;
		}
		if (status != ERROR_SUCCESS) {
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

#if 0
		printf("subkey: %s\n", subkey);
#endif

		status = replaceSID(current_hk, current_path, subkey,
				    oldDomainSid, newDomainSid);
		if (status != ERROR_SUCCESS) {
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}
	}

	for (i=0; ; i++) {
		char valueName[10240];
		DWORD cbValueName;
		DWORD valueType = 0;
		BYTE *valueData = NULL;
		DWORD cbValueData = 0;
		DWORD ofs = 0;
		BOOL modified = FALSE;

		memset(valueName, 0, sizeof(valueName));
		cbValueName = sizeof(valueName)-1;
		status = RegEnumValue(current_hk, i,
				      valueName, &cbValueName,
				      NULL, NULL,
				      NULL, &cbValueData);
		if (status == ERROR_NO_MORE_ITEMS) {
			break;
		}
		if (status != ERROR_SUCCESS) {
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

		valueData = (BYTE *)malloc(cbValueData);
		if (valueData == NULL) {
			printf("LINE:%u: Error: no memory\n", __LINE__);
			return ERROR_NOT_ENOUGH_MEMORY;
		}

		cbValueName = sizeof(valueName)-1;
		status = RegEnumValue(current_hk, i,
				      valueName, &cbValueName,
				      NULL, &valueType,
				      valueData, &cbValueData);
		if (status != ERROR_SUCCESS) {
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

		if (valueType != REG_BINARY) {
			free(valueData);
			continue;
		}

		for (ofs=0; ofs < cbValueData;) {
			DWORD len;

			len = replaceSIDBuffer(valueData + ofs,
					       cbValueData - ofs,
					       oldDomainSid,
					       newDomainSid);
			if (len == 0) {
				ofs += 4;
				continue;
			}

#if 0
			printf("%s value[%u]:%s modified ofs:%u (0x%X) len:%u\n",
				current_path, i, valueName, ofs, ofs, len);
#endif

			ofs += len;
			modified = TRUE;
		}

		if (!modified) {
			free(valueData);
			continue;
		}

		printf("%s value[%u]:%s replacing data\n",
			current_path, i, valueName);
		status = RegSetValueEx(current_hk,
				       valueName,
				       0,
				       valueType,
				       valueData,
				       cbValueData);
		if (status != ERROR_SUCCESS) {
			printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
			return status;
		}

		free(valueData);
	}

	RegCloseKey(current_hk);

	return ERROR_SUCCESS;
}

int main(int argc, char *argv[])
{
	LONG status;
	HANDLE tokenHandle = NULL;
	TOKEN_USER *tokenUser = NULL;
	DWORD cbTokenUser = 0;
	HKEY hklm;
	HKEY hk_security;
	HKEY hk_account_domain;
	DWORD cbSecurityDescriptor = 0;
	SECURITY_DESCRIPTOR *security_old_sd = NULL;
	SECURITY_DESCRIPTOR *security_parent_old_sd = NULL;
	SECURITY_DESCRIPTOR *security_child_old_sd = NULL;
	SECURITY_DESCRIPTOR *security_parent_new_sd = NULL;
	SECURITY_DESCRIPTOR *security_child_new_sd = NULL;
	SID *currentUserSid = NULL;
	char *currentUserSidString = NULL;
	BOOL ok;
	DWORD cbTmp = 0;
	BYTE *AccountDomainF = NULL;
	DWORD cbAccountDomainF = 0;
	DWORD AccountDomainFType = 0;
	DWORD *nextRid = NULL;
	DWORD oldNextRid = 0;
	DWORD newNextRid = 0;
	BYTE *AccountDomainV = NULL;
	DWORD cbAccountDomainV = 0;
	SID *oldDomainSid = NULL;
	char *oldDomainSidString = NULL;
	SID *newDomainSid = NULL;
	const char *newDomainSidString = NULL;

	if (argc < 2 || argc > 3) {
		printf("Usage: %s <DOMAINSID> [<NEXTRID>]\n", argv[0]);
		return -1;
	}

	newDomainSidString = argv[1];

	newDomainSid = (SID *)malloc(24);
	if (newDomainSid == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return -1;
	}

	status = StringToSid(newDomainSidString, newDomainSid);
	if (status != ERROR_SUCCESS) {
		printf("Failed to parse DOMAINSID[%s]: Error: %d (0x%X)\n",
			newDomainSidString, status, status);
		return -1;
	}
	if (newDomainSid->SubAuthorityCount != 4) {
		printf("DOMAINSID[%s]: Invalid SubAuthorityCount[%u] should be 4\n",
			newDomainSidString, newDomainSid->SubAuthorityCount);
		return -1;
	}

	if (argc == 3) {
		char *q = NULL;
		newNextRid = (DWORD)strtoul(argv[2], &q, 10);
		if (newNextRid == 0 || newNextRid == 0xFFFFFFFF || !q || *q!='\0') {
			printf("Invalid newNextRid[%s]\n", argv[2]);
			return -1;
		}
		if (newNextRid < 1000) {
			printf("newNextRid[%u] < 1000\n", newNextRid);
			return -1;
		}
	}

	ok = OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &tokenHandle);
	if (!ok) {
		status = GetLastError();
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	ok = GetTokenInformation(tokenHandle, TokenUser,
				 NULL, 0, &cbTokenUser);
	if (!ok) {
		status = GetLastError();
	}
	if (status != ERROR_INSUFFICIENT_BUFFER) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	tokenUser = (TOKEN_USER *)malloc(cbTokenUser);
	if (tokenUser == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return -1;
	}

	ok = GetTokenInformation(tokenHandle, TokenUser,
				 tokenUser, cbTokenUser, &cbTokenUser);
	if (!ok) {
		status = GetLastError();
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	currentUserSid = tokenUser->User.Sid;

	status = SidToString(currentUserSid, &currentUserSidString);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	status = RegConnectRegistry(NULL, HKEY_LOCAL_MACHINE, &hklm);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	status = RegOpenKeyEx(hklm, "SECURITY",
			      0, /* options */
			      READ_CONTROL, /* samDesired */
			      &hk_security);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	status = RegGetKeySecurity(hk_security,
				   OWNER_SECURITY_INFORMATION |
				   GROUP_SECURITY_INFORMATION |
				   DACL_SECURITY_INFORMATION |
				   PROTECTED_DACL_SECURITY_INFORMATION |
				   UNPROTECTED_DACL_SECURITY_INFORMATION |
				   UNPROTECTED_SACL_SECURITY_INFORMATION,
				   NULL, /* pSecurityDescriptor */
				   &cbSecurityDescriptor /* lpcbSecurityDescriptor */
				   );
	if (status != ERROR_INSUFFICIENT_BUFFER) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	security_old_sd = (SECURITY_DESCRIPTOR *)malloc(cbSecurityDescriptor);
	if (security_old_sd == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return -1;
	}

	status = RegGetKeySecurity(hk_security,
				   OWNER_SECURITY_INFORMATION |
				   GROUP_SECURITY_INFORMATION |
				   DACL_SECURITY_INFORMATION |
				   PROTECTED_DACL_SECURITY_INFORMATION |
				   UNPROTECTED_DACL_SECURITY_INFORMATION |
				   UNPROTECTED_SACL_SECURITY_INFORMATION,
				   security_old_sd, /* pSecurityDescriptor */
				   &cbSecurityDescriptor /* lpcbSecurityDescriptor */
				   );
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	RegCloseKey(hk_security);

	printf("currentUserSid: %s\n", currentUserSidString);

	status = calc_tmp_HKLM_SECURITY_SD(security_old_sd,
					   currentUserSid,
					   &security_parent_old_sd,
					   &security_child_old_sd,
					   &security_parent_new_sd,
					   &security_child_new_sd);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	printf("Grant full access to HKLM\\SECURITY\n");
	status = inherit_SD(hklm, "SECURITY", FALSE,
			    security_parent_new_sd, security_child_new_sd);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	status = RegOpenKeyEx(hklm, "SECURITY\\SAM\\Domains\\Account",
			      0, /* options */
			      KEY_ALL_ACCESS, /* samDesired */
			      &hk_account_domain);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	status = RegQueryValueEx(hk_account_domain,
				 "F", NULL, NULL,
				 NULL,
				 &cbAccountDomainF);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	AccountDomainF = (BYTE *)malloc(cbAccountDomainF);
	if (AccountDomainF == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return -1;
	}

	status = RegQueryValueEx(hk_account_domain,
				 "F", NULL, NULL,
				 AccountDomainF,
				 &cbAccountDomainF);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	nextRid = (DWORD *)((BYTE *)AccountDomainF + 0x48);
	oldNextRid = *nextRid;
	if (newNextRid == 0) {
		newNextRid = oldNextRid;
	}
	printf("AccountDomainF: %u bytes\n", cbAccountDomainF);

	status = RegQueryValueEx(hk_account_domain,
				 "V", NULL, NULL,
				 NULL,
				 &cbAccountDomainV);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	AccountDomainV = (BYTE *)malloc(cbAccountDomainV);
	if (AccountDomainV == NULL) {
		printf("LINE:%u: Error: no memory\n", __LINE__);
		return -1;
	}

	status = RegQueryValueEx(hk_account_domain,
				 "V", NULL, NULL,
				 AccountDomainV,
				 &cbAccountDomainV);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	printf("AccountDomainV: %u bytes\n", cbAccountDomainV);
	oldDomainSid = (SID *)((BYTE *)AccountDomainV + (cbAccountDomainV - 24));

	status = SidToString(oldDomainSid, &oldDomainSidString);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}

	printf("Old Domain:%s, NextRid: %u (0x%08X)\n",
		oldDomainSidString, oldNextRid, oldNextRid);
	printf("New Domain:%s, NextRid: %u (0x%08X)\n",
		newDomainSidString, newNextRid, newNextRid);

	status = replaceSID(hklm, "HKLM", "SECURITY\\SAM\\Domains",
			    oldDomainSid, newDomainSid);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		goto failed;
	}

	status = RegQueryValueEx(hk_account_domain,
				 "F", NULL, &AccountDomainFType,
				 AccountDomainF,
				 &cbAccountDomainF);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}
	nextRid = (DWORD *)((BYTE *)AccountDomainF + 0x48);
	*nextRid = newNextRid;

	printf("AccountDomainF replacing data (nextRid)\n");
	status = RegSetValueEx(hk_account_domain,
			       "F",
			       0,
			       AccountDomainFType,
			       AccountDomainF,
			       cbAccountDomainF);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return status;
	}

	printf("Withdraw full access to HKLM\\SECURITY\n");
	status = inherit_SD(hklm, "SECURITY", TRUE,
			    security_parent_old_sd, security_child_old_sd);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}
	printf("DONE!\n");
	return 0;
failed:
	printf("Withdraw full access to HKLM\\SECURITY\n");
	status = inherit_SD(hklm, "SECURITY", TRUE,
			    security_parent_old_sd, security_child_old_sd);
	if (status != ERROR_SUCCESS) {
		printf("LINE:%u: Error: %d (0x%X)\n", __LINE__, status, status);
		return -1;
	}
	printf("FAILED!\n");
	return 0;
}
