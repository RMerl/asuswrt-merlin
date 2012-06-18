
This test code requires a tdb that is configured for to use the asq module.
You can do that adding the following record to a tdb:

dn: @MODULES
@LIST: asq

Other modules can be used as well (like rdn_name for example)

The uidNumber 0 and the gidNumber 0 are considered invalid.

The user records should contain the followin attributes:
uid (required)			the user name
userPassword (optional)		the user password (if not present "LDB" is
				returned in the password field)
uidNumber (required)		the user uid
gidNumber (required)		the user primary gid
gecos (optional)		the GECOS
homeDirectory (required)	the home directory
loginShell (required)		the login shell
memberOf (required)		all the groups the user is member of should
				be reported here using their DNs. The
				primary group as well.

The group accounts should contain the following attributes:
cn (required)			the group name
uesrPassword (optional)		the group password (if not present "LDB" is
				returned in the password field)
gidNumber (required)		the group gid
member (optional)		the DNs of the member users, also the ones
				that have this group as primary


SSS
