#
# LDAP Schema file for SAMBA 3.0 attribute storage
# For Netscape Directory Server 4.1x
# Prepared by Osman Demirhan

attribute	sambaLMPassword			1.3.6.1.4.1.7165.2.1.24		cis single
attribute	sambaNTPassword			1.3.6.1.4.1.7165.2.1.25		cis single
attribute	sambaAcctFlags			1.3.6.1.4.1.7165.2.1.26		cis single
attribute	sambaPwdLastSet			1.3.6.1.4.1.7165.2.1.27		int single
attribute	sambaPwdCanChange		1.3.6.1.4.1.7165.2.1.28		int single
attribute	sambaPwdMustChange		1.3.6.1.4.1.7165.2.1.29		int single
attribute	sambaLogonTime			1.3.6.1.4.1.7165.2.1.30		int single
attribute	sambaLogoffTime			1.3.6.1.4.1.7165.2.1.31		int single
attribute	sambaKickoffTime		1.3.6.1.4.1.7165.2.1.32		int single
attribute	sambaHomeDrive			1.3.6.1.4.1.7165.2.1.33		cis single
attribute	sambaLogonScript		1.3.6.1.4.1.7165.2.1.34		cis single
attribute	sambaProfilePath		1.3.6.1.4.1.7165.2.1.35		cis single
attribute	sambaUserWorkstations	1.3.6.1.4.1.7165.2.1.36		cis single
attribute	sambaHomePath			1.3.6.1.4.1.7165.2.1.37		cis single
attribute	sambaDomainName			1.3.6.1.4.1.7165.2.1.38		cis single
attribute	sambaSID				1.3.6.1.4.1.7165.2.1.20		cis single
attribute	sambaPrimaryGroupSID	1.3.6.1.4.1.7165.2.1.23		cis single
attribute	sambaGroupType			1.3.6.1.4.1.7165.2.1.19		int single
attribute	sambaNextUserRid		1.3.6.1.4.1.7165.2.1.21		int single
attribute	sambaNextGroupRid		1.3.6.1.4.1.7165.2.1.22		int single
attribute	sambaNextRid			1.3.6.1.4.1.7165.2.1.39		int single
attribute	sambaAlgorithmicRidBase	1.3.6.1.4.1.7165.2.1.40		int single

objectclass sambaSamAccount
		oid
				1.3.6.1.4.1.7165.2.2.6
		superior
				top
		requires
				objectClass,
				uid,
				sambaSID
		allows
				cn,
				sambaLMPassword,
				sambaNTPassword,
				sambaPwdLastSet,
				sambaLogonTime,
				sambaLogoffTime,
				sambaKickoffTime,
				sambaPwdCanChange,
				sambaPwdMustChange,
				sambaAcctFlags,
				displayName,
				sambaHomePath,
				sambaHomeDrive,
				sambaLogonScript,
				sambaProfilePath,
				description,
				sambaUserWorkstations,
				sambaPrimaryGroupSID,
				sambaDomainName

objectclass sambaGroupMapping
		oid
				1.3.6.1.4.1.7165.2.2.4
		superior
				top
		requires
				gidNumber,
				sambaSID,
				sambaGroupType
		allows
				displayName,
				description

objectclass sambaDomain
		oid
				1.3.6.1.4.1.7165.2.2.5
		superior
				top
		requires
				sambaDomainName,
				sambaSID
		allows
				sambaNextRid,
				sambaNextGroupRid,
				sambaNextUserRid,
				sambaAlgorithmicRidBase

objectclass sambaUnixIdPool
		oid
				1.3.6.1.4.1.7165.1.2.2.7
		superior
				top
		requires
				uidNumber,
				gidNumber

objectclass sambaIdmapEntry
		oid
				1.3.6.1.4.1.7165.1.2.2.8
		superior
				top
		requires
				sambaSID
		allows
				uidNumber,
				gidNumber

objectclass sambaSidEntry
		oid
				1.3.6.1.4.1.7165.1.2.2.9
		superior
				top
		requires
				sambaSID
