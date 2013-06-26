Configuring NFS4 ACLs in Samba3
===============================
Created: Peter Somogyi, 2006-JUN-06
Last modified: Peter Somogyi, 2006-JUL-20
Revision no.: 4 
-------------------------------


Parameters in smb.conf:
=======================

Each parameter must have a prefix "nfs4:".
Each one affects the behaviour only when _setting_ an acl on a file/dir:

mode = [simple|special]
- simple: don't use OWNER@ and GROUP@ special IDs in ACEs. - default
- special: use OWNER@ and GROUP@ special IDs in ACEs instead of simple user&group ids.
Note: EVERYONE@ is always processed (if found such an ACE).
Note2: special mode will have side effect when _only_ chown is performed. Later this may be worked out.

Use "simple" mode when the share is used mainly by windows users and unix side is not significant. You will loose unix bits in this case.
It's strongly advised setting "store dos attributes = yes" in smb.conf.

chown = [true|false]
- true => enable changing owner and group - default.
- false => disable support for changing owner or group

acedup = [dontcare|reject|ignore|merge]
- dontcare: copy ACEs as they come, don't care with "duplicate" records. Default.
- reject: stop operation, exit acl setter operation with an error
- ignore: don't include the second matching ACE
- merge: OR 2 ace.flag fields and 2 ace.mask fields of the 2 duplicate ACEs into 1 ACE

Two ACEs are considered here "duplicate" when their type and id fields are matching.

Example:

[smbtest]
path = /tests/psomogyi/smbtest
writable = yes
vfs objects = aixacl2
nfs4: mode = special
nfs4: chown = yes
nfs4: acedup = merge

Configuring AIX ACL support
==============================

Binaries: (default install path is [samba]/lib/vfs/)
- aixacl.so: provides AIXC ACL support only, can be compiled and works on all AIX platforms
- aixacl2.so: provides AIXC and JFS2-NFS4 ACL support, can be compiled and works only under AIX 5.3 and newer.
NFS4 acl currently has support only under JFS2 (ext. attr. format must be set to v2).
aixacl2.so always detects support for NFS4 acls and redirects to POSIX ACL handling automatically when NFS4 is not supported for a path.

Adding "vfs objects = aixacl2" to a share should be done only in case when NFS4 is really supported by the filesystem.
(Otherwise you may get performance loss.)

For configuration see also the example above.

General notes
=============

NFS4 handling logic is separated from AIX/jfs2 ACL parsing.

Samba and its VFS modules dosn't reorder ACEs. Windows clients do that (and the smbcacl tool). MSDN also says deny ACEs must come first.
NFS4 ACL's validity is checked by the system API, not by Samba.
NFS4 ACL rights are enforced by the OS or filesystem, not by Samba.

The flag INHERITED_ACE is never set (not required, as doesn't do WinNT/98/me, only since Win2k).
Win2k GUI behaves strangely when detecting inheritance (sometimes it doesn't detect, 
but after adding an ace it shows that - it's some GUI error).

Unknown (unmappable) SIDs are not accepted.

TODOs
=====
- Creator Owner & Group SID handling (same way as posix)
- the 4 generic rights bits support (GENERIC_RIGHT_READ_ACCESS, WRITE, EXEC, ALL)
- chown & no ACL, but we have ONWER@ and GROUP@
- DIALUP, ANONYMOUS, ... builtin SIDs
- audit & alarm support - in theory it's forwarded so it should work, but currently there's no platform which supports them to test
- support for a real NFS4 client (we don't have an accepted API yet)
