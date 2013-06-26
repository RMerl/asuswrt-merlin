This patch has been taken against SAMBA_3_0 Release 20028.

The patch basically moves the GPFS-ACL functionalities into the new GPFS VFS module( vfs_gpfs ).

Please read SAMBA_3_0/source/modules/README.nfs4acls.txt - generalised README file on Samba support for NFS4-ACLS. 
This README file is specific for GPFS only.

Configuring GPFS ACL support
===============================

Binary: (default install path is [samba]/lib/vfs/)
- gpfs.so 

Its compiled by default, no additional configure option needed.

To enable/use/load this module, include "vfs objects = gpfs" in the smb.conf file under concerned share-name.

Example of smb.conf:

[smbtest]
path = /gpfs-test
vfs objects = gpfs
nfs4: mode = special
nfs4: chown = yes
nfs4: acedup = merge

Adding "vfs objects = gpfs" to a share should be done only in case when NFS4 is really supported by the filesystem.
(Otherwise you may get performance loss.)

==================================================
Below are some limitations listed for this module:
==================================================
1. When a child file or child directory is created, the results are a bit different from windows as specified below:

Eg: Prent directory is set to have 2 ACES - full access for owner and everyone

Default ACL for Windows: 2 aces: allow ACE for owner and everyone	
Default ACL for GPFS: 6 aces: allow and deny ACEs for owner, group and everyone

The below mentioned inheritance flags and its combinations are applied only to the owner ACE and not to everyone ACE	

"fi"------>File Inherit
"di"------>Directory Inherit
"oi"------>Inherit Only

	
Parent dir: no inheritance flag set	
	Windows: index=0:			GPFS(special mode): index=0:			GPFS(simple mode): index=0:
child File: default acl: 2 aces		child file: default acl: 6 aces		child file: default acl: 6 aces
child dir: default acl: 2 aces		child dir: default acl: 6 aces		child dir: default acl: 6 aces


Parent dir: "fi" flag set
	Windows: index=1:			GPFS(special mode): index=1:			GPFS(simple mode): index=1:
child file: no flag set			child file: "fi" flag set		child file: default acl: 6 aces	
child dir: "fioi" flag set		child dir: "fi" flag set		child dir: "fi" flag set


Parent dir: "di" flag set
	Windows: index=2:			GPFS(special mode): index=2:			GPFS(simple mode): index=2:
child file: default acl: 2 aces		child file: default acl: 6 aces		child file: default acl: 6 aces


Parent dir: "fidi" flag set
	Windows: index=3:			GPFS(special mode): index=3:			GPFS(simple mode): index=3:
child file: no flag set			child file: "fidi" flag set		child file: default acl: 6 aces


Parent dir: "fioi" flag set	
	Windows: index=4:			GPFS(special mode): index=4:			GPFS(simple mode): index=4:
child file: no flag set			child file: "fi" flag set		child file: default acl: 6 aces
child dir: "fioi" flag set		child dir: "fi" flag set		child dir: "fi" flag set


Parent dir: "dioi" flag set
	Windows: index=5:			GPFS(special mode): index=5:			GPFS(simple mode): index=5:
child file: default acl: 2 aces		child file: default acl: 6 aces		child file: default acl: 6 aces


Parent dir: "fidioi" flag set
	Windows: index=6:			GPFS(special mode): index=6:			GPFS(simple mode): index=6:
child file: no flag set			child file: "fidi" flag set		child file: default acl: 6 aces
